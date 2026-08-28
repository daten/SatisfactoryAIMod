// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HttpRequestHandler.h"
#include "HttpRouteHandle.h"
#include "DocModHttpServerSubsystem.generated.h"

class IHttpRouter;
struct FHttpServerRequest;
struct FActorsInitializedParams;

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
 * "world.pipeConnections" (same shape as "world.connections" but for
 * UFGPipeConnectionComponentBase - fluid pipes AND hypertubes, see
 * FDocModPipeConnectionTelemetry - added 2026-08-27, "world.connections"
 * never covered these),
 * "world.conveyorBeltTiers" (each of Recipe_ConveyorBeltMk1..Mk6's real
 * queried AFGBuildableConveyorBase::GetSpeed() - see
 * LogConveyorBeltTiersAsJson's doc comment on why this is NOT a hardcoded
 * items-per-minute table), "world.powerLineLimits" (Recipe_PowerLine's
 * real mMaxLength/mMaxPowerTowerLength/mLengthPerCost, in cm - see
 * LogPowerLineLimitsAsJson's doc comment), "world.pipelineTiers"
 * (Recipe_Pipeline/Recipe_PipelineMK2's real queried
 * flowLimit/maxSplineLength/bendRadius/minBendRadius - pipe groundwork,
 * 2026-08-25, see LogPipelineTiersAsJson's doc comment),
 * "world.conveyorAttachments" (the real Splitter/Merger/Priority
 * Merger/Smart Splitter/Programmable Splitter recipe catalog with each
 * variant's real inputCount/outputCount/supportsSortRules - added
 * 2026-08-25, see LogConveyorAttachmentCatalogAsJson's doc comment;
 * placing/connecting these needs NO new write method - existing
 * "world.placeBuilding"/"world.connectConveyor" already handle them
 * generically), "world.conveyorLiftTiers" (Recipe_ConveyorLiftMk1..Mk6's
 * real queried speed - vertical conveyor groundwork, 2026-08-25, per
 * explicit user request, see LogConveyorLiftTiersAsJson's doc comment
 * on why min/max height limits are NOT reported - a real gap, not an
 * omission), "world.targetedManufacturer"
 * (whatever manufacturer the local player
 * is currently looking at - "manufacturer":null if none),
 * "world.recipeCatalog"/"world.itemCatalog"/"world.buildableCatalog"
 * (2026-08-27, per explicit user request to support pre-planning complex
 * builds - the full static game database: every recipe/alternate
 * including not-yet-unlocked ones, every item descriptor with its form/
 * stack size/energy value, every building with its construction cost,
 * power consumption/production, clock-speed range, and factory/pipe/
 * power connection counts - see LogRecipeCatalogAsJson's doc comment for
 * the full field list and an IMPORTANT caveat: the backing
 * AFGRecipeManager::Get() is stub-source in Editor/PIE and only resolves
 * in the packaged/Alpakit-deployed game). Write methods
 * (PLAN.md Phase 12, take a "params" object): "world.setClockSpeed"
 * ({"buildableId","clockSpeedPercent"}), "world.setRecipe"
 * ({"buildableId","recipeClass"}) - both delegate all validation to
 * UDocModFunctionLibrary's SetManufacturer* functions and only translate
 * FDocModOperationResult into the HTTP/JSON response shape here. This
 * class must not grow a generic "call any function by name" method
 * (CLAUDE.md's Safety and Stability Boundary).
 *
 * "world.deleteBuilding" ({"buildableId"}) is SYNCHRONOUS (no build
 * gun/hologram involved, unlike construction) - delegates to
 * UDocModFunctionLibrary::DismantleBuildable, the real
 * IFGDismantleInterface flow, not AActor::Destroy(). Added 2026-08-25
 * so live testing can clean up stray buildables between attempts. Fixed
 * 2026-08-27 to also dismantle any IFGDismantleInterface::
 * GetChildDismantleActors() children first (e.g. a pipe's separate
 * AFGBuildablePipelineFlowIndicator actor, previously left floating in
 * place after the pipe itself was removed - see DismantleBuildable's
 * doc comment).
 *
 * "world.timeOfDay" (read-only, no params) reports the current
 * {"hour","minute","daySeconds","isDay"} via AFGTimeOfDaySubsystem.
 * "world.setTimeOfDay" ({"hour","minute"}, minute optional/default 0) is
 * SYNCHRONOUS - added 2026-08-27 per explicit user request so the
 * day/night cycle going dark doesn't block live visual observation. See
 * SetTimeOfDay's doc comment for why this calls
 * AFGTimeOfDaySubsystem::SetDaySeconds() directly rather than going
 * through UFGCheatManager.
 *
 * Player-controlled mod settings (DocModConfiguration.h, registered here
 * in Initialize() via UConfigManager::RegisterModConfiguration) gate three
 * safety/capability trade-offs, added 2026-08-27 per explicit user
 * request - all default off, preserving prior behavior unless the player
 * opts in from DocMod's entry in SML's normal mod settings menu: "Allow
 * Remote Connections" (consulted right here, alongside the existing
 * defense-in-depth IsLoopbackPeer() check - an RPC caller cannot request
 * this itself), "Unlimited Resources for RPC Builds" (bypasses
 * UFGCDUnaffordable in every Construct* function's disqualifier-ignore
 * logic - see UDocModFunctionLibrary::GetDocModConfigBool's doc comment),
 * and "Limit RPC Build Distance From Player" + "Max Build Distance" (a
 * brand-new synthetic restriction - no such disqualifier exists natively;
 * checked early in ConstructBuildingAtPosition/ConstructExtractorOnNode,
 * default 8000 units ~= 10 foundation tiles).
 *
 * "world.groundHeight" ({"x","y"}, optional "z" anchor) reports the real
 * ground trace at that X/Y - {"found","x","y","z","normal"} - added
 * 2026-08-27 per explicit user request to make placement Z deterministic
 * without the caller needing to already know "z" is a search center, not
 * a literal height (see LogGroundHeightAsJson's doc comment): query the
 * real height first, then pass it straight back in as world.placeBuilding's
 * "z". "world.placeBuilding" also gained an optional "faceBuildableId"
 * (see ConstructBuildingAtPosition's doc comment) - resolves an existing
 * buildable's real position and computes yaw automatically, instead of
 * the caller fetching it and doing the vector math themselves.
 *
 * "world.cleanupOrphanedFlowIndicators" (no params) deletes every
 * AFGBuildablePipelineFlowIndicator not currently returned by any live
 * AFGBuildablePipeline's own GetFlowIndicator() - exact, not
 * proximity-based (see CleanupOrphanedFlowIndicatorsAsJson's doc
 * comment for why proximity guessing was rejected). Added 2026-08-27 to
 * clean up debris from pipes deleted before this session's
 * DismantleBuildable child-actor-cleanup fix existed.
 *
 * "world.portableMiners" (read-only, no params) lists every AFGPortableMiner
 * with its position/node/output inventory - "world.placePortableMiner"
 * ({"nodeId","itemClass"}, itemClass optional) equips+places one (real
 * equipment-based flow, NOT the buildable/hologram path every other
 * construction method uses - see ConstructPortableMinerOnNode's doc
 * comment) and "world.retrievePortableMinerInventory"
 * ({"portableMinerId"}, SYNCHRONOUS) empties one into the player's own
 * inventory - all three added 2026-08-27 per explicit user request.
 *
 * "world.spawnCreature" ({"creatureClass","distanceFromPlayer"?,"scale"?},
 * SYNCHRONOUS - see SpawnCreatureNearPlayer, added 2026-08-28) spawns a
 * real AFGCreature near the player via AFGCreatureSubsystem's own
 * BeginSpawningCreature. Off by default - gated behind the
 * "AllowCreatureSpawning" mod setting, the only capability besides
 * bUnlimitedResources that an external AI controller can never enable
 * itself, only the player from DocMod's settings menu. "scale" is an
 * optional uniform scale factor (default 1.0, clamped to [0.05, 20.0]).
 * "world.despawnCreature" ({"creatureId"}, SYNCHRONOUS) removes a
 * creature by its GetPathName() id.
 *
 * Instant chat acknowledgment (2026-08-28, per explicit user request):
 * independent of "world.chatHistory"/"world.sendChatMessage" below, this
 * class also binds directly to AFGChatManager::OnChatMessageAdded
 * (TryBindChatManagerDelegate/HandlePlayerChatMessageAdded) so a real
 * player-typed chat message gets an immediate "seen" reply - event-driven,
 * not subject to an external polling loop's latency floor. Gated by the
 * "Auto-Acknowledge Chat Messages" mod setting (default ON, unlike the
 * four safety/capability toggles above).
 *
 * Fixed 2026-08-28 (same day): confirmed live that binding only once, in
 * Initialize(), produces a burst of dozens of duplicate acks the moment a
 * save actually loads into a real game world, then goes completely silent
 * for genuinely new messages after that. AFGChatManager is a world/actor-
 * based subsystem (see TryBindChatManagerDelegate's own doc comment) -
 * GameInstanceSubsystem::Initialize() runs long before the player's save
 * finishes loading into its real world (often at the main menu), so the
 * original binding was against a ChatManager/message-count snapshot from
 * before the real session existed. Now also rebinds on every real game
 * world's initialization, mirroring FDocModModule::RunPerWorldSetup's own
 * established two-delegate pattern (OnWorldInitializedActors +
 * PostLoadMapWithWorld, de-duplicated via LastRebindWorld) for exactly the
 * same reason documented there: a save-load via ProcessServerTravel doesn't
 * reliably fire both.
 *
 * "world.chatHistory" (read-only, no params) reports
 * {"messages":[{"sender","text","type","timestamp","isLocalPlayerMessage"},...]}
 * via AFGChatManager::GetReceivedChatMessages() - genuinely two-way
 * without extra plumbing, since a message the player types normally
 * (not "/"-prefixed - those go to chat command dispatch instead) lands
 * in this same array through the ordinary game chat pipeline. See
 * LogChatHistoryAsJson's doc comment. "world.sendChatMessage"
 * ({"message","sender"}, sender optional/default "DocMod AI") is
 * SYNCHRONOUS - both added 2026-08-27 per explicit user request for
 * optional two-way chat between the AI controller and the player.
 *
 * "world.placeBuilding" ({"recipeClass","x","y"}, plus optional
 * "rotationScrollDelta" (default 0), "gridSnapSize" (default 100,
 * i.e. snap-to-1m-grid by default - pass 0 to disable), "z"
 * (anchors the ground-trace search to this Z instead of the player's
 * current Z - recommended for any multi-step layout), and
 * "ignoreAimLocation"/"ignorePlayerEncroachment"/"ignoreClearance"/
 * "ignoreInvalidFloor" (all default false - named, scoped bypasses of
 * specific UX-only placement gates for large autonomous layouts,
 * accepting collision risk in exchange; see
 * ConstructBuildingAtPosition's doc comment for all of these)), "world.placeExtractor"
 * ({"nodeId"}), "world.testPowerConnection" (dry run, never touches the
 * save) and "world.connectPower" (real - both
 * {"buildableIdA","buildableIdB"}, plus optional "ignoreAimLocation" and
 * "ignoreWireSnap" (both default false, added 2026-08-25 - named bypasses
 * for disqualifiers live-diagnosed as camera/aim-state flakiness rather
 * than real geometry failures, see ConstructPowerConnection's doc comment;
 * NOT YET LIVE-VERIFIED to resolve it)), "world.testConveyorBelt" (dry run) and
 * "world.connectConveyor" (real - both
 * {"sourceBuildableId","destBuildableId"}, plus optional "recipeClass"
 * (default Recipe_ConveyorBeltMk1 - any of Mk1..Mk6, see
 * "world.conveyorBeltTiers" above to pick by real queried speed) and
 * optional "routeMode" (one of "Straight"/"Curve"/"Auto", default
 * empty = hologram's own default mode - added 2026-08-25 to bypass the
 * 2-click mechanism's confirmed inability to bend for mismatched
 * connectors, see ConstructConveyorBelt's doc comment; NOT YET
 * LIVE-VERIFIED to resolve it)),
 * "world.testConveyorLift" (dry run) and "world.connectConveyorLift"
 * (real - both {"sourceBuildableId","destBuildableId"}, plus optional
 * "recipeClass" (default Recipe_ConveyorLiftMk1 - any of Mk1..Mk6, see
 * "world.conveyorLiftTiers" above)) - vertical conveyor groundwork,
 * 2026-08-25, per explicit user request, NOT YET LIVE-TESTED, see
 * ConstructConveyorLift's doc comment,
 * "world.testPipe" (dry run) and "world.connectPipe" (real - both
 * {"sourceBuildableId","destBuildableId"}, plus optional "recipeClass"
 * (default Recipe_Pipeline - or Recipe_PipelineMK2, see
 * "world.pipelineTiers" above) - live-tested 2026-08-27 over a real
 * ~4000-unit run once given the same deterministic-look player-
 * independence fix as ConstructConveyorBelt (it predated that fix and
 * failed with "Invalid aim location!" the same way belts used to),
 * "world.testHypertube" (dry run) and "world.connectHypertube" (real -
 * both {"sourceBuildableId","destBuildableId"}, no recipeClass -
 * Recipe_PipeHyper is the only tube recipe) - hypertube tube-segment
 * construction, 2026-08-27, see ConstructHypertube's doc comment)
 * (PLAN.md Phase 13/14) are
 * GENUINELY ASYNCHRONOUS methods - UDocModFunctionLibrary::ConstructBuildingAtPosition/
 * ConstructExtractorOnNode/ConstructPowerConnection/ConstructConveyorBelt/ConstructConveyorLift/ConstructPipe/ConstructHypertube's completion callback may fire well after
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

	/**
	 * Instant chat acknowledgment (2026-08-28, per explicit user request):
	 * binds to AFGChatManager::OnChatMessageAdded so a real player-typed
	 * chat message gets an immediate "seen" reply, independent of - and
	 * much faster than - any external polling loop (an external agent
	 * watching via world.chatHistory has an inherent latency floor of at
	 * least tens of seconds per poll; this is event-driven, fires the
	 * same tick the message is added). Deliberately does NOT attempt to
	 * answer the request itself - DocMod has no embedded LLM/decision
	 * logic (see CLAUDE.md's "Keep the Unreal Mod Small" - this stays
	 * exactly the same class of thing as everything else in this file,
	 * a narrow, explicit reaction, not a planner). AFGChatManager is a
	 * world/actor-based subsystem (unlike UConfigManager, a
	 * GameInstanceSubsystem) and may not exist yet when this
	 * GameInstanceSubsystem's own Initialize() runs, since GameInstance
	 * subsystems can initialize before any level/world subsystems spawn -
	 * TryBindChatManagerDelegate retries on a short repeating timer until
	 * it succeeds, then stops.
	 */
	void TryBindChatManagerDelegate();

	/** Re-triggers TryBindChatManagerDelegate on real game world init - see this class's header doc comment ("Fixed 2026-08-28"). */
	void OnWorldInitializedActorsForChat(const FActorsInitializedParams& Params);
	void OnPostLoadMapWithWorldForChat(UWorld* World);
	void RebindChatManagerForWorld(UWorld* World);

	UFUNCTION()
	void HandlePlayerChatMessageAdded();

	FTimerHandle ChatManagerBindRetryTimer;
	FDelegateHandle ChatWorldInitializedActorsHandle;
	FDelegateHandle ChatPostLoadMapWithWorldHandle;
	TWeakObjectPtr<UWorld> LastRebindWorld;

	/**
	 * Index into AFGChatManager::GetReceivedChatMessages() of the last
	 * message this subsystem has already reacted to (acked or not) -
	 * prevents re-acking the same message if OnChatMessageAdded fires
	 * more than once for it, and is what keeps the ack's own message
	 * (a CustomMessage, not a PlayerMessage) from being mistaken for a
	 * new player message on the delegate's own re-fire.
	 */
	int32 LastSeenChatMessageCount = 0;

	TSharedPtr<IHttpRouter> Router;
	FHttpRouteHandle RpcRouteHandle;
};
