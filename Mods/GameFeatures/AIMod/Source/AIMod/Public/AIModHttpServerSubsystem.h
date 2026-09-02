// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HttpRequestHandler.h"
#include "HttpRouteHandle.h"
#include "AIModHttpServerSubsystem.generated.h"

class IHttpRouter;
struct FHttpServerRequest;
struct FActorsInitializedParams;

/**
 * Localhost-only JSON RPC transport for the AIMod AI interface.
 *
 * Full RPC method reference lives in RPC_REFERENCE.md at the repo root,
 * not here. This comment used to enumerate every method inline and grew
 * past a UHT/MSVC string length limit, breaking the build entirely.
 * Add new methods to RPC_REFERENCE.md and the relevant function doc
 * comment in AIModFunctionLibrary, not to this class comment.
 *
 * Binds to 127.0.0.1 only. Enforced two ways: the per-port
 * ListenerOverrides entry in Config/DefaultEngine.ini, and defense in
 * depth in HandleRpcRequest, which rejects any request whose PeerAddress
 * is not loopback regardless of what the socket is bound to (the ini
 * override does not propagate to a separately Alpakit-deployed package).
 *
 * Single POST /rpc endpoint, dispatching by method name. Per CLAUDE.md
 * Safety and Stability Boundary rules, this class must never grow a
 * generic call-any-function method - every method is explicit and
 * narrowly scoped, delegating to a real UAIModFunctionLibrary function.
 *
 * Most Construct*-backed write methods are genuinely asynchronous (real-
 * tick polling before the completion callback fires); most reads and
 * simple deletes/moves are synchronous. See RPC_REFERENCE.md per method.
 */
UCLASS()
class AIMOD_API UAIModHttpServerSubsystem : public UGameInstanceSubsystem
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
	 * world.batch (2026-09-02, docs/build-efficiency-plan.md 2b): runs an
	 * ordered list of sub-operations SEQUENTIALLY - each sub-op is a
	 * normal {method, params} pair dispatched back through
	 * HandleRpcRequest with a synthesized request that keeps the parent's
	 * PeerAddress (so the loopback/remote policy applies identically -
	 * batch is not a bypass). Asynchronous construction sub-ops chain
	 * through their completion callbacks, so a batch of belts/placements
	 * costs ONE HTTP round trip instead of one per op. The parent
	 * response reports every sub-result in order. Nested world.batch is
	 * rejected. See RunBatchStep in the .cpp.
	 */
	void RunBatchStep(TSharedRef<struct FAIModBatchState> State, FHttpServerRequest BaseRequest, FHttpResultCallback ParentComplete, FString ParentRequestId);

	/**
	 * Instant chat acknowledgment (2026-08-28, per explicit user request):
	 * binds to AFGChatManager::OnChatMessageAdded so a real player-typed
	 * chat message gets an immediate "seen" reply, independent of - and
	 * much faster than - any external polling loop (an external agent
	 * watching via world.chatHistory has an inherent latency floor of at
	 * least tens of seconds per poll; this is event-driven, fires the
	 * same tick the message is added). Deliberately does NOT attempt to
	 * answer the request itself - AIMod has no embedded LLM/decision
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

	/**
	 * Duplicate-submission guard (2026-08-28) - see
	 * HandlePlayerChatMessageAdded's doc comment. Confirmed live that the
	 * game's own chat system can submit the same literal player message
	 * dozens of times for a single keystroke; suppresses re-acking the
	 * same text within half a second, without touching the upstream
	 * cause (which isn't in this file).
	 */
	FString LastAckedMessageText;
	double LastAckedMessageTime = -1.0;

	TSharedPtr<IHttpRouter> Router;
	FHttpRouteHandle RpcRouteHandle;
};
