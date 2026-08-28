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
	 * Reads a bool property's live value out of DocMod's player-facing mod
	 * configuration (see DocModConfiguration.h - UDocModConfiguration,
	 * registered via UConfigManager::RegisterModConfiguration in
	 * UDocModHttpServerSubsystem::Initialize). Returns DefaultValue if the
	 * config manager/section/property can't be found (e.g. called before
	 * the game instance subsystem chain is ready) - never throws, never
	 * treats a missing config as an error, since these are all
	 * off-by-default safety/capability toggles where "can't read it" and
	 * "player left it off" should behave identically.
	 *
	 * Not a UFUNCTION - internal infrastructure shared between
	 * DocModFunctionLibrary.cpp's construction functions and
	 * DocModHttpServerSubsystem.cpp's loopback check, not part of the
	 * external RPC surface.
	 */
	static bool GetDocModConfigBool(UObject* WorldContextObject, const FString& PropertyName, bool DefaultValue);

	/** Float counterpart to GetDocModConfigBool - see its doc comment. */
	static float GetDocModConfigFloat(UObject* WorldContextObject, const FString& PropertyName, float DefaultValue);

	/**
	 * Serializes a real ground-trace query to
	 * {"protocolVersion":1,"found":bool,"x":X,"y":Y,"z":realZ,"normal":{...}}
	 * - added 2026-08-27 per explicit user request to make placement
	 * height deterministic without requiring the caller to already know
	 * that world.placeBuilding's "z" param is a +/-1000-unit ground-trace
	 * SEARCH CENTER, not a literal height (see
	 * docs/placement-lessons.md's real z/gridSnapSize semantics section).
	 * A caller can query the real ground Z at a given X/Y first, then
	 * pass the returned "z" straight back in as world.placeBuilding's
	 * ReferenceZ - guaranteed to match, no more guess-and-iterate.
	 *
	 * ReferenceZ works exactly like ConstructBuildingAtPosition's own
	 * param: anchors the search, defaults to the player's current Z if
	 * omitted (sentinel <= -1000000). Uses the exact same trace as actual
	 * construction (FindGroundAtXY, factored out of
	 * ConstructBuildingAtPosition specifically so this can't drift out
	 * of sync with what a real placement would resolve to). "found":false
	 * means the trace hit nothing within +/-1000 units of ReferenceZ -
	 * "z" in that case is just the literal search center, matching what
	 * ConstructBuildingAtPosition itself falls back to.
	 */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FString LogGroundHeightAsJson(UObject* WorldContextObject, float X, float Y, float ReferenceZ);

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
	 * Same purpose as GetFactoryConnectionTelemetry, for pipes
	 * (UFGPipeConnectionComponentBase - covers both fluid pipes and
	 * hypertubes, see FDocModPipeConnectionTelemetry's comment). Added
	 * 2026-08-27 after discovering live that "world.connections" only
	 * ever covered factory connections, leaving no way to read a real
	 * pipe/hypertube connector's position/normal before placing one -
	 * exactly the data needed to plan a straight run instead of guessing
	 * rotation and hoping.
	 */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static TArray<FDocModPipeConnectionTelemetry> GetPipeConnectionTelemetry(UObject* WorldContextObject);

	/** Debug entry point: logs one line per pipe/hypertube connection point via LogDocModAI. */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static void LogPipeConnections(UObject* WorldContextObject);

	/** Serializes pipe connection telemetry to {"protocolVersion":1,"connections":[...]}, logs it, and returns it. */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FString LogPipeConnectionsAsJson(UObject* WorldContextObject);

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
	 * Serializes the current in-game time of day to
	 * {"protocolVersion":1,"hour":H,"minute":M,"daySeconds":S,"isDay":bool}
	 * via AFGTimeOfDaySubsystem::Get()'s own GetHours()/GetMinutes()/
	 * GetDaySeconds()/IsDay() - added 2026-08-27 so a caller can check the
	 * current time before deciding whether to call SetTimeOfDay.
	 */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FString LogTimeOfDayAsJson(UObject* WorldContextObject);

	/**
	 * Forces the in-game time of day to a specific hour/minute - added
	 * 2026-08-27 per explicit user request: the day/night cycle made it
	 * hard to visually observe live builds once it went dark, and they
	 * wanted to be able to reset to a specific daylight time on demand.
	 *
	 * AFGTimeOfDaySubsystem (FGTimeSubsystem.h) exposes a public, plain
	 * C++ SetDaySeconds(float) - not BlueprintCallable, and not exposed
	 * to the generic RPC surface directly, since this class must never
	 * grow a "call any function by name" method (see this class's header
	 * doc comment). The real hour/minute-based setter,
	 * UFGCheatManager::SetTimeOfDay(int32 hour, int32 minute), exists
	 * specifically to convert hour/minute to seconds via
	 * AFGTimeOfDaySubsystem's own SECONDS_PER_HOUR/SECONDS_PER_MINUTE
	 * constants (AFGTimeOfDaySubsystem declares
	 * "friend class UFGCheatManager;" for exactly this) and route through
	 * Server_SetTimeOfDay - but reaching a real UFGCheatManager instance
	 * depends on cheats being enabled for the local player controller,
	 * which isn't guaranteed in an ordinary session. Since DocMod's own
	 * code has full engine access (not a Blueprint-only caller), this
	 * calls AFGTimeOfDaySubsystem::SetDaySeconds() directly with
	 * Hour*SECONDS_PER_HOUR + Minute*SECONDS_PER_MINUTE, then
	 * ForceReplicateTimeToClients() (public, exists specifically to push
	 * a manual time change out) so the change is visible immediately
	 * rather than waiting for the next periodic sync.
	 *
	 * Fails with INVALID_REQUEST if Hour is outside [0,23] or Minute is
	 * outside [0,59]. Does not persist as a "fixed" time - the day/night
	 * cycle continues advancing normally from the new time, per
	 * mUpdateTime/the normal Tick() behavior; this is a one-shot jump,
	 * not a pause.
	 */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FDocModOperationResult SetTimeOfDay(UObject* WorldContextObject, int32 Hour, int32 Minute);

	/**
	 * Serializes the local player's chat history (AFGChatManager::
	 * GetReceivedChatMessages()) to
	 * {"protocolVersion":1,"messages":[{"sender","text","type","timestamp","isLocalPlayerMessage"},...]}
	 * - added 2026-08-27 per explicit user request for optional two-way
	 * chat with the player.
	 *
	 * This is genuinely two-way without any extra plumbing: a message the
	 * player types in the normal in-game chat box flows through the
	 * ordinary game chat pipeline into this same
	 * AFGChatManager::mReceivedMessages array SendChatMessage below
	 * writes to - there is no separate "player input" channel to poll.
	 * "type" is one of "PlayerMessage"/"SystemMessage"/"AdaMessage"/
	 * "CustomMessage" (EFGChatMessageType). No "since last call"
	 * filtering exists here - the caller is expected to track its own
	 * high-water mark (e.g. by count or by "timestamp") between polls,
	 * per CLAUDE.md's "external controller" responsibilities.
	 *
	 * NOTE: messages the player sends with a leading "/" never reach
	 * this array at all - SML's own AFGPlayerController::
	 * ChatMessageEntered hook diverts anything "/"-prefixed to chat
	 * command dispatch (this is also how DocMod's own "/docmod" command
	 * works - see DocModChatCommand.cpp) before it's ever added as a
	 * normal chat message.
	 */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FString LogChatHistoryAsJson(UObject* WorldContextObject);

	/**
	 * Posts a message into the player's in-game chat - added 2026-08-27
	 * per explicit user request so the AI controller can optionally talk
	 * back to the player through Satisfactory's own chat UI, not just
	 * this process's own logs/console.
	 *
	 * Uses AFGChatManager::AddChatMessageToReceived() - the same
	 * mechanism SML's own USMLRemoteCallObject::SendChatMessage_
	 * Implementation uses (SMLRemoteCallObject.cpp) - rather than
	 * BroadcastChatMessage()'s NetMulticast RPC, since AddChatMessageToReceived
	 * is the simpler, already-proven-in-this-codebase path and this
	 * project is explicitly single-player/local-session scope (PLAN.md/
	 * CLAUDE.md), where there's no separate remote client to multicast
	 * to. Defaults Sender to "DocMod AI" and MessageType to
	 * EFGChatMessageType::CMT_CustomMessage (a distinct visual style from
	 * ordinary player chat) if not overridden. Fails with INVALID_REQUEST
	 * if Message is empty.
	 *
	 * No rate limiting is implemented (flagged, not built - the real
	 * FGChatManager.cpp doesn't visibly enforce any either, per this
	 * feature's own research, so this matches the base game's own
	 * behavior rather than under- or over-restricting it) - a caller
	 * hammering this repeatedly could still flood the player's chat.
	 */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FDocModOperationResult SendChatMessage(UObject* WorldContextObject, const FString& Message, const FString& Sender);

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
	 *
	 * Also dismantles any IFGDismantleInterface::GetChildDismantleActors()
	 * children first (fixed 2026-08-27) - some buildables reference a
	 * separate, independently-existing actor that Dismantle_Implementation()
	 * itself does not clean up (e.g. AFGBuildablePipeline's mFlowIndicator,
	 * a distinct AFGBuildablePipelineFlowIndicator actor, not a child
	 * component). The real player-driven dismantle path
	 * (UFGBuildGunStateDismantle) consults GetChildDismantleActors;
	 * Execute_Dismantle() alone does not. Without this, deleting a pipe
	 * left its fluid-fill indicator floating in place after the pipe
	 * itself disappeared - confirmed live, reported by the user.
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
	 * Spawns a real AFGCreature near the player, added 2026-08-28 per
	 * explicit user request. Gated behind the "AllowCreatureSpawning" mod
	 * setting (see DocModConfiguration.h), OFF by default - unlike
	 * construction, there is no existing in-game equivalent of "a player
	 * manually spawns a creature", so this is treated as the same
	 * category of player-opt-in-only capability as bUnlimitedResources,
	 * not something an external AI controller can ever enable itself.
	 *
	 * CreatureClassPath must resolve to a real TSubclassOf<AFGCreature> -
	 * same narrow "load and validate one specific type" pattern as
	 * RecipeClassPath elsewhere in this file (INVALID_CREATURE_CLASS if
	 * not), not a generic "spawn any actor" capability - see CLAUDE.md's
	 * Safety and Stability Boundary.
	 *
	 * Genuinely synchronous, unlike the buildable placement functions -
	 * AFGCreatureSubsystem::BeginSpawningCreature is a plain (non-UFUNCTION,
	 * non-RPC) public C++ function that returns the spawned AFGCreature*
	 * directly, so no hologram/real-tick polling is needed here.
	 *
	 * DistanceFromPlayer is clamped to [100, 5000] units in front of the
	 * player; the actual spawn Z comes from a real ground trace at that
	 * X/Y (see FindGroundAtXY), same fallback-to-flat behavior as
	 * ConstructBuildingNearPlayer if nothing is hit.
	 */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FDocModOperationResult SpawnCreatureNearPlayer(UObject* WorldContextObject, const FString& CreatureClassPath, float DistanceFromPlayer);

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
	 *
	 * bHasTargetYaw/TargetYawDegrees (2026-08-27): RotationScrollDelta's
	 * "call AFGHologram::Scroll(+-1) N times in a tight synchronous loop"
	 * approach (see the .cpp's calibration comment) was live-confirmed
	 * this session to be NON-LINEAR for |N|>1 - a sweep of delta=-1..-9
	 * against the same recipe/location produced resolved yaws with no
	 * consistent per-click increment (e.g. -10, +70, +40, 0, -50, -110,
	 * -180, +90, +170 degrees - not a monotonic or evenly-spaced
	 * sequence). Root cause unconfirmed (stub source), but calling
	 * Scroll() repeatedly with zero real ticks between calls is the
	 * prime suspect, since a real player's mouse-wheel notches are never
	 * that close together. Rather than chase Scroll()'s internal
	 * behavior further, bHasTargetYaw lets the caller specify the exact
	 * final world yaw directly - when true, RotationScrollDelta is
	 * ignored entirely and the hologram's actor rotation is force-set to
	 * FRotator(0, TargetYawDegrees, 0) every poll tick (same
	 * re-assertion pattern as the deterministic-look fix above, since
	 * UpdateHologramPlacement() may re-derive yaw each tick). This is
	 * the recommended way to get a specific orientation reliably -
	 * RotationScrollDelta remains for callers that only care about SOME
	 * rotation being applied, not a specific one.
	 *
	 * FaceBuildableId (2026-08-27, optional, empty = unused): resolves an
	 * existing buildable's real position (FindBuildableById) and computes
	 * TargetYawDegrees from it automatically - (Target - PlacementLocation)
	 * .Rotation().Yaw - instead of requiring the caller to fetch that
	 * buildable's position separately and do the vector math themselves.
	 * Takes priority over an explicit bHasTargetYaw/TargetYawDegrees if
	 * both are given. Fails with FACE_TARGET_NOT_FOUND if the id doesn't
	 * resolve. Added per explicit user request to make orientation
	 * "not require special knowledge on the side of the agent" - this
	 * automates the exact manual "read connector normal, compute delta,
	 * rotate, reverify" dance repeated all session for splitters/mergers/
	 * hypertube entrances. Only orients the WHOLE building to face the
	 * target's position - does not (yet) reason about which SPECIFIC
	 * connector on a multi-connector building ends up facing it.
	 */
	static void ConstructBuildingAtPosition(UObject* WorldContextObject, const FString& RecipeClassPath, float X, float Y, int32 RotationScrollDelta, float GridSnapSize, float ReferenceZ, bool bIgnoreAimLocation, bool bIgnorePlayerEncroachment, bool bIgnoreClearance, bool bIgnoreInvalidFloor, bool bHasTargetYaw, float TargetYawDegrees, const FString& FaceBuildableId, TFunction<void(const FDocModOperationResult&)> OnComplete);

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
	 *
	 * FIXED 2026-08-26 - live-diagnosed a real, reproducible regression:
	 * consistently failed with UFGCDNeedsResourceNode ("Must be placed
	 * on a Resource Node!") across three different fresh Pure,
	 * unoccupied resource nodes. Root cause, CONFIRMED live via a
	 * two-round diagnostic pass (ruled out bad GetPlacementLocation()/
	 * GetPlacementRotation() values and a wrong hologram class first):
	 * the synthetic FHitResult's `Distance` field was left at its
	 * default (0.f) while every other field (Location/Normal/Component/
	 * HitObjectHandle) was deliberately populated to look like a real
	 * trace result - AFGResourceExtractorHologram's internal placement
	 * validation evidently sanity-checks Distance. Now sets it to the
	 * real player-to-placement-point distance. Also added an explicit
	 * `Hologram->TrySnapToActor(Hit)` call (previously relied solely on
	 * UpdateHologramPlacement(), unlike every other click-driven
	 * Construct* function in this file) in the same redeploy - its own
	 * contribution to the fix is unconfirmed, kept for consistency with
	 * the rest of this file and because TrySnapToActor() is what
	 * populates mSnappedExtractableResource, needed for the extractor to
	 * actually function correctly, not just pass this one disqualifier.
	 *
	 * RecipeClassPath (2026-08-27, per explicit user request to support
	 * Resource Well Pressurizers/Extractors): was hardcoded to
	 * Recipe_MinerMk1 - now caller-chosen, any solid/liquid/gas extractor
	 * recipe (Recipe_MinerMk1..Mk3, Recipe_WaterPump, Recipe_OilPump,
	 * Recipe_FrackingSmasher, Recipe_FrackingExtractor). The RF_SOLID-only
	 * gate this function used to enforce manually was removed - it only
	 * ever existed because this function was written and tested against
	 * Miners first; the real engine-side gating (AFGBuildableResourceExtractorBase::
	 * mAllowedResourceForms, mRestrictToNodeType, and their disqualifiers)
	 * already does this correctly for every extractor type, confirmed
	 * from source (docs/resource-well-research.md) - trust it the same
	 * way this function already trusts CanConstruct() for everything
	 * else, rather than re-deriving a redundant, narrower check.
	 *
	 * NodeId now resolves against AFGResourceNodeBase (was AFGResourceNode) -
	 * a strictly wider search, not a behavior change for existing
	 * callers: AFGResourceNode (normal nodes and Fracking Satellites,
	 * since AFGResourceNodeFrackingSatellite : AFGResourceNode) was
	 * already covered; AFGResourceNodeFrackingCore (the Resource Well
	 * Pressurizer's real target - NOT an AFGResourceNode, confirmed from
	 * source) is the new case this makes reachable at all. See
	 * docs/resource-well-research.md for the full class hierarchy and why
	 * a Pressurizer must be built on a core node specifically, never a
	 * satellite.
	 *
	 * Sequencing (NOT enforced by this function - a real, engine-side
	 * construction-time gate, not a bypassable disqualifier): a Fracking
	 * Satellite's construct disqualifier (UFGCDNeedsFrackingSatelliteNode)
	 * requires the satellite to have been ACTIVATED by its core's own
	 * Pressurizer already producing (AFGResourceNodeFrackingSatellite::
	 * GetState() != FSS_Untouched) before Recipe_FrackingExtractor can be
	 * built there at all - confirmed from source/localized disqualifier
	 * text ("Must be placed on an activated Fracking Satellite Node!").
	 * The real required order is: build the Pressurizer on the core,
	 * power it, wait for GetState() to leave FSS_Untouched (poll
	 * "world.resourceNodes" - see its satelliteState field), only then
	 * build extractors on the satellites - this function will correctly
	 * fail with CANNOT_CONSTRUCT if called on a not-yet-activated
	 * satellite, it does not silently bypass the check.
	 */
	static void ConstructExtractorOnNode(UObject* WorldContextObject, const FString& NodeId, const FString& RecipeClassPath, TFunction<void(const FDocModOperationResult&)> OnComplete);

	/**
	 * Places a Portable Miner (AFGPortableMiner) on a resource node - added
	 * 2026-08-27 per explicit user request. Architecturally unrelated to
	 * every other Construct* function in this file: the Portable Miner is
	 * NOT an AFGBuildable and is never driven by AFGBuildGunStateBuild/a
	 * hologram at all (confirmed from source - AFGPortableMiner derives
	 * directly from AActor). It's deployed as EQUIPMENT (like the Golf
	 * Cart), via AFGPortableMinerDispenser : AFGEquipment, whose real
	 * placement logic is a `protected UFUNCTION(Server, Reliable)
	 * Server_SpawnPortableMiner(location, resourceNode)`.
	 *
	 * Real flow, reverse-engineered from FGPortableMiner.h/
	 * FGPortableMinerDispenser.h/FGInventoryComponentEquipment.h/
	 * FGCharacterPlayer.h (no .cpp bodies available - this SDK's
	 * FactoryGame .cpp files are stub source):
	 * 1. The player must already have a real Portable Miner ITEM in
	 *    inventory (ItemClassPath, default the real
	 *    BP_ItemDescriptorPortableMiner path) - it's consumed on
	 *    placement like a real player crafting+placing one, not
	 *    synthesized. Fails with PORTABLE_MINER_NOT_IN_INVENTORY if absent.
	 * 2. Moves the item from the player's general inventory into the ARMS
	 *    equipment slot (a genuinely SEPARATE small
	 *    UFGInventoryComponentEquipment, not a view over the backpack -
	 *    live-confirmed 2026-08-27: an item just sitting in the general
	 *    inventory does not automatically appear here), finds the index
	 *    it landed at, then calls the REAL, public, BlueprintCallable
	 *    SetActiveEquipmentIndex(index) - the same sanctioned path a
	 *    player's own hotbar key-press uses (internally spawns+equips
	 *    the dispenser) - deliberately NOT calling
	 *    AFGCharacterPlayer::SpawnEquipment directly, since that's a
	 *    private, non-reflected C++ method with no public/reflectable
	 *    entry point at all. If the move itself fails, the item is
	 *    restored to the general inventory rather than left stranded.
	 * 3. Polls (real ticks, same pattern as every other deferred
	 *    Construct* function) until AFGCharacterPlayer::GetEquipmentInSlot
	 *    (ES_ARMS) resolves to a real AFGPortableMinerDispenser instance.
	 * 4. Calls that dispenser's protected Server_SpawnPortableMiner via
	 *    Unreal reflection (FindFunction+ProcessEvent - UFUNCTION
	 *    reflection isn't gated by C++ access specifiers) with the
	 *    resolved node's OWN real location, not a camera trace - this
	 *    deliberately bypasses TraceForPortableMinerPlacementLocation's
	 *    camera-dependent aim entirely, matching this project's
	 *    established player-independence pattern for every other
	 *    Construct* function.
	 * 5. Polls again for a new AFGPortableMiner actor whose
	 *    mExtractResourceNode matches the target node, then unequips
	 *    (UnequipEquipment) to return to a clean state.
	 *
	 * NodeId uses the same AFGResourceNodeBase-based lookup as
	 * ConstructExtractorOnNode (any real, unoccupied node). Fails with
	 * NODE_OCCUPIED if IsOccupied() is already true, matching real game
	 * behavior (a Portable Miner still occupies the node like any other
	 * extractor).
	 */
	static void ConstructPortableMinerOnNode(UObject* WorldContextObject, const FString& NodeId, const FString& ItemClassPath, TFunction<void(const FDocModOperationResult&)> OnComplete);

	/**
	 * Serializes every AFGPortableMiner actor in the world to
	 * {"protocolVersion":1,"portableMiners":[{"id","position","nodeId",
	 * "isProducing","extractionProgress","outputInventory":[{"itemClass",
	 * "numItems"},...]},...]} - added 2026-08-27 alongside
	 * ConstructPortableMinerOnNode, since a Portable Miner "has to be
	 * emptied directly by the player" (no belt output) per the user's own
	 * framing - this is how a caller finds out one needs emptying before
	 * calling RetrievePortableMinerInventory. Id is the same session-local
	 * GetPathName()-based scheme as every other actor id in this protocol.
	 */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FString LogPortableMinersAsJson(UObject* WorldContextObject);

	/**
	 * Moves items out of a Portable Miner's output inventory
	 * (GetOutputInventory(), a plain UFGInventoryComponent) and into the
	 * local player's own inventory - the "have to be emptied directly by
	 * the player" step, done via RPC instead of walking up and pressing E.
	 * Added 2026-08-27 per explicit user request ("build support for
	 * managing machine inventory").
	 *
	 * Moves EVERY item currently in the output inventory (no partial/
	 * selective retrieval yet - a real gap, not an oversight, flagged for
	 * a future pass if selective retrieval turns out to matter). Uses
	 * UFGInventoryComponent::Remove()+AddStack(allowPartialAdd=true) -
	 * real inventory mutation, not a synthesized item grant. Only the
	 * amount AddStack actually reports as added is ever Remove()'d from
	 * the source - if the player's inventory fills up partway through,
	 * whatever didn't fit simply stays in the Portable Miner rather than
	 * being lost, so this is always safe to call even against a nearly-
	 * full player inventory.
	 *
	 * Fails with TARGET_NOT_FOUND if PortableMinerId doesn't resolve,
	 * NOTHING_TO_RETRIEVE if the output inventory is already empty, or
	 * INVENTORY_FULL if it has items but none of them fit in the
	 * player's inventory.
	 */
	static void RetrievePortableMinerInventory(UObject* WorldContextObject, const FString& PortableMinerId, TFunction<void(const FDocModOperationResult&)> OnComplete);

	/**
	 * Deletes every AFGBuildablePipelineFlowIndicator in the world that
	 * isn't the real, currently-attached indicator of any live
	 * AFGBuildablePipeline - added 2026-08-27 per explicit user request,
	 * after position-proximity heuristics turned out to be unreliable for
	 * telling an orphaned indicator apart from a legitimate one in a
	 * dense pipe cluster (see docs/placement-lessons.md).
	 *
	 * Exact, not a guess: for every AFGBuildablePipeline in the world,
	 * calls its own public GetFlowIndicator() (a real accessor for the
	 * mFlowIndicator UPROPERTY, no reflection needed) to build the set of
	 * genuinely-attached indicators; any AFGBuildablePipelineFlowIndicator
	 * actor not in that set is a confirmed orphan - real debris left
	 * behind by a pipe deleted before this project's
	 * DismantleBuildable child-actor-cleanup fix existed (see that
	 * function's doc comment), not a false positive from proximity
	 * matching. Deletes orphans via the same real
	 * IFGDismantleInterface::Execute_Dismantle() path as
	 * DismantleBuildable, not AActor::Destroy(). Returns
	 * {"protocolVersion":1,"totalIndicators","attachedCount","orphanCount",
	 * "deletedIds":[...]}.
	 */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FString CleanupOrphanedFlowIndicatorsAsJson(UObject* WorldContextObject);

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
	 *
	 * RouteMode (2026-08-25, added after live-diagnosing a real, well-
	 * evidenced gap): the 2-click TrySnapToActor flow above reliably
	 * fails - "Conveyor Belt is too long!"/"Invalid placement!" - for ANY
	 * meaningful direction mismatch between the source's output and the
	 * dest's input, confirmed across many distances (600-4100+ units)
	 * and mismatch angles (20-90+ degrees), even when straight-line
	 * distance was well under the real queried maxSplineLength. One of
	 * "Straight"/"Curve"/"Auto" (case-insensitive; empty/omitted leaves
	 * the hologram's own default mode untouched, matching prior
	 * behavior) - maps to the real, disk-confirmed
	 * `/Game/FactoryGame/Buildable/Factory/-Shared/BuildGunModes/BuildMode_*`
	 * assets via `AFGHologram::SetBuildModeOverride()` (public,
	 * FGHologram.h) - `AFGConveyorBeltHologram::mBuildModeStraight`/
	 * `mBuildModeCurve` are the two it exposes past the implicit default
	 * ("Auto"). `AutoRouteSpline()`'s own doc comment ("routes the spline
	 * to the new location, inserting bends and straights") is the
	 * evidence "Curve" should be the fix for the bend-failure gap above -
	 * NOT YET LIVE-VERIFIED to actually resolve it, only a well-evidenced
	 * hypothesis, since the private engine logic behind
	 * SetBuildModeOverride()/AutoRouteSpline() is stub-source in this SDK
	 * like everything else here.
	 */
	static void ConstructConveyorBelt(UObject* WorldContextObject, const FString& SourceBuildableId, const FString& DestBuildableId, const FString& RecipeClassPath, const FString& RouteMode, bool bDryRun, TFunction<void(const FDocModOperationResult&)> OnComplete);

	/**
	 * Telemetry, not a mutation - same LogXAsJson convention as
	 * LogConveyorBeltTiersAsJson. Added 2026-08-25, vertical conveyor
	 * groundwork, per explicit user request ("add support for vertical
	 * conveyors, these can be used strategically to transition from
	 * miners locked to the terrain and raised foundations").
	 *
	 * Recipe_ConveyorLiftMk1..Mk6 (all six confirmed present on disk,
	 * same naming as belts) resolve to `AFGBuildableConveyorLift` -
	 * `AFGBuildableConveyorBase`'s OTHER direct subclass alongside
	 * regular belts, confirmed from source, sharing
	 * `GetSpeed()`/`GetConnection0()`/`GetConnection1()`. Reports each
	 * tier's real queried `speed` only - deliberately does NOT report
	 * min/max height limits: `AFGConveyorLiftHologram`'s
	 * `mStepHeight`/`mMinimumHeight`/`mMaximumHeight`/
	 * `mMinimumHeightWithVerticalConnection` are plain private `float`
	 * members with NO `UPROPERTY` macro (confirmed from header) - unlike
	 * every other reflection-based CDO read in this file,
	 * `FindFProperty<FFloatProperty>` cannot find a non-`UPROPERTY`
	 * field at all, since UHT never generates reflection data for it.
	 * This is a genuine, real gap, not an omission - real height limits
	 * remain unknown until discovered another way.
	 */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FString LogConveyorLiftTiersAsJson(UObject* WorldContextObject);

	/**
	 * PLAN.md Phase 13/14, vertical conveyor groundwork (2026-08-25), per
	 * explicit user request. Deliberate near-mirror of
	 * ConstructConveyorBelt's two-click `TrySnapToActor` flow -
	 * `AFGConveyorLiftHologram` is NOT a spline hologram (confirmed from
	 * source: `AFGConveyorLiftHologram : AFGBuildableHologram` directly,
	 * NOT `AFGSplineHologram` like belts/pipes - a vertical lift is a
	 * straight column, no bending), but it DOES override
	 * `TrySnapToActor`/`DoMultiStepPlacement` itself, so the same
	 * click-driven pattern applies: `UpdateHologramPlacement()` before
	 * each `TrySnapToActor()`, the connector's real
	 * `GetConnectorNormal()` (not a placeholder) in the synthetic hit.
	 * Reuses `FindFreeFactoryConnection`/`UFGFactoryConnectionComponent` -
	 * `AFGBuildableConveyorLift` shares the exact same connection
	 * component type as regular belts (both derive from
	 * `AFGBuildableConveyorBase`).
	 *
	 * NOT YET LIVE-TESTED. No post-end-click connectivity diagnostic is
	 * available here (unlike belts'/pipes' `GetAnyConnectedBuildables()`/
	 * `IsConnectionSnapped()`, inherited from `AFGSplineHologram` which
	 * this hologram does NOT derive from) - only the disqualifier list
	 * is logged. No `RouteMode` param - lifts are a fixed vertical
	 * column, no bend/curve concept applies. Same `bDryRun` switch and
	 * real-construction posture as every other `Construct*` function.
	 * Not a `UFUNCTION` - same reason as the other async entry points.
	 */
	static void ConstructConveyorLift(UObject* WorldContextObject, const FString& SourceBuildableId, const FString& DestBuildableId, const FString& RecipeClassPath, bool bDryRun, TFunction<void(const FDocModOperationResult&)> OnComplete);

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
	 * each buildable class CDO's UFGFactoryConnectionComponents (via
	 * AFGBuildable::GetDefaultComponents<>() - a plain CDO
	 * GetComponents<>() scan finds nothing here, since these connectors
	 * are added via the Blueprint's Simple Construction Script, not a
	 * native CreateDefaultSubobject; fixed 2026-08-27, see this
	 * function's .cpp comment - the original version of this function
	 * reported 0/0 for every entry, undetected until then) rather than
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
	 * world.recipeCatalog (2026-08-27, per explicit user request to
	 * support pre-planning: "what recipes/alternates build each item,
	 * what machines are needed, resource/power requirements, rates").
	 * Reports EVERY recipe in the game via AFGRecipeManager::GetAllRecipes()
	 * - including ones not yet unlocked in the current save, unlike the
	 * progression-gated GetAllAvailableRecipes(). Each entry: recipeClass,
	 * displayName, isBuildingRecipe, manufacturingDuration, ingredients/
	 * products (itemClass/itemName/amount - amount is FItemAmount's raw
	 * unit, thousandths of a m^3 for liquids/gases, NOT pre-converted -
	 * check the item's "form" via world.itemCatalog), producedIn (real
	 * buildable/build-gun class paths), and the recipe's variable-power-
	 * consumption constant/factor.
	 *
	 * IMPORTANT: AFGRecipeManager::Get() is stub-source in Editor/PIE and
	 * only resolves to real data in the packaged/Alpakit-deployed game -
	 * test against the real Steam session, not Play-in-Editor.
	 *
	 * Deliberately does NOT compute effective production rates (items/min
	 * accounting for clock speed or Somersloop boost) - see
	 * world.buildableCatalog for the per-building min/max potential and
	 * production-boost fields needed to do that arithmetic on the caller
	 * side, per this project's toolkit-not-solver preference.
	 */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FString LogRecipeCatalogAsJson(UObject* WorldContextObject);

	/**
	 * world.itemCatalog (2026-08-27) - companion to LogRecipeCatalogAsJson,
	 * see that function's doc comment for the shared AFGRecipeManager/
	 * stub-source caveats. Reports every item descriptor via
	 * AFGRecipeManager::GetAllItemDescriptors(): itemClass, name, form
	 * ("Solid"/"Liquid"/"Gas"/"Invalid"), isBuildingDescriptor (true for
	 * building "items" like Recipe_ConstructorMk1's product - cross-check
	 * against world.recipeCatalog's isBuildingRecipe), stackSize,
	 * energyValue (for fuel), radioactiveDecay, and gasType (only set when
	 * form is "Gas").
	 */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FString LogItemCatalogAsJson(UObject* WorldContextObject);

	/**
	 * world.buildableCatalog (2026-08-27) - companion to
	 * LogRecipeCatalogAsJson/LogItemCatalogAsJson, see LogRecipeCatalogAsJson's
	 * doc comment for the shared AFGRecipeManager/stub-source caveats.
	 * Derived from the recipe catalog (filters to isBuildingRecipe==true,
	 * resolves each to its real AFGBuildable class via
	 * UFGBuildingDescriptor::GetBuildableClass()) rather than a separate
	 * enumeration source, so a building's construction cost is always
	 * exactly its recipe's ingredients.
	 *
	 * Each entry: recipeClass, buildableClass, category ("Generator"/
	 * "Extractor"/"Manufacturer"/"Other" - determined by C++ class
	 * hierarchy, not a FactoryGame-declared enum), constructionCost (same
	 * shape as recipe ingredients), clearance (2026-08-27, per explicit
	 * user request to pre-plan layouts/estimate foundation counts/space
	 * for belt+pipe routing gaps - an array of the buildable's real
	 * mClearanceData boxes, the SAME data FactoryGame's own construction-
	 * overlap checks use, each with min/max/size in the buildable's local
	 * space and "type" ("Default"/"Soft"/"BlockEverything") - most
	 * buildables have exactly one entry, but some declare more than one
	 * (e.g. a base volume plus a separate one for an attached arm), never
	 * spawns anything since mClearanceData is a plain class-default
	 * property, retrieved via the IFGClearanceInterface
	 * BlueprintNativeEvent), factoryInputCount/factoryOutputCount
	 * (solid connections), pipeInputCount/pipeOutputCount (fluid
	 * connections), powerConnectionCount, overridesShardSlotCount +
	 * potentialShardSlots (power-shard overclock slot count, via
	 * reflection - mMaxPotential/GetMaxPotential() is explicitly
	 * documented as the un-shard baseline, this is what tells a caller
	 * how much headroom shards can add on top of it - BUT
	 * potentialShardSlots is only meaningful when
	 * overridesShardSlotCount is true; live-confirmed most buildings
	 * report the override off, meaning the real slot count falls back to
	 * a global default this per-building read cannot see - a real,
	 * documented gap, not a wrong number) - all read off each buildable
	 * class's CDO via AFGBuildable::GetDefaultComponents<>() (NOT plain
	 * GetComponents<>(), which misses every Blueprint-SCS-added connector
	 * - see LogConveyorAttachmentCatalogAsJson's doc comment for the same
	 * fix and why it was needed), class-level defaults only, never
	 * spawned in the world.
	 *
	 * For anything deriving from AFGBuildableFactory (Manufacturer,
	 * Extractor, Generator all do): runsOnPower, idlePowerConsumption,
	 * producingPowerConsumptionBase, defaultProducingPowerConsumption,
	 * minPotential/maxPotential (clock speed range, i.e. what power shards
	 * can reach), canChangePotential. For AFGBuildableGenerator
	 * specifically, additionally: powerProductionCapacity/
	 * defaultPowerProductionCapacity (real MW output). Non-factory
	 * buildables (foundations, walls, belts, poles...) still appear with
	 * category "Other" and omit these power/potential fields entirely
	 * (rather than reporting misleading zeros).
	 */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FString LogBuildableCatalogAsJson(UObject* WorldContextObject);

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
	/**
	 * FindFreeFluidPipeConnection fallback (2026-08-27, per explicit user
	 * request to connect pipes to a Storage Tank and merge multiple lines
	 * through Pipeline Junctions): a genuine producer/consumer distinction
	 * (PCT_PRODUCER/PCT_CONSUMER) only exists on machines that actually
	 * have one (Refineries, Pumps, Blenders). Confirmed live that Storage
	 * Tanks (Recipe_PipeStorageTank) and Pipeline Junctions (Cross/T) have
	 * ONLY PCT_ANY connectors - the exact-type match used to find nothing
	 * on either, making it impossible to build a pipe to/from them at
	 * all. Now falls back to any free PCT_ANY connector once the exact
	 * match fails, so both work.
	 */
	static void ConstructPipe(UObject* WorldContextObject, const FString& SourceBuildableId, const FString& DestBuildableId, const FString& RecipeClassPath, bool bDryRun, TFunction<void(const FDocModOperationResult&)> OnComplete);

	/**
	 * Constructs a real hypertube tube segment (Recipe_PipeHyper) between
	 * two existing buildables' free hypertube connectors - a Hypertube
	 * entrance/exit (AFGPipeHyperStart), a junction/T-junction, or another
	 * tube segment. Despite the "Recipe_HyperTube*" naming of the
	 * junction/support recipes in the catalog, the actual connecting tube
	 * is `Recipe_PipeHyper`, and its hologram is a Blueprint child of the
	 * SAME AFGPipelineHologram class ConstructPipe drives - confirmed from
	 * source/asset research (docs/hypertube-research.md). Deliberately a
	 * near-mirror of ConstructPipe (same two-click TrySnapToActor +
	 * DoMultiStepPlacement flow, same deterministic-look/disqualifier-
	 * ignore player-independence pattern established this session, applied
	 * from the start here rather than retrofitted), differing only in:
	 * (1) no recipeClass param - Recipe_PipeHyper is hardcoded, since no
	 * hypertube tier variants exist; (2) connector lookup accepts
	 * UFGPipeConnectionComponentHyper at PCT_ANY (not PCT_PRODUCER/
	 * PCT_CONSUMER like fluid pipes - confirmed from source that hypertube
	 * connectors never override the PCT_ANY default); (3) no producer/
	 * consumer distinction - hypertubes are bidirectional, so
	 * source/dest just mean "which buildable's free connector each end
	 * uses", not a flow direction. Not a UFUNCTION - same reason as the
	 * other async entry points.
	 */
	static void ConstructHypertube(UObject* WorldContextObject, const FString& SourceBuildableId, const FString& DestBuildableId, bool bDryRun, TFunction<void(const FDocModOperationResult&)> OnComplete);
};
