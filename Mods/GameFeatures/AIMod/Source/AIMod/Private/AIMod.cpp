// Copyright Epic Games, Inc. All Rights Reserved.

#include "AIMod.h"
#include "AIModSelfTest.h"
#include "AIModFunctionLibrary.h"
#include "AIModHotkey.h"
#include "Engine/World.h"
#include "UObject/UObjectGlobals.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY(LogAIModAI);

#define LOCTEXT_NAMESPACE "FAIModModule"

void FAIModModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	UE_LOG(LogAIModAI, Display, TEXT("AIMod AI interface module initialized"));

#if !UE_BUILD_SHIPPING
	// Development-time convenience only (see AIModSelfTest.h) - compiled
	// out of Shipping entirely, not just disabled at runtime, so it can
	// never run automatically for players of a released mod. The console
	// commands below are separate - they only run when someone explicitly
	// types one, so they stay available in all configs.
	WorldInitializedActorsHandle = FWorldDelegates::OnWorldInitializedActors.AddRaw(this, &FAIModModule::OnWorldInitializedActors);
	PostLoadMapWithWorldHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddRaw(this, &FAIModModule::OnPostLoadMapWithWorld);
#endif

	RegisterConsoleCommands();
}

void FAIModModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	UnregisterConsoleCommands();

#if !UE_BUILD_SHIPPING
	FWorldDelegates::OnWorldInitializedActors.Remove(WorldInitializedActorsHandle);
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapWithWorldHandle);
#endif

	UE_LOG(LogAIModAI, Display, TEXT("AIMod AI interface module shutting down"));
}

#if !UE_BUILD_SHIPPING
void FAIModModule::OnWorldInitializedActors(const FActorsInitializedParams& Params)
{
	RunPerWorldSetup(Params.World);
}

void FAIModModule::OnPostLoadMapWithWorld(UWorld* World)
{
	RunPerWorldSetup(World);
}

void FAIModModule::RunPerWorldSetup(UWorld* World)
{
	// Fires for every world load, including menu/editor-preview worlds -
	// skip anything that isn't an actual game world. AIModSelfTest's
	// checks all tolerate "0 found" gracefully, so this is safe to run
	// even on a bare level with no resource nodes/buildings. De-duplicate
	// since two delegates feed this (see AIMod.h's comment on why).
	if (!World || !World->IsGameWorld() || LastSetupWorld == World)
	{
		return;
	}
	LastSetupWorld = World;

	AIModSelfTest::RunAll(World);

	// AIModHotkey performs a REAL mutation when pressed (see its own
	// header comment) - kept behind this same !UE_BUILD_SHIPPING gate as
	// the self-test, deliberately, so it can never fire for a real
	// player of a released mod.
	AIModHotkey::SetupForWorld(World);
}
#endif

namespace
{
	// Shared by all AIMod.* console command handlers below: reports how
	// many records a telemetry getter found, or a clear message if there's
	// no world to query yet (e.g. invoked from -ExecCmds before any level
	// has loaded).
	template <typename GetterFunc>
	void PrintTelemetryCount(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar, const TCHAR* Noun, GetterFunc&& Getter)
	{
		if (!World)
		{
			Ar.Log(TEXT("AIMod: no world available - load a level first"));
			return;
		}
		const int32 Count = Getter(World).Num();
		Ar.Log(FString::Printf(TEXT("AIMod: %d %s found"), Count, Noun));
	}
}

void FAIModModule::RegisterConsoleCommands()
{
	IConsoleManager& ConsoleManager = IConsoleManager::Get();

	ConsoleCommands.Add(ConsoleManager.RegisterConsoleCommand(
		TEXT("AIMod.SelfTest"),
		TEXT("Runs the AIMod self-test against the current world and logs PASS/FAIL results to LogAIModAI."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(
			[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
			{
				if (!World)
				{
					Ar.Log(TEXT("AIMod: no world available - load a level first"));
					return;
				}
				Ar.Log(TEXT("AIMod: running self-test, see LogAIModAI for full detail..."));
				AIModSelfTest::RunAll(World);
			}),
		ECVF_Default));

	ConsoleCommands.Add(ConsoleManager.RegisterConsoleCommand(
		TEXT("AIMod.ResourceNodes"),
		TEXT("Prints how many resource nodes AIMod currently finds in the world."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(
			[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
			{
				PrintTelemetryCount(Args, World, Ar, TEXT("resource node(s)"), &UAIModFunctionLibrary::GetResourceNodeTelemetry);
			}),
		ECVF_Default));

	ConsoleCommands.Add(ConsoleManager.RegisterConsoleCommand(
		TEXT("AIMod.Buildables"),
		TEXT("Prints how many buildables AIMod currently finds in the world."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(
			[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
			{
				PrintTelemetryCount(Args, World, Ar, TEXT("buildable(s)"), &UAIModFunctionLibrary::GetBuildableTelemetry);
			}),
		ECVF_Default));

	ConsoleCommands.Add(ConsoleManager.RegisterConsoleCommand(
		TEXT("AIMod.Manufacturers"),
		TEXT("Prints how many manufacturers AIMod currently finds in the world."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(
			[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
			{
				PrintTelemetryCount(Args, World, Ar, TEXT("manufacturer(s)"), &UAIModFunctionLibrary::GetManufacturerTelemetry);
			}),
		ECVF_Default));

	ConsoleCommands.Add(ConsoleManager.RegisterConsoleCommand(
		TEXT("AIMod.Connections"),
		TEXT("Prints how many factory connection points AIMod currently finds in the world."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(
			[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
			{
				PrintTelemetryCount(Args, World, Ar, TEXT("factory connection point(s)"), &UAIModFunctionLibrary::GetFactoryConnectionTelemetry);
			}),
		ECVF_Default));

	ConsoleCommands.Add(ConsoleManager.RegisterConsoleCommand(
		TEXT("AIMod.Target"),
		TEXT("Prints info about the manufacturer the local player is currently looking at, if any."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(
			[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
			{
				if (!World)
				{
					Ar.Log(TEXT("AIMod: no world available - load a level first"));
					return;
				}
				const FAIModManufacturerTelemetry Target = UAIModFunctionLibrary::GetTargetedManufacturer(World);
				if (Target.Id.IsEmpty())
				{
					Ar.Log(TEXT("AIMod: not currently looking at a manufacturer"));
				}
				else
				{
					Ar.Log(FString::Printf(TEXT("AIMod: %s recipe=\"%s\" clock=%.0f%% status=%s id=%s"),
						*Target.BuildableClass, *Target.Recipe, Target.ClockSpeedPercent, *Target.ProductionStatus, *Target.Id));
				}
			}),
		ECVF_Default));

	ConsoleCommands.Add(ConsoleManager.RegisterConsoleCommand(
		TEXT("AIMod.TargetNode"),
		TEXT("Prints info about the resource node the local player is currently looking at, if any (PLAN.md Phase 13)."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(
			[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
			{
				if (!World)
				{
					Ar.Log(TEXT("AIMod: no world available - load a level first"));
					return;
				}
				const FAIModResourceNodeTelemetry Target = UAIModFunctionLibrary::GetTargetedResourceNode(World);
				if (Target.Id.IsEmpty())
				{
					Ar.Log(TEXT("AIMod: not currently looking at a resource node"));
				}
				else
				{
					Ar.Log(FString::Printf(TEXT("AIMod: %s purity=%s occupied=%s id=%s"),
						*Target.Resource, *Target.Purity, Target.bOccupied ? TEXT("true") : TEXT("false"), *Target.Id));
				}
			}),
		ECVF_Default));

	ConsoleCommands.Add(ConsoleManager.RegisterConsoleCommand(
		TEXT("AIMod.TestExtractorPlacement"),
		TEXT("Dry-run only (never calls Construct(), never touches the save): spawns a Miner Mk1 hologram at the currently-targeted resource node and reports CanConstruct()'s real disqualifier list. Polls real ticks until ready, then logs the result to LogAIModAI - see docs/extractor-placement-research.md."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(
			[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
			{
				if (!World)
				{
					Ar.Log(TEXT("AIMod: no world available - load a level first"));
					return;
				}
				Ar.Log(TEXT("AIMod: running extractor placement dry-run..."));
				const FAIModOperationResult Result = UAIModFunctionLibrary::DebugCheckExtractorPlacementOnTargetedNode(World);
				if (Result.bSuccess)
				{
					Ar.Log(TEXT("AIMod: CanConstruct() = true - placement would succeed (hologram destroyed, nothing was built)"));
				}
				else if (Result.ErrorCode == TEXT("PENDING"))
				{
					Ar.Log(TEXT("AIMod: scheduled - waiting on real ticks, check LogAIModAI in a moment for the actual result"));
				}
				else
				{
					Ar.Log(FString::Printf(TEXT("AIMod: CanConstruct() = false - %s: %s"), *Result.ErrorCode, *Result.ErrorMessage));
				}
			}),
		ECVF_Default));

	ConsoleCommands.Add(ConsoleManager.RegisterConsoleCommand(
		TEXT("AIMod.TestExtractorPlacementViaBuildGun"),
		TEXT("Like AIMod.TestExtractorPlacement, but drives the REAL build gun (HotKeyRecipe) instead of a standalone hologram - VISIBLY equips the build gun for the duration of the test, then unequips it. Never calls Construct() - see docs/buildgun-driven-placement-research.md."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(
			[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
			{
				if (!World)
				{
					Ar.Log(TEXT("AIMod: no world available - load a level first"));
					return;
				}
				Ar.Log(TEXT("AIMod: running extractor placement dry-run via the real build gun (will briefly equip it)..."));
				const FAIModOperationResult Result = UAIModFunctionLibrary::DebugCheckExtractorPlacementViaBuildGun(World);
				if (Result.bSuccess)
				{
					Ar.Log(TEXT("AIMod: CanConstruct() = true - placement would succeed (build gun unequipped, nothing was built)"));
				}
				else if (Result.ErrorCode == TEXT("PENDING"))
				{
					Ar.Log(TEXT("AIMod: scheduled - waiting on real ticks, check LogAIModAI in a moment for the actual result"));
				}
				else
				{
					Ar.Log(FString::Printf(TEXT("AIMod: CanConstruct() = false - %s: %s"), *Result.ErrorCode, *Result.ErrorMessage));
				}
			}),
		ECVF_Default));

	ConsoleCommands.Add(ConsoleManager.RegisterConsoleCommand(
		TEXT("AIMod.ConstructExtractorOnTargetedNode"),
		TEXT("REAL MUTATION - places an actual Miner Mk1 on the currently-targeted resource node if CanConstruct() resolves true (same validated flow as AIMod.TestExtractorPlacementViaBuildGun). Touches the save. See docs/buildgun-driven-placement-research.md."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(
			[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
			{
				if (!World)
				{
					Ar.Log(TEXT("AIMod: no world available - load a level first"));
					return;
				}
				Ar.Log(TEXT("AIMod: attempting REAL extractor construction via the real build gun (will briefly equip it)..."));
				const FAIModOperationResult Result = UAIModFunctionLibrary::ConstructExtractorOnTargetedNode(World);
				if (Result.ErrorCode == TEXT("PENDING"))
				{
					Ar.Log(TEXT("AIMod: scheduled - if CanConstruct() resolves true, a real Miner Mk1 WILL be built; check LogAIModAI in a moment for the actual result"));
				}
				else
				{
					Ar.Log(FString::Printf(TEXT("AIMod: not attempted - %s: %s"), *Result.ErrorCode, *Result.ErrorMessage));
				}
			}),
		ECVF_Default));

	ConsoleCommands.Add(ConsoleManager.RegisterConsoleCommand(
		TEXT("AIMod.PlaceBuildingNearPlayer"),
		TEXT("REAL MUTATION - places a simple, single-step building (production machines only - NOT belts/pipes/walls/foundations) a short distance in front of the player. Optional arg: a UFGRecipe class path, defaults to Recipe_ConstructorMk1 if omitted. Touches the save. See docs/buildgun-driven-placement-research.md."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(
			[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
			{
				if (!World)
				{
					Ar.Log(TEXT("AIMod: no world available - load a level first"));
					return;
				}
				// Verified to exist as a real asset in Content/FactoryGame/Recipes/Buildings/
				// (not guessed from memory).
				const FString RecipeClassPath = Args.Num() > 0 && !Args[0].IsEmpty()
					? Args[0]
					: TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConstructorMk1.Recipe_ConstructorMk1_C");
				Ar.Log(FString::Printf(TEXT("AIMod: attempting REAL construction of %s near the player via the real build gun (will briefly equip it)..."), *RecipeClassPath));
				const FAIModOperationResult Result = UAIModFunctionLibrary::ConstructBuildingNearPlayer(World, RecipeClassPath);
				if (Result.ErrorCode == TEXT("PENDING"))
				{
					Ar.Log(TEXT("AIMod: scheduled - if CanConstruct() resolves true, the building WILL be built; check LogAIModAI in a moment for the actual result"));
				}
				else
				{
					Ar.Log(FString::Printf(TEXT("AIMod: not attempted - %s: %s"), *Result.ErrorCode, *Result.ErrorMessage));
				}
			}),
		ECVF_Default));

	ConsoleCommands.Add(ConsoleManager.RegisterConsoleCommand(
		TEXT("AIMod.TestPowerConnection"),
		TEXT("Dry-run only (never constructs, never touches the save): args <buildableIdA> <buildableIdB> (session-local ids, e.g. from a prior placement's result.buildableId). Attempts to wire their power connections and reports CanConstruct(). See docs/conveyor-power-connection-research.md."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(
			[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
			{
				if (!World)
				{
					Ar.Log(TEXT("AIMod: no world available - load a level first"));
					return;
				}
				if (Args.Num() < 2 || Args[0].IsEmpty() || Args[1].IsEmpty())
				{
					Ar.Log(TEXT("AIMod: usage - AIMod.TestPowerConnection <buildableIdA> <buildableIdB>"));
					return;
				}
				Ar.Log(TEXT("AIMod: running power connection dry-run via the real build gun (will briefly equip it)..."));
				const FAIModOperationResult Result = UAIModFunctionLibrary::DebugCheckPowerConnection(World, Args[0], Args[1]);
				if (Result.ErrorCode == TEXT("PENDING"))
				{
					Ar.Log(TEXT("AIMod: scheduled - check LogAIModAI in a moment for the actual result"));
				}
				else
				{
					Ar.Log(FString::Printf(TEXT("AIMod: not attempted - %s: %s"), *Result.ErrorCode, *Result.ErrorMessage));
				}
			}),
		ECVF_Default));
}

void FAIModModule::UnregisterConsoleCommands()
{
	IConsoleManager& ConsoleManager = IConsoleManager::Get();
	for (IConsoleCommand* Command : ConsoleCommands)
	{
		ConsoleManager.UnregisterConsoleObject(Command);
	}
	ConsoleCommands.Empty();
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FAIModModule, AIMod)