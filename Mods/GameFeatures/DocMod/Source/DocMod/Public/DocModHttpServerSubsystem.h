// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HttpRequestHandler.h"
#include "HttpRouteHandle.h"
#include "DocModHttpServerSubsystem.generated.h"

class IHttpRouter;
struct FHttpServerRequest;

/**
 * PLAN.md Phase 9: localhost-only JSON RPC transport for the DocMod AI
 * interface. See docs/networking-research.md for the API research this
 * is built on.
 *
 * Binds to 127.0.0.1 only. The project-wide HTTPServer default
 * (Config/DefaultEngine.ini, [HTTPServer.Listeners] DefaultBindAddress)
 * is "any" (0.0.0.0) - NOT safe on its own - so this subsystem's port has
 * an explicit per-port ListenerOverrides entry forcing BindAddress=localhost.
 * Verify at runtime (docs/manual-verification.md) that the listening
 * socket is actually 127.0.0.1, not 0.0.0.0 - config parsing mistakes
 * would silently defeat this and aren't detectable from source alone.
 *
 * Single POST /rpc endpoint accepting
 * {"protocolVersion":1,"requestId":"...","method":"..."} and dispatching
 * by method name, per CLAUDE.md's protocol design/networking rules:
 * versioned protocol, structured errors, reject unknown methods, reject
 * malformed JSON, enforce a message-size limit, never deserialize
 * arbitrary executable objects. Read-only for now - only
 * "world.resourceNodes" is implemented. Phase 12+ will add mutation
 * methods with their own explicit validation; this class must not grow a
 * generic "call any function by name" method (CLAUDE.md's Safety and
 * Stability Boundary).
 *
 * A UGameInstanceSubsystem's HTTPServer route handler executes on the
 * game thread already (the module ticks via FTSTickerObjectBase on the
 * game thread), so no manual thread-marshaling is needed for this simple
 * read-only case.
 */
UCLASS()
class DOCMOD_API UDocModHttpServerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Must match the Port= value in Config/DefaultEngine.ini's ListenerOverrides entry. */
	static constexpr uint32 ListenPort = 51902;

	/** Hard cap on request body size, enforced before any JSON parsing. */
	static constexpr int64 MaxRequestBodyBytes = 64 * 1024;

private:
	bool HandleRpcRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);

	TSharedPtr<IHttpRouter> Router;
	FHttpRouteHandle RpcRouteHandle;
};
