// Copyright Epic Games, Inc. All Rights Reserved.

#include "AIModHttpServerSubsystem.h"
#include "AIMod.h"
#include "AIModFunctionLibrary.h"
#include "AIModOperationTypes.h"
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
#include "AIModConfiguration.h"
#include "Configuration/ConfigManager.h"
#include "FGChatManager.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "FGRecipe.h"

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
		// matching comment in AIModFunctionLibrary.cpp.
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

		UE_LOG(LogAIModAI, Warning, TEXT("AIMod HTTP server: request %s failed - %s: %s"), *RequestId, *ErrorCode, *Message);

		return MakeJsonResponse(Code, Root);
	}

	// Maps FAIModOperationResult::ErrorCode (AIModFunctionLibrary.cpp's
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

	TUniquePtr<FHttpServerResponse> MakeOperationResponse(const FAIModOperationResult& OperationResult, const FString& RequestId)
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
		if (!OperationResult.ResultDetailJson.IsEmpty())
		{
			TSharedPtr<FJsonObject> DetailObject;
			const TSharedRef<TJsonReader<>> DetailReader = TJsonReaderFactory<>::Create(OperationResult.ResultDetailJson);
			if (FJsonSerializer::Deserialize(DetailReader, DetailObject) && DetailObject.IsValid())
			{
				ResultObject->SetObjectField(TEXT("detail"), DetailObject);
			}
		}
		Root->SetObjectField(TEXT("result"), ResultObject);
		return MakeJsonResponse(EHttpServerResponseCodes::Ok, Root);
	}
}

void UAIModHttpServerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Registers AIMod's player-facing mod settings (AIModConfiguration.h)
	// so they show up in SML's normal mod settings menu, load from/save to
	// disk, and are readable via UAIModFunctionLibrary::GetAIModConfigBool/
	// GetAIModConfigFloat elsewhere in this module. Added 2026-08-27 per
	// explicit user request for player-controlled safety/capability
	// toggles instead of hardcoded defaults or per-call opt-in flags.
	if (UConfigManager* ConfigManager = GetGameInstance() ? GetGameInstance()->GetSubsystem<UConfigManager>() : nullptr)
	{
		ConfigManager->RegisterModConfiguration(UAIModConfiguration::StaticClass());
	}
	else
	{
		UE_LOG(LogAIModAI, Warning, TEXT("AIMod HTTP server: no UConfigManager found - mod settings (remote connections, unlimited resources, build distance limit) will use their off-by-default values and won't be player-editable this session"));
	}

	FHttpServerModule& HttpServerModule = FHttpServerModule::Get();
	Router = HttpServerModule.GetHttpRouter(ListenPort, /*bFailOnBindFailure=*/false);
	if (!Router.IsValid())
	{
		UE_LOG(LogAIModAI, Error, TEXT("AIMod HTTP server: failed to bind router on port %u"), ListenPort);
		return;
	}

	RpcRouteHandle = Router->BindRoute(
		FHttpPath(TEXT("/rpc")),
		EHttpServerRequestVerbs::VERB_POST,
		FHttpRequestHandler::CreateUObject(this, &UAIModHttpServerSubsystem::HandleRpcRequest));

	HttpServerModule.StartAllListeners();

	UE_LOG(LogAIModAI, Display, TEXT("AIMod HTTP server listening on http://127.0.0.1:%u/rpc (loopback only - see Config/DefaultEngine.ini ListenerOverrides)"), ListenPort);

	TryBindChatManagerDelegate();

	// See this class's header doc comment ("Fixed 2026-08-28") - Initialize()
	// alone runs too early (often at the main menu, before the player's
	// save has finished loading into its real world), so also rebind on
	// every real game world's init, mirroring FAIModModule::RunPerWorldSetup's
	// two-delegate pattern for the same ProcessServerTravel reliability reason.
	ChatWorldInitializedActorsHandle = FWorldDelegates::OnWorldInitializedActors.AddUObject(this, &UAIModHttpServerSubsystem::OnWorldInitializedActorsForChat);
	ChatPostLoadMapWithWorldHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UAIModHttpServerSubsystem::OnPostLoadMapWithWorldForChat);
}

void UAIModHttpServerSubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldInitializedActors.Remove(ChatWorldInitializedActorsHandle);
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(ChatPostLoadMapWithWorldHandle);

	if (UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
	{
		World->GetTimerManager().ClearTimer(ChatManagerBindRetryTimer);
	}
	if (AFGChatManager* ChatManager = AFGChatManager::Get(GetGameInstance()))
	{
		ChatManager->OnChatMessageAdded.RemoveDynamic(this, &UAIModHttpServerSubsystem::HandlePlayerChatMessageAdded);
	}

	if (Router.IsValid() && RpcRouteHandle.IsValid())
	{
		Router->UnbindRoute(RpcRouteHandle);
	}
	Router.Reset();

	UE_LOG(LogAIModAI, Display, TEXT("AIMod HTTP server stopped"));

	Super::Deinitialize();
}

void UAIModHttpServerSubsystem::OnWorldInitializedActorsForChat(const FActorsInitializedParams& Params)
{
	RebindChatManagerForWorld(Params.World);
}

void UAIModHttpServerSubsystem::OnPostLoadMapWithWorldForChat(UWorld* World)
{
	RebindChatManagerForWorld(World);
}

void UAIModHttpServerSubsystem::RebindChatManagerForWorld(UWorld* World)
{
	// Fires for every world load, including menu/editor-preview worlds -
	// skip anything that isn't a real game world, and de-duplicate since
	// both delegates can fire for the same world (see FAIModModule::
	// RunPerWorldSetup's matching comment).
	if (!World || !World->IsGameWorld() || LastRebindWorld == World)
	{
		return;
	}
	LastRebindWorld = World;

	// Clear any pending retry from an earlier (now-stale) world before
	// trying again against this one.
	World->GetTimerManager().ClearTimer(ChatManagerBindRetryTimer);
	TryBindChatManagerDelegate();
}

void UAIModHttpServerSubsystem::TryBindChatManagerDelegate()
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
			World->GetTimerManager().SetTimer(ChatManagerBindRetryTimer, this, &UAIModHttpServerSubsystem::TryBindChatManagerDelegate, 1.0f, /*bLoop=*/false);
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
	ChatManager->OnChatMessageAdded.RemoveDynamic(this, &UAIModHttpServerSubsystem::HandlePlayerChatMessageAdded);
	ChatManager->OnChatMessageAdded.AddDynamic(this, &UAIModHttpServerSubsystem::HandlePlayerChatMessageAdded);

	UE_LOG(LogAIModAI, Display, TEXT("AIMod HTTP server: bound to AFGChatManager::OnChatMessageAdded for instant chat acknowledgment"));
}

void UAIModHttpServerSubsystem::HandlePlayerChatMessageAdded()
{
	AFGChatManager* ChatManager = AFGChatManager::Get(GetGameInstance());
	if (!ChatManager)
	{
		return;
	}

	TArray<FChatMessageStruct> Messages;
	ChatManager->GetReceivedChatMessages(Messages);

	// Bulk-load guard, added 2026-08-28: confirmed live, twice, that a
	// burst of dozens of messages can appear between one broadcast and
	// the next - a save actually finishing its load into the real world
	// restores its persisted chat history in one go, and (whether due to
	// lossy type/sender persistence or some other restore-path quirk not
	// fully root-caused from source alone) entries that were never real
	// live player input can end up satisfying the PlayerMessage+local
	// check below. A genuine live keystroke can only ever add exactly
	// ONE message per broadcast - anything more than that in a single
	// invocation is unambiguously not real-time typing, so silently
	// catch up the watermark without acking rather than flooding chat.
	const int32 NewMessageCount = Messages.Num() - LastSeenChatMessageCount;
	if (NewMessageCount > 1)
	{
		UE_LOG(LogAIModAI, Warning, TEXT("HandlePlayerChatMessageAdded: %d messages appeared at once (was %d, now %d) - treating as a bulk history load, not live typing, and skipping acks for all of them"),
			NewMessageCount, LastSeenChatMessageCount, Messages.Num());
		LastSeenChatMessageCount = Messages.Num();
		return;
	}

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

		// Diagnostic logging, added 2026-08-28: two prior fix attempts
		// (idempotent binding, world-rebind, bulk-load guard) have not
		// stopped a live-confirmed burst of dozens of acks per real
		// keystroke, and the burst turned out to be dozens of SEPARATE
		// sequential broadcasts (not one batch), so the bulk-load guard
		// never even triggers. This logs every message's real content as
		// it's processed, to settle definitively whether the repeated
		// entries are literally the same player text re-added many times
		// (pointing to an upstream chat-submission bug outside AIMod) or
		// old historical entries somehow being replayed with corrupted
		// type/sender metadata (pointing to a save-restore issue) -
		// needed before attempting another fix blind.
		UE_LOG(LogAIModAI, Display, TEXT("HandlePlayerChatMessageAdded: index=%d sender=\"%s\" text=\"%s\" type=%d isLocal=%s"),
			LastSeenChatMessageCount - 1, *Message.MessageSender.ToString(), *Message.MessageText.ToString(),
			static_cast<int32>(Message.MessageType), Message.bIsLocalPlayerMessage ? TEXT("true") : TEXT("false"));

		// Only a genuine player-typed message ("/"-prefixed text never
		// reaches this array at all - diverted to chat command dispatch -
		// so no separate check is needed for that). Explicitly excludes
		// System/Ada/Custom messages, which includes AIMod's own acks -
		// without this a real ack would count as "new" too.
		if (Message.MessageType == EFGChatMessageType::CMT_PlayerMessage && Message.bIsLocalPlayerMessage)
		{
			// Duplicate-submission guard, added 2026-08-28: root-caused
			// live via the diagnostic logging above - confirmed the
			// game's own chat system can submit the SAME literal message
			// dozens of times for a single keystroke (seen once, for the
			// first message of a session: 49 back-to-back identical
			// "first" entries, same millisecond). That's upstream of
			// AIMod entirely (nothing here adds player-typed messages),
			// not something fixable from this file, but acking each
			// duplicate individually floods chat regardless of the root
			// cause. A real human retyping the exact same text takes far
			// longer than this - suppress only when the same text repeats
			// within half a second of the last ack.
			const FString MessageText = Message.MessageText.ToString();
			const double NowSeconds = FPlatformTime::Seconds();
			const bool bIsRepeatSpam = (MessageText == LastAckedMessageText) && (NowSeconds - LastAckedMessageTime) < 0.5;

			if (bIsRepeatSpam)
			{
				UE_LOG(LogAIModAI, Verbose, TEXT("HandlePlayerChatMessageAdded: suppressing ack for duplicate \"%s\" (upstream repeat-submission, not a new player action)"), *MessageText);
			}
			else
			{
				const bool bAutoAck = UAIModFunctionLibrary::GetAIModConfigBool(GetGameInstance(), TEXT("AutoAcknowledgeChatMessages"), true);
				if (bAutoAck)
				{
					UAIModFunctionLibrary::SendChatMessage(GetGameInstance(), TEXT("received, thinking..."), TEXT("AIMod"));
				}
				LastAckedMessageText = MessageText;
				LastAckedMessageTime = NowSeconds;
			}
		}
	}
}

bool UAIModHttpServerSubsystem::HandleRpcRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	// AllowRemoteConnections (2026-08-27) - a player-controlled mod
	// setting (AIModConfiguration.h), off by default: this defense-in-depth
	// check still runs unconditionally otherwise, per this class's header
	// doc comment ("bind only to loopback by default... design the
	// transport so remote access is not accidentally enabled" -
	// CLAUDE.md's Networking rules). An external RPC caller cannot enable
	// this itself - only the player, from AIMod's settings menu.
	const bool bAllowRemoteConnections = UAIModFunctionLibrary::GetAIModConfigBool(GetGameInstance(), TEXT("AllowRemoteConnections"), false);
	if (!bAllowRemoteConnections && !IsLoopbackPeer(Request))
	{
		const FString PeerDescription = Request.PeerAddress.IsValid() ? Request.PeerAddress->ToString(/*bAppendPort=*/true) : TEXT("<unknown>");
		UE_LOG(LogAIModAI, Warning, TEXT("AIMod HTTP server: rejected non-loopback request from %s (enable 'Allow Remote Connections' in AIMod's mod settings to accept this)"), *PeerDescription);
		OnComplete(MakeErrorResponse(EHttpServerResponseCodes::Forbidden, TEXT(""), TEXT("FORBIDDEN"), TEXT("AIMod RPC only accepts loopback connections (enable 'Allow Remote Connections' in AIMod's mod settings to change this)")));
		return true;
	}
	else if (bAllowRemoteConnections && !IsLoopbackPeer(Request))
	{
		const FString PeerDescription = Request.PeerAddress.IsValid() ? Request.PeerAddress->ToString(/*bAppendPort=*/true) : TEXT("<unknown>");
		UE_LOG(LogAIModAI, Warning, TEXT("AIMod HTTP server: accepted non-loopback request from %s - 'Allow Remote Connections' is enabled in AIMod's mod settings"), *PeerDescription);
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

			const FAIModOperationResult Result = UAIModFunctionLibrary::SetManufacturerClockSpeed(GetGameInstance(), BuildableId, static_cast<float>(ClockSpeedPercent));
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

			const FAIModOperationResult Result = UAIModFunctionLibrary::SetManufacturerRecipe(GetGameInstance(), BuildableId, RecipeClassPath);
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

		const FAIModOperationResult Result = UAIModFunctionLibrary::DismantleBuildable(GetGameInstance(), BuildableId);
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

		const FAIModOperationResult Result = UAIModFunctionLibrary::SetTimeOfDay(GetGameInstance(), Hour, Minute);
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

		// Optional, defaults to "AIMod AI" - see SendChatMessage's doc comment.
		FString Sender;
		ParamsObject->TryGetStringField(TEXT("sender"), Sender);

		const FAIModOperationResult Result = UAIModFunctionLibrary::SendChatMessage(GetGameInstance(), Message, Sender);
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

		const FAIModOperationResult Result = UAIModFunctionLibrary::DebugCheckConveyorSnap(GetGameInstance(), SourceBuildableId);
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

		// Optional, default false - skips the ground trace entirely and
		// places at the literal (x, y, z) given. See
		// ConstructBuildingAtPosition's doc comment on bIgnoreGroundTrace.
		// Requires "z" to be provided; fails MISSING_REFERENCE_Z otherwise.
		bool bIgnoreGroundTrace = false;
		ParamsObject->TryGetBoolField(TEXT("ignoreGroundTrace"), bIgnoreGroundTrace);

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
		UAIModFunctionLibrary::ConstructBuildingAtPosition(GetGameInstance(), RecipeClassPath, static_cast<float>(X), static_cast<float>(Y), static_cast<int32>(RotationScrollDelta), static_cast<float>(GridSnapSize), static_cast<float>(ReferenceZ),
			bIgnoreGroundTrace, bIgnoreAimLocation, bIgnorePlayerEncroachment, bIgnoreClearance, bIgnoreInvalidFloor,
			bHasTargetYaw, static_cast<float>(TargetYawDegrees), FaceBuildableId,
			[OnComplete, RequestId](const FAIModOperationResult& Result)
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

		UAIModFunctionLibrary::ConstructExtractorOnNode(GetGameInstance(), NodeId, RecipeClassPath,
			[OnComplete, RequestId](const FAIModOperationResult& Result)
			{
				OnComplete(MakeOperationResponse(Result, RequestId));
			});
		return true;
	}

	// Drones/wheeled vehicles - see ConstructVehicle's doc comment.
	// Hologram-driven like every other Construct* method above, NOT the
	// Portable Miner's equipment-dispenser mechanism below.
	if (Method == TEXT("world.constructVehicle"))
	{
		const TSharedPtr<FJsonObject>* ParamsObjectPtr = nullptr;
		if (!RequestObject->TryGetObjectField(TEXT("params"), ParamsObjectPtr) || !ParamsObjectPtr || !ParamsObjectPtr->IsValid())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("Missing required 'params' object")));
			return true;
		}
		const TSharedPtr<FJsonObject> ParamsObject = *ParamsObjectPtr;

		FString RecipeClassPath;
		if (!ParamsObject->TryGetStringField(TEXT("recipeClass"), RecipeClassPath) || RecipeClassPath.IsEmpty())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("params.recipeClass must be a non-empty string")));
			return true;
		}

		// Required for a drone recipe (snaps to this Drone Station),
		// ignored for a wheeled vehicle recipe (free placement via x/y/z
		// below instead) - see ConstructVehicle's doc comment.
		FString DroneStationId;
		ParamsObject->TryGetStringField(TEXT("droneStationId"), DroneStationId);

		double X = 0.0;
		double Y = 0.0;
		ParamsObject->TryGetNumberField(TEXT("x"), X);
		ParamsObject->TryGetNumberField(TEXT("y"), Y);

		double Z = -1000000.0;
		ParamsObject->TryGetNumberField(TEXT("z"), Z);

		bool bIgnoreGroundTrace = false;
		ParamsObject->TryGetBoolField(TEXT("ignoreGroundTrace"), bIgnoreGroundTrace);

		double TargetYawDegrees = 0.0;
		const bool bHasTargetYaw = ParamsObject->TryGetNumberField(TEXT("yaw"), TargetYawDegrees);

		UAIModFunctionLibrary::ConstructVehicle(GetGameInstance(), RecipeClassPath, DroneStationId,
			static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Z), bIgnoreGroundTrace,
			bHasTargetYaw, static_cast<float>(TargetYawDegrees),
			[OnComplete, RequestId](const FAIModOperationResult& Result)
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

		UAIModFunctionLibrary::ConstructPortableMinerOnNode(GetGameInstance(), NodeId, ItemClassPath,
			[OnComplete, RequestId](const FAIModOperationResult& Result)
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

		UAIModFunctionLibrary::RetrievePortableMinerInventory(GetGameInstance(), PortableMinerId,
			[OnComplete, RequestId](const FAIModOperationResult& Result)
			{
				OnComplete(MakeOperationResponse(Result, RequestId));
			});
		return true;
	}

	if (Method == TEXT("world.movePortableMinerToInventory"))
	{
		const FAIModOperationResult Result = UAIModFunctionLibrary::MovePortableMinerToInventory(GetGameInstance());
		OnComplete(MakeOperationResponse(Result, RequestId));
		return true;
	}

	if (Method == TEXT("world.simulatedCraft"))
	{
		const TSharedPtr<FJsonObject>* ParamsObjectPtr = nullptr;
		if (!RequestObject->TryGetObjectField(TEXT("params"), ParamsObjectPtr) || !ParamsObjectPtr || !ParamsObjectPtr->IsValid())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("Missing required 'params' object")));
			return true;
		}
		const TSharedPtr<FJsonObject> ParamsObject = *ParamsObjectPtr;

		FString RecipeClassPath;
		if (!ParamsObject->TryGetStringField(TEXT("recipeClass"), RecipeClassPath) || RecipeClassPath.IsEmpty())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("params.recipeClass must be a non-empty string")));
			return true;
		}

		const FAIModOperationResult Result = UAIModFunctionLibrary::SimulatedCraft(GetGameInstance(), RecipeClassPath);
		OnComplete(MakeOperationResponse(Result, RequestId));
		return true;
	}

	if (Method == TEXT("world.payMilestone"))
	{
		const TSharedPtr<FJsonObject>* ParamsObjectPtr = nullptr;
		if (!RequestObject->TryGetObjectField(TEXT("params"), ParamsObjectPtr) || !ParamsObjectPtr || !ParamsObjectPtr->IsValid())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("Missing required 'params' object")));
			return true;
		}
		const TSharedPtr<FJsonObject> ParamsObject = *ParamsObjectPtr;

		// Optional - empty means "target GetActiveSchematic()", see
		// PayOffMilestone's doc comment.
		FString SchematicClassPath;
		ParamsObject->TryGetStringField(TEXT("schematicClass"), SchematicClassPath);

		bool bDryRun = false;
		ParamsObject->TryGetBoolField(TEXT("dryRun"), bDryRun);

		const FAIModOperationResult Result = UAIModFunctionLibrary::PayOffMilestone(GetGameInstance(), SchematicClassPath, bDryRun);
		OnComplete(MakeOperationResponse(Result, RequestId));
		return true;
	}

	if (Method == TEXT("world.startMamResearch"))
	{
		const TSharedPtr<FJsonObject>* ParamsObjectPtr = nullptr;
		if (!RequestObject->TryGetObjectField(TEXT("params"), ParamsObjectPtr) || !ParamsObjectPtr || !ParamsObjectPtr->IsValid())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("Missing required 'params' object")));
			return true;
		}
		const TSharedPtr<FJsonObject> ParamsObject = *ParamsObjectPtr;

		FString SchematicClassPath;
		if (!ParamsObject->TryGetStringField(TEXT("schematicClass"), SchematicClassPath) || SchematicClassPath.IsEmpty())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("params.schematicClass must be a non-empty string")));
			return true;
		}
		FString ResearchTreeClassPath;
		if (!ParamsObject->TryGetStringField(TEXT("researchTreeClass"), ResearchTreeClassPath) || ResearchTreeClassPath.IsEmpty())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("params.researchTreeClass must be a non-empty string")));
			return true;
		}
		bool bDryRun = false;
		ParamsObject->TryGetBoolField(TEXT("dryRun"), bDryRun);

		const FAIModOperationResult Result = UAIModFunctionLibrary::StartMamResearch(GetGameInstance(), SchematicClassPath, ResearchTreeClassPath, bDryRun);
		OnComplete(MakeOperationResponse(Result, RequestId));
		return true;
	}

	if (Method == TEXT("world.claimMamResearch"))
	{
		const TSharedPtr<FJsonObject>* ParamsObjectPtr = nullptr;
		if (!RequestObject->TryGetObjectField(TEXT("params"), ParamsObjectPtr) || !ParamsObjectPtr || !ParamsObjectPtr->IsValid())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("Missing required 'params' object")));
			return true;
		}
		const TSharedPtr<FJsonObject> ParamsObject = *ParamsObjectPtr;

		FString SchematicClassPath;
		if (!ParamsObject->TryGetStringField(TEXT("schematicClass"), SchematicClassPath) || SchematicClassPath.IsEmpty())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("params.schematicClass must be a non-empty string")));
			return true;
		}

		const FAIModOperationResult Result = UAIModFunctionLibrary::ClaimMamResearch(GetGameInstance(), SchematicClassPath);
		OnComplete(MakeOperationResponse(Result, RequestId));
		return true;
	}

	if (Method == TEXT("world.claimMamHardDriveReward"))
	{
		const TSharedPtr<FJsonObject>* ParamsObjectPtr = nullptr;
		if (!RequestObject->TryGetObjectField(TEXT("params"), ParamsObjectPtr) || !ParamsObjectPtr || !ParamsObjectPtr->IsValid())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("Missing required 'params' object")));
			return true;
		}
		const TSharedPtr<FJsonObject> ParamsObject = *ParamsObjectPtr;

		FString SchematicClassPath;
		if (!ParamsObject->TryGetStringField(TEXT("schematicClass"), SchematicClassPath) || SchematicClassPath.IsEmpty())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("params.schematicClass must be a non-empty string")));
			return true;
		}

		const FAIModOperationResult Result = UAIModFunctionLibrary::ClaimMamHardDriveReward(GetGameInstance(), SchematicClassPath);
		OnComplete(MakeOperationResponse(Result, RequestId));
		return true;
	}

	if (Method == TEXT("world.rerollMamHardDrive"))
	{
		const TSharedPtr<FJsonObject>* ParamsObjectPtr = nullptr;
		if (!RequestObject->TryGetObjectField(TEXT("params"), ParamsObjectPtr) || !ParamsObjectPtr || !ParamsObjectPtr->IsValid())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("Missing required 'params' object")));
			return true;
		}
		const TSharedPtr<FJsonObject> ParamsObject = *ParamsObjectPtr;

		FString SchematicClassPath;
		if (!ParamsObject->TryGetStringField(TEXT("schematicClass"), SchematicClassPath) || SchematicClassPath.IsEmpty())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("params.schematicClass must be a non-empty string")));
			return true;
		}

		const FAIModOperationResult Result = UAIModFunctionLibrary::RerollMamHardDrive(GetGameInstance(), SchematicClassPath);
		OnComplete(MakeOperationResponse(Result, RequestId));
		return true;
	}

	if (Method == TEXT("world.withdrawFromCentralStorage"))
	{
		const TSharedPtr<FJsonObject>* ParamsObjectPtr = nullptr;
		if (!RequestObject->TryGetObjectField(TEXT("params"), ParamsObjectPtr) || !ParamsObjectPtr || !ParamsObjectPtr->IsValid())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("Missing required 'params' object")));
			return true;
		}
		const TSharedPtr<FJsonObject> ParamsObject = *ParamsObjectPtr;

		FString ItemClassPath;
		if (!ParamsObject->TryGetStringField(TEXT("itemClass"), ItemClassPath) || ItemClassPath.IsEmpty())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("params.itemClass must be a non-empty string")));
			return true;
		}
		double Amount = 0.0;
		if (!ParamsObject->TryGetNumberField(TEXT("amount"), Amount) || Amount <= 0.0)
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("params.amount must be a positive number")));
			return true;
		}

		const FAIModOperationResult Result = UAIModFunctionLibrary::WithdrawFromCentralStorage(GetGameInstance(), ItemClassPath, static_cast<int32>(Amount));
		OnComplete(MakeOperationResponse(Result, RequestId));
		return true;
	}

	// Synchronous, unlike the buildable-placement RPCs above -
	// UAIModFunctionLibrary::SpawnCreatureNearPlayer calls
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

		// Optional, defaults to 1.0 (normal size) - see SpawnCreatureNearPlayer,
		// which clamps to [0.05, 20.0] regardless of what's passed here.
		double Scale = 1.0;
		ParamsObject->TryGetNumberField(TEXT("scale"), Scale);

		const FAIModOperationResult Result = UAIModFunctionLibrary::SpawnCreatureNearPlayer(GetGameInstance(), CreatureClassPath, static_cast<float>(DistanceFromPlayer), static_cast<float>(Scale));
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

		const FAIModOperationResult Result = UAIModFunctionLibrary::DespawnCreature(GetGameInstance(), CreatureId);
		OnComplete(MakeOperationResponse(Result, RequestId));
		return true;
	}

	// Genuinely asynchronous, same shape as "world.placeBuilding" above.
	// "world.testPowerConnection" (dry run, never touches the save) and
	// "world.connectPower" (real - see
	// docs/conveyor-power-connection-research.md's pole-vs-daisy-chain
	// note before assuming a failure here is a bug) both take the same
	// params and share UAIModFunctionLibrary::ConstructPowerConnection,
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
		UAIModFunctionLibrary::ConstructPowerConnection(GetGameInstance(), BuildableIdA, BuildableIdB, bDryRun, bIgnoreAimLocation, bIgnoreWireSnap,
			[OnComplete, RequestId](const FAIModOperationResult& Result)
			{
				OnComplete(MakeOperationResponse(Result, RequestId));
			});
		return true;
	}

	// Genuinely asynchronous, same shape as "world.testPowerConnection"/
	// "world.connectPower" above. "world.testConveyorBelt" (dry run) and
	// "world.connectConveyor" (real) share UAIModFunctionLibrary::
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
		UAIModFunctionLibrary::ConstructConveyorBelt(GetGameInstance(), SourceBuildableId, DestBuildableId, RecipeClassPath, RouteMode, bDryRun,
			[OnComplete, RequestId](const FAIModOperationResult& Result)
			{
				OnComplete(MakeOperationResponse(Result, RequestId));
			});
		return true;
	}

	// Genuinely asynchronous, same shape as "world.testConveyorBelt"/
	// "world.connectConveyor" above. "world.testConveyorLift" (dry run)
	// and "world.connectConveyorLift" (real) share UAIModFunctionLibrary::
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
		UAIModFunctionLibrary::ConstructConveyorLift(GetGameInstance(), SourceBuildableId, DestBuildableId, RecipeClassPath, bDryRun,
			[OnComplete, RequestId](const FAIModOperationResult& Result)
			{
				OnComplete(MakeOperationResponse(Result, RequestId));
			});
		return true;
	}

	// Genuinely asynchronous, same shape as "world.testConveyorBelt"/
	// "world.connectConveyor" above. "world.testPipe" (dry run) and
	// "world.connectPipe" (real) share UAIModFunctionLibrary::
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
		UAIModFunctionLibrary::ConstructPipe(GetGameInstance(), SourceBuildableId, DestBuildableId, RecipeClassPath, bDryRun,
			[OnComplete, RequestId](const FAIModOperationResult& Result)
			{
				OnComplete(MakeOperationResponse(Result, RequestId));
			});
		return true;
	}

	// "world.testHypertube" (dry run) and "world.connectHypertube" (real)
	// share UAIModFunctionLibrary::ConstructHypertube, differing only in
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
		UAIModFunctionLibrary::ConstructHypertube(GetGameInstance(), SourceBuildableId, DestBuildableId, bDryRunHyper,
			[OnComplete, RequestId](const FAIModOperationResult& Result)
			{
				OnComplete(MakeOperationResponse(Result, RequestId));
			});
		return true;
	}

	// "world.testRailroadTrack" (dry run) and "world.constructRailroadTrack"
	// (real) share UAIModFunctionLibrary::ConstructRailroadTrack, same
	// shape as world.testPipe/world.connectPipe above. See
	// ConstructRailroadTrack's doc comment - NOT YET LIVE-TESTED.
	if (Method == TEXT("world.testRailroadTrack") || Method == TEXT("world.constructRailroadTrack"))
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

		FString RecipeClassPath;
		if (!ParamsObject->TryGetStringField(TEXT("recipeClass"), RecipeClassPath) || RecipeClassPath.IsEmpty())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("params.recipeClass must be a non-empty string - no confirmed default track recipe path, query world.recipeCatalog first")));
			return true;
		}

		const bool bDryRunTrack = Method == TEXT("world.testRailroadTrack");
		UAIModFunctionLibrary::ConstructRailroadTrack(GetGameInstance(), SourceBuildableId, DestBuildableId, RecipeClassPath, bDryRunTrack,
			[OnComplete, RequestId](const FAIModOperationResult& Result)
			{
				OnComplete(MakeOperationResponse(Result, RequestId));
			});
		return true;
	}

	// world.constructVehiclePathSegment - see ConstructVehiclePathSegment's
	// doc comment. NOT YET LIVE-TESTED.
	if (Method == TEXT("world.constructVehiclePathSegment"))
	{
		const TSharedPtr<FJsonObject>* ParamsObjectPtr = nullptr;
		if (!RequestObject->TryGetObjectField(TEXT("params"), ParamsObjectPtr) || !ParamsObjectPtr || !ParamsObjectPtr->IsValid())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("Missing required 'params' object")));
			return true;
		}
		const TSharedPtr<FJsonObject> ParamsObject = *ParamsObjectPtr;

		FString RecipeClassPath;
		if (!ParamsObject->TryGetStringField(TEXT("recipeClass"), RecipeClassPath) || RecipeClassPath.IsEmpty())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("params.recipeClass must be a non-empty string")));
			return true;
		}

		double StartX = 0.0, StartY = 0.0, EndX = 0.0, EndY = 0.0;
		if (!ParamsObject->TryGetNumberField(TEXT("startX"), StartX) || !ParamsObject->TryGetNumberField(TEXT("startY"), StartY)
			|| !ParamsObject->TryGetNumberField(TEXT("endX"), EndX) || !ParamsObject->TryGetNumberField(TEXT("endY"), EndY))
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("params.startX/startY/endX/endY must all be numbers")));
			return true;
		}

		double StartZ = -1000000.0;
		double EndZ = -1000000.0;
		ParamsObject->TryGetNumberField(TEXT("startZ"), StartZ);
		ParamsObject->TryGetNumberField(TEXT("endZ"), EndZ);

		bool bIgnoreGroundTrace = false;
		ParamsObject->TryGetBoolField(TEXT("ignoreGroundTrace"), bIgnoreGroundTrace);

		UAIModFunctionLibrary::ConstructVehiclePathSegment(GetGameInstance(), RecipeClassPath,
			static_cast<float>(StartX), static_cast<float>(StartY), static_cast<float>(StartZ),
			static_cast<float>(EndX), static_cast<float>(EndY), static_cast<float>(EndZ), bIgnoreGroundTrace,
			[OnComplete, RequestId](const FAIModOperationResult& Result)
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
		MethodResultJson = UAIModFunctionLibrary::LogResourceNodesAsJson(GetGameInstance());
	}
	else if (Method == TEXT("world.buildables"))
	{
		MethodResultJson = UAIModFunctionLibrary::LogBuildablesAsJson(GetGameInstance());
	}
	else if (Method == TEXT("world.vehicles"))
	{
		MethodResultJson = UAIModFunctionLibrary::LogVehiclesAsJson(GetGameInstance());
	}
	else if (Method == TEXT("world.milestoneProgress"))
	{
		MethodResultJson = UAIModFunctionLibrary::LogMilestoneProgressAsJson(GetGameInstance());
	}
	else if (Method == TEXT("world.mamStatus"))
	{
		MethodResultJson = UAIModFunctionLibrary::LogMamStatusAsJson(GetGameInstance());
	}
	else if (Method == TEXT("world.manufacturers"))
	{
		MethodResultJson = UAIModFunctionLibrary::LogManufacturersAsJson(GetGameInstance());
	}
	else if (Method == TEXT("world.connections"))
	{
		MethodResultJson = UAIModFunctionLibrary::LogFactoryConnectionsAsJson(GetGameInstance());
	}
	else if (Method == TEXT("world.pipeConnections"))
	{
		MethodResultJson = UAIModFunctionLibrary::LogPipeConnectionsAsJson(GetGameInstance());
	}
	else if (Method == TEXT("world.conveyorBeltTiers"))
	{
		MethodResultJson = UAIModFunctionLibrary::LogConveyorBeltTiersAsJson(GetGameInstance());
	}
	else if (Method == TEXT("world.powerLineLimits"))
	{
		MethodResultJson = UAIModFunctionLibrary::LogPowerLineLimitsAsJson(GetGameInstance());
	}
	else if (Method == TEXT("world.pipelineTiers"))
	{
		MethodResultJson = UAIModFunctionLibrary::LogPipelineTiersAsJson(GetGameInstance());
	}
	else if (Method == TEXT("world.conveyorAttachments"))
	{
		MethodResultJson = UAIModFunctionLibrary::LogConveyorAttachmentCatalogAsJson(GetGameInstance());
	}
	else if (Method == TEXT("world.conveyorLiftTiers"))
	{
		MethodResultJson = UAIModFunctionLibrary::LogConveyorLiftTiersAsJson(GetGameInstance());
	}
	else if (Method == TEXT("world.recipeCatalog"))
	{
		MethodResultJson = UAIModFunctionLibrary::LogRecipeCatalogAsJson(GetGameInstance());
	}
	else if (Method == TEXT("world.itemCatalog"))
	{
		MethodResultJson = UAIModFunctionLibrary::LogItemCatalogAsJson(GetGameInstance());
	}
	else if (Method == TEXT("world.buildableCatalog"))
	{
		MethodResultJson = UAIModFunctionLibrary::LogBuildableCatalogAsJson(GetGameInstance());
	}
	else if (Method == TEXT("world.constructionCost"))
	{
		const TSharedPtr<FJsonObject>* ParamsObjectPtr = nullptr;
		if (!RequestObject->TryGetObjectField(TEXT("params"), ParamsObjectPtr) || !ParamsObjectPtr || !ParamsObjectPtr->IsValid())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("Missing required 'params' object")));
			return true;
		}
		const TSharedPtr<FJsonObject> ParamsObject = *ParamsObjectPtr;

		FString RecipeClassPath;
		if (!ParamsObject->TryGetStringField(TEXT("recipeClass"), RecipeClassPath) || RecipeClassPath.IsEmpty())
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_REQUEST"), TEXT("params.recipeClass must be a non-empty string")));
			return true;
		}

		// Validated here, not inside LogConstructionCostAsJson - see
		// LogGroundHeightAsJson's dispatch entry for why: every Log*AsJson
		// function's result is unconditionally wrapped success:true, so a
		// real INVALID_RECIPE error can only be surfaced by checking before
		// calling it, same as world.groundHeight's params.
		UClass* ResolvedRecipeClass = LoadObject<UClass>(nullptr, *RecipeClassPath);
		if (!ResolvedRecipeClass || !ResolvedRecipeClass->IsChildOf(UFGRecipe::StaticClass()))
		{
			OnComplete(MakeErrorResponse(EHttpServerResponseCodes::BadRequest, RequestId, TEXT("INVALID_RECIPE"),
				FString::Printf(TEXT("'%s' did not resolve to a UFGRecipe subclass"), *RecipeClassPath)));
			return true;
		}

		MethodResultJson = UAIModFunctionLibrary::LogConstructionCostAsJson(GetGameInstance(), RecipeClassPath);
	}
	else if (Method == TEXT("world.targetedManufacturer"))
	{
		MethodResultJson = UAIModFunctionLibrary::LogTargetedManufacturerAsJson(GetGameInstance());
	}
	else if (Method == TEXT("world.player"))
	{
		MethodResultJson = UAIModFunctionLibrary::LogPlayerAsJson(GetGameInstance());
	}
	else if (Method == TEXT("world.timeOfDay"))
	{
		MethodResultJson = UAIModFunctionLibrary::LogTimeOfDayAsJson(GetGameInstance());
	}
	else if (Method == TEXT("world.chatHistory"))
	{
		MethodResultJson = UAIModFunctionLibrary::LogChatHistoryAsJson(GetGameInstance());
	}
	else if (Method == TEXT("world.portableMiners"))
	{
		MethodResultJson = UAIModFunctionLibrary::LogPortableMinersAsJson(GetGameInstance());
	}
	else if (Method == TEXT("world.centralStorage"))
	{
		MethodResultJson = UAIModFunctionLibrary::LogCentralStorageAsJson(GetGameInstance());
	}
	else if (Method == TEXT("world.cleanupOrphanedFlowIndicators"))
	{
		// A real write operation (deletes actors) but takes no params, so
		// it fits this simple dispatch shape rather than the params-object
		// one. See CleanupOrphanedFlowIndicatorsAsJson's doc comment.
		MethodResultJson = UAIModFunctionLibrary::CleanupOrphanedFlowIndicatorsAsJson(GetGameInstance());
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

		MethodResultJson = UAIModFunctionLibrary::LogGroundHeightAsJson(GetGameInstance(), static_cast<float>(X), static_cast<float>(Y), static_cast<float>(ReferenceZ));
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
