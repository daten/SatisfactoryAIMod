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
 * Intended to bind to 127.0.0.1 only. The project-wide HTTPServer default
 * (Config/DefaultEngine.ini, [HTTPServer.Listeners] DefaultBindAddress)
 * is "any" (0.0.0.0) - NOT safe on its own - so this subsystem's port has
 * an explicit per-port ListenerOverrides entry forcing BindAddress=localhost.
 *
 * That ini override alone is NOT trustworthy: confirmed live (2026-08-24)
 * that the actual Steam-launched (packaged Shipping) game still binds
 * 0.0.0.0, because the override lives in this dev workspace's
 * project-level Config/DefaultEngine.ini, which doesn't propagate to the
 * separately Alpakit-deployed package. Root-cause packaging fix still
 * pending - see docs/networking-research.md. Until then, and as
 * defense-in-depth even after it's fixed, HandleRpcRequest itself rejects
 * any request whose FHttpServerRequest::PeerAddress isn't loopback,
 * regardless of what the socket is actually bound to.
 *
 * Single POST /rpc endpoint accepting
 * {"protocolVersion":1,"requestId":"...","method":"..."} and dispatching
 * by method name, per CLAUDE.md's protocol design/networking rules:
 * versioned protocol, structured errors, reject unknown methods, reject
 * malformed JSON, enforce a message-size limit, never deserialize
 * arbitrary executable objects. Read-only methods: "world.resourceNodes",
 * "world.buildables", "world.manufacturers", "world.connections",
 * "world.targetedManufacturer" (whatever manufacturer the local player
 * is currently looking at - "manufacturer":null if none). Write methods
 * (PLAN.md Phase 12, take a "params" object): "world.setClockSpeed"
 * ({"buildableId","clockSpeedPercent"}), "world.setRecipe"
 * ({"buildableId","recipeClass"}) - both delegate all validation to
 * UDocModFunctionLibrary's SetManufacturer* functions and only translate
 * FDocModOperationResult into the HTTP/JSON response shape here. This
 * class must not grow a generic "call any function by name" method
 * (CLAUDE.md's Safety and Stability Boundary).
 *
 * "world.placeBuilding" ({"recipeClass","x","y"}), "world.placeExtractor"
 * ({"nodeId"}), "world.testPowerConnection" (dry run, never touches the
 * save) and "world.connectPower" (real - both
 * {"buildableIdA","buildableIdB"}) (PLAN.md Phase 13/14) are GENUINELY
 * ASYNCHRONOUS methods - UDocModFunctionLibrary::ConstructBuildingAtPosition/
 * ConstructExtractorOnNode/ConstructPowerConnection's completion callback may fire well after
 * HandleRpcRequest returns (real-tick polling to resolve
 * UFGCDInitializing/CanConstruct(), typically 1 tick, capped ~2s) -
 * FHttpResultCallback is captured by value and invoked from the deferred
 * poll once the real result is known, per FHttpRequestHandler's own
 * documented "return true now, call OnComplete later" contract. On
 * success, result.buildableId names the constructed building
 * (session-local id, same caveat as every other id in this protocol) -
 * empty for methods that don't create a buildable.
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
