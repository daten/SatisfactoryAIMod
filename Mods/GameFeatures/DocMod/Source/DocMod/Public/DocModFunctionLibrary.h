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
	 * Returns the local player character's current position/rotation
	 * (see FDocModPlayerTelemetry's comment for why this exists) - player
	 * index 0 only, single-player/local scope per PLAN.md/CLAUDE.md.
	 */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FDocModPlayerTelemetry GetPlayerTelemetry(UObject* WorldContextObject);

	/** Serializes GetPlayerTelemetry to {"protocolVersion":1,"position":{...},"rotation":{...}}, logs it, and returns it. */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FString LogPlayerAsJson(UObject* WorldContextObject);

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

	/**
	 * PLAN.md Phase 13: the first real write operation for building
	 * placement - places a real Miner Mk1 on the currently-targeted
	 * resource node. THIS IS A REAL MUTATION, unlike every other
	 * "extractor placement" function in this class - it genuinely
	 * constructs a building and touches the save.
	 *
	 * Runs the exact same validated flow as
	 * DebugCheckExtractorPlacementViaBuildGun (HotKeyRecipe, synthetic
	 * hit result via GetHitResult(), real-tick polling for
	 * CanConstruct()) - see docs/buildgun-driven-placement-research.md,
	 * confirmed live 2026-08-24 to resolve canConstruct=true after 1
	 * real tick. Only once CanConstruct() genuinely returns true does
	 * this go one step further than the dry-run: calls
	 * UFGBuildGunStateBuild::InternalConstructHologram() (the same
	 * function Server_ConstructHologram's RPC delegates to
	 * server-side - both public, calling it directly here skips only the
	 * client->server RPC serialization round-trip, a no-op in this
	 * same-process singleplayer/listen-server context) with a fresh
	 * FNetConstructionID from AFGBuildableSubsystem::GetNewNetConstructionID().
	 * If CanConstruct() ever returns false, this behaves identically to
	 * the dry-run - logs why, unequips the build gun, and never attempts
	 * construction.
	 *
	 * PARTIALLY ASYNCHRONOUS, same as the dry-run: returns immediately
	 * with ErrorCode="PENDING" while polling; the real outcome (including
	 * whether the target node reports occupied afterward, as
	 * confirmation) is logged to LogDocModAI once resolved.
	 *
	 * VISIBLE SIDE EFFECT: equips the build gun for the duration (same as
	 * the dry-run), unequipped afterward regardless of outcome.
	 */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FDocModOperationResult ConstructExtractorOnTargetedNode(UObject* WorldContextObject);

	/**
	 * PLAN.md Phase 13: generalizes ConstructExtractorOnTargetedNode
	 * beyond resource-node-anchored extractors - places any simple,
	 * single-step, non-snapping building recipe (production machines:
	 * Constructor/Assembler/Manufacturer/Smelter/Foundry/Refinery/etc.)
	 * a short distance in front of the player. REAL MUTATION - places an
	 * actual building and touches the save, same as
	 * ConstructExtractorOnTargetedNode.
	 *
	 * NOT for buildings that need snap points or multi-step placement
	 * (belts, pipes, walls, foundations, power lines) - those need
	 * different hologram-driving logic this function doesn't implement.
	 * Untested outside a couple of production-machine recipes; if
	 * CanConstruct() reports a real (non-Initializing) disqualifier for
	 * some other recipe class, that's the game correctly saying no, not
	 * a bug to work around here.
	 *
	 * Position: deliberately simple, not a real "solve valid placement"
	 * algorithm (explicit scope decision - see docs/manual-verification.md).
	 * A candidate X/Y is chosen 800 units in front of the player (their
	 * actor forward vector, horizontal only); a single vertical line
	 * trace (ECC_Visibility, player ignored) at that X/Y finds real
	 * ground and its actual hit result is used directly. If nothing is
	 * hit (e.g. no ground within 1000 units either way), falls back to a
	 * synthetic hit result at the player's own Z with an assumed
	 * FVector::UpVector normal. Either way, CanConstruct() is still the
	 * real gate - a bad spot (e.g. overlapping another building) is
	 * reported as CANNOT_CONSTRUCT, not silently forced.
	 *
	 * RecipeClassPath uses the same validation as SetManufacturerRecipe -
	 * must resolve to a real UFGRecipe subclass (INVALID_RECIPE if not).
	 * Same PARTIALLY ASYNCHRONOUS behavior and real-tick polling for
	 * CanConstruct() as ConstructExtractorOnTargetedNode - see that
	 * function's comment and docs/buildgun-driven-placement-research.md.
	 * Same visible side effect (equips the build gun for the duration).
	 */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FDocModOperationResult ConstructBuildingNearPlayer(UObject* WorldContextObject, const FString& RecipeClassPath);

	/**
	 * PLAN.md Phase 13/14: RPC-drivable building placement at an
	 * explicit position, for scenarios (like placing several buildings
	 * a controlled distance apart for a production chain) where "near
	 * the player, facing forward" isn't good enough. Not a UFUNCTION -
	 * TFunction callbacks aren't UHT-compatible - this is a plain C++
	 * entry point for UDocModHttpServerSubsystem specifically, not
	 * exposed to Blueprint.
	 *
	 * Same validated flow, scope, and safety posture as
	 * ConstructBuildingNearPlayer (HotKeyRecipe, real-tick polling for
	 * CanConstruct(), UpdateHologramPlacement() re-asserted every tick
	 * to make position deterministic - see
	 * docs/buildgun-driven-placement-research.md's §3 correction) -
	 * simple/single-step/non-snapping buildings only. The only
	 * difference: X/Y come directly from the caller instead of being
	 * computed from the player's position/facing; a vertical line trace
	 * at that X/Y (falling back to the player's own Z if nothing is hit)
	 * still finds the real ground height, same as the near-player
	 * version.
	 *
	 * Genuinely asynchronous, not "PENDING then log": OnComplete is
	 * invoked exactly once, either synchronously (for early validation
	 * failures) or after real-tick polling resolves - this is what lets
	 * UDocModHttpServerSubsystem hold an HTTP response open and reply
	 * with the real result instead of returning a placeholder over the
	 * wire.
	 */
	static void ConstructBuildingAtPosition(UObject* WorldContextObject, const FString& RecipeClassPath, float X, float Y, TFunction<void(const FDocModOperationResult&)> OnComplete);

	/**
	 * PLAN.md Phase 13/14: dry-run only, no-mutation experiment toward
	 * conveyor/power connections (see docs/conveyor-power-connection-research.md).
	 * Given two ALREADY-PLACED buildables (by session-local id, e.g. from
	 * ConstructBuildingAtPosition's result.buildableId), finds a free
	 * UFGPowerConnectionComponent on each (via generic
	 * Buildable->GetComponents<>(), the same pattern already proven for
	 * factory connections), spawns a real AFGWireHologram (via
	 * HotKeyRecipe(Recipe_PowerLine)), calls its public
	 * SetConnection(0/1, ...) with both components, and reports
	 * CanConstruct()'s real disqualifier list. Never calls Construct() -
	 * this cannot place a real wire or touch the save, same safety
	 * posture as DebugCheckExtractorPlacementOnTargetedNode.
	 *
	 * Exists specifically to test the untested assumption in
	 * docs/conveyor-power-connection-research.md: whether SetConnection()
	 * alone is sufficient, or whether AFGWireHologram's private
	 * CheckValidSnap()/CheckLength() need the real multi-step
	 * TrySnapToActor/DoMultiStepPlacement flow to have run first to
	 * populate some other internal state SetConnection() alone doesn't.
	 *
	 * Same PARTIALLY ASYNCHRONOUS behavior as the other build-gun-driven
	 * dry-run functions (ErrorCode="PENDING" while polling, real result
	 * logged to LogDocModAI once resolved) and the same visible
	 * build-gun-equip side effect.
	 */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FDocModOperationResult DebugCheckPowerConnection(UObject* WorldContextObject, const FString& BuildableIdA, const FString& BuildableIdB);

	/**
	 * PLAN.md Phase 13/14: RPC-drivable variant of
	 * ConstructExtractorOnTargetedNode - places a real Miner Mk1 on a
	 * resource node identified by session-local id (e.g. from
	 * GetResourceNodeTelemetry/"world.resourceNodes"), instead of
	 * requiring the player to actually be looking at it. Not a
	 * UFUNCTION - TFunction callbacks aren't UHT-compatible - a plain
	 * C++ entry point for UDocModHttpServerSubsystem, same shape as
	 * ConstructBuildingAtPosition.
	 *
	 * Same validated flow, scope, and safety posture as
	 * ConstructExtractorOnTargetedNode (solid resources only,
	 * GetPlacementLocation/GetPlacementRotation for the synthetic hit,
	 * UpdateHologramPlacement() re-asserted every poll tick,
	 * InternalConstructHologram() only once CanConstruct() genuinely
	 * resolves true) and genuinely asynchronous like
	 * ConstructBuildingAtPosition (OnComplete invoked once, with
	 * ResultBuildableId set on success).
	 */
	static void ConstructExtractorOnNode(UObject* WorldContextObject, const FString& NodeId, TFunction<void(const FDocModOperationResult&)> OnComplete);

	/**
	 * PLAN.md Phase 13/14: RPC-drivable, genuinely asynchronous variant
	 * of the DebugCheckPowerConnection dry-run - same validated flow
	 * (HotKeyRecipe(Recipe_PowerLine), free UFGPowerConnectionComponent
	 * lookup on each buildable, AFGWireHologram::SetConnection(0/1,...),
	 * real-tick polling for CanConstruct()), but with an added
	 * bDryRun switch: when false, calls InternalConstructHologram() once
	 * CanConstruct() genuinely resolves true (same real-construction
	 * pattern as ConstructExtractorOnNode/ConstructBuildingAtPosition -
	 * only ever constructs after a live, confirmed-true validation, never
	 * unconditionally). Not a UFUNCTION - same reason as the other
	 * async entry points.
	 *
	 * See docs/conveyor-power-connection-research.md for the open
	 * question this function's dry-run mode exists to answer (whether
	 * SetConnection() alone is sufficient), and its note on the
	 * pole-vs-daisy-chain gameplay constraint that may make direct
	 * machine-to-machine connection unavailable depending on the save's
	 * progression state - a CANNOT_CONSTRUCT/NO_POWER_CONNECTION result
	 * may correctly reflect that, not indicate a bug.
	 */
	static void ConstructPowerConnection(UObject* WorldContextObject, const FString& BuildableIdA, const FString& BuildableIdB, bool bDryRun, TFunction<void(const FDocModOperationResult&)> OnComplete);
};
