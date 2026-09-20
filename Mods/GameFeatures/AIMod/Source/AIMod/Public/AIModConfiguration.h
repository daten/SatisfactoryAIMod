// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Configuration/ModConfiguration.h"
#include "AIModConfiguration.generated.h"

/**
 * AIMod's player-facing mod settings: safety/capability trade-offs that are
 * otherwise either hardcoded (loopback-only networking, real material cost
 * enforcement) or entirely absent (no distance-from-player limit exists for
 * RPC-driven placement, unlike a real player's Build Gun). These are
 * deliberately PLAYER-controlled master switches, not per-RPC-call opt-in
 * flags like ConstructBuildingAtPosition's ignoreClearance/ignoreAimLocation
 * params - an external AI controller cannot enable any of these by asking
 * for them in a request; only the player, from this mod's settings menu
 * (SML's UConfigManager::CreateConfigurationWidget), can.
 *
 * The safety/capability toggles default to OFF/false, so existing
 * already-shipped behavior is preserved unless the player deliberately opts in:
 *  - bAllowRemoteConnections: default is loopback-only (CLAUDE.md's
 *    "bind only to loopback by default... design the transport so remote
 *    access is not accidentally enabled" - this makes the opt-in explicit
 *    and player-controlled instead of requiring a config file edit).
 *  - bUnlimitedResources: by default RPC-driven construction enforces real
 *    material cost (UFGCDUnaffordable) exactly like a real player. This is a
 *    bypass, off by default.
 *  - bLimitBuildDistance + MaxBuildDistance: the INVERSE of the other two -
 *    no distance-from-player limit exists at all for RPC placement (there is
 *    no "too far from player" disqualifier class in FGConstructDisqualifier.h;
 *    AIMod can build at 100,000+ unit distances with nothing rejecting it).
 *    Enabling this ADDS a restriction that doesn't exist in the base game or
 *    in AIMod's default behavior, so it defaults OFF to preserve unrestricted
 *    behavior for remote-testing workflows.
 *
 * bAutoAcknowledgeChatMessages is a different character - a pure UX nicety
 * (an instant "seen" chat reply, see
 * UAIModHttpServerSubsystem::HandlePlayerChatMessageAdded), not a
 * safety/capability trade-off, and defaults ON rather than off.
 *
 * bAllowCreatureSpawning is back to the off-by-default, player-opt-in-only
 * character - it gates UAIModFunctionLibrary::SpawnCreatureNearPlayer. There's
 * no existing in-game equivalent of a player spawning a creature, so this is
 * treated like bUnlimitedResources: an external AI controller can never enable
 * it itself, only the player from this settings menu.
 *
 * AllowNonHostChatMessages is multiplayer chat safety (off by default). With
 * it off, only the HOST player's chat messages reach the external AI through
 * world.chatHistory, and only the host gets the instant auto-acknowledgment -
 * guests in a multiplayer session cannot direct the AI through chat. The
 * discriminator is the game's own FChatMessageStruct::bIsLocalPlayerMessage
 * evaluated in the host process. See UAIModFunctionLibrary::LogChatHistoryAsJson
 * and UAIModHttpServerSubsystem::HandlePlayerChatMessageAdded.
 *
 * See AIModFunctionLibrary::GetAIModConfigBool/GetAIModConfigFloat for
 * how these are read at construction time, and
 * UAIModHttpServerSubsystem::Initialize for registration and where
 * bAllowRemoteConnections is consulted.
 */
UCLASS()
class AIMOD_API UAIModConfiguration : public UModConfiguration
{
	GENERATED_BODY()

public:
	// Takes an explicit FObjectInitializer so the config PROPERTIES can be
	// created as subobjects of the RootSection (not of this configuration
	// object). That parenting is load-bearing for persistence: SML's
	// UConfigProperty::MarkDirty() walks a changed property's Outer chain
	// for the nearest IConfigValueDirtyHandlerInterface (the section /
	// URootConfigValueHolder) to trigger a save - if the properties are
	// subobjects of the configuration object instead, that walk never
	// reaches a handler and edits are silently never written to disk.
	UAIModConfiguration(const FObjectInitializer& ObjectInitializer);
};
