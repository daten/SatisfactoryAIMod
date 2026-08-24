// Copyright Epic Games, Inc. All Rights Reserved.

#include "DocModChatCommand.h"
#include "DocMod.h"
#include "DocModFunctionLibrary.h"
#include "DocModSelfTest.h"
#include "Command/CommandSender.h"

ADocModChatCommand::ADocModChatCommand()
{
	CommandName = TEXT("docmod");
	Aliases = {TEXT("dm")};
	Usage = FText::FromString(TEXT("docmod <selftest|resourcenodes|buildables|manufacturers|connections|target>"));
	MinNumberOfArguments = 1;
	bOnlyUsableByPlayer = false;
}

EExecutionStatus ADocModChatCommand::ExecuteCommand_Implementation(UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label)
{
	if (!Sender || Arguments.Num() < 1)
	{
		return EExecutionStatus::BAD_ARGUMENTS;
	}

	const FString Subcommand = Arguments[0].ToLower();

	if (Subcommand == TEXT("selftest"))
	{
		Sender->SendChatMessage(TEXT("Running DocMod self-test..."));
		DocModSelfTest::RunAll(GetWorld());
		Sender->SendChatMessage(TEXT("Self-test complete - see LogDocModAI in the log for full PASS/FAIL detail."));
		return EExecutionStatus::COMPLETED;
	}

	if (Subcommand == TEXT("resourcenodes"))
	{
		const int32 Count = UDocModFunctionLibrary::GetResourceNodeTelemetry(GetWorld()).Num();
		Sender->SendChatMessage(FString::Printf(TEXT("%d resource node(s) found"), Count));
		return EExecutionStatus::COMPLETED;
	}

	if (Subcommand == TEXT("buildables"))
	{
		const int32 Count = UDocModFunctionLibrary::GetBuildableTelemetry(GetWorld()).Num();
		Sender->SendChatMessage(FString::Printf(TEXT("%d buildable(s) found"), Count));
		return EExecutionStatus::COMPLETED;
	}

	if (Subcommand == TEXT("manufacturers"))
	{
		const int32 Count = UDocModFunctionLibrary::GetManufacturerTelemetry(GetWorld()).Num();
		Sender->SendChatMessage(FString::Printf(TEXT("%d manufacturer(s) found"), Count));
		return EExecutionStatus::COMPLETED;
	}

	if (Subcommand == TEXT("connections"))
	{
		const int32 Count = UDocModFunctionLibrary::GetFactoryConnectionTelemetry(GetWorld()).Num();
		Sender->SendChatMessage(FString::Printf(TEXT("%d factory connection point(s) found"), Count));
		return EExecutionStatus::COMPLETED;
	}

	if (Subcommand == TEXT("target"))
	{
		// Player index 0 only (see GetTargetedManufacturer's comment) -
		// consistent with the rest of the mod's single-player scope, even
		// though a chat Sender could in principle be a different player.
		const FDocModManufacturerTelemetry Target = UDocModFunctionLibrary::GetTargetedManufacturer(GetWorld());
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
