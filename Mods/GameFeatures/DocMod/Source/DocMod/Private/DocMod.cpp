// Copyright Epic Games, Inc. All Rights Reserved.

#include "DocMod.h"
#include "DocModSelfTest.h"
#include "DocModFunctionLibrary.h"
#include "DocModHotkey.h"
#include "Engine/World.h"
#include "UObject/UObjectGlobals.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY(LogDocModAI);

#define LOCTEXT_NAMESPACE "FDocModModule"

void FDocModModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	UE_LOG(LogDocModAI, Display, TEXT("DocMod AI interface module initialized"));

#if !UE_BUILD_SHIPPING
	// Development-time convenience only (see DocModSelfTest.h) - compiled
	// out of Shipping entirely, not just disabled at runtime, so it can
	// never run automatically for players of a released mod. The console
	// commands below are separate - they only run when someone explicitly
	// types one, so they stay available in all configs.
	WorldInitializedActorsHandle = FWorldDelegates::OnWorldInitializedActors.AddRaw(this, &FDocModModule::OnWorldInitializedActors);
	PostLoadMapWithWorldHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddRaw(this, &FDocModModule::OnPostLoadMapWithWorld);
#endif

	RegisterConsoleCommands();
}

void FDocModModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	UnregisterConsoleCommands();

#if !UE_BUILD_SHIPPING
	FWorldDelegates::OnWorldInitializedActors.Remove(WorldInitializedActorsHandle);
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapWithWorldHandle);
#endif

	UE_LOG(LogDocModAI, Display, TEXT("DocMod AI interface module shutting down"));
}

#if !UE_BUILD_SHIPPING
void FDocModModule::OnWorldInitializedActors(const FActorsInitializedParams& Params)
{
	RunPerWorldSetup(Params.World);
}

void FDocModModule::OnPostLoadMapWithWorld(UWorld* World)
{
	RunPerWorldSetup(World);
}

void FDocModModule::RunPerWorldSetup(UWorld* World)
{
	// Fires for every world load, including menu/editor-preview worlds -
	// skip anything that isn't an actual game world. DocModSelfTest's
	// checks all tolerate "0 found" gracefully, so this is safe to run
	// even on a bare level with no resource nodes/buildings. De-duplicate
	// since two delegates feed this (see DocMod.h's comment on why).
	if (!World || !World->IsGameWorld() || LastSetupWorld == World)
	{
		return;
	}
	LastSetupWorld = World;

	DocModSelfTest::RunAll(World);

	// DocModHotkey performs a REAL mutation when pressed (see its own
	// header comment) - kept behind this same !UE_BUILD_SHIPPING gate as
	// the self-test, deliberately, so it can never fire for a real
	// player of a released mod.
	DocModHotkey::SetupForWorld(World);
}
#endif

namespace
{
	// Shared by all DocMod.* console command handlers below: reports how
	// many records a telemetry getter found, or a clear message if there's
	// no world to query yet (e.g. invoked from -ExecCmds before any level
	// has loaded).
	template <typename GetterFunc>
	void PrintTelemetryCount(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar, const TCHAR* Noun, GetterFunc&& Getter)
	{
		if (!World)
		{
			Ar.Log(TEXT("DocMod: no world available - load a level first"));
			return;
		}
		const int32 Count = Getter(World).Num();
		Ar.Log(FString::Printf(TEXT("DocMod: %d %s found"), Count, Noun));
	}
}

void FDocModModule::RegisterConsoleCommands()
{
	IConsoleManager& ConsoleManager = IConsoleManager::Get();

	ConsoleCommands.Add(ConsoleManager.RegisterConsoleCommand(
		TEXT("DocMod.SelfTest"),
		TEXT("Runs the DocMod self-test against the current world and logs PASS/FAIL results to LogDocModAI."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(
			[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
			{
				if (!World)
				{
					Ar.Log(TEXT("DocMod: no world available - load a level first"));
					return;
				}
				Ar.Log(TEXT("DocMod: running self-test, see LogDocModAI for full detail..."));
				DocModSelfTest::RunAll(World);
			}),
		ECVF_Default));

	ConsoleCommands.Add(ConsoleManager.RegisterConsoleCommand(
		TEXT("DocMod.ResourceNodes"),
		TEXT("Prints how many resource nodes DocMod currently finds in the world."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(
			[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
			{
				PrintTelemetryCount(Args, World, Ar, TEXT("resource node(s)"), &UDocModFunctionLibrary::GetResourceNodeTelemetry);
			}),
		ECVF_Default));

	ConsoleCommands.Add(ConsoleManager.RegisterConsoleCommand(
		TEXT("DocMod.Buildables"),
		TEXT("Prints how many buildables DocMod currently finds in the world."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(
			[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
			{
				PrintTelemetryCount(Args, World, Ar, TEXT("buildable(s)"), &UDocModFunctionLibrary::GetBuildableTelemetry);
			}),
		ECVF_Default));

	ConsoleCommands.Add(ConsoleManager.RegisterConsoleCommand(
		TEXT("DocMod.Manufacturers"),
		TEXT("Prints how many manufacturers DocMod currently finds in the world."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(
			[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
			{
				PrintTelemetryCount(Args, World, Ar, TEXT("manufacturer(s)"), &UDocModFunctionLibrary::GetManufacturerTelemetry);
			}),
		ECVF_Default));

	ConsoleCommands.Add(ConsoleManager.RegisterConsoleCommand(
		TEXT("DocMod.Connections"),
		TEXT("Prints how many factory connection points DocMod currently finds in the world."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(
			[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
			{
				PrintTelemetryCount(Args, World, Ar, TEXT("factory connection point(s)"), &UDocModFunctionLibrary::GetFactoryConnectionTelemetry);
			}),
		ECVF_Default));

	ConsoleCommands.Add(ConsoleManager.RegisterConsoleCommand(
		TEXT("DocMod.Target"),
		TEXT("Prints info about the manufacturer the local player is currently looking at, if any."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(
			[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
			{
				if (!World)
				{
					Ar.Log(TEXT("DocMod: no world available - load a level first"));
					return;
				}
				const FDocModManufacturerTelemetry Target = UDocModFunctionLibrary::GetTargetedManufacturer(World);
				if (Target.Id.IsEmpty())
				{
					Ar.Log(TEXT("DocMod: not currently looking at a manufacturer"));
				}
				else
				{
					Ar.Log(FString::Printf(TEXT("DocMod: %s recipe=\"%s\" clock=%.0f%% status=%s id=%s"),
						*Target.BuildableClass, *Target.Recipe, Target.ClockSpeedPercent, *Target.ProductionStatus, *Target.Id));
				}
			}),
		ECVF_Default));

	ConsoleCommands.Add(ConsoleManager.RegisterConsoleCommand(
		TEXT("DocMod.TargetNode"),
		TEXT("Prints info about the resource node the local player is currently looking at, if any (PLAN.md Phase 13)."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(
			[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
			{
				if (!World)
				{
					Ar.Log(TEXT("DocMod: no world available - load a level first"));
					return;
				}
				const FDocModResourceNodeTelemetry Target = UDocModFunctionLibrary::GetTargetedResourceNode(World);
				if (Target.Id.IsEmpty())
				{
					Ar.Log(TEXT("DocMod: not currently looking at a resource node"));
				}
				else
				{
					Ar.Log(FString::Printf(TEXT("DocMod: %s purity=%s occupied=%s id=%s"),
						*Target.Resource, *Target.Purity, Target.bOccupied ? TEXT("true") : TEXT("false"), *Target.Id));
				}
			}),
		ECVF_Default));

	ConsoleCommands.Add(ConsoleManager.RegisterConsoleCommand(
		TEXT("DocMod.TestExtractorPlacement"),
		TEXT("Dry-run only (never calls Construct(), never touches the save): spawns a Miner Mk1 hologram at the currently-targeted resource node and reports CanConstruct()'s real disqualifier list. Polls real ticks until ready, then logs the result to LogDocModAI - see docs/extractor-placement-research.md."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(
			[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
			{
				if (!World)
				{
					Ar.Log(TEXT("DocMod: no world available - load a level first"));
					return;
				}
				Ar.Log(TEXT("DocMod: running extractor placement dry-run..."));
				const FDocModOperationResult Result = UDocModFunctionLibrary::DebugCheckExtractorPlacementOnTargetedNode(World);
				if (Result.bSuccess)
				{
					Ar.Log(TEXT("DocMod: CanConstruct() = true - placement would succeed (hologram destroyed, nothing was built)"));
				}
				else if (Result.ErrorCode == TEXT("PENDING"))
				{
					Ar.Log(TEXT("DocMod: scheduled - waiting on real ticks, check LogDocModAI in a moment for the actual result"));
				}
				else
				{
					Ar.Log(FString::Printf(TEXT("DocMod: CanConstruct() = false - %s: %s"), *Result.ErrorCode, *Result.ErrorMessage));
				}
			}),
		ECVF_Default));
}

void FDocModModule::UnregisterConsoleCommands()
{
	IConsoleManager& ConsoleManager = IConsoleManager::Get();
	for (IConsoleCommand* Command : ConsoleCommands)
	{
		ConsoleManager.UnregisterConsoleObject(Command);
	}
	ConsoleCommands.Empty();
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FDocModModule, DocMod)