// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogDocModAI, Log, All);

struct FActorsInitializedParams;
struct IConsoleCommand;

class FDocModModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
#if !UE_BUILD_SHIPPING
	/** Runs DocModSelfTest::RunAll for every loaded game world. See DocModSelfTest.h. */
	void OnWorldInitializedActors(const FActorsInitializedParams& Params);

	FDelegateHandle WorldInitializedActorsHandle;
#endif

	/**
	 * Console command fallback for triggering DocMod telemetry/self-test
	 * reporting (see docs/chat-and-console-commands.md) - unlike the
	 * chat command (DocModChatCommand.h), needs no Editor/Content
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
