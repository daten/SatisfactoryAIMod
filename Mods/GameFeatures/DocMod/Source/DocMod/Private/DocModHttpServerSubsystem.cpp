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
}

void UDocModHttpServerSubsystem::Deinitialize()
{
	if (Router.IsValid() && RpcRouteHandle.IsValid())
	{
		Router->UnbindRoute(RpcRouteHandle);
	}
	Router.Reset();

	UE_LOG(LogDocModAI, Display, TEXT("DocMod HTTP server stopped"));

	Super::Deinitialize();
}

bool UDocModHttpServerSubsystem::HandleRpcRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	if (!IsLoopbackPeer(Request))
	{
		const FString PeerDescription = Request.PeerAddress.IsValid() ? Request.PeerAddress->ToString(/*bAppendPort=*/true) : TEXT("<unknown>");
		UE_LOG(LogDocModAI, Warning, TEXT("DocMod HTTP server: rejected non-loopback request from %s"), *PeerDescription);
		OnComplete(MakeErrorResponse(EHttpServerResponseCodes::Forbidden, TEXT(""), TEXT("FORBIDDEN"), TEXT("DocMod RPC only accepts loopback connections")));
		return true;
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

		// FHttpResultCallback is a TFunction, safe to copy - captured by
		// value so it stays alive until the deferred poll actually calls it.
		UDocModFunctionLibrary::ConstructBuildingAtPosition(GetGameInstance(), RecipeClassPath, static_cast<float>(X), static_cast<float>(Y), static_cast<int32>(RotationScrollDelta), static_cast<float>(GridSnapSize), static_cast<float>(ReferenceZ),
			bIgnoreAimLocation, bIgnorePlayerEncroachment, bIgnoreClearance, bIgnoreInvalidFloor,
			bHasTargetYaw, static_cast<float>(TargetYawDegrees),
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

		UDocModFunctionLibrary::ConstructExtractorOnNode(GetGameInstance(), NodeId,
			[OnComplete, RequestId](const FDocModOperationResult& Result)
			{
				OnComplete(MakeOperationResponse(Result, RequestId));
			});
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
