// Copyright Epic Games, Inc. All Rights Reserved.

#include "AIModChatCommand.h"
#include "AIMod.h"
#include "AIModFunctionLibrary.h"
#include "AIModSelfTest.h"
#include "Command/CommandSender.h"

AAIModChatCommand::AAIModChatCommand()
{
	CommandName = TEXT("aimod");
	Aliases = {TEXT("dm")};
	Usage = FText::FromString(TEXT("aimod <selftest|resourcenodes|buildables|manufacturers|connections|target>"));
	MinNumberOfArguments = 1;
	bOnlyUsableByPlayer = false;
}

EExecutionStatus AAIModChatCommand::ExecuteCommand_Implementation(UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label)
{
	if (!Sender || Arguments.Num() < 1)
	{
		return EExecutionStatus::BAD_ARGUMENTS;
	}

	const FString Subcommand = Arguments[0].ToLower();

	if (Subcommand == TEXT("selftest"))
	{
		Sender->SendChatMessage(TEXT("Running AIMod self-test..."));
		AIModSelfTest::RunAll(GetWorld());
		Sender->SendChatMessage(TEXT("Self-test complete - see LogAIModAI in the log for full PASS/FAIL detail."));
		return EExecutionStatus::COMPLETED;
	}

	if (Subcommand == TEXT("resourcenodes"))
	{
		const int32 Count = UAIModFunctionLibrary::GetResourceNodeTelemetry(GetWorld()).Num();
		Sender->SendChatMessage(FString::Printf(TEXT("%d resource node(s) found"), Count));
		return EExecutionStatus::COMPLETED;
	}

	if (Subcommand == TEXT("buildables"))
	{
		const int32 Count = UAIModFunctionLibrary::GetBuildableTelemetry(GetWorld()).Num();
		Sender->SendChatMessage(FString::Printf(TEXT("%d buildable(s) found"), Count));
		return EExecutionStatus::COMPLETED;
	}

	if (Subcommand == TEXT("manufacturers"))
	{
		const int32 Count = UAIModFunctionLibrary::GetManufacturerTelemetry(GetWorld()).Num();
		Sender->SendChatMessage(FString::Printf(TEXT("%d manufacturer(s) found"), Count));
		return EExecutionStatus::COMPLETED;
	}

	if (Subcommand == TEXT("connections"))
	{
		const int32 Count = UAIModFunctionLibrary::GetFactoryConnectionTelemetry(GetWorld()).Num();
		Sender->SendChatMessage(FString::Printf(TEXT("%d factory connection point(s) found"), Count));
		return EExecutionStatus::COMPLETED;
	}

	if (Subcommand == TEXT("target"))
	{
		// Player index 0 only (see GetTargetedManufacturer's comment) -
		// consistent with the rest of the mod's single-player scope, even
		// though a chat Sender could in principle be a different player.
		const FAIModManufacturerTelemetry Target = UAIModFunctionLibrary::GetTargetedManufacturer(GetWorld());
		if (Target.Id.IsEmpty())
		{
			Sender->SendChatMessage(TEXT("Not currently looking at a manufacturer"));
		}
		else
		{
			Sender->SendChatMessage(FString::Printf(TEXT("%s: recipe=\"%s\" clock=%.0f%% status=%s"),
				*Target.BuildableClass, *Target.Recipe, Target.ClockSpeedPercent, *Target.ProductionStatus));
		}
		return EExecutionStatus::COMPLETED;
	}

	PrintCommandUsage(Sender);
	return EExecutionStatus::BAD_ARGUMENTS;
}
