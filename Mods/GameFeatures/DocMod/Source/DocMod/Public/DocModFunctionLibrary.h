// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DocModTelemetryTypes.h"
#include "DocModOperationTypes.h"
#include "DocModFunctionLibrary.generated.h"

/**
 * DocMod AI interface Blueprint entry points. PLAN.md Phase 3-12: a
 * version smoke test, read-only world telemetry (resource nodes,
 * buildables, manufacturers, factory connections) with JSON
 * serialization, and - as of Phase 12 - the first two controlled write
 * operations (SetManufacturerClockSpeed, SetManufacturerRecipe), each
 * with explicit validation per CLAUDE.md's Safety and Stability Boundary.
 * This class must never grow a generic "call any function by name" or
 * "set any property by name" method.
 */
UCLASS()
class DOCMOD_API UDocModFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Returns the DocMod AI interface version string. */
	UFUNCTION(BlueprintPure, Category = "DocMod|AI Interface", meta = (BlueprintThreadSafe))
	static FString GetInterfaceVersion();

	/**
	 * Enumerates all AFGResourceNode actors in the world via TActorIterator
	 * (see docs/resource-node-research.md for why - no working manager API
	 * was found) and returns them as normalized, protocol-facing telemetry
	 * structs (PLAN.md Phase 5, Task 8). Id is session-local only; see
	 * FDocModResourceNodeTelemetry's comment.
	 */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static TArray<FDocModResourceNodeTelemetry> GetResourceNodeTelemetry(UObject* WorldContextObject);

	/** Debug entry point: logs one human-readable line per resource node via LogDocModAI. */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static void LogResourceNodes(UObject* WorldContextObject);

	/**
	 * Serializes resource node telemetry to the Phase 6 JSON protocol
	 * shape ({"protocolVersion":1,"resourceNodes":[...]}), logs it via
	 * LogDocModAI, and returns the JSON string. Used internally by the
	 * "world.resourceNodes" RPC method (UDocModHttpServerSubsystem).
	 */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FString LogResourceNodesAsJson(UObject* WorldContextObject);

	/**
	 * Enumerates all placed AFGBuildable actors (PLAN.md Phase 10,
	 * "buildings"). Tries AFGBuildableSubsystem::GetAllBuildablesRef()
	 * first (a real public getter exists, per
	 * docs/buildable-research.md) and falls back to a TActorIterator
	 * scan if the subsystem isn't available - its .cpp is a stub in
	 * this repo, so whether it's actually populated at runtime is
	 * unverified.
	 */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static TArray<FDocModBuildableTelemetry> GetBuildableTelemetry(UObject* WorldContextObject);

	/** Debug entry point: logs one line per buildable via LogDocModAI. */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static void LogBuildables(UObject* WorldContextObject);

	/** Serializes buildable telemetry to {"protocolVersion":1,"buildables":[...]}, logs it, and returns it. */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FString LogBuildablesAsJson(UObject* WorldContextObject);

	/**
	 * Enumerates all AFGBuildableManufacturer actors and reads their
	 * current recipe, clock speed, production status/progress/
	 * productivity, and input/output inventory contents (PLAN.md
	 * Phase 10, "machine recipes" / "machine inventories" / "machine
	 * production status"). Read-only.
	 */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static TArray<FDocModManufacturerTelemetry> GetManufacturerTelemetry(UObject* WorldContextObject);

	/** Debug entry point: logs one line per manufacturer via LogDocModAI. */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static void LogManufacturers(UObject* WorldContextObject);

	/** Serializes manufacturer telemetry to {"protocolVersion":1,"manufacturers":[...]}, logs it, and returns it. */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FString LogManufacturersAsJson(UObject* WorldContextObject);

	/**
	 * Returns telemetry for the manufacturer the local player is
	 * currently looking at / can interact with
	 * (AFGCharacterPlayer::GetBestUsableActor() - the same game-owned
	 * "what am I aiming at" state that drives the "Press E to interact"
	 * prompt, not a reimplemented line trace). An empty-Id struct
	 * (Id.IsEmpty()) means nothing is targeted or the targeted actor
	 * isn't a manufacturer. Player index 0 only - single-player/local
	 * session scope, per PLAN.md/CLAUDE.md. Meant to let a caller target
	 * write operations (SetManufacturerClockSpeed/SetManufacturerRecipe)
	 * at whatever the player is currently looking at, instead of picking
	 * an id out of the full GetManufacturerTelemetry list.
	 */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FDocModManufacturerTelemetry GetTargetedManufacturer(UObject* WorldContextObject);

	/**
	 * Serializes GetTargetedManufacturer's result to
	 * {"protocolVersion":1,"manufacturer":null|{...}}, logs it, and
	 * returns it. "manufacturer" is JSON null when nothing/non-manufacturer
	 * is targeted, not an error.
	 */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FString LogTargetedManufacturerAsJson(UObject* WorldContextObject);

	/**
	 * Enumerates factory connection points on every AFGBuildableFactory
	 * actor (PLAN.md Phase 10, "conveyor connection components"). One row
	 * per connection point, not a constructed graph - see
	 * FDocModFactoryConnectionTelemetry's comment. This is the raw
	 * material Phase 11's external-controller-side world graph is built
	 * from, alongside GetBuildableTelemetry.
	 */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static TArray<FDocModFactoryConnectionTelemetry> GetFactoryConnectionTelemetry(UObject* WorldContextObject);

	/** Debug entry point: logs one line per connection point via LogDocModAI. */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static void LogFactoryConnections(UObject* WorldContextObject);

	/** Serializes connection telemetry to {"protocolVersion":1,"connections":[...]}, logs it, and returns it. */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FString LogFactoryConnectionsAsJson(UObject* WorldContextObject);

	/**
	 * PLAN.md Phase 12: first controlled write operation. Sets the clock
	 * speed (FactoryGame's "potential") on an existing manufacturing
	 * building, identified by the session-local id from
	 * GetManufacturerTelemetry. Validates, in order: buildable exists and
	 * is a manufacturer (TARGET_NOT_FOUND), the building actually allows
	 * changing potential (OPERATION_NOT_PERMITTED), and the requested
	 * percent is within [GetCurrentMinPotential(),GetCurrentMaxPotential()]
	 * (INVALID_CLOCK_SPEED, message includes the valid range). Uses
	 * SetPendingPotential() - per FactoryGame's own doc comment, the
	 * change takes effect at the NEXT production cycle, not instantly.
	 */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FDocModOperationResult SetManufacturerClockSpeed(UObject* WorldContextObject, const FString& BuildableId, float ClockSpeedPercent);

	/**
	 * PLAN.md Phase 12: sets the recipe on an existing manufacturing
	 * building. Validates, in order: buildable exists and is a
	 * manufacturer (TARGET_NOT_FOUND), recipeClassPath resolves to an
	 * actual UFGRecipe subclass - this rejects any other class path, it
	 * is NOT a generic "load any class" capability, per CLAUDE.md's
	 * Safety and Stability Boundary (INVALID_RECIPE), the recipe is
	 * actually producible in this building's class via
	 * UFGRecipe::IsProducedIn (RECIPE_NOT_COMPATIBLE), and both input and
	 * output inventories are empty - FactoryGame's own SetRecipe doc
	 * comment: "It is up to the caller to make sure input and output
	 * inventories are empty before changing recipe" (INVENTORY_NOT_EMPTY).
	 */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FDocModOperationResult SetManufacturerRecipe(UObject* WorldContextObject, const FString& BuildableId, const FString& RecipeClassPath);

	/**
	 * Returns telemetry for the resource node the local player is
	 * currently aiming at, via the same
	 * AFGCharacterPlayer::GetBestUsableActor() GetTargetedManufacturer
	 * uses - AFGResourceNodeBase implements IFGUseableInterface
	 * (FGResourceNodeBase.h:93, note the spelling), confirmed live to be
	 * the same game state that drives the "Press E to start mining..."
	 * prompt. (An earlier version of this function used a hand-rolled
	 * view-angle heuristic based on a research gap that missed this -
	 * see docs/extractor-placement-research.md's correction note.) An
	 * empty-Id struct means nothing/non-node is targeted right now.
	 */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FDocModResourceNodeTelemetry GetTargetedResourceNode(UObject* WorldContextObject);

	/**
	 * PLAN.md Phase 13 (extractor placement), stage 2 of the staged
	 * verification plan in docs/manual-verification.md: a dry-run only,
	 * no-mutation experiment. Spawns a real AFGResourceExtractorHologram
	 * (via AFGHologram::SpawnHologramFromRecipe(Recipe_MinerMk1)) at the
	 * currently-targeted resource node (GetTargetedResourceNode), feeds
	 * it a synthetic FHitResult pointing at that node, then reports
	 * CanConstruct()'s real disqualifier list - then destroys the
	 * hologram. Never calls Construct() - this function cannot place a
	 * real building or touch the save. Scoped to solid resources only
	 * (Miner Mk1) for this first experiment; liquid/gas nodes (Water/Oil
	 * Extractor) are out of scope and return UNSUPPORTED_RESOURCE_FORM.
	 *
	 * PARTIALLY ASYNCHRONOUS: all validation up through spawning the
	 * hologram and snapping it happens synchronously and returns
	 * immediately on failure. If that all succeeds, the actual
	 * CanConstruct() check is deferred and polled across real engine
	 * ticks (found live, 2026-08-24: a freshly-spawned hologram reports
	 * a hard UFGCDInitializing disqualifier that neither an immediate
	 * check nor several manually-invoked Tick() calls clear - it appears
	 * to depend on a real per-frame engine cycle, plausibly an async
	 * clearance/overlap query per FGHologram.h's
	 * InitializeClearanceData()/PostInitializeClearanceData() split).
	 * In that case this function returns immediately with
	 * ErrorCode="PENDING" and the real result is logged to LogDocModAI
	 * once polling resolves (or a safety-cap number of ticks is hit).
	 * See docs/extractor-placement-research.md for the full trail of
	 * evidence this function exists to gather - the two load-bearing
	 * assumptions (synthetic-hit-result snapping, and hologram
	 * construction without a real AFGBuildGun) are still unverified
	 * against real runtime behavior.
	 */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FDocModOperationResult DebugCheckExtractorPlacementOnTargetedNode(UObject* WorldContextObject);

	/**
	 * PLAN.md Phase 13, second dry-run experiment: after
	 * DebugCheckExtractorPlacementOnTargetedNode's standalone hologram
	 * (spawned via AFGHologram::SpawnHologramFromRecipe, no real
	 * AFGBuildGun involved) consistently reported a hard
	 * UFGCDInitializing disqualifier across five independently-verified
	 * fix attempts (see docs/extractor-placement-research.md's "will not
	 * clear" section), this drives the REAL build gun flow instead, per
	 * docs/buildgun-driven-placement-research.md: calls
	 * AFGCharacterPlayer::HotKeyRecipe(Recipe_MinerMk1) (a real,
	 * ordinary player-facing hotkey feature - not something invented for
	 * this experiment) to equip the build gun and enter build mode,
	 * retrieves the resulting UFGBuildGunStateBuild/hologram via public
	 * accessors, feeds a synthetic FHitResult through
	 * AFGBuildGun::GetHitResult()'s mutable reference (bypassing the
	 * real camera trace), polls real ticks the same way as the standalone
	 * experiment, and always calls UnequipBuildGun() before returning to
	 * restore the player's prior equipped state.
	 *
	 * VISIBLE SIDE EFFECT, unlike the standalone experiment: this
	 * genuinely equips the build gun and shows the normal in-game
	 * build-mode HUD/hologram UI for the duration of the test, briefly
	 * replacing whatever the player currently has equipped.
	 *
	 * Same safety posture as the standalone experiment: never calls
	 * Construct()/Server_ConstructHologram - this cannot place a real
	 * building or touch the save. Same solid-resource-only scope
	 * (UNSUPPORTED_RESOURCE_FORM for liquid/gas nodes) and the same
	 * PARTIALLY ASYNCHRONOUS behavior (ErrorCode="PENDING" while polling,
	 * real result logged to LogDocModAI once resolved).
	 */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FDocModOperationResult DebugCheckExtractorPlacementViaBuildGun(UObject* WorldContextObject);
};
