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
	 * Dismantles an existing buildable via the real IFGDismantleInterface
	 * (Execute_CanDismantle/Execute_Dismantle), not AActor::Destroy() -
	 * per CLAUDE.md's "No Direct Memory Manipulation", this is the same
	 * cleanup path (connections, attached wires, inventories,
	 * AFGBuildableSubsystem removal) a player's in-game dismantle uses.
	 * Synchronous, no hologram involved. Fails TARGET_NOT_FOUND if the id
	 * does not resolve, or CANNOT_DISMANTLE if the buildable refuses
	 * (already dismantled, or an un-dismantled parent like integrated
	 * railroad track).
	 *
	 * Also handles "lightweight:<ClassPath>|<Index>" ids (see
	 * docs/lightweight-buildable-research.md): foundations and other
	 * mass-placed pieces are not AFGBuildable actors but
	 * FRuntimeBuildableInstanceData in AFGLightweightBuildableSubsystem -
	 * materializes a temporary AFGBuildable* via
	 * FindOrSpawnBuildableForRuntimeData() and reuses Execute_Dismantle(),
	 * which has a dedicated branch for removing the lightweight instance.
	 *
	 * Also dismantles GetChildDismantleActors() children first - some
	 * buildables reference a separate actor that Dismantle_Implementation
	 * itself does not clean up (e.g. AFGBuildablePipeline's
	 * mFlowIndicator). The real player dismantle path
	 * (UFGBuildGunStateDismantle) consults this; Execute_Dismantle alone
	 * does not - without it, a dismantled pipe left its fluid indicator
	 * floating in place, confirmed live.
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
	 *
	 * Scale is a uniform scale factor applied to the spawn transform
	 * (default/invalid values treated as 1.0, clamped to [0.05, 20.0]) -
	 * untested against FactoryGame's own creature Blueprints, since
	 * collision/AI ranges are often hardcoded independent of
	 * RootComponent scale.
	 */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FDocModOperationResult SpawnCreatureNearPlayer(UObject* WorldContextObject, const FString& CreatureClassPath, float DistanceFromPlayer, float Scale);

	/**
	 * Despawns a creature previously spawned via SpawnCreatureNearPlayer
	 * (or any real AFGCreature), by its GetPathName() id. Added
	 * 2026-08-28 for cleanup before a save - narrowly scoped to
	 * AFGCreature (TActorIterator<AFGCreature> lookup), not a generic
	 * "destroy any actor" capability - see CLAUDE.md's Safety and
	 * Stability Boundary.
	 */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FDocModOperationResult DespawnCreature(UObject* WorldContextObject, const FString& CreatureId);

	/**
	 * Building placement at an explicit X/Y (not near-player). Not a
	 * UFUNCTION - TFunction callbacks are not UHT-compatible - plain C++
	 * entry point for UDocModHttpServerSubsystem only. Same validated
	 * flow as ConstructBuildingNearPlayer (real-tick polling for
	 * CanConstruct, re-asserted every tick for deterministic placement).
	 * Genuinely asynchronous: OnComplete fires exactly once, sync or
	 * after polling resolves.
	 *
	 * RotationScrollDelta: applies AFGHologram::Scroll(sign) that many
	 * times. Non-linear for magnitude greater than 1 and terrain
	 * dependent - see docs/buildgun-driven-placement-research.md.
	 * Prefer bHasTargetYaw/TargetYawDegrees for a reliable result.
	 *
	 * GridSnapSize: rounds X/Y to the nearest multiple before placing.
	 * 0 disables. Independent of any per-building internal snap.
	 *
	 * ReferenceZ: anchors the ground trace to this Z instead of the
	 * player current Z - needed because the trace search range is only
	 * +/-1000 units, so an unrelated player position can miss real
	 * terrain entirely. Pass an existing buildable Z (world.buildables)
	 * for deterministic multi-step layouts. Sentinel -1000000 means
	 * not provided, falls back to player Z.
	 *
	 * bIgnoreGroundTrace: skips the ground trace entirely and places at
	 * the literal (X, Y, ReferenceZ) - requires ReferenceZ to be
	 * explicitly provided (fails MISSING_REFERENCE_Z otherwise). Added
	 * because the trace is unreliable in two confirmed ways: (1) at an
	 * exact foundation tile edge, the same request can non-
	 * deterministically find either the real top surface or fall through
	 * to unrelated lower terrain; (2) above open interior space (e.g. a
	 * roof over a room, nothing solid within +/-1000 units), it always
	 * falls through to the floor below, never the intended height. Use
	 * world.groundHeight once to find a real surface Z nearby, compute
	 * the true target Z from that (see docs/placement-lessons.md), then
	 * pass it here for a placement no game-thread trace can perturb.
	 *
	 * bIgnoreAimLocation/bIgnorePlayerEncroachment/bIgnoreClearance/
	 * bIgnoreInvalidFloor: named, scoped bypasses of specific UX-only
	 * disqualifiers, for large autonomous layouts that accept the
	 * collision risk. Every other disqualifier still applies.
	 *
	 * bHasTargetYaw/TargetYawDegrees: sets an exact final world yaw
	 * directly, bypassing RotationScrollDelta - the reliable way to get
	 * a specific orientation.
	 *
	 * FaceBuildableId: resolves an existing buildable position and
	 * computes TargetYawDegrees to face it automatically. Takes
	 * priority over an explicit yaw. Fails with FACE_TARGET_NOT_FOUND.
	 * Orients the whole building only, not a specific connector on a
	 * multi-connector building.
	 */
	static void ConstructBuildingAtPosition(UObject* WorldContextObject, const FString& RecipeClassPath, float X, float Y, int32 RotationScrollDelta, float GridSnapSize, float ReferenceZ, bool bIgnoreGroundTrace, bool bIgnoreAimLocation, bool bIgnorePlayerEncroachment, bool bIgnoreClearance, bool bIgnoreInvalidFloor, bool bHasTargetYaw, float TargetYawDegrees, const FString& FaceBuildableId, TFunction<void(const FDocModOperationResult&)> OnComplete);

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
	 * Places a real extractor on a resource node by session-local id
	 * (world.resourceNodes), instead of requiring the player to be
	 * looking at it. Not a UFUNCTION - plain C++ entry point for
	 * UDocModHttpServerSubsystem, same async shape as
	 * ConstructBuildingAtPosition.
	 *
	 * The synthetic FHitResult must set a real Distance field (not the
	 * 0.f default) - AFGResourceExtractorHologram sanity-checks it, a
	 * default value fails with "Must be placed on a Resource Node!"
	 * even on a valid node. Also calls Hologram->TrySnapToActor(Hit),
	 * which populates mSnappedExtractableResource.
	 *
	 * RecipeClassPath is caller-chosen (any solid/liquid/gas extractor
	 * recipe) - engine-side gating on AFGBuildableResourceExtractorBase
	 * already restricts which recipe fits which node type correctly, no
	 * need to re-derive that here. NodeId resolves against the wider
	 * AFGResourceNodeBase, which also reaches Fracking Core nodes (a
	 * Resource Well Pressurizer target, not an AFGResourceNode - see
	 * docs/resource-well-research.md).
	 *
	 * Fracking Satellite sequencing is a real engine-side gate, not
	 * bypassable here: a satellite extractor needs its core Pressurizer
	 * already producing (poll world.resourceNodes' satelliteState)
	 * before it can be built - this function correctly fails rather
	 * than silently skipping that check.
	 */
	static void ConstructExtractorOnNode(UObject* WorldContextObject, const FString& NodeId, const FString& RecipeClassPath, TFunction<void(const FDocModOperationResult&)> OnComplete);

	/**
	 * Places a Portable Miner (AFGPortableMiner) on a resource node.
	 * Architecturally unlike every other Construct* function here: a
	 * Portable Miner is not an AFGBuildable, never driven by a hologram.
	 * It is deployed as EQUIPMENT via AFGPortableMinerDispenser, whose
	 * real placement logic is a protected
	 * Server_SpawnPortableMiner(location, resourceNode).
	 *
	 * Flow: (1) requires a real Portable Miner item already in inventory
	 * (ItemClassPath, PORTABLE_MINER_NOT_IN_INVENTORY if absent) - real
	 * consumption, not synthesized; (2) moves it into the ARMS
	 * equipment slot (a genuinely separate inventory component from the
	 * general backpack) and calls the real SetActiveEquipmentIndex to
	 * equip the dispenser; (3) polls for the dispenser to actually
	 * exist in that slot; (4) calls Server_SpawnPortableMiner with the
	 * node real location (not a camera trace, matching this file player-
	 * independence pattern elsewhere); (5) polls for the new actor and
	 * unequips.
	 *
	 * STILL UNRESOLVED as of 2026-08-28: step 4 executes with no error
	 * and correct parameters (confirmed via log), but no real actor
	 * appears - true even when called as a direct C++ member call via a
	 * protected-access-bypass accessor (see the .cpp), not just via
	 * reflection. Root cause not found; something inside the real,
	 * stub-source compiled implementation is rejecting it.
	 *
	 * NodeId uses the same AFGResourceNodeBase lookup as
	 * ConstructExtractorOnNode. Fails with NODE_OCCUPIED if already
	 * occupied, matching real game behavior.
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
	 * Moves a Portable Miner item from the player's ARMS equipment slot
	 * back into their general inventory, added 2026-08-28. Needed because
	 * the ARMS slot is a genuinely separate inventory component (see
	 * ConstructPortableMinerOnNode's doc comment) that a stationary
	 * Miner's real construction-cost affordability check does not see -
	 * confirmed live: world.placeExtractor failed with "Missing
	 * materials!" for a Portable Miner ingredient even though the player
	 * had one equipped as their active item. No-ops successfully (returns
	 * Success with no change) if the ARMS slot doesn't currently hold a
	 * Portable Miner - not an error, since the general inventory may
	 * already have one. Uses the real UFGInventoryComponent::
	 * RemoveFromIndex(idx, num, targetInventory) transfer overload, not a
	 * separate Remove()+AddStack() pair.
	 */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FDocModOperationResult MovePortableMinerToInventory(UObject* WorldContextObject);

	/**
	 * Simulated handheld-item crafting, added 2026-08-28 per explicit
	 * user request - a deliberate alternative to driving the real
	 * Workshop/WorkBench crafting UI (never implemented - see
	 * docs/placement-lessons.md and the manual-crafting research this
	 * session), for the specific case of a player who has the real
	 * ingredients for a handheld item but can't reach a bench, or (the
	 * motivating case) needs a Portable Miner and world.placePortableMiner's
	 * underlying Server_SpawnPortableMiner RPC is still unresolved.
	 *
	 * Deliberately scoped to HANDHELD ITEMS ONLY, not a generic "spawn any
	 * item" capability (CLAUDE.md's Safety and Stability Boundary) - the
	 * recipe's real UFGRecipe::GetProducts() is checked and EVERY product
	 * must resolve to a UFGEquipmentDescriptor subclass, rejecting recipes
	 * for raw parts, buildings, or bulk factory components entirely.
	 * Fails with NOT_HANDHELD_ITEM if the recipe produces anything else.
	 *
	 * Real inventory mutation, not a synthesized grant with no cost: reads
	 * the recipe's actual UFGRecipe::GetIngredients() and verifies the
	 * player's real inventory (UFGInventoryComponent::HasItems()) can
	 * afford ALL of them before changing anything - fails with
	 * INSUFFICIENT_INGREDIENTS (naming what's short) rather than partially
	 * consuming ingredients for a craft that can't complete. Only on a
	 * full pass does it Remove() each ingredient and AddStack() each
	 * product, in the recipe's own stated amounts (single craft, no
	 * multiplier).
	 */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FDocModOperationResult SimulatedCraft(UObject* WorldContextObject, const FString& RecipeClassPath);

	/**
	 * Reports every item currently held in the Dimensional Depot (real
	 * class name AFGCentralStorageSubsystem - "Dimensional Depot" is only
	 * the in-game display name), added 2026-08-28. Root-caused live: the
	 * player had 2500 Concrete "in dimensional storage" yet
	 * world.placeBuilding kept failing "Missing materials!" on a 2-
	 * Concrete wall. Per the user, real interactive player building pulls
	 * from both the Depot and carried inventory automatically (with a
	 * player-configurable preference for draw order) - this mod's
	 * Construct* functions do NOT yet replicate that, they only check
	 * carried UFGInventoryComponent. See WithdrawFromCentralStorage for
	 * the current manual workaround. Empty {"items":[]} if
	 * IsCentralStorageBuilt() is false (no Depot Uploader exists yet)
	 * rather than an error - a legitimately empty state.
	 */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FString LogCentralStorageAsJson(UObject* WorldContextObject);

	/**
	 * Withdraws items from the Dimensional Depot into the player's
	 * general inventory, added 2026-08-28 alongside
	 * LogCentralStorageAsJson - see that function's doc comment for why
	 * this is needed (Depot storage and carried inventory are genuinely
	 * separate, confirmed live). Uses the real
	 * AFGCentralStorageSubsystem::TryRemoveItemsFromCentralStorage(),
	 * which itself clamps to whatever is actually available (a request
	 * for more than the Depot holds is not an error - it withdraws
	 * whatever it can). Fails with NO_CENTRAL_STORAGE if no Depot exists,
	 * or NOTHING_WITHDRAWN if the Depot holds none of the requested item.
	 */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FDocModOperationResult WithdrawFromCentralStorage(UObject* WorldContextObject, const FString& ItemClassPath, int32 Amount);

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
	 * Connects power between two buildings via
	 * AFGWireHologram::SetConnection(0/1, ...) directly with a single
	 * GetHitResult() assignment for ConnectionA - the only mechanism
	 * confirmed to work for machine-to-machine wire connections. A
	 * click-based TrySnapToActor rewrite never populated
	 * GetConnection(0)/(1) for wires at all; do not repeat without new
	 * evidence.
	 *
	 * bIgnoreAimLocation/bIgnoreWireSnap are named, per-disqualifier
	 * bypasses (manually walks GetConstructDisqualifiers rather than
	 * trusting the opaque CanConstruct bool) added after diagnosing
	 * real disqualifier flakiness - the same connection pair returning
	 * different disqualifiers across identical repeated calls.
	 * UFGCDWireTooLong is deliberately not ignorable, since it reflects
	 * a real deterministic length check. Not yet live-verified to
	 * resolve the flakiness.
	 *
	 * Same bDryRun/async pattern as ConstructExtractorOnNode. A
	 * CANNOT_CONSTRUCT/NO_POWER_CONNECTION result may correctly reflect
	 * a real pole-vs-daisy-chain progression gate (see
	 * docs/conveyor-power-connection-research.md), not a bug.
	 */
	static void ConstructPowerConnection(UObject* WorldContextObject, const FString& BuildableIdA, const FString& BuildableIdB, bool bDryRun, bool bIgnoreAimLocation, bool bIgnoreWireSnap, TFunction<void(const FDocModOperationResult&)> OnComplete);

	/**
	 * Diagnostic only, not a real placement attempt: spawns a real belt
	 * hologram and feeds a synthetic hit at a free output connection
	 * through three entry points (UpdateHologramPlacement, TrySnapToActor,
	 * a single click), logging the resulting connection state after
	 * each. Never calls CanConstruct/Construct, never touches the save,
	 * reports synchronously.
	 */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FDocModOperationResult DebugCheckConveyorSnap(UObject* WorldContextObject, const FString& SourceBuildableId);

	/**
	 * Connects a conveyor belt between two connectors via the real
	 * TrySnapToActor()+DoMultiStepPlacement() click sequence, once at
	 * the source and once at the dest, polling for UFGCDInitializing to
	 * clear before checking CanConstruct(). Same bDryRun/async pattern
	 * as ConstructPowerConnection. Not a UFUNCTION, same reason as the
	 * other async entry points.
	 *
	 * RecipeClassPath: any of Recipe_ConveyorBeltMk1..Mk6. Tier/rate
	 * selection is deliberately not done here - see
	 * world.conveyorBeltTiers and controller/satisfactory_ai/conveyors.py.
	 * Source/dest are not required to be machines - any AFGBuildable
	 * with a free UFGFactoryConnectionComponent works, including belts
	 * themselves, so chaining calls should build multi-segment routes
	 * (not live-tested beyond single machine-to-machine segments).
	 *
	 * RouteMode: one of Straight/Curve/Auto (case-insensitive, empty
	 * leaves the hologram default). Added because the basic click
	 * sequence reliably fails on any real direction mismatch between
	 * source and dest connectors - not yet live-verified to resolve it.
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
	 * Telemetry. KEY FINDING (see docs/conveyor-attachment-research.md):
	 * splitters/mergers use AFGConveyorAttachmentHologram :
	 * AFGFactoryHologram : AFGBuildableHologram, the same simple single-
	 * step lineage as Miners/Smelters/Constructors, not the
	 * AFGSplineHologram branch belts/pipes need multi-click driving for.
	 * So ConstructBuildingAtPosition + ConstructConveyorBelt (both
	 * already generic) place and connect them with zero new construction
	 * code - deliberately no ConstructSplitter wrapper.
	 *
	 * Reports the real recipe catalog (Splitter/Merger plus Smart/
	 * Programmable Splitter and Priority Merger) with each variant's
	 * real inputCount/outputCount, read via GetDirection() on each CDO's
	 * connectors via GetDefaultComponents<>() - plain GetComponents<>()
	 * finds nothing here since these connectors are Blueprint-SCS-added,
	 * not native CreateDefaultSubobject (fixed 2026-08-27; this
	 * previously silently reported 0/0 for every entry). Also reports
	 * supportsSortRules (true only for Smart/Programmable variants) -
	 * per-output item-type routing (mSortRules/AddSortRule, public on
	 * FGBuildableSplitterSmart.h) has no write operation yet; placement
	 * alone does not let an agent configure routing rules.
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
	 * world.buildableCatalog - full field list in RPC_REFERENCE.md.
	 * Derived from the recipe catalog (filters isBuildingRecipe==true,
	 * resolves each to its AFGBuildable via
	 * UFGBuildingDescriptor::GetBuildableClass()) so construction cost is
	 * always exactly the recipe's ingredients. Clearance boxes are the
	 * buildable's real mClearanceData - the same data FactoryGame's own
	 * overlap checks use - read via IFGClearanceInterface, class-default
	 * only, never spawns anything.
	 *
	 * potentialShardSlots is only meaningful when overridesShardSlotCount
	 * is true; most buildings report the override off, meaning the real
	 * slot count falls back to a global default this per-building read
	 * cannot see - a real, documented gap, not a wrong number. Components
	 * are read via GetDefaultComponents<>() not GetComponents<>(), which
	 * misses Blueprint-SCS-added connectors (see
	 * LogConveyorAttachmentCatalogAsJson).
	 */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FString LogBuildableCatalogAsJson(UObject* WorldContextObject);

	/**
	 * Telemetry (JSON string, not FDocModOperationResult). Returns
	 * AFGBuildableConveyorBase::GetSpeed() for each Recipe_ConveyorBeltMk1..
	 * Mk6 buildable class, read live off each CDO. GetSpeed()'s unit is
	 * unconfirmed from source (FactoryGame's own header just says "Speed
	 * of this conveyor") - treat values as relative/comparable across
	 * tiers, not confirmed items-per-minute. A recipe whose class fails
	 * to load is simply omitted, not a hard error.
	 *
	 * Also reports maxSplineLength/bendRadius (public hologram getters)
	 * and maxInclineDegrees (mMaxIncline, no public getter, read via
	 * FindFProperty reflection - a single hardcoded field lookup, not a
	 * generic property-access capability), read off each recipe's
	 * hologram class CDO via
	 * ResolveConveyorBeltHologramClassForRecipe/GetHologramClass (a
	 * different accessor than the buildable class GetSpeed() reads off).
	 * Lets a caller check before connecting whether two connectors are
	 * too far apart or too steep for one belt segment and need chaining
	 * (see ConstructConveyorBelt). Omitted per-tier if the hologram class
	 * does not resolve.
	 */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FString LogConveyorBeltTiersAsJson(UObject* WorldContextObject);

	/**
	 * Telemetry. Returns Recipe_PowerLine's buildable class
	 * (AFGBuildableWire) CDO's mMaxLength/mMaxPowerTowerLength/
	 * mLengthPerCost - public EditDefaultsOnly UPROPERTYs with a
	 * documented "[cm]" unit, unlike the belt tier data's ambiguous-unit
	 * GetSpeed(). Only one power line tier exists (no Mk1..N), so this
	 * returns a single flat object, not an array.
	 *
	 * ConstructPowerConnection's source/dest lookup is already generic
	 * (FindFreePowerConnection searches any AFGBuildable, not hardcoded
	 * to machines), so a real power pole should work as an intermediate
	 * relay for a connection exceeding mMaxLength by chaining
	 * world.connectPower calls - untested live so far, every power
	 * connection built has been one direct machine-to-machine segment.
	 * See docs/conveyor-power-connection-research.md: a machine's
	 * default single power slot may require routing through a pole even
	 * for a short connection if the daisy-chain unlock is not active in
	 * the current save, separate from the mMaxLength question.
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
	 * Near-exact mirror of ConstructConveyorBelt: same build-gun two-
	 * click mechanism (TrySnapToActor + DoMultiStepPlacement(true) at
	 * source, then dest), including the UpdateHologramPlacement-before-
	 * TrySnapToActor and real GetConnectorNormal fixes. AFGPipelineHologram
	 * is a sibling of AFGConveyorBeltHologram (both derive from
	 * AFGSplineHologram), but pipes use their own parallel connection
	 * hierarchy - UFGPipeConnectionComponentBase/EPipeConnectionType
	 * (PCT_PRODUCER/PCT_CONSUMER) - not UFGFactoryConnectionComponent.
	 *
	 * AFGSplineHologram has no GetAnyConnectedBuildables (only
	 * AFGConveyorBeltHologram declares that), so the post-end-click
	 * diagnostic here uses IsConnectionSnapped(false) instead, the same
	 * not-fully-reliable indicator noted on DebugCheckConveyorSnap.
	 * Fluid type mismatch (UFGCDPipeFluidTypeMismatch) is left to the
	 * real CanConstruct() disqualifier check, same as any other
	 * disqualifier this function does not special-case. Whether
	 * chaining calls through an intermediate pipe support pole works
	 * like it does for belts/power, or poles only auto-spawn internally
	 * via the hologram's own default support recipe, is unconfirmed.
	 *
	 * RecipeClassPath: Recipe_Pipeline or Recipe_PipelineMK2 (see
	 * world.pipelineTiers for each tier's real flowLimit/
	 * maxSplineLength/bendRadius). Tier selection and multi-segment
	 * routing are deliberately not decided here. Source/dest need not
	 * be machines - any AFGBuildable with a free
	 * UFGPipeConnectionComponentBase works.
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
