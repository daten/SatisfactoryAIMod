// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAIModAI, Log, All);

struct FActorsInitializedParams;
struct IConsoleCommand;

class FAIModModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
#if !UE_BUILD_SHIPPING
	/**
	 * Two delegates feed RunPerWorldSetup: FWorldDelegates::OnWorldInitializedActors
	 * alone was found, live (2026-08-24), to not fire for every world-load
	 * path - a save-load via ProcessServerTravel produced no self-test/
	 * hotkey output at all, while a fresh "New Game" load worked fine in
	 * an earlier session. FCoreUObjectDelegates::PostLoadMapWithWorld
	 * (fires after LoadMap completes) covers the gap. Both can fire for
	 * the same world, so RunPerWorldSetup de-duplicates via LastSetupWorld.
	 */
	void OnWorldInitializedActors(const FActorsInitializedParams& Params);
	void OnPostLoadMapWithWorld(UWorld* World);
	void RunPerWorldSetup(UWorld* World);

	FDelegateHandle WorldInitializedActorsHandle;
	FDelegateHandle PostLoadMapWithWorldHandle;
	TWeakObjectPtr<UWorld> LastSetupWorld;
#endif

	/**
	 * Console command fallback for triggering AIMod telemetry/self-test
	 * reporting (see docs/chat-and-console-commands.md) - unlike the
	 * chat command (AIModChatCommand.h), needs no Editor/Content
	 * wiring, works from the in-game `~` console, -ExecCmds=, or the
	 * Editor's Output Log console input immediately. Available in all
	 * configurations, not just non-Shipping - only runs when explicitly
	 * invoked by someone typing a command, unlike the automatic
	 * self-test hook above.
	 */
	void RegisterConsoleCommands();
	void UnregisterConsoleCommands();

	TArray<IConsoleCommand*> ConsoleCommands;
};
