// Copyright Epic Games, Inc. All Rights Reserved.

#include "DocModHttpServerSubsystem.h"
#include "DocMod.h"
#include "DocModFunctionLibrary.h"
#include "DocModOperationTypes.h"
#include "HttpServerModule.h"
#include "IHttpRouter.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"
#include "HttpPath.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Engine/GameInstance.h"
#include "IPAddress.h"
#include "DocModConfiguration.h"
#include "Configuration/ConfigManager.h"
#include "FGChatManager.h"

namespace
{
	// Defense-in-depth: don't rely solely on the socket-level
	// Config/DefaultEngine.ini ListenerOverrides loopback binding. Found
	// live (2026-08-24) that it does NOT take effect in the actual
	// Steam-launched (packaged Shipping) game - netstat showed
	// 0.0.0.0:51902 LISTENING, not 127.0.0.1:51902 - because that ini
	// override lives in this dev workspace's project-level
	// Config/DefaultEngine.ini, which only applies to Development Editor
	// sessions run from here, not the separately-deployed Alpakit
	// package. Until the packaging-side fix is in place, this check is
	// the only thing actually enforcing "loopback only" for real players.
	bool IsLoopbackPeer(const FHttpServerRequest& Request)
	{
		if (!Request.PeerAddress.IsValid())
		{
			return false;
		}
		const FString PeerIp = Request.PeerAddress->ToString(/*bAppendPort=*/false);
		return PeerIp == TEXT("127.0.0.1") || PeerIp == TEXT("::1") || PeerIp.StartsWith(TEXT("127."));
	}

	TUniquePtr<FHttpServerResponse> MakeJsonResponse(EHttpServerResponseCodes Code, const TSharedRef<FJsonObject>& Body)
	{
		FString JsonString;
		// Condensed, not the default pretty-printed policy - see the
		// matching comment in DocModFunctionLibrary.cpp.
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonString);
		FJsonSerializer::Serialize(Body, Writer);

		TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(JsonString, TEXT("application/json"));
		Response->Code = Code;
		return Response;
	}

	TUniquePtr<FHttpServerResponse> MakeErrorResponse(EHttpServerResponseCodes Code, const FString& RequestId, const FString& ErrorCode, const FString& Message)
	{
		const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetNumberField(TEXT("protocolVersion"), 1);
		Root->SetStringField(TEXT("requestId"), RequestId);
		Root->SetBoolField(TEXT("success"), false);

		const TSharedRef<FJsonObject> ErrorObject = MakeShared<FJsonObject>();
		ErrorObject->SetStringField(TEXT("code"), ErrorCode);
		ErrorObject->SetStringField(TEXT("message"), Message);
		Root->SetObjectField(TEXT("error"), ErrorObject);

		UE_LOG(LogDocModAI, Warning, TEXT("DocMod HTTP server: request %s failed - %s: %s"), *RequestId, *ErrorCode, *Message);

		return MakeJsonResponse(Code, Root);
	}

	// Maps FDocModOperationResult::ErrorCode (DocModFunctionLibrary.cpp's
	// write operations) to an HTTP status. Defaults to BadRequest for any
	// code not explicitly listed, rather than guessing at codes that
	// don't exist yet.
	EHttpServerResponseCodes HttpCodeForOperationError(const FString& ErrorCode)
	{
		if (ErrorCode == TEXT("TARGET_NOT_FOUND")) { return EHttpServerResponseCodes::NotFound; }
		if (ErrorCode == TEXT("OPERATION_NOT_PERMITTED")) { return EHttpServerResponseCodes::Forbidden; }
		if (ErrorCode == TEXT("INTERNAL_ERROR")) { return EHttpServerResponseCodes::ServerError; }
		return EHttpServerResponseCodes::BadRequest;
	}

	TUniquePtr<FHttpServerResponse> MakeOperationResponse(const FDocModOperationResult& OperationResult, const FString& RequestId)
	{
		if (!OperationResult.bSuccess)
		{
			return MakeErrorResponse(HttpCodeForOperationError(OperationResult.ErrorCode), RequestId, OperationResult.ErrorCode, OperationResult.ErrorMessage);
		}

		const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetNumberField(TEXT("protocolVersion"), 1);
		Root->SetStringField(TEXT("requestId"), RequestId);
		Root->SetBoolField(TEXT("success"), true);

		const TSharedRef<FJsonObject> ResultObject = MakeShared<FJsonObject>();
		if (!OperationResult.ResultBuildableId.IsEmpty())
		{
			ResultObject->SetStringField(TEXT("buildableId"), OperationResult.ResultBuildableId);
		}
		Root->SetObjectField(TEXT("result"), ResultObject);
		return MakeJsonResponse(EHttpServerResponseCodes::Ok, Root);
	}
}

void UDocModHttpServerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Registers DocMod's player-facing mod settings (DocModConfiguration.h)
	// so they show up in SML's normal mod settings menu, load from/save to
	// disk, and are readable via UDocModFunctionLibrary::GetDocModConfigBool/
	// GetDocModConfigFloat elsewhere in this module. Added 2026-08-27 per
	// explicit user request for player-controlled safety/capability
	// toggles instead of hardcoded defaults or per-call opt-in flags.
	if (UConfigManager* ConfigManager = GetGameInstance() ? GetGameInstance()->GetSubsystem<UConfigManager>() : nullptr)
	{
		ConfigManager->RegisterModConfiguration(UDocModConfiguration::StaticClass());
	}
	else
	{
		UE_LOG(LogDocModAI, Warning, TEXT("DocMod HTTP server: no UConfigManager found - mod settings (remote connections, unlimited resources, build distance limit) will use their off-by-default values and won't be player-editable this session"));
	}

	FHttpServerModule& HttpServerModule = FHttpServerModule::Get();
	Router = HttpServerModule.GetHttpRouter(ListenPort, /*bFailOnBindFailure=*/false);
	if (!Router.IsValid())
	{
		UE_LOG(LogDocModAI, Error, TEXT("DocMod HTTP server: failed to bind router on port %u"), ListenPort);
		return;
	}

	RpcRouteHandle = Router->BindRoute(
		FHttpPath(TEXT("/rpc")),
		EHttpServerRequestVerbs::VERB_POST,
		FHttpRequestHandler::CreateUObject(this, &UDocModHttpServerSubsystem::HandleRpcRequest));

	HttpServerModule.StartAllListeners();

	UE_LOG(LogDocModAI, Display, TEXT("DocMod HTTP server listening on http://127.0.0.1:%u/rpc (loopback only - see Config/DefaultEngine.ini ListenerOverrides)"), ListenPort);

	TryBindChatManagerDelegate();
}

void UDocModHttpServerSubsystem::Deinitialize()
{
	if (UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
	{
		World->GetTimerManager().ClearTimer(ChatManagerBindRetryTimer);
	}
	if (AFGChatManager* ChatManager = AFGChatManager::Get(GetGameInstance()))
	{
		ChatManager->OnChatMessageAdded.RemoveDynamic(this, &UDocModHttpServerSubsystem::HandlePlayerChatMessageAdded);
	}

	if (Router.IsValid() && RpcRouteHandle.IsValid())
	{
		Router->UnbindRoute(RpcRouteHandle);
	}
	Router.Reset();

	UE_LOG(LogDocModAI, Display, TEXT("DocMod HTTP server stopped"));

	Super::Deinitialize();
}

void UDocModHttpServerSubsystem::TryBindChatManagerDelegate()
{
	AFGChatManager* ChatManager = AFGChatManager::Get(GetGameInstance());
	if (!ChatManager)
	{
		// AFGChatManager is a world/actor-based subsystem, unlike
		// UConfigManager - it may not exist yet this early. Retry on a
		// short repeating timer until it does, then stop (see this
		// function's header doc comment).
		if (UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
		{
			World->GetTimerManager().SetTimer(ChatManagerBindRetryTimer, this, &UDocModHttpServerSubsystem::TryBindChatManagerDelegate, 1.0f, /*bLoop=*/false);
		}
		return;
	}

	// Seed LastSeenChatMessageCount to whatever history already exists
	// (e.g. the "X has joined the game!" system message) so only
	// messages added AFTER this binding trigger an ack - not a backlog
	// from before the mod finished initializing.
	TArray<FChatMessageStruct> ExistingMessages;
	ChatManager->GetReceivedChatMessages(ExistingMessages);
	LastSeenChatMessageCount = ExistingMessages.Num();

	// RemoveDynamic before AddDynamic makes this idempotent - confirmed
	// live (2026-08-28) that without this, TryBindChatManagerDelegate
	// ending up bound more than once (exact cause unconfirmed - possibly
	// this being called again in some world-reload scenario without an
	// intervening Deinitialize) turned a single real chat message into
	// dozens of duplicate "received, thinking..." acks: a multicast
	// delegate bound N times fires the handler N times per broadcast, and
	// since SendChatMessage's AddChatMessageToReceived re-triggers this
	// same delegate synchronously, N bindings compound multiplicatively
	// rather than just linearly. RemoveDynamic is a safe no-op if not
	// currently bound.
	ChatManager->OnChatMessageAdded.RemoveDynamic(this, &UDocModHttpServerSubsystem::HandlePlayerChatMessageAdded);
	ChatManager->OnChatMessageAdded.AddDynamic(this, &UDocModHttpServerSubsystem::HandlePlayerChatMessageAdded);

	UE_LOG(LogDocModAI, Display, TEXT("DocMod HTTP server: bound to AFGChatManager::OnChatMessageAdded for instant chat acknowledgment"));
}

void UDocModHttpServerSubsystem::HandlePlayerChatMessageAdded()
{
	AFGChatManager* ChatManager = AFGChatManager::Get(GetGameInstance());
	if (!ChatManager)
	{
		return;
	}

	TArray<FChatMessageStruct> Messages;
	ChatManager->GetReceivedChatMessages(Messages);

	// Re-entrancy note: SendChatMessage below calls AddChatMessageToReceived,
	// which fires THIS SAME delegate again, synchronously, before this call
	// returns. Advancing LastSeenChatMessageCount BEFORE reacting (not
	// after the whole batch) means the re-entrant call's own fresh read of
	// LastSeenChatMessageCount already excludes the message this call is
	// currently handling - it only ever sees the newly-added ack (a
	// CustomMessage, filtered out below regardless) as "new", so it can't
	// double-ack the same player message or roll the watermark backward.
	while (LastSeenChatMessageCount < Messages.Num())
	{
		const FChatMessageStruct Message = Messages[LastSeenChatMessageCount];
		++LastSeenChatMessageCount;

		// Only a genuine player-typed message ("/"-prefixed text never
		// reaches this array at all - diverted to chat command dispatch -
		// so no separate check is needed for that). Explicitly excludes
		// System/Ada/Custom messages, which includes DocMod's own acks -
		// without this a real ack would count as "new" too.
		if (Message.MessageType == EFGChatMessageType::CMT_PlayerMessage && Message.bIsLocalPlayerMessage)
		{
			const bool bAutoAck = UDocModFunctionLibrary::GetDocModConfigBool(GetGameInstance(), TEXT("AutoAcknowledgeChatMessages"), true);
			if (bAutoAck)
			{
				UDocModFunctionLibrary::SendChatMessage(GetGameInstance(), TEXT("received, thinking..."), TEXT("DocMod"));
			}
		}
	}
}

bool UDocModHttpServerSubsystem::HandleRpcRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	// AllowRemoteConnections (2026-08-27) - a player-controlled mod
	// setting (DocModConfiguration.h), off by default: this defense-in-depth
	// check still runs unconditionally otherwise, per this class's header
	// doc comment ("bind only to loopback by default... design the
	// transport so remote access is not accidentally enabled" -
	// CLAUDE.md's Networking rules). An external RPC caller cannot enable
	// this itself - only the player, from DocMod's settings menu.
	const bool bAllowRemoteConnections = UDocModFunctionLibrary::GetDocModConfigBool(GetGameInstance(), TEXT("AllowRemoteConnections"), false);
	if (!bAllowRemoteConnections && !IsLoopbackPeer(Request))
	{
		const FString PeerDescription = Request.PeerAddress.IsValid() ? Request.PeerAddress->ToString(/*bAppendPort=*/true) : TEXT("<unknown>");
		UE_LOG(LogDocModAI, Warning, TEXT("DocMod HTTP server: rejected non-loopback request from %s (enable 'Allow Remote Connections' in DocMod's mod settings to accept this)"), *PeerDescription);
		OnComplete(MakeErrorResponse(EHttpServerResponseCodes::Forbidden, TEXT(""), TEXT("FORBIDDEN"), TEXT("DocMod RPC only accepts loopback connections (enable 'Allow Remote Connections' in DocMod's mod settings to change this)")));
		return true;
	}
	else if (bAllowRemoteConnections && !IsLoopbackPeer(Request))
	{
		const FString PeerDescription = Request.PeerAddress.IsValid() ? Request.PeerAddress->ToString(/*bAppendPort=*/true) : TEXT("<unknown>");
		UE_LOG(LogDocModAI, Warning, TEXT("DocMod HTTP server: accepted non-loopback request from %s - 'Allow Remote Connections' is enabled in DocMod's mod settings"), *PeerDescription);
	}

	if (Request.Body.Num() > MaxRequestBodyBytes)
	{
		OnComplete(MakeErrorResponse(EHttpServerResponseCodes::RequestTooLarge, TEXT(""), TEXT("PAYLOAD_TOO_LARGE"),
			FString::Printf(TEXT("Request body exceeds %lld byte limit"), MaxRequestBodyBytes)));
		return true;
	}

	// Body is raw UTF-8 bytes, not pre-decoded. FUTF8ToTCHAR's pointer
	// constructor is deprecated in this engine version - use StringCast.
	const auto Converted = StringCast<TCHAR>(reinterpret_cast<const UTF8CHAR*>(Request.Body.GetData()), Request.Body.Num());
	const FString BodyString(Converted.Length(), Converted.Get());

	TSharedPtr<FJsonObject> RequestObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(BodyString);
	if (!FJsonSerializer::Deserialize(Reader, RequestObject) || !RequestObject.IsValid())
	{
		OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, TEXT(""), TEXT("INVALID_REQUEST"), TEXT("Request body is not valid JSON")));
		return true;
	}

	FString RequestId;
	RequestObject->TryGetStringField(TEXT("requestId"), RequestId);

	int32 ProtocolVersion = 0;
	if (!RequestObject->TryGetNumberField(TEXT("protocolVersion"), ProtocolVersion) || ProtocolVersion != 1)
	{
		OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("UNSUPPORTED_PROTOCOL_VERSION"), TEXT("protocolVersion must be 1")));
		return true;
	}

	FString Method;
	if (!RequestObject->TryGetStringField(TEXT("method"), Method))
	{
		OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("Missing required 'method' field")));
		return true;
	}

	// PLAN.md Phase 12 write methods take a "params" object. Handled
	// first and returns directly - they don't share the read methods'
	// "wrap a Log*AsJson string as the result" shape below.
	if (Method == TEXT("world.setClockSpeed") || Method == TEXT("world.setRecipe"))
	{
		const TSharedPtr<FJsonObject>* ParamsObjectPtr = nullptr;
		if (!RequestObject->TryGetObjectField(TEXT("params"), ParamsObjectPtr) || !ParamsObjectPtr || !ParamsObjectPtr->IsValid())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("Missing required 'params' object")));
			return true;
		}
		const TSharedPtr<FJsonObject> ParamsObject = *ParamsObjectPtr;

		FString BuildableId;
		if (!ParamsObject->TryGetStringField(TEXT("buildableId"), BuildableId) || BuildableId.IsEmpty())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("params.buildableId must be a non-empty string")));
			return true;
		}

		// GetGameInstance(), not `this` - UGameInstanceSubsystem itself
		// does not implement GetWorld(); UGameInstance does.
		if (Method == TEXT("world.setClockSpeed"))
		{
			double ClockSpeedPercent = 0.0;
			if (!ParamsObject->TryGetNumberField(TEXT("clockSpeedPercent"), ClockSpeedPercent))
			{
				OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("params.clockSpeedPercent must be a number")));
				return true;
			}

			const FDocModOperationResult Result = UDocModFunctionLibrary::SetManufacturerClockSpeed(GetGameInstance(), BuildableId, static_cast<float>(ClockSpeedPercent));
			OnComplete(MakeOperationResponse(Result, RequestId));
			return true;
		}
		else
		{
			FString RecipeClassPath;
			if (!ParamsObject->TryGetStringField(TEXT("recipeClass"), RecipeClassPath) || RecipeClassPath.IsEmpty())
			{
				OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("params.recipeClass must be a non-empty string")));
				return true;
			}

			const FDocModOperationResult Result = UDocModFunctionLibrary::SetManufacturerRecipe(GetGameInstance(), BuildableId, RecipeClassPath);
			OnComplete(MakeOperationResponse(Result, RequestId));
			return true;
		}
	}

	// Synchronous - no build gun/hologram involved, unlike construction.
	// Added 2026-08-25 so live testing (e.g. rotation calibration) can
	// clean up stray test buildables instead of accumulating them - see
	// DismantleBuildable's doc comment.
	if (Method == TEXT("world.deleteBuilding"))
	{
		const TSharedPtr<FJsonObject>* ParamsObjectPtr = nullptr;
		if (!RequestObject->TryGetObjectField(TEXT("params"), ParamsObjectPtr) || !ParamsObjectPtr || !ParamsObjectPtr->IsValid())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("Missing required 'params' object")));
			return true;
		}
		const TSharedPtr<FJsonObject> ParamsObject = *ParamsObjectPtr;

		FString BuildableId;
		if (!ParamsObject->TryGetStringField(TEXT("buildableId"), BuildableId) || BuildableId.IsEmpty())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("params.buildableId must be a non-empty string")));
			return true;
		}

		const FDocModOperationResult Result = UDocModFunctionLibrary::DismantleBuildable(GetGameInstance(), BuildableId);
		OnComplete(MakeOperationResponse(Result, RequestId));
		return true;
	}

	// Synchronous - direct AFGTimeOfDaySubsystem::SetDaySeconds() call, no
	// build gun/hologram involved. Added 2026-08-27 per explicit user
	// request so live testing/observation isn't blocked by the day/night
	// cycle going dark - see SetTimeOfDay's doc comment for why this
	// doesn't go through UFGCheatManager.
	if (Method == TEXT("world.setTimeOfDay"))
	{
		const TSharedPtr<FJsonObject>* ParamsObjectPtr = nullptr;
		if (!RequestObject->TryGetObjectField(TEXT("params"), ParamsObjectPtr) || !ParamsObjectPtr || !ParamsObjectPtr->IsValid())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("Missing required 'params' object")));
			return true;
		}
		const TSharedPtr<FJsonObject> ParamsObject = *ParamsObjectPtr;

		int32 Hour = 0;
		if (!ParamsObject->TryGetNumberField(TEXT("hour"), Hour))
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("params.hour must be an integer")));
			return true;
		}

		int32 Minute = 0;
		ParamsObject->TryGetNumberField(TEXT("minute"), Minute);

		const FDocModOperationResult Result = UDocModFunctionLibrary::SetTimeOfDay(GetGameInstance(), Hour, Minute);
		OnComplete(MakeOperationResponse(Result, RequestId));
		return true;
	}

	// Synchronous - AFGChatManager::AddChatMessageToReceived(), no build
	// gun/hologram involved. Added 2026-08-27 per explicit user request
	// for optional two-way chat with the player - see SendChatMessage's
	// doc comment for why this doesn't use BroadcastChatMessage's
	// NetMulticast RPC.
	if (Method == TEXT("world.sendChatMessage"))
	{
		const TSharedPtr<FJsonObject>* ParamsObjectPtr = nullptr;
		if (!RequestObject->TryGetObjectField(TEXT("params"), ParamsObjectPtr) || !ParamsObjectPtr || !ParamsObjectPtr->IsValid())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("Missing required 'params' object")));
			return true;
		}
		const TSharedPtr<FJsonObject> ParamsObject = *ParamsObjectPtr;

		FString Message;
		if (!ParamsObject->TryGetStringField(TEXT("message"), Message) || Message.IsEmpty())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("params.message must be a non-empty string")));
			return true;
		}

		// Optional, defaults to "DocMod AI" - see SendChatMessage's doc comment.
		FString Sender;
		ParamsObject->TryGetStringField(TEXT("sender"), Sender);

		const FDocModOperationResult Result = UDocModFunctionLibrary::SendChatMessage(GetGameInstance(), Message, Sender);
		OnComplete(MakeOperationResponse(Result, RequestId));
		return true;
	}

	// Synchronous, unlike the placement/power methods below - DebugCheckConveyorSnap
	// never polls, it's a single-call experiment (see its own doc comment).
	if (Method == TEXT("world.testConveyorSnap"))
	{
		const TSharedPtr<FJsonObject>* ParamsObjectPtr = nullptr;
		if (!RequestObject->TryGetObjectField(TEXT("params"), ParamsObjectPtr) || !ParamsObjectPtr || !ParamsObjectPtr->IsValid())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("Missing required 'params' object")));
			return true;
		}
		const TSharedPtr<FJsonObject> ParamsObject = *ParamsObjectPtr;

		FString SourceBuildableId;
		if (!ParamsObject->TryGetStringField(TEXT("sourceBuildableId"), SourceBuildableId) || SourceBuildableId.IsEmpty())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("params.sourceBuildableId must be a non-empty string")));
			return true;
		}

		const FDocModOperationResult Result = UDocModFunctionLibrary::DebugCheckConveyorSnap(GetGameInstance(), SourceBuildableId);
		OnComplete(MakeOperationResponse(Result, RequestId));
		return true;
	}

	// PLAN.md Phase 13/14: genuinely asynchronous - ConstructBuildingAtPosition's
	// OnComplete may fire well after this function returns (real-tick
	// polling, typically 1 tick but up to a ~2s safety cap). Per
	// FHttpRequestHandler's own contract (HttpRequestHandler.h: "returning
	// true means the delegate itself will (now or later) invoke
	// OnComplete"), returning true here without having called OnComplete
	// yet is correct - the HTTP response stays open until the copied
	// OnComplete is eventually invoked from the deferred poll.
	if (Method == TEXT("world.placeBuilding"))
	{
		const TSharedPtr<FJsonObject>* ParamsObjectPtr = nullptr;
		if (!RequestObject->TryGetObjectField(TEXT("params"), ParamsObjectPtr) || !ParamsObjectPtr || !ParamsObjectPtr->IsValid())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("Missing required 'params' object")));
			return true;
		}
		const TSharedPtr<FJsonObject> ParamsObject = *ParamsObjectPtr;

		FString RecipeClassPath;
		double X = 0.0;
		double Y = 0.0;
		if (!ParamsObject->TryGetStringField(TEXT("recipeClass"), RecipeClassPath) || RecipeClassPath.IsEmpty())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("params.recipeClass must be a non-empty string")));
			return true;
		}
		if (!ParamsObject->TryGetNumberField(TEXT("x"), X) || !ParamsObject->TryGetNumberField(TEXT("y"), Y))
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("params.x and params.y must both be numbers")));
			return true;
		}

		// Optional, defaults to 0 (no rotation, prior behavior) - see
		// ConstructBuildingAtPosition's doc comment on why this is a raw,
		// uncalibrated Scroll() delta rather than a degrees value.
		double RotationScrollDelta = 0.0;
		ParamsObject->TryGetNumberField(TEXT("rotationScrollDelta"), RotationScrollDelta);

		// Optional, defaults to 100 (1m) - snap-to-grid-by-default per
		// explicit project direction (2026-08-25), so callers get tidy
		// coordinates without having to opt in every time. Pass 0
		// explicitly to disable.
		double GridSnapSize = 100.0;
		ParamsObject->TryGetNumberField(TEXT("gridSnapSize"), GridSnapSize);

		// Optional - anchors the ground-trace search to this Z instead
		// of the player's current Z. See ConstructBuildingAtPosition's
		// doc comment on ReferenceZ for why this matters for reliable,
		// player-position-independent placement (e.g. building on
		// foundations step by step) - sentinel -1000000 means "not
		// provided, use player Z" (the prior behavior).
		double ReferenceZ = -1000000.0;
		ParamsObject->TryGetNumberField(TEXT("z"), ReferenceZ);

		// Optional, all default false (today's strict behavior) - see
		// ConstructBuildingAtPosition's doc comment. Named, scoped
		// bypasses of specific UX-only disqualifiers, per explicit user
		// direction that these gates don't scale for large autonomous
		// layouts and the user accepts the resulting collision risk.
		bool bIgnoreAimLocation = false;
		ParamsObject->TryGetBoolField(TEXT("ignoreAimLocation"), bIgnoreAimLocation);
		bool bIgnorePlayerEncroachment = false;
		ParamsObject->TryGetBoolField(TEXT("ignorePlayerEncroachment"), bIgnorePlayerEncroachment);
		bool bIgnoreClearance = false;
		ParamsObject->TryGetBoolField(TEXT("ignoreClearance"), bIgnoreClearance);
		bool bIgnoreInvalidFloor = false;
		ParamsObject->TryGetBoolField(TEXT("ignoreInvalidFloor"), bIgnoreInvalidFloor);

		// Optional - an exact absolute world yaw in degrees. Takes priority
		// over rotationScrollDelta entirely when present (see
		// ConstructBuildingAtPosition's doc comment: Scroll() called
		// repeatedly is non-linear for |N|>1, so this bypasses it for
		// callers that need a specific, reliable orientation - which is
		// every multi-building layout).
		double TargetYawDegrees = 0.0;
		const bool bHasTargetYaw = ParamsObject->TryGetNumberField(TEXT("yaw"), TargetYawDegrees);

		// Optional - resolves an existing buildable's real position and
		// computes yaw automatically, instead of requiring the caller to
		// fetch it and do the vector math themselves. Takes priority over
		// "yaw" if both are given. See ConstructBuildingAtPosition's doc
		// comment on FaceBuildableId.
		FString FaceBuildableId;
		ParamsObject->TryGetStringField(TEXT("faceBuildableId"), FaceBuildableId);

		// FHttpResultCallback is a TFunction, safe to copy - captured by
		// value so it stays alive until the deferred poll actually calls it.
		UDocModFunctionLibrary::ConstructBuildingAtPosition(GetGameInstance(), RecipeClassPath, static_cast<float>(X), static_cast<float>(Y), static_cast<int32>(RotationScrollDelta), static_cast<float>(GridSnapSize), static_cast<float>(ReferenceZ),
			bIgnoreAimLocation, bIgnorePlayerEncroachment, bIgnoreClearance, bIgnoreInvalidFloor,
			bHasTargetYaw, static_cast<float>(TargetYawDegrees), FaceBuildableId,
			[OnComplete, RequestId](const FDocModOperationResult& Result)
			{
				OnComplete(MakeOperationResponse(Result, RequestId));
			});
		return true;
	}

	// Genuinely asynchronous, same shape as "world.placeBuilding" above -
	// see that method's comment.
	if (Method == TEXT("world.placeExtractor"))
	{
		const TSharedPtr<FJsonObject>* ParamsObjectPtr = nullptr;
		if (!RequestObject->TryGetObjectField(TEXT("params"), ParamsObjectPtr) || !ParamsObjectPtr || !ParamsObjectPtr->IsValid())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("Missing required 'params' object")));
			return true;
		}
		const TSharedPtr<FJsonObject> ParamsObject = *ParamsObjectPtr;

		FString NodeId;
		if (!ParamsObject->TryGetStringField(TEXT("nodeId"), NodeId) || NodeId.IsEmpty())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("params.nodeId must be a non-empty string")));
			return true;
		}

		// Optional, defaults to Mk1 (prior hardcoded behavior) - any
		// extractor recipe now works (2026-08-27, per explicit user
		// request to support Resource Well Pressurizers/Extractors):
		// Recipe_MinerMk1..Mk3, Recipe_WaterPump, Recipe_OilPump,
		// Recipe_FrackingSmasher, Recipe_FrackingExtractor - see
		// ConstructExtractorOnNode's doc comment for the node-type
		// gating (Pressurizer needs a Fracking Core node, Extractor
		// needs an ACTIVATED Fracking Satellite node).
		FString RecipeClassPath;
		if (!ParamsObject->TryGetStringField(TEXT("recipeClass"), RecipeClassPath) || RecipeClassPath.IsEmpty())
		{
			RecipeClassPath = TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_MinerMk1.Recipe_MinerMk1_C");
		}

		UDocModFunctionLibrary::ConstructExtractorOnNode(GetGameInstance(), NodeId, RecipeClassPath,
			[OnComplete, RequestId](const FDocModOperationResult& Result)
			{
				OnComplete(MakeOperationResponse(Result, RequestId));
			});
		return true;
	}

	// Genuinely asynchronous, unrelated mechanism to every other
	// construction method above - the Portable Miner is equipment, not a
	// buildable/hologram. See ConstructPortableMinerOnNode's doc comment
	// for the full flow (real hotbar-equip path + reflection-invoked
	// protected Server RPC). Added 2026-08-27 per explicit user request.
	if (Method == TEXT("world.placePortableMiner"))
	{
		const TSharedPtr<FJsonObject>* ParamsObjectPtr = nullptr;
		if (!RequestObject->TryGetObjectField(TEXT("params"), ParamsObjectPtr) || !ParamsObjectPtr || !ParamsObjectPtr->IsValid())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("Missing required 'params' object")));
			return true;
		}
		const TSharedPtr<FJsonObject> ParamsObject = *ParamsObjectPtr;

		FString NodeId;
		if (!ParamsObject->TryGetStringField(TEXT("nodeId"), NodeId) || NodeId.IsEmpty())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("params.nodeId must be a non-empty string")));
			return true;
		}

		// Optional - defaults to the real BP_ItemDescriptorPortableMiner
		// path. See ConstructPortableMinerOnNode's doc comment.
		FString ItemClassPath;
		ParamsObject->TryGetStringField(TEXT("itemClass"), ItemClassPath);

		UDocModFunctionLibrary::ConstructPortableMinerOnNode(GetGameInstance(), NodeId, ItemClassPath,
			[OnComplete, RequestId](const FDocModOperationResult& Result)
			{
				OnComplete(MakeOperationResponse(Result, RequestId));
			});
		return true;
	}

	// Synchronous - see RetrievePortableMinerInventory's doc comment.
	if (Method == TEXT("world.retrievePortableMinerInventory"))
	{
		const TSharedPtr<FJsonObject>* ParamsObjectPtr = nullptr;
		if (!RequestObject->TryGetObjectField(TEXT("params"), ParamsObjectPtr) || !ParamsObjectPtr || !ParamsObjectPtr->IsValid())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("Missing required 'params' object")));
			return true;
		}
		const TSharedPtr<FJsonObject> ParamsObject = *ParamsObjectPtr;

		FString PortableMinerId;
		if (!ParamsObject->TryGetStringField(TEXT("portableMinerId"), PortableMinerId) || PortableMinerId.IsEmpty())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("params.portableMinerId must be a non-empty string")));
			return true;
		}

		UDocModFunctionLibrary::RetrievePortableMinerInventory(GetGameInstance(), PortableMinerId,
			[OnComplete, RequestId](const FDocModOperationResult& Result)
			{
				OnComplete(MakeOperationResponse(Result, RequestId));
			});
		return true;
	}

	// Synchronous, unlike the buildable-placement RPCs above -
	// UDocModFunctionLibrary::SpawnCreatureNearPlayer calls
	// AFGCreatureSubsystem::BeginSpawningCreature directly (a plain C++
	// function, not a hologram/RPC dispatch), so the result is known
	// immediately. Off by default - see the "AllowCreatureSpawning" mod
	// setting; a disabled request comes back as CREATURE_SPAWNING_DISABLED.
	if (Method == TEXT("world.spawnCreature"))
	{
		const TSharedPtr<FJsonObject>* ParamsObjectPtr = nullptr;
		if (!RequestObject->TryGetObjectField(TEXT("params"), ParamsObjectPtr) || !ParamsObjectPtr || !ParamsObjectPtr->IsValid())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("Missing required 'params' object")));
			return true;
		}
		const TSharedPtr<FJsonObject> ParamsObject = *ParamsObjectPtr;

		FString CreatureClassPath;
		if (!ParamsObject->TryGetStringField(TEXT("creatureClass"), CreatureClassPath) || CreatureClassPath.IsEmpty())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("params.creatureClass must be a non-empty string")));
			return true;
		}

		// Optional, defaults to 800 (see SpawnCreatureNearPlayer, which
		// also clamps to [100, 5000] regardless of what's passed here).
		double DistanceFromPlayer = 800.0;
		ParamsObject->TryGetNumberField(TEXT("distanceFromPlayer"), DistanceFromPlayer);

		const FDocModOperationResult Result = UDocModFunctionLibrary::SpawnCreatureNearPlayer(GetGameInstance(), CreatureClassPath, static_cast<float>(DistanceFromPlayer));
		OnComplete(MakeOperationResponse(Result, RequestId));
		return true;
	}

	if (Method == TEXT("world.despawnCreature"))
	{
		const TSharedPtr<FJsonObject>* ParamsObjectPtr = nullptr;
		if (!RequestObject->TryGetObjectField(TEXT("params"), ParamsObjectPtr) || !ParamsObjectPtr || !ParamsObjectPtr->IsValid())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("Missing required 'params' object")));
			return true;
		}
		const TSharedPtr<FJsonObject> ParamsObject = *ParamsObjectPtr;

		FString CreatureId;
		if (!ParamsObject->TryGetStringField(TEXT("creatureId"), CreatureId) || CreatureId.IsEmpty())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("params.creatureId must be a non-empty string")));
			return true;
		}

		const FDocModOperationResult Result = UDocModFunctionLibrary::DespawnCreature(GetGameInstance(), CreatureId);
		OnComplete(MakeOperationResponse(Result, RequestId));
		return true;
	}

	// Genuinely asynchronous, same shape as "world.placeBuilding" above.
	// "world.testPowerConnection" (dry run, never touches the save) and
	// "world.connectPower" (real - see
	// docs/conveyor-power-connection-research.md's pole-vs-daisy-chain
	// note before assuming a failure here is a bug) both take the same
	// params and share UDocModFunctionLibrary::ConstructPowerConnection,
	// differing only in the bDryRun argument.
	if (Method == TEXT("world.testPowerConnection") || Method == TEXT("world.connectPower"))
	{
		const TSharedPtr<FJsonObject>* ParamsObjectPtr = nullptr;
		if (!RequestObject->TryGetObjectField(TEXT("params"), ParamsObjectPtr) || !ParamsObjectPtr || !ParamsObjectPtr->IsValid())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("Missing required 'params' object")));
			return true;
		}
		const TSharedPtr<FJsonObject> ParamsObject = *ParamsObjectPtr;

		FString BuildableIdA;
		FString BuildableIdB;
		if (!ParamsObject->TryGetStringField(TEXT("buildableIdA"), BuildableIdA) || BuildableIdA.IsEmpty()
			|| !ParamsObject->TryGetStringField(TEXT("buildableIdB"), BuildableIdB) || BuildableIdB.IsEmpty())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("params.buildableIdA and params.buildableIdB must both be non-empty strings")));
			return true;
		}

		// Optional, both default false - see ConstructPowerConnection's
		// doc comment on the live-diagnosed disqualifier flakiness
		// (UFGCDWireSnap/UFGCDInvalidAimLocation) these bypass.
		bool bIgnoreAimLocation = false;
		ParamsObject->TryGetBoolField(TEXT("ignoreAimLocation"), bIgnoreAimLocation);
		bool bIgnoreWireSnap = false;
		ParamsObject->TryGetBoolField(TEXT("ignoreWireSnap"), bIgnoreWireSnap);

		const bool bDryRun = Method == TEXT("world.testPowerConnection");
		UDocModFunctionLibrary::ConstructPowerConnection(GetGameInstance(), BuildableIdA, BuildableIdB, bDryRun, bIgnoreAimLocation, bIgnoreWireSnap,
			[OnComplete, RequestId](const FDocModOperationResult& Result)
			{
				OnComplete(MakeOperationResponse(Result, RequestId));
			});
		return true;
	}

	// Genuinely asynchronous, same shape as "world.testPowerConnection"/
	// "world.connectPower" above. "world.testConveyorBelt" (dry run) and
	// "world.connectConveyor" (real) share UDocModFunctionLibrary::
	// ConstructConveyorBelt, differing only in the bDryRun argument. Params
	// use sourceBuildableId/destBuildableId (not A/B) to match the belt's
	// directional Output->Input semantics - see ConstructConveyorBelt's doc
	// comment for the two-click TrySnapToActor/DoMultiStepPlacement
	// mechanism this drives.
	if (Method == TEXT("world.testConveyorBelt") || Method == TEXT("world.connectConveyor"))
	{
		const TSharedPtr<FJsonObject>* ParamsObjectPtr = nullptr;
		if (!RequestObject->TryGetObjectField(TEXT("params"), ParamsObjectPtr) || !ParamsObjectPtr || !ParamsObjectPtr->IsValid())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("Missing required 'params' object")));
			return true;
		}
		const TSharedPtr<FJsonObject> ParamsObject = *ParamsObjectPtr;

		FString SourceBuildableId;
		FString DestBuildableId;
		if (!ParamsObject->TryGetStringField(TEXT("sourceBuildableId"), SourceBuildableId) || SourceBuildableId.IsEmpty()
			|| !ParamsObject->TryGetStringField(TEXT("destBuildableId"), DestBuildableId) || DestBuildableId.IsEmpty())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("params.sourceBuildableId and params.destBuildableId must both be non-empty strings")));
			return true;
		}

		// Optional, defaults to Mk1 (the prior hardcoded-only behavior) -
		// see ConstructConveyorBelt's doc comment. Any of
		// Recipe_ConveyorBeltMk1..Mk6 - see world.conveyorBeltTiers for
		// each tier's real queried GetSpeed().
		FString RecipeClassPath;
		if (!ParamsObject->TryGetStringField(TEXT("recipeClass"), RecipeClassPath) || RecipeClassPath.IsEmpty())
		{
			RecipeClassPath = TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorBeltMk1.Recipe_ConveyorBeltMk1_C");
		}

		// Optional, defaults empty (hologram's own default mode, prior
		// behavior unchanged) - "Straight"/"Curve"/"Auto", see
		// ConstructConveyorBelt's doc comment.
		FString RouteMode;
		ParamsObject->TryGetStringField(TEXT("routeMode"), RouteMode);

		const bool bDryRun = Method == TEXT("world.testConveyorBelt");
		UDocModFunctionLibrary::ConstructConveyorBelt(GetGameInstance(), SourceBuildableId, DestBuildableId, RecipeClassPath, RouteMode, bDryRun,
			[OnComplete, RequestId](const FDocModOperationResult& Result)
			{
				OnComplete(MakeOperationResponse(Result, RequestId));
			});
		return true;
	}

	// Genuinely asynchronous, same shape as "world.testConveyorBelt"/
	// "world.connectConveyor" above. "world.testConveyorLift" (dry run)
	// and "world.connectConveyorLift" (real) share UDocModFunctionLibrary::
	// ConstructConveyorLift, differing only in the bDryRun argument.
	// Vertical conveyor groundwork (2026-08-25, per explicit user
	// request) - NOT YET LIVE-TESTED, see ConstructConveyorLift's doc
	// comment.
	if (Method == TEXT("world.testConveyorLift") || Method == TEXT("world.connectConveyorLift"))
	{
		const TSharedPtr<FJsonObject>* ParamsObjectPtr = nullptr;
		if (!RequestObject->TryGetObjectField(TEXT("params"), ParamsObjectPtr) || !ParamsObjectPtr || !ParamsObjectPtr->IsValid())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("Missing required 'params' object")));
			return true;
		}
		const TSharedPtr<FJsonObject> ParamsObject = *ParamsObjectPtr;

		FString SourceBuildableId;
		FString DestBuildableId;
		if (!ParamsObject->TryGetStringField(TEXT("sourceBuildableId"), SourceBuildableId) || SourceBuildableId.IsEmpty()
			|| !ParamsObject->TryGetStringField(TEXT("destBuildableId"), DestBuildableId) || DestBuildableId.IsEmpty())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("params.sourceBuildableId and params.destBuildableId must both be non-empty strings")));
			return true;
		}

		// Optional, defaults to Mk1. Any of Recipe_ConveyorLiftMk1..Mk6 -
		// see world.conveyorLiftTiers for each tier's real queried speed.
		FString RecipeClassPath;
		if (!ParamsObject->TryGetStringField(TEXT("recipeClass"), RecipeClassPath) || RecipeClassPath.IsEmpty())
		{
			RecipeClassPath = TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorLiftMk1.Recipe_ConveyorLiftMk1_C");
		}

		const bool bDryRun = Method == TEXT("world.testConveyorLift");
		UDocModFunctionLibrary::ConstructConveyorLift(GetGameInstance(), SourceBuildableId, DestBuildableId, RecipeClassPath, bDryRun,
			[OnComplete, RequestId](const FDocModOperationResult& Result)
			{
				OnComplete(MakeOperationResponse(Result, RequestId));
			});
		return true;
	}

	// Genuinely asynchronous, same shape as "world.testConveyorBelt"/
	// "world.connectConveyor" above. "world.testPipe" (dry run) and
	// "world.connectPipe" (real) share UDocModFunctionLibrary::
	// ConstructPipe, differing only in the bDryRun argument. Pipe
	// groundwork (2026-08-25) - NOT YET LIVE-TESTED, see ConstructPipe's
	// doc comment for the open questions (no GetAnyConnectedBuildables()
	// on the shared hologram base, no standalone pole recipe found on
	// disk).
	if (Method == TEXT("world.testPipe") || Method == TEXT("world.connectPipe"))
	{
		const TSharedPtr<FJsonObject>* ParamsObjectPtr = nullptr;
		if (!RequestObject->TryGetObjectField(TEXT("params"), ParamsObjectPtr) || !ParamsObjectPtr || !ParamsObjectPtr->IsValid())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("Missing required 'params' object")));
			return true;
		}
		const TSharedPtr<FJsonObject> ParamsObject = *ParamsObjectPtr;

		FString SourceBuildableId;
		FString DestBuildableId;
		if (!ParamsObject->TryGetStringField(TEXT("sourceBuildableId"), SourceBuildableId) || SourceBuildableId.IsEmpty()
			|| !ParamsObject->TryGetStringField(TEXT("destBuildableId"), DestBuildableId) || DestBuildableId.IsEmpty())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("params.sourceBuildableId and params.destBuildableId must both be non-empty strings")));
			return true;
		}

		// Optional, defaults to Mk1. Recipe_Pipeline or
		// Recipe_PipelineMK2 - see world.pipelineTiers for each tier's
		// real queried flowLimit/maxSplineLength/bendRadius/minBendRadius.
		FString RecipeClassPath;
		if (!ParamsObject->TryGetStringField(TEXT("recipeClass"), RecipeClassPath) || RecipeClassPath.IsEmpty())
		{
			RecipeClassPath = TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_Pipeline.Recipe_Pipeline_C");
		}

		const bool bDryRun = Method == TEXT("world.testPipe");
		UDocModFunctionLibrary::ConstructPipe(GetGameInstance(), SourceBuildableId, DestBuildableId, RecipeClassPath, bDryRun,
			[OnComplete, RequestId](const FDocModOperationResult& Result)
			{
				OnComplete(MakeOperationResponse(Result, RequestId));
			});
		return true;
	}

	// "world.testHypertube" (dry run) and "world.connectHypertube" (real)
	// share UDocModFunctionLibrary::ConstructHypertube, differing only in
	// bDryRun - same shape as world.testPipe/world.connectPipe above, but
	// no recipeClass param (Recipe_PipeHyper is hardcoded - see
	// ConstructHypertube's doc comment).
	if (Method == TEXT("world.testHypertube") || Method == TEXT("world.connectHypertube"))
	{
		const TSharedPtr<FJsonObject>* ParamsObjectPtr = nullptr;
		if (!RequestObject->TryGetObjectField(TEXT("params"), ParamsObjectPtr) || !ParamsObjectPtr || !ParamsObjectPtr->IsValid())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("Missing required 'params' object")));
			return true;
		}
		const TSharedPtr<FJsonObject> ParamsObject = *ParamsObjectPtr;

		FString SourceBuildableId;
		FString DestBuildableId;
		if (!ParamsObject->TryGetStringField(TEXT("sourceBuildableId"), SourceBuildableId) || SourceBuildableId.IsEmpty()
			|| !ParamsObject->TryGetStringField(TEXT("destBuildableId"), DestBuildableId) || DestBuildableId.IsEmpty())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("params.sourceBuildableId and params.destBuildableId must both be non-empty strings")));
			return true;
		}

		const bool bDryRunHyper = Method == TEXT("world.testHypertube");
		UDocModFunctionLibrary::ConstructHypertube(GetGameInstance(), SourceBuildableId, DestBuildableId, bDryRunHyper,
			[OnComplete, RequestId](const FDocModOperationResult& Result)
			{
				OnComplete(MakeOperationResponse(Result, RequestId));
			});
		return true;
	}

	// GetGameInstance(), not `this` - UGameInstanceSubsystem itself does
	// not implement GetWorld(); UGameInstance does.
	FString MethodResultJson;
	if (Method == TEXT("world.resourceNodes"))
	{
		MethodResultJson = UDocModFunctionLibrary::LogResourceNodesAsJson(GetGameInstance());
	}
	else if (Method == TEXT("world.buildables"))
	{
		MethodResultJson = UDocModFunctionLibrary::LogBuildablesAsJson(GetGameInstance());
	}
	else if (Method == TEXT("world.manufacturers"))
	{
		MethodResultJson = UDocModFunctionLibrary::LogManufacturersAsJson(GetGameInstance());
	}
	else if (Method == TEXT("world.connections"))
	{
		MethodResultJson = UDocModFunctionLibrary::LogFactoryConnectionsAsJson(GetGameInstance());
	}
	else if (Method == TEXT("world.pipeConnections"))
	{
		MethodResultJson = UDocModFunctionLibrary::LogPipeConnectionsAsJson(GetGameInstance());
	}
	else if (Method == TEXT("world.conveyorBeltTiers"))
	{
		MethodResultJson = UDocModFunctionLibrary::LogConveyorBeltTiersAsJson(GetGameInstance());
	}
	else if (Method == TEXT("world.powerLineLimits"))
	{
		MethodResultJson = UDocModFunctionLibrary::LogPowerLineLimitsAsJson(GetGameInstance());
	}
	else if (Method == TEXT("world.pipelineTiers"))
	{
		MethodResultJson = UDocModFunctionLibrary::LogPipelineTiersAsJson(GetGameInstance());
	}
	else if (Method == TEXT("world.conveyorAttachments"))
	{
		MethodResultJson = UDocModFunctionLibrary::LogConveyorAttachmentCatalogAsJson(GetGameInstance());
	}
	else if (Method == TEXT("world.conveyorLiftTiers"))
	{
		MethodResultJson = UDocModFunctionLibrary::LogConveyorLiftTiersAsJson(GetGameInstance());
	}
	else if (Method == TEXT("world.recipeCatalog"))
	{
		MethodResultJson = UDocModFunctionLibrary::LogRecipeCatalogAsJson(GetGameInstance());
	}
	else if (Method == TEXT("world.itemCatalog"))
	{
		MethodResultJson = UDocModFunctionLibrary::LogItemCatalogAsJson(GetGameInstance());
	}
	else if (Method == TEXT("world.buildableCatalog"))
	{
		MethodResultJson = UDocModFunctionLibrary::LogBuildableCatalogAsJson(GetGameInstance());
	}
	else if (Method == TEXT("world.targetedManufacturer"))
	{
		MethodResultJson = UDocModFunctionLibrary::LogTargetedManufacturerAsJson(GetGameInstance());
	}
	else if (Method == TEXT("world.player"))
	{
		MethodResultJson = UDocModFunctionLibrary::LogPlayerAsJson(GetGameInstance());
	}
	else if (Method == TEXT("world.timeOfDay"))
	{
		MethodResultJson = UDocModFunctionLibrary::LogTimeOfDayAsJson(GetGameInstance());
	}
	else if (Method == TEXT("world.chatHistory"))
	{
		MethodResultJson = UDocModFunctionLibrary::LogChatHistoryAsJson(GetGameInstance());
	}
	else if (Method == TEXT("world.portableMiners"))
	{
		MethodResultJson = UDocModFunctionLibrary::LogPortableMinersAsJson(GetGameInstance());
	}
	else if (Method == TEXT("world.cleanupOrphanedFlowIndicators"))
	{
		// A real write operation (deletes actors) but takes no params, so
		// it fits this simple dispatch shape rather than the params-object
		// one. See CleanupOrphanedFlowIndicatorsAsJson's doc comment.
		MethodResultJson = UDocModFunctionLibrary::CleanupOrphanedFlowIndicatorsAsJson(GetGameInstance());
	}
	else if (Method == TEXT("world.groundHeight"))
	{
		const TSharedPtr<FJsonObject>* ParamsObjectPtr = nullptr;
		if (!RequestObject->TryGetObjectField(TEXT("params"), ParamsObjectPtr) || !ParamsObjectPtr || !ParamsObjectPtr->IsValid())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("Missing required 'params' object")));
			return true;
		}
		const TSharedPtr<FJsonObject> ParamsObject = *ParamsObjectPtr;

		double X = 0.0;
		double Y = 0.0;
		if (!ParamsObject->TryGetNumberField(TEXT("x"), X) || !ParamsObject->TryGetNumberField(TEXT("y"), Y))
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("params.x and params.y must both be numbers")));
			return true;
		}

		// Optional - same ReferenceZ semantics as world.placeBuilding's
		// "z" param (search-center anchor, defaults to player Z). See
		// LogGroundHeightAsJson's doc comment.
		double ReferenceZ = -1000000.0;
		ParamsObject->TryGetNumberField(TEXT("z"), ReferenceZ);

		MethodResultJson = UDocModFunctionLibrary::LogGroundHeightAsJson(GetGameInstance(), static_cast<float>(X), static_cast<float>(Y), static_cast<float>(ReferenceZ));
	}
	else
	{
		OnComplete(MakeErrorResponse(EHttpServerResponseCodes::NotFound, RequestId, TEXT("UNKNOWN_METHOD"), FString::Printf(TEXT("Unknown method '%s'"), *Method)));
		return true;
	}

	TSharedPtr<FJsonObject> ResultObject;
	const TSharedRef<TJsonReader<>> ResultReader = TJsonReaderFactory<>::Create(MethodResultJson);
	FJsonSerializer::Deserialize(ResultReader, ResultObject);

	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("protocolVersion"), 1);
	Root->SetStringField(TEXT("requestId"), RequestId);
	Root->SetBoolField(TEXT("success"), true);
	if (ResultObject.IsValid())
	{
		Root->SetObjectField(TEXT("result"), ResultObject.ToSharedRef());
	}
	else
	{
		// Should not happen - LogResourceNodesAsJson always emits valid JSON -
		// but never silently return "success" with no result if it somehow did.
		OnComplete(MakeErrorResponse(EHttpServerResponseCodes::ServerError, RequestId, TEXT("INTERNAL_ERROR"), TEXT("Failed to build result payload")));
		return true;
	}

	OnComplete(MakeJsonResponse(EHttpServerResponseCodes::Ok, Root));
	return true;
}
