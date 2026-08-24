// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Command/ChatCommandInstance.h"
#include "DocModChatCommand.generated.h"

/**
 * In-game chat command for the DocMod AI interface: `/docmod <subcommand>`
 * (SML's chat command framework - see
 * Mods/SML/Source/SML/Public/Command/ChatCommandInstance.h). Lets a
 * player trigger DocMod telemetry/self-test reporting directly from the
 * game's chat UI, without Blueprint editing or an HTTP call. See
 * docs/self-test.md and docs/telemetry-protocol.md for what each
 * subcommand reports.
 *
 * Read-only: no subcommand here mutates game state. The write operations
 * (SetManufacturerClockSpeed/SetManufacturerRecipe) are deliberately NOT
 * exposed via chat command - a mistyped buildable id/recipe path in chat
 * is an easy way to trigger CLAUDE.md's validation rejections by
 * accident, and per CLAUDE.md's Safety and Stability Boundary that
 * should be a considered addition on its own, not tacked onto a
 * read-only reporting command.
 *
 * NOT auto-registered - unlike DocModSelfTest's automatic
 * OnWorldInitializedActors hook, SML's chat commands are registered
 * data-driven, via a UGameWorldModule Blueprint asset's mChatCommands
 * array (see Mods/SML/Source/SML/Public/Module/GameWorldModule.h). See
 * docs/chat-and-console-commands.md for the one remaining Editor step
 * needed to wire this up.
 */
UCLASS()
class DOCMOD_API ADocModChatCommand : public AChatCommandInstance
{
	GENERATED_BODY()

public:
	ADocModChatCommand();

	virtual EExecutionStatus ExecuteCommand_Implementation(UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label) override;
};
