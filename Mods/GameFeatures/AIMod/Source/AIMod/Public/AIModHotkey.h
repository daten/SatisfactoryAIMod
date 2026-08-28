// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Configurable hotkey (default F6 - see UAIModDeveloperSettings::HotkeyKey,
 * Project Settings > Plugins > AIMod, or Config/DefaultAIMod.ini):
 * prints the manufacturer the local player is currently looking at (via
 * GetTargetedManufacturer, same as AIMod.Target / /aimod target) and,
 * if one is targeted, attempts a real clock-speed change as an
 * end-to-end test of the write-operation path (PLAN.md Phase 12) -
 * nudges clock speed up by 10 percentage points and reports
 * success/failure via LogAIModAI AND an in-game chat message (through
 * SML's USMLRemoteCallObject::SendChatMessage - the same underlying call
 * SML's own chat commands use, reached directly since there's no
 * UCommandSender for a keypress).
 *
 * Default is F6, not F11 - F11 was tried first and found (in actual
 * play) to collide with the engine's own bF11TogglesFullscreen setting
 * (Config/DefaultInput.ini), which intercepts the key before Enhanced
 * Input gameplay bindings ever see it. Made configurable specifically
 * because of that collision, in case F6 turns out to collide with
 * something else too.
 *
 * THIS PERFORMS A REAL MUTATION, not a dry run - only bind this while
 * testing against a disposable save (see docs/manual-verification.md).
 * Registration is wrapped in #if !UE_BUILD_SHIPPING at the call site in
 * AIMod.cpp for that reason - it should never fire for a real player of
 * a released mod pressing the hotkey by accident.
 *
 * Uses AFGPlayerController::AddMappingContextImmediately() (a real,
 * public FactoryGame function - FGPlayerController.h) with a
 * UInputMappingContext/UInputAction pair built entirely at runtime via
 * NewObject<>() - no Content/Blueprint asset needed, unlike the chat
 * command (see docs/chat-and-console-commands.md's Editor-wiring note).
 */
namespace AIModHotkey
{
	/**
	 * Binds the hotkey for the local player (index 0) in the given world.
	 * Meant to be called once per world load (see AIMod.cpp's
	 * OnWorldInitializedActors) - calling it again for the same, still-live
	 * PlayerController would add a duplicate binding. No-op (with a
	 * warning logged) if no local player controller or Enhanced Input
	 * component exists yet.
	 */
	void SetupForWorld(UWorld* World);
}
