// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Configuration/ModConfiguration.h"
#include "AIModConfiguration.generated.h"

/**
 * AIMod's player-facing mod settings, added 2026-08-27 per explicit user
 * request: three safety/capability trade-offs that were previously either
 * hardcoded (loopback-only networking, real material cost enforcement) or
 * entirely absent (no distance-from-player limit ever existed for
 * RPC-driven placement, unlike a real player's Build Gun). These are
 * deliberately PLAYER-controlled master switches, not per-RPC-call opt-in
 * flags like ConstructBuildingAtPosition's ignoreClearance/ignoreAimLocation
 * params - an external AI controller cannot enable any of these by asking
 * for them in a request; only the player, from this mod's settings menu
 * (SML's UConfigManager::CreateConfigurationWidget), can.
 *
 * All three default to OFF/false - i.e. today's existing, already-shipped
 * behavior is preserved unless the player deliberately opts in:
 *  - bAllowRemoteConnections: today's default is loopback-only (CLAUDE.md's
 *    "bind only to loopback by default... design the transport so remote
 *    access is not accidentally enabled" - this makes the opt-in explicit
 *    and player-controlled instead of requiring a config file edit).
 *  - bUnlimitedResources: today, RPC-driven construction already enforces
 *    real material cost (UFGCDUnaffordable) exactly like a real player -
 *    confirmed live this session ("Missing materials!" on a genuine
 *    Fracking Smasher placement). This is a new bypass, off by default.
 *  - bLimitBuildDistance + MaxBuildDistance: the INVERSE of the other two -
 *    today NO distance-from-player limit exists at all for RPC placement
 *    (confirmed by grepping FGConstructDisqualifier.h - there is no
 *    "too far from player" disqualifier class; AIMod has built at
 *    100,000+ unit distances all session with nothing ever rejecting it).
 *    Enabling this ADDS a restriction that doesn't exist in the base game
 *    or in AIMod's current behavior, per explicit user request ("simulate
 *    a build distance limit... so structures cannot be built clear on the
 *    other side of the map"), so it defaults OFF to preserve today's
 *    unrestricted behavior for existing remote-testing workflows.
 *
 * A fifth property, added 2026-08-28, is a different character entirely -
 * bAutoAcknowledgeChatMessages is a pure UX nicety (an instant "seen" chat
 * reply, see UAIModHttpServerSubsystem::HandlePlayerChatMessageAdded), not
 * a safety/capability trade-off, and defaults ON rather than off.
 *
 * A sixth property, also added 2026-08-28, is back to the off-by-default,
 * player-opt-in-only character of the original four - bAllowCreatureSpawning
 * gates UAIModFunctionLibrary::SpawnCreatureNearPlayer. There's no existing
 * in-game equivalent of a player spawning a creature, so this is treated
 * like bUnlimitedResources: an external AI controller can never enable it
 * itself, only the player from this settings menu.
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
	// reaches a handler and edits are silently never written to disk
	// (the 2026-09-01 "config changes don't persist across sessions" bug).
	UAIModConfiguration(const FObjectInitializer& ObjectInitializer);
};
