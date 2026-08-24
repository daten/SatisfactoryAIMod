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
#include "Engine/GameInstance.h"

namespace
{
	TUniquePtr<FHttpServerResponse> MakeJsonResponse(EHttpServerResponseCodes Code, const TSharedRef<FJsonObject>& Body)
	{
		FString JsonString;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
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
		Root->SetObjectField(TEXT("result"), MakeShared<FJsonObject>());
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
