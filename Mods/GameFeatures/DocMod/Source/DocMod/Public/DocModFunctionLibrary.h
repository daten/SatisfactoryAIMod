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
	 * Dismantles an existing buildable via the real, safe
	 * IFGDismantleInterface (Execute_CanDismantle/Execute_Dismantle) -
	 * NOT AActor::Destroy(), per CLAUDE.md's "No Direct Memory
	 * Manipulation"/"perform explicitly supported game operations" -
	 * AFGBuildable::Dismantle_Implementation() (FGBuildable.cpp) properly
	 * clears factory/circuit connections, dismantles attached wires,
	 * empties and locks inventories, and removes the buildable from
	 * AFGBuildableSubsystem before network-replicated destruction, the
	 * same real cleanup a player's in-game dismantle does. Added
	 * 2026-08-25 specifically so live RPC-driven testing (e.g. rotation
	 * calibration - docs/buildgun-driven-placement-research.md §4) can
	 * clean up stray test buildables between attempts instead of
	 * accumulating them or relying on spacing/error-text alone to avoid
	 * cross-test collisions.
	 *
	 * Synchronous - no build gun/hologram involved, unlike construction.
	 * Fails with TARGET_NOT_FOUND if the id doesn't resolve, or
	 * CANNOT_DISMANTLE (with Execute_CanDismantle()'s reason where
	 * available) if the buildable refuses - e.g. an already-dismantled
	 * actor, or one with an un-dismantled parent (integrated sub-
	 * buildables like railroad platform track).
	 *
	 * Also handles "lightweight:<ClassPath>|<Index>" ids (2026-08-25 -
	 * see docs/lightweight-buildable-research.md): foundations, and
	 * likely other mass-placed pieces, aren't AFGBuildable actors at all
	 * - they're FRuntimeBuildableInstanceData in
	 * AFGLightweightBuildableSubsystem. For these, materializes a real
	 * temporary AFGBuildable* via FindOrSpawnBuildableForRuntimeData()
	 * and reuses the exact same Execute_Dismantle() call -
	 * AFGBuildable::Dismantle_Implementation() already has a dedicated
	 * branch that correctly removes the lightweight instance for us.
	 */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FDocModOperationResult DismantleBuildable(UObject* WorldContextObject, const FString& BuildableId);

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
	 *
	 * RotationScrollDelta (2026-08-25, live-calibrated - see
	 * docs/buildgun-driven-placement-research.md §4 for the full
	 * investigation): applied as AFGHologram::Scroll(sign) called
	 * |RotationScrollDelta| times (NOT Scroll(RotationScrollDelta) once
	 * - a single call with |N|>1 was found to behave identically to
	 * Scroll(1), and negative values didn't decrement, matching a
	 * per-call clamped input handler built for one wheel notch per
	 * call) before the placement poll begins. NOT camera-direction-
	 * dependent (ruled out live). The resulting orientation is
	 * context/terrain-dependent, not a fixed default or a clean
	 * degrees-per-unit constant: on a foundation, rotation appears to
	 * hard-snap to 90-degree multiples; on raw terrain, one live
	 * calibration point measured Scroll(1) ~= +10 degrees, but only a
	 * narrow window of deltas kept the placement's aim location valid
	 * at that specific spot - ground slope constrains which
	 * orientations are even valid. Pass 0 for the pre-rotation-control
	 * behavior (still context-dependent, just no additional scroll).
	 * Treat any delta as a candidate to verify via the constructed
	 * buildable's real rotation.yaw (world.buildables), not a
	 * guaranteed/precomputed result.
	 *
	 * GridSnapSize (2026-08-25): if > 0, X and Y are each rounded to the
	 * nearest multiple of this value before ground-tracing/placement.
	 * Added per explicit project direction to snap buildables to the
	 * world grid when possible, so layout planning
	 * (controller/satisfactory_ai/layout.py) works with tidy, predictable
	 * coordinates instead of arbitrary floats carried forward from live
	 * telemetry. This is a caller-chosen general-purpose snap, distinct
	 * from and in addition to whatever per-hologram-class grid snapping
	 * FactoryGame's own AFGBuildableHologram::mGridSnapSize applies
	 * internally (protected, no public accessor, varies per building
	 * type - not read or relied on here). Pass 0 to disable (the prior
	 * behavior).
	 *
	 * ReferenceZ (2026-08-25): anchors the ground trace's vertical search
	 * range (+/-1000 units) to this Z instead of the player's current Z.
	 * Found live to matter, not just in theory: the player's elevation
	 * at call time is otherwise load-bearing for where a building
	 * actually lands - if the player is somewhere unrelated (standing on
	 * top of another building, on a walkway far above/below the real
	 * target), the trace can miss real terrain and fall back to that
	 * irrelevant Z, landing nowhere near the intended spot on any axis.
	 * Pass a KNOWN, FIXED reference (e.g. an existing buildable's own Z
	 * from world.buildables) for deterministic placement independent of
	 * where the player happens to be standing - the recommended approach
	 * for any multi-step layout (foundations, then buildings on them)
	 * rather than relying on player position at every step. Sentinel
	 * -1000000 (an unrealistic in-game Z) means "not provided", which
	 * preserves the prior player-Z-anchored behavior.
	 *
	 * bIgnoreAimLocation/bIgnorePlayerEncroachment/bIgnoreClearance/
	 * bIgnoreInvalidFloor (2026-08-25, all default false = today's
	 * strict behavior): per explicit user direction - player-proximity/
	 * camera-direction gates don't scale for large, autonomous,
	 * multi-building layouts, and the user explicitly accepts the risk
	 * of invalid terrain collisions in exchange. When set, the
	 * corresponding UFGCDInvalidAimLocation/UFGCDEncroachingPlayer/
	 * UFGCDEncroachingClearance/UFGCDInvalidFloor disqualifier class is
	 * excluded from DocMod's own "does this block construction" gate
	 * (which replicates the documented "any non-soft disqualifier
	 * blocks" rule itself here, rather than calling the real
	 * AFGHologram::CanConstruct() - see this function's .cpp for why:
	 * that function's actual logic is unreadable, stub source). Every
	 * OTHER disqualifier (structural validity, resource requirements,
	 * snap requirements, etc.) still applies normally - this is a
	 * scoped, named bypass of specific UX-only gates, not a generic
	 * "ignore everything" switch. Still calls the real
	 * InternalConstructHologram() either way - FactoryGame's own
	 * server-side validation inside that function, if any, is
	 * unverified from source and not bypassed by these flags.
	 */
	static void ConstructBuildingAtPosition(UObject* WorldContextObject, const FString& RecipeClassPath, float X, float Y, int32 RotationScrollDelta, float GridSnapSize, float ReferenceZ, bool bIgnoreAimLocation, bool bIgnorePlayerEncroachment, bool bIgnoreClearance, bool bIgnoreInvalidFloor, TFunction<void(const FDocModOperationResult&)> OnComplete);

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
	 * of the DebugCheckPowerConnection dry-run - calls
	 * AFGWireHologram::SetConnection(0/1, ...) directly with a single
	 * GetHitResult() assignment for ConnectionA, no click/snap step.
	 *
	 * CONFIRMED LIVE, TWICE (2026-08-25), that this exact mechanism is
	 * the only one that works at all for machine<->machine connections -
	 * two mechanism deviations were tried and both regressed it: (1) a
	 * click-based UpdateHologramPlacement()+TrySnapToActor()+
	 * DoMultiStepPlacement() rewrite mirroring belts/pipes -
	 * TrySnapToActor() never populated AFGWireHologram::GetConnection(0)/(1)
	 * for wires at all; (2) also setting GetHitResult() for ConnectionB
	 * (not just A) before SetConnection(1,...) - broke ConnectionA's own
	 * validation. Do not repeat either without new evidence.
	 *
	 * SEPARATELY diagnosed (also 2026-08-25, same mechanism, no code
	 * change): repeated identical dry-run calls against the exact same
	 * pair of buildables returned THREE DIFFERENT disqualifiers across
	 * attempts - UFGCDWireSnap ("Must be hooked up to a connection!"),
	 * UFGCDWireTooLong ("Wire is too long!" - despite the real 3D
	 * distance being well under the real queried maxLength=10000), and
	 * UFGCDInvalidAimLocation ("Invalid aim location!") - with success
	 * on other attempts, still no change. This matches the same class of
	 * live-camera-dependent flakiness already solved for building
	 * placement (ConstructBuildingAtPosition's bIgnoreAimLocation etc.),
	 * not a genuine geometry problem this function's own logic gets
	 * wrong.
	 *
	 * Added bIgnoreAimLocation/bIgnoreWireSnap (mirroring
	 * ConstructBuildingAtPosition's named, per-disqualifier bypass
	 * pattern - manually walks GetConstructDisqualifiers() instead of
	 * trusting the opaque CanConstruct() bool, skipping only the named
	 * classes the caller opts into ignoring) to test whether bypassing
	 * these two resolves the flakiness. UFGCDWireTooLong is deliberately
	 * NOT ignorable - presumed to reflect the real, deterministic
	 * mMaxLength check, unlike the other two. NOT YET LIVE-VERIFIED to
	 * actually resolve the flakiness, only diagnosed and hypothesized.
	 *
	 * Same bDryRun switch and real-construction posture as
	 * ConstructExtractorOnNode/ConstructBuildingAtPosition: only calls
	 * InternalConstructHologram() once CanConstruct() genuinely resolves
	 * true. Not a UFUNCTION - same reason as the other async entry
	 * points.
	 *
	 * See docs/conveyor-power-connection-research.md's separate note on
	 * the pole-vs-daisy-chain gameplay constraint that may make direct
	 * machine-to-machine connection unavailable depending on the save's
	 * progression state - a CANNOT_CONSTRUCT/NO_POWER_CONNECTION result
	 * may correctly reflect that, not indicate a bug; distinct from the
	 * disqualifier-flakiness this addition targets.
	 */
	static void ConstructPowerConnection(UObject* WorldContextObject, const FString& BuildableIdA, const FString& BuildableIdB, bool bDryRun, bool bIgnoreAimLocation, bool bIgnoreWireSnap, TFunction<void(const FDocModOperationResult&)> OnComplete);

	/**
	 * PLAN.md Phase 13/14: the smallest possible conveyor belt
	 * experiment - deliberately NOT a full placement attempt. Per
	 * docs/conveyor-power-connection-research.md's plan ("start even
	 * smaller: confirm a single TrySnapToActor call can fix a start
	 * point before attempting the full sequence"), this spawns a real
	 * AFGConveyorBeltHologram (via HotKeyRecipe(Recipe_ConveyorBeltMk1)),
	 * finds a free Output UFGFactoryConnectionComponent on
	 * SourceBuildableId, feeds a synthetic FHitResult at that
	 * connection's location through three different entry points in
	 * sequence (UpdateHologramPlacement(), TrySnapToActor(), a single
	 * DoMultiStepPlacement() "click"), logging the build step/
	 * IsConnectionSnapped()/GetAnyConnectedBuildables() state after each
	 * - widened from a single TrySnapToActor() call after that alone
	 * produced contradictory evidence (returned true but no state
	 * indicator actually changed) on the first live test. Never checks
	 * CanConstruct() or calls Construct(). No polling, nothing deferred -
	 * reports synchronously. Never touches the save.
	 */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FDocModOperationResult DebugCheckConveyorSnap(UObject* WorldContextObject, const FString& SourceBuildableId);

	/**
	 * PLAN.md Phase 13/14: builds on DebugCheckConveyorSnap's finding
	 * (2026-08-25, live) that a belt's start point genuinely snaps via
	 * TrySnapToActor() + a single DoMultiStepPlacement(true) "click"
	 * together - neither alone was sufficient, but combined the build
	 * step advanced from SHBS_FindStart straight to
	 * SHBS_PlacePoleOrSnapEnding and GetAnyConnectedBuildables() went
	 * from empty to containing the source buildable. This repeats that
	 * same TrySnapToActor+click pair a second time at the destination's
	 * free Input connection, then polls real ticks for the same
	 * UFGCDInitializing disqualifier seen on the first click to clear
	 * (identical pattern to every other build-gun-driven function in
	 * this file) before checking CanConstruct(). Same bDryRun switch and
	 * real-construction posture as ConstructPowerConnection - only calls
	 * InternalConstructHologram() once CanConstruct() genuinely resolves
	 * true. Whether the second click actually completes the sequence
	 * (DoMultiStepPlacement returning true) is itself still an open
	 * question this function's first live run will answer - if it
	 * doesn't, the real result will report exactly that rather than
	 * guessing further blindly. Not a UFUNCTION - same reason as the
	 * other async entry points.
	 *
	 * RecipeClassPath (2026-08-25, was hardcoded to Recipe_ConveyorBeltMk1
	 * before this): any of Recipe_ConveyorBeltMk1..Mk6 (all six exist on
	 * disk, confirmed) resolve the same way ConstructBuildingAtPosition's
	 * recipe param does - loaded and required to be a real UFGRecipe.
	 * Belt tier selection by desired throughput, or a too-far-apart
	 * source/dest needing multiple chained segments, is deliberately NOT
	 * done here or anywhere in DocMod - see LogConveyorBeltTiersAsJson/
	 * "world.conveyorBeltTiers" for each tier's real queried speed/
	 * maxSplineLength/bendRadius/maxInclineDegrees, and
	 * controller/satisfactory_ai/layout.py and
	 * controller/satisfactory_ai/conveyors.py for where a rate->tier or
	 * routing decision should live instead, per this project's
	 * established toolkit-not-solver direction. This function connects
	 * exactly one source connector to one dest connector in a single
	 * belt segment - SourceBuildableId/DestBuildableId are NOT required
	 * to be machines: any AFGBuildable with a free
	 * UFGFactoryConnectionComponent works, which includes belts
	 * themselves (AFGBuildableConveyorBase::GetConnection0()/
	 * GetConnection1() are UFGFactoryConnectionComponents too) - chaining
	 * multiple calls (machine -> belt, belt -> belt, belt -> machine)
	 * should therefore let an agent build multi-segment routes, though
	 * this has NOT been live-tested, only every machine-to-machine
	 * single-segment case has.
	 */
	static void ConstructConveyorBelt(UObject* WorldContextObject, const FString& SourceBuildableId, const FString& DestBuildableId, const FString& RecipeClassPath, bool bDryRun, TFunction<void(const FDocModOperationResult&)> OnComplete);

	/**
	 * Telemetry, not a mutation - same LogXAsJson convention as
	 * LogConveyorBeltTiersAsJson/LogPipelineTiersAsJson. Added 2026-08-25
	 * per the user's request to research/verify/build splitter+merger
	 * support.
	 *
	 * KEY FINDING (see docs/conveyor-attachment-research.md): splitters
	 * and mergers use AFGConveyorAttachmentHologram : AFGFactoryHologram
	 * : AFGBuildableHologram - the SAME simple, single-step hologram
	 * lineage already proven for Miners/Smelters/Constructors, NOT the
	 * AFGSplineHologram branch belts/pipes needed special multi-click
	 * driving for. This means ConstructBuildingAtPosition/
	 * world.placeBuilding and ConstructConveyorBelt/world.connectConveyor
	 * (both already generic - source/dest never restricted to machines)
	 * place and connect splitters/mergers with ZERO new construction
	 * code - deliberately no ConstructSplitter-style wrapper was added.
	 *
	 * Reports the real recipe catalog (plain Splitter/Merger plus Smart/
	 * Programmable Splitter and Priority Merger variants - all five
	 * confirmed present on disk) with each variant's real
	 * "inputCount"/"outputCount", read generically via GetDirection() on
	 * each buildable class CDO's UFGFactoryConnectionComponents (the
	 * same technique world.connections itself uses) rather than
	 * hardcoding the commonly-known 1-in/3-out (splitter) / 3-in/1-out
	 * (merger) figures - AFGBuildableConveyorAttachment's header doesn't
	 * declare them as a literal constant anywhere. Also reports
	 * "supportsSortRules" (true only for the Smart/Programmable variants,
	 * which share the AFGBuildableSplitterSmart native class) - a real,
	 * separate, NOT-yet-built capability gap: per-output item-type
	 * routing (mSortRules/AddSortRule/etc., confirmed public on
	 * FGBuildableSplitterSmart.h) needs its own future write operation:
	 * placement/connection alone does not let an agent configure routing
	 * rules on a Smart/Programmable splitter yet.
	 */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FString LogConveyorAttachmentCatalogAsJson(UObject* WorldContextObject);

	/**
	 * Telemetry, not a mutation - follows the LogXAsJson return-a-JSON-
	 * string convention used elsewhere in this file (world.resourceNodes/
	 * world.buildables/etc.), not FDocModOperationResult.
	 *
	 * Returns AFGBuildableConveyorBase::GetSpeed() for the buildable
	 * class each of Recipe_ConveyorBeltMk1..Mk6 produces, read live from
	 * each class's CDO (LoadObject + GetDefaultObject) - not a hardcoded
	 * or assumed table. Added 2026-08-25 so belt-tier selection can be
	 * based on real queried data instead of remembered/assumed
	 * items-per-minute figures. NOTE: GetSpeed() is FactoryGame's own
	 * internal conveyor simulation speed (unit unconfirmed from source -
	 * AFGBuildableConveyorBase.h only comments it as "Speed of this
	 * conveyor", no unit given) - this is NOT necessarily
	 * items-per-minute directly. Treat the returned values as
	 * relative/comparable across tiers (useful for "pick the belt whose
	 * speed is at least this multiple of Mk1's" reasoning) until/unless
	 * a live comparison against the game's own displayed
	 * items-per-minute figures confirms the exact conversion - a recipe
	 * whose class fails to load is simply omitted from the result, not
	 * a hard error, so this still reports on whichever tiers succeed.
	 *
	 * Also reports, per tier (2026-08-25, all via
	 * ResolveConveyorBeltHologramClassForRecipe -> the HOLOGRAM class's
	 * CDO, a different descriptor accessor - UFGBuildDescriptor::
	 * GetHologramClass - than the buildable class GetSpeed() reads
	 * off): "maxSplineLength"/"bendRadius"
	 * (AFGConveyorBeltHologram::GetMaxSplineLength()/GetBendRadius(),
	 * public getters) and "maxInclineDegrees" (mMaxIncline, degrees per
	 * its own doc comment - no public getter exists, read via
	 * FindFProperty<FFloatProperty> reflection instead, a single
	 * hardcoded read-only field lookup, not a generic property-access
	 * capability). Added so an agent can tell BEFORE attempting a
	 * connection whether two connectors are too far apart or the
	 * required incline is too steep for a single belt segment, and
	 * needs either a taller/shorter platform or multiple chained
	 * segments instead - see ConstructConveyorBelt's doc comment on
	 * chaining. These three fields are omitted (not a hard error) for
	 * any tier whose hologram class doesn't resolve.
	 */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FString LogConveyorBeltTiersAsJson(UObject* WorldContextObject);

	/**
	 * Telemetry, not a mutation - same LogXAsJson convention as
	 * LogConveyorBeltTiersAsJson.
	 *
	 * Returns Recipe_PowerLine's buildable class (AFGBuildableWire) CDO's
	 * mMaxLength/mMaxPowerTowerLength/mLengthPerCost - all three are
	 * PUBLIC EditDefaultsOnly UPROPERTYs with a documented unit ("[cm]"
	 * per FGBuildableWire.h's own doc comments), unlike the belt tier
	 * data's ambiguous-unit GetSpeed() - no reflection needed, this is a
	 * plain public member read. Added 2026-08-25 directly motivated by
	 * the user's question about whether power (like conveyors) needs
	 * distance-limit/intermediate-pole handling - it does, and this is
	 * the real number to check a candidate connection's distance
	 * against. Only one power line tier exists in this game (no Mk1..N
	 * like belts), so this returns a single flat object, not an array.
	 *
	 * ConstructPowerConnection/world.connectPower's source/dest were
	 * already generic (FindFreePowerConnection searches any AFGBuildable
	 * for a free UFGPowerConnectionComponent via GetComponents<>(), not
	 * hardcoded to machines) - a real power pole
	 * (Recipe_PowerPoleMk1/Mk2/Mk3, confirmed present on disk) should
	 * therefore already work as an intermediate relay for a connection
	 * exceeding mMaxLength, chaining multiple world.connectPower calls,
	 * with NO C++ changes needed for that part - only untested live
	 * (every power connection built so far has been one direct
	 * machine-to-machine segment). See docs/conveyor-power-connection-research.md's
	 * pole-vs-daisy-chain note: a machine's default single power slot
	 * may require routing through a pole even for a SHORT connection if
	 * the daisy-chain unlock isn't active in the current save, separate
	 * from the mMaxLength distance question entirely.
	 */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FString LogPowerLineLimitsAsJson(UObject* WorldContextObject);

	/**
	 * Telemetry, not a mutation - same LogXAsJson convention as
	 * LogConveyorBeltTiersAsJson/LogPowerLineLimitsAsJson. Added
	 * 2026-08-25 for pipe groundwork, directly motivated by the user
	 * asking to prepare pipe handling ahead of the next live session,
	 * the same way belts/power were prepared.
	 *
	 * Reports Recipe_Pipeline and Recipe_PipelineMK2 (both confirmed on
	 * disk - note the capital "MK2", unlike belts' "Mk2"). Per tier:
	 * "flowLimit" is AFGBuildablePipeline::GetFlowLimit(), a PUBLIC
	 * getter with a documented unit ("Maximum flow through this pipe in
	 * cubic meters. [m^3/s]" per FGBuildablePipeline.h's own doc
	 * comment) - unlike belt speed, this is directly usable without a
	 * unit caveat. "maxSplineLength"/"bendRadius"/"minBendRadius" come
	 * from AFGPipelineHologram's CDO (resolved via
	 * ResolvePipelineHologramClassForRecipe, the same
	 * UFGBuildDescriptor::GetHologramClass pattern as the belt hologram
	 * lookup) - ALL THREE are private UPROPERTY(EditDefaultsOnly)
	 * fields with no public getter (unlike belts, where two of three
	 * had public getters), read via FindFProperty<FFloatProperty>
	 * reflection - a single hardcoded read-only field lookup per field,
	 * not a generic property-access capability, same justification as
	 * the belt incline read. Any tier/field that fails to resolve is
	 * simply omitted, not a hard error.
	 */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FString LogPipelineTiersAsJson(UObject* WorldContextObject);

	/**
	 * PLAN.md Phase 13/14 pipe groundwork (2026-08-25) - a deliberate
	 * near-exact mirror of ConstructConveyorBelt, same build-gun-driven
	 * two-click mechanism (TrySnapToActor + DoMultiStepPlacement(true)
	 * at the source connector, then again at the dest connector),
	 * applying every fix already discovered live for belts up front:
	 * UpdateHologramPlacement() before each TrySnapToActor(), and the
	 * connector's real GetConnectorNormal() (not a placeholder
	 * UpVector) in the synthetic FHitResult. AFGPipelineHologram is a
	 * sibling of AFGConveyorBeltHologram - both derive directly from
	 * AFGSplineHologram, confirmed from source - and
	 * UFGPipeConnectionComponentBase/EPipeConnectionType is pipes' own
	 * parallel connection-type hierarchy (PCT_PRODUCER/PCT_CONSUMER),
	 * NOT UFGFactoryConnectionComponent/EFactoryConnectionDirection.
	 *
	 * NOT YET LIVE-TESTED - unlike ConstructConveyorBelt, none of this
	 * has been run against a real game session; this is groundwork
	 * ahead of the next testing session, same posture as the belt-tier
	 * and power-limit telemetry added earlier tonight. Two known
	 * pipe-specific open questions going into that first live test:
	 * (1) AFGSplineHologram (the shared base) has no
	 * GetAnyConnectedBuildables() - only AFGConveyorBeltHologram
	 * declares that - so the post-end-click diagnostic here uses
	 * IsConnectionSnapped(false) instead, an indicator already noted
	 * (DebugCheckConveyorSnap's findings) as not fully reliable even
	 * for belts; (2) fluid type compatibility
	 * (UFGCDPipeFluidTypeMismatch, confirmed present in
	 * FGConstructDisqualifier.h) is NOT pre-validated here - the real
	 * CanConstruct() disqualifier check is trusted to catch it, same as
	 * every other disqualifier this function doesn't special-case; (3)
	 * no standalone Recipe_PipelineSupport/pole recipe was found on
	 * disk (unlike Recipe_ConveyorPole for belts) even though
	 * Build_PipelineSupport.uasset exists as a buildable - whether
	 * chaining ConstructPipe calls through an intermediate pole works
	 * the same way it does for belts/power, or whether pipe poles are
	 * only auto-spawned internally by the hologram's own
	 * mDefaultPipelineSupportRecipe mechanism during placement, is an
	 * open question for the first live pipe test to answer, not
	 * assumed either way here.
	 *
	 * RecipeClassPath: Recipe_Pipeline or Recipe_PipelineMK2 (see
	 * LogPipelineTiersAsJson/"world.pipelineTiers" for each tier's real
	 * queried flowLimit/maxSplineLength/bendRadius/minBendRadius). Tier
	 * selection and multi-segment routing decisions are deliberately
	 * NOT made here - see the planned controller/satisfactory_ai/pipes.py
	 * toolkit module for where that belongs, per this project's
	 * established toolkit-not-solver direction. SourceBuildableId/
	 * DestBuildableId are NOT required to be machines: any AFGBuildable
	 * with a free UFGPipeConnectionComponentBase works, same generic
	 * posture as ConstructConveyorBelt. Not a UFUNCTION - same reason
	 * as the other async entry points.
	 */
	static void ConstructPipe(UObject* WorldContextObject, const FString& SourceBuildableId, const FString& DestBuildableId, const FString& RecipeClassPath, bool bDryRun, TFunction<void(const FDocModOperationResult&)> OnComplete);
};
