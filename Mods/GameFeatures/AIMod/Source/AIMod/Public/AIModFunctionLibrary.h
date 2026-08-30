// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AIModTelemetryTypes.h"
#include "AIModOperationTypes.h"
#include "AIModFunctionLibrary.generated.h"

/**
 * Binds AFGAdminInterface::SaveGame's dynamic delegate (a UFUNCTION-bound
 * delegate, not a plain TFunction like this mod's other async operations
 * use) to forward the result into UAIModFunctionLibrary::SaveGame's
 * TFunction callback (2026-08-30). One short-lived instance per save
 * request - AddToRoot() on creation keeps it alive across the async save,
 * RemoveFromRoot() in the handler releases it once the delegate fires.
 */
UCLASS()
class UAIModSaveGameCallbackProxy : public UObject
{
	GENERATED_BODY()
public:
	TFunction<void(const FAIModOperationResult&)> OnComplete;

	UFUNCTION()
	void HandleSaveComplete(bool bSuccess, const FText& ErrorMessage);
};

/**
 * AIMod AI interface Blueprint entry points. PLAN.md Phase 3-12: a
 * version smoke test, read-only world telemetry (resource nodes,
 * buildables, manufacturers, factory connections) with JSON
 * serialization, and - as of Phase 12 - the first two controlled write
 * operations (SetManufacturerClockSpeed, SetManufacturerRecipe), each
 * with explicit validation per CLAUDE.md's Safety and Stability Boundary.
 * This class must never grow a generic "call any function by name" or
 * "set any property by name" method.
 */
UCLASS()
class AIMOD_API UAIModFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Returns the AIMod AI interface version string. */
	UFUNCTION(BlueprintPure, Category = "AIMod|AI Interface", meta = (BlueprintThreadSafe))
	static FString GetInterfaceVersion();

	/**
	 * Reads a bool property's live value out of AIMod's player-facing mod
	 * configuration (see AIModConfiguration.h - UAIModConfiguration,
	 * registered via UConfigManager::RegisterModConfiguration in
	 * UAIModHttpServerSubsystem::Initialize). Returns DefaultValue if the
	 * config manager/section/property can't be found (e.g. called before
	 * the game instance subsystem chain is ready) - never throws, never
	 * treats a missing config as an error, since these are all
	 * off-by-default safety/capability toggles where "can't read it" and
	 * "player left it off" should behave identically.
	 *
	 * Not a UFUNCTION - internal infrastructure shared between
	 * AIModFunctionLibrary.cpp's construction functions and
	 * AIModHttpServerSubsystem.cpp's loopback check, not part of the
	 * external RPC surface.
	 */
	static bool GetAIModConfigBool(UObject* WorldContextObject, const FString& PropertyName, bool DefaultValue);

	/** Float counterpart to GetAIModConfigBool - see its doc comment. */
	static float GetAIModConfigFloat(UObject* WorldContextObject, const FString& PropertyName, float DefaultValue);

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
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FString LogGroundHeightAsJson(UObject* WorldContextObject, float X, float Y, float ReferenceZ);

	/**
	 * world.terrainHeightGrid (2026-08-30) - batched version of
	 * world.groundHeight: the SAME real trace (FindGroundAtXY, shared
	 * helper - can't drift out of sync with either single-point queries
	 * or real construction placement), run for an entire rectangular
	 * grid of X/Y points in ONE call instead of one HTTP round-trip per
	 * point. Added per explicit user request after a live circular-
	 * platform build needed hundreds of individual world.groundHeight
	 * calls from Python to scan for a terrain intrusion - each call
	 * paying full HTTP+JSON overhead for a trace that itself takes a
	 * small fraction of that time. Batching moves the loop server-side,
	 * where only the real trace cost remains per point.
	 *
	 * Grid: MinX/MinY to MaxX/MaxY inclusive, spaced StepSize apart in
	 * both axes (CountX = floor((MaxX-MinX)/StepSize)+1, same for Y).
	 * ReferenceZ is the same +/-1000-unit search-center anchor as
	 * world.groundHeight (defaults to the player's current Z if <=
	 * -1000000) - used for EVERY point in the grid, so choose an anchor
	 * that plausibly covers the whole area's real height range, or issue
	 * multiple calls with different anchors for areas spanning a wide Z
	 * range (e.g. a cliff).
	 *
	 * Result is two parallel flat arrays (row-major, length
	 * CountX*CountY: index = row*CountX + col, x = MinX + col*StepSize,
	 * y = MinY + row*StepSize) rather than an array of per-point
	 * objects - deliberately compact, since a useful survey area can
	 * easily mean thousands of points and per-point JSON object overhead
	 * would dominate the payload. No normal is returned (unlike
	 * world.groundHeight) - query that single point directly if the
	 * surface normal at a specific spot is needed; the batched form is
	 * for height/gap survey, not full surface characterization.
	 *
	 * Capped at MaxTerrainHeightGridPoints (10000, ~a 100x100 grid) to
	 * bound the single-call cost - this runs entirely on the game
	 * thread like every other trace in this file, so an unbounded grid
	 * would risk a real frame hitch. Returns "tooManyPoints":true with
	 * the real requested/max counts (no heights/found data) if exceeded
	 * - tile a larger area into multiple calls, or use a coarser
	 * stepSize, rather than raising the cap.
	 *
	 * Real terrain doesn't change between sessions on the same map/game
	 * version (per project discussion, Satisfactory's map is hand-
	 * crafted and static) - callers are expected to CACHE a survey's
	 * result to a local file for reuse rather than re-querying the same
	 * area repeatedly; this function itself has no caching of its own,
	 * it's a faster primitive for the caller's own cache-building pass.
	 */
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FString LogTerrainHeightGridAsJson(UObject* WorldContextObject, float MinX, float MinY, float MaxX, float MaxY, float StepSize, float ReferenceZ);

	/**
	 * Enumerates all AFGResourceNode actors in the world via TActorIterator
	 * (see docs/resource-node-research.md for why - no working manager API
	 * was found) and returns them as normalized, protocol-facing telemetry
	 * structs (PLAN.md Phase 5, Task 8). Id is session-local only; see
	 * FAIModResourceNodeTelemetry's comment.
	 */
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static TArray<FAIModResourceNodeTelemetry> GetResourceNodeTelemetry(UObject* WorldContextObject);

	/** Debug entry point: logs one human-readable line per resource node via LogAIModAI. */
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static void LogResourceNodes(UObject* WorldContextObject);

	/**
	 * Serializes resource node telemetry to the Phase 6 JSON protocol
	 * shape ({"protocolVersion":1,"resourceNodes":[...]}), logs it via
	 * LogAIModAI, and returns the JSON string. Used internally by the
	 * "world.resourceNodes" RPC method (UAIModHttpServerSubsystem).
	 */
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
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
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static TArray<FAIModBuildableTelemetry> GetBuildableTelemetry(UObject* WorldContextObject);

	/** Debug entry point: logs one line per buildable via LogAIModAI. */
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static void LogBuildables(UObject* WorldContextObject);

	/** Serializes buildable telemetry to {"protocolVersion":1,"buildables":[...]}, logs it, and returns it. */
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FString LogBuildablesAsJson(UObject* WorldContextObject);

	/**
	 * world.vehicles (2026-08-29) - AFGVehicle is not an AFGBuildable, so
	 * world.buildables cannot see anything world.constructVehicle builds.
	 * Minimal id/class/position/rotation via a real TActorIterator<AFGVehicle>
	 * scan, same shape as world.buildables. Enough to find an id for
	 * world.deleteBuilding (now vehicle-aware too). Richer per-vehicle
	 * state (fuel, cargo, drone docking status) is future work.
	 */
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FString LogVehiclesAsJson(UObject* WorldContextObject);

	/**
	 * Enumerates all AFGBuildableManufacturer actors and reads their
	 * current recipe, clock speed, production status/progress/
	 * productivity, and input/output inventory contents (PLAN.md
	 * Phase 10, "machine recipes" / "machine inventories" / "machine
	 * production status"). Read-only.
	 */
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static TArray<FAIModManufacturerTelemetry> GetManufacturerTelemetry(UObject* WorldContextObject);

	/** Debug entry point: logs one line per manufacturer via LogAIModAI. */
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static void LogManufacturers(UObject* WorldContextObject);

	/** Serializes manufacturer telemetry to {"protocolVersion":1,"manufacturers":[...]}, logs it, and returns it. */
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
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
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FAIModManufacturerTelemetry GetTargetedManufacturer(UObject* WorldContextObject);

	/**
	 * Serializes GetTargetedManufacturer's result to
	 * {"protocolVersion":1,"manufacturer":null|{...}}, logs it, and
	 * returns it. "manufacturer" is JSON null when nothing/non-manufacturer
	 * is targeted, not an error.
	 */
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FString LogTargetedManufacturerAsJson(UObject* WorldContextObject);

	/**
	 * Enumerates factory connection points on every AFGBuildableFactory
	 * actor (PLAN.md Phase 10, "conveyor connection components"). One row
	 * per connection point, not a constructed graph - see
	 * FAIModFactoryConnectionTelemetry's comment. This is the raw
	 * material Phase 11's external-controller-side world graph is built
	 * from, alongside GetBuildableTelemetry.
	 */
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static TArray<FAIModFactoryConnectionTelemetry> GetFactoryConnectionTelemetry(UObject* WorldContextObject);

	/** Debug entry point: logs one line per connection point via LogAIModAI. */
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static void LogFactoryConnections(UObject* WorldContextObject);

	/** Serializes connection telemetry to {"protocolVersion":1,"connections":[...]}, logs it, and returns it. */
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FString LogFactoryConnectionsAsJson(UObject* WorldContextObject);

	/**
	 * Same purpose as GetFactoryConnectionTelemetry, for pipes
	 * (UFGPipeConnectionComponentBase - covers both fluid pipes and
	 * hypertubes, see FAIModPipeConnectionTelemetry's comment). Added
	 * 2026-08-27 after discovering live that "world.connections" only
	 * ever covered factory connections, leaving no way to read a real
	 * pipe/hypertube connector's position/normal before placing one -
	 * exactly the data needed to plan a straight run instead of guessing
	 * rotation and hoping.
	 */
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static TArray<FAIModPipeConnectionTelemetry> GetPipeConnectionTelemetry(UObject* WorldContextObject);

	/** Debug entry point: logs one line per pipe/hypertube connection point via LogAIModAI. */
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static void LogPipeConnections(UObject* WorldContextObject);

	/** Serializes pipe connection telemetry to {"protocolVersion":1,"connections":[...]}, logs it, and returns it. */
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FString LogPipeConnectionsAsJson(UObject* WorldContextObject);

	/**
	 * Returns the local player character's current position/rotation
	 * (see FAIModPlayerTelemetry's comment for why this exists) - player
	 * index 0 only, single-player/local scope per PLAN.md/CLAUDE.md.
	 */
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FAIModPlayerTelemetry GetPlayerTelemetry(UObject* WorldContextObject);

	/** Serializes GetPlayerTelemetry to {"protocolVersion":1,"position":{...},"rotation":{...}}, logs it, and returns it. */
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FString LogPlayerAsJson(UObject* WorldContextObject);

	/**
	 * Serializes the current in-game time of day to
	 * {"protocolVersion":1,"hour":H,"minute":M,"daySeconds":S,"isDay":bool}
	 * via AFGTimeOfDaySubsystem::Get()'s own GetHours()/GetMinutes()/
	 * GetDaySeconds()/IsDay() - added 2026-08-27 so a caller can check the
	 * current time before deciding whether to call SetTimeOfDay.
	 */
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
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
	 * which isn't guaranteed in an ordinary session. Since AIMod's own
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
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FAIModOperationResult SetTimeOfDay(UObject* WorldContextObject, int32 Hour, int32 Minute);

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
	 * command dispatch (this is also how AIMod's own "/aimod" command
	 * works - see AIModChatCommand.cpp) before it's ever added as a
	 * normal chat message.
	 */
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
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
	 * to. Defaults Sender to "AIMod AI" and MessageType to
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
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FAIModOperationResult SendChatMessage(UObject* WorldContextObject, const FString& Message, const FString& Sender);

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
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FAIModOperationResult SetManufacturerClockSpeed(UObject* WorldContextObject, const FString& BuildableId, float ClockSpeedPercent);

	/**
	 * world.installPowerShard (2026-08-30) - a real, previously-missing
	 * capability: there was no way to actually insert a Power Shard
	 * before this, only to set a clock speed within whatever max
	 * potential already existed. Found live source support for it while
	 * planning a multi-tier overclocked production line test:
	 * AFGBuildableFactory::GetPotentialInventory() is real, public, and a
	 * plain UFGInventoryComponent* - the same direct AddStack pattern
	 * already used elsewhere in this file (SimulatedCraft,
	 * MovePortableMinerToInventory, the dismantle refund fix), not the
	 * more complex TryFillPotentialInventory (a "fill to X value"
	 * player-UI-shaped API with EPowerShardType bucketing this function
	 * deliberately does not use).
	 *
	 * Works uniformly on ANY AFGBuildableFactory - both manufacturers
	 * AND extractors (a Miner) - confirmed from source both derive from
	 * the same base class exposing GetPotentialInventory()/
	 * mCanChangePotential identically (AFGBuildableResourceExtractorBase
	 * : AFGBuildableFactory), so this one function covers overclocking a
	 * Mk3 Miner the same way it covers a Smelter or Constructor.
	 *
	 * Real, verified item class: the "Power Shard" item is
	 * /Game/FactoryGame/Resource/Environment/Crystal/Desc_CrystalShard.Desc_CrystalShard_C
	 * (confirmed via world.recipeCatalog, not guessed) - hardcoded here
	 * the same way DebugCheckPowerConnection hardcodes Recipe_PowerLine,
	 * since there is exactly one real overclock shard item in the game.
	 *
	 * Removes Count shards from the player's carried inventory only
	 * after verifying it holds that many (INSUFFICIENT_INGREDIENTS
	 * otherwise), adds them to the target's potential inventory via
	 * AddStack(allowPartialAdd=true), then - if fewer were actually
	 * added than requested (the potential inventory ran out of real
	 * slots) - returns the unplaced excess back to the player rather
	 * than losing it, same restore-on-partial-failure discipline as the
	 * dismantle refund fix. Fails with OPERATION_NOT_PERMITTED if the
	 * target has no potential inventory at all (mCanChangePotential is
	 * false) or if zero shards could be added (no free slots).
	 *
	 * Result detail (result.detail): shardsAdded (the real count that
	 * fit), newMaxPotentialPercent (GetCurrentMaxPotential()*100 after
	 * insertion - re-query this or world.setClockSpeed's own error
	 * message to get the real new overclock ceiling, do not assume +50%
	 * per shard without confirming live - the exact per-shard boost is a
	 * real UFGPowerShardDescriptor::GetBoostValue() value this function
	 * does not currently look up separately).
	 *
	 * NOT YET LIVE-TESTED - the real per-building default shard slot
	 * count (when overridesShardSlotCount is false, which is the case
	 * for all three buildings this was designed for: Miner Mk3, Smelter
	 * Mk1, Constructor Mk1) is a real value this project's own
	 * world.buildableCatalog cannot currently see (see
	 * LogBuildableCatalogAsJson's doc comment) - confirm the real slot
	 * count and per-shard boost live before assuming a specific number
	 * of shards will fit or how much overclock headroom they grant.
	 */
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FAIModOperationResult InstallPowerShard(UObject* WorldContextObject, const FString& BuildableId, int32 Count);

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
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FAIModOperationResult SetManufacturerRecipe(UObject* WorldContextObject, const FString& BuildableId, const FString& RecipeClassPath);

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
	 *
	 * REAL CONSTRUCTION-COST REFUND (fixed 2026-08-30, was a real bug
	 * before this) - GetDismantleRefund() is computed BEFORE dismantling
	 * and its stacks are added directly to the local player's carried
	 * inventory via AddStack(allowPartialAdd=true) after. Confirmed live
	 * that Execute_Dismantle() alone does NOT refund anything - that's a
	 * separate interface function the real player dismantle path calls
	 * independently; this function previously never called it at all,
	 * silently destroying every dismantled buildable's construction cost
	 * with no refund (confirmed live to have cost the user several
	 * thousand real Iron Plates before being caught - see
	 * docs/placement-lessons.md). If no local player/inventory can be
	 * found, the refund is logged as lost rather than silently dropped
	 * without a trace - dismantling itself still succeeds either way.
	 */
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FAIModOperationResult DismantleBuildable(UObject* WorldContextObject, const FString& BuildableId);

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
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FAIModResourceNodeTelemetry GetTargetedResourceNode(UObject* WorldContextObject);

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
	 * ErrorCode="PENDING" and the real result is logged to LogAIModAI
	 * once polling resolves (or a safety-cap number of ticks is hit).
	 * See docs/extractor-placement-research.md for the full trail of
	 * evidence this function exists to gather - the two load-bearing
	 * assumptions (synthetic-hit-result snapping, and hologram
	 * construction without a real AFGBuildGun) are still unverified
	 * against real runtime behavior.
	 */
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FAIModOperationResult DebugCheckExtractorPlacementOnTargetedNode(UObject* WorldContextObject);

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
	 * real result logged to LogAIModAI once resolved).
	 */
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FAIModOperationResult DebugCheckExtractorPlacementViaBuildGun(UObject* WorldContextObject);

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
	 * confirmation) is logged to LogAIModAI once resolved.
	 *
	 * VISIBLE SIDE EFFECT: equips the build gun for the duration (same as
	 * the dry-run), unequipped afterward regardless of outcome.
	 */
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FAIModOperationResult ConstructExtractorOnTargetedNode(UObject* WorldContextObject);

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
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FAIModOperationResult ConstructBuildingNearPlayer(UObject* WorldContextObject, const FString& RecipeClassPath);

	/**
	 * Spawns a real AFGCreature near the player, added 2026-08-28 per
	 * explicit user request. Gated behind the "AllowCreatureSpawning" mod
	 * setting (see AIModConfiguration.h), OFF by default - unlike
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
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FAIModOperationResult SpawnCreatureNearPlayer(UObject* WorldContextObject, const FString& CreatureClassPath, float DistanceFromPlayer, float Scale);

	/**
	 * Despawns a creature previously spawned via SpawnCreatureNearPlayer
	 * (or any real AFGCreature), by its GetPathName() id. Added
	 * 2026-08-28 for cleanup before a save - narrowly scoped to
	 * AFGCreature (TActorIterator<AFGCreature> lookup), not a generic
	 * "destroy any actor" capability - see CLAUDE.md's Safety and
	 * Stability Boundary.
	 */
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FAIModOperationResult DespawnCreature(UObject* WorldContextObject, const FString& CreatureId);

	/**
	 * Building placement at an explicit X/Y (not near-player). Not a
	 * UFUNCTION - TFunction callbacks are not UHT-compatible - plain C++
	 * entry point for UAIModHttpServerSubsystem only. Same validated
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
	static void ConstructBuildingAtPosition(UObject* WorldContextObject, const FString& RecipeClassPath, float X, float Y, int32 RotationScrollDelta, float GridSnapSize, float ReferenceZ, bool bIgnoreGroundTrace, bool bIgnoreAimLocation, bool bIgnorePlayerEncroachment, bool bIgnoreClearance, bool bIgnoreInvalidFloor, bool bHasTargetYaw, float TargetYawDegrees, const FString& FaceBuildableId, TFunction<void(const FAIModOperationResult&)> OnComplete);

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
	 * logged to LogAIModAI once resolved) and the same visible
	 * build-gun-equip side effect.
	 */
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FAIModOperationResult DebugCheckPowerConnection(UObject* WorldContextObject, const FString& BuildableIdA, const FString& BuildableIdB);

	/**
	 * Places a real extractor on a resource node by session-local id
	 * (world.resourceNodes), instead of requiring the player to be
	 * looking at it. Not a UFUNCTION - plain C++ entry point for
	 * UAIModHttpServerSubsystem, same async shape as
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
	static void ConstructExtractorOnNode(UObject* WorldContextObject, const FString& NodeId, const FString& RecipeClassPath, TFunction<void(const FAIModOperationResult&)> OnComplete);

	/**
	 * world.constructVehicle (2026-08-29) - Drones and wheeled vehicles
	 * (Tractor/Truck/Explorer/Cyber Wagon/Golf Cart) are hologram-driven
	 * (AFGVehicleHologram : AFGHologram), the SAME class hierarchy every
	 * other Construct* function here already drives - confirmed from
	 * source, NOT the Portable Miner's equipment-dispenser/reflection
	 * pattern. ConstructBuildingAtPosition refuses vehicle recipes
	 * outright (WRONG_METHOD_FOR_VEHICLE) so this is the only path.
	 *
	 * DroneStationId (optional): if provided, snaps to that
	 * AFGBuildableDroneStation the same way ConstructExtractorOnNode
	 * snaps to a resource node - required for drone recipes, since
	 * AFGBuildableDroneHologram has a mandatory mSnappedStation
	 * reference (UFGCDMustSnapStation/UFGCDOccupiedStation/
	 * UFGCDDroneStationHasDrone in FGConstructDisqualifier.h) this
	 * function never bypasses, by the same conservative logic
	 * ConstructBuildingAtPosition refuses extractors outright - a real,
	 * confirmed crash already exists for an analogous unset-mandatory-
	 * reference case (AFGResourceExtractorHologram::ConfigureActor), and
	 * whether AFGBuildableDroneHologram's own ConstructVehicle() is
	 * equally unsafe when unsnapped is unconfirmed from source (stub
	 * .cpp), not worth gambling on.
	 *
	 * Left empty: free placement at literal X/Y, same
	 * bIgnoreGroundTrace/ground-trace choice as
	 * ConstructBuildingAtPosition - for wheeled vehicles, which have no
	 * mandatory-reference disqualifier found in source.
	 *
	 * AFGVehicle is not an AFGBuildable (a separate AFGDriveablePawn
	 * hierarchy) - construction confirmation uses a real AFGVehicle
	 * actor-iterator proximity scan instead of AFGBuildableSubsystem's
	 * registry, everything else here mirrors ConstructExtractorOnNode's
	 * proven poll/disqualifier/construct shape.
	 *
	 * A freshly-built vehicle still needs fuel (GetFuelInventory()) to
	 * actually move, and a drone still needs its station paired to a
	 * destination (AFGDroneSubsystem::Server_PairStations, a public
	 * BlueprintCallable function, not yet exposed here) before it will
	 * fly a route - neither is handled by this function, which only
	 * covers construction itself. NOT YET LIVE-TESTED.
	 */
	static void ConstructVehicle(UObject* WorldContextObject, const FString& RecipeClassPath, const FString& DroneStationId, float X, float Y, float Z, bool bIgnoreGroundTrace, bool bHasTargetYaw, float TargetYawDegrees, TFunction<void(const FAIModOperationResult&)> OnComplete);

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
	static void ConstructPortableMinerOnNode(UObject* WorldContextObject, const FString& NodeId, const FString& ItemClassPath, TFunction<void(const FAIModOperationResult&)> OnComplete);

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
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
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
	static void RetrievePortableMinerInventory(UObject* WorldContextObject, const FString& PortableMinerId, TFunction<void(const FAIModOperationResult&)> OnComplete);

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
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FAIModOperationResult MovePortableMinerToInventory(UObject* WorldContextObject);

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
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FAIModOperationResult SimulatedCraft(UObject* WorldContextObject, const FString& RecipeClassPath);

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
	 * the current manual workaround.
	 *
	 * FIXED 2026-08-30 (real bug): this previously gated the item lookup
	 * behind AFGCentralStorageSubsystem::IsCentralStorageBuilt(), which
	 * reports a SEPARATE container-registration bookkeeping array
	 * (mCentralStorages) - confirmed live unreliable/false even with
	 * real, already-built AFGCentralStorageContainer buildables present
	 * (12 confirmed via world.buildables, user reported thousands of
	 * real items), so this silently reported an empty Depot regardless
	 * of actual contents. Now calls GetAllItemsFromCentralStorage()
	 * unconditionally - the `isCentralStorageBuilt` field in the
	 * response still reflects the same unreliable flag for reference/
	 * diagnostics, but callers should trust `items` being empty (not
	 * this flag) to mean "genuinely nothing in the Depot."
	 */
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FString LogCentralStorageAsJson(UObject* WorldContextObject);

	/**
	 * Reports every item currently in the local player's CARRIED
	 * inventory (UFGInventoryComponent, general backpack - not the ARMS
	 * equipment slot, not the Dimensional Depot), added 2026-08-30. One
	 * aggregated entry per distinct item class, summed across every
	 * stack. Empty {"items":[]} (hasPlayer:false) if no local
	 * AFGCharacterPlayer exists rather than an error. Complements
	 * LogCentralStorageAsJson - added specifically so a caller can
	 * compute a reliable "combined carried + Depot" total for a given
	 * item (no RPC previously exposed carried-inventory counts at all).
	 */
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FString LogPlayerInventoryAsJson(UObject* WorldContextObject);

	/**
	 * Triggers a real, local game save via the same path the pause menu's
	 * "Save" button uses (AFGPlayerControllerBase::GetAdminInterface() ->
	 * AFGAdminInterface::SaveGame(true, ...)), added 2026-08-30 per
	 * explicit user request - repeated rebuild/redeploy/restart cycles
	 * during live testing had no way to checkpoint progress beforehand
	 * short of the user manually pausing and saving.
	 *
	 * SaveName, if empty, falls back to the current session's name
	 * (AFGGameState::GetSessionName()) so this overwrites the active save
	 * slot, matching a normal in-game quicksave rather than creating a new
	 * save file. Fails with NO_SESSION_NAME if that's also empty (no
	 * active session to save).
	 *
	 * Always saves locally (the `locally` bool AFGAdminInterface::SaveGame
	 * takes) - this project's primary target is a single-player/local
	 * session per CLAUDE.md; a remote/host save isn't exposed here.
	 *
	 * Genuinely asynchronous - SaveGame's own delegate is a Save
	 * confirmation from the engine, not just "request sent" - so OnComplete
	 * only fires once the save has actually finished (or failed).
	 */
	static void SaveGame(UObject* WorldContextObject, const FString& SaveName, TFunction<void(const FAIModOperationResult&)> OnComplete);

	/**
	 * Withdraws items from the Dimensional Depot into the player's
	 * general inventory, added 2026-08-28 alongside
	 * LogCentralStorageAsJson - see that function's doc comment for why
	 * this is needed (Depot storage and carried inventory are genuinely
	 * separate, confirmed live). Uses the real
	 * AFGCentralStorageSubsystem::TryRemoveItemsFromCentralStorage(),
	 * which itself clamps to whatever is actually available (a request
	 * for more than the Depot holds is not an error - it withdraws
	 * whatever it can). Fails with NO_CENTRAL_STORAGE only if the
	 * subsystem itself doesn't exist for this world (should never
	 * happen in practice), or NOTHING_WITHDRAWN if the Depot holds none
	 * of the requested item. FIXED 2026-08-30 (same real bug as
	 * LogCentralStorageAsJson): previously also gated on
	 * IsCentralStorageBuilt(), which silently blocked every withdrawal
	 * attempt even with real, populated Depot storage present - removed
	 * that gate.
	 */
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FAIModOperationResult WithdrawFromCentralStorage(UObject* WorldContextObject, const FString& ItemClassPath, int32 Amount);

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
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
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
	static void ConstructPowerConnection(UObject* WorldContextObject, const FString& BuildableIdA, const FString& BuildableIdB, bool bDryRun, bool bIgnoreAimLocation, bool bIgnoreWireSnap, TFunction<void(const FAIModOperationResult&)> OnComplete);

	/**
	 * Diagnostic only, not a real placement attempt: spawns a real belt
	 * hologram and feeds a synthetic hit at a free output connection
	 * through three entry points (UpdateHologramPlacement, TrySnapToActor,
	 * a single click), logging the resulting connection state after
	 * each. Never calls CanConstruct/Construct, never touches the save,
	 * reports synchronously.
	 */
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FAIModOperationResult DebugCheckConveyorSnap(UObject* WorldContextObject, const FString& SourceBuildableId);

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
	 *
	 * InstigatorStrategy (2026-08-30, case-insensitive, empty defaults to
	 * "PlayerController"): which pawn/controller drives the hologram's
	 * construction. "RealCharacter" uses the actual player's real,
	 * equipped BuildGun (proven reliable, but visibly moves the real
	 * camera - see ConstructConveyorBelt_RealCharacterStrategy's comment
	 * in the .cpp). "AIController"/"PlayerController" spawn a throwaway
	 * decoy pawn+controller instead so the real player's camera is never
	 * touched - both CONFIRMED (live-tested) to leave the hologram
	 * permanently stuck on UFGCDInitializing, controller class ruled out
	 * as the variable. "LocalPlayer" (added 2026-08-30, NOT YET
	 * LIVE-TESTED - written and compiled without a redeploy being
	 * possible) spawns a genuine second ULocalPlayer via
	 * UGameInstance::CreateLocalPlayer() instead of a bare decoy, on the
	 * hypothesis that genuine local-player identity (not just controller
	 * class) is what UFGCDInitializing's gate actually requires - see
	 * docs/camera-hijack-and-second-player-research.md for the full
	 * research this is based on. See the .cpp's "Decoy-instigator
	 * rewrite" comment for the full story.
	 *
	 * SourceConnectorPosition/DestConnectorPosition (2026-08-30, explicit
	 * user requirement): when provided (real world coordinates, e.g. from
	 * a prior world.connections call), targets ONE SPECIFIC connector by
	 * position instead of "the first free one of the right direction" -
	 * required for deterministic port selection on a multi-output
	 * buildable like a splitter (world.connections' own "direction" field
	 * only tells you Input vs Output, not WHICH of several same-direction
	 * connectors will be used - this is what makes that choice explicit
	 * and provable). Optional; omitting both keeps prior behavior exactly.
	 * Errors NO_FACTORY_CONNECTION if nothing free is within tolerance of
	 * the given position - never silently falls back to a different one.
	 */
	static void ConstructConveyorBelt(UObject* WorldContextObject, const FString& SourceBuildableId, const FString& DestBuildableId, const FString& RecipeClassPath, const FString& RouteMode, const FString& InstigatorStrategy, const TOptional<FVector>& SourceConnectorPosition, const TOptional<FVector>& DestConnectorPosition, bool bDryRun, TFunction<void(const FAIModOperationResult&)> OnComplete);

	/**
	 * Read-only telemetry (2026-08-30) - returns a placed belt or pipe's
	 * REAL world-space path, added specifically so a mod-constructed
	 * conveyor's actual geometry can be compared against a normally
	 * (player-)placed one. world.connectConveyor's belts have been
	 * observed to curve unpredictably despite aligned connectors, and
	 * there was previously no way to inspect the resulting path itself -
	 * only whether the two endpoints ended up connected
	 * (world.connections). Works on anything implementing
	 * IFGSplineBuildableInterface - confirmed from source that both
	 * AFGBuildableConveyorBelt and AFGBuildablePipeBase share this
	 * interface and its exact accessor set, so this is deliberately
	 * generic rather than belt-specific. Does NOT cover
	 * AFGBuildableConveyorLift - lifts are not spline-based (confirmed
	 * from source: a lift's placement is fully described by its
	 * mTopTransform/GetHeight(), no spline component exists on that
	 * class at all).
	 *
	 * Same "embed found/error in the payload" convention as
	 * LogGroundHeightAsJson, not a thrown RPC error - see that function's
	 * doc comment for why (every Log*AsJson result is unconditionally
	 * wrapped success:true at the dispatch layer).
	 */
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FString LogSplineGeometryAsJson(UObject* WorldContextObject, const FString& BuildableId);

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
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
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
	static void ConstructConveyorLift(UObject* WorldContextObject, const FString& SourceBuildableId, const FString& DestBuildableId, const FString& RecipeClassPath, bool bDryRun, TFunction<void(const FAIModOperationResult&)> OnComplete);

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
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
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
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
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
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
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
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FString LogBuildableCatalogAsJson(UObject* WorldContextObject);

	/**
	 * world.constructionCost (2026-08-28) - the recipe catalog's own
	 * "ingredients" field is the BASE recipe cost only. Real construction
	 * (both interactive and via world.placeBuilding) also charges for
	 * whatever building customization (swatch/pattern/material) is
	 * currently active for that buildable category - confirmed live: an
	 * RPC wall placement failed "Missing materials!" needing Iron Plate
	 * even though Recipe_Wall_8x4_01's own ingredients list only Concrete,
	 * matching the player's own build menu showing the same combined
	 * cost. FFactoryCustomizationData::GetAppliedRecipes() is the real
	 * mechanism - each applied customization (a UFGCustomizationRecipe,
	 * itself a UFGRecipe subclass) has its own ingredients, added on top
	 * of the base recipe's.
	 *
	 * Spawns a real hologram for RecipeClassPath via the same HotKeyRecipe
	 * path construction uses (so it inherits whatever default swatch the
	 * engine itself would apply to a real new placement), reads its
	 * mCustomizationData (protected, no public getter - read via
	 * FStructProperty reflection, the established pattern for protected
	 * UPROPERTYs in this file), sums GetIngredients() across the base
	 * recipe and every applied customization recipe (merging amounts for
	 * the same item class), then unequips - never calls CanConstruct or
	 * Construct, nothing is placed. Generic across every buildable
	 * category (walls, foundations, pipes, roofs, etc.) since it is
	 * driven by the same real per-hologram customization state the game
	 * itself maintains, not a hardcoded per-category rule.
	 *
	 * Returns "baseIngredients" (matches world.recipeCatalog for this
	 * recipe), "appliedCustomizationRecipes" (class paths, empty if no
	 * swatch/pattern/material is currently active for this category), and
	 * "totalIngredients" (the real combined cost) - use the last one for
	 * an accurate affordability check before building, not
	 * world.recipeCatalog's "ingredients" alone.
	 */
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FString LogConstructionCostAsJson(UObject* WorldContextObject, const FString& RecipeClassPath);

	/**
	 * Telemetry (JSON string, not FAIModOperationResult). Returns
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
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
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
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
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
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
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
	static void ConstructPipe(UObject* WorldContextObject, const FString& SourceBuildableId, const FString& DestBuildableId, const FString& RecipeClassPath, bool bDryRun, TFunction<void(const FAIModOperationResult&)> OnComplete);

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
	static void ConstructHypertube(UObject* WorldContextObject, const FString& SourceBuildableId, const FString& DestBuildableId, bool bDryRun, TFunction<void(const FAIModOperationResult&)> OnComplete);

	/**
	 * world.constructRailroadTrack (2026-08-29) - researched from source
	 * before implementing: AFGRailroadTrackHologram : AFGSplineHologram,
	 * the SAME base ConstructPipe/ConstructConveyorBelt already drive
	 * (GetConstructDisqualifiers/TrySnapToActor/DoMultiStepPlacement/
	 * GetCurrentBuildStep are all AFGSplineHologram members) - a near-
	 * mirror of ConstructPipe, same two-click snap-to-connector-component
	 * flow, differing only in the connector type
	 * (UFGRailroadTrackConnectionComponent, bidirectional - no producer/
	 * consumer split).
	 *
	 * SourceBuildableId/DestBuildableId must each have a free
	 * UFGRailroadTrackConnectionComponent (e.g. a Train Station platform,
	 * or an existing track's open end) - deliberately mirrors belts/
	 * pipes' existing-buildable-to-existing-buildable model rather than
	 * solving free-floating track placement in this pass. Switches and
	 * signals are out of scope - this only builds a single point-to-point
	 * segment; UFGCDTrackTooLong/TooShort/TooSteep/TrunToSharp (sic, real
	 * name typo in FGConstructDisqualifier.h) are never bypassed.
	 *
	 * NOT YET LIVE-TESTED - implemented from header research only
	 * (FGRailroadTrackHologram.cpp is a stub, real construct-path
	 * behavior unconfirmed).
	 */
	static void ConstructRailroadTrack(UObject* WorldContextObject, const FString& SourceBuildableId, const FString& DestBuildableId, const FString& RecipeClassPath, bool bDryRun, TFunction<void(const FAIModOperationResult&)> OnComplete);

	/**
	 * world.constructVehiclePathSegment (2026-08-29) - researched from
	 * source before implementing: AFGVehiclePathSegmentHologram :
	 * AFGBuildableHologram (not AFGSplineHologram, unlike tracks/belts/
	 * pipes), but implements the same TrySnapToActor+DoMultiStepPlacement
	 * two-click contract on its own terms.
	 *
	 * Unlike every other spline-ish Construct* function here,
	 * StartX/Y/Z and EndX/Y/Z are LITERAL coordinates, not existing
	 * buildable ids - path nodes are free and auto-created by segment
	 * placement (confirmed from source:
	 * AFGVehiclePathSegment::SetNodeConnections's own doc comment, "Null
	 * connections will be automatically initialized to fresh nodes").
	 * Same ignoreGroundTrace/literal-Z convention as
	 * ConstructVehicle/ConstructBuildingAtPosition - the direct way to
	 * lay a path on a flat foundation platform rather than raw terrain.
	 * A point near an existing AFGVehiclePathNode/AFGVehiclePathSegment
	 * (within ~800cm) lets the hologram's own TrySnapToActor connect to
	 * it instead of creating a new node - not specially handled here,
	 * same "let the real engine trace decide" posture as
	 * ConstructExtractorOnNode.
	 *
	 * Deliberately NOT covered: assigning a built vehicle to auto-drive
	 * a route over these segments. Real, public, BlueprintCallable API
	 * exists for this in source
	 * (AFGWheeledVehicleIdentifier::SetVehicleRoute/AddWaypoint/
	 * SetAutopilotEnabled, resolved via AFGVehicleSubsystem's path-node
	 * GUID lookups) but needs its own telemetry/RPC layer (path node
	 * GUIDs aren't exposed anywhere yet) - a real, separate follow-up,
	 * same posture as drone station-pairing being deferred alongside
	 * ConstructVehicle.
	 *
	 * NOT YET LIVE-TESTED - implemented from header research only
	 * (FGVehiclePathSegmentHologram.cpp is a stub, real construct-path
	 * behavior unconfirmed).
	 */
	static void ConstructVehiclePathSegment(UObject* WorldContextObject, const FString& RecipeClassPath, float StartX, float StartY, float StartZ, float EndX, float EndY, float EndZ, bool bIgnoreGroundTrace, TFunction<void(const FAIModOperationResult&)> OnComplete);

	/**
	 * world.milestoneProgress (2026-08-29) - reports HUB milestone/tutorial
	 * schematic progress by tier (AFGSchematicManager::GetHubSchematicsForTier/
	 * GetTechTierState/GetRemainingCostFor/GetPaidOffCostFor/IsSchematicPurchased/
	 * GetActiveSchematic - all real, public, non-stub-bodied getters, unlike
	 * most of this project's other FactoryGame research targets) plus every
	 * AFGBuildableSpaceElevator's phase-upgrade state (GetNextPhaseCost/
	 * IsReadyToUpgrade/IsFullyUpgraded/GetInputInventory - the Elevator is a
	 * normal AFGBuildableFactory, already visible to world.buildables and
	 * already belt-connectable via the existing generic world.connectConveyor
	 * path with zero new code, per the user's own framing that the Elevator
	 * "can be fed with conveyor belts" unlike the HUB).
	 *
	 * "The HUB" in player terms is AFGBuildableTradingPost's mHubTerminal
	 * sub-building - confirmed from source it holds no inventory of its own;
	 * milestone payment is tracked purely as FItemAmount bookkeeping on
	 * AFGSchematicManager (mPaidOffSchematic), not any physical buildable
	 * inventory - see PayOffMilestone below for the actual submission path.
	 *
	 * Tiers 0-14 are scanned (comfortably covers every real game tier);
	 * a tier is only included if GetHubSchematicsForTier returns anything
	 * for it, so unused/future tiers don't clutter the output.
	 */
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FString LogMilestoneProgressAsJson(UObject* WorldContextObject);

	/**
	 * world.payMilestone (2026-08-29) - moves items from the player's
	 * CARRIED inventory (AFGCharacterPlayer::GetInventory(), same scope
	 * SimulatedCraft/Construct* functions already use - NOT the Dimensional
	 * Depot, see LogCentralStorageAsJson's doc comment for that same
	 * established gap; use WithdrawFromCentralStorage first if the needed
	 * items are in the Depot) toward a HUB milestone/tutorial schematic's
	 * remaining cost, then calls the real
	 * AFGSchematicManager::PayOffOnSchematic to register the payment -
	 * answering the user's "can you assist moving necessary items from
	 * player inventory to the hub" question with a real, careful write path.
	 *
	 * If SchematicClassPath is empty, targets the manager's current
	 * GetActiveSchematic() (whatever the player has focused in the real
	 * in-game HUB widget) - does NOT call SetActiveSchematic itself, so it
	 * never silently redirects what the player is working toward.
	 *
	 * Per item in GetRemainingCostFor(schematic): submits
	 * min(remainingAmount, player's carried amount) - never more than what's
	 * owed, never more than what's actually carried, so this can only ever
	 * submit real, affordable amounts, same "verify affordability first"
	 * discipline as SimulatedCraft. If bDryRun, inventory and the schematic
	 * manager are never touched - only the would-be submission/shortfall is
	 * computed and reported, so this can be checked safely before the first
	 * live attempt.
	 *
	 * If NOT a dry run and nothing is submittable (player carries none of
	 * what's still owed), fails with NOTHING_TO_SUBMIT rather than
	 * claiming a silent no-op success. Otherwise removes each submitted
	 * item from carried inventory, calls PayOffOnSchematic, and - critically
	 * - if that call returns false, RESTORES every removed item back to the
	 * player's inventory (AddStack, allowPartialAdd=true) before failing
	 * with PAYOFF_REJECTED, the same restore-on-failure discipline already
	 * established for the Portable Miner ARMS-slot move
	 * (MovePortableMinerToInventory) - never destroys real items on a
	 * rejected payment.
	 *
	 * Result detail (submitted/shortfall item lists, dryRun flag) is
	 * reported via FAIModOperationResult::ResultDetailJson, a JSON object
	 * embedded under result.detail by the RPC layer - see
	 * AIModOperationTypes.h.
	 *
	 * PayOffOnSchematic's real contract is UNCONFIRMED from source
	 * (FGSchematicManager.cpp is a stub like nearly everything else in this
	 * codebase) - specifically whether it requires the target to already be
	 * the ACTIVE schematic, and whether it mutates the amount array it's
	 * passed (ProcessEvent-by-reference pattern seen elsewhere in this
	 * file). Deliberately NOT gated on "must be active" here - the real
	 * engine call is trusted to enforce or not enforce that itself
	 * (PAYOFF_REJECTED surfaces a false return either way) rather than this
	 * function guessing at a restriction the source doesn't actually state.
	 * NOT YET LIVE-TESTED.
	 */
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FAIModOperationResult PayOffMilestone(UObject* WorldContextObject, const FString& SchematicClassPath, bool bDryRun);

	/**
	 * world.mamStatus (2026-08-29) - full M.A.M. (research) status:
	 * AFGResearchManager's current/ongoing research (with time left - a
	 * reflective read of the protected mOngoingResearch array, no public
	 * full-list getter exists; GetResearchBeingConducted() only returns a
	 * single schematic even though mCanConductMultipleResearch is real),
	 * completed-but-unclaimed research, unclaimed hard drives and their
	 * pending alternate-recipe reward choices, and every visible research
	 * tree's nodes with each node's real ESchematicState (Locked/Available/
	 * Purchased/Hidden - the same enum world.milestoneProgress uses for HUB
	 * schematics) so a caller can tell "unlocked" apart from "locked but
	 * available" apart from genuinely locked, per the user's exact ask.
	 *
	 * Only research trees whose UFGResearchTree::GetResearchTreeStatus is
	 * NOT Locked are expanded with node detail - a fully locked tree isn't
	 * visible to the real player either, so its nodes would be noise.
	 *
	 * Note: AFGBuildableMAM itself gates nothing - InitiateResearch/
	 * ClaimResearchResults take no building reference at all, confirmed
	 * from source (same player-independence property as every other write
	 * function in this file already relies on).
	 *
	 * Separately, UFGResearchMachine/UFGResearchRecipe (FGResearchMachine.h/
	 * FGResearchRecipe.h) appear to be a distinct visual-feedback layer
	 * (mesh-scaling animation during analysis) that reacts to
	 * AFGResearchManager's real delegates - not itself a second research
	 * mechanism to drive. Not used here; flagged in case this assumption
	 * turns out wrong under live testing.
	 */
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FString LogMamStatusAsJson(UObject* WorldContextObject);

	/**
	 * world.startMamResearch (2026-08-29) - the real
	 * AFGResearchManager::InitiateResearch call is atomic (pays the FULL
	 * schematic cost from carried inventory and starts the research timer
	 * in one step) - unlike world.payMilestone's incremental partial-
	 * payment model, there is no real "submit some ingredients now, more
	 * later" mechanic for M.A.M. research to expose, confirmed from source
	 * (CanAffordResearch checks the whole cost at once; no analog to
	 * AFGSchematicManager's per-item FSchematicCost paid-off tracking
	 * exists on AFGResearchManager). This function reflects that reality:
	 * one call both "submits ingredients" and "activates research".
	 *
	 * InitiateResearch itself returns void (its own doc comment claims a
	 * bool return that the real signature doesn't have - stale text, not
	 * trusted) - so this function pre-validates with the real
	 * CanResearchBeInitiated/CanAffordResearch gates BEFORE calling it
	 * (surfacing CANNOT_RESEARCH/INSUFFICIENT_INGREDIENTS as clear
	 * failures with the real cost attached via ResultDetailJson), then
	 * verifies IsResearchBeingConducted(schematic) actually flipped true
	 * afterward - never trusts a void call blindly, same "verify after
	 * every write" discipline as everything else in this file.
	 *
	 * bDryRun runs only the pre-validation, touching nothing - same
	 * convention as world.payMilestone. SchematicClassPath and
	 * ResearchTreeClassPath are both required - InitiateResearch itself
	 * needs the initiating tree, there is no "current active tree" concept
	 * to default to the way world.payMilestone can default to
	 * GetActiveSchematic().
	 *
	 * NOT YET LIVE-TESTED.
	 */
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FAIModOperationResult StartMamResearch(UObject* WorldContextObject, const FString& SchematicClassPath, const FString& ResearchTreeClassPath, bool bDryRun);

	/**
	 * world.claimMamResearch (2026-08-29) - claims a completed research's
	 * results (AFGResearchManager::ClaimResearchResults). For a normal
	 * M.A.M. schematic this grants the real unlock immediately. For a hard
	 * drive analysis schematic (ESchematicType::EST_HardDrive), the real
	 * engine internally generates a new unclaimed hard drive with random
	 * alternate-recipe reward choices instead (ProcessCompletedHardDriveResearch)
	 * - see world.claimMamHardDriveReward for the follow-up step that
	 * actually picks one. Fails with NOT_COMPLETE if
	 * IsResearchComplete(schematic) is false rather than calling a void
	 * function that would silently no-op. Verifies afterward that
	 * IsResearchComplete(schematic) actually flipped false.
	 *
	 * NOT YET LIVE-TESTED.
	 */
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FAIModOperationResult ClaimMamResearch(UObject* WorldContextObject, const FString& SchematicClassPath);

	/**
	 * world.claimMamHardDriveReward (2026-08-29) - picks one alternate
	 * recipe from an unclaimed hard drive's randomly-rolled reward choices
	 * (UFGHardDrive::ClaimSchematic). Deliberately identified by
	 * RewardSchematicClassPath - one of the schematics currently offered
	 * by SOME unclaimed hard drive, found via
	 * AFGResearchManager::GetUnclaimedHardDrives + each UFGHardDrive's own
	 * public GetSchematics() - rather than by a numeric hard drive id.
	 *
	 * This is a deliberate design choice, not an oversight:
	 * AFGResearchManager::mUnclaimedHardDriveData's real HardDriveID field
	 * exists but has no public accessor, and UFGHardDrive::mHardDriveID is
	 * a plain private int with no UPROPERTY at all - genuinely
	 * unreflectable, unlike every other "read a protected/private field
	 * via FindFProperty" case elsewhere in this file. Since
	 * GetAvailableAlternateSchematics already excludes schematics already
	 * offered by another unclaimed hard drive from future rolls (confirmed
	 * from source), a given reward schematic class should only ever be
	 * offered by one unclaimed hard drive at a time - safe to use as the
	 * lookup key. Fails with REWARD_NOT_FOUND if no unclaimed hard drive
	 * currently offers it.
	 *
	 * NOT YET LIVE-TESTED.
	 */
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FAIModOperationResult ClaimMamHardDriveReward(UObject* WorldContextObject, const FString& RewardSchematicClassPath);

	/**
	 * world.rerollMamHardDrive (2026-08-29) - rerolls an unclaimed hard
	 * drive's reward choices (UFGHardDrive::Reroll). Same
	 * identify-by-current-reward-content lookup as
	 * ClaimMamHardDriveReward, for the same reason - pass any ONE of the
	 * schematics currently offered by the target hard drive. Fails with
	 * CANNOT_REROLL if UFGHardDrive::CanReroll() is false - the detail
	 * distinguishes "no rerolls left for this drive"
	 * (!HasReroll()) from "no alternate recipes currently available to
	 * reroll into" via ResultDetailJson. Since a reroll changes the reward
	 * schematic set, re-query world.mamStatus afterward to see the new
	 * choices rather than expecting them echoed back here.
	 *
	 * NOT YET LIVE-TESTED.
	 */
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FAIModOperationResult RerollMamHardDrive(UObject* WorldContextObject, const FString& AnyCurrentRewardSchematicClassPath);

	/**
	 * world.trainStations (2026-08-29) - lists every AFGTrainStationIdentifier
	 * via AFGRailroadSubsystem::GetAllTrainStations, each with its real
	 * station name and the underlying AFGBuildableRailroadStation's id (the
	 * same id world.buildables already uses for it). Needed because
	 * world.buildables alone doesn't expose the station's display name,
	 * and a timetable stop is identified by buildable id, not name - this
	 * is how a caller finds the right id to build a world.setTrainTimetable
	 * request without guessing from world.buildables' bare class/position
	 * data.
	 */
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FString LogTrainStationsAsJson(UObject* WorldContextObject);

	/**
	 * world.trains (2026-08-29) - lists every AFGTrain via
	 * AFGRailroadSubsystem::GetAllTrains: name, status (Parked/
	 * ManualDriving/SelfDriving/Derailed), self-driving enabled + its real
	 * ESelfDrivingLocomotiveError (NoPower/NoTimeTable/InvalidNextStop/
	 * NoPath/StationUnreachable/etc - the actual reason a train isn't
	 * moving, not just a boolean), docking state, and its FULL timetable
	 * (every AFGRailroadTimeTable stop's station id/name + real
	 * FTrainDockingRuleSet - DockingDefinition/DockForDuration/
	 * IsDurationAndRule/load+unload item filters).
	 *
	 * Trains are AActors, not AFGBuildable (same category as AFGVehicle) -
	 * `id` is GetPathName(), usable with world.setTrainTimetable/
	 * world.setTrainSelfDriving, NOT with world.buildables/
	 * world.deleteBuilding.
	 */
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FString LogTrainsAsJson(UObject* WorldContextObject);

	/**
	 * world.setTrainTimetable (2026-08-29) - configures a train's full
	 * timetable in one call: AFGRailroadTimeTable::SetStops(), a real,
	 * public, BlueprintCallable "replace everything" setter (not an
	 * incremental add/remove) - so this always REPLACES the whole stop
	 * list, matching that real semantics rather than layering a partial-
	 * update abstraction over it. Creates a new time table via
	 * AFGTrain::NewTimeTable() first if the train doesn't have one yet
	 * (AFGTrain::HasTimeTable() is false).
	 *
	 * Each stop resolves a station by buildable id (from world.buildables
	 * or world.trainStations) to its real AFGTrainStationIdentifier via
	 * AFGBuildableRailroadStation::GetStationIdentifier() - fails with
	 * TARGET_NOT_FOUND per-stop if the id isn't a real, currently-existing
	 * railroad station (checked BEFORE calling SetStops, so a bad stop id
	 * never partially commits a timetable).
	 *
	 * Verifies afterward that GetNumStops() matches the requested stop
	 * count - never trusts SetStops's own bool return blindly (same
	 * "verify after every write" discipline as everything else in this
	 * file).
	 *
	 * NOT YET LIVE-TESTED.
	 */
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FAIModOperationResult SetTrainTimetable(UObject* WorldContextObject, const FString& TrainId, const FString& StopsJson);

	/**
	 * world.setTrainSelfDriving (2026-08-29) - enables/disables a train's
	 * autopilot (AFGTrain::SetSelfDrivingEnabled). Does NOT fail if the
	 * train reports a self-driving error afterward (e.g. no time table, no
	 * path, station unreachable) - that's real, informative train
	 * configuration state for the caller to see via
	 * result.detail.selfDrivingError, not a failure of this RPC call
	 * itself (the engine's own SetSelfDrivingEnabled has no failure return
	 * to check against). Only fails if IsSelfDrivingEnabled() doesn't match
	 * the requested value at all afterward.
	 *
	 * NOT YET LIVE-TESTED.
	 */
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FAIModOperationResult SetTrainSelfDriving(UObject* WorldContextObject, const FString& TrainId, bool bEnabled);

	/**
	 * world.droneStations (2026-08-29) - lists every drone station via
	 * AFGDroneSubsystem::GetAllStations(): id (the underlying
	 * AFGBuildableDroneStation's buildable id), pairedStationId (its
	 * single paired partner, if any - drone pairing is a mutual 1:1 link,
	 * not a directional source/destination pair, confirmed from source:
	 * AFGDroneStationInfo::mPairedStation is a single pointer, not a list;
	 * cargo flows both directions between paired stations), real drone
	 * status (NoDrone/Docked/Loading/Takeoff/EnRoute/Docking/Unloading/
	 * NotEnoughFuel/CannotUnload), active/allowed fuel types, latest+
	 * average round-trip/item-rate statistics, and the station's real
	 * input/output/fuel inventories.
	 */
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FString LogDroneStationsAsJson(UObject* WorldContextObject);

	/**
	 * world.pairDroneStations (2026-08-29) - pairs (or unpairs) two drone
	 * stations, i.e. sets which station a drone route connects - this is
	 * the RPC that answers "configure source and destination for a drone."
	 * Real mechanism confirmed from source:
	 * AFGDroneStationInfo::PairStation(otherStation) - a single mutual
	 * link per station (not a directional route list), matching
	 * world.droneStations' pairedStationId shape above.
	 *
	 * TargetStationBuildableId empty/omitted unpairs StationBuildableId
	 * instead (PairStation(nullptr) - OnPairedStationUpdate's own doc
	 * comment confirms "newStation can be nullptr"). Verifies afterward
	 * that GetPairedStation() matches the requested target (or is null,
	 * for an unpair) - never trusts the call blindly.
	 *
	 * NOT YET LIVE-TESTED.
	 */
	UFUNCTION(BlueprintCallable, Category = "AIMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FAIModOperationResult PairDroneStations(UObject* WorldContextObject, const FString& StationBuildableId, const FString& TargetStationBuildableId);
};
