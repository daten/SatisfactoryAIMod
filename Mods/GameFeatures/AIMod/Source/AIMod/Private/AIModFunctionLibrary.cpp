// Copyright Epic Games, Inc. All Rights Reserved.

#include "AIModFunctionLibrary.h"
#include "AIMod.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Resources/FGResourceNode.h"
#include "Resources/FGResourceNodeBase.h"
#include "Resources/FGResourceNodeFrackingCore.h"
#include "Resources/FGResourceNodeFrackingSatellite.h"
#include "Resources/FGItemDescriptor.h"
#include "FGBuildableSubsystem.h"
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableFactory.h"
#include "Buildables/FGBuildableManufacturer.h"
#include "FGRecipe.h"
#include "FGInventoryComponent.h"
#include "FGFactoryConnectionComponent.h"
#include "FGCharacterPlayer.h"
#include "Kismet/GameplayStatics.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Hologram/FGHologram.h"
#include "FGConstructDisqualifier.h"
#include "Equipment/FGBuildGun.h"
#include "Equipment/FGBuildGunBuild.h"
#include "CollisionQueryParams.h"
#include "Hologram/FGWireHologram.h"
#include "FGPowerConnectionComponent.h"
#include "Hologram/FGConveyorBeltHologram.h"
#include "Buildables/FGBuildableConveyorBase.h"
#include "Buildables/FGBuildableWire.h"
#include "Buildables/FGBuildablePipeline.h"
#include "Buildables/FGBuildableConveyorAttachment.h"
#include "Buildables/FGBuildableSplitterSmart.h"
#include "Buildables/FGBuildableConveyorLift.h"
#include "Hologram/FGConveyorLiftHologram.h"
#include "Hologram/FGHologramBuildModeDescriptor.h"
#include "Hologram/FGPipelineHologram.h"
#include "FGPipeConnectionComponent.h"
#include "FGPipeConnectionComponentHyper.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/HitResult.h"
#include "Engine/ActorInstanceHandle.h"
#include "FGDismantleInterface.h"
#include "FGLightweightBuildableSubsystem.h"
#include "Resources/FGBuildingDescriptor.h"
#include "Resources/FGBuildDescriptor.h"
#include "FGRecipeManager.h"
#include "Buildables/FGBuildableGenerator.h"
#include "Buildables/FGBuildableResourceExtractorBase.h"
#include "FGClearanceInterface.h"
#include "FGClearanceData.h"
#include "FGTimeSubsystem.h"
#include "FGChatManager.h"
#include "Configuration/ConfigManager.h"
#include "Configuration/Properties/ConfigPropertyBool.h"
#include "Configuration/Properties/ConfigPropertyFloat.h"
#include "Configuration/Properties/ConfigPropertySection.h"
#include "FGPortableMiner.h"
#include "Equipment/FGPortableMinerDispenser.h"
#include "FGInventoryComponentEquipment.h"
#include "Resources/FGEquipmentDescriptor.h"
#include "FGBuildablePipelineFlowIndicator.h"
#include "FGCreatureSubsystem.h"
#include "FGCentralStorageSubsystem.h"
#include "Creature/FGCreature.h"

namespace
{
	// Forward declaration - real definition lives further down (originally
	// written for use only after that point in the file); ConstructBuildingAtPosition's
	// faceBuildableId support (2026-08-27) needs to call it earlier than that.
	// Anonymous namespaces in the same translation unit all merge into one,
	// so this and the later definition refer to the same function - only
	// textual order (declare before use) matters here.
	AFGBuildable* FindBuildableById(UWorld* World, const FString& BuildableId);
	FString WriteCondensedJson(const TSharedRef<FJsonObject>& RootObject);

	/**
	 * Shared ground-trace logic (2026-08-27), factored out of
	 * ConstructBuildingAtPosition so world.groundHeight can expose the
	 * exact same real trace as a standalone, read-only query - added per
	 * explicit user request to make placement Z deterministic without
	 * requiring the caller to already know that "z" is a +/-1000-unit
	 * search center, not a literal height (see docs/placement-lessons.md).
	 * A caller can now query the real ground Z at an X/Y first, then pass
	 * that exact value back in as ReferenceZ - no more guess-and-iterate.
	 */
	struct FGroundTraceResult
	{
		bool bFound = false;
		// Full hit, not just Location/Normal - ConstructBuildingAtPosition's
		// synthetic hit needs every field a real trace would populate
		// (Component, Distance, HitObjectHandle, etc.), not just the two
		// values world.groundHeight cares about.
		FHitResult Hit;
	};

	FGroundTraceResult FindGroundAtXY(UWorld* World, float X, float Y, float ZSearchCenter, AActor* IgnoreActor)
	{
		FGroundTraceResult Result;

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AIModGroundTrace), false);
		if (IgnoreActor)
		{
			QueryParams.AddIgnoredActor(IgnoreActor);
		}
		const FVector TraceStart(X, Y, ZSearchCenter + 1000.0f);
		const FVector TraceEnd(X, Y, ZSearchCenter - 1000.0f);

		Result.bFound = World->LineTraceSingleByChannel(Result.Hit, TraceStart, TraceEnd, ECC_Visibility, QueryParams);
		if (!Result.bFound)
		{
			// Same fallback ConstructBuildingAtPosition always used: the
			// literal search-center point, facing straight up.
			Result.Hit.Location = FVector(X, Y, ZSearchCenter);
			Result.Hit.ImpactPoint = Result.Hit.Location;
			Result.Hit.Normal = FVector::UpVector;
			Result.Hit.ImpactNormal = FVector::UpVector;
			Result.Hit.bBlockingHit = true;
		}
		return Result;
	}

	// GetResourcePurityText() looked like a plain display string but is
	// actually Slate rich-text markup meant for on-screen UI (its own doc
	// comment says "For UI") - confirmed against a real save, it returned
	// literal "<Bold>(Normal)</>" instead of "Normal", caught by
	// AIModSelfTest on its first real run. Use the raw enum
	// (GetResourcePurity(), also "For UI" per its comment but returns the
	// actual EResourcePurity value) and map it ourselves, consistent with
	// ProductionStatusToString/FactoryConnectionDirectionToString below.
	FString ResourcePurityToString(EResourcePurity Purity)
	{
		switch (Purity)
		{
		case EResourcePurity::RP_Inpure: return TEXT("Impure");
		case EResourcePurity::RP_Normal: return TEXT("Normal");
		case EResourcePurity::RP_Pure: return TEXT("Pure");
		default: return TEXT("Unknown");
		}
	}

	// EResourceNodeType is confirmed present on AFGResourceNodeBase
	// (FGResourceNodeBase.h:23-33) - real UENUM, manual switch for the
	// same reason as the other To*String helpers in this file.
	FString ResourceNodeTypeToString(EResourceNodeType Type)
	{
		switch (Type)
		{
		case EResourceNodeType::Node: return TEXT("Node");
		case EResourceNodeType::FrackingSatellite: return TEXT("FrackingSatellite");
		case EResourceNodeType::FrackingCore: return TEXT("FrackingCore");
		case EResourceNodeType::Geyser: return TEXT("Geyser");
		case EResourceNodeType::Deposit: return TEXT("Deposit");
		default: return TEXT("Invalid");
		}
	}

	FString FrackingSatelliteStateToString(EFrackingSatelliteState State)
	{
		switch (State)
		{
		case EFrackingSatelliteState::FSS_Untouched: return TEXT("Untouched");
		case EFrackingSatelliteState::FSS_Active: return TEXT("Active");
		case EFrackingSatelliteState::FSS_Inactive: return TEXT("Inactive");
		default: return TEXT("Unknown");
		}
	}

	// AFGResourceNodeBase (2026-08-27, was AFGResourceNode) - widened
	// alongside ConstructExtractorOnNode to also cover
	// AFGResourceNodeFrackingCore (a Resource Well Pressurizer's real
	// target, NOT an AFGResourceNode - see that function's doc comment).
	// GetResourcePurity() only exists on AFGResourceNode (confirmed from
	// source - FGResourceNode.h, not declared on the shared
	// AFGResourceNodeBase), so it's read conditionally here; a Fracking
	// Core has no meaningful purity of its own.
	FAIModResourceNodeTelemetry MakeResourceNodeTelemetry(AFGResourceNodeBase* Node)
	{
		const TSubclassOf<UFGResourceDescriptor> ResourceClass = Node->GetResourceClass();

		FAIModResourceNodeTelemetry Telemetry;
		// Session-local only - see FAIModResourceNodeTelemetry's comment.
		Telemetry.Id = Node->GetPathName();
		Telemetry.Resource = ResourceClass ? UFGItemDescriptor::GetItemName(ResourceClass).ToString() : TEXT("Unknown");
		Telemetry.ResourceClass = ResourceClass ? ResourceClass->GetPathName() : FString();
		Telemetry.Position = Node->GetActorLocation();
		Telemetry.bOccupied = Node->IsOccupied();
		Telemetry.NodeType = ResourceNodeTypeToString(Node->GetResourceNodeType());

		if (const AFGResourceNode* PlainNode = Cast<AFGResourceNode>(Node))
		{
			Telemetry.Purity = ResourcePurityToString(PlainNode->GetResourcePurity());
		}
		else
		{
			Telemetry.Purity = TEXT("N/A");
		}

		if (AFGResourceNodeFrackingSatellite* Satellite = Cast<AFGResourceNodeFrackingSatellite>(Node))
		{
			Telemetry.SatelliteState = FrackingSatelliteStateToString(Satellite->GetState());
			if (AFGResourceNodeFrackingCore* Core = Satellite->GetCore().Get())
			{
				Telemetry.CoreId = Core->GetPathName();
			}
		}

		return Telemetry;
	}

	// AFGResourceNodeManager exists but its node array has no public
	// getter and its .cpp is a stub (see docs/resource-node-research.md),
	// so a plain actor-iterator world scan is the only evidenced way to
	// enumerate nodes right now. Fine for a debug/Phase-4 entry point;
	// CLAUDE.md steers production code toward a subsystem/event-driven
	// approach instead of scanning every frame.
	TArray<FAIModResourceNodeTelemetry> CollectResourceNodeTelemetry(UWorld* World)
	{
		TArray<FAIModResourceNodeTelemetry> Nodes;
		for (TActorIterator<AFGResourceNodeBase> It(World); It; ++It)
		{
			AFGResourceNodeBase* Node = *It;
			if (!IsValid(Node))
			{
				continue;
			}
			Nodes.Add(MakeResourceNodeTelemetry(Node));
		}
		return Nodes;
	}

	// AFGBuildableSubsystem::GetAllBuildablesRef() is a real public getter
	// (unlike the resource node manager), but its .cpp is stubbed in this
	// repo so whether mBuildables is actually populated at runtime is
	// unverified - fall back to a world scan if the subsystem is missing.
	// See docs/buildable-research.md.
	TArray<AFGBuildable*> CollectAllBuildables(UWorld* World)
	{
		if (AFGBuildableSubsystem* Subsystem = AFGBuildableSubsystem::Get(World))
		{
			return Subsystem->GetAllBuildablesRef();
		}

		UE_LOG(LogAIModAI, Warning, TEXT("CollectAllBuildables: AFGBuildableSubsystem unavailable, falling back to TActorIterator"));
		TArray<AFGBuildable*> Buildables;
		for (TActorIterator<AFGBuildable> It(World); It; ++It)
		{
			if (IsValid(*It))
			{
				Buildables.Add(*It);
			}
		}
		return Buildables;
	}

	// Id prefix for "lightweight" buildables (foundations, and likely
	// walls/other mass-placed pieces) - see MakeLightweightBuildableId's
	// doc comment for why these need a wholly different id shape than
	// AFGBuildable::GetPathName().
	const TCHAR* LightweightIdPrefix = TEXT("lightweight:");

	/**
	 * Lightweight buildables (2026-08-25 discovery - see
	 * docs/lightweight-buildable-research.md) are NOT AFGBuildable actors
	 * at all - they're stored as FRuntimeBuildableInstanceData in
	 * AFGLightweightBuildableSubsystem, for performance at scale
	 * (thousands of foundation/wall pieces would be expensive as full
	 * actors). Confirmed live: placing a foundation via
	 * ConstructBuildingAtPosition reported success and the piece was
	 * visually confirmed in-game, but it appeared in neither the
	 * proximity-based buildableId lookup nor the full world.buildables
	 * list (still exactly 10121 real AFGBuildable actors before and
	 * after). GetPathName() is meaningless here - there's no actor - so
	 * these use "lightweight:<BuildableClassPath>|<Index>" instead,
	 * identity being (class, array index) into
	 * GetAllLightweightBuildableInstances(). '|' rather than a second
	 * ':' as the separator - Unreal object paths can themselves contain
	 * ':' (e.g. a level's "Persistent_Level:PersistentLevel" nesting),
	 * but never '|'.
	 */
	FString MakeLightweightBuildableId(const TSubclassOf<AFGBuildable>& BuildableClass, int32 Index)
	{
		return FString::Printf(TEXT("%s%s|%d"), LightweightIdPrefix, *BuildableClass->GetPathName(), Index);
	}

	bool IsLightweightBuildableId(const FString& BuildableId)
	{
		return BuildableId.StartsWith(LightweightIdPrefix);
	}

	// Splits a "lightweight:<ClassPath>|<Index>" id back into its class
	// path and index. Returns false (and leaves outputs unchanged) if
	// BuildableId isn't well-formed.
	bool ParseLightweightBuildableId(const FString& BuildableId, FString& OutClassPath, int32& OutIndex)
	{
		if (!IsLightweightBuildableId(BuildableId))
		{
			return false;
		}
		const FString Remainder = BuildableId.RightChop(FCString::Strlen(LightweightIdPrefix));
		FString ClassPath;
		FString IndexString;
		if (!Remainder.Split(TEXT("|"), &ClassPath, &IndexString, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
		{
			return false;
		}
		if (!IndexString.IsNumeric())
		{
			return false;
		}
		OutClassPath = ClassPath;
		OutIndex = FCString::Atoi(*IndexString);
		return true;
	}

	// Resolves a building recipe class path to the AFGBuildable subclass
	// it actually constructs, via its first product (expected to be a
	// UFGBuildingDescriptor - true for every simple building recipe used
	// in this project so far) and
	// UFGBuildingDescriptor::GetBuildableClass(). Used to search
	// AFGLightweightBuildableSubsystem's instances by class when a
	// just-placed recipe produced a lightweight buildable instead of a
	// real actor - see docs/lightweight-buildable-research.md. Returns
	// nullptr (not an error) if the recipe's first product isn't a
	// building descriptor.
	// Shared by ResolveBuildableClassForRecipe and
	// ResolveConveyorBeltHologramClassForRecipe - both need the recipe's
	// product descriptor class, just call a different static getter on
	// it afterward (GetBuildableClass vs GetHologramClass).
	TSubclassOf<UFGBuildingDescriptor> ResolveBuildingDescriptorClassForRecipe(const FString& RecipeClassPath)
	{
		UClass* RecipeClass = LoadObject<UClass>(nullptr, *RecipeClassPath);
		if (!RecipeClass || !RecipeClass->IsChildOf(UFGRecipe::StaticClass()))
		{
			return nullptr;
		}
		const TArray<FItemAmount> Products = UFGRecipe::GetProducts(RecipeClass);
		if (Products.Num() == 0 || !Products[0].ItemClass || !Products[0].ItemClass->IsChildOf(UFGBuildingDescriptor::StaticClass()))
		{
			return nullptr;
		}
		return TSubclassOf<UFGBuildingDescriptor>(Products[0].ItemClass.Get());
	}

	TSubclassOf<AFGBuildable> ResolveBuildableClassForRecipe(const FString& RecipeClassPath)
	{
		const TSubclassOf<UFGBuildingDescriptor> BuildingDescriptorClass = ResolveBuildingDescriptorClassForRecipe(RecipeClassPath);
		if (!BuildingDescriptorClass)
		{
			return nullptr;
		}
		return UFGBuildingDescriptor::GetBuildableClass(BuildingDescriptorClass);
	}

	// UFGBuildDescriptor::GetHologramClass (UFGBuildingDescriptor's base
	// class) - a different static accessor on the same descriptor class
	// ResolveBuildableClassForRecipe resolves, giving the HOLOGRAM class
	// (which owns mBendRadius/mMaxSplineLength) rather than the buildable
	// class. Used for LogConveyorBeltTiersAsJson - these limits are
	// EditDefaultsOnly Blueprint-configured class defaults, so the CDO
	// has the real per-tier value without ever spawning an instance or
	// needing a player.
	TSubclassOf<AFGConveyorBeltHologram> ResolveConveyorBeltHologramClassForRecipe(const FString& RecipeClassPath)
	{
		const TSubclassOf<UFGBuildingDescriptor> BuildingDescriptorClass = ResolveBuildingDescriptorClassForRecipe(RecipeClassPath);
		if (!BuildingDescriptorClass)
		{
			return nullptr;
		}
		UClass* HologramClass = UFGBuildDescriptor::GetHologramClass(BuildingDescriptorClass);
		return (HologramClass && HologramClass->IsChildOf(AFGConveyorBeltHologram::StaticClass()))
			? TSubclassOf<AFGConveyorBeltHologram>(HologramClass)
			: nullptr;
	}

	// Same pattern as ResolveConveyorBeltHologramClassForRecipe, for
	// pipelines (2026-08-25 pipe groundwork). AFGPipelineHologram is a
	// sibling of AFGConveyorBeltHologram - both derive directly from
	// AFGSplineHologram - confirmed from source, not assumed.
	TSubclassOf<AFGPipelineHologram> ResolvePipelineHologramClassForRecipe(const FString& RecipeClassPath)
	{
		const TSubclassOf<UFGBuildingDescriptor> BuildingDescriptorClass = ResolveBuildingDescriptorClassForRecipe(RecipeClassPath);
		if (!BuildingDescriptorClass)
		{
			return nullptr;
		}
		UClass* HologramClass = UFGBuildDescriptor::GetHologramClass(BuildingDescriptorClass);
		return (HologramClass && HologramClass->IsChildOf(AFGPipelineHologram::StaticClass()))
			? TSubclassOf<AFGPipelineHologram>(HologramClass)
			: nullptr;
	}

	// Same generic-discovery pattern as FindFreeFactoryConnection/
	// FindFreePowerConnection, for pipes: UFGPipeConnectionComponentBase/
	// EPipeConnectionType are pipes' own parallel type hierarchy (NOT
	// UFGFactoryConnectionComponent/EFactoryConnectionDirection) -
	// confirmed from source (FGPipeConnectionComponent.h), not assumed.
	// PCT_PRODUCER is the pipe equivalent of FCD_OUTPUT, PCT_CONSUMER of
	// FCD_INPUT.
	UFGPipeConnectionComponentBase* FindFreePipeConnection(AFGBuildable* Buildable, EPipeConnectionType Type)
	{
		TArray<UFGPipeConnectionComponentBase*> Connections;
		Buildable->GetComponents<UFGPipeConnectionComponentBase>(Connections);
		for (UFGPipeConnectionComponentBase* Connection : Connections)
		{
			if (IsValid(Connection) && Connection->GetPipeConnectionType() == Type && !Connection->IsConnected())
			{
				return Connection;
			}
		}
		return nullptr;
	}

	// Fluid pipe machines with a genuine producer/consumer distinction
	// (Refineries, Pumps, Blenders, etc.) match via FindFreePipeConnection's
	// exact PCT_PRODUCER/PCT_CONSUMER filter above. But confirmed live
	// 2026-08-27 that several real, common fluid-pipe buildables -
	// Storage Tanks (Recipe_PipeStorageTank) and Pipeline Junctions
	// (Cross/T) - have ONLY PCT_ANY connectors (a fresh Storage Tank's 2
	// connectors and a Cross Junction's 4 were all "Any", none
	// Producer/Consumer) - the strict match finds nothing on either, so
	// ConstructPipe could never reach a storage buffer or merge multiple
	// lines through a junction. Falls back to any free PCT_ANY connector
	// once the exact match fails - explicitly excludes
	// UFGPipeConnectionComponentHyper (that's FindFreeHyperPipeConnection's
	// job) and PCT_SNAP_ONLY (structural, not a real endpoint - same
	// exclusion as the hyper finder above).
	UFGPipeConnectionComponentBase* FindFreeFluidPipeConnection(AFGBuildable* Buildable, EPipeConnectionType PreferredType)
	{
		if (UFGPipeConnectionComponentBase* Exact = FindFreePipeConnection(Buildable, PreferredType))
		{
			return Exact;
		}

		TArray<UFGPipeConnectionComponentBase*> Connections;
		Buildable->GetComponents<UFGPipeConnectionComponentBase>(Connections);
		for (UFGPipeConnectionComponentBase* Connection : Connections)
		{
			if (IsValid(Connection) && !Cast<UFGPipeConnectionComponentHyper>(Connection)
				&& Connection->GetPipeConnectionType() == EPipeConnectionType::PCT_ANY
				&& !Connection->IsConnected())
			{
				return Connection;
			}
		}
		return nullptr;
	}

	// Hypertube connectors are a different shape than fluid pipe connectors
	// (research 2026-08-27, docs/hypertube-research.md): they're all
	// UFGPipeConnectionComponentHyper (a plain type-tag subclass of
	// UFGPipeConnectionComponentBase - confirmed no added members from
	// source) and their mPipeConnectionType stays the CDO default
	// PCT_ANY, NOT PCT_PRODUCER/PCT_CONSUMER like fluid pipe machines -
	// FindFreePipeConnection's exact-type-match filter would find nothing
	// on any hypertube part. Hypertubes are also bidirectional (no real
	// producer/consumer distinction), so both ends use this same finder.
	// Skip PCT_SNAP_ONLY connectors (wall supports/poles) - those are
	// explicitly not real endpoints (FGPipeConnectionComponent.h's
	// IsConnected() doc comment), just structural snap points.
	UFGPipeConnectionComponentBase* FindFreeHyperPipeConnection(AFGBuildable* Buildable)
	{
		TArray<UFGPipeConnectionComponentBase*> Connections;
		Buildable->GetComponents<UFGPipeConnectionComponentBase>(Connections);
		for (UFGPipeConnectionComponentBase* Connection : Connections)
		{
			if (IsValid(Connection) && Cast<UFGPipeConnectionComponentHyper>(Connection)
				&& Connection->GetPipeConnectionType() != EPipeConnectionType::PCT_SNAP_ONLY
				&& !Connection->IsConnected())
			{
				return Connection;
			}
		}
		return nullptr;
	}

	TArray<FAIModBuildableTelemetry> CollectLightweightBuildableTelemetry(UWorld* World)
	{
		TArray<FAIModBuildableTelemetry> Result;
		AFGLightweightBuildableSubsystem* LightweightSubsystem = AFGLightweightBuildableSubsystem::Get(World);
		if (!LightweightSubsystem)
		{
			return Result;
		}

		for (const auto& ClassAndInstances : LightweightSubsystem->GetAllLightweightBuildableInstances())
		{
			const TSubclassOf<AFGBuildable> BuildableClass = ClassAndInstances.Key;
			if (!BuildableClass)
			{
				continue;
			}
			const TArray<FRuntimeBuildableInstanceData>& Instances = ClassAndInstances.Value;
			for (int32 Index = 0; Index < Instances.Num(); ++Index)
			{
				const FRuntimeBuildableInstanceData& InstanceData = Instances[Index];
				// A removed instance's slot has Handles.Num()==0 &&
				// BuiltWithRecipe==nullptr (matches Clear()'s real body in
				// FGLightweightBuildableSubsystem.h) - IsValid() below skips
				// it. CORRECTION (2026-08-25, docs/lightweight-buildable-
				// research.md "Index stability"): this does NOT mean
				// (class, index) identity is stable over time - live
				// evidence showed a whole batch of indices shift after
				// unrelated deletes (a periodic compaction, not a pure
				// tombstone) - always re-resolve an id by position before
				// trusting an old one.
				if (!InstanceData.IsValid())
				{
					continue;
				}

				FAIModBuildableTelemetry Telemetry;
				Telemetry.Id = MakeLightweightBuildableId(BuildableClass, Index);
				Telemetry.BuildableClass = BuildableClass->GetPathName();
				Telemetry.Position = InstanceData.Transform.GetLocation();
				Telemetry.Rotation = InstanceData.Transform.Rotator();
				Result.Add(MoveTemp(Telemetry));
			}
		}
		return Result;
	}

	TArray<FAIModBuildableTelemetry> CollectBuildableTelemetry(UWorld* World)
	{
		TArray<FAIModBuildableTelemetry> Result;
		for (AFGBuildable* Buildable : CollectAllBuildables(World))
		{
			if (!IsValid(Buildable))
			{
				continue;
			}

			FAIModBuildableTelemetry Telemetry;
			Telemetry.Id = Buildable->GetPathName();
			Telemetry.BuildableClass = Buildable->GetClass()->GetPathName();
			Telemetry.Position = Buildable->GetActorLocation();
			Telemetry.Rotation = Buildable->GetActorRotation();
			Result.Add(MoveTemp(Telemetry));
		}
		Result.Append(CollectLightweightBuildableTelemetry(World));
		return Result;
	}

	// EProductionStatus (FGBuildable.h) is a plain C++ enum class, not a
	// UENUM, so there is no UEnum::GetDisplayValueAsText reflection path.
	FString ProductionStatusToString(EProductionStatus Status)
	{
		switch (Status)
		{
		case EProductionStatus::IS_NONE: return TEXT("None");
		case EProductionStatus::IS_PRODUCING: return TEXT("Producing");
		case EProductionStatus::IS_PRODUCING_WITH_CRYSTAL: return TEXT("ProducingWithCrystal");
		case EProductionStatus::IS_STANDBY: return TEXT("Standby");
		case EProductionStatus::IS_ERROR: return TEXT("Error");
		default: return TEXT("Unknown");
		}
	}

	TArray<FAIModInventoryItemTelemetry> CollectInventoryTelemetry(UFGInventoryComponent* Inventory)
	{
		TArray<FAIModInventoryItemTelemetry> Result;
		if (!IsValid(Inventory))
		{
			return Result;
		}

		TArray<FInventoryStack> Stacks;
		Inventory->GetInventoryStacks(Stacks, /*getEmptyStacks=*/false);

		for (const FInventoryStack& Stack : Stacks)
		{
			const TSubclassOf<UFGItemDescriptor> ItemClass = Stack.Item.GetItemClass();
			if (!ItemClass)
			{
				continue;
			}

			FAIModInventoryItemTelemetry ItemTelemetry;
			ItemTelemetry.ItemClass = ItemClass->GetPathName();
			ItemTelemetry.ItemName = UFGItemDescriptor::GetItemName(ItemClass).ToString();
			ItemTelemetry.Count = Stack.NumItems;
			Result.Add(MoveTemp(ItemTelemetry));
		}
		return Result;
	}

	FAIModManufacturerTelemetry MakeManufacturerTelemetry(AFGBuildableManufacturer* Manufacturer)
	{
		FAIModManufacturerTelemetry Telemetry;
		Telemetry.Id = Manufacturer->GetPathName();
		Telemetry.BuildableClass = Manufacturer->GetClass()->GetPathName();
		Telemetry.Position = Manufacturer->GetActorLocation();

		const TSubclassOf<UFGRecipe> Recipe = Manufacturer->GetCurrentRecipe();
		Telemetry.Recipe = Recipe ? UFGRecipe::GetRecipeName(Recipe).ToString() : FString();

		Telemetry.ClockSpeedPercent = Manufacturer->GetCurrentPotential() * 100.0f;
		Telemetry.ProductionStatus = ProductionStatusToString(Manufacturer->GetProductionIndicatorStatus());
		Telemetry.ProductionProgress = Manufacturer->GetProductionProgress();
		Telemetry.Productivity = Manufacturer->GetProductivity();
		Telemetry.InputInventory = CollectInventoryTelemetry(Manufacturer->GetInputInventory());
		Telemetry.OutputInventory = CollectInventoryTelemetry(Manufacturer->GetOutputInventory());
		return Telemetry;
	}

	TArray<FAIModManufacturerTelemetry> CollectManufacturerTelemetry(UWorld* World)
	{
		TArray<FAIModManufacturerTelemetry> Result;
		for (TActorIterator<AFGBuildableManufacturer> It(World); It; ++It)
		{
			if (IsValid(*It))
			{
				Result.Add(MakeManufacturerTelemetry(*It));
			}
		}
		return Result;
	}

	// EFactoryConnectionDirection IS a real UENUM (unlike EProductionStatus),
	// but a manual switch keeps this consistent with ProductionStatusToString
	// and avoids pulling in StaticEnum<> boilerplate for four values.
	FString FactoryConnectionDirectionToString(EFactoryConnectionDirection Direction)
	{
		switch (Direction)
		{
		case EFactoryConnectionDirection::FCD_INPUT: return TEXT("Input");
		case EFactoryConnectionDirection::FCD_OUTPUT: return TEXT("Output");
		case EFactoryConnectionDirection::FCD_ANY: return TEXT("Any");
		case EFactoryConnectionDirection::FCD_SNAP_ONLY: return TEXT("SnapOnly");
		default: return TEXT("Unknown");
		}
	}

	FAIModFactoryConnectionTelemetry MakeConnectionTelemetry(const FString& OwnerId, UFGFactoryConnectionComponent* Connection)
	{
		FAIModFactoryConnectionTelemetry Telemetry;
		Telemetry.OwnerBuildableId = OwnerId;
		Telemetry.Direction = FactoryConnectionDirectionToString(Connection->GetDirection());
		Telemetry.bConnected = Connection->IsConnected();
		Telemetry.Position = Connection->GetConnectorLocation();
		Telemetry.Normal = Connection->GetConnectorNormal();

		if (Telemetry.bConnected)
		{
			if (const UFGFactoryConnectionComponent* Peer = Connection->GetConnection())
			{
				if (const AFGBuildable* PeerOwner = Cast<AFGBuildable>(Peer->GetOwner()))
				{
					Telemetry.ConnectedBuildableId = PeerOwner->GetPathName();
				}
			}
		}
		return Telemetry;
	}

	TArray<FAIModFactoryConnectionTelemetry> CollectFactoryConnectionTelemetry(UWorld* World)
	{
		TArray<FAIModFactoryConnectionTelemetry> Result;

		// Discover UFGFactoryConnectionComponents generically via
		// AActor::GetComponents<>() rather than maintaining a per-class-
		// hierarchy enumeration list. Found the hard way, twice, live
		// (2026-08-24): AFGBuildableFactory (machines), AFGBuildableConveyorBase
		// (belts/lifts, via named GetConnection0()/GetConnection1()), and
		// AFGBuildableConveyorAttachment (splitters/mergers, via protected
		// mInputs/mOutputs arrays with no public getter at all) are THREE
		// separate sibling hierarchies, none deriving from another, each
		// with its own connection storage and accessor (or none). The
		// first fix (adding AFGBuildableConveyorBase) only dropped the
		// self-test's reciprocity failure from 435/1265 to a still-broken
		// 920/6791 - every remaining unmatched peer was a
		// Build_ConveyorAttachmentMerger/Splitter (confirmed by pulling
		// live world.connections/world.buildables data and cross-
		// referencing peer buildableClass). Generic component discovery
		// is robust against any other sibling hierarchy not yet found,
		// since UFGFactoryConnectionComponent is always a component on
		// the owning AFGBuildable regardless of which subclass it is.
		for (TActorIterator<AFGBuildable> It(World); It; ++It)
		{
			AFGBuildable* Buildable = *It;
			if (!IsValid(Buildable))
			{
				continue;
			}

			TArray<UFGFactoryConnectionComponent*> Connections;
			Buildable->GetComponents<UFGFactoryConnectionComponent>(Connections);
			if (Connections.Num() == 0)
			{
				continue;
			}

			const FString OwnerId = Buildable->GetPathName();
			for (UFGFactoryConnectionComponent* Connection : Connections)
			{
				if (IsValid(Connection))
				{
					Result.Add(MakeConnectionTelemetry(OwnerId, Connection));
				}
			}
		}

		return Result;
	}

	FString PipeConnectionTypeToString(EPipeConnectionType Type)
	{
		switch (Type)
		{
		case EPipeConnectionType::PCT_ANY: return TEXT("Any");
		case EPipeConnectionType::PCT_PRODUCER: return TEXT("Producer");
		case EPipeConnectionType::PCT_CONSUMER: return TEXT("Consumer");
		case EPipeConnectionType::PCT_SNAP_ONLY: return TEXT("SnapOnly");
		default: return TEXT("Unknown");
		}
	}

	FAIModPipeConnectionTelemetry MakePipeConnectionTelemetry(const FString& OwnerId, UFGPipeConnectionComponentBase* Connection)
	{
		FAIModPipeConnectionTelemetry Telemetry;
		Telemetry.OwnerBuildableId = OwnerId;
		Telemetry.ConnectionType = PipeConnectionTypeToString(Connection->GetPipeConnectionType());
		Telemetry.bIsHypertube = Cast<UFGPipeConnectionComponentHyper>(Connection) != nullptr;
		Telemetry.bConnected = Connection->IsConnected();
		Telemetry.Position = Connection->GetConnectorLocation();
		Telemetry.Normal = Connection->GetConnectorNormal();

		if (Telemetry.bConnected)
		{
			if (const UFGPipeConnectionComponentBase* Peer = Connection->GetConnection())
			{
				if (const AFGBuildable* PeerOwner = Cast<AFGBuildable>(Peer->GetOwner()))
				{
					Telemetry.ConnectedBuildableId = PeerOwner->GetPathName();
				}
			}
		}
		return Telemetry;
	}

	// Same generic-discovery pattern as CollectFactoryConnectionTelemetry -
	// see that function's comment for why (three separate sibling
	// hierarchies were found the hard way for factory connections; pipes
	// are a fourth, entirely separate type hierarchy - UFGPipeConnectionComponentBase,
	// not UFGFactoryConnectionComponent - covering both fluid pipes and
	// hypertubes, since UFGPipeConnectionComponentHyper is a subclass of
	// the same base, added 2026-08-27 after discovering live that no
	// telemetry existed for either.
	TArray<FAIModPipeConnectionTelemetry> CollectPipeConnectionTelemetry(UWorld* World)
	{
		TArray<FAIModPipeConnectionTelemetry> Result;

		for (TActorIterator<AFGBuildable> It(World); It; ++It)
		{
			AFGBuildable* Buildable = *It;
			if (!IsValid(Buildable))
			{
				continue;
			}

			TArray<UFGPipeConnectionComponentBase*> Connections;
			Buildable->GetComponents<UFGPipeConnectionComponentBase>(Connections);
			if (Connections.Num() == 0)
			{
				continue;
			}

			const FString OwnerId = Buildable->GetPathName();
			for (UFGPipeConnectionComponentBase* Connection : Connections)
			{
				if (IsValid(Connection))
				{
					Result.Add(MakePipeConnectionTelemetry(OwnerId, Connection));
				}
			}
		}

		return Result;
	}

	// Id is the session-local GetPathName() (see AIModTelemetryTypes.h) -
	// no stable/indexed lookup exists, so resolving one back to an actor
	// means scanning. Fine at this scale (dozens/hundreds of
	// manufacturers per save, and this only runs on an explicit write
	// request, not every frame).
	AFGBuildableManufacturer* FindManufacturerById(UWorld* World, const FString& BuildableId)
	{
		for (TActorIterator<AFGBuildableManufacturer> It(World); It; ++It)
		{
			if (IsValid(*It) && It->GetPathName() == BuildableId)
			{
				return *It;
			}
		}
		return nullptr;
	}
}

FString UAIModFunctionLibrary::GetInterfaceVersion()
{
	UE_LOG(LogAIModAI, Verbose, TEXT("GetInterfaceVersion called"));
	return TEXT("0.1.0");
}

namespace
{
	UConfigManager* GetAIModConfigManager(UObject* WorldContextObject)
	{
		UGameInstance* GameInstance = Cast<UGameInstance>(WorldContextObject);
		if (!GameInstance)
		{
			UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
			GameInstance = World ? World->GetGameInstance() : nullptr;
		}
		return GameInstance ? GameInstance->GetSubsystem<UConfigManager>() : nullptr;
	}

	UConfigPropertySection* GetAIModConfigRootSection(UObject* WorldContextObject)
	{
		UConfigManager* ConfigManager = GetAIModConfigManager(WorldContextObject);
		if (!ConfigManager)
		{
			return nullptr;
		}
		FConfigId ConfigId;
		ConfigId.ModReference = TEXT("AIMod");
		return ConfigManager->GetConfigurationRootSection(ConfigId);
	}
}

bool UAIModFunctionLibrary::GetAIModConfigBool(UObject* WorldContextObject, const FString& PropertyName, bool DefaultValue)
{
	const UConfigPropertySection* Root = GetAIModConfigRootSection(WorldContextObject);
	if (!Root)
	{
		return DefaultValue;
	}
	if (const UConfigPropertyBool* BoolProperty = Cast<UConfigPropertyBool>(Root->SectionProperties.FindRef(PropertyName)))
	{
		return BoolProperty->Value;
	}
	return DefaultValue;
}

float UAIModFunctionLibrary::GetAIModConfigFloat(UObject* WorldContextObject, const FString& PropertyName, float DefaultValue)
{
	const UConfigPropertySection* Root = GetAIModConfigRootSection(WorldContextObject);
	if (!Root)
	{
		return DefaultValue;
	}
	if (const UConfigPropertyFloat* FloatProperty = Cast<UConfigPropertyFloat>(Root->SectionProperties.FindRef(PropertyName)))
	{
		return FloatProperty->Value;
	}
	return DefaultValue;
}

FString UAIModFunctionLibrary::LogGroundHeightAsJson(UObject* WorldContextObject, float X, float Y, float ReferenceZ)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		UE_LOG(LogAIModAI, Warning, TEXT("LogGroundHeightAsJson: no valid world context"));
		return TEXT("{}");
	}

	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
	const float ZSearchCenter = (ReferenceZ > -1000000.0f) ? ReferenceZ : (Character ? Character->GetActorLocation().Z : 0.0f);

	const FGroundTraceResult GroundTrace = FindGroundAtXY(World, X, Y, ZSearchCenter, Character);

	const TSharedRef<FJsonObject> NormalObject = MakeShared<FJsonObject>();
	NormalObject->SetNumberField(TEXT("x"), GroundTrace.Hit.Normal.X);
	NormalObject->SetNumberField(TEXT("y"), GroundTrace.Hit.Normal.Y);
	NormalObject->SetNumberField(TEXT("z"), GroundTrace.Hit.Normal.Z);

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetBoolField(TEXT("found"), GroundTrace.bFound);
	RootObject->SetNumberField(TEXT("x"), X);
	RootObject->SetNumberField(TEXT("y"), Y);
	RootObject->SetNumberField(TEXT("z"), GroundTrace.Hit.Location.Z);
	RootObject->SetObjectField(TEXT("normal"), NormalObject);

	const FString JsonString = WriteCondensedJson(RootObject);

	UE_LOG(LogAIModAI, Display, TEXT("LogGroundHeightAsJson: %s"), *JsonString);

	return JsonString;
}

TArray<FAIModResourceNodeTelemetry> UAIModFunctionLibrary::GetResourceNodeTelemetry(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		UE_LOG(LogAIModAI, Warning, TEXT("GetResourceNodeTelemetry: no valid world context"));
		return {};
	}

	return CollectResourceNodeTelemetry(World);
}

void UAIModFunctionLibrary::LogResourceNodes(UObject* WorldContextObject)
{
	const TArray<FAIModResourceNodeTelemetry> Nodes = GetResourceNodeTelemetry(WorldContextObject);

	for (const FAIModResourceNodeTelemetry& Node : Nodes)
	{
		UE_LOG(LogAIModAI, Display, TEXT("ResourceNode: id=%s resource=\"%s\" purity=%s pos=(%.1f, %.1f, %.1f) occupied=%s"),
			*Node.Id, *Node.Resource, *Node.Purity, Node.Position.X, Node.Position.Y, Node.Position.Z,
			Node.bOccupied ? TEXT("true") : TEXT("false"));
	}

	UE_LOG(LogAIModAI, Display, TEXT("LogResourceNodes: enumerated %d resource node(s)"), Nodes.Num());
}

FString UAIModFunctionLibrary::LogResourceNodesAsJson(UObject* WorldContextObject)
{
	const TArray<FAIModResourceNodeTelemetry> Nodes = GetResourceNodeTelemetry(WorldContextObject);

	TArray<TSharedPtr<FJsonValue>> NodeJsonArray;
	NodeJsonArray.Reserve(Nodes.Num());

	for (const FAIModResourceNodeTelemetry& Node : Nodes)
	{
		const TSharedRef<FJsonObject> NodeObject = MakeShared<FJsonObject>();
		NodeObject->SetStringField(TEXT("id"), Node.Id);
		NodeObject->SetStringField(TEXT("resource"), Node.Resource);
		NodeObject->SetStringField(TEXT("resourceClass"), Node.ResourceClass);
		NodeObject->SetStringField(TEXT("purity"), Node.Purity);
		NodeObject->SetStringField(TEXT("nodeType"), Node.NodeType);
		NodeObject->SetStringField(TEXT("coreId"), Node.CoreId);
		NodeObject->SetStringField(TEXT("satelliteState"), Node.SatelliteState);

		const TSharedRef<FJsonObject> PositionObject = MakeShared<FJsonObject>();
		PositionObject->SetNumberField(TEXT("x"), Node.Position.X);
		PositionObject->SetNumberField(TEXT("y"), Node.Position.Y);
		PositionObject->SetNumberField(TEXT("z"), Node.Position.Z);
		NodeObject->SetObjectField(TEXT("position"), PositionObject);

		NodeObject->SetBoolField(TEXT("occupied"), Node.bOccupied);

		NodeJsonArray.Add(MakeShared<FJsonValueObject>(NodeObject));
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetArrayField(TEXT("resourceNodes"), NodeJsonArray);

	FString JsonString;
	// Condensed, not the default TPrettyJsonPrintPolicy - a real save's
	// resourceNodes payload pretty-printed to over 8000 log lines for one
	// call (631 nodes), confirmed against an actual FactoryGame.log.
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonString);
	FJsonSerializer::Serialize(RootObject, Writer);

	UE_LOG(LogAIModAI, Display, TEXT("LogResourceNodesAsJson: %s"), *JsonString);

	return JsonString;
}

TArray<FAIModBuildableTelemetry> UAIModFunctionLibrary::GetBuildableTelemetry(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		UE_LOG(LogAIModAI, Warning, TEXT("GetBuildableTelemetry: no valid world context"));
		return {};
	}

	return CollectBuildableTelemetry(World);
}

void UAIModFunctionLibrary::LogBuildables(UObject* WorldContextObject)
{
	const TArray<FAIModBuildableTelemetry> Buildables = GetBuildableTelemetry(WorldContextObject);

	for (const FAIModBuildableTelemetry& Buildable : Buildables)
	{
		UE_LOG(LogAIModAI, Display, TEXT("Buildable: id=%s class=%s pos=(%.1f, %.1f, %.1f) rot=(%.1f, %.1f, %.1f)"),
			*Buildable.Id, *Buildable.BuildableClass, Buildable.Position.X, Buildable.Position.Y, Buildable.Position.Z,
			Buildable.Rotation.Pitch, Buildable.Rotation.Yaw, Buildable.Rotation.Roll);
	}

	UE_LOG(LogAIModAI, Display, TEXT("LogBuildables: enumerated %d buildable(s)"), Buildables.Num());
}

FString UAIModFunctionLibrary::LogBuildablesAsJson(UObject* WorldContextObject)
{
	const TArray<FAIModBuildableTelemetry> Buildables = GetBuildableTelemetry(WorldContextObject);

	TArray<TSharedPtr<FJsonValue>> BuildableJsonArray;
	BuildableJsonArray.Reserve(Buildables.Num());

	for (const FAIModBuildableTelemetry& Buildable : Buildables)
	{
		const TSharedRef<FJsonObject> BuildableObject = MakeShared<FJsonObject>();
		BuildableObject->SetStringField(TEXT("id"), Buildable.Id);
		BuildableObject->SetStringField(TEXT("buildableClass"), Buildable.BuildableClass);

		const TSharedRef<FJsonObject> PositionObject = MakeShared<FJsonObject>();
		PositionObject->SetNumberField(TEXT("x"), Buildable.Position.X);
		PositionObject->SetNumberField(TEXT("y"), Buildable.Position.Y);
		PositionObject->SetNumberField(TEXT("z"), Buildable.Position.Z);
		BuildableObject->SetObjectField(TEXT("position"), PositionObject);

		const TSharedRef<FJsonObject> RotationObject = MakeShared<FJsonObject>();
		RotationObject->SetNumberField(TEXT("pitch"), Buildable.Rotation.Pitch);
		RotationObject->SetNumberField(TEXT("yaw"), Buildable.Rotation.Yaw);
		RotationObject->SetNumberField(TEXT("roll"), Buildable.Rotation.Roll);
		BuildableObject->SetObjectField(TEXT("rotation"), RotationObject);

		BuildableJsonArray.Add(MakeShared<FJsonValueObject>(BuildableObject));
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetArrayField(TEXT("buildables"), BuildableJsonArray);

	FString JsonString;
	// Condensed, not the default TPrettyJsonPrintPolicy - a real save's
	// resourceNodes payload pretty-printed to over 8000 log lines for one
	// call (631 nodes), confirmed against an actual FactoryGame.log.
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonString);
	FJsonSerializer::Serialize(RootObject, Writer);

	UE_LOG(LogAIModAI, Display, TEXT("LogBuildablesAsJson: %s"), *JsonString);

	return JsonString;
}

TArray<FAIModManufacturerTelemetry> UAIModFunctionLibrary::GetManufacturerTelemetry(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		UE_LOG(LogAIModAI, Warning, TEXT("GetManufacturerTelemetry: no valid world context"));
		return {};
	}

	return CollectManufacturerTelemetry(World);
}

void UAIModFunctionLibrary::LogManufacturers(UObject* WorldContextObject)
{
	const TArray<FAIModManufacturerTelemetry> Manufacturers = GetManufacturerTelemetry(WorldContextObject);

	for (const FAIModManufacturerTelemetry& Manufacturer : Manufacturers)
	{
		UE_LOG(LogAIModAI, Display,
			TEXT("Manufacturer: id=%s class=%s recipe=\"%s\" clock=%.0f%% status=%s progress=%.2f productivity=%.2f inputItems=%d outputItems=%d"),
			*Manufacturer.Id, *Manufacturer.BuildableClass, *Manufacturer.Recipe, Manufacturer.ClockSpeedPercent,
			*Manufacturer.ProductionStatus, Manufacturer.ProductionProgress, Manufacturer.Productivity,
			Manufacturer.InputInventory.Num(), Manufacturer.OutputInventory.Num());
	}

	UE_LOG(LogAIModAI, Display, TEXT("LogManufacturers: enumerated %d manufacturer(s)"), Manufacturers.Num());
}

namespace
{
	TSharedRef<FJsonObject> InventoryItemToJson(const FAIModInventoryItemTelemetry& Item)
	{
		const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("itemClass"), Item.ItemClass);
		Object->SetStringField(TEXT("itemName"), Item.ItemName);
		Object->SetNumberField(TEXT("count"), Item.Count);
		return Object;
	}

	TArray<TSharedPtr<FJsonValue>> InventoryToJsonArray(const TArray<FAIModInventoryItemTelemetry>& Items)
	{
		TArray<TSharedPtr<FJsonValue>> Array;
		Array.Reserve(Items.Num());
		for (const FAIModInventoryItemTelemetry& Item : Items)
		{
			Array.Add(MakeShared<FJsonValueObject>(InventoryItemToJson(Item)));
		}
		return Array;
	}

	TSharedRef<FJsonObject> ManufacturerToJson(const FAIModManufacturerTelemetry& Manufacturer)
	{
		const TSharedRef<FJsonObject> ManufacturerObject = MakeShared<FJsonObject>();
		ManufacturerObject->SetStringField(TEXT("id"), Manufacturer.Id);
		ManufacturerObject->SetStringField(TEXT("buildableClass"), Manufacturer.BuildableClass);

		const TSharedRef<FJsonObject> PositionObject = MakeShared<FJsonObject>();
		PositionObject->SetNumberField(TEXT("x"), Manufacturer.Position.X);
		PositionObject->SetNumberField(TEXT("y"), Manufacturer.Position.Y);
		PositionObject->SetNumberField(TEXT("z"), Manufacturer.Position.Z);
		ManufacturerObject->SetObjectField(TEXT("position"), PositionObject);

		ManufacturerObject->SetStringField(TEXT("recipe"), Manufacturer.Recipe);
		ManufacturerObject->SetNumberField(TEXT("clockSpeedPercent"), Manufacturer.ClockSpeedPercent);
		ManufacturerObject->SetStringField(TEXT("productionStatus"), Manufacturer.ProductionStatus);
		ManufacturerObject->SetNumberField(TEXT("productionProgress"), Manufacturer.ProductionProgress);
		ManufacturerObject->SetNumberField(TEXT("productivity"), Manufacturer.Productivity);
		ManufacturerObject->SetArrayField(TEXT("inputInventory"), InventoryToJsonArray(Manufacturer.InputInventory));
		ManufacturerObject->SetArrayField(TEXT("outputInventory"), InventoryToJsonArray(Manufacturer.OutputInventory));
		return ManufacturerObject;
	}

	FString WriteCondensedJson(const TSharedRef<FJsonObject>& RootObject)
	{
		FString JsonString;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonString);
		FJsonSerializer::Serialize(RootObject, Writer);
		return JsonString;
	}
}

FString UAIModFunctionLibrary::LogManufacturersAsJson(UObject* WorldContextObject)
{
	const TArray<FAIModManufacturerTelemetry> Manufacturers = GetManufacturerTelemetry(WorldContextObject);

	TArray<TSharedPtr<FJsonValue>> ManufacturerJsonArray;
	ManufacturerJsonArray.Reserve(Manufacturers.Num());
	for (const FAIModManufacturerTelemetry& Manufacturer : Manufacturers)
	{
		ManufacturerJsonArray.Add(MakeShared<FJsonValueObject>(ManufacturerToJson(Manufacturer)));
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetArrayField(TEXT("manufacturers"), ManufacturerJsonArray);

	// Condensed, not the default TPrettyJsonPrintPolicy - a real save's
	// resourceNodes payload pretty-printed to over 8000 log lines for one
	// call (631 nodes), confirmed against an actual FactoryGame.log.
	const FString JsonString = WriteCondensedJson(RootObject);

	UE_LOG(LogAIModAI, Display, TEXT("LogManufacturersAsJson: %s"), *JsonString);

	return JsonString;
}

FString UAIModFunctionLibrary::LogTargetedManufacturerAsJson(UObject* WorldContextObject)
{
	const FAIModManufacturerTelemetry Manufacturer = GetTargetedManufacturer(WorldContextObject);

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	if (Manufacturer.Id.IsEmpty())
	{
		RootObject->SetField(TEXT("manufacturer"), MakeShared<FJsonValueNull>());
	}
	else
	{
		RootObject->SetObjectField(TEXT("manufacturer"), ManufacturerToJson(Manufacturer));
	}

	const FString JsonString = WriteCondensedJson(RootObject);

	UE_LOG(LogAIModAI, Display, TEXT("LogTargetedManufacturerAsJson: %s"), *JsonString);

	return JsonString;
}

TArray<FAIModFactoryConnectionTelemetry> UAIModFunctionLibrary::GetFactoryConnectionTelemetry(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		UE_LOG(LogAIModAI, Warning, TEXT("GetFactoryConnectionTelemetry: no valid world context"));
		return {};
	}

	return CollectFactoryConnectionTelemetry(World);
}

void UAIModFunctionLibrary::LogFactoryConnections(UObject* WorldContextObject)
{
	const TArray<FAIModFactoryConnectionTelemetry> Connections = GetFactoryConnectionTelemetry(WorldContextObject);

	for (const FAIModFactoryConnectionTelemetry& Connection : Connections)
	{
		UE_LOG(LogAIModAI, Display, TEXT("Connection: owner=%s direction=%s connected=%s connectedTo=%s position=%s normal=%s"),
			*Connection.OwnerBuildableId, *Connection.Direction, Connection.bConnected ? TEXT("true") : TEXT("false"),
			*Connection.ConnectedBuildableId, *Connection.Position.ToString(), *Connection.Normal.ToString());
	}

	UE_LOG(LogAIModAI, Display, TEXT("LogFactoryConnections: enumerated %d connection point(s)"), Connections.Num());
}

FString UAIModFunctionLibrary::LogFactoryConnectionsAsJson(UObject* WorldContextObject)
{
	const TArray<FAIModFactoryConnectionTelemetry> Connections = GetFactoryConnectionTelemetry(WorldContextObject);

	TArray<TSharedPtr<FJsonValue>> ConnectionJsonArray;
	ConnectionJsonArray.Reserve(Connections.Num());

	for (const FAIModFactoryConnectionTelemetry& Connection : Connections)
	{
		const TSharedRef<FJsonObject> ConnectionObject = MakeShared<FJsonObject>();
		ConnectionObject->SetStringField(TEXT("ownerBuildableId"), Connection.OwnerBuildableId);
		ConnectionObject->SetStringField(TEXT("direction"), Connection.Direction);
		ConnectionObject->SetBoolField(TEXT("connected"), Connection.bConnected);
		ConnectionObject->SetStringField(TEXT("connectedBuildableId"), Connection.ConnectedBuildableId);

		const TSharedRef<FJsonObject> PositionObject = MakeShared<FJsonObject>();
		PositionObject->SetNumberField(TEXT("x"), Connection.Position.X);
		PositionObject->SetNumberField(TEXT("y"), Connection.Position.Y);
		PositionObject->SetNumberField(TEXT("z"), Connection.Position.Z);
		ConnectionObject->SetObjectField(TEXT("position"), PositionObject);

		const TSharedRef<FJsonObject> NormalObject = MakeShared<FJsonObject>();
		NormalObject->SetNumberField(TEXT("x"), Connection.Normal.X);
		NormalObject->SetNumberField(TEXT("y"), Connection.Normal.Y);
		NormalObject->SetNumberField(TEXT("z"), Connection.Normal.Z);
		ConnectionObject->SetObjectField(TEXT("normal"), NormalObject);

		ConnectionJsonArray.Add(MakeShared<FJsonValueObject>(ConnectionObject));
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetArrayField(TEXT("connections"), ConnectionJsonArray);

	FString JsonString;
	// Condensed, not the default TPrettyJsonPrintPolicy - a real save's
	// resourceNodes payload pretty-printed to over 8000 log lines for one
	// call (631 nodes), confirmed against an actual FactoryGame.log.
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonString);
	FJsonSerializer::Serialize(RootObject, Writer);

	UE_LOG(LogAIModAI, Display, TEXT("LogFactoryConnectionsAsJson: %s"), *JsonString);

	return JsonString;
}

TArray<FAIModPipeConnectionTelemetry> UAIModFunctionLibrary::GetPipeConnectionTelemetry(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		UE_LOG(LogAIModAI, Warning, TEXT("GetPipeConnectionTelemetry: no valid world context"));
		return {};
	}

	return CollectPipeConnectionTelemetry(World);
}

void UAIModFunctionLibrary::LogPipeConnections(UObject* WorldContextObject)
{
	const TArray<FAIModPipeConnectionTelemetry> Connections = GetPipeConnectionTelemetry(WorldContextObject);

	for (const FAIModPipeConnectionTelemetry& Connection : Connections)
	{
		UE_LOG(LogAIModAI, Display, TEXT("PipeConnection: owner=%s type=%s isHypertube=%s connected=%s connectedTo=%s position=%s normal=%s"),
			*Connection.OwnerBuildableId, *Connection.ConnectionType, Connection.bIsHypertube ? TEXT("true") : TEXT("false"),
			Connection.bConnected ? TEXT("true") : TEXT("false"), *Connection.ConnectedBuildableId, *Connection.Position.ToString(), *Connection.Normal.ToString());
	}

	UE_LOG(LogAIModAI, Display, TEXT("LogPipeConnections: enumerated %d connection point(s)"), Connections.Num());
}

FString UAIModFunctionLibrary::LogPipeConnectionsAsJson(UObject* WorldContextObject)
{
	const TArray<FAIModPipeConnectionTelemetry> Connections = GetPipeConnectionTelemetry(WorldContextObject);

	TArray<TSharedPtr<FJsonValue>> ConnectionJsonArray;
	ConnectionJsonArray.Reserve(Connections.Num());

	for (const FAIModPipeConnectionTelemetry& Connection : Connections)
	{
		const TSharedRef<FJsonObject> ConnectionObject = MakeShared<FJsonObject>();
		ConnectionObject->SetStringField(TEXT("ownerBuildableId"), Connection.OwnerBuildableId);
		ConnectionObject->SetStringField(TEXT("connectionType"), Connection.ConnectionType);
		ConnectionObject->SetBoolField(TEXT("isHypertube"), Connection.bIsHypertube);
		ConnectionObject->SetBoolField(TEXT("connected"), Connection.bConnected);
		ConnectionObject->SetStringField(TEXT("connectedBuildableId"), Connection.ConnectedBuildableId);

		const TSharedRef<FJsonObject> PositionObject = MakeShared<FJsonObject>();
		PositionObject->SetNumberField(TEXT("x"), Connection.Position.X);
		PositionObject->SetNumberField(TEXT("y"), Connection.Position.Y);
		PositionObject->SetNumberField(TEXT("z"), Connection.Position.Z);
		ConnectionObject->SetObjectField(TEXT("position"), PositionObject);

		const TSharedRef<FJsonObject> NormalObject = MakeShared<FJsonObject>();
		NormalObject->SetNumberField(TEXT("x"), Connection.Normal.X);
		NormalObject->SetNumberField(TEXT("y"), Connection.Normal.Y);
		NormalObject->SetNumberField(TEXT("z"), Connection.Normal.Z);
		ConnectionObject->SetObjectField(TEXT("normal"), NormalObject);

		ConnectionJsonArray.Add(MakeShared<FJsonValueObject>(ConnectionObject));
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetArrayField(TEXT("connections"), ConnectionJsonArray);

	const FString JsonString = WriteCondensedJson(RootObject);

	UE_LOG(LogAIModAI, Display, TEXT("LogPipeConnectionsAsJson: %s"), *JsonString);

	return JsonString;
}

FAIModPlayerTelemetry UAIModFunctionLibrary::GetPlayerTelemetry(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		UE_LOG(LogAIModAI, Warning, TEXT("GetPlayerTelemetry: no valid world context"));
		return FAIModPlayerTelemetry();
	}

	// Player index 0 only - single-player/local session scope, per
	// PLAN.md/CLAUDE.md's multiplayer stance (same as GetTargetedManufacturer).
	const AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Character)
	{
		UE_LOG(LogAIModAI, Warning, TEXT("GetPlayerTelemetry: no local AFGCharacterPlayer (player index 0)"));
		return FAIModPlayerTelemetry();
	}

	FAIModPlayerTelemetry Telemetry;
	Telemetry.Position = Character->GetActorLocation();
	Telemetry.Rotation = Character->GetActorRotation();
	return Telemetry;
}

FString UAIModFunctionLibrary::LogPlayerAsJson(UObject* WorldContextObject)
{
	const FAIModPlayerTelemetry Player = GetPlayerTelemetry(WorldContextObject);

	const TSharedRef<FJsonObject> PositionObject = MakeShared<FJsonObject>();
	PositionObject->SetNumberField(TEXT("x"), Player.Position.X);
	PositionObject->SetNumberField(TEXT("y"), Player.Position.Y);
	PositionObject->SetNumberField(TEXT("z"), Player.Position.Z);

	const TSharedRef<FJsonObject> RotationObject = MakeShared<FJsonObject>();
	RotationObject->SetNumberField(TEXT("pitch"), Player.Rotation.Pitch);
	RotationObject->SetNumberField(TEXT("yaw"), Player.Rotation.Yaw);
	RotationObject->SetNumberField(TEXT("roll"), Player.Rotation.Roll);

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetObjectField(TEXT("position"), PositionObject);
	RootObject->SetObjectField(TEXT("rotation"), RotationObject);

	const FString JsonString = WriteCondensedJson(RootObject);

	UE_LOG(LogAIModAI, Display, TEXT("LogPlayerAsJson: %s"), *JsonString);

	return JsonString;
}

FString UAIModFunctionLibrary::LogTimeOfDayAsJson(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	AFGTimeOfDaySubsystem* TimeSubsystem = World ? AFGTimeOfDaySubsystem::Get(World) : nullptr;
	if (!TimeSubsystem)
	{
		UE_LOG(LogAIModAI, Warning, TEXT("LogTimeOfDayAsJson: no valid world context or time subsystem"));
		return TEXT("{}");
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetNumberField(TEXT("hour"), TimeSubsystem->GetHours());
	RootObject->SetNumberField(TEXT("minute"), TimeSubsystem->GetMinutes());
	RootObject->SetNumberField(TEXT("daySeconds"), TimeSubsystem->GetDaySeconds());
	RootObject->SetBoolField(TEXT("isDay"), TimeSubsystem->IsDay());

	const FString JsonString = WriteCondensedJson(RootObject);

	UE_LOG(LogAIModAI, Display, TEXT("LogTimeOfDayAsJson: %s"), *JsonString);

	return JsonString;
}

FAIModOperationResult UAIModFunctionLibrary::SetTimeOfDay(UObject* WorldContextObject, int32 Hour, int32 Minute)
{
	if (Hour < 0 || Hour > 23)
	{
		return FAIModOperationResult::Failure(TEXT("INVALID_REQUEST"),
			FString::Printf(TEXT("hour %d is outside the valid range [0, 23]"), Hour));
	}
	if (Minute < 0 || Minute > 59)
	{
		return FAIModOperationResult::Failure(TEXT("INVALID_REQUEST"),
			FString::Printf(TEXT("minute %d is outside the valid range [0, 59]"), Minute));
	}

	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}

	AFGTimeOfDaySubsystem* TimeSubsystem = AFGTimeOfDaySubsystem::Get(World);
	if (!TimeSubsystem)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No AFGTimeOfDaySubsystem found in this world"));
	}

	const float DaySeconds = Hour * AFGTimeOfDaySubsystem::SECONDS_PER_HOUR + Minute * AFGTimeOfDaySubsystem::SECONDS_PER_MINUTE;
	TimeSubsystem->SetDaySeconds(DaySeconds);
	TimeSubsystem->ForceReplicateTimeToClients();

	UE_LOG(LogAIModAI, Display, TEXT("SetTimeOfDay: %02d:%02d (daySeconds=%.0f)"), Hour, Minute, DaySeconds);

	return FAIModOperationResult::Success();
}

namespace
{
	FString ChatMessageTypeToString(EFGChatMessageType Type)
	{
		switch (Type)
		{
		case EFGChatMessageType::CMT_PlayerMessage: return TEXT("PlayerMessage");
		case EFGChatMessageType::CMT_SystemMessage: return TEXT("SystemMessage");
		case EFGChatMessageType::CMT_AdaMessage: return TEXT("AdaMessage");
		case EFGChatMessageType::CMT_CustomMessage: return TEXT("CustomMessage");
		default: return TEXT("Unknown");
		}
	}
}

FString UAIModFunctionLibrary::LogChatHistoryAsJson(UObject* WorldContextObject)
{
	AFGChatManager* ChatManager = AFGChatManager::Get(WorldContextObject);
	if (!ChatManager)
	{
		UE_LOG(LogAIModAI, Warning, TEXT("LogChatHistoryAsJson: no AFGChatManager found (too early in level load?)"));
		return TEXT("{}");
	}

	TArray<FChatMessageStruct> Messages;
	ChatManager->GetReceivedChatMessages(Messages);

	TArray<TSharedPtr<FJsonValue>> MessageArray;
	for (const FChatMessageStruct& Message : Messages)
	{
		const TSharedRef<FJsonObject> MessageObject = MakeShared<FJsonObject>();
		MessageObject->SetStringField(TEXT("sender"), Message.MessageSender.ToString());
		MessageObject->SetStringField(TEXT("text"), Message.MessageText.ToString());
		MessageObject->SetStringField(TEXT("type"), ChatMessageTypeToString(Message.MessageType));
		MessageObject->SetNumberField(TEXT("timestamp"), Message.ServerTimeStamp);
		MessageObject->SetBoolField(TEXT("isLocalPlayerMessage"), Message.bIsLocalPlayerMessage);
		MessageArray.Add(MakeShared<FJsonValueObject>(MessageObject));
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetArrayField(TEXT("messages"), MessageArray);

	const FString JsonString = WriteCondensedJson(RootObject);

	UE_LOG(LogAIModAI, Verbose, TEXT("LogChatHistoryAsJson: %d message(s)"), Messages.Num());

	return JsonString;
}

FAIModOperationResult UAIModFunctionLibrary::SendChatMessage(UObject* WorldContextObject, const FString& Message, const FString& Sender)
{
	if (Message.IsEmpty())
	{
		return FAIModOperationResult::Failure(TEXT("INVALID_REQUEST"), TEXT("Message must not be empty"));
	}

	AFGChatManager* ChatManager = AFGChatManager::Get(WorldContextObject);
	if (!ChatManager)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No AFGChatManager found (too early in level load?)"));
	}

	FChatMessageStruct ChatMessage;
	ChatMessage.MessageText = FText::FromString(Message);
	ChatMessage.MessageType = EFGChatMessageType::CMT_CustomMessage;
	ChatMessage.MessageSender = FText::FromString(Sender.IsEmpty() ? TEXT("AIMod AI") : Sender);
	ChatMessage.MessageSenderColor = FLinearColor(0.2f, 0.8f, 1.0f);

	// AddChatMessageToReceived's own doc comment: "Helper function to add
	// a chat message to the LOCAL received messages" - silent bookkeeping
	// only, queryable via GetReceivedChatMessages/world.chatHistory but
	// confirmed live (2026-08-28) NOT visible in the actual in-game chat
	// UI. BroadcastChatMessage ("Broadcasts a chat message to all
	// connected players") is the real public entry point - it calls the
	// NetMulticast Multicast_BroadcastChatMessage internally, which is
	// almost certainly what actually drives the on-screen chat widget
	// for a normal player-typed message too.
	ChatManager->BroadcastChatMessage(ChatMessage, nullptr);

	UE_LOG(LogAIModAI, Display, TEXT("SendChatMessage: [%s] %s"), *ChatMessage.MessageSender.ToString(), *Message);

	return FAIModOperationResult::Success();
}

FAIModManufacturerTelemetry UAIModFunctionLibrary::GetTargetedManufacturer(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		UE_LOG(LogAIModAI, Warning, TEXT("GetTargetedManufacturer: no valid world context"));
		return FAIModManufacturerTelemetry();
	}

	// Player index 0 only - single-player/local session scope, per
	// PLAN.md/CLAUDE.md's multiplayer stance.
	const AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Character)
	{
		UE_LOG(LogAIModAI, Warning, TEXT("GetTargetedManufacturer: no local AFGCharacterPlayer (player index 0)"));
		return FAIModManufacturerTelemetry();
	}

	// GetBestUsableActor() is the game's own "what am I looking at / can
	// interact with" state (drives the "Press E to interact" prompt) -
	// not a reimplemented line trace.
	AFGBuildableManufacturer* Manufacturer = Cast<AFGBuildableManufacturer>(Character->GetBestUsableActor());
	if (!Manufacturer)
	{
		// Not an error - the player just isn't looking at a manufacturer
		// right now. Empty Id signals "none" to the caller.
		return FAIModManufacturerTelemetry();
	}

	return MakeManufacturerTelemetry(Manufacturer);
}

FAIModOperationResult UAIModFunctionLibrary::SetManufacturerClockSpeed(UObject* WorldContextObject, const FString& BuildableId, float ClockSpeedPercent)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}

	AFGBuildableManufacturer* Manufacturer = FindManufacturerById(World, BuildableId);
	if (!Manufacturer)
	{
		return FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"),
			FString::Printf(TEXT("No manufacturer found with id '%s'"), *BuildableId));
	}

	if (!Manufacturer->GetCanChangePotential())
	{
		return FAIModOperationResult::Failure(TEXT("OPERATION_NOT_PERMITTED"),
			TEXT("This building does not allow changing clock speed"));
	}

	const float RequestedPotential = ClockSpeedPercent / 100.0f;
	const float MinPotential = Manufacturer->GetCurrentMinPotential();
	const float MaxPotential = Manufacturer->GetCurrentMaxPotential();
	if (RequestedPotential < MinPotential || RequestedPotential > MaxPotential)
	{
		return FAIModOperationResult::Failure(TEXT("INVALID_CLOCK_SPEED"),
			FString::Printf(TEXT("clockSpeedPercent %.1f is outside the valid range [%.1f, %.1f]"),
				ClockSpeedPercent, MinPotential * 100.0f, MaxPotential * 100.0f));
	}

	// Takes effect at the next production cycle, not instantly - see
	// AFGBuildableFactory::SetPendingPotential's doc comment.
	Manufacturer->SetPendingPotential(RequestedPotential);

	UE_LOG(LogAIModAI, Display, TEXT("SetManufacturerClockSpeed: %s -> %.1f%% (pending)"), *BuildableId, ClockSpeedPercent);

	return FAIModOperationResult::Success();
}

FAIModOperationResult UAIModFunctionLibrary::SetManufacturerRecipe(UObject* WorldContextObject, const FString& BuildableId, const FString& RecipeClassPath)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}

	AFGBuildableManufacturer* Manufacturer = FindManufacturerById(World, BuildableId);
	if (!Manufacturer)
	{
		return FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"),
			FString::Printf(TEXT("No manufacturer found with id '%s'"), *BuildableId));
	}

	// Resolve the path to a class and require it to actually be a
	// UFGRecipe subclass before doing anything else with it. This is
	// deliberately narrow - not a generic "load any class by path"
	// capability - per CLAUDE.md's Safety and Stability Boundary.
	UClass* ResolvedClass = LoadObject<UClass>(nullptr, *RecipeClassPath);
	if (!ResolvedClass || !ResolvedClass->IsChildOf(UFGRecipe::StaticClass()))
	{
		return FAIModOperationResult::Failure(TEXT("INVALID_RECIPE"),
			FString::Printf(TEXT("'%s' did not resolve to a UFGRecipe subclass"), *RecipeClassPath));
	}
	const TSubclassOf<UFGRecipe> RecipeClass = ResolvedClass;

	if (!UFGRecipe::IsProducedIn(RecipeClass, Manufacturer->GetClass()))
	{
		return FAIModOperationResult::Failure(TEXT("RECIPE_NOT_COMPATIBLE"),
			FString::Printf(TEXT("Recipe '%s' is not producible in '%s'"), *RecipeClassPath, *Manufacturer->GetClass()->GetPathName()));
	}

	// FGBuildableManufacturer::SetRecipe's own doc comment: "It is up to
	// the caller to make sure input and output inventories are empty
	// before changing recipe." Enforce it rather than trusting the
	// caller/engine to handle a non-empty swap gracefully.
	UFGInventoryComponent* InputInventory = Manufacturer->GetInputInventory();
	UFGInventoryComponent* OutputInventory = Manufacturer->GetOutputInventory();
	const bool bInputEmpty = !InputInventory || InputInventory->IsEmpty();
	const bool bOutputEmpty = !OutputInventory || OutputInventory->IsEmpty();
	if (!bInputEmpty || !bOutputEmpty)
	{
		return FAIModOperationResult::Failure(TEXT("INVENTORY_NOT_EMPTY"),
			TEXT("Input and output inventories must be empty before changing recipe"));
	}

	Manufacturer->SetRecipe(RecipeClass);

	UE_LOG(LogAIModAI, Display, TEXT("SetManufacturerRecipe: %s -> %s"), *BuildableId, *RecipeClassPath);

	return FAIModOperationResult::Success();
}

FAIModResourceNodeTelemetry UAIModFunctionLibrary::GetTargetedResourceNode(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		UE_LOG(LogAIModAI, Warning, TEXT("GetTargetedResourceNode: no valid world context"));
		return FAIModResourceNodeTelemetry();
	}

	// Player index 0 only - single-player/local session scope, per
	// PLAN.md/CLAUDE.md's multiplayer stance (same as GetTargetedManufacturer).
	const AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Character)
	{
		UE_LOG(LogAIModAI, Warning, TEXT("GetTargetedResourceNode: no local AFGCharacterPlayer (player index 0)"));
		return FAIModResourceNodeTelemetry();
	}

	// Corrected 2026-08-24: an earlier pass here used a hand-rolled
	// view-angle-cone heuristic based on a research gap - searched for
	// "IFGUsableInterface" (zero hits) and concluded resource nodes have
	// no usable interface at all. FactoryGame actually spells it
	// "IFGUseableInterface", and AFGResourceNodeBase (FGResourceNodeBase.h:93)
	// does implement it - confirmed live: the same GetBestUsableActor()
	// GetTargetedManufacturer already trusts also finds resource nodes,
	// exactly matching the game's own "Press E to start mining" prompt.
	AFGResourceNode* Node = Cast<AFGResourceNode>(Character->GetBestUsableActor());
	if (!Node)
	{
		// Not an error - the player just isn't looking at a resource
		// node right now. Empty Id signals "none" to the caller.
		return FAIModResourceNodeTelemetry();
	}

	return MakeResourceNodeTelemetry(Node);
}

FAIModOperationResult UAIModFunctionLibrary::DebugCheckExtractorPlacementOnTargetedNode(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}

	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Character)
	{
		return FAIModOperationResult::Failure(TEXT("NO_PLAYER"), TEXT("No local AFGCharacterPlayer (player index 0)"));
	}

	const FAIModResourceNodeTelemetry TargetTelemetry = GetTargetedResourceNode(WorldContextObject);
	if (TargetTelemetry.Id.IsEmpty())
	{
		return FAIModOperationResult::Failure(TEXT("NO_TARGET_NODE"), TEXT("Not currently looking at a resource node"));
	}

	// GetTargetedResourceNode returns normalized telemetry, not a
	// pointer (CLAUDE.md: no raw AActor*/UObject* across the
	// external-facing data model) - re-find the actual actor by id.
	AFGResourceNode* TargetNode = nullptr;
	for (TActorIterator<AFGResourceNode> It(World); It; ++It)
	{
		if (IsValid(*It) && It->GetPathName() == TargetTelemetry.Id)
		{
			TargetNode = *It;
			break;
		}
	}
	if (!TargetNode)
	{
		return FAIModOperationResult::Failure(TEXT("NODE_NOT_FOUND"), TEXT("Targeted node id could not be re-resolved"));
	}

	if (TargetNode->IsOccupied())
	{
		return FAIModOperationResult::Failure(TEXT("NODE_OCCUPIED"), TEXT("Targeted node is already occupied"));
	}

	// Scoped to solid-resource extraction (Miner Mk1) only for this first
	// experiment - see this function's header comment. Liquid/gas nodes
	// need a different buildable/recipe (Water/Oil Extractor) and are
	// deliberately out of scope here.
	const EResourceForm Form = UFGItemDescriptor::GetForm(TargetNode->GetResourceClass());
	if (Form != EResourceForm::RF_SOLID)
	{
		return FAIModOperationResult::Failure(TEXT("UNSUPPORTED_RESOURCE_FORM"),
			TEXT("This experiment only supports solid resource nodes (Miner Mk1) so far"));
	}

	// Diagnostic: IFGExtractableResourceInterface (FGExtractableResourceInterface.h,
	// which AFGResourceNodeBase implements - same interface IsOccupied()
	// above already calls) exposes a node-level "can an extractor even go
	// here" check, independent of the hologram's own clearance system
	// (already confirmed working - see the clearanceDetector diagnostic
	// added earlier). If this reports false, the real answer is here, not
	// in anything hologram-related.
	UE_LOG(LogAIModAI, Display, TEXT("DebugCheckExtractorPlacementOnTargetedNode: node diagnostics - CanPlaceResourceExtractor=%s HasAnyResources=%s CanBecomeOccupied=%s"),
		TargetNode->CanPlaceResourceExtractor() ? TEXT("true") : TEXT("false"),
		TargetNode->HasAnyResources() ? TEXT("true") : TEXT("false"),
		TargetNode->CanBecomeOccupied() ? TEXT("true") : TEXT("false"));

	// Verified to exist as a real asset in Content/FactoryGame/Recipes/Buildings/
	// (not guessed from memory - see docs/extractor-placement-research.md).
	// This is the building's BUILD-COST recipe (what it costs to
	// construct), not a production recipe - Miner Mk1 has no production
	// recipe, it extracts automatically based on the node's purity.
	UClass* MinerRecipeClass = LoadObject<UClass>(nullptr, TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_MinerMk1.Recipe_MinerMk1_C"));
	if (!MinerRecipeClass || !MinerRecipeClass->IsChildOf(UFGRecipe::StaticClass()))
	{
		return FAIModOperationResult::Failure(TEXT("RECIPE_LOAD_FAILED"), TEXT("Failed to load Recipe_MinerMk1 as a UFGRecipe"));
	}
	const TSubclassOf<UFGRecipe> RecipeClass = MinerRecipeClass;

	// AFGHologram::SpawnHologramFromRecipe resolves the descriptor's
	// hologram class internally and spawns it - the one real "spawn a
	// hologram correctly" API found in docs/building-placement-research.md.
	AFGHologram* Hologram = AFGHologram::SpawnHologramFromRecipe(RecipeClass, Character, TargetNode->GetActorLocation(), Character);
	if (!Hologram)
	{
		return FAIModOperationResult::Failure(TEXT("HOLOGRAM_SPAWN_FAILED"), TEXT("SpawnHologramFromRecipe returned null"));
	}

	Hologram->SetConstructionInstigator(Character);

	// Synthetic FHitResult - see docs/extractor-placement-research.md §2:
	// no direct "set target node" setter exists on the hologram, only
	// FHitResult-driven entry points. This is the single most
	// load-bearing unverified assumption in this whole experiment -
	// whether a synthetic (non-traced) hit result produces correct
	// snapping is exactly what this function exists to find out.
	// Use the interface's own placement helpers rather than the node's
	// raw actor location/no rotation - FGExtractableResourceInterface.h's
	// own doc comments say these are "used by holograms to get the
	// correct location/rotation for snapping when placed on this
	// extractable resource," which is a closer match to what a real
	// AFGBuildGun-produced hit result would carry than a raw actor
	// transform.
	const FVector RawLocation = TargetNode->GetActorLocation();
	const FVector PlacementLocation = TargetNode->GetPlacementLocation(RawLocation);
	const FRotator PlacementRotation = TargetNode->GetPlacementRotation(RawLocation);
	UE_LOG(LogAIModAI, Display, TEXT("DebugCheckExtractorPlacementOnTargetedNode: rawLocation=%s placementLocation=%s placementRotation=%s"),
		*RawLocation.ToString(), *PlacementLocation.ToString(), *PlacementRotation.ToString());

	FHitResult SyntheticHit;
	SyntheticHit.Location = PlacementLocation;
	SyntheticHit.ImpactPoint = PlacementLocation;
	SyntheticHit.Normal = PlacementRotation.RotateVector(FVector::UpVector);
	SyntheticHit.ImpactNormal = SyntheticHit.Normal;
	SyntheticHit.HitObjectHandle = FActorInstanceHandle(TargetNode);
	SyntheticHit.bBlockingHit = true;
	if (UPrimitiveComponent* NodePrimitive = Cast<UPrimitiveComponent>(TargetNode->GetRootComponent()))
	{
		SyntheticHit.Component = NodePrimitive;
	}
	else
	{
		UE_LOG(LogAIModAI, Warning, TEXT("DebugCheckExtractorPlacementOnTargetedNode: resource node's root component is not a UPrimitiveComponent - synthetic hit result has no Component set"));
	}

	if (!Hologram->IsValidHitResult(SyntheticHit))
	{
		Hologram->Destroy();
		return FAIModOperationResult::Failure(TEXT("INVALID_HIT_RESULT"), TEXT("Hologram::IsValidHitResult rejected the synthetic hit result"));
	}

	// The single external trigger point for placement updates
	// (docs/extractor-placement-research.md §2) - handles snapping
	// internally rather than calling TrySnapToActor/SetHologramLocationAndRotation
	// separately.
	Hologram->UpdateHologramPlacement(SyntheticHit);

	// Found live (2026-08-24): checking CanConstruct() immediately, and
	// even after 5 manually-invoked Hologram->Tick(0.1f) calls, both
	// reported a hard UFGCDInitializing ("Initializing") disqualifier.
	// FGHologram.h's InitializeClearanceData()/PostInitializeClearanceData()
	// split (:535-536) suggests clearance checking kicks off a world
	// query - most plausibly an async overlap - that doesn't resolve
	// within the same frame it starts in. A manually-invoked Tick() call
	// never gives an actual engine frame boundary a chance to complete
	// that query; polling across real World Tick cycles (via
	// SetTimerForNextTick) does. Poll every real tick rather than
	// blind-waiting a fixed duration, so this resolves in as few real
	// frames as the engine actually needs - MaxPollAttempts is only a
	// safety cap, not the expected case.
	struct FPollState
	{
		TWeakObjectPtr<AFGHologram> Hologram;
		TWeakObjectPtr<UWorld> World;
		FString NodeId;
		int32 AttemptsRemaining = 120; // safety cap - real ticks, not a fixed duration
		int32 AttemptsTaken = 0;
	};
	const TSharedRef<FPollState> PollState = MakeShared<FPollState>();
	PollState->Hologram = Hologram;
	PollState->World = World;
	PollState->NodeId = TargetTelemetry.Id;

	const TSharedRef<TFunction<void()>> PollFn = MakeShared<TFunction<void()>>();
	*PollFn = [PollState, PollFn]()
	{
		++PollState->AttemptsTaken;

		AFGHologram* PollHologram = PollState->Hologram.Get();
		UWorld* PollWorld = PollState->World.Get();
		if (!IsValid(PollHologram) || !PollWorld)
		{
			UE_LOG(LogAIModAI, Warning, TEXT("DebugCheckExtractorPlacementOnTargetedNode (deferred): hologram or world became invalid while polling (after %d tick(s))"), PollState->AttemptsTaken);
			return;
		}

		TArray<TSubclassOf<UFGConstructDisqualifier>> Disqualifiers;
		PollHologram->GetConstructDisqualifiers(Disqualifiers);
		const bool bStillInitializing = Disqualifiers.Contains(TSubclassOf<UFGConstructDisqualifier>(UFGCDInitializing::StaticClass()));

		--PollState->AttemptsRemaining;
		if (bStillInitializing && PollState->AttemptsRemaining > 0)
		{
			PollWorld->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
			return;
		}

		const bool bCanConstruct = PollHologram->CanConstruct();
		TArray<FString> DisqualifierTexts;
		for (const TSubclassOf<UFGConstructDisqualifier>& DisqualifierClass : Disqualifiers)
		{
			DisqualifierTexts.Add(FString::Printf(TEXT("%s (%s)"),
				*UFGConstructDisqualifier::GetDisqualifyingText(DisqualifierClass).ToString(),
				UFGConstructDisqualifier::GetIsSoftDisqualifier(DisqualifierClass) ? TEXT("soft") : TEXT("hard")));
		}
		const FString DisqualifierSummary = DisqualifierTexts.IsEmpty() ? TEXT("<none>") : FString::Join(DisqualifierTexts, TEXT("; "));

		// Diagnostic only, both public on AFGHologram (FGHologram.h:363,370)
		// - testing the hypothesis that UFGCDInitializing never clearing
		// (three attempts, docs/extractor-placement-research.md) is
		// because the protected SetupClearanceDetector() (:540), which the
		// real AFGBuildGun explicitly calls after spawning a hologram
		// (paired with its own CleanupHologramClearanceDetection()), never
		// ran for a hologram spawned via SpawnHologramFromRecipe. A null
		// GetClearanceDetector() here would directly confirm that.
		UE_LOG(LogAIModAI, Display, TEXT("DebugCheckExtractorPlacementOnTargetedNode (deferred, resolved after %d real tick(s)): node=%s canConstruct=%s disqualifiers=[%s] clearanceDetector=%s hasClearance=%s"),
			PollState->AttemptsTaken, *PollState->NodeId, bCanConstruct ? TEXT("true") : TEXT("false"), *DisqualifierSummary,
			PollHologram->GetClearanceDetector() ? TEXT("set") : TEXT("null"),
			PollHologram->HasClearance() ? TEXT("true") : TEXT("false"));

		PollHologram->Destroy();
	};

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));

	return FAIModOperationResult::Failure(TEXT("PENDING"),
		TEXT("Scheduled - polling real ticks until UFGCDInitializing clears (or a safety cap is hit); see LogAIModAI for the real result"));
}

FAIModOperationResult UAIModFunctionLibrary::DebugCheckExtractorPlacementViaBuildGun(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}

	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Character)
	{
		return FAIModOperationResult::Failure(TEXT("NO_PLAYER"), TEXT("No local AFGCharacterPlayer (player index 0)"));
	}

	const FAIModResourceNodeTelemetry TargetTelemetry = GetTargetedResourceNode(WorldContextObject);
	if (TargetTelemetry.Id.IsEmpty())
	{
		return FAIModOperationResult::Failure(TEXT("NO_TARGET_NODE"), TEXT("Not currently looking at a resource node"));
	}

	AFGResourceNode* TargetNode = nullptr;
	for (TActorIterator<AFGResourceNode> It(World); It; ++It)
	{
		if (IsValid(*It) && It->GetPathName() == TargetTelemetry.Id)
		{
			TargetNode = *It;
			break;
		}
	}
	if (!TargetNode)
	{
		return FAIModOperationResult::Failure(TEXT("NODE_NOT_FOUND"), TEXT("Targeted node id could not be re-resolved"));
	}

	if (TargetNode->IsOccupied())
	{
		return FAIModOperationResult::Failure(TEXT("NODE_OCCUPIED"), TEXT("Targeted node is already occupied"));
	}

	const EResourceForm Form = UFGItemDescriptor::GetForm(TargetNode->GetResourceClass());
	if (Form != EResourceForm::RF_SOLID)
	{
		return FAIModOperationResult::Failure(TEXT("UNSUPPORTED_RESOURCE_FORM"),
			TEXT("This experiment only supports solid resource nodes (Miner Mk1) so far"));
	}

	UClass* MinerRecipeClass = LoadObject<UClass>(nullptr, TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_MinerMk1.Recipe_MinerMk1_C"));
	if (!MinerRecipeClass || !MinerRecipeClass->IsChildOf(UFGRecipe::StaticClass()))
	{
		return FAIModOperationResult::Failure(TEXT("RECIPE_LOAD_FAILED"), TEXT("Failed to load Recipe_MinerMk1 as a UFGRecipe"));
	}
	const TSubclassOf<UFGRecipe> RecipeClass = MinerRecipeClass;

	// Real, ordinary player-facing hotkey
	// (docs/buildgun-driven-placement-research.md §1) - equips the build
	// gun and enters build mode with this recipe in one call, same as a
	// player pressing a recipe hotkey. VISIBLE SIDE EFFECT: genuinely
	// changes what the player has equipped for the duration of this call
	// - always restored via UnequipBuildGun() below, on every exit path
	// from here on (including inside the poll lambda).
	Character->HotKeyRecipe(RecipeClass);

	AFGBuildGun* BuildGun = Character->GetBuildGun();
	if (!BuildGun)
	{
		Character->UnequipBuildGun();
		return FAIModOperationResult::Failure(TEXT("NO_BUILD_GUN"), TEXT("AFGCharacterPlayer::GetBuildGun() returned null"));
	}

	UFGBuildGunStateBuild* BuildState = Cast<UFGBuildGunStateBuild>(BuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD));
	if (!BuildState)
	{
		Character->UnequipBuildGun();
		return FAIModOperationResult::Failure(TEXT("NO_BUILD_STATE"), TEXT("Could not resolve UFGBuildGunStateBuild from the build gun"));
	}

	AFGHologram* Hologram = BuildState->GetHologram();
	if (!Hologram)
	{
		Character->UnequipBuildGun();
		return FAIModOperationResult::Failure(TEXT("HOLOGRAM_SPAWN_FAILED"), TEXT("HotKeyRecipe did not result in a spawned hologram - build gun may not have entered build mode as expected"));
	}

	// Same GetPlacementLocation/GetPlacementRotation fix as the
	// standalone experiment (docs/extractor-placement-research.md).
	const FVector RawLocation = TargetNode->GetActorLocation();
	const FVector PlacementLocation = TargetNode->GetPlacementLocation(RawLocation);
	const FRotator PlacementRotation = TargetNode->GetPlacementRotation(RawLocation);

	FHitResult SyntheticHit;
	SyntheticHit.Location = PlacementLocation;
	SyntheticHit.ImpactPoint = PlacementLocation;
	SyntheticHit.Normal = PlacementRotation.RotateVector(FVector::UpVector);
	SyntheticHit.ImpactNormal = SyntheticHit.Normal;
	SyntheticHit.HitObjectHandle = FActorInstanceHandle(TargetNode);
	SyntheticHit.bBlockingHit = true;
	if (UPrimitiveComponent* NodePrimitive = Cast<UPrimitiveComponent>(TargetNode->GetRootComponent()))
	{
		SyntheticHit.Component = NodePrimitive;
	}

	// Feed our synthetic hit result through the build gun's own mutable
	// GetHitResult() reference. NOTE, found live (2026-08-24, see
	// ConstructBuildingNearPlayer's matching fix and
	// docs/buildgun-driven-placement-research.md): setting this ONCE is
	// not enough - UFGBuildGunStateBuild::TickState_Implementation runs
	// its own real TraceForBuilding() every tick and overwrites this with
	// the player's live aim. Set it here anyway (harmless, and covers the
	// very first frame before any real trace has run), but the poll below
	// re-asserts our placement directly via UpdateHologramPlacement()
	// every tick, which is what actually makes the position deterministic.
	BuildGun->GetHitResult() = SyntheticHit;

	struct FPollState
	{
		TWeakObjectPtr<AFGHologram> Hologram;
		TWeakObjectPtr<AFGCharacterPlayer> Character;
		TWeakObjectPtr<UWorld> World;
		FString NodeId;
		FHitResult SyntheticHit;
		int32 AttemptsRemaining = 120; // safety cap - real ticks, not a fixed duration
		int32 AttemptsTaken = 0;
	};
	const TSharedRef<FPollState> PollState = MakeShared<FPollState>();
	PollState->Hologram = Hologram;
	PollState->Character = Character;
	PollState->World = World;
	PollState->NodeId = TargetTelemetry.Id;
	PollState->SyntheticHit = SyntheticHit;

	const TSharedRef<TFunction<void()>> PollFn = MakeShared<TFunction<void()>>();
	*PollFn = [PollState, PollFn]()
	{
		++PollState->AttemptsTaken;

		AFGHologram* PollHologram = PollState->Hologram.Get();
		UWorld* PollWorld = PollState->World.Get();
		AFGCharacterPlayer* PollCharacter = PollState->Character.Get();
		if (!IsValid(PollHologram) || !PollWorld)
		{
			UE_LOG(LogAIModAI, Warning, TEXT("DebugCheckExtractorPlacementViaBuildGun (deferred): hologram or world became invalid while polling (after %d tick(s))"), PollState->AttemptsTaken);
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			return;
		}

		// Re-assert our intended placement, overriding whatever the build
		// gun's own real per-tick trace did to the hologram since we last
		// checked - see the comment on BuildGun->GetHitResult() above.
		PollHologram->UpdateHologramPlacement(PollState->SyntheticHit);

		TArray<TSubclassOf<UFGConstructDisqualifier>> Disqualifiers;
		PollHologram->GetConstructDisqualifiers(Disqualifiers);
		const bool bStillInitializing = Disqualifiers.Contains(TSubclassOf<UFGConstructDisqualifier>(UFGCDInitializing::StaticClass()));

		--PollState->AttemptsRemaining;
		if (bStillInitializing && PollState->AttemptsRemaining > 0)
		{
			PollWorld->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
			return;
		}

		const bool bCanConstruct = PollHologram->CanConstruct();
		TArray<FString> DisqualifierTexts;
		for (const TSubclassOf<UFGConstructDisqualifier>& DisqualifierClass : Disqualifiers)
		{
			DisqualifierTexts.Add(FString::Printf(TEXT("%s (%s)"),
				*UFGConstructDisqualifier::GetDisqualifyingText(DisqualifierClass).ToString(),
				UFGConstructDisqualifier::GetIsSoftDisqualifier(DisqualifierClass) ? TEXT("soft") : TEXT("hard")));
		}
		const FString DisqualifierSummary = DisqualifierTexts.IsEmpty() ? TEXT("<none>") : FString::Join(DisqualifierTexts, TEXT("; "));

		UE_LOG(LogAIModAI, Display, TEXT("DebugCheckExtractorPlacementViaBuildGun (deferred, resolved after %d real tick(s)): node=%s canConstruct=%s disqualifiers=[%s]"),
			PollState->AttemptsTaken, *PollState->NodeId, bCanConstruct ? TEXT("true") : TEXT("false"), *DisqualifierSummary);

		// Never calls Construct()/Server_ConstructHologram - see this
		// function's header comment. Always restore the player's prior
		// equipped state, regardless of outcome.
		if (IsValid(PollCharacter))
		{
			PollCharacter->UnequipBuildGun();
		}
	};

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));

	return FAIModOperationResult::Failure(TEXT("PENDING"),
		TEXT("Scheduled via the real build gun - polling real ticks until UFGCDInitializing clears (or a safety cap is hit); see LogAIModAI for the real result"));
}

FAIModOperationResult UAIModFunctionLibrary::ConstructExtractorOnTargetedNode(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}

	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Character)
	{
		return FAIModOperationResult::Failure(TEXT("NO_PLAYER"), TEXT("No local AFGCharacterPlayer (player index 0)"));
	}

	const FAIModResourceNodeTelemetry TargetTelemetry = GetTargetedResourceNode(WorldContextObject);
	if (TargetTelemetry.Id.IsEmpty())
	{
		return FAIModOperationResult::Failure(TEXT("NO_TARGET_NODE"), TEXT("Not currently looking at a resource node"));
	}

	AFGResourceNode* TargetNode = nullptr;
	for (TActorIterator<AFGResourceNode> It(World); It; ++It)
	{
		if (IsValid(*It) && It->GetPathName() == TargetTelemetry.Id)
		{
			TargetNode = *It;
			break;
		}
	}
	if (!TargetNode)
	{
		return FAIModOperationResult::Failure(TEXT("NODE_NOT_FOUND"), TEXT("Targeted node id could not be re-resolved"));
	}

	if (TargetNode->IsOccupied())
	{
		return FAIModOperationResult::Failure(TEXT("NODE_OCCUPIED"), TEXT("Targeted node is already occupied"));
	}

	const EResourceForm Form = UFGItemDescriptor::GetForm(TargetNode->GetResourceClass());
	if (Form != EResourceForm::RF_SOLID)
	{
		return FAIModOperationResult::Failure(TEXT("UNSUPPORTED_RESOURCE_FORM"),
			TEXT("This experiment only supports solid resource nodes (Miner Mk1) so far"));
	}

	UClass* MinerRecipeClass = LoadObject<UClass>(nullptr, TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_MinerMk1.Recipe_MinerMk1_C"));
	if (!MinerRecipeClass || !MinerRecipeClass->IsChildOf(UFGRecipe::StaticClass()))
	{
		return FAIModOperationResult::Failure(TEXT("RECIPE_LOAD_FAILED"), TEXT("Failed to load Recipe_MinerMk1 as a UFGRecipe"));
	}
	const TSubclassOf<UFGRecipe> RecipeClass = MinerRecipeClass;

	// Real, ordinary player-facing hotkey - see
	// DebugCheckExtractorPlacementViaBuildGun's matching comment. VISIBLE
	// SIDE EFFECT, always restored via UnequipBuildGun().
	Character->HotKeyRecipe(RecipeClass);

	AFGBuildGun* BuildGun = Character->GetBuildGun();
	if (!BuildGun)
	{
		Character->UnequipBuildGun();
		return FAIModOperationResult::Failure(TEXT("NO_BUILD_GUN"), TEXT("AFGCharacterPlayer::GetBuildGun() returned null"));
	}

	UFGBuildGunStateBuild* BuildState = Cast<UFGBuildGunStateBuild>(BuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD));
	if (!BuildState)
	{
		Character->UnequipBuildGun();
		return FAIModOperationResult::Failure(TEXT("NO_BUILD_STATE"), TEXT("Could not resolve UFGBuildGunStateBuild from the build gun"));
	}

	AFGHologram* Hologram = BuildState->GetHologram();
	if (!Hologram)
	{
		Character->UnequipBuildGun();
		return FAIModOperationResult::Failure(TEXT("HOLOGRAM_SPAWN_FAILED"), TEXT("HotKeyRecipe did not result in a spawned hologram"));
	}

	const FVector RawLocation = TargetNode->GetActorLocation();
	const FVector PlacementLocation = TargetNode->GetPlacementLocation(RawLocation);
	const FRotator PlacementRotation = TargetNode->GetPlacementRotation(RawLocation);

	FHitResult SyntheticHit;
	SyntheticHit.Location = PlacementLocation;
	SyntheticHit.ImpactPoint = PlacementLocation;
	SyntheticHit.Normal = PlacementRotation.RotateVector(FVector::UpVector);
	SyntheticHit.ImpactNormal = SyntheticHit.Normal;
	SyntheticHit.HitObjectHandle = FActorInstanceHandle(TargetNode);
	SyntheticHit.bBlockingHit = true;
	if (UPrimitiveComponent* NodePrimitive = Cast<UPrimitiveComponent>(TargetNode->GetRootComponent()))
	{
		SyntheticHit.Component = NodePrimitive;
	}

	// See DebugCheckExtractorPlacementViaBuildGun's matching comment:
	// setting this once is not enough - the poll below re-asserts it via
	// UpdateHologramPlacement() every tick to override the build gun's
	// own real per-tick trace.
	BuildGun->GetHitResult() = SyntheticHit;

	struct FPollState
	{
		TWeakObjectPtr<AFGHologram> Hologram;
		TWeakObjectPtr<AFGCharacterPlayer> Character;
		TWeakObjectPtr<AFGResourceNode> TargetNode;
		TWeakObjectPtr<UWorld> World;
		FString NodeId;
		FHitResult SyntheticHit;
		int32 AttemptsRemaining = 120; // safety cap - real ticks, not a fixed duration
		int32 AttemptsTaken = 0;
	};
	const TSharedRef<FPollState> PollState = MakeShared<FPollState>();
	PollState->Hologram = Hologram;
	PollState->Character = Character;
	PollState->TargetNode = TargetNode;
	PollState->World = World;
	PollState->NodeId = TargetTelemetry.Id;
	PollState->SyntheticHit = SyntheticHit;

	const TSharedRef<TFunction<void()>> PollFn = MakeShared<TFunction<void()>>();
	*PollFn = [PollState, PollFn]()
	{
		++PollState->AttemptsTaken;

		AFGHologram* PollHologram = PollState->Hologram.Get();
		UWorld* PollWorld = PollState->World.Get();
		AFGCharacterPlayer* PollCharacter = PollState->Character.Get();
		AFGResourceNode* PollTargetNode = PollState->TargetNode.Get();
		if (!IsValid(PollHologram) || !PollWorld)
		{
			UE_LOG(LogAIModAI, Warning, TEXT("ConstructExtractorOnTargetedNode (deferred): hologram or world became invalid while polling (after %d tick(s)) - nothing built"), PollState->AttemptsTaken);
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			return;
		}

		PollHologram->UpdateHologramPlacement(PollState->SyntheticHit);

		TArray<TSubclassOf<UFGConstructDisqualifier>> Disqualifiers;
		PollHologram->GetConstructDisqualifiers(Disqualifiers);
		const bool bStillInitializing = Disqualifiers.Contains(TSubclassOf<UFGConstructDisqualifier>(UFGCDInitializing::StaticClass()));

		--PollState->AttemptsRemaining;
		if (bStillInitializing && PollState->AttemptsRemaining > 0)
		{
			PollWorld->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
			return;
		}

		const bool bCanConstruct = PollHologram->CanConstruct();
		TArray<FString> DisqualifierTexts;
		for (const TSubclassOf<UFGConstructDisqualifier>& DisqualifierClass : Disqualifiers)
		{
			DisqualifierTexts.Add(FString::Printf(TEXT("%s (%s)"),
				*UFGConstructDisqualifier::GetDisqualifyingText(DisqualifierClass).ToString(),
				UFGConstructDisqualifier::GetIsSoftDisqualifier(DisqualifierClass) ? TEXT("soft") : TEXT("hard")));
		}
		const FString DisqualifierSummary = DisqualifierTexts.IsEmpty() ? TEXT("<none>") : FString::Join(DisqualifierTexts, TEXT("; "));

		if (!bCanConstruct)
		{
			UE_LOG(LogAIModAI, Display, TEXT("ConstructExtractorOnTargetedNode (deferred, resolved after %d real tick(s)): CanConstruct()=false, NOT constructing - node=%s disqualifiers=[%s]"),
				PollState->AttemptsTaken, *PollState->NodeId, *DisqualifierSummary);
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			return;
		}

		// The only point where this function actually differs from
		// DebugCheckExtractorPlacementViaBuildGun - everything above is
		// the identical validated dry-run flow. InternalConstructHologram
		// is the same function Server_ConstructHologram's RPC delegates
		// to server-side (both public - see
		// docs/buildgun-driven-placement-research.md §5); calling it
		// directly here skips only the client->server RPC serialization
		// round-trip, a no-op in this same-process singleplayer/
		// listen-server context, and operates on the same live hologram
		// this poll already validated with CanConstruct()==true.
		AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(PollWorld);
		const FNetConstructionID ConstructionID = BuildableSubsystem ? BuildableSubsystem->GetNewNetConstructionID() : FNetConstructionID();

		AFGBuildGun* PollBuildGun = IsValid(PollCharacter) ? PollCharacter->GetBuildGun() : nullptr;
		UFGBuildGunStateBuild* PollBuildState = PollBuildGun ? Cast<UFGBuildGunStateBuild>(PollBuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD)) : nullptr;
		if (!PollBuildState)
		{
			UE_LOG(LogAIModAI, Error, TEXT("ConstructExtractorOnTargetedNode (deferred): lost the build state before constructing - aborting, nothing built"));
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			return;
		}

		PollBuildState->InternalConstructHologram(ConstructionID);

		// Confirmation, not just trust: if construction genuinely
		// succeeded, the node should now report occupied.
		const bool bNowOccupied = IsValid(PollTargetNode) && PollTargetNode->IsOccupied();
		UE_LOG(LogAIModAI, Display, TEXT("ConstructExtractorOnTargetedNode (deferred, resolved after %d real tick(s)): construction attempted via InternalConstructHologram - node=%s nodeNowOccupied=%s"),
			PollState->AttemptsTaken, *PollState->NodeId, bNowOccupied ? TEXT("true") : TEXT("false"));

		if (IsValid(PollCharacter))
		{
			PollCharacter->UnequipBuildGun();
		}
	};

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));

	return FAIModOperationResult::Failure(TEXT("PENDING"),
		TEXT("Scheduled via the real build gun - if CanConstruct() resolves true, a real Miner Mk1 WILL be constructed; see LogAIModAI for the real result"));
}

FAIModOperationResult UAIModFunctionLibrary::ConstructBuildingNearPlayer(UObject* WorldContextObject, const FString& RecipeClassPath)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}

	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Character)
	{
		return FAIModOperationResult::Failure(TEXT("NO_PLAYER"), TEXT("No local AFGCharacterPlayer (player index 0)"));
	}

	// Same validation as SetManufacturerRecipe - deliberately narrow, not
	// a generic "load any class by path" capability.
	UClass* ResolvedClass = LoadObject<UClass>(nullptr, *RecipeClassPath);
	if (!ResolvedClass || !ResolvedClass->IsChildOf(UFGRecipe::StaticClass()))
	{
		return FAIModOperationResult::Failure(TEXT("INVALID_RECIPE"),
			FString::Printf(TEXT("'%s' did not resolve to a UFGRecipe subclass"), *RecipeClassPath));
	}
	const TSubclassOf<UFGRecipe> RecipeClass = ResolvedClass;

	// Deliberately simple position choice, not a real "solve valid
	// placement" algorithm - see this function's header comment. A
	// candidate X/Y 800 units in front of the player, then a single
	// vertical line trace to find real ground there. CanConstruct() is
	// still the real gate on whether this spot actually works.
	const FVector PlayerLocation = Character->GetActorLocation();
	const FVector PlayerForward2D = Character->GetActorForwardVector().GetSafeNormal2D();
	const FVector CandidateXY = PlayerLocation + PlayerForward2D * 800.0f;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AIModConstructBuildingNearPlayer), false);
	QueryParams.AddIgnoredActor(Character);
	const FVector TraceStart(CandidateXY.X, CandidateXY.Y, PlayerLocation.Z + 1000.0f);
	const FVector TraceEnd(CandidateXY.X, CandidateXY.Y, PlayerLocation.Z - 1000.0f);

	FHitResult GroundHit;
	const bool bFoundGround = World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, QueryParams);

	FHitResult SyntheticHit;
	if (bFoundGround)
	{
		SyntheticHit = GroundHit;
	}
	else
	{
		// Fallback: no real ground found within range - assume flat
		// ground at the player's own Z, same degraded pattern used
		// elsewhere in this file when a real reference isn't available.
		UE_LOG(LogAIModAI, Warning, TEXT("ConstructBuildingNearPlayer: ground trace found nothing at (%.0f, %.0f) - falling back to player Z"), CandidateXY.X, CandidateXY.Y);
		SyntheticHit.Location = FVector(CandidateXY.X, CandidateXY.Y, PlayerLocation.Z);
		SyntheticHit.ImpactPoint = SyntheticHit.Location;
		SyntheticHit.Normal = FVector::UpVector;
		SyntheticHit.ImpactNormal = FVector::UpVector;
		SyntheticHit.bBlockingHit = true;
	}

	UE_LOG(LogAIModAI, Display, TEXT("ConstructBuildingNearPlayer: recipe=%s groundTraceHit=%s location=%s"),
		*RecipeClassPath, bFoundGround ? TEXT("true") : TEXT("false"), *SyntheticHit.Location.ToString());

	// Real, ordinary player-facing hotkey - see
	// ConstructExtractorOnTargetedNode's matching comment. VISIBLE SIDE
	// EFFECT, always restored via UnequipBuildGun().
	Character->HotKeyRecipe(RecipeClass);

	AFGBuildGun* BuildGun = Character->GetBuildGun();
	if (!BuildGun)
	{
		Character->UnequipBuildGun();
		return FAIModOperationResult::Failure(TEXT("NO_BUILD_GUN"), TEXT("AFGCharacterPlayer::GetBuildGun() returned null"));
	}

	UFGBuildGunStateBuild* BuildState = Cast<UFGBuildGunStateBuild>(BuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD));
	if (!BuildState)
	{
		Character->UnequipBuildGun();
		return FAIModOperationResult::Failure(TEXT("NO_BUILD_STATE"), TEXT("Could not resolve UFGBuildGunStateBuild from the build gun"));
	}

	AFGHologram* Hologram = BuildState->GetHologram();
	if (!Hologram)
	{
		Character->UnequipBuildGun();
		return FAIModOperationResult::Failure(TEXT("HOLOGRAM_SPAWN_FAILED"),
			TEXT("HotKeyRecipe did not result in a spawned hologram - recipe may not be a simple single-step buildable"));
	}

	// See DebugCheckExtractorPlacementViaBuildGun's matching comment -
	// confirmed live (2026-08-24) that setting this once is not enough:
	// UFGBuildGunStateBuild::TickState_Implementation runs its own real
	// TraceForBuilding() every tick and overwrites GetHitResult() with
	// the player's live aim, silently discarding whatever we set here.
	// The poll below re-asserts our intended placement directly via
	// UpdateHologramPlacement() every tick, which is what actually makes
	// the position deterministic regardless of where the player is
	// looking.
	BuildGun->GetHitResult() = SyntheticHit;

	struct FPollState
	{
		TWeakObjectPtr<AFGHologram> Hologram;
		TWeakObjectPtr<AFGCharacterPlayer> Character;
		TWeakObjectPtr<UWorld> World;
		FString RecipeClassPath;
		FHitResult SyntheticHit;
		int32 AttemptsRemaining = 120; // safety cap - real ticks, not a fixed duration
		int32 AttemptsTaken = 0;
	};
	const TSharedRef<FPollState> PollState = MakeShared<FPollState>();
	PollState->Hologram = Hologram;
	PollState->Character = Character;
	PollState->World = World;
	PollState->RecipeClassPath = RecipeClassPath;
	PollState->SyntheticHit = SyntheticHit;

	const TSharedRef<TFunction<void()>> PollFn = MakeShared<TFunction<void()>>();
	*PollFn = [PollState, PollFn]()
	{
		++PollState->AttemptsTaken;

		AFGHologram* PollHologram = PollState->Hologram.Get();
		UWorld* PollWorld = PollState->World.Get();
		AFGCharacterPlayer* PollCharacter = PollState->Character.Get();
		if (!IsValid(PollHologram) || !PollWorld)
		{
			UE_LOG(LogAIModAI, Warning, TEXT("ConstructBuildingNearPlayer (deferred): hologram or world became invalid while polling (after %d tick(s)) - nothing built"), PollState->AttemptsTaken);
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			return;
		}

		// The actual fix (see comment above BuildGun->GetHitResult()):
		// re-assert our intended placement, overriding whatever the build
		// gun's own real per-tick trace did to the hologram since we last
		// checked.
		PollHologram->UpdateHologramPlacement(PollState->SyntheticHit);

		TArray<TSubclassOf<UFGConstructDisqualifier>> Disqualifiers;
		PollHologram->GetConstructDisqualifiers(Disqualifiers);
		const bool bStillInitializing = Disqualifiers.Contains(TSubclassOf<UFGConstructDisqualifier>(UFGCDInitializing::StaticClass()));

		--PollState->AttemptsRemaining;
		if (bStillInitializing && PollState->AttemptsRemaining > 0)
		{
			PollWorld->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
			return;
		}

		const bool bCanConstruct = PollHologram->CanConstruct();
		TArray<FString> DisqualifierTexts;
		for (const TSubclassOf<UFGConstructDisqualifier>& DisqualifierClass : Disqualifiers)
		{
			DisqualifierTexts.Add(FString::Printf(TEXT("%s (%s)"),
				*UFGConstructDisqualifier::GetDisqualifyingText(DisqualifierClass).ToString(),
				UFGConstructDisqualifier::GetIsSoftDisqualifier(DisqualifierClass) ? TEXT("soft") : TEXT("hard")));
		}
		const FString DisqualifierSummary = DisqualifierTexts.IsEmpty() ? TEXT("<none>") : FString::Join(DisqualifierTexts, TEXT("; "));

		if (!bCanConstruct)
		{
			UE_LOG(LogAIModAI, Display, TEXT("ConstructBuildingNearPlayer (deferred, resolved after %d real tick(s)): CanConstruct()=false, NOT constructing - recipe=%s disqualifiers=[%s]"),
				PollState->AttemptsTaken, *PollState->RecipeClassPath, *DisqualifierSummary);
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			return;
		}

		AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(PollWorld);
		const FNetConstructionID ConstructionID = BuildableSubsystem ? BuildableSubsystem->GetNewNetConstructionID() : FNetConstructionID();

		AFGBuildGun* PollBuildGun = IsValid(PollCharacter) ? PollCharacter->GetBuildGun() : nullptr;
		UFGBuildGunStateBuild* PollBuildState = PollBuildGun ? Cast<UFGBuildGunStateBuild>(PollBuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD)) : nullptr;
		if (!PollBuildState)
		{
			UE_LOG(LogAIModAI, Error, TEXT("ConstructBuildingNearPlayer (deferred): lost the build state before constructing - aborting, nothing built"));
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			return;
		}

		const FVector ConstructLocation = PollHologram->GetActorLocation();
		PollBuildState->InternalConstructHologram(ConstructionID);

		UE_LOG(LogAIModAI, Display, TEXT("ConstructBuildingNearPlayer (deferred, resolved after %d real tick(s)): construction attempted via InternalConstructHologram - recipe=%s location=%s"),
			PollState->AttemptsTaken, *PollState->RecipeClassPath, *ConstructLocation.ToString());

		if (IsValid(PollCharacter))
		{
			PollCharacter->UnequipBuildGun();
		}
	};

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));

	return FAIModOperationResult::Failure(TEXT("PENDING"),
		TEXT("Scheduled via the real build gun - if CanConstruct() resolves true, the building WILL be constructed; see LogAIModAI for the real result"));
}

FAIModOperationResult UAIModFunctionLibrary::SpawnCreatureNearPlayer(UObject* WorldContextObject, const FString& CreatureClassPath, float DistanceFromPlayer, float Scale)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}

	// Player-opt-in-only gate, off by default - see this function's header
	// comment and AIModConfiguration.h. Checked first, before any other
	// validation, so a disabled request never even resolves the class or
	// touches the world.
	if (!UAIModFunctionLibrary::GetAIModConfigBool(World, TEXT("AllowCreatureSpawning"), false))
	{
		return FAIModOperationResult::Failure(TEXT("CREATURE_SPAWNING_DISABLED"),
			TEXT("Creature spawning is off by default - enable \"Allow Creature Spawning\" in AIMod's mod settings to allow this"));
	}

	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Character)
	{
		return FAIModOperationResult::Failure(TEXT("NO_PLAYER"), TEXT("No local AFGCharacterPlayer (player index 0)"));
	}

	// Same narrow "load and validate one specific type" pattern as
	// RecipeClassPath elsewhere in this file - not a generic "spawn any
	// actor" capability.
	UClass* ResolvedClass = LoadObject<UClass>(nullptr, *CreatureClassPath);
	if (!ResolvedClass || !ResolvedClass->IsChildOf(AFGCreature::StaticClass()))
	{
		return FAIModOperationResult::Failure(TEXT("INVALID_CREATURE_CLASS"),
			FString::Printf(TEXT("'%s' did not resolve to an AFGCreature subclass"), *CreatureClassPath));
	}
	const TSubclassOf<AFGCreature> CreatureClass = ResolvedClass;

	AFGCreatureSubsystem* CreatureSubsystem = AFGCreatureSubsystem::Get(World);
	if (!CreatureSubsystem)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("AFGCreatureSubsystem::Get() returned null"));
	}

	const float ClampedDistance = FMath::Clamp(DistanceFromPlayer > 0.0f ? DistanceFromPlayer : 800.0f, 100.0f, 5000.0f);
	const FVector PlayerLocation = Character->GetActorLocation();
	const FVector PlayerForward2D = Character->GetActorForwardVector().GetSafeNormal2D();
	const FVector CandidateXY = PlayerLocation + PlayerForward2D * ClampedDistance;

	const FGroundTraceResult GroundTrace = FindGroundAtXY(World, CandidateXY.X, CandidateXY.Y, PlayerLocation.Z, Character);
	const FVector SpawnLocation = GroundTrace.Hit.Location + FVector(0.0f, 0.0f, 50.0f);

	// Uniform scale, applied via the same FTransform BeginSpawningCreature/
	// FinishSpawning already take - untested against FactoryGame's actual
	// creature Blueprints (collision capsules and AI behavior-tree
	// distances are often hardcoded rather than derived from RootComponent
	// scale, so an extreme value may look/behave oddly even if it spawns
	// cleanly). Clamped well short of 0 to avoid a degenerate/inverted
	// actor; not clamped tightly otherwise since "abnormally scaled" is
	// the explicit point of this parameter.
	const float ClampedScale = FMath::Clamp(Scale > 0.0f ? Scale : 1.0f, 0.05f, 20.0f);
	const FTransform SpawnTransform(Character->GetActorRotation(), SpawnLocation, FVector(ClampedScale));

	UE_LOG(LogAIModAI, Display, TEXT("SpawnCreatureNearPlayer: class=%s distance=%.0f scale=%.2f groundTraceHit=%s location=%s"),
		*CreatureClassPath, ClampedDistance, ClampedScale, GroundTrace.bFound ? TEXT("true") : TEXT("false"), *SpawnLocation.ToString());

	AFGCreature* NewCreature = CreatureSubsystem->BeginSpawningCreature(CreatureClass, SpawnTransform);
	if (!NewCreature)
	{
		return FAIModOperationResult::Failure(TEXT("SPAWN_FAILED"),
			TEXT("AFGCreatureSubsystem::BeginSpawningCreature returned null - see LogAIModAI"));
	}

	// "Begin..." naming mirrors AFGBuildableSubsystem::BeginSpawnBuildable,
	// whose doc comment is explicit: "you need to call FinishSpawning...
	// to finalize the spawning" - a standard Unreal deferred-actor-spawn
	// pattern (AActor::FinishSpawning, Actor.h). Confirmed live
	// (2026-08-28): without this call the creature spawned but was frozen
	// (no animation, no movement, no AI) - Actor.h's own comment on why:
	// "Whether FinishSpawning has been called for this Actor. If it has
	// not, the Actor is in a malformed state."
	NewCreature->FinishSpawning(SpawnTransform);

	UE_LOG(LogAIModAI, Display, TEXT("SpawnCreatureNearPlayer: spawned %s"), *NewCreature->GetPathName());
	return FAIModOperationResult::SuccessWithBuildableId(NewCreature->GetPathName());
}

FAIModOperationResult UAIModFunctionLibrary::DespawnCreature(UObject* WorldContextObject, const FString& CreatureId)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}

	for (TActorIterator<AFGCreature> It(World); It; ++It)
	{
		if (IsValid(*It) && It->GetPathName() == CreatureId)
		{
			UE_LOG(LogAIModAI, Display, TEXT("DespawnCreature: destroying %s"), *CreatureId);
			It->Destroy();
			return FAIModOperationResult::Success();
		}
	}

	return FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"),
		FString::Printf(TEXT("No live AFGCreature found with id '%s'"), *CreatureId));
}

void UAIModFunctionLibrary::ConstructBuildingAtPosition(UObject* WorldContextObject, const FString& RecipeClassPath, float X, float Y, int32 RotationScrollDelta, float GridSnapSize, float ReferenceZ, bool bIgnoreGroundTrace, bool bIgnoreAimLocation, bool bIgnorePlayerEncroachment, bool bIgnoreClearance, bool bIgnoreInvalidFloor, bool bHasTargetYaw, float TargetYawDegrees, const FString& FaceBuildableId, TFunction<void(const FAIModOperationResult&)> OnComplete)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context")));
		return;
	}

	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Character)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_PLAYER"), TEXT("No local AFGCharacterPlayer (player index 0)")));
		return;
	}

	// LimitBuildDistance (2026-08-27) - a NEW synthetic restriction, not a
	// real FactoryGame disqualifier: grepping FGConstructDisqualifier.h
	// found no "too far from player" class at all, and this codebase has
	// built at 100,000+ unit distances all session with nothing ever
	// rejecting it (unlike a real player's Build Gun, which has a real
	// reach limit). Added per explicit user request ("simulate a build
	// distance limit... so structures cannot be built clear on the other
	// side of the map") as a player-controlled mod setting - off by
	// default, preserving today's unrestricted behavior. 2D distance
	// only (X/Y) - Z isn't resolved yet at this point (ground trace
	// hasn't run), and "how far away" is naturally a horizontal notion
	// for this use case anyway. Checked here, before any hologram/poll
	// work starts, so a rejected request never has any construction
	// side effects to clean up.
	if (UAIModFunctionLibrary::GetAIModConfigBool(World, TEXT("LimitBuildDistance"), false))
	{
		const float MaxBuildDistance = UAIModFunctionLibrary::GetAIModConfigFloat(World, TEXT("MaxBuildDistance"), 8000.0f);
		const float DistanceFromPlayer = FVector::Dist2D(Character->GetActorLocation(), FVector(X, Y, 0.0f));
		if (DistanceFromPlayer > MaxBuildDistance)
		{
			OnComplete(FAIModOperationResult::Failure(TEXT("BUILD_DISTANCE_EXCEEDED"),
				FString::Printf(TEXT("Target is %.0f units from the player, exceeding the configured Max Build Distance of %.0f units (Limit RPC Build Distance From Player is enabled in AIMod's mod settings)"),
					DistanceFromPlayer, MaxBuildDistance)));
			return;
		}
	}

	UClass* ResolvedClass = LoadObject<UClass>(nullptr, *RecipeClassPath);
	if (!ResolvedClass || !ResolvedClass->IsChildOf(UFGRecipe::StaticClass()))
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("INVALID_RECIPE"),
			FString::Printf(TEXT("'%s' did not resolve to a UFGRecipe subclass"), *RecipeClassPath)));
		return;
	}
	const TSubclassOf<UFGRecipe> RecipeClass = ResolvedClass;

	// Safety guard (2026-08-27, added after a live game CRASH, not just a
	// bad result): this generic single-step placement path must never be
	// used for extractor recipes (Miners, Water/Oil Pumps, Fracking
	// buildings) - confirmed live that placing Recipe_MinerMk2 through
	// here (no real resource node under it) resolved canConstruct=true
	// (unlike Recipe_MinerMk1 moments earlier at a different location,
	// which correctly refused with "Must be placed on a Resource Node!" -
	// the disqualifier is evidently not reliably present for every
	// extractor/location combination) and proceeded into
	// InternalConstructHologram() -> AFGBuildableHologram::ConstructInstance()
	// -> AFGResourceExtractorHologram::ConfigureActor(), which unconditionally
	// asserts on a valid mSnappedExtractableResource - a hard engine
	// assertion, not a catchable disqualifier, that takes the whole game
	// process down. Extractors have a dedicated, correct entry point
	// (ConstructExtractorOnNode / world.placeExtractor) that actually
	// snaps a real resource node reference before construction - refuse
	// here unconditionally (not just another disqualifier bIgnore* could
	// bypass) rather than gamble on GetConstructDisqualifiers() catching
	// every case.
	const TSubclassOf<AFGBuildable> ResolvedBuildableClass = ResolveBuildableClassForRecipe(RecipeClassPath);
	if (ResolvedBuildableClass && ResolvedBuildableClass->IsChildOf(AFGBuildableResourceExtractorBase::StaticClass()))
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("WRONG_METHOD_FOR_EXTRACTOR"),
			FString::Printf(TEXT("'%s' is an extractor recipe - use world.placeExtractor (ConstructExtractorOnNode) instead, which snaps a real resource node reference. Constructing an extractor through world.placeBuilding without one is a confirmed CRASH (AFGResourceExtractorHologram::ConfigureActor's mSnappedExtractableResource assertion), not just a bad placement."), *RecipeClassPath)));
		return;
	}

	// Caller-chosen general-purpose grid snap - see this function's
	// header doc comment. Applied before ground-tracing so the trace
	// itself (and everything downstream) sees the snapped coordinate.
	if (GridSnapSize > 0.0f)
	{
		const float SnappedX = FMath::RoundToFloat(X / GridSnapSize) * GridSnapSize;
		const float SnappedY = FMath::RoundToFloat(Y / GridSnapSize) * GridSnapSize;
		UE_LOG(LogAIModAI, Display, TEXT("ConstructBuildingAtPosition: gridSnapSize=%.1f snapped (%.1f, %.1f) -> (%.1f, %.1f)"),
			GridSnapSize, X, Y, SnappedX, SnappedY);
		X = SnappedX;
		Y = SnappedY;
	}

	// Same ground-finding approach as ConstructBuildingNearPlayer, just
	// driven by an explicit X/Y instead of a player-relative offset - see
	// that function's header comment for why this stays simple rather
	// than a real "solve valid placement" algorithm.
	//
	// ZSearchCenter (2026-08-25): the vertical search range for the
	// ground trace defaults to the PLAYER's current Z +/-1000 units -
	// found live to be a real reliability problem, not just a
	// theoretical one: placing a building far from the player's current
	// elevation (e.g. player standing on top of another building, or on
	// an unrelated walkway) can make the trace miss real terrain
	// entirely and fall back to the player's own (irrelevant) Z, putting
	// the result nowhere near the intended location on ANY axis - not
	// just wrong height, since a bad Z anchor can also make the trace
	// hit the wrong piece of geometry entirely. ReferenceZ lets the
	// caller anchor the search to a KNOWN, FIXED point instead (e.g. an
	// existing buildable's own Z from world.buildables) - deterministic
	// regardless of where the player happens to be standing at call
	// time, matching this session's broader push toward intentional,
	// planned placement rather than player-relative guessing. Sentinel
	// -1000000 (an unrealistic in-game Z) means "not provided" and
	// preserves the prior player-Z-anchored behavior.
	const bool bHasReferenceZ = ReferenceZ > -1000000.0f;
	if (bIgnoreGroundTrace && !bHasReferenceZ)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("MISSING_REFERENCE_Z"),
			TEXT("bIgnoreGroundTrace requires an explicit referenceZ - there is no ground trace to fall back to")));
		return;
	}

	FHitResult SyntheticHit;
	bool bGroundTraceFound = false;
	if (bIgnoreGroundTrace)
	{
		// See this function's header doc comment - deliberately skips
		// FindGroundAtXY entirely, since the whole point is to place at a
		// caller-computed Z no line trace can perturb (edge non-determinism,
		// or open-interior-space fall-through).
		SyntheticHit.Location = FVector(X, Y, ReferenceZ);
		SyntheticHit.ImpactPoint = SyntheticHit.Location;
		SyntheticHit.Normal = FVector::UpVector;
		SyntheticHit.ImpactNormal = FVector::UpVector;
		SyntheticHit.bBlockingHit = true;
	}
	else
	{
		const FVector PlayerLocation = Character->GetActorLocation();
		const float ZSearchCenter = bHasReferenceZ ? ReferenceZ : PlayerLocation.Z;

		const FGroundTraceResult GroundTrace = FindGroundAtXY(World, X, Y, ZSearchCenter, Character);
		if (!GroundTrace.bFound)
		{
			UE_LOG(LogAIModAI, Warning, TEXT("ConstructBuildingAtPosition: ground trace found nothing at (%.0f, %.0f) around Z=%.0f - falling back to that Z"), X, Y, ZSearchCenter);
		}
		SyntheticHit = GroundTrace.Hit;
		bGroundTraceFound = GroundTrace.bFound;
	}

	UE_LOG(LogAIModAI, Display, TEXT("ConstructBuildingAtPosition: recipe=%s ignoreGroundTrace=%s groundTraceHit=%s location=%s"),
		*RecipeClassPath, bIgnoreGroundTrace ? TEXT("true") : TEXT("false"), bGroundTraceFound ? TEXT("true") : TEXT("false"), *SyntheticHit.Location.ToString());

	// faceBuildableId (2026-08-27) - computes TargetYawDegrees from the
	// REAL placement location and an existing buildable's REAL position,
	// instead of requiring the caller to fetch both separately and do
	// this vector math themselves externally (the exact manual dance
	// this project's own placement work has repeated all session for
	// splitters/mergers/hypertube entrances - see
	// docs/placement-lessons.md). Takes priority over an explicit
	// bHasTargetYaw/TargetYawDegrees if both are somehow provided, since
	// a resolved real target is more specific than a raw number.
	if (!FaceBuildableId.IsEmpty())
	{
		AFGBuildable* FaceTarget = FindBuildableById(World, FaceBuildableId);
		if (!FaceTarget)
		{
			OnComplete(FAIModOperationResult::Failure(TEXT("FACE_TARGET_NOT_FOUND"),
				FString::Printf(TEXT("faceBuildableId '%s' did not resolve to an existing buildable"), *FaceBuildableId)));
			return;
		}
		const FRotator FaceRotation = (FaceTarget->GetActorLocation() - SyntheticHit.Location).Rotation();
		bHasTargetYaw = true;
		TargetYawDegrees = FaceRotation.Yaw;
		UE_LOG(LogAIModAI, Display, TEXT("ConstructBuildingAtPosition: faceBuildableId=%s resolved yaw=%.1f"), *FaceBuildableId, TargetYawDegrees);
	}

	// Live investigation (2026-08-25): "Invalid aim location!" was found
	// to persist even at ~100 units distance, directly along the
	// character's own capsule-facing direction, with a confirmed-valid
	// ground trace hit - ruling out both distance and horizontal capsule
	// yaw as the cause. world.player/AFGCharacterPlayer::GetActorRotation()
	// only reflects capsule yaw, not the actual camera pitch/yaw
	// (decoupled in this game, as in most third-person-capable
	// characters) - if the player's camera happens to be pointed
	// somewhere unrelated (e.g. looking down at inventory) when this RPC
	// fires, some disqualifier apparently still consults the real
	// camera/control rotation despite UpdateHologramPlacement()
	// overriding the hologram's *position* every poll tick (see
	// docs/buildgun-driven-placement-research.md's "GetHitResult() alone
	// does not control final placement" finding - this looks like the
	// same class of problem, one layer deeper). Fix: point the
	// controller's ControlRotation at the synthetic hit location before
	// placing, same spirit as overriding GetHitResult() - makes
	// placement work regardless of where the camera actually happens to
	// be aimed, which is the whole point of RPC-driven placement.
	if (AController* Controller = Character->GetController())
	{
		const FRotator LookAtTarget = (SyntheticHit.Location - Character->GetActorLocation()).Rotation();

		// Fix (2026-08-26): only LookAtTarget's PITCH was ever load-bearing
		// for the "Invalid aim location!" fix above - the YAW was not. But
		// AFGHologram::UpdateHologramPlacement() (called every poll tick
		// below, stub source / unreadable) evidently re-derives the
		// hologram's own default (pre-Scroll) facing from the controller's
		// CURRENT yaw each tick. Confirmed live this session: the exact
		// same RotationScrollDelta produced a DIFFERENT resolved yaw
		// depending only on where the player character happened to be
		// standing relative to the target - including a "yaw=0 expected"
		// case with the player nowhere near the target, and even a
		// completely isolated placement far from all other geometry. The
		// resolved (pre-scroll) yaw was consistently just the compass
		// bearing FROM the player's position TO the target, i.e. exactly
		// what LookAtTarget.Yaw computes here. This made every automated,
		// multi-building layout this session non-deterministic and was the
		// root cause of the "chaotic" scattered/misrotated result the user
		// found live via screenshots - not terrain, not gridSnapSize, not
		// per-building randomness. Pinning yaw to a fixed 0 baseline here
		// (independent of player position) makes RotationScrollDelta
		// finally reproducible: delta=0 is always due north, and each
		// scroll click's effect is now relative to that same fixed origin
		// every time, regardless of where the player stands when the RPC
		// fires.
		const FRotator DeterministicLook(LookAtTarget.Pitch, 0.0f, 0.0f);
		Controller->SetControlRotation(DeterministicLook);
	}

	Character->HotKeyRecipe(RecipeClass);

	AFGBuildGun* BuildGun = Character->GetBuildGun();
	if (!BuildGun)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_BUILD_GUN"), TEXT("AFGCharacterPlayer::GetBuildGun() returned null")));
		return;
	}

	UFGBuildGunStateBuild* BuildState = Cast<UFGBuildGunStateBuild>(BuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD));
	if (!BuildState)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_BUILD_STATE"), TEXT("Could not resolve UFGBuildGunStateBuild from the build gun")));
		return;
	}

	AFGHologram* Hologram = BuildState->GetHologram();
	if (!Hologram)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("HOLOGRAM_SPAWN_FAILED"),
			TEXT("HotKeyRecipe did not result in a spawned hologram - recipe may not be a simple single-step buildable")));
		return;
	}

	BuildGun->GetHitResult() = SyntheticHit;

	// Calibration (2026-08-25, revised): a single Scroll(N) call with
	// |N|>1 was found live to behave identically to Scroll(1) - delta=1,2,3
	// all produced the same resolved yaw, and delta=-1,-2 produced the
	// SAME (positive) result as +1,+2, not a negative rotation - the
	// signature of a per-call clamped/smoothed input handler built for
	// one mouse-wheel notch per call, not an arbitrary-magnitude delta
	// encoded in a single call. Calling Scroll(sign) REPEATEDLY,
	// |RotationScrollDelta| times, mimics how a real player's wheel
	// input actually arrives (one small event per call) - applied once,
	// before the poll starts; Scroll()'s effect is expected to persist
	// across the repeated UpdateHologramPlacement() calls in the poll
	// below (mScrollRotation is a UPROPERTY member, not re-derived from
	// the hit each tick). Logging rotation before/after so the RPC
	// caller can read the real effect back via LogAIModAI without a
	// separate throwaway experiment.
	const FRotator RotationBeforeScroll = Hologram->GetActorRotation();
	if (bHasTargetYaw)
	{
		// See this function's header doc for why: Scroll() called N times
		// synchronously is non-linear for |N|>1, so an exact target yaw is
		// set directly instead. Re-asserted every poll tick below since
		// UpdateHologramPlacement() may re-derive/reset yaw each tick.
		Hologram->SetActorRotation(FRotator(0.0f, TargetYawDegrees, 0.0f));
	}
	else if (RotationScrollDelta != 0)
	{
		const int32 ScrollStep = RotationScrollDelta > 0 ? 1 : -1;
		for (int32 i = 0; i < FMath::Abs(RotationScrollDelta); ++i)
		{
			Hologram->Scroll(ScrollStep);
		}
	}
	UE_LOG(LogAIModAI, Display, TEXT("ConstructBuildingAtPosition: rotationScrollDelta=%d hasTargetYaw=%s targetYaw=%.1f rotationBeforeScroll=%s rotationAfterScroll=%s"),
		RotationScrollDelta, bHasTargetYaw ? TEXT("true") : TEXT("false"), TargetYawDegrees, *RotationBeforeScroll.ToString(), *Hologram->GetActorRotation().ToString());

	struct FPollState
	{
		TWeakObjectPtr<AFGHologram> Hologram;
		TWeakObjectPtr<AFGCharacterPlayer> Character;
		TWeakObjectPtr<UWorld> World;
		FString RecipeClassPath;
		FHitResult SyntheticHit;
		TFunction<void(const FAIModOperationResult&)> OnComplete;
		int32 AttemptsRemaining = 120; // safety cap - real ticks, not a fixed duration
		int32 AttemptsTaken = 0;
		bool bIgnoreAimLocation = false;
		bool bIgnorePlayerEncroachment = false;
		bool bIgnoreClearance = false;
		bool bIgnoreInvalidFloor = false;
		bool bHasTargetYaw = false;
		float TargetYawDegrees = 0.0f;
	};
	const TSharedRef<FPollState> PollState = MakeShared<FPollState>();
	PollState->Hologram = Hologram;
	PollState->Character = Character;
	PollState->World = World;
	PollState->RecipeClassPath = RecipeClassPath;
	PollState->SyntheticHit = SyntheticHit;
	PollState->bIgnoreAimLocation = bIgnoreAimLocation;
	PollState->bIgnorePlayerEncroachment = bIgnorePlayerEncroachment;
	PollState->bIgnoreClearance = bIgnoreClearance;
	PollState->bIgnoreInvalidFloor = bIgnoreInvalidFloor;
	PollState->bHasTargetYaw = bHasTargetYaw;
	PollState->TargetYawDegrees = TargetYawDegrees;
	PollState->OnComplete = MoveTemp(OnComplete);

	const TSharedRef<TFunction<void()>> PollFn = MakeShared<TFunction<void()>>();
	*PollFn = [PollState, PollFn]()
	{
		++PollState->AttemptsTaken;

		AFGHologram* PollHologram = PollState->Hologram.Get();
		UWorld* PollWorld = PollState->World.Get();
		AFGCharacterPlayer* PollCharacter = PollState->Character.Get();
		if (!IsValid(PollHologram) || !PollWorld)
		{
			UE_LOG(LogAIModAI, Warning, TEXT("ConstructBuildingAtPosition (deferred): hologram or world became invalid while polling (after %d tick(s)) - nothing built"), PollState->AttemptsTaken);
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("HOLOGRAM_INVALIDATED"), TEXT("Hologram or world became invalid while polling")));
			return;
		}

		// Re-assert every tick, not just once before the loop (2026-08-26):
		// live-diagnosed with the user hovering on a jetpack at the time -
		// the one-time SetControlRotation() before this poll loop starts
		// can get overridden by the game's own ongoing camera/flight input
		// before a LATER tick's UpdateHologramPlacement() call reads it,
		// since AFGHologram evidently re-derives its default yaw from
		// whatever the controller's CURRENT rotation is on every tick (see
		// the fix comment above this poll loop for the original diagnosis).
		// A player standing still never showed this because their control
		// rotation wasn't changing tick-to-tick regardless of whether this
		// was re-applied - it only surfaces when the player is actively
		// looking around (e.g. mid-flight) while a poll spans multiple
		// ticks (bStillInitializing retries).
		if (IsValid(PollCharacter))
		{
			if (AController* PollController = PollCharacter->GetController())
			{
				const FRotator PollLookAtTarget = (PollState->SyntheticHit.Location - PollCharacter->GetActorLocation()).Rotation();
				PollController->SetControlRotation(FRotator(PollLookAtTarget.Pitch, 0.0f, 0.0f));
			}
		}

		PollHologram->UpdateHologramPlacement(PollState->SyntheticHit);

		// Re-assert the exact target yaw every tick too, for the same
		// reason the camera look direction is re-asserted above -
		// UpdateHologramPlacement() may have just reset it.
		if (PollState->bHasTargetYaw)
		{
			PollHologram->SetActorRotation(FRotator(0.0f, PollState->TargetYawDegrees, 0.0f));
		}

		TArray<TSubclassOf<UFGConstructDisqualifier>> Disqualifiers;
		PollHologram->GetConstructDisqualifiers(Disqualifiers);
		const bool bStillInitializing = Disqualifiers.Contains(TSubclassOf<UFGConstructDisqualifier>(UFGCDInitializing::StaticClass()));

		--PollState->AttemptsRemaining;
		if (bStillInitializing && PollState->AttemptsRemaining > 0)
		{
			PollWorld->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
			return;
		}

		// Real CanConstruct() (whatever its internal logic actually is -
		// stub source, unreadable) is intentionally NOT used as the gate
		// here when any bIgnore* flag is set. Instead: replicate the
		// documented "any non-soft (hard) disqualifier blocks" rule
		// ourselves via the same GetIsSoftDisqualifier() query used for
		// logging everywhere else in this file, skipping specific
		// disqualifier classes the caller explicitly opted to ignore.
		// Added 2026-08-25 per explicit user direction: player-proximity/
		// camera-direction/clearance gates don't scale for large,
		// autonomous, multi-building layouts, and the user explicitly
		// accepts the risk of invalid terrain collisions in exchange.
		// This does NOT bypass FactoryGame's OWN validation inside
		// InternalConstructHologram() itself (unknown/unverified from
		// source) - only AIMod's decision to attempt construction.
		// UnlimitedResources (2026-08-27) - a player-controlled mod
		// setting (AIModConfiguration.h), NOT another bIgnore* request
		// param like the flags above - the caller can't opt into this,
		// only the player can via the settings menu. Computed once per
		// poll tick, not per disqualifier, to avoid a config lookup per
		// entry in Disqualifiers.
		const bool bUnlimitedResources = UAIModFunctionLibrary::GetAIModConfigBool(PollWorld, TEXT("UnlimitedResources"), false);

		bool bCanConstruct = true;
		TArray<FString> DisqualifierTexts;
		for (const TSubclassOf<UFGConstructDisqualifier>& DisqualifierClass : Disqualifiers)
		{
			const bool bIgnoredByFlag =
				(PollState->bIgnoreAimLocation && DisqualifierClass == UFGCDInvalidAimLocation::StaticClass()) ||
				(PollState->bIgnorePlayerEncroachment && DisqualifierClass == UFGCDEncroachingPlayer::StaticClass()) ||
				(PollState->bIgnoreClearance && DisqualifierClass == UFGCDEncroachingClearance::StaticClass()) ||
				(PollState->bIgnoreInvalidFloor && DisqualifierClass == UFGCDInvalidFloor::StaticClass()) ||
				(bUnlimitedResources && DisqualifierClass == UFGCDUnaffordable::StaticClass());
			const bool bIsSoft = UFGConstructDisqualifier::GetIsSoftDisqualifier(DisqualifierClass);
			if (!bIgnoredByFlag && !bIsSoft)
			{
				bCanConstruct = false;
			}
			DisqualifierTexts.Add(FString::Printf(TEXT("%s (%s%s)"),
				*UFGConstructDisqualifier::GetDisqualifyingText(DisqualifierClass).ToString(),
				bIsSoft ? TEXT("soft") : TEXT("hard"), bIgnoredByFlag ? TEXT(", ignored") : TEXT("")));
		}
		const FString DisqualifierSummary = DisqualifierTexts.IsEmpty() ? TEXT("<none>") : FString::Join(DisqualifierTexts, TEXT("; "));

		UE_LOG(LogAIModAI, Display, TEXT("ConstructBuildingAtPosition (deferred, resolved after %d real tick(s)): resolvedRotation=%s canConstruct=%s"),
			PollState->AttemptsTaken, *PollHologram->GetActorRotation().ToString(), bCanConstruct ? TEXT("true") : TEXT("false"));

		if (!bCanConstruct)
		{
			UE_LOG(LogAIModAI, Display, TEXT("ConstructBuildingAtPosition (deferred, resolved after %d real tick(s)): CanConstruct()=false, NOT constructing - recipe=%s disqualifiers=[%s]"),
				PollState->AttemptsTaken, *PollState->RecipeClassPath, *DisqualifierSummary);
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("CANNOT_CONSTRUCT"), DisqualifierSummary));
			return;
		}

		AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(PollWorld);
		const FNetConstructionID ConstructionID = BuildableSubsystem ? BuildableSubsystem->GetNewNetConstructionID() : FNetConstructionID();

		AFGBuildGun* PollBuildGun = IsValid(PollCharacter) ? PollCharacter->GetBuildGun() : nullptr;
		UFGBuildGunStateBuild* PollBuildState = PollBuildGun ? Cast<UFGBuildGunStateBuild>(PollBuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD)) : nullptr;
		if (!PollBuildState)
		{
			UE_LOG(LogAIModAI, Error, TEXT("ConstructBuildingAtPosition (deferred): lost the build state before constructing - aborting, nothing built"));
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("Lost the build state before constructing")));
			return;
		}

		const FVector ConstructLocation = PollHologram->GetActorLocation();
		PollBuildState->InternalConstructHologram(ConstructionID);

		// Identify the newly-constructed buildable by proximity to where
		// we just built - InternalConstructHologram is void and there's
		// no direct return value, but nothing else should legitimately
		// exist within a couple hundred units of a spot CanConstruct()
		// just validated as clear a moment ago.
		FString ConstructedBuildableId;
		if (BuildableSubsystem)
		{
			float BestDistSq = TNumericLimits<float>::Max();
			AFGBuildable* BestMatch = nullptr;
			for (AFGBuildable* Candidate : BuildableSubsystem->GetAllBuildablesRef())
			{
				if (!IsValid(Candidate)) { continue; }
				const float DistSq = FVector::DistSquared(Candidate->GetActorLocation(), ConstructLocation);
				if (DistSq < BestDistSq)
				{
					BestDistSq = DistSq;
					BestMatch = Candidate;
				}
			}
			if (BestMatch && BestDistSq < FMath::Square(200.0f))
			{
				ConstructedBuildableId = BestMatch->GetPathName();
			}
		}

		if (ConstructedBuildableId.IsEmpty())
		{
			// The regular-actor search above found nothing - the recipe
			// may have produced a lightweight buildable instead
			// (foundations, likely other mass-placed pieces - see
			// docs/lightweight-buildable-research.md). Search
			// AFGLightweightBuildableSubsystem's instances for the
			// recipe's buildable class by proximity, same 200-unit
			// tolerance as the regular-actor search.
			if (AFGLightweightBuildableSubsystem* LightweightSubsystem = AFGLightweightBuildableSubsystem::Get(PollWorld))
			{
				const TSubclassOf<AFGBuildable> BuildableClass = ResolveBuildableClassForRecipe(PollState->RecipeClassPath);
				if (const TArray<FRuntimeBuildableInstanceData>* Instances = BuildableClass ? LightweightSubsystem->GetAllLightweightBuildableInstances().Find(BuildableClass) : nullptr)
				{
					float BestDistSq = TNumericLimits<float>::Max();
					int32 BestIndex = INDEX_NONE;
					for (int32 Index = 0; Index < Instances->Num(); ++Index)
					{
						const FRuntimeBuildableInstanceData& InstanceData = (*Instances)[Index];
						if (!InstanceData.IsValid()) { continue; }
						const float DistSq = FVector::DistSquared(InstanceData.Transform.GetLocation(), ConstructLocation);
						if (DistSq < BestDistSq)
						{
							BestDistSq = DistSq;
							BestIndex = Index;
						}
					}
					if (BestIndex != INDEX_NONE && BestDistSq < FMath::Square(200.0f))
					{
						ConstructedBuildableId = MakeLightweightBuildableId(BuildableClass, BestIndex);
					}
				}
			}
		}

		UE_LOG(LogAIModAI, Display, TEXT("ConstructBuildingAtPosition (deferred, resolved after %d real tick(s)): construction attempted via InternalConstructHologram - recipe=%s location=%s id=%s"),
			PollState->AttemptsTaken, *PollState->RecipeClassPath, *ConstructLocation.ToString(), *ConstructedBuildableId);

		if (IsValid(PollCharacter))
		{
			PollCharacter->UnequipBuildGun();
		}

		PollState->OnComplete(ConstructedBuildableId.IsEmpty()
			? FAIModOperationResult::Success()
			: FAIModOperationResult::SuccessWithBuildableId(ConstructedBuildableId));
	};

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
}

namespace
{
	AFGBuildable* FindBuildableById(UWorld* World, const FString& BuildableId)
	{
		if (AFGBuildableSubsystem* Subsystem = AFGBuildableSubsystem::Get(World))
		{
			for (AFGBuildable* Candidate : Subsystem->GetAllBuildablesRef())
			{
				if (IsValid(Candidate) && Candidate->GetPathName() == BuildableId)
				{
					return Candidate;
				}
			}
		}
		return nullptr;
	}

	// Same generic-discovery pattern CollectFactoryConnectionTelemetry
	// uses for UFGFactoryConnectionComponent, mirrored here for power -
	// see docs/conveyor-power-connection-research.md.
	UFGPowerConnectionComponent* FindFreePowerConnection(AFGBuildable* Buildable)
	{
		TArray<UFGPowerConnectionComponent*> PowerConnections;
		Buildable->GetComponents<UFGPowerConnectionComponent>(PowerConnections);
		for (UFGPowerConnectionComponent* Connection : PowerConnections)
		{
			if (IsValid(Connection) && Connection->GetNumFreeConnections() > 0)
			{
				return Connection;
			}
		}
		return nullptr;
	}

	// Same pattern as FindFreePowerConnection, for the conveyor-belt
	// snap-target experiment - matches the given direction (Output for
	// the belt's start point) and isn't already connected.
	UFGFactoryConnectionComponent* FindFreeFactoryConnection(AFGBuildable* Buildable, EFactoryConnectionDirection Direction)
	{
		TArray<UFGFactoryConnectionComponent*> Connections;
		Buildable->GetComponents<UFGFactoryConnectionComponent>(Connections);
		for (UFGFactoryConnectionComponent* Connection : Connections)
		{
			if (IsValid(Connection) && Connection->GetDirection() == Direction && !Connection->IsConnected())
			{
				return Connection;
			}
		}
		return nullptr;
	}
}

FAIModOperationResult UAIModFunctionLibrary::DismantleBuildable(UObject* WorldContextObject, const FString& BuildableId)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}

	AFGBuildable* Buildable = nullptr;

	if (IsLightweightBuildableId(BuildableId))
	{
		// Lightweight buildables (foundations, likely walls/other
		// mass-placed pieces - see docs/lightweight-buildable-research.md)
		// aren't actors, so there's nothing to look up directly. Resolve
		// the class+index, then materialize a real, temporary
		// AFGBuildable* via FindOrSpawnBuildableForRuntimeData() and reuse
		// the exact same Execute_Dismantle() call below -
		// AFGBuildable::Dismantle_Implementation() already has a
		// dedicated mIsLightweightTemporary branch that correctly calls
		// AFGLightweightBuildableSubsystem::RemoveByInstanceIndex() for
		// us (confirmed by reading FGBuildable.cpp), so there's no need
		// to duplicate that logic here.
		FString ClassPath;
		int32 Index = INDEX_NONE;
		if (!ParseLightweightBuildableId(BuildableId, ClassPath, Index))
		{
			return FAIModOperationResult::Failure(TEXT("INVALID_REQUEST"),
				FString::Printf(TEXT("'%s' is not a well-formed lightweight buildable id"), *BuildableId));
		}

		UClass* ResolvedClass = LoadObject<UClass>(nullptr, *ClassPath);
		const TSubclassOf<AFGBuildable> BuildableClass = (ResolvedClass && ResolvedClass->IsChildOf(AFGBuildable::StaticClass())) ? ResolvedClass : nullptr;
		if (!BuildableClass)
		{
			return FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"),
				FString::Printf(TEXT("'%s' did not resolve to an AFGBuildable subclass"), *ClassPath));
		}

		AFGLightweightBuildableSubsystem* LightweightSubsystem = AFGLightweightBuildableSubsystem::Get(World);
		FRuntimeBuildableInstanceData* RuntimeData = LightweightSubsystem ? LightweightSubsystem->GetRuntimeDataForBuildableClassAndIndex(BuildableClass, Index) : nullptr;
		if (!RuntimeData || !RuntimeData->IsValid())
		{
			return FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"),
				FString::Printf(TEXT("No lightweight buildable found with id '%s'"), *BuildableId));
		}

		bool bDidSpawn = false;
		FInstanceToTemporaryBuildable* Temporary = LightweightSubsystem->FindOrSpawnBuildableForRuntimeData(BuildableClass, RuntimeData, Index, bDidSpawn);
		if (!Temporary || !Temporary->IsValid())
		{
			return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"),
				FString::Printf(TEXT("Failed to materialize a temporary buildable for '%s'"), *BuildableId));
		}
		Buildable = Temporary->Buildable;
	}
	else
	{
		Buildable = FindBuildableById(World, BuildableId);
	}

	if (!Buildable)
	{
		return FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"),
			FString::Printf(TEXT("No buildable found with id '%s'"), *BuildableId));
	}

	if (!IFGDismantleInterface::Execute_CanDismantle(Buildable))
	{
		return FAIModOperationResult::Failure(TEXT("CANNOT_DISMANTLE"),
			FString::Printf(TEXT("'%s' cannot currently be dismantled (already dismantled, or has an un-dismantled parent)"), *BuildableId));
	}

	// Some buildables reference a separate, independently-existing child
	// actor that Dismantle_Implementation() itself does not clean up -
	// e.g. AFGBuildablePipeline's mFlowIndicator (a distinct
	// AFGBuildablePipelineFlowIndicator actor, not a child component that
	// would auto-destroy with its owner). GetChildDismantleActors exists
	// specifically for this ("If we want to dismantle something else
	// along with this, then add it through this" - FGDismantleInterface.h);
	// the real player-driven path (UFGBuildGunStateDismantle) consults it,
	// but Execute_Dismantle() on the buildable itself does not call it
	// automatically. Without this, deleting a pipe via world.deleteBuilding
	// left its fluid-fill indicator floating in place - confirmed live
	// 2026-08-27, reported directly by the user. Gather while the
	// buildable is still valid, dismantle each child first, then the
	// buildable itself.
	TArray<AActor*> ChildDismantleActors;
	IFGDismantleInterface::Execute_GetChildDismantleActors(Buildable, ChildDismantleActors);
	for (AActor* ChildActor : ChildDismantleActors)
	{
		if (IsValid(ChildActor) && ChildActor->Implements<UFGDismantleInterface>()
			&& IFGDismantleInterface::Execute_CanDismantle(ChildActor))
		{
			IFGDismantleInterface::Execute_Dismantle(ChildActor);
		}
	}

	// Real, safe dismantle - see this function's header doc comment.
	// AFGBuildable::Dismantle_Implementation() handles connection
	// cleanup, inventory locking/emptying, subsystem deregistration, and
	// network-replicated actor destruction; this is not AActor::Destroy().
	IFGDismantleInterface::Execute_Dismantle(Buildable);

	UE_LOG(LogAIModAI, Display, TEXT("DismantleBuildable: %s (%d child actor(s) dismantled)"), *BuildableId, ChildDismantleActors.Num());

	return FAIModOperationResult::Success();
}

FAIModOperationResult UAIModFunctionLibrary::DebugCheckPowerConnection(UObject* WorldContextObject, const FString& BuildableIdA, const FString& BuildableIdB)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}

	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Character)
	{
		return FAIModOperationResult::Failure(TEXT("NO_PLAYER"), TEXT("No local AFGCharacterPlayer (player index 0)"));
	}

	AFGBuildable* BuildableA = FindBuildableById(World, BuildableIdA);
	if (!BuildableA)
	{
		return FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No buildable found with id '%s'"), *BuildableIdA));
	}
	AFGBuildable* BuildableB = FindBuildableById(World, BuildableIdB);
	if (!BuildableB)
	{
		return FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No buildable found with id '%s'"), *BuildableIdB));
	}

	UFGPowerConnectionComponent* ConnectionA = FindFreePowerConnection(BuildableA);
	if (!ConnectionA)
	{
		return FAIModOperationResult::Failure(TEXT("NO_POWER_CONNECTION"), FString::Printf(TEXT("'%s' has no free power connection component"), *BuildableIdA));
	}
	UFGPowerConnectionComponent* ConnectionB = FindFreePowerConnection(BuildableB);
	if (!ConnectionB)
	{
		return FAIModOperationResult::Failure(TEXT("NO_POWER_CONNECTION"), FString::Printf(TEXT("'%s' has no free power connection component"), *BuildableIdB));
	}

	// Verified to exist as a real asset in Content/FactoryGame/Recipes/Buildings/
	// (see docs/conveyor-power-connection-research.md) - the build-cost
	// recipe for a plain power line/wire, not guessed from memory.
	UClass* PowerLineRecipeClass = LoadObject<UClass>(nullptr, TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PowerLine.Recipe_PowerLine_C"));
	if (!PowerLineRecipeClass || !PowerLineRecipeClass->IsChildOf(UFGRecipe::StaticClass()))
	{
		return FAIModOperationResult::Failure(TEXT("RECIPE_LOAD_FAILED"), TEXT("Failed to load Recipe_PowerLine as a UFGRecipe"));
	}
	const TSubclassOf<UFGRecipe> RecipeClass = PowerLineRecipeClass;

	Character->HotKeyRecipe(RecipeClass);

	AFGBuildGun* BuildGun = Character->GetBuildGun();
	if (!BuildGun)
	{
		Character->UnequipBuildGun();
		return FAIModOperationResult::Failure(TEXT("NO_BUILD_GUN"), TEXT("AFGCharacterPlayer::GetBuildGun() returned null"));
	}

	UFGBuildGunStateBuild* BuildState = Cast<UFGBuildGunStateBuild>(BuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD));
	if (!BuildState)
	{
		Character->UnequipBuildGun();
		return FAIModOperationResult::Failure(TEXT("NO_BUILD_STATE"), TEXT("Could not resolve UFGBuildGunStateBuild from the build gun"));
	}

	AFGHologram* Hologram = BuildState->GetHologram();
	AFGWireHologram* WireHologram = Cast<AFGWireHologram>(Hologram);
	if (!WireHologram)
	{
		Character->UnequipBuildGun();
		return FAIModOperationResult::Failure(TEXT("HOLOGRAM_SPAWN_FAILED"),
			FString::Printf(TEXT("HotKeyRecipe(Recipe_PowerLine) did not result in an AFGWireHologram (got %s)"),
				Hologram ? *Hologram->GetClass()->GetName() : TEXT("null")));
	}

	// Untested assumption being probed here (docs/conveyor-power-connection-research.md):
	// whether SetConnection() alone is enough, or whether the real
	// multi-step TrySnapToActor/DoMultiStepPlacement flow needs to run
	// first. Feed a synthetic hit result at ConnectionA's location first,
	// same pattern as every other build-gun-driven function, in case
	// UpdateHologramPlacement is a precondition CanConstruct() depends on.
	FHitResult SyntheticHit;
	SyntheticHit.Location = ConnectionA->GetComponentLocation();
	SyntheticHit.ImpactPoint = SyntheticHit.Location;
	SyntheticHit.Normal = FVector::UpVector;
	SyntheticHit.ImpactNormal = FVector::UpVector;
	SyntheticHit.HitObjectHandle = FActorInstanceHandle(BuildableA);
	// UFGPowerConnectionComponent derives from USceneComponent (via
	// UFGCircuitConnectionComponent/UFGConnectionComponent), not
	// UPrimitiveComponent - FHitResult::Component can't reference it
	// directly, so it's left unset here.
	SyntheticHit.bBlockingHit = true;
	BuildGun->GetHitResult() = SyntheticHit;

	WireHologram->SetConnection(0, ConnectionA);
	WireHologram->SetConnection(1, ConnectionB);

	struct FPollState
	{
		TWeakObjectPtr<AFGWireHologram> Hologram;
		TWeakObjectPtr<AFGCharacterPlayer> Character;
		TWeakObjectPtr<UWorld> World;
		FHitResult SyntheticHit;
		FString BuildableIdA;
		FString BuildableIdB;
		int32 AttemptsRemaining = 120; // safety cap - real ticks, not a fixed duration
		int32 AttemptsTaken = 0;
	};
	const TSharedRef<FPollState> PollState = MakeShared<FPollState>();
	PollState->Hologram = WireHologram;
	PollState->Character = Character;
	PollState->World = World;
	PollState->SyntheticHit = SyntheticHit;
	PollState->BuildableIdA = BuildableIdA;
	PollState->BuildableIdB = BuildableIdB;

	const TSharedRef<TFunction<void()>> PollFn = MakeShared<TFunction<void()>>();
	*PollFn = [PollState, PollFn]()
	{
		++PollState->AttemptsTaken;

		AFGWireHologram* PollHologram = PollState->Hologram.Get();
		UWorld* PollWorld = PollState->World.Get();
		AFGCharacterPlayer* PollCharacter = PollState->Character.Get();
		if (!IsValid(PollHologram) || !PollWorld)
		{
			UE_LOG(LogAIModAI, Warning, TEXT("DebugCheckPowerConnection (deferred): hologram or world became invalid while polling (after %d tick(s))"), PollState->AttemptsTaken);
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			return;
		}

		PollHologram->UpdateHologramPlacement(PollState->SyntheticHit);

		TArray<TSubclassOf<UFGConstructDisqualifier>> Disqualifiers;
		PollHologram->GetConstructDisqualifiers(Disqualifiers);
		const bool bStillInitializing = Disqualifiers.Contains(TSubclassOf<UFGConstructDisqualifier>(UFGCDInitializing::StaticClass()));

		--PollState->AttemptsRemaining;
		if (bStillInitializing && PollState->AttemptsRemaining > 0)
		{
			PollWorld->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
			return;
		}

		const bool bCanConstruct = PollHologram->CanConstruct();
		TArray<FString> DisqualifierTexts;
		for (const TSubclassOf<UFGConstructDisqualifier>& DisqualifierClass : Disqualifiers)
		{
			DisqualifierTexts.Add(FString::Printf(TEXT("%s (%s)"),
				*UFGConstructDisqualifier::GetDisqualifyingText(DisqualifierClass).ToString(),
				UFGConstructDisqualifier::GetIsSoftDisqualifier(DisqualifierClass) ? TEXT("soft") : TEXT("hard")));
		}
		const FString DisqualifierSummary = DisqualifierTexts.IsEmpty() ? TEXT("<none>") : FString::Join(DisqualifierTexts, TEXT("; "));

		UE_LOG(LogAIModAI, Display, TEXT("DebugCheckPowerConnection (deferred, resolved after %d real tick(s)): a=%s b=%s canConstruct=%s disqualifiers=[%s]"),
			PollState->AttemptsTaken, *PollState->BuildableIdA, *PollState->BuildableIdB, bCanConstruct ? TEXT("true") : TEXT("false"), *DisqualifierSummary);

		if (IsValid(PollCharacter))
		{
			PollCharacter->UnequipBuildGun();
		}
	};

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));

	return FAIModOperationResult::Failure(TEXT("PENDING"),
		TEXT("Scheduled via the real build gun - dry-run only, never constructs; see LogAIModAI for the real result"));
}

void UAIModFunctionLibrary::ConstructExtractorOnNode(UObject* WorldContextObject, const FString& NodeId, const FString& RecipeClassPath, TFunction<void(const FAIModOperationResult&)> OnComplete)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context")));
		return;
	}

	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Character)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_PLAYER"), TEXT("No local AFGCharacterPlayer (player index 0)")));
		return;
	}

	// AFGResourceNodeBase (2026-08-27, was AFGResourceNode) - a strictly
	// wider search. AFGResourceNode already covered normal nodes and
	// Fracking Satellites (AFGResourceNodeFrackingSatellite : AFGResourceNode).
	// AFGResourceNodeFrackingCore is NOT an AFGResourceNode - it derives
	// from AFGResourceNodeBase directly - so a Resource Well Pressurizer's
	// target node was previously unreachable here at all. See this
	// function's header doc comment / docs/resource-well-research.md.
	AFGResourceNodeBase* TargetNode = nullptr;
	for (TActorIterator<AFGResourceNodeBase> It(World); It; ++It)
	{
		if (IsValid(*It) && It->GetPathName() == NodeId)
		{
			TargetNode = *It;
			break;
		}
	}
	if (!TargetNode)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NODE_NOT_FOUND"), FString::Printf(TEXT("No resource node found with id '%s'"), *NodeId)));
		return;
	}

	if (TargetNode->IsOccupied())
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NODE_OCCUPIED"), TEXT("Targeted node is already occupied")));
		return;
	}

	// LimitBuildDistance (2026-08-27) - see ConstructBuildingAtPosition's
	// identical check/comment for the full rationale (player-controlled
	// mod setting, off by default, new synthetic restriction that doesn't
	// exist in the base game or in AIMod's prior behavior).
	if (UAIModFunctionLibrary::GetAIModConfigBool(World, TEXT("LimitBuildDistance"), false))
	{
		const float MaxBuildDistance = UAIModFunctionLibrary::GetAIModConfigFloat(World, TEXT("MaxBuildDistance"), 8000.0f);
		const float DistanceFromPlayer = FVector::Dist2D(Character->GetActorLocation(), TargetNode->GetActorLocation());
		if (DistanceFromPlayer > MaxBuildDistance)
		{
			OnComplete(FAIModOperationResult::Failure(TEXT("BUILD_DISTANCE_EXCEEDED"),
				FString::Printf(TEXT("Target node is %.0f units from the player, exceeding the configured Max Build Distance of %.0f units (Limit RPC Build Distance From Player is enabled in AIMod's mod settings)"),
					DistanceFromPlayer, MaxBuildDistance)));
			return;
		}
	}

	// The RF_SOLID-only gate this function used to enforce manually is
	// gone (2026-08-27) - see this function's header doc comment for why
	// (it only ever existed from being written/tested against Miners
	// first; the real engine-side gating already does this correctly for
	// every extractor type, including rejecting a mismatched recipe/node
	// pairing via UFGCDNeedsFrackingCoreNode/UFGCDNeedsFrackingSatelliteNode -
	// trust CanConstruct() for it the same way this function already does
	// for every other disqualifier).
	UClass* ResolvedRecipeClass = LoadObject<UClass>(nullptr, *RecipeClassPath);
	if (!ResolvedRecipeClass || !ResolvedRecipeClass->IsChildOf(UFGRecipe::StaticClass()))
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("INVALID_RECIPE"), FString::Printf(TEXT("'%s' did not resolve to a UFGRecipe subclass"), *RecipeClassPath)));
		return;
	}
	const TSubclassOf<UFGRecipe> RecipeClass = ResolvedRecipeClass;

	Character->HotKeyRecipe(RecipeClass);

	AFGBuildGun* BuildGun = Character->GetBuildGun();
	if (!BuildGun)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_BUILD_GUN"), TEXT("AFGCharacterPlayer::GetBuildGun() returned null")));
		return;
	}

	UFGBuildGunStateBuild* BuildState = Cast<UFGBuildGunStateBuild>(BuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD));
	if (!BuildState)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_BUILD_STATE"), TEXT("Could not resolve UFGBuildGunStateBuild from the build gun")));
		return;
	}

	AFGHologram* Hologram = BuildState->GetHologram();
	if (!Hologram)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("HOLOGRAM_SPAWN_FAILED"), TEXT("HotKeyRecipe did not result in a spawned hologram")));
		return;
	}

	const FVector RawLocation = TargetNode->GetActorLocation();
	const FVector PlacementLocation = TargetNode->GetPlacementLocation(RawLocation);
	const FRotator PlacementRotation = TargetNode->GetPlacementRotation(RawLocation);

	FHitResult SyntheticHit;
	SyntheticHit.Location = PlacementLocation;
	SyntheticHit.ImpactPoint = PlacementLocation;
	SyntheticHit.Normal = PlacementRotation.RotateVector(FVector::UpVector);
	SyntheticHit.ImpactNormal = SyntheticHit.Normal;
	SyntheticHit.HitObjectHandle = FActorInstanceHandle(TargetNode);
	SyntheticHit.bBlockingHit = true;
	if (UPrimitiveComponent* NodePrimitive = Cast<UPrimitiveComponent>(TargetNode->GetRootComponent()))
	{
		SyntheticHit.Component = NodePrimitive;
	}
	// Distance (2026-08-26) - CONFIRMED LIVE root cause of a real
	// regression: this synthetic hit's Distance field was left at the
	// FHitResult default (0.f) while every other field (Location,
	// Normal, Component, HitObjectHandle) was deliberately populated to
	// look like a real trace result. AFGResourceExtractorHologram's
	// internal placement validation evidently sanity-checks Distance -
	// a zero-distance "hit" was rejected with UFGCDNeedsResourceNode
	// ("Must be placed on a Resource Node!") even though the hit
	// otherwise correctly identified the target node (confirmed via a
	// diagnostic pass: GetPlacementLocation()/GetPlacementRotation()
	// were sane, hologram class was genuinely AFGResourceExtractorHologram -
	// both ruled out first). A real build-gun trace always has a
	// positive camera-to-hit distance; this now sets one.
	SyntheticHit.Distance = FVector::Dist(Character->GetActorLocation(), PlacementLocation);

	BuildGun->GetHitResult() = SyntheticHit;

	// Player-independence (2026-08-27, applying the same fix already
	// proven for ConstructConveyorBelt/ConstructConveyorLift/ConstructPipe -
	// see their comments for the full incident): this function predates
	// that fix and was never updated - live-confirmed this session it
	// fails with "Invalid aim location!" even for a fully valid,
	// unoccupied Fracking Core node. Point the controller at a
	// deterministic target (the real placement location, never the
	// player's actual aim), reasserted every poll tick below.
	const FRotator ExtractorDeterministicLook = (PlacementLocation - Character->GetActorLocation()).Rotation();
	if (AController* ExtractorController = Character->GetController())
	{
		ExtractorController->SetControlRotation(ExtractorDeterministicLook);
	}

	// TrySnapToActor() call added 2026-08-26 alongside the Distance fix
	// above while chasing the same live UFGCDNeedsResourceNode failure -
	// this function previously relied solely on UpdateHologramPlacement()
	// (called every poll tick below) plus the raw BuildGun->GetHitResult()
	// assignment, with no explicit snap call, unlike every other
	// click-driven Construct* function in this file (belts/pipes/lifts),
	// which all call Hologram->TrySnapToActor(Hit) explicitly.
	// AFGResourceExtractorHologram's own TrySnapToActor() override calls
	// TrySnapToExtractableResource() internally (confirmed from source)
	// to populate mSnappedExtractableResource. NOTE: live-confirmed that
	// the Distance fix above was the actual fix for the failure this was
	// added to chase (added in the same redeploy, so this call's own
	// contribution is unconfirmed) - kept because it matches the
	// already-proven pattern elsewhere in this file and mSnappedExtractableResource
	// still needs populating correctly for the extractor to actually
	// function, not just pass the disqualifier check.
	Hologram->UpdateHologramPlacement(SyntheticHit);
	Hologram->TrySnapToActor(SyntheticHit);

	struct FPollState
	{
		TWeakObjectPtr<AFGHologram> Hologram;
		TWeakObjectPtr<AFGCharacterPlayer> Character;
		TWeakObjectPtr<AFGResourceNodeBase> TargetNode;
		TWeakObjectPtr<UWorld> World;
		FString NodeId;
		FHitResult SyntheticHit;
		FRotator DeterministicLook;
		TFunction<void(const FAIModOperationResult&)> OnComplete;
		int32 AttemptsRemaining = 120; // safety cap - real ticks, not a fixed duration
		int32 AttemptsTaken = 0;
	};
	const TSharedRef<FPollState> PollState = MakeShared<FPollState>();
	PollState->Hologram = Hologram;
	PollState->Character = Character;
	PollState->TargetNode = TargetNode;
	PollState->World = World;
	PollState->NodeId = NodeId;
	PollState->SyntheticHit = SyntheticHit;
	PollState->DeterministicLook = ExtractorDeterministicLook;
	PollState->OnComplete = MoveTemp(OnComplete);

	const TSharedRef<TFunction<void()>> PollFn = MakeShared<TFunction<void()>>();
	*PollFn = [PollState, PollFn]()
	{
		++PollState->AttemptsTaken;

		AFGHologram* PollHologram = PollState->Hologram.Get();
		UWorld* PollWorld = PollState->World.Get();
		AFGCharacterPlayer* PollCharacter = PollState->Character.Get();
		AFGResourceNodeBase* PollTargetNode = PollState->TargetNode.Get();
		if (!IsValid(PollHologram) || !PollWorld)
		{
			UE_LOG(LogAIModAI, Warning, TEXT("ConstructExtractorOnNode (deferred): hologram or world became invalid while polling (after %d tick(s)) - nothing built"), PollState->AttemptsTaken);
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("HOLOGRAM_INVALIDATED"), TEXT("Hologram or world became invalid while polling")));
			return;
		}

		// Re-assert every tick - see ConstructConveyorBelt's identical block
		// for the full rationale (player camera movement between ticks can
		// still drag the resolved result off a one-time value).
		if (IsValid(PollCharacter))
		{
			if (AController* PollController = PollCharacter->GetController())
			{
				PollController->SetControlRotation(PollState->DeterministicLook);
			}
		}

		PollHologram->UpdateHologramPlacement(PollState->SyntheticHit);

		TArray<TSubclassOf<UFGConstructDisqualifier>> Disqualifiers;
		PollHologram->GetConstructDisqualifiers(Disqualifiers);
		const bool bStillInitializing = Disqualifiers.Contains(TSubclassOf<UFGConstructDisqualifier>(UFGCDInitializing::StaticClass()));

		--PollState->AttemptsRemaining;
		if (bStillInitializing && PollState->AttemptsRemaining > 0)
		{
			PollWorld->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
			return;
		}

		// Player-independence (2026-08-27) - same manual disqualifier-ignore
		// pattern as ConstructConveyorBelt, replacing the real (opaque)
		// CanConstruct() this function used to call directly.
		// UnlimitedResources (2026-08-27) - see ConstructBuildingAtPosition's
		// comment on this being a player-controlled mod setting, not a
		// per-call flag.
		const bool bUnlimitedResources = UAIModFunctionLibrary::GetAIModConfigBool(PollWorld, TEXT("UnlimitedResources"), false);

		bool bCanConstruct = true;
		TArray<FString> DisqualifierTexts;
		for (const TSubclassOf<UFGConstructDisqualifier>& DisqualifierClass : Disqualifiers)
		{
			const bool bIgnoredForPlayerIndependence = (DisqualifierClass == UFGCDInvalidAimLocation::StaticClass())
				|| (bUnlimitedResources && DisqualifierClass == UFGCDUnaffordable::StaticClass());
			const bool bIsSoft = UFGConstructDisqualifier::GetIsSoftDisqualifier(DisqualifierClass);
			if (!bIgnoredForPlayerIndependence && !bIsSoft)
			{
				bCanConstruct = false;
			}
			DisqualifierTexts.Add(FString::Printf(TEXT("%s (%s%s)"),
				*UFGConstructDisqualifier::GetDisqualifyingText(DisqualifierClass).ToString(),
				bIsSoft ? TEXT("soft") : TEXT("hard"), bIgnoredForPlayerIndependence ? TEXT(", ignored") : TEXT("")));
		}
		const FString DisqualifierSummary = DisqualifierTexts.IsEmpty() ? TEXT("<none>") : FString::Join(DisqualifierTexts, TEXT("; "));

		if (!bCanConstruct)
		{
			UE_LOG(LogAIModAI, Display, TEXT("ConstructExtractorOnNode (deferred, resolved after %d real tick(s)): CanConstruct()=false, NOT constructing - node=%s disqualifiers=[%s]"),
				PollState->AttemptsTaken, *PollState->NodeId, *DisqualifierSummary);
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("CANNOT_CONSTRUCT"), DisqualifierSummary));
			return;
		}

		AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(PollWorld);
		const FNetConstructionID ConstructionID = BuildableSubsystem ? BuildableSubsystem->GetNewNetConstructionID() : FNetConstructionID();

		AFGBuildGun* PollBuildGun = IsValid(PollCharacter) ? PollCharacter->GetBuildGun() : nullptr;
		UFGBuildGunStateBuild* PollBuildState = PollBuildGun ? Cast<UFGBuildGunStateBuild>(PollBuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD)) : nullptr;
		if (!PollBuildState)
		{
			UE_LOG(LogAIModAI, Error, TEXT("ConstructExtractorOnNode (deferred): lost the build state before constructing - aborting, nothing built"));
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("Lost the build state before constructing")));
			return;
		}

		const FVector ConstructLocation = PollHologram->GetActorLocation();
		PollBuildState->InternalConstructHologram(ConstructionID);

		// Confirmation, not just trust: if construction genuinely
		// succeeded, the node should now report occupied. Identify the
		// buildable by proximity, same pattern as ConstructBuildingAtPosition.
		const bool bNowOccupied = IsValid(PollTargetNode) && PollTargetNode->IsOccupied();

		FString ConstructedBuildableId;
		if (BuildableSubsystem)
		{
			float BestDistSq = TNumericLimits<float>::Max();
			AFGBuildable* BestMatch = nullptr;
			for (AFGBuildable* Candidate : BuildableSubsystem->GetAllBuildablesRef())
			{
				if (!IsValid(Candidate)) { continue; }
				const float DistSq = FVector::DistSquared(Candidate->GetActorLocation(), ConstructLocation);
				if (DistSq < BestDistSq)
				{
					BestDistSq = DistSq;
					BestMatch = Candidate;
				}
			}
			if (BestMatch && BestDistSq < FMath::Square(200.0f))
			{
				ConstructedBuildableId = BestMatch->GetPathName();
			}
		}

		UE_LOG(LogAIModAI, Display, TEXT("ConstructExtractorOnNode (deferred, resolved after %d real tick(s)): construction attempted via InternalConstructHologram - node=%s nodeNowOccupied=%s id=%s"),
			PollState->AttemptsTaken, *PollState->NodeId, bNowOccupied ? TEXT("true") : TEXT("false"), *ConstructedBuildableId);

		if (IsValid(PollCharacter))
		{
			PollCharacter->UnequipBuildGun();
		}

		if (!bNowOccupied)
		{
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("CONSTRUCTION_UNCONFIRMED"), TEXT("InternalConstructHologram was called but the node does not report occupied afterward")));
			return;
		}

		PollState->OnComplete(ConstructedBuildableId.IsEmpty()
			? FAIModOperationResult::Success()
			: FAIModOperationResult::SuccessWithBuildableId(ConstructedBuildableId));
	};

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
}

namespace
{
	AFGPortableMiner* FindPortableMinerById(UWorld* World, const FString& Id)
	{
		for (TActorIterator<AFGPortableMiner> It(World); It; ++It)
		{
			if (IsValid(*It) && It->GetPathName() == Id)
			{
				return *It;
			}
		}
		return nullptr;
	}
}

namespace
{
	// Server_SpawnPortableMiner is a protected UFUNCTION(Server, Reliable).
	// Two prior fixes (2026-08-27, 2026-08-28) called it via
	// FindFunction+ProcessEvent reflection - confirmed live both times
	// that the call executes with no error and correct parameters, but no
	// real AFGPortableMiner ever appears. Root cause (2026-08-28,
	// confirmed via FactoryGame.log showing "resolved spawn function
	// 'Server_SpawnPortableMiner'" - the _Implementation UFUNCTION
	// doesn't exist, since _Implementation methods for Server RPCs are
	// NOT separately reflected): AActor::ProcessEvent's own net-function
	// interception for FUNC_Net-flagged UFunctions is a DIFFERENT code
	// path than the UHT-generated call-site thunk a normal
	// `Dispenser->Server_SpawnPortableMiner(...)` call would use - the
	// thunk's "if I have authority, call _Implementation directly, else
	// send over the wire" routing isn't necessarily reproduced by
	// ProcessEvent for every call context, and our HTTP-subsystem-
	// triggered call isn't the actor's owning client, so it's plausible
	// the "send over the wire" branch fires and is silently dropped
	// (no owning NetConnection to actually deliver it to).
	//
	// Fix: call the REAL UHT-generated thunk directly as a normal C++
	// member function instead of through reflection, so its own
	// authority-check-then-execute logic runs exactly as it would from
	// any real in-class caller. Server_SpawnPortableMiner is protected,
	// so this accessor re-exposes it as public via a `using` declaration
	// - safe because C++ access specifiers are compile-time only, add no
	// data members, and don't change object layout, so a static_cast
	// from AFGPortableMinerDispenser* is valid. AFGPortableMinerDispenser
	// is FACTORYGAME_API, so its member function symbols (including
	// protected ones) are exported for external linkage.
	class FPortableMinerDispenserAccessor : public AFGPortableMinerDispenser
	{
	public:
		using AFGPortableMinerDispenser::Server_SpawnPortableMiner;
	};
}

void UAIModFunctionLibrary::ConstructPortableMinerOnNode(UObject* WorldContextObject, const FString& NodeId, const FString& ItemClassPath, TFunction<void(const FAIModOperationResult&)> OnComplete)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context")));
		return;
	}

	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Character)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_PLAYER"), TEXT("No local AFGCharacterPlayer (player index 0)")));
		return;
	}

	AFGResourceNodeBase* TargetNodeBase = nullptr;
	for (TActorIterator<AFGResourceNodeBase> It(World); It; ++It)
	{
		if (IsValid(*It) && It->GetPathName() == NodeId)
		{
			TargetNodeBase = *It;
			break;
		}
	}
	if (!TargetNodeBase)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NODE_NOT_FOUND"), FString::Printf(TEXT("No resource node found with id '%s'"), *NodeId)));
		return;
	}
	if (TargetNodeBase->IsOccupied())
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NODE_OCCUPIED"), TEXT("Targeted node is already occupied")));
		return;
	}

	// Server_SpawnPortableMiner takes a real AFGResourceNode*, not the
	// wider AFGResourceNodeBase - Portable Miners only ever go on normal
	// solid ore nodes, never Fracking cores/satellites. Fail clearly here
	// rather than passing a null resourceNode into the reflection call
	// below.
	AFGResourceNode* TargetNode = Cast<AFGResourceNode>(TargetNodeBase);
	if (!TargetNode)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("INVALID_NODE_TYPE"),
			TEXT("Portable Miners only work on normal AFGResourceNode instances (not Fracking cores/satellites)")));
		return;
	}

	const FString EffectiveItemClassPath = ItemClassPath.IsEmpty()
		? TEXT("/Game/FactoryGame/Resource/Equipment/PortableMiner/BP_ItemDescriptorPortableMiner.BP_ItemDescriptorPortableMiner_C")
		: ItemClassPath;
	UClass* ResolvedItemClass = LoadObject<UClass>(nullptr, *EffectiveItemClassPath);
	const TSubclassOf<UFGItemDescriptor> ItemClass = (ResolvedItemClass && ResolvedItemClass->IsChildOf(UFGItemDescriptor::StaticClass())) ? ResolvedItemClass : nullptr;
	if (!ItemClass)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("INVALID_RECIPE"),
			FString::Printf(TEXT("'%s' did not resolve to a UFGItemDescriptor subclass"), *EffectiveItemClassPath)));
		return;
	}

	UFGInventoryComponent* PlayerInventory = Character->GetInventory();
	UFGInventoryComponentEquipment* ArmsSlot = Character->GetEquipmentSlot(EEquipmentSlot::ES_ARMS);
	if (!PlayerInventory || !ArmsSlot)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No player/ARMS equipment slot inventory found")));
		return;
	}

	// The ARMS equipment slot is a genuinely SEPARATE small inventory
	// component (UFGInventoryComponentEquipment), not a view/filter over
	// the player's general backpack inventory - live-confirmed 2026-08-27
	// two different ways: (1) an item sitting only in the general
	// inventory never showed up scanning this component's own stacks,
	// and (2) once the user manually moved the item into this slot via
	// the in-game UI, it correctly stopped showing up in the general
	// inventory's HasItems() check - the two are mutually exclusive
	// locations, not a view/mirror. So: check the ARMS slot FIRST (covers
	// "already equipped/slotted" including the user's own manual move),
	// and only fall back to moving it from the general inventory if it's
	// not already there.
	int32 FoundIndex = INDEX_NONE;
	for (int32 i = 0; i < ArmsSlot->GetSizeLinear(); ++i)
	{
		FInventoryStack Stack;
		if (ArmsSlot->GetStackFromIndex(i, Stack) && Stack.HasItems() && Stack.Item.GetItemClass() == ItemClass)
		{
			FoundIndex = i;
			break;
		}
	}
	if (FoundIndex == INDEX_NONE)
	{
		if (!PlayerInventory->HasItems(ItemClass, 1))
		{
			OnComplete(FAIModOperationResult::Failure(TEXT("PORTABLE_MINER_NOT_IN_INVENTORY"),
				FString::Printf(TEXT("Player has no '%s' in the general inventory or the ARMS equipment slot - craft one first"), *EffectiveItemClassPath)));
			return;
		}

		PlayerInventory->Remove(ItemClass, 1);
		const int32 NumAdded = ArmsSlot->AddStack(FInventoryStack(1, ItemClass), /*allowPartialAdd=*/false);
		if (NumAdded <= 0)
		{
			// Put it back - never leave the player's inventory short an
			// item because of a failed internal move.
			PlayerInventory->AddStack(FInventoryStack(1, ItemClass), /*allowPartialAdd=*/true);
			OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"),
				FString::Printf(TEXT("'%s' could not be moved into the ARMS equipment slot inventory"), *EffectiveItemClassPath)));
			return;
		}

		for (int32 i = 0; i < ArmsSlot->GetSizeLinear(); ++i)
		{
			FInventoryStack Stack;
			if (ArmsSlot->GetStackFromIndex(i, Stack) && Stack.HasItems() && Stack.Item.GetItemClass() == ItemClass)
			{
				FoundIndex = i;
				break;
			}
		}
	}
	if (FoundIndex == INDEX_NONE)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"),
			FString::Printf(TEXT("'%s' was moved into the ARMS equipment slot but its resulting index could not be found"), *EffectiveItemClassPath)));
		return;
	}

	// Real, sanctioned equip path - the same one a player's own hotbar
	// key-press uses. See this function's header doc comment for why
	// AFGCharacterPlayer::SpawnEquipment/EquipEquipment aren't called
	// directly (SpawnEquipment is private, non-reflected C++).
	ArmsSlot->SetActiveEquipmentIndex(FoundIndex);

	struct FPollState
	{
		TWeakObjectPtr<AFGCharacterPlayer> Character;
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<AFGResourceNode> TargetNode;
		TFunction<void(const FAIModOperationResult&)> OnComplete;
		int32 AttemptsRemaining = 120; // real ticks, not a fixed duration
		int32 AttemptsTaken = 0;
	};
	const TSharedRef<FPollState> PollState = MakeShared<FPollState>();
	PollState->Character = Character;
	PollState->World = World;
	PollState->TargetNode = TargetNode;
	PollState->OnComplete = MoveTemp(OnComplete);

	const TSharedRef<TFunction<void()>> PollFn = MakeShared<TFunction<void()>>();
	*PollFn = [PollState, PollFn]()
	{
		++PollState->AttemptsTaken;

		UWorld* PollWorld = PollState->World.Get();
		AFGCharacterPlayer* PollCharacter = PollState->Character.Get();
		AFGResourceNode* PollTargetNode = PollState->TargetNode.Get();
		if (!PollWorld || !IsValid(PollCharacter) || !IsValid(PollTargetNode))
		{
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("HOLOGRAM_INVALIDATED"), TEXT("World/character/node became invalid while polling")));
			return;
		}

		AFGPortableMinerDispenser* Dispenser = Cast<AFGPortableMinerDispenser>(PollCharacter->GetEquipmentInSlot(EEquipmentSlot::ES_ARMS));
		--PollState->AttemptsRemaining;
		if (!Dispenser && PollState->AttemptsRemaining > 0)
		{
			PollWorld->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
			return;
		}
		if (!Dispenser)
		{
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("EQUIP_FAILED"),
				FString::Printf(TEXT("Portable Miner Dispenser did not equip after %d tick(s)"), PollState->AttemptsTaken)));
			return;
		}

		// Bypasses TraceForPortableMinerPlacementLocation's camera-dependent
		// aim entirely - real node location, not a trace, matching this
		// project's player-independence pattern for every other
		// Construct* function. See FPortableMinerDispenserAccessor's doc
		// comment above for why this calls the real UHT-generated thunk
		// directly (via a protected-access-bypass accessor) instead of
		// through FindFunction+ProcessEvent reflection, which twice
		// executed with no error but never produced a real actor.
		UE_LOG(LogAIModAI, Display, TEXT("ConstructPortableMinerOnNode: Dispenser HasAuthority=%s LocalRole=%d, Character HasAuthority=%s"),
			Dispenser->HasAuthority() ? TEXT("true") : TEXT("false"), static_cast<int32>(Dispenser->GetLocalRole()),
			PollCharacter->HasAuthority() ? TEXT("true") : TEXT("false"));

		static_cast<FPortableMinerDispenserAccessor*>(Dispenser)->Server_SpawnPortableMiner(PollTargetNode->GetActorLocation(), PollTargetNode);

		UE_LOG(LogAIModAI, Display, TEXT("ConstructPortableMinerOnNode: invoked Server_SpawnPortableMiner (direct C++ call) at %s for node %s"),
			*PollTargetNode->GetActorLocation().ToString(), *PollTargetNode->GetPathName());

		// Poll again for the real actor to appear before reporting success -
		// never trust an RPC call alone (this project's established
		// discipline).
		struct FVerifyState
		{
			TWeakObjectPtr<UWorld> World;
			TWeakObjectPtr<AFGCharacterPlayer> Character;
			TWeakObjectPtr<AFGResourceNode> TargetNode;
			TWeakObjectPtr<AFGPortableMinerDispenser> Dispenser;
			TFunction<void(const FAIModOperationResult&)> OnComplete;
			int32 AttemptsRemaining = 60;
			int32 AttemptsTaken = 0;
		};
		const TSharedRef<FVerifyState> VerifyState = MakeShared<FVerifyState>();
		VerifyState->World = PollWorld;
		VerifyState->Character = PollCharacter;
		VerifyState->TargetNode = PollTargetNode;
		VerifyState->Dispenser = Dispenser;
		VerifyState->OnComplete = PollState->OnComplete;

		const TSharedRef<TFunction<void()>> VerifyFn = MakeShared<TFunction<void()>>();
		*VerifyFn = [VerifyState, VerifyFn]()
		{
			++VerifyState->AttemptsTaken;

			UWorld* VerifyWorld = VerifyState->World.Get();
			AFGResourceNode* VerifyTargetNode = VerifyState->TargetNode.Get();
			if (!VerifyWorld || !IsValid(VerifyTargetNode))
			{
				VerifyState->OnComplete(FAIModOperationResult::Failure(TEXT("HOLOGRAM_INVALIDATED"), TEXT("World/node became invalid while verifying")));
				return;
			}

			AFGPortableMiner* NewMiner = nullptr;
			for (TActorIterator<AFGPortableMiner> It(VerifyWorld); It; ++It)
			{
				if (IsValid(*It) && It->mExtractResourceNode == VerifyTargetNode)
				{
					NewMiner = *It;
					break;
				}
			}

			--VerifyState->AttemptsRemaining;
			if (!NewMiner && VerifyState->AttemptsRemaining > 0)
			{
				VerifyWorld->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([VerifyFn]() { (*VerifyFn)(); }));
				return;
			}

			AFGCharacterPlayer* VerifyCharacter = VerifyState->Character.Get();
			AFGPortableMinerDispenser* VerifyDispenser = VerifyState->Dispenser.Get();
			if (IsValid(VerifyCharacter) && IsValid(VerifyDispenser))
			{
				VerifyCharacter->UnequipEquipment(VerifyDispenser);
			}

			if (!NewMiner)
			{
				VerifyState->OnComplete(FAIModOperationResult::Failure(TEXT("CONSTRUCTION_UNCONFIRMED"),
					FString::Printf(TEXT("Server_SpawnPortableMiner was invoked but no AFGPortableMiner targeting the node appeared after %d tick(s)"), VerifyState->AttemptsTaken)));
				return;
			}

			VerifyState->OnComplete(FAIModOperationResult::SuccessWithBuildableId(NewMiner->GetPathName()));
		};

		PollWorld->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([VerifyFn]() { (*VerifyFn)(); }));
	};

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
}

FString UAIModFunctionLibrary::LogPortableMinersAsJson(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		UE_LOG(LogAIModAI, Warning, TEXT("LogPortableMinersAsJson: no valid world context"));
		return TEXT("{}");
	}

	TArray<TSharedPtr<FJsonValue>> MinersArray;
	for (TActorIterator<AFGPortableMiner> It(World); It; ++It)
	{
		AFGPortableMiner* Miner = *It;
		if (!IsValid(Miner))
		{
			continue;
		}

		const FVector Position = Miner->GetActorLocation();
		const TSharedRef<FJsonObject> PositionObject = MakeShared<FJsonObject>();
		PositionObject->SetNumberField(TEXT("x"), Position.X);
		PositionObject->SetNumberField(TEXT("y"), Position.Y);
		PositionObject->SetNumberField(TEXT("z"), Position.Z);

		TArray<TSharedPtr<FJsonValue>> OutputArray;
		if (UFGInventoryComponent* OutputInventory = Miner->GetOutputInventory())
		{
			TArray<FInventoryStack> Stacks;
			OutputInventory->GetInventoryStacks(Stacks, false);
			for (const FInventoryStack& Stack : Stacks)
			{
				if (!Stack.HasItems())
				{
					continue;
				}
				const TSharedRef<FJsonObject> StackObject = MakeShared<FJsonObject>();
				StackObject->SetStringField(TEXT("itemClass"), Stack.Item.GetItemClass() ? Stack.Item.GetItemClass()->GetPathName() : TEXT(""));
				StackObject->SetNumberField(TEXT("numItems"), Stack.NumItems);
				OutputArray.Add(MakeShared<FJsonValueObject>(StackObject));
			}
		}

		const TSharedRef<FJsonObject> MinerObject = MakeShared<FJsonObject>();
		MinerObject->SetStringField(TEXT("id"), Miner->GetPathName());
		MinerObject->SetObjectField(TEXT("position"), PositionObject);
		MinerObject->SetStringField(TEXT("nodeId"), Miner->mExtractResourceNode ? Miner->mExtractResourceNode->GetPathName() : TEXT(""));
		MinerObject->SetBoolField(TEXT("isProducing"), Miner->IsProducing());
		MinerObject->SetNumberField(TEXT("extractionProgress"), Miner->GetExtractionProgress());
		MinerObject->SetArrayField(TEXT("outputInventory"), OutputArray);
		MinersArray.Add(MakeShared<FJsonValueObject>(MinerObject));
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetArrayField(TEXT("portableMiners"), MinersArray);

	const FString JsonString = WriteCondensedJson(RootObject);

	UE_LOG(LogAIModAI, Display, TEXT("LogPortableMinersAsJson: %d portable miner(s)"), MinersArray.Num());

	return JsonString;
}

void UAIModFunctionLibrary::RetrievePortableMinerInventory(UObject* WorldContextObject, const FString& PortableMinerId, TFunction<void(const FAIModOperationResult&)> OnComplete)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context")));
		return;
	}

	AFGPortableMiner* Miner = FindPortableMinerById(World, PortableMinerId);
	if (!Miner)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No Portable Miner found with id '%s'"), *PortableMinerId)));
		return;
	}

	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Character)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_PLAYER"), TEXT("No local AFGCharacterPlayer (player index 0)")));
		return;
	}

	UFGInventoryComponent* OutputInventory = Miner->GetOutputInventory();
	UFGInventoryComponent* PlayerInventory = Character->GetInventory();
	if (!OutputInventory || !PlayerInventory)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("Missing output or player inventory component")));
		return;
	}

	TArray<FInventoryStack> Stacks;
	OutputInventory->GetInventoryStacks(Stacks, false);

	bool bHadAnyItems = false;
	int32 TotalMoved = 0;
	for (const FInventoryStack& Stack : Stacks)
	{
		if (!Stack.HasItems())
		{
			continue;
		}
		bHadAnyItems = true;

		const TSubclassOf<UFGItemDescriptor> ItemClass = Stack.Item.GetItemClass();
		const int32 NumAdded = PlayerInventory->AddStack(FInventoryStack(Stack.NumItems, ItemClass), /*allowPartialAdd=*/true);
		if (NumAdded > 0)
		{
			OutputInventory->Remove(ItemClass, NumAdded);
			TotalMoved += NumAdded;
		}
	}

	if (!bHadAnyItems)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NOTHING_TO_RETRIEVE"), TEXT("Output inventory is empty")));
		return;
	}
	if (TotalMoved == 0)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("INVENTORY_FULL"), TEXT("Output inventory has items, but none of them fit in the player's inventory")));
		return;
	}

	UE_LOG(LogAIModAI, Display, TEXT("RetrievePortableMinerInventory: moved %d item(s) from %s to player inventory"), TotalMoved, *PortableMinerId);

	OnComplete(FAIModOperationResult::Success());
}

FAIModOperationResult UAIModFunctionLibrary::MovePortableMinerToInventory(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}

	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Character)
	{
		return FAIModOperationResult::Failure(TEXT("NO_PLAYER"), TEXT("No local AFGCharacterPlayer (player index 0)"));
	}

	UFGInventoryComponent* PlayerInventory = Character->GetInventory();
	UFGInventoryComponentEquipment* ArmsSlot = Character->GetEquipmentSlot(EEquipmentSlot::ES_ARMS);
	if (!PlayerInventory || !ArmsSlot)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No player/ARMS equipment slot inventory found"));
	}

	const TSubclassOf<UFGItemDescriptor> PortableMinerItemClass = LoadObject<UClass>(nullptr,
		TEXT("/Game/FactoryGame/Resource/Equipment/PortableMiner/BP_ItemDescriptorPortableMiner.BP_ItemDescriptorPortableMiner_C"));

	int32 FoundIndex = INDEX_NONE;
	for (int32 i = 0; i < ArmsSlot->GetSizeLinear(); ++i)
	{
		FInventoryStack Stack;
		if (ArmsSlot->GetStackFromIndex(i, Stack) && Stack.HasItems() && Stack.Item.GetItemClass() == PortableMinerItemClass)
		{
			FoundIndex = i;
			break;
		}
	}

	if (FoundIndex == INDEX_NONE)
	{
		// Not an error - the general inventory may already have one, or
		// the player may genuinely have none at all (a different failure
		// the caller will see when it actually tries to build).
		return FAIModOperationResult::Success();
	}

	ArmsSlot->RemoveFromIndex(FoundIndex, 1, PlayerInventory);

	UE_LOG(LogAIModAI, Display, TEXT("MovePortableMinerToInventory: moved 1 Portable Miner from ARMS slot to general inventory"));
	return FAIModOperationResult::Success();
}

FAIModOperationResult UAIModFunctionLibrary::SimulatedCraft(UObject* WorldContextObject, const FString& RecipeClassPath)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}

	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Character)
	{
		return FAIModOperationResult::Failure(TEXT("NO_PLAYER"), TEXT("No local AFGCharacterPlayer (player index 0)"));
	}

	UFGInventoryComponent* PlayerInventory = Character->GetInventory();
	if (!PlayerInventory)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No player inventory found"));
	}

	// Same narrow "load and validate one specific type" pattern as
	// RecipeClassPath elsewhere in this file.
	UClass* ResolvedClass = LoadObject<UClass>(nullptr, *RecipeClassPath);
	if (!ResolvedClass || !ResolvedClass->IsChildOf(UFGRecipe::StaticClass()))
	{
		return FAIModOperationResult::Failure(TEXT("INVALID_RECIPE"),
			FString::Printf(TEXT("'%s' did not resolve to a UFGRecipe subclass"), *RecipeClassPath));
	}
	const TSubclassOf<UFGRecipe> RecipeClass = ResolvedClass;

	// Deliberately scoped to handheld items only - see this function's
	// header comment. Every product must be equipment, not a building,
	// raw part, or bulk factory component.
	const TArray<FItemAmount> Products = UFGRecipe::GetProducts(RecipeClass);
	if (Products.Num() == 0)
	{
		return FAIModOperationResult::Failure(TEXT("INVALID_RECIPE"), TEXT("Recipe has no products"));
	}
	for (const FItemAmount& Product : Products)
	{
		if (!Product.ItemClass || !Product.ItemClass->IsChildOf(UFGEquipmentDescriptor::StaticClass()))
		{
			return FAIModOperationResult::Failure(TEXT("NOT_HANDHELD_ITEM"),
				FString::Printf(TEXT("'%s' produces a non-equipment item ('%s') - simulated crafting is scoped to handheld items only"),
					*RecipeClassPath, Product.ItemClass ? *Product.ItemClass->GetName() : TEXT("<null>")));
		}
	}

	// Verify affordability of EVERY ingredient before changing anything -
	// never partially consume ingredients for a craft that can't complete.
	const TArray<FItemAmount> Ingredients = UFGRecipe::GetIngredients(World, RecipeClass);
	TArray<FString> ShortfallDescriptions;
	for (const FItemAmount& Ingredient : Ingredients)
	{
		if (!Ingredient.ItemClass || !PlayerInventory->HasItems(Ingredient.ItemClass, Ingredient.Amount))
		{
			const int32 Have = Ingredient.ItemClass ? PlayerInventory->GetNumItems(Ingredient.ItemClass) : 0;
			ShortfallDescriptions.Add(FString::Printf(TEXT("%s (need %d, have %d)"),
				Ingredient.ItemClass ? *Ingredient.ItemClass->GetName() : TEXT("<null>"), Ingredient.Amount, Have));
		}
	}
	if (!ShortfallDescriptions.IsEmpty())
	{
		return FAIModOperationResult::Failure(TEXT("INSUFFICIENT_INGREDIENTS"),
			FString::Printf(TEXT("Missing: %s"), *FString::Join(ShortfallDescriptions, TEXT("; "))));
	}

	for (const FItemAmount& Ingredient : Ingredients)
	{
		PlayerInventory->Remove(Ingredient.ItemClass, Ingredient.Amount);
	}
	for (const FItemAmount& Product : Products)
	{
		PlayerInventory->AddStack(FInventoryStack(Product.Amount, Product.ItemClass), /*allowPartialAdd=*/true);
	}

	UE_LOG(LogAIModAI, Display, TEXT("SimulatedCraft: crafted %s (recipe %s)"), *Products[0].ItemClass->GetName(), *RecipeClassPath);
	return FAIModOperationResult::Success();
}

FString UAIModFunctionLibrary::LogCentralStorageAsJson(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		UE_LOG(LogAIModAI, Warning, TEXT("LogCentralStorageAsJson: no valid world context"));
		return TEXT("{}");
	}

	AFGCentralStorageSubsystem* CentralStorage = AFGCentralStorageSubsystem::Get(World);

	TArray<TSharedPtr<FJsonValue>> ItemsArray;
	if (CentralStorage && CentralStorage->IsCentralStorageBuilt())
	{
		TArray<FItemAmount> AllItems;
		CentralStorage->GetAllItemsFromCentralStorage(AllItems);
		for (const FItemAmount& Item : AllItems)
		{
			const TSharedRef<FJsonObject> ItemObject = MakeShared<FJsonObject>();
			ItemObject->SetStringField(TEXT("itemClass"), Item.ItemClass ? Item.ItemClass->GetPathName() : TEXT(""));
			ItemObject->SetStringField(TEXT("itemName"), Item.ItemClass ? Item.ItemClass->GetName() : TEXT(""));
			ItemObject->SetNumberField(TEXT("amount"), Item.Amount);
			ItemsArray.Add(MakeShared<FJsonValueObject>(ItemObject));
		}
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetBoolField(TEXT("isCentralStorageBuilt"), CentralStorage && CentralStorage->IsCentralStorageBuilt());
	RootObject->SetArrayField(TEXT("items"), ItemsArray);

	const FString JsonString = WriteCondensedJson(RootObject);

	UE_LOG(LogAIModAI, Display, TEXT("LogCentralStorageAsJson: %d item type(s)"), ItemsArray.Num());

	return JsonString;
}

FAIModOperationResult UAIModFunctionLibrary::WithdrawFromCentralStorage(UObject* WorldContextObject, const FString& ItemClassPath, int32 Amount)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}

	if (Amount <= 0)
	{
		return FAIModOperationResult::Failure(TEXT("INVALID_REQUEST"), TEXT("Amount must be greater than 0"));
	}

	AFGCentralStorageSubsystem* CentralStorage = AFGCentralStorageSubsystem::Get(World);
	if (!CentralStorage || !CentralStorage->IsCentralStorageBuilt())
	{
		return FAIModOperationResult::Failure(TEXT("NO_CENTRAL_STORAGE"), TEXT("No Dimensional Depot Uploader has been built yet"));
	}

	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Character)
	{
		return FAIModOperationResult::Failure(TEXT("NO_PLAYER"), TEXT("No local AFGCharacterPlayer (player index 0)"));
	}
	UFGInventoryComponent* PlayerInventory = Character->GetInventory();
	if (!PlayerInventory)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No player inventory found"));
	}

	// Same narrow "load and validate one specific type" pattern as
	// RecipeClassPath elsewhere in this file.
	UClass* ResolvedClass = LoadObject<UClass>(nullptr, *ItemClassPath);
	if (!ResolvedClass || !ResolvedClass->IsChildOf(UFGItemDescriptor::StaticClass()))
	{
		return FAIModOperationResult::Failure(TEXT("INVALID_ITEM_CLASS"),
			FString::Printf(TEXT("'%s' did not resolve to a UFGItemDescriptor subclass"), *ItemClassPath));
	}
	const TSubclassOf<UFGItemDescriptor> ItemClass = ResolvedClass;

	// TryRemoveItemsFromCentralStorage itself clamps to what's actually
	// available - a request for more than the Depot holds is not an
	// error, it just withdraws whatever it can (own doc comment: "If
	// count is more than the items available, a partial remove is done").
	const int32 NumRemoved = CentralStorage->TryRemoveItemsFromCentralStorage(ItemClass, Amount);
	if (NumRemoved <= 0)
	{
		return FAIModOperationResult::Failure(TEXT("NOTHING_WITHDRAWN"),
			FString::Printf(TEXT("Dimensional Depot has none of '%s'"), *ItemClassPath));
	}

	// No API exists to deposit a raw amount back into the Depot (only
	// UploadItemFromInventoryToCentralStorage, which needs the item to
	// already be sitting in a real inventory slot) - if the player's
	// inventory can't hold all of it, whatever doesn't fit is genuinely
	// lost rather than silently stuck in limbo. Reported honestly below,
	// not hidden.
	const int32 NumAdded = PlayerInventory->AddStack(FInventoryStack(NumRemoved, ItemClass), /*allowPartialAdd=*/true);

	UE_LOG(LogAIModAI, Display, TEXT("WithdrawFromCentralStorage: withdrew %d of %s from Dimensional Depot to player inventory (requested %d)"),
		NumAdded, *ItemClassPath, Amount);

	if (NumAdded < NumRemoved)
	{
		return FAIModOperationResult::Failure(TEXT("INVENTORY_FULL"),
			FString::Printf(TEXT("Withdrew %d of %d requested, but only %d fit in inventory - the rest was lost (inventory was full)"), NumRemoved, Amount, NumAdded));
	}

	return FAIModOperationResult::Success();
}

FString UAIModFunctionLibrary::CleanupOrphanedFlowIndicatorsAsJson(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		UE_LOG(LogAIModAI, Warning, TEXT("CleanupOrphanedFlowIndicatorsAsJson: no valid world context"));
		return TEXT("{}");
	}

	// Exact, not proximity-based - see this function's header doc comment
	// for why proximity guessing was rejected as unsafe in a dense pipe
	// cluster. AFGBuildablePipeline::GetFlowIndicator() is the real,
	// public accessor for the mFlowIndicator UPROPERTY - no reflection
	// needed, unlike the Portable Miner's protected Server RPC.
	TSet<AFGBuildablePipelineFlowIndicator*> AttachedIndicators;
	for (TActorIterator<AFGBuildablePipeline> It(World); It; ++It)
	{
		if (!IsValid(*It))
		{
			continue;
		}
		if (AFGBuildablePipelineFlowIndicator* Indicator = It->GetFlowIndicator())
		{
			AttachedIndicators.Add(Indicator);
		}
	}

	TArray<AFGBuildablePipelineFlowIndicator*> Orphans;
	for (TActorIterator<AFGBuildablePipelineFlowIndicator> It(World); It; ++It)
	{
		if (IsValid(*It) && !AttachedIndicators.Contains(*It))
		{
			Orphans.Add(*It);
		}
	}

	TArray<TSharedPtr<FJsonValue>> DeletedIdsArray;
	int32 TotalIndicators = AttachedIndicators.Num() + Orphans.Num();
	for (AFGBuildablePipelineFlowIndicator* Orphan : Orphans)
	{
		const FString OrphanId = Orphan->GetPathName();
		// Real, safe dismantle - same IFGDismantleInterface path as
		// DismantleBuildable, not AActor::Destroy().
		IFGDismantleInterface::Execute_Dismantle(Orphan);
		DeletedIdsArray.Add(MakeShared<FJsonValueString>(OrphanId));
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetNumberField(TEXT("totalIndicators"), TotalIndicators);
	RootObject->SetNumberField(TEXT("attachedCount"), AttachedIndicators.Num());
	RootObject->SetNumberField(TEXT("orphanCount"), Orphans.Num());
	RootObject->SetArrayField(TEXT("deletedIds"), DeletedIdsArray);

	const FString JsonString = WriteCondensedJson(RootObject);

	UE_LOG(LogAIModAI, Display, TEXT("CleanupOrphanedFlowIndicatorsAsJson: %d total, %d attached, %d orphan(s) deleted"),
		TotalIndicators, AttachedIndicators.Num(), Orphans.Num());

	return JsonString;
}

void UAIModFunctionLibrary::ConstructPowerConnection(UObject* WorldContextObject, const FString& BuildableIdA, const FString& BuildableIdB, bool bDryRun, bool bIgnoreAimLocation, bool bIgnoreWireSnap, TFunction<void(const FAIModOperationResult&)> OnComplete)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context")));
		return;
	}

	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Character)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_PLAYER"), TEXT("No local AFGCharacterPlayer (player index 0)")));
		return;
	}

	AFGBuildable* BuildableA = FindBuildableById(World, BuildableIdA);
	if (!BuildableA)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No buildable found with id '%s'"), *BuildableIdA)));
		return;
	}
	AFGBuildable* BuildableB = FindBuildableById(World, BuildableIdB);
	if (!BuildableB)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No buildable found with id '%s'"), *BuildableIdB)));
		return;
	}

	// See this function's header comment on the pole-vs-daisy-chain
	// gameplay constraint - a NO_POWER_CONNECTION result here may
	// correctly reflect that the save's progression hasn't unlocked
	// direct machine-to-machine wiring yet, not a bug.
	UFGPowerConnectionComponent* ConnectionA = FindFreePowerConnection(BuildableA);
	if (!ConnectionA)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_POWER_CONNECTION"), FString::Printf(TEXT("'%s' has no free power connection component"), *BuildableIdA)));
		return;
	}
	UFGPowerConnectionComponent* ConnectionB = FindFreePowerConnection(BuildableB);
	if (!ConnectionB)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_POWER_CONNECTION"), FString::Printf(TEXT("'%s' has no free power connection component"), *BuildableIdB)));
		return;
	}

	UClass* PowerLineRecipeClass = LoadObject<UClass>(nullptr, TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PowerLine.Recipe_PowerLine_C"));
	if (!PowerLineRecipeClass || !PowerLineRecipeClass->IsChildOf(UFGRecipe::StaticClass()))
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("RECIPE_LOAD_FAILED"), TEXT("Failed to load Recipe_PowerLine as a UFGRecipe")));
		return;
	}
	const TSubclassOf<UFGRecipe> RecipeClass = PowerLineRecipeClass;

	Character->HotKeyRecipe(RecipeClass);

	AFGBuildGun* BuildGun = Character->GetBuildGun();
	if (!BuildGun)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_BUILD_GUN"), TEXT("AFGCharacterPlayer::GetBuildGun() returned null")));
		return;
	}

	UFGBuildGunStateBuild* BuildState = Cast<UFGBuildGunStateBuild>(BuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD));
	if (!BuildState)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_BUILD_STATE"), TEXT("Could not resolve UFGBuildGunStateBuild from the build gun")));
		return;
	}

	AFGHologram* Hologram = BuildState->GetHologram();
	AFGWireHologram* WireHologram = Cast<AFGWireHologram>(Hologram);
	if (!WireHologram)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("HOLOGRAM_SPAWN_FAILED"),
			FString::Printf(TEXT("HotKeyRecipe(Recipe_PowerLine) did not result in an AFGWireHologram (got %s)"),
				Hologram ? *Hologram->GetClass()->GetName() : TEXT("null"))));
		return;
	}

	// ConstructPowerConnection (2026-08-25): confirmed live, TWICE, that
	// this exact SetConnection()-only mechanism (no click/snap step, a
	// single GetHitResult() assignment for ConnectionA only) is the only
	// mechanism that works at all for machine<->machine connections. Two
	// mechanism deviations were tried and BOTH regressed the
	// previously-working machine<->machine case live: (1) a click-based
	// UpdateHologramPlacement()+TrySnapToActor()+DoMultiStepPlacement()
	// rewrite - TrySnapToActor() never populated GetConnection(0)/(1)
	// for wires at all; (2) touching BuildGun->GetHitResult() for
	// ConnectionB too (not just A) before SetConnection(1,...) - broke
	// the ConnectionA validation somehow. Do not repeat either.
	//
	// SEPARATELY diagnosed (also 2026-08-25, same mechanism, no code
	// change): repeated identical dry-run calls against the exact same
	// pair of buildables returned THREE DIFFERENT disqualifiers across
	// attempts - UFGCDWireSnap, UFGCDWireTooLong (despite real 3D
	// distance well under the real queried maxLength), and
	// UFGCDInvalidAimLocation - with success on other attempts, still no
	// change. This is the same class of live-camera-dependent flakiness
	// already solved for building placement
	// (ConstructBuildingAtPosition's bIgnoreAimLocation etc.), not a
	// genuine geometry problem with this function's own logic. See the
	// poll loop below for the bIgnoreAimLocation/bIgnoreWireSnap
	// disqualifier-bypass this motivated - NOT YET LIVE-VERIFIED to
	// resolve it, only diagnosed.
	FHitResult SyntheticHit;
	SyntheticHit.Location = ConnectionA->GetComponentLocation();
	SyntheticHit.ImpactPoint = SyntheticHit.Location;
	SyntheticHit.Normal = FVector::UpVector;
	SyntheticHit.ImpactNormal = FVector::UpVector;
	SyntheticHit.HitObjectHandle = FActorInstanceHandle(BuildableA);
	// UFGPowerConnectionComponent derives from USceneComponent, not
	// UPrimitiveComponent - FHitResult::Component left unset, same as
	// the dry-run.
	SyntheticHit.bBlockingHit = true;
	BuildGun->GetHitResult() = SyntheticHit;

	WireHologram->SetConnection(0, ConnectionA);
	WireHologram->SetConnection(1, ConnectionB);

	struct FPollState
	{
		TWeakObjectPtr<AFGWireHologram> Hologram;
		TWeakObjectPtr<AFGCharacterPlayer> Character;
		TWeakObjectPtr<UWorld> World;
		FHitResult SyntheticHit;
		FString BuildableIdA;
		FString BuildableIdB;
		bool bDryRun = true;
		bool bIgnoreAimLocation = false;
		bool bIgnoreWireSnap = false;
		int32 AttemptsRemaining = 120; // safety cap - real ticks, not a fixed duration
		int32 AttemptsTaken = 0;
		TFunction<void(const FAIModOperationResult&)> OnComplete;
	};
	const TSharedRef<FPollState> PollState = MakeShared<FPollState>();
	PollState->Hologram = WireHologram;
	PollState->Character = Character;
	PollState->World = World;
	PollState->SyntheticHit = SyntheticHit;
	PollState->BuildableIdA = BuildableIdA;
	PollState->BuildableIdB = BuildableIdB;
	PollState->bDryRun = bDryRun;
	PollState->bIgnoreAimLocation = bIgnoreAimLocation;
	PollState->bIgnoreWireSnap = bIgnoreWireSnap;
	PollState->OnComplete = MoveTemp(OnComplete);

	const TSharedRef<TFunction<void()>> PollFn = MakeShared<TFunction<void()>>();
	*PollFn = [PollState, PollFn]()
	{
		++PollState->AttemptsTaken;

		AFGWireHologram* PollHologram = PollState->Hologram.Get();
		UWorld* PollWorld = PollState->World.Get();
		AFGCharacterPlayer* PollCharacter = PollState->Character.Get();
		if (!IsValid(PollHologram) || !PollWorld)
		{
			UE_LOG(LogAIModAI, Warning, TEXT("ConstructPowerConnection (deferred): hologram or world became invalid while polling (after %d tick(s))"), PollState->AttemptsTaken);
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("HOLOGRAM_INVALIDATED"), TEXT("Hologram or world became invalid while polling")));
			return;
		}

		PollHologram->UpdateHologramPlacement(PollState->SyntheticHit);

		TArray<TSubclassOf<UFGConstructDisqualifier>> Disqualifiers;
		PollHologram->GetConstructDisqualifiers(Disqualifiers);
		const bool bStillInitializing = Disqualifiers.Contains(TSubclassOf<UFGConstructDisqualifier>(UFGCDInitializing::StaticClass()));

		--PollState->AttemptsRemaining;
		if (bStillInitializing && PollState->AttemptsRemaining > 0)
		{
			PollWorld->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
			return;
		}

		// Live-diagnosed 2026-08-25: repeated identical dry-run calls
		// against the exact same pair of buildables (no code or geometry
		// change between calls) returned THREE DIFFERENT disqualifiers
		// across attempts - UFGCDWireSnap ("Must be hooked up to a
		// connection!"), UFGCDWireTooLong ("Wire is too long!" - despite
		// the real 3D distance being well under the real queried
		// maxLength), and UFGCDInvalidAimLocation ("Invalid aim
		// location!") - with success on other attempts with STILL no
		// change. This matches the exact same class of live-camera-
		// dependent flakiness already solved for building placement
		// (ConstructBuildingAtPosition's bIgnoreAimLocation etc.) - the
		// wire hologram's validation appears to depend on real,
		// currently-changing player camera/aim state our synthetic hit
		// doesn't fully override, not on anything this function's own
		// geometry gets wrong. Bypassing PollHologram->CanConstruct()'s
		// opaque bool (stub source, unreadable) in favor of manually
		// walking GetConstructDisqualifiers() ourselves - same pattern
		// as ConstructBuildingAtPosition - so bIgnoreAimLocation/
		// bIgnoreWireSnap can skip specific disqualifier classes the
		// caller explicitly opts into ignoring, without bypassing
		// FactoryGame's own real validation inside
		// InternalConstructHologram() itself. UFGCDWireTooLong is
		// deliberately NOT ignorable here - unlike the other two, it is
		// presumed to reflect the real, deterministic mMaxLength check.
		// UnlimitedResources (2026-08-27) - see ConstructBuildingAtPosition's
		// identical comment on this being a player-controlled mod setting.
		const bool bUnlimitedResources = UAIModFunctionLibrary::GetAIModConfigBool(PollWorld, TEXT("UnlimitedResources"), false);

		bool bCanConstruct = true;
		TArray<FString> DisqualifierTexts;
		for (const TSubclassOf<UFGConstructDisqualifier>& DisqualifierClass : Disqualifiers)
		{
			const bool bIgnoredByFlag =
				(PollState->bIgnoreAimLocation && DisqualifierClass == UFGCDInvalidAimLocation::StaticClass()) ||
				(PollState->bIgnoreWireSnap && DisqualifierClass == UFGCDWireSnap::StaticClass()) ||
				(bUnlimitedResources && DisqualifierClass == UFGCDUnaffordable::StaticClass());
			const bool bIsSoft = UFGConstructDisqualifier::GetIsSoftDisqualifier(DisqualifierClass);
			if (!bIgnoredByFlag && !bIsSoft)
			{
				bCanConstruct = false;
			}
			DisqualifierTexts.Add(FString::Printf(TEXT("%s (%s%s)"),
				*UFGConstructDisqualifier::GetDisqualifyingText(DisqualifierClass).ToString(),
				bIsSoft ? TEXT("soft") : TEXT("hard"), bIgnoredByFlag ? TEXT(", ignored") : TEXT("")));
		}
		const FString DisqualifierSummary = DisqualifierTexts.IsEmpty() ? TEXT("<none>") : FString::Join(DisqualifierTexts, TEXT("; "));

		UE_LOG(LogAIModAI, Display, TEXT("ConstructPowerConnection (deferred, resolved after %d real tick(s)): a=%s b=%s dryRun=%s canConstruct=%s disqualifiers=[%s]"),
			PollState->AttemptsTaken, *PollState->BuildableIdA, *PollState->BuildableIdB, PollState->bDryRun ? TEXT("true") : TEXT("false"),
			bCanConstruct ? TEXT("true") : TEXT("false"), *DisqualifierSummary);

		if (!bCanConstruct)
		{
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("CANNOT_CONSTRUCT"), DisqualifierSummary));
			return;
		}

		if (PollState->bDryRun)
		{
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Success());
			return;
		}

		AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(PollWorld);
		const FNetConstructionID ConstructionID = BuildableSubsystem ? BuildableSubsystem->GetNewNetConstructionID() : FNetConstructionID();

		AFGBuildGun* PollBuildGun = IsValid(PollCharacter) ? PollCharacter->GetBuildGun() : nullptr;
		UFGBuildGunStateBuild* PollBuildState = PollBuildGun ? Cast<UFGBuildGunStateBuild>(PollBuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD)) : nullptr;
		if (!PollBuildState)
		{
			UE_LOG(LogAIModAI, Error, TEXT("ConstructPowerConnection (deferred): lost the build state before constructing - aborting, nothing built"));
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("Lost the build state before constructing")));
			return;
		}

		PollBuildState->InternalConstructHologram(ConstructionID);

		UE_LOG(LogAIModAI, Display, TEXT("ConstructPowerConnection (deferred, resolved after %d real tick(s)): construction attempted via InternalConstructHologram - a=%s b=%s"),
			PollState->AttemptsTaken, *PollState->BuildableIdA, *PollState->BuildableIdB);

		if (IsValid(PollCharacter))
		{
			PollCharacter->UnequipBuildGun();
		}

		PollState->OnComplete(FAIModOperationResult::Success());
	};

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
}

FAIModOperationResult UAIModFunctionLibrary::DebugCheckConveyorSnap(UObject* WorldContextObject, const FString& SourceBuildableId)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}

	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Character)
	{
		return FAIModOperationResult::Failure(TEXT("NO_PLAYER"), TEXT("No local AFGCharacterPlayer (player index 0)"));
	}

	AFGBuildable* SourceBuildable = FindBuildableById(World, SourceBuildableId);
	if (!SourceBuildable)
	{
		return FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No buildable found with id '%s'"), *SourceBuildableId));
	}

	UFGFactoryConnectionComponent* SourceConnection = FindFreeFactoryConnection(SourceBuildable, EFactoryConnectionDirection::FCD_OUTPUT);
	if (!SourceConnection)
	{
		return FAIModOperationResult::Failure(TEXT("NO_FACTORY_CONNECTION"), FString::Printf(TEXT("'%s' has no free Output factory connection component"), *SourceBuildableId));
	}

	UClass* BeltRecipeClass = LoadObject<UClass>(nullptr, TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorBeltMk1.Recipe_ConveyorBeltMk1_C"));
	if (!BeltRecipeClass || !BeltRecipeClass->IsChildOf(UFGRecipe::StaticClass()))
	{
		return FAIModOperationResult::Failure(TEXT("RECIPE_LOAD_FAILED"), TEXT("Failed to load Recipe_ConveyorBeltMk1 as a UFGRecipe"));
	}
	const TSubclassOf<UFGRecipe> RecipeClass = BeltRecipeClass;

	Character->HotKeyRecipe(RecipeClass);

	AFGBuildGun* BuildGun = Character->GetBuildGun();
	if (!BuildGun)
	{
		Character->UnequipBuildGun();
		return FAIModOperationResult::Failure(TEXT("NO_BUILD_GUN"), TEXT("AFGCharacterPlayer::GetBuildGun() returned null"));
	}

	UFGBuildGunStateBuild* BuildState = Cast<UFGBuildGunStateBuild>(BuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD));
	if (!BuildState)
	{
		Character->UnequipBuildGun();
		return FAIModOperationResult::Failure(TEXT("NO_BUILD_STATE"), TEXT("Could not resolve UFGBuildGunStateBuild from the build gun"));
	}

	AFGHologram* Hologram = BuildState->GetHologram();
	AFGConveyorBeltHologram* BeltHologram = Cast<AFGConveyorBeltHologram>(Hologram);
	if (!BeltHologram)
	{
		Character->UnequipBuildGun();
		return FAIModOperationResult::Failure(TEXT("HOLOGRAM_SPAWN_FAILED"),
			FString::Printf(TEXT("HotKeyRecipe(Recipe_ConveyorBeltMk1) did not result in an AFGConveyorBeltHologram (got %s)"),
				Hologram ? *Hologram->GetClass()->GetName() : TEXT("null")));
	}

	const ESplineHologramBuildStep StepInitial = BeltHologram->GetCurrentBuildStep();

	FHitResult SyntheticHit;
	SyntheticHit.Location = SourceConnection->GetConnectorLocation();
	SyntheticHit.ImpactPoint = SyntheticHit.Location;
	SyntheticHit.Normal = FVector::UpVector;
	SyntheticHit.ImpactNormal = FVector::UpVector;
	SyntheticHit.HitObjectHandle = FActorInstanceHandle(SourceBuildable);
	SyntheticHit.bBlockingHit = true;

	// Widened experiment (2026-08-25): calling TrySnapToActor() directly
	// returned true but left every state indicator unchanged
	// (step/IsConnectionSnapped/connected count all showed no real
	// snap) - contradictory evidence. Every other hologram in this
	// project (buildings, wires) is driven through
	// UpdateHologramPlacement(), not by calling the override method
	// directly - try that first, matching the proven pattern, before
	// (and after) TrySnapToActor() and a single DoMultiStepPlacement()
	// "click" to gather maximum evidence in one pass.
	BeltHologram->UpdateHologramPlacement(SyntheticHit);
	const ESplineHologramBuildStep StepAfterUpdate = BeltHologram->GetCurrentBuildStep();
	const bool bConnectionSnappedAfterUpdate = BeltHologram->IsConnectionSnapped(false);

	const bool bSnapped = BeltHologram->TrySnapToActor(SyntheticHit);
	const ESplineHologramBuildStep StepAfterSnap = BeltHologram->GetCurrentBuildStep();
	const bool bConnectionSnappedAfterSnap = BeltHologram->IsConnectionSnapped(false);
	const TArray<AFGBuildable*> ConnectedBuildablesAfterSnap = BeltHologram->GetAnyConnectedBuildables();

	TArray<TSubclassOf<UFGConstructDisqualifier>> Disqualifiers;
	BeltHologram->GetConstructDisqualifiers(Disqualifiers);
	TArray<FString> DisqualifierTexts;
	for (const TSubclassOf<UFGConstructDisqualifier>& DisqualifierClass : Disqualifiers)
	{
		DisqualifierTexts.Add(UFGConstructDisqualifier::GetDisqualifyingText(DisqualifierClass).ToString());
	}
	const FString DisqualifierSummary = DisqualifierTexts.IsEmpty() ? TEXT("<none>") : FString::Join(DisqualifierTexts, TEXT("; "));

	// A single simulated "click" - per FGHologram.h's doc comment on
	// DoMultiStepPlacement, true signals a release; a real player's
	// first click on a belt should fix the start point without
	// finishing the belt (only returns true once the whole sequence is
	// done), so false is expected here, not a failure indicator by
	// itself.
	const bool bStepComplete = BeltHologram->DoMultiStepPlacement(true);
	const ESplineHologramBuildStep StepAfterClick = BeltHologram->GetCurrentBuildStep();
	const bool bConnectionSnappedAfterClick = BeltHologram->IsConnectionSnapped(false);
	const TArray<AFGBuildable*> ConnectedBuildablesAfterClick = BeltHologram->GetAnyConnectedBuildables();

	UE_LOG(LogAIModAI, Display, TEXT("DebugCheckConveyorSnap: source=%s connectorLocation=%s stepInitial=%d | afterUpdateHologramPlacement: step=%d snapped=%s | afterTrySnapToActor: result=%s step=%d snapped=%s connectedCount=%d disqualifiers=[%s] | afterDoMultiStepPlacement(true): stepComplete=%s step=%d snapped=%s connectedCount=%d"),
		*SourceBuildableId, *SyntheticHit.Location.ToString(), static_cast<int32>(StepInitial),
		static_cast<int32>(StepAfterUpdate), bConnectionSnappedAfterUpdate ? TEXT("true") : TEXT("false"),
		bSnapped ? TEXT("true") : TEXT("false"), static_cast<int32>(StepAfterSnap), bConnectionSnappedAfterSnap ? TEXT("true") : TEXT("false"), ConnectedBuildablesAfterSnap.Num(), *DisqualifierSummary,
		bStepComplete ? TEXT("true") : TEXT("false"), static_cast<int32>(StepAfterClick), bConnectionSnappedAfterClick ? TEXT("true") : TEXT("false"), ConnectedBuildablesAfterClick.Num());

	// Never calls CanConstruct()/Construct() - this hologram is just a
	// scratch actor to be cleaned up now, regardless of outcome.
	Character->UnequipBuildGun();

	const bool bAnySnapIndicator = bConnectionSnappedAfterUpdate || bSnapped || bConnectionSnappedAfterSnap || bConnectionSnappedAfterClick || ConnectedBuildablesAfterClick.Num() > 0;
	if (!bAnySnapIndicator)
	{
		return FAIModOperationResult::Failure(TEXT("SNAP_FAILED"), TEXT("No snap indicator was ever true - see LogAIModAI for the full step-by-step trace"));
	}

	return FAIModOperationResult::Success();
}

FString UAIModFunctionLibrary::LogConveyorBeltTiersAsJson(UObject* WorldContextObject)
{
	// Read-only telemetry, no World/player needed - just resolves each
	// recipe -> buildable class -> CDO and reads GetSpeed() off it.
	// Reuses ResolveBuildableClassForRecipe (anonymous namespace above),
	// the same recipe->buildable-class resolution
	// ConstructBuildingAtPosition's lightweight-instance fallback uses.
	static const TCHAR* TierRecipePaths[] = {
		TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorBeltMk1.Recipe_ConveyorBeltMk1_C"),
		TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorBeltMk2.Recipe_ConveyorBeltMk2_C"),
		TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorBeltMk3.Recipe_ConveyorBeltMk3_C"),
		TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorBeltMk4.Recipe_ConveyorBeltMk4_C"),
		TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorBeltMk5.Recipe_ConveyorBeltMk5_C"),
		TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorBeltMk6.Recipe_ConveyorBeltMk6_C"),
	};

	TArray<TSharedPtr<FJsonValue>> TierJsonArray;
	for (const TCHAR* RecipePath : TierRecipePaths)
	{
		const TSubclassOf<AFGBuildable> BuildableClass = ResolveBuildableClassForRecipe(RecipePath);
		const AFGBuildableConveyorBase* BeltCDO = BuildableClass ? Cast<AFGBuildableConveyorBase>(BuildableClass->GetDefaultObject()) : nullptr;
		if (!BeltCDO)
		{
			UE_LOG(LogAIModAI, Warning, TEXT("LogConveyorBeltTiersAsJson: could not resolve a AFGBuildableConveyorBase CDO for '%s' - omitting"), RecipePath);
			continue;
		}

		const TSharedRef<FJsonObject> TierObject = MakeShared<FJsonObject>();
		TierObject->SetStringField(TEXT("recipeClass"), RecipePath);
		TierObject->SetStringField(TEXT("buildableClass"), BuildableClass->GetPathName());
		TierObject->SetNumberField(TEXT("speed"), BeltCDO->GetSpeed());

		// bendRadius/maxSplineLength (2026-08-25): read off the HOLOGRAM
		// class's CDO (a different descriptor accessor,
		// UFGBuildDescriptor::GetHologramClass, than the buildable class
		// above) - both are EditDefaultsOnly Blueprint-configured class
		// defaults (AFGConveyorBeltHologram.h), so this works without
		// spawning anything. Added so an agent can tell BEFORE attempting
		// a connection whether two points are too far apart for a single
		// belt segment, rather than discovering it via a failed
		// world.testConveyorBelt.
		if (const TSubclassOf<AFGConveyorBeltHologram> HologramClass = ResolveConveyorBeltHologramClassForRecipe(RecipePath))
		{
			if (const AFGConveyorBeltHologram* HologramCDO = Cast<AFGConveyorBeltHologram>(HologramClass->GetDefaultObject()))
			{
				TierObject->SetNumberField(TEXT("maxSplineLength"), HologramCDO->GetMaxSplineLength());
				TierObject->SetNumberField(TEXT("bendRadius"), HologramCDO->GetBendRadius());

				// mMaxIncline (degrees) has no public C++ getter, but is a
				// real UPROPERTY (EditDefaultsOnly) with a documented
				// meaning ("What is the maximum incline of the conveyor
				// belt (degrees)") - read via reflection, a single
				// hardcoded read-only field lookup, not a generic
				// property-access capability (CLAUDE.md's Safety and
				// Stability Boundary is about not exposing arbitrary
				// property get/set to external commands; this is neither
				// arbitrary - the field name is fixed in this file - nor
				// a write).
				if (const FFloatProperty* MaxInclineProperty = FindFProperty<FFloatProperty>(HologramClass, TEXT("mMaxIncline")))
				{
					TierObject->SetNumberField(TEXT("maxInclineDegrees"), MaxInclineProperty->GetPropertyValue_InContainer(HologramCDO));
				}
			}
		}

		TierJsonArray.Add(MakeShared<FJsonValueObject>(TierObject));
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetArrayField(TEXT("tiers"), TierJsonArray);

	FString JsonString;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonString);
	FJsonSerializer::Serialize(RootObject, Writer);

	UE_LOG(LogAIModAI, Display, TEXT("LogConveyorBeltTiersAsJson: %s"), *JsonString);

	return JsonString;
}

FString UAIModFunctionLibrary::LogPowerLineLimitsAsJson(UObject* WorldContextObject)
{
	// Read-only telemetry, no World/player needed - see this function's
	// header doc comment for why mMaxLength/mMaxPowerTowerLength/
	// mLengthPerCost are plain public member reads (real, documented-unit
	// UPROPERTYs), unlike the belt tier data's reflection-based
	// mMaxIncline read.
	static const TCHAR* PowerLineRecipePath = TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PowerLine.Recipe_PowerLine_C");

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);

	const TSubclassOf<AFGBuildable> BuildableClass = ResolveBuildableClassForRecipe(PowerLineRecipePath);
	const AFGBuildableWire* WireCDO = BuildableClass ? Cast<AFGBuildableWire>(BuildableClass->GetDefaultObject()) : nullptr;
	if (WireCDO)
	{
		RootObject->SetStringField(TEXT("recipeClass"), PowerLineRecipePath);
		RootObject->SetStringField(TEXT("buildableClass"), BuildableClass->GetPathName());
		RootObject->SetNumberField(TEXT("maxLength"), WireCDO->mMaxLength);
		RootObject->SetNumberField(TEXT("maxPowerTowerLength"), WireCDO->mMaxPowerTowerLength);
		RootObject->SetNumberField(TEXT("lengthPerCost"), WireCDO->mLengthPerCost);
	}
	else
	{
		UE_LOG(LogAIModAI, Warning, TEXT("LogPowerLineLimitsAsJson: could not resolve a AFGBuildableWire CDO for '%s'"), PowerLineRecipePath);
	}

	FString JsonString;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonString);
	FJsonSerializer::Serialize(RootObject, Writer);

	UE_LOG(LogAIModAI, Display, TEXT("LogPowerLineLimitsAsJson: %s"), *JsonString);

	return JsonString;
}

FString UAIModFunctionLibrary::LogPipelineTiersAsJson(UObject* WorldContextObject)
{
	// Read-only telemetry, no World/player needed - mirrors
	// LogConveyorBeltTiersAsJson's structure for the pipe equivalent.
	// Recipe_Pipeline (Mk1) and Recipe_PipelineMK2 (note the capital
	// "MK2", unlike belts' "Mk2" - confirmed from the actual filename on
	// disk) are the two real tiers.
	static const TCHAR* TierRecipePaths[] = {
		TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_Pipeline.Recipe_Pipeline_C"),
		TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PipelineMK2.Recipe_PipelineMK2_C"),
	};

	TArray<TSharedPtr<FJsonValue>> TierJsonArray;
	for (const TCHAR* RecipePath : TierRecipePaths)
	{
		const TSubclassOf<AFGBuildable> BuildableClass = ResolveBuildableClassForRecipe(RecipePath);
		const AFGBuildablePipeline* PipelineCDO = BuildableClass ? Cast<AFGBuildablePipeline>(BuildableClass->GetDefaultObject()) : nullptr;
		if (!PipelineCDO)
		{
			UE_LOG(LogAIModAI, Warning, TEXT("LogPipelineTiersAsJson: could not resolve a AFGBuildablePipeline CDO for '%s' - omitting"), RecipePath);
			continue;
		}

		const TSharedRef<FJsonObject> TierObject = MakeShared<FJsonObject>();
		TierObject->SetStringField(TEXT("recipeClass"), RecipePath);
		TierObject->SetStringField(TEXT("buildableClass"), BuildableClass->GetPathName());
		// flowLimit: "Maximum flow through this pipe in cubic meters.
		// [m^3/s]" per FGBuildablePipeline.h's own doc comment - a real
		// documented unit, unlike belts' ambiguous GetSpeed().
		TierObject->SetNumberField(TEXT("flowLimit"), PipelineCDO->GetFlowLimit());

		// maxSplineLength/bendRadius/minBendRadius (2026-08-25): ALL
		// THREE are private on AFGPipelineHologram with no public
		// getters (unlike belts, where two of three had public getters)
		// - confirmed from source, all real UPROPERTY(EditDefaultsOnly)
		// fields, read via reflection same as belts' mMaxIncline.
		if (const TSubclassOf<AFGPipelineHologram> HologramClass = ResolvePipelineHologramClassForRecipe(RecipePath))
		{
			if (const AFGPipelineHologram* HologramCDO = Cast<AFGPipelineHologram>(HologramClass->GetDefaultObject()))
			{
				if (const FFloatProperty* MaxSplineLengthProperty = FindFProperty<FFloatProperty>(HologramClass, TEXT("mMaxSplineLength")))
				{
					TierObject->SetNumberField(TEXT("maxSplineLength"), MaxSplineLengthProperty->GetPropertyValue_InContainer(HologramCDO));
				}
				if (const FFloatProperty* BendRadiusProperty = FindFProperty<FFloatProperty>(HologramClass, TEXT("mBendRadius")))
				{
					TierObject->SetNumberField(TEXT("bendRadius"), BendRadiusProperty->GetPropertyValue_InContainer(HologramCDO));
				}
				if (const FFloatProperty* MinBendRadiusProperty = FindFProperty<FFloatProperty>(HologramClass, TEXT("mMinBendRadius")))
				{
					TierObject->SetNumberField(TEXT("minBendRadius"), MinBendRadiusProperty->GetPropertyValue_InContainer(HologramCDO));
				}
			}
		}

		TierJsonArray.Add(MakeShared<FJsonValueObject>(TierObject));
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetArrayField(TEXT("tiers"), TierJsonArray);

	FString JsonString;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonString);
	FJsonSerializer::Serialize(RootObject, Writer);

	UE_LOG(LogAIModAI, Display, TEXT("LogPipelineTiersAsJson: %s"), *JsonString);

	return JsonString;
}

// LogConveyorAttachmentCatalogAsJson (2026-08-25, splitter/merger
// groundwork) - research-confirmed (see docs/conveyor-attachment-research.md)
// that splitters/mergers use AFGConveyorAttachmentHologram : AFGFactoryHologram
// : AFGBuildableHologram - the SAME simple, single-step hologram lineage
// already proven for Miners/Smelters/Constructors, NOT the AFGSplineHologram
// branch belts/pipes needed special multi-click driving for. That means
// ConstructBuildingAtPosition/world.placeBuilding (already generic) and
// ConstructConveyorBelt/world.connectConveyor (source/dest already not
// restricted to machines) place and connect splitters/mergers with ZERO
// new construction code - deliberately NOT duplicating a
// ConstructSplitter-style function that would just be a thin, unnecessary
// wrapper. This function exists purely to report the real recipe catalog
// and real per-class input/output UFGFactoryConnectionComponent counts -
// read generically via GetDirection() on each buildable class CDO's
// components (the same technique world.connections itself uses), rather
// than hardcoding the commonly-known 1-in/3-out (splitter) / 3-in/1-out
// (merger) figures, since AFGBuildableConveyorAttachment's header doesn't
// declare them as a literal constant anywhere.
FString UAIModFunctionLibrary::LogConveyorAttachmentCatalogAsJson(UObject* WorldContextObject)
{
	static const TCHAR* RecipePaths[] = {
		TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorAttachmentSplitter.Recipe_ConveyorAttachmentSplitter_C"),
		TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorAttachmentSplitterSmart.Recipe_ConveyorAttachmentSplitterSmart_C"),
		TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorAttachmentSplitterProgrammable.Recipe_ConveyorAttachmentSplitterProgrammable_C"),
		TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorAttachmentMerger.Recipe_ConveyorAttachmentMerger_C"),
		TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorAttachmentMergerPriority.Recipe_ConveyorAttachmentMergerPriority_C"),
	};

	TArray<TSharedPtr<FJsonValue>> EntryJsonArray;
	for (const TCHAR* RecipePath : RecipePaths)
	{
		const TSubclassOf<AFGBuildable> BuildableClass = ResolveBuildableClassForRecipe(RecipePath);
		const AFGBuildableConveyorAttachment* AttachmentCDO = BuildableClass ? Cast<AFGBuildableConveyorAttachment>(BuildableClass->GetDefaultObject()) : nullptr;
		if (!AttachmentCDO)
		{
			UE_LOG(LogAIModAI, Warning, TEXT("LogConveyorAttachmentCatalogAsJson: could not resolve a AFGBuildableConveyorAttachment CDO for '%s' - omitting"), RecipePath);
			continue;
		}

		// Fix (2026-08-27, live-diagnosed): GetComponents<T>() on a CDO
		// only finds NATIVE (CreateDefaultSubobject) components - these
		// buildables add their connectors via the Blueprint's Simple
		// Construction Script, which does NOT populate onto the CDO
		// (SCS-added components only exist on a real spawned instance).
		// This function had reported inputCount=0/outputCount=0 for
		// EVERY entry since it was written - never actually caught
		// because it was flagged "NOT YET LIVE-TESTED" until now.
		// AFGBuildable::GetDefaultComponents<T>() (FGBuildable.h) is
		// FactoryGame's own purpose-built helper for exactly this - walks
		// the Blueprint inheritance chain's SimpleConstructionScript
		// nodes (resolving InheritableComponentHandler overrides) in
		// addition to the native GetComponents() scan, giving the
		// correct full list without spawning anything.
		TArray<UFGFactoryConnectionComponent*> Connections;
		AttachmentCDO->GetDefaultComponents<UFGFactoryConnectionComponent>(Connections);
		int32 InputCount = 0;
		int32 OutputCount = 0;
		for (const UFGFactoryConnectionComponent* Connection : Connections)
		{
			if (!IsValid(Connection)) { continue; }
			if (Connection->GetDirection() == EFactoryConnectionDirection::FCD_INPUT) { ++InputCount; }
			else if (Connection->GetDirection() == EFactoryConnectionDirection::FCD_OUTPUT) { ++OutputCount; }
		}

		const TSharedRef<FJsonObject> EntryObject = MakeShared<FJsonObject>();
		EntryObject->SetStringField(TEXT("recipeClass"), RecipePath);
		EntryObject->SetStringField(TEXT("buildableClass"), BuildableClass->GetPathName());
		EntryObject->SetNumberField(TEXT("inputCount"), InputCount);
		EntryObject->SetNumberField(TEXT("outputCount"), OutputCount);
		// supportsSortRules (2026-08-25): Smart and Programmable
		// splitters share the AFGBuildableSplitterSmart native class -
		// per-output item-type routing (mSortRules, AddSortRule/
		// RemoveSortRuleAt/SetSortRuleAt/GetSortRules, confirmed public
		// on FGBuildableSplitterSmart.h) needs RPC support this project
		// does NOT yet have. Placement/connection works today via the
		// generic mechanism above; sort-rule configuration is a
		// genuinely separate, not-yet-built capability - this flag lets
		// an agent know not to assume a placed Smart/Programmable
		// splitter can already be configured to route by item type.
		EntryObject->SetBoolField(TEXT("supportsSortRules"), Cast<AFGBuildableSplitterSmart>(AttachmentCDO) != nullptr);

		EntryJsonArray.Add(MakeShared<FJsonValueObject>(EntryObject));
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetArrayField(TEXT("attachments"), EntryJsonArray);

	FString JsonString;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonString);
	FJsonSerializer::Serialize(RootObject, Writer);

	UE_LOG(LogAIModAI, Display, TEXT("LogConveyorAttachmentCatalogAsJson: %s"), *JsonString);

	return JsonString;
}

namespace
{
	// Shared by LogRecipeCatalogAsJson/LogBuildableCatalogAsJson. Amount is
	// FItemAmount's raw internal unit (ItemAmount.h) - for RF_LIQUID/RF_GAS
	// items this is thousandths of a cubic meter (same convention
	// UFGItemDescriptor::GetStackSizeConverted()'s own doc comment
	// describes for fluid stack sizes), NOT pre-converted here. Callers
	// should check the item's "form" field (world.itemCatalog) before
	// interpreting the number - deliberately not doing that conversion in
	// C++, per this project's toolkit-not-solver preference (the AI/Python
	// side should do unit interpretation, not have it baked in here).
	TArray<TSharedPtr<FJsonValue>> ItemAmountsToJsonArray(const TArray<FItemAmount>& Amounts)
	{
		TArray<TSharedPtr<FJsonValue>> Result;
		for (const FItemAmount& Amount : Amounts)
		{
			const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
			Entry->SetStringField(TEXT("itemClass"), Amount.ItemClass ? Amount.ItemClass->GetPathName() : FString());
			Entry->SetStringField(TEXT("itemName"), Amount.ItemClass ? UFGItemDescriptor::GetItemName(Amount.ItemClass).ToString() : FString());
			Entry->SetNumberField(TEXT("amount"), Amount.Amount);
			Result.Add(MakeShared<FJsonValueObject>(Entry));
		}
		return Result;
	}

	FString ResourceFormToString(EResourceForm Form)
	{
		switch (Form)
		{
		case EResourceForm::RF_SOLID: return TEXT("Solid");
		case EResourceForm::RF_LIQUID: return TEXT("Liquid");
		case EResourceForm::RF_GAS: return TEXT("Gas");
		default: return TEXT("Invalid");
		}
	}

	bool IsBuildingRecipe(const TArray<FItemAmount>& Products)
	{
		return Products.Num() > 0 && Products[0].ItemClass && Products[0].ItemClass->IsChildOf(UFGBuildingDescriptor::StaticClass());
	}

	FString ClearanceTypeToString(EClearanceType Type)
	{
		switch (Type)
		{
		case EClearanceType::CT_Default: return TEXT("Default");
		case EClearanceType::CT_Soft: return TEXT("Soft");
		case EClearanceType::CT_BlockEverything: return TEXT("BlockEverything");
		default: return TEXT("Unknown");
		}
	}

	TSharedRef<FJsonObject> VectorToJson(const FVector& V)
	{
		const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetNumberField(TEXT("x"), V.X);
		Object->SetNumberField(TEXT("y"), V.Y);
		Object->SetNumberField(TEXT("z"), V.Z);
		return Object;
	}

	// Shared by LogBuildableCatalogAsJson - a buildable's real footprint,
	// for layout pre-planning (foundation counts, spacing for belt/pipe
	// routing gaps). mClearanceData (FGClearanceData.h) is the SAME data
	// FactoryGame's own construction-overlap checks use - a plain
	// UPROPERTY(EditDefaultsOnly) array of FBox-based clearance volumes,
	// safe to read on a CDO (unlike connector components elsewhere in
	// this file, this is NOT added via Blueprint SCS). Retrieved via the
	// IFGClearanceInterface BlueprintNativeEvent (Execute_ dispatch, not
	// a direct call) since that's the documented, standard way to invoke
	// it regardless of whether a given buildable overrides it further.
	// Reports GetTransformedClearanceBox() (RelativeTransform already
	// applied) so min/max/size are directly in the buildable's own local
	// space - some buildables declare more than one clearance box (e.g.
	// a base volume plus a separate one for an attached arm/platform), so
	// this returns an array, not a single box.
	TArray<TSharedPtr<FJsonValue>> ClearanceDataToJsonArray(const AFGBuildable* BuildableCDO)
	{
		TArray<TSharedPtr<FJsonValue>> Result;
		if (!BuildableCDO->GetClass()->ImplementsInterface(UFGClearanceInterface::StaticClass()))
		{
			return Result;
		}

		TArray<FFGClearanceData> ClearanceData;
		IFGClearanceInterface::Execute_GetClearanceData(const_cast<AFGBuildable*>(BuildableCDO), ClearanceData);

		for (const FFGClearanceData& Clearance : ClearanceData)
		{
			if (!Clearance.IsValid()) { continue; }

			const FBox Box = Clearance.GetTransformedClearanceBox();
			const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
			Entry->SetObjectField(TEXT("min"), VectorToJson(Box.Min));
			Entry->SetObjectField(TEXT("max"), VectorToJson(Box.Max));
			Entry->SetObjectField(TEXT("size"), VectorToJson(Box.GetSize()));
			Entry->SetStringField(TEXT("type"), ClearanceTypeToString(Clearance.Type));
			Result.Add(MakeShared<FJsonValueObject>(Entry));
		}
		return Result;
	}
}

// LogRecipeCatalogAsJson/world.recipeCatalog, LogItemCatalogAsJson/
// world.itemCatalog, LogBuildableCatalogAsJson/world.buildableCatalog
// (2026-08-27, per explicit user request to support pre-planning complex
// builds: "what items can be built, what recipes/alternates build each
// item, what machines are needed, resource/power requirements, input/
// output counts and types, rates including power shards/Somersloop,
// belt/pipe rates"). Belt/pipe rates were already covered by
// world.conveyorBeltTiers/world.pipelineTiers (2026-08-25) - these three
// cover the rest.
//
// Enumeration source: AFGRecipeManager::GetAllRecipes()/
// GetAllItemDescriptors() (FGRecipeManager.h) - confirmed via source
// research to return EVERY recipe/item descriptor in the game, including
// ones not yet unlocked in the current save (unlike the progression-gated
// GetAllAvailableRecipes()). These are plain inline header reads of an
// already-populated TArray, not stub-source themselves - BUT
// AFGRecipeManager::Get() and the private PopulateAllRecipesList() that
// fills those arrays ARE stub-source (Source/FactoryGame/Private/
// FGRecipeManager.cpp), meaning Get() returns null in-Editor/PIE and only
// resolves to real data in the packaged/Alpakit-deployed game. Test these
// three methods against the real Steam session, not Play-in-Editor.
//
// Deliberately NOT pre-computing effective rates (items/min accounting
// for clock speed or Somersloop boost) - these report the raw recipe
// duration/amounts and the building's min/max potential and production-
// boost fields, and leave the arithmetic to the caller, consistent with
// this project's toolkit-not-solver preference elsewhere (Python/AI side
// decides, C++ exposes real data).
FString UAIModFunctionLibrary::LogRecipeCatalogAsJson(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	AFGRecipeManager* RecipeManager = World ? AFGRecipeManager::Get(World) : nullptr;
	if (!RecipeManager)
	{
		UE_LOG(LogAIModAI, Warning, TEXT("LogRecipeCatalogAsJson: AFGRecipeManager::Get() returned null - stub-source in Editor/PIE, only resolves in the packaged game (see this function's doc comment)"));
	}

	TArray<TSharedPtr<FJsonValue>> RecipeJsonArray;
	if (RecipeManager)
	{
		for (const TSubclassOf<UFGRecipe>& RecipeClass : RecipeManager->GetAllRecipes())
		{
			if (!RecipeClass) { continue; }
			const UFGRecipe* RecipeCDO = RecipeClass->GetDefaultObject<UFGRecipe>();
			if (!RecipeCDO) { continue; }

			const TArray<FItemAmount>& Ingredients = RecipeCDO->GetIngredients();
			const TArray<FItemAmount>& Products = RecipeCDO->GetProducts();

			TArray<TSharedPtr<FJsonValue>> ProducedInJsonArray;
			for (const TSubclassOf<UObject>& Producer : UFGRecipe::GetProducedIn(RecipeClass))
			{
				if (Producer) { ProducedInJsonArray.Add(MakeShared<FJsonValueString>(Producer->GetPathName())); }
			}

			const TSharedRef<FJsonObject> EntryObject = MakeShared<FJsonObject>();
			EntryObject->SetStringField(TEXT("recipeClass"), RecipeClass->GetPathName());
			EntryObject->SetStringField(TEXT("displayName"), UFGRecipe::GetRecipeName(RecipeClass).ToString());
			EntryObject->SetBoolField(TEXT("isBuildingRecipe"), IsBuildingRecipe(Products));
			EntryObject->SetNumberField(TEXT("manufacturingDuration"), RecipeCDO->GetManufacturingDuration());
			EntryObject->SetArrayField(TEXT("ingredients"), ItemAmountsToJsonArray(Ingredients));
			EntryObject->SetArrayField(TEXT("products"), ItemAmountsToJsonArray(Products));
			EntryObject->SetArrayField(TEXT("producedIn"), ProducedInJsonArray);
			EntryObject->SetNumberField(TEXT("variablePowerConsumptionConstant"), RecipeCDO->GetPowerConsumptionConstant());
			EntryObject->SetNumberField(TEXT("variablePowerConsumptionFactor"), RecipeCDO->GetPowerConsumptionFactor());

			RecipeJsonArray.Add(MakeShared<FJsonValueObject>(EntryObject));
		}
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetArrayField(TEXT("recipes"), RecipeJsonArray);

	FString JsonString;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonString);
	FJsonSerializer::Serialize(RootObject, Writer);

	UE_LOG(LogAIModAI, Display, TEXT("LogRecipeCatalogAsJson: %d recipe(s)"), RecipeJsonArray.Num());

	return JsonString;
}

// See LogRecipeCatalogAsJson's doc comment above for the shared
// AFGRecipeManager/stub-source caveats - identical here.
FString UAIModFunctionLibrary::LogItemCatalogAsJson(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	AFGRecipeManager* RecipeManager = World ? AFGRecipeManager::Get(World) : nullptr;
	if (!RecipeManager)
	{
		UE_LOG(LogAIModAI, Warning, TEXT("LogItemCatalogAsJson: AFGRecipeManager::Get() returned null - stub-source in Editor/PIE, only resolves in the packaged game"));
	}

	TArray<TSharedPtr<FJsonValue>> ItemJsonArray;
	if (RecipeManager)
	{
		for (const TSubclassOf<UFGItemDescriptor>& ItemClass : RecipeManager->GetAllItemDescriptors())
		{
			if (!ItemClass) { continue; }

			const EResourceForm Form = UFGItemDescriptor::GetForm(ItemClass);

			const TSharedRef<FJsonObject> EntryObject = MakeShared<FJsonObject>();
			EntryObject->SetStringField(TEXT("itemClass"), ItemClass->GetPathName());
			EntryObject->SetStringField(TEXT("name"), UFGItemDescriptor::GetItemName(ItemClass).ToString());
			EntryObject->SetStringField(TEXT("form"), ResourceFormToString(Form));
			EntryObject->SetBoolField(TEXT("isBuildingDescriptor"), ItemClass->IsChildOf(UFGBuildingDescriptor::StaticClass()));
			EntryObject->SetNumberField(TEXT("stackSize"), UFGItemDescriptor::GetStackSize(ItemClass));
			EntryObject->SetNumberField(TEXT("energyValue"), UFGItemDescriptor::GetEnergyValue(ItemClass));
			EntryObject->SetNumberField(TEXT("radioactiveDecay"), UFGItemDescriptor::GetRadioactiveDecay(ItemClass));
			if (Form == EResourceForm::RF_GAS)
			{
				EntryObject->SetStringField(TEXT("gasType"), UFGItemDescriptor::GetGasType(ItemClass) == EGasType::GT_ENERGY ? TEXT("Energy") : TEXT("Normal"));
			}

			ItemJsonArray.Add(MakeShared<FJsonValueObject>(EntryObject));
		}
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetArrayField(TEXT("items"), ItemJsonArray);

	FString JsonString;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonString);
	FJsonSerializer::Serialize(RootObject, Writer);

	UE_LOG(LogAIModAI, Display, TEXT("LogItemCatalogAsJson: %d item(s)"), ItemJsonArray.Num());

	return JsonString;
}

// Derived from the recipe catalog (filters to isBuildingRecipe==true and
// resolves each to its real AFGBuildable class via
// UFGBuildingDescriptor::GetBuildableClass()), rather than a separate
// enumeration source - a building's construction cost is exactly its
// recipe's ingredients, so this avoids a second, potentially-inconsistent
// catalog. "category" is determined by C++ class hierarchy
// (AFGBuildableGenerator/AFGBuildableResourceExtractorBase/
// AFGBuildableManufacturer), not any FactoryGame-declared enum - covers
// the buildings most relevant to production planning; non-factory
// buildables (foundations, walls, belts, poles...) still appear with
// category "Other" and zeroed power/potential fields. Power/potential
// fields are read off each buildable class's CDO (never spawned in the
// world) - safe for class-level defaults per the same technique
// world.conveyorAttachments/world.conveyorBeltTiers already use.
FString UAIModFunctionLibrary::LogBuildableCatalogAsJson(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	AFGRecipeManager* RecipeManager = World ? AFGRecipeManager::Get(World) : nullptr;
	if (!RecipeManager)
	{
		UE_LOG(LogAIModAI, Warning, TEXT("LogBuildableCatalogAsJson: AFGRecipeManager::Get() returned null - stub-source in Editor/PIE, only resolves in the packaged game"));
	}

	TArray<TSharedPtr<FJsonValue>> BuildableJsonArray;
	if (RecipeManager)
	{
		for (const TSubclassOf<UFGRecipe>& RecipeClass : RecipeManager->GetAllRecipes())
		{
			if (!RecipeClass) { continue; }
			const UFGRecipe* RecipeCDO = RecipeClass->GetDefaultObject<UFGRecipe>();
			if (!RecipeCDO) { continue; }

			const TArray<FItemAmount>& Products = RecipeCDO->GetProducts();
			if (!IsBuildingRecipe(Products)) { continue; }

			const TSubclassOf<UFGBuildingDescriptor> BuildingDescriptorClass(Products[0].ItemClass.Get());
			const TSubclassOf<AFGBuildable> BuildableClass = UFGBuildingDescriptor::GetBuildableClass(BuildingDescriptorClass);
			const AFGBuildable* BuildableCDO = BuildableClass ? BuildableClass->GetDefaultObject<AFGBuildable>() : nullptr;
			if (!BuildableCDO)
			{
				continue;
			}

			FString Category = TEXT("Other");
			if (Cast<AFGBuildableGenerator>(BuildableCDO)) { Category = TEXT("Generator"); }
			else if (Cast<AFGBuildableResourceExtractorBase>(BuildableCDO)) { Category = TEXT("Extractor"); }
			else if (Cast<AFGBuildableManufacturer>(BuildableCDO)) { Category = TEXT("Manufacturer"); }

			const TSharedRef<FJsonObject> EntryObject = MakeShared<FJsonObject>();
			EntryObject->SetStringField(TEXT("recipeClass"), RecipeClass->GetPathName());
			EntryObject->SetStringField(TEXT("buildableClass"), BuildableClass->GetPathName());
			EntryObject->SetStringField(TEXT("category"), Category);
			EntryObject->SetArrayField(TEXT("constructionCost"), ItemAmountsToJsonArray(RecipeCDO->GetIngredients()));
			EntryObject->SetArrayField(TEXT("clearance"), ClearanceDataToJsonArray(BuildableCDO));

			if (const AFGBuildableFactory* FactoryCDO = Cast<AFGBuildableFactory>(BuildableCDO))
			{
				EntryObject->SetBoolField(TEXT("runsOnPower"), FactoryCDO->RunsOnPower());
				EntryObject->SetNumberField(TEXT("idlePowerConsumption"), FactoryCDO->GetIdlePowerConsumption());
				EntryObject->SetNumberField(TEXT("producingPowerConsumptionBase"), FactoryCDO->GetProducingPowerConsumptionBase());
				EntryObject->SetNumberField(TEXT("defaultProducingPowerConsumption"), FactoryCDO->GetDefaultProducingPowerConsumption());
				EntryObject->SetNumberField(TEXT("minPotential"), FactoryCDO->GetCurrentMinPotential());
				EntryObject->SetNumberField(TEXT("maxPotential"), FactoryCDO->GetMaxPotential());
				EntryObject->SetBoolField(TEXT("canChangePotential"), FactoryCDO->GetCanChangePotential());

				// mPotentialShardSlots (power-shard overclock slot count)
				// has no public getter, but is a real UPROPERTY - same
				// reflection technique as belts' mMaxIncline elsewhere in
				// this file. maxPotential above is explicitly documented
				// on the class ("Default maximum potential on the
				// buildable, NOT accounting for the installed power
				// shards") as the un-overclocked baseline - this slot
				// count is what a caller needs to know how much headroom
				// power shards can actually add on top of it.
				//
				// mPotentialShardSlots is itself gated by
				// mOverridePotentialShardSlots (EditCondition in its own
				// UPROPERTY meta) - live-confirmed most buildings
				// (Constructor, Miner) report potentialShardSlots=0 with
				// the override off, even though real instances DO accept
				// shards. When the override is off, this field's value is
				// meaningless - the real slot count comes from some
				// global default this per-building reflection read
				// cannot see. Report overridesShardSlotCount so callers
				// can tell a genuine 0 (override on, deliberately no
				// slots) from "unknown, falls back to a global default
				// not exposed here" - a real, documented gap rather than
				// a silently wrong number.
				bool bOverridesShardSlotCount = false;
				if (const FBoolProperty* OverrideProperty = FindFProperty<FBoolProperty>(FactoryCDO->GetClass(), TEXT("mOverridePotentialShardSlots")))
				{
					bOverridesShardSlotCount = OverrideProperty->GetPropertyValue_InContainer(FactoryCDO);
				}
				EntryObject->SetBoolField(TEXT("overridesShardSlotCount"), bOverridesShardSlotCount);
				if (bOverridesShardSlotCount)
				{
					if (const FIntProperty* ShardSlotsProperty = FindFProperty<FIntProperty>(FactoryCDO->GetClass(), TEXT("mPotentialShardSlots")))
					{
						EntryObject->SetNumberField(TEXT("potentialShardSlots"), ShardSlotsProperty->GetPropertyValue_InContainer(FactoryCDO));
					}
				}
			}

			if (const AFGBuildableGenerator* GeneratorCDO = Cast<AFGBuildableGenerator>(BuildableCDO))
			{
				EntryObject->SetNumberField(TEXT("powerProductionCapacity"), GeneratorCDO->GetPowerProductionCapacity());
				EntryObject->SetNumberField(TEXT("defaultPowerProductionCapacity"), GeneratorCDO->GetDefaultPowerProductionCapacity());
			}

			// GetDefaultComponents(), not GetComponents() - see the
			// identical fix/comment in LogConveyorAttachmentCatalogAsJson
			// above for why a plain CDO GetComponents<>() scan misses
			// every Blueprint-SCS-added connector (which is most of
			// them).
			TArray<UFGFactoryConnectionComponent*> FactoryConnections;
			BuildableCDO->GetDefaultComponents<UFGFactoryConnectionComponent>(FactoryConnections);
			int32 FactoryInputCount = 0, FactoryOutputCount = 0;
			for (const UFGFactoryConnectionComponent* Connection : FactoryConnections)
			{
				if (!IsValid(Connection)) { continue; }
				if (Connection->GetDirection() == EFactoryConnectionDirection::FCD_INPUT) { ++FactoryInputCount; }
				else if (Connection->GetDirection() == EFactoryConnectionDirection::FCD_OUTPUT) { ++FactoryOutputCount; }
			}
			EntryObject->SetNumberField(TEXT("factoryInputCount"), FactoryInputCount);
			EntryObject->SetNumberField(TEXT("factoryOutputCount"), FactoryOutputCount);

			TArray<UFGPipeConnectionComponentBase*> PipeConnections;
			BuildableCDO->GetDefaultComponents<UFGPipeConnectionComponentBase>(PipeConnections);
			int32 PipeInputCount = 0, PipeOutputCount = 0;
			for (const UFGPipeConnectionComponentBase* Connection : PipeConnections)
			{
				if (!IsValid(Connection)) { continue; }
				if (Connection->GetPipeConnectionType() == EPipeConnectionType::PCT_CONSUMER) { ++PipeInputCount; }
				else if (Connection->GetPipeConnectionType() == EPipeConnectionType::PCT_PRODUCER) { ++PipeOutputCount; }
			}
			EntryObject->SetNumberField(TEXT("pipeInputCount"), PipeInputCount);
			EntryObject->SetNumberField(TEXT("pipeOutputCount"), PipeOutputCount);

			TArray<UFGPowerConnectionComponent*> PowerConnections;
			BuildableCDO->GetDefaultComponents<UFGPowerConnectionComponent>(PowerConnections);
			EntryObject->SetNumberField(TEXT("powerConnectionCount"), PowerConnections.Num());

			BuildableJsonArray.Add(MakeShared<FJsonValueObject>(EntryObject));
		}
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetArrayField(TEXT("buildables"), BuildableJsonArray);

	FString JsonString;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonString);
	FJsonSerializer::Serialize(RootObject, Writer);

	UE_LOG(LogAIModAI, Display, TEXT("LogBuildableCatalogAsJson: %d buildable(s)"), BuildableJsonArray.Num());

	return JsonString;
}

void UAIModFunctionLibrary::ConstructConveyorBelt(UObject* WorldContextObject, const FString& SourceBuildableId, const FString& DestBuildableId, const FString& RecipeClassPath, const FString& RouteMode, bool bDryRun, TFunction<void(const FAIModOperationResult&)> OnComplete)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context")));
		return;
	}

	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Character)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_PLAYER"), TEXT("No local AFGCharacterPlayer (player index 0)")));
		return;
	}

	AFGBuildable* SourceBuildable = FindBuildableById(World, SourceBuildableId);
	if (!SourceBuildable)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No buildable found with id '%s'"), *SourceBuildableId)));
		return;
	}
	AFGBuildable* DestBuildable = FindBuildableById(World, DestBuildableId);
	if (!DestBuildable)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No buildable found with id '%s'"), *DestBuildableId)));
		return;
	}

	UFGFactoryConnectionComponent* SourceConnection = FindFreeFactoryConnection(SourceBuildable, EFactoryConnectionDirection::FCD_OUTPUT);
	if (!SourceConnection)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_FACTORY_CONNECTION"), FString::Printf(TEXT("'%s' has no free Output factory connection component"), *SourceBuildableId)));
		return;
	}
	UFGFactoryConnectionComponent* DestConnection = FindFreeFactoryConnection(DestBuildable, EFactoryConnectionDirection::FCD_INPUT);
	if (!DestConnection)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_FACTORY_CONNECTION"), FString::Printf(TEXT("'%s' has no free Input factory connection component"), *DestBuildableId)));
		return;
	}

	// Caller-chosen belt tier (2026-08-25) - was hardcoded to
	// Recipe_ConveyorBeltMk1 before this; any of Recipe_ConveyorBeltMk1..Mk6
	// resolve the same way. Same validation posture as
	// ConstructBuildingAtPosition's RecipeClassPath - not a generic
	// "load any class" capability, just requires a real UFGRecipe.
	UClass* BeltRecipeClass = LoadObject<UClass>(nullptr, *RecipeClassPath);
	if (!BeltRecipeClass || !BeltRecipeClass->IsChildOf(UFGRecipe::StaticClass()))
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("INVALID_RECIPE"), FString::Printf(TEXT("'%s' did not resolve to a UFGRecipe subclass"), *RecipeClassPath)));
		return;
	}
	const TSubclassOf<UFGRecipe> RecipeClass = BeltRecipeClass;

	Character->HotKeyRecipe(RecipeClass);

	AFGBuildGun* BuildGun = Character->GetBuildGun();
	if (!BuildGun)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_BUILD_GUN"), TEXT("AFGCharacterPlayer::GetBuildGun() returned null")));
		return;
	}

	UFGBuildGunStateBuild* BuildState = Cast<UFGBuildGunStateBuild>(BuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD));
	if (!BuildState)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_BUILD_STATE"), TEXT("Could not resolve UFGBuildGunStateBuild from the build gun")));
		return;
	}

	AFGHologram* Hologram = BuildState->GetHologram();
	AFGConveyorBeltHologram* BeltHologram = Cast<AFGConveyorBeltHologram>(Hologram);
	if (!BeltHologram)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("HOLOGRAM_SPAWN_FAILED"),
			FString::Printf(TEXT("HotKeyRecipe(%s) did not result in an AFGConveyorBeltHologram (got %s)"),
				*RecipeClassPath, Hologram ? *Hologram->GetClass()->GetName() : TEXT("null"))));
		return;
	}

	// RouteMode (2026-08-25, added after live-diagnosing that the 2-click
	// TrySnapToActor flow fails ("Conveyor Belt is too long!"/"Invalid
	// placement!") for ANY meaningful direction mismatch between source
	// and destination connectors). Real, confirmed-on-disk asset paths
	// (grepped from Holo_ConveyorBelt.uasset's own string table, not
	// guessed): AFGHologram::SetBuildModeOverride() (public,
	// FGHologram.h) accepts one of
	// "/Game/FactoryGame/Buildable/Factory/-Shared/BuildGunModes/BuildMode_Default"
	// (the implicit default when nothing is overridden - what the player
	// UX calls "Auto"), "...BuildMode_Straight", or "...BuildMode_Curve"
	// - AFGConveyorBeltHologram exposes exactly two of these via its own
	// mBuildModeStraight/mBuildModeCurve fields (GetSupportedBuildModes_Implementation).
	// Empty RouteMode (default) leaves the hologram's own default mode
	// untouched - matches prior behavior exactly. NOT YET LIVE-VERIFIED
	// that forcing Curve actually resolves the bend failures above - this
	// is a well-evidenced hypothesis (AutoRouteSpline's own doc comment:
	// "routes the spline to the new location, inserting bends and
	// straights"), not a proven fix, since the private engine logic
	// behind SetBuildModeOverride()/AutoRouteSpline() is stub-source in
	// this SDK like everything else - only the public entry point and
	// real asset paths are confirmed from source/binary inspection.
	if (!RouteMode.IsEmpty())
	{
		FString RouteModeAssetPath;
		if (RouteMode.Equals(TEXT("Straight"), ESearchCase::IgnoreCase))
		{
			RouteModeAssetPath = TEXT("/Game/FactoryGame/Buildable/Factory/-Shared/BuildGunModes/BuildMode_Straight.BuildMode_Straight_C");
		}
		else if (RouteMode.Equals(TEXT("Curve"), ESearchCase::IgnoreCase))
		{
			RouteModeAssetPath = TEXT("/Game/FactoryGame/Buildable/Factory/-Shared/BuildGunModes/BuildMode_Curve.BuildMode_Curve_C");
		}
		else if (RouteMode.Equals(TEXT("Auto"), ESearchCase::IgnoreCase) || RouteMode.Equals(TEXT("Default"), ESearchCase::IgnoreCase))
		{
			RouteModeAssetPath = TEXT("/Game/FactoryGame/Buildable/Factory/-Shared/BuildGunModes/BuildMode_Default.BuildMode_Default_C");
		}
		else
		{
			Character->UnequipBuildGun();
			OnComplete(FAIModOperationResult::Failure(TEXT("INVALID_ROUTE_MODE"), FString::Printf(TEXT("'%s' is not one of \"Straight\", \"Curve\", \"Auto\""), *RouteMode)));
			return;
		}

		UClass* RouteModeClass = LoadObject<UClass>(nullptr, *RouteModeAssetPath);
		if (!RouteModeClass || !RouteModeClass->IsChildOf(UFGHologramBuildModeDescriptor::StaticClass()))
		{
			Character->UnequipBuildGun();
			OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), FString::Printf(TEXT("Failed to resolve '%s' as a UFGHologramBuildModeDescriptor"), *RouteModeAssetPath)));
			return;
		}

		BeltHologram->SetBuildModeOverride(TSubclassOf<UFGHologramBuildModeDescriptor>(RouteModeClass));
		UE_LOG(LogAIModAI, Display, TEXT("ConstructConveyorBelt: applied RouteMode='%s' (%s)"), *RouteMode, *RouteModeAssetPath);
	}

	// Using FVector::UpVector for Normal/ImpactNormal here (as an
	// arbitrary placeholder) previously produced a live
	// "Invalid Conveyor Belt shape! (hard)" CanConstruct() failure even
	// though both endpoints snapped cleanly (stepComplete/connectedCount
	// looked correct) - the spline's arrive/leave tangent is evidently
	// derived from the hit normal, so an UpVector normal on a
	// horizontally-facing connector produced a degenerate tangent.
	// UFGFactoryConnectionComponent::GetConnectorNormal() (GetComponentRotation().Vector())
	// is the connector's real outward-facing direction - use that instead.
	auto MakeHitAt = [](AFGBuildable* Buildable, UFGFactoryConnectionComponent* Connection) -> FHitResult
	{
		FHitResult Hit;
		Hit.Location = Connection->GetConnectorLocation();
		Hit.ImpactPoint = Hit.Location;
		Hit.Normal = Connection->GetConnectorNormal();
		Hit.ImpactNormal = Hit.Normal;
		Hit.HitObjectHandle = FActorInstanceHandle(Buildable);
		Hit.bBlockingHit = true;
		return Hit;
	};

	// Diagnostic evidence-gathering (2026-08-25): the "Invalid aim
	// location! (hard)"/"Invalid Conveyor Belt shape! (hard)"
	// disqualifiers have been observed to flip depending solely on
	// Hit.Normal, at fixed player/buildable positions - log everything
	// relevant to correlate. This block never changes behavior, only logs.
	auto SummarizeDisqualifiers = [](AFGConveyorBeltHologram* H) -> FString
	{
		TArray<TSubclassOf<UFGConstructDisqualifier>> Disqualifiers;
		H->GetConstructDisqualifiers(Disqualifiers);
		TArray<FString> Texts;
		for (const TSubclassOf<UFGConstructDisqualifier>& D : Disqualifiers)
		{
			Texts.Add(FString::Printf(TEXT("%s (%s)"), *UFGConstructDisqualifier::GetDisqualifyingText(D).ToString(),
				UFGConstructDisqualifier::GetIsSoftDisqualifier(D) ? TEXT("soft") : TEXT("hard")));
		}
		return Texts.IsEmpty() ? TEXT("<none>") : FString::Join(Texts, TEXT("; "));
	};
	UE_LOG(LogAIModAI, Display, TEXT("ConstructConveyorBelt diagnostic: playerLoc=%s playerRot=%s sourceConnectorLoc=%s sourceConnectorNormal=%s sourceConnectorClearanceLoc=%s destConnectorLoc=%s destConnectorNormal=%s destConnectorClearanceLoc=%s"),
		*Character->GetActorLocation().ToString(), *Character->GetActorRotation().ToString(),
		*SourceConnection->GetConnectorLocation().ToString(), *SourceConnection->GetConnectorNormal().ToString(), *SourceConnection->GetConnectorLocation(true).ToString(),
		*DestConnection->GetConnectorLocation().ToString(), *DestConnection->GetConnectorNormal().ToString(), *DestConnection->GetConnectorLocation(true).ToString());

	// Player-independence, take 2 (2026-08-27): the disqualifier-ignore
	// alone was NOT sufficient - live-confirmed this session that
	// AutoRouteSpline()/the belt's internal pathing (stub source, called
	// from inside DoMultiStepPlacement/TrySnapToActor below) reads the
	// player CONTROLLER's live rotation as an implicit routing hint,
	// completely independent of the disqualifier check and independent
	// of the correct, connector-anchored FHitResults built above. Proven
	// live: identical connectConveyor calls against the same two
	// buildables produced a genuinely mis-terminated belt (far end
	// landing near the player's actual look direction, not the
	// destination connector) when the player's camera was aimed
	// elsewhere, and a correctly-terminated belt (verified via
	// world.connections) once the player was aimed at the destination.
	// Since this function is already anchored to explicit buildable IDs,
	// there's no legitimate reason the result should depend on the
	// player at all - so, same spirit as ConstructBuildingAtPosition's
	// fix, point the controller at a DETERMINISTIC target computed from
	// the two connectors themselves (never the player's real aim), and
	// reassert it every poll tick below so it survives the hologram
	// re-deriving state each tick (see that same "jetpack hovering"
	// class of bug in ConstructBuildingAtPosition's history).
	const FRotator BeltDeterministicLook = (DestConnection->GetConnectorLocation() - SourceConnection->GetConnectorLocation()).Rotation();
	if (AController* BeltController = Character->GetController())
	{
		BeltController->SetControlRotation(BeltDeterministicLook);
	}

	// Step 1 of the flow found live via DebugCheckConveyorSnap - fix the
	// start point on the source's Output connection. UpdateHologramPlacement()
	// before TrySnapToActor() is not optional: DebugCheckConveyorSnap's
	// successful trace called both, and omitting it here reproduced a
	// live "Invalid aim location! (hard)" CanConstruct() failure even
	// though the snap/step/connectedCount indicators all looked correct -
	// evidently CanConstruct()'s aim-location disqualifier reads state
	// that only UpdateHologramPlacement() sets, not TrySnapToActor() alone.
	const FHitResult StartHit = MakeHitAt(SourceBuildable, SourceConnection);
	BeltHologram->UpdateHologramPlacement(StartHit);
	BeltHologram->TrySnapToActor(StartHit);
	const bool bStartStepComplete = BeltHologram->DoMultiStepPlacement(true);
	const ESplineHologramBuildStep StepAfterStart = BeltHologram->GetCurrentBuildStep();

	UE_LOG(LogAIModAI, Display, TEXT("ConstructConveyorBelt: source=%s dest=%s after start click: stepComplete=%s step=%d disqualifiers=[%s]"),
		*SourceBuildableId, *DestBuildableId, bStartStepComplete ? TEXT("true") : TEXT("false"), static_cast<int32>(StepAfterStart), *SummarizeDisqualifiers(BeltHologram));

	if (bStartStepComplete)
	{
		// Unexpected - a two-endpoint belt shouldn't complete on the
		// first click. Report exactly what happened rather than
		// guessing further; do not proceed to a second click on an
		// already-"complete" hologram.
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("UNEXPECTED_STEP_COMPLETE"), TEXT("DoMultiStepPlacement() reported complete after only the start click")));
		return;
	}

	// Step 2 - the destination's free Input connection. Same
	// UpdateHologramPlacement()-before-TrySnapToActor() requirement as
	// step 1 above.
	const FHitResult EndHit = MakeHitAt(DestBuildable, DestConnection);
	BeltHologram->UpdateHologramPlacement(EndHit);
	BeltHologram->TrySnapToActor(EndHit);
	const bool bEndStepComplete = BeltHologram->DoMultiStepPlacement(true);
	const ESplineHologramBuildStep StepAfterEnd = BeltHologram->GetCurrentBuildStep();
	const TArray<AFGBuildable*> ConnectedBuildables = BeltHologram->GetAnyConnectedBuildables();

	UE_LOG(LogAIModAI, Display, TEXT("ConstructConveyorBelt: source=%s dest=%s after end click: stepComplete=%s step=%d connectedCount=%d disqualifiers=[%s]"),
		*SourceBuildableId, *DestBuildableId, bEndStepComplete ? TEXT("true") : TEXT("false"), static_cast<int32>(StepAfterEnd), ConnectedBuildables.Num(), *SummarizeDisqualifiers(BeltHologram));

	if (!bEndStepComplete)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("PLACEMENT_INCOMPLETE"),
			FString::Printf(TEXT("DoMultiStepPlacement() did not report complete after the end click - step=%d connectedCount=%d, may need a third step"), static_cast<int32>(StepAfterEnd), ConnectedBuildables.Num())));
		return;
	}

	struct FPollState
	{
		TWeakObjectPtr<AFGConveyorBeltHologram> Hologram;
		TWeakObjectPtr<AFGCharacterPlayer> Character;
		TWeakObjectPtr<UWorld> World;
		FString SourceBuildableId;
		FString DestBuildableId;
		bool bDryRun = true;
		FRotator DeterministicLook;
		int32 AttemptsRemaining = 120; // safety cap - real ticks, not a fixed duration
		int32 AttemptsTaken = 0;
		TFunction<void(const FAIModOperationResult&)> OnComplete;
	};
	const TSharedRef<FPollState> PollState = MakeShared<FPollState>();
	PollState->Hologram = BeltHologram;
	PollState->Character = Character;
	PollState->World = World;
	PollState->SourceBuildableId = SourceBuildableId;
	PollState->DestBuildableId = DestBuildableId;
	PollState->bDryRun = bDryRun;
	PollState->DeterministicLook = BeltDeterministicLook;
	PollState->OnComplete = MoveTemp(OnComplete);

	const TSharedRef<TFunction<void()>> PollFn = MakeShared<TFunction<void()>>();
	*PollFn = [PollState, PollFn]()
	{
		++PollState->AttemptsTaken;

		AFGConveyorBeltHologram* PollHologram = PollState->Hologram.Get();
		UWorld* PollWorld = PollState->World.Get();
		AFGCharacterPlayer* PollCharacter = PollState->Character.Get();
		if (!IsValid(PollHologram) || !PollWorld)
		{
			UE_LOG(LogAIModAI, Warning, TEXT("ConstructConveyorBelt (deferred): hologram or world became invalid while polling (after %d tick(s))"), PollState->AttemptsTaken);
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("HOLOGRAM_INVALIDATED"), TEXT("Hologram or world became invalid while polling")));
			return;
		}

		// Re-assert every tick, not just once before the poll started -
		// AutoRouteSpline()/UpdateHologramPlacement() (stub source) re-reads
		// the controller's CURRENT rotation each tick, so a live player
		// moving their own camera between ticks (or just standing still
		// while looking around) can still drag the resolved path off the
		// one-time value set above. Same fix shape as
		// ConstructBuildingAtPosition's "jetpack hovering" fix.
		if (IsValid(PollCharacter))
		{
			if (AController* PollController = PollCharacter->GetController())
			{
				PollController->SetControlRotation(PollState->DeterministicLook);
			}
		}

		TArray<TSubclassOf<UFGConstructDisqualifier>> Disqualifiers;
		PollHologram->GetConstructDisqualifiers(Disqualifiers);
		const bool bStillInitializing = Disqualifiers.Contains(TSubclassOf<UFGConstructDisqualifier>(UFGCDInitializing::StaticClass()));

		--PollState->AttemptsRemaining;
		if (bStillInitializing && PollState->AttemptsRemaining > 0)
		{
			PollWorld->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
			return;
		}

		// Player-independence (2026-08-26, explicit user direction): don't
		// use the real (stub-source, opaque) CanConstruct() here - it has
		// no way to selectively ignore a disqualifier. Belts/lifts are
		// always built between two EXPLICIT existing buildables (never
		// player-relative), so there is no legitimate reason a fixed
		// source->dest connection should ever depend on where the player
		// happens to be standing or looking. Always ignore
		// UFGCDInvalidAimLocation here, same "any other hard disqualifier
		// blocks" rule as ConstructBuildingAtPosition's own manual
		// disqualifier loop. This is what let the SetControlRotation()
		// aim-pointing workaround be removed entirely from this function -
		// belt/lift construction results are now fully independent of
		// player position/camera.
		// UnlimitedResources (2026-08-27) - see ConstructBuildingAtPosition's
		// comment on this being a player-controlled mod setting, not a
		// per-call flag.
		const bool bUnlimitedResources = UAIModFunctionLibrary::GetAIModConfigBool(PollWorld, TEXT("UnlimitedResources"), false);

		bool bCanConstruct = true;
		TArray<FString> DisqualifierTexts;
		for (const TSubclassOf<UFGConstructDisqualifier>& DisqualifierClass : Disqualifiers)
		{
			const bool bIgnoredForPlayerIndependence = (DisqualifierClass == UFGCDInvalidAimLocation::StaticClass())
				|| (bUnlimitedResources && DisqualifierClass == UFGCDUnaffordable::StaticClass());
			const bool bIsSoft = UFGConstructDisqualifier::GetIsSoftDisqualifier(DisqualifierClass);
			if (!bIgnoredForPlayerIndependence && !bIsSoft)
			{
				bCanConstruct = false;
			}
			DisqualifierTexts.Add(FString::Printf(TEXT("%s (%s%s)"),
				*UFGConstructDisqualifier::GetDisqualifyingText(DisqualifierClass).ToString(),
				bIsSoft ? TEXT("soft") : TEXT("hard"), bIgnoredForPlayerIndependence ? TEXT(", ignored") : TEXT("")));
		}
		const FString DisqualifierSummary = DisqualifierTexts.IsEmpty() ? TEXT("<none>") : FString::Join(DisqualifierTexts, TEXT("; "));

		UE_LOG(LogAIModAI, Display, TEXT("ConstructConveyorBelt (deferred, resolved after %d real tick(s)): source=%s dest=%s dryRun=%s canConstruct=%s disqualifiers=[%s]"),
			PollState->AttemptsTaken, *PollState->SourceBuildableId, *PollState->DestBuildableId, PollState->bDryRun ? TEXT("true") : TEXT("false"),
			bCanConstruct ? TEXT("true") : TEXT("false"), *DisqualifierSummary);

		if (!bCanConstruct)
		{
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("CANNOT_CONSTRUCT"), DisqualifierSummary));
			return;
		}

		if (PollState->bDryRun)
		{
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Success());
			return;
		}

		AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(PollWorld);
		const FNetConstructionID ConstructionID = BuildableSubsystem ? BuildableSubsystem->GetNewNetConstructionID() : FNetConstructionID();

		AFGBuildGun* PollBuildGun = IsValid(PollCharacter) ? PollCharacter->GetBuildGun() : nullptr;
		UFGBuildGunStateBuild* PollBuildState = PollBuildGun ? Cast<UFGBuildGunStateBuild>(PollBuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD)) : nullptr;
		if (!PollBuildState)
		{
			UE_LOG(LogAIModAI, Error, TEXT("ConstructConveyorBelt (deferred): lost the build state before constructing - aborting, nothing built"));
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("Lost the build state before constructing")));
			return;
		}

		PollBuildState->InternalConstructHologram(ConstructionID);

		UE_LOG(LogAIModAI, Display, TEXT("ConstructConveyorBelt (deferred, resolved after %d real tick(s)): construction attempted via InternalConstructHologram - source=%s dest=%s"),
			PollState->AttemptsTaken, *PollState->SourceBuildableId, *PollState->DestBuildableId);

		if (IsValid(PollCharacter))
		{
			PollCharacter->UnequipBuildGun();
		}

		PollState->OnComplete(FAIModOperationResult::Success());
	};

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
}

// LogConveyorLiftTiersAsJson (2026-08-25, vertical conveyor groundwork,
// per explicit user request) - mirrors LogConveyorBeltTiersAsJson's
// structure. Recipe_ConveyorLiftMk1..Mk6 (all six confirmed present on
// disk, same "Mk1..Mk6" naming as belts) resolve to AFGBuildableConveyorLift
// (AFGBuildableConveyorBase's OTHER direct subclass alongside regular
// belts - confirmed from source, shares GetSpeed()/GetConnection0()/
// GetConnection1()). Deliberately does NOT report min/max height limits:
// AFGConveyorLiftHologram's mStepHeight/mMinimumHeight/mMaximumHeight/
// mMinimumHeightWithVerticalConnection are plain private float members
// with NO UPROPERTY macro (confirmed from header) - unlike every other
// reflection-based CDO read in this file (belts' mMaxIncline, pipes'
// mMaxSplineLength/etc.), FindFProperty<FFloatProperty> cannot find a
// non-UPROPERTY field at all, since UHT never generates reflection data
// for it. This is a genuine, real gap (not yet solved) rather than an
// omission - real height limits remain unknown until discovered another
// way (e.g. live binary-search construction attempts).
FString UAIModFunctionLibrary::LogConveyorLiftTiersAsJson(UObject* WorldContextObject)
{
	static const TCHAR* RecipePaths[] = {
		TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorLiftMk1.Recipe_ConveyorLiftMk1_C"),
		TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorLiftMk2.Recipe_ConveyorLiftMk2_C"),
		TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorLiftMk3.Recipe_ConveyorLiftMk3_C"),
		TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorLiftMk4.Recipe_ConveyorLiftMk4_C"),
		TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorLiftMk5.Recipe_ConveyorLiftMk5_C"),
		TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorLiftMk6.Recipe_ConveyorLiftMk6_C"),
	};

	TArray<TSharedPtr<FJsonValue>> TierJsonArray;
	for (const TCHAR* RecipePath : RecipePaths)
	{
		const TSubclassOf<AFGBuildable> BuildableClass = ResolveBuildableClassForRecipe(RecipePath);
		const AFGBuildableConveyorLift* LiftCDO = BuildableClass ? Cast<AFGBuildableConveyorLift>(BuildableClass->GetDefaultObject()) : nullptr;
		if (!LiftCDO)
		{
			UE_LOG(LogAIModAI, Warning, TEXT("LogConveyorLiftTiersAsJson: could not resolve a AFGBuildableConveyorLift CDO for '%s' - omitting"), RecipePath);
			continue;
		}

		const TSharedRef<FJsonObject> TierObject = MakeShared<FJsonObject>();
		TierObject->SetStringField(TEXT("recipeClass"), RecipePath);
		TierObject->SetStringField(TEXT("buildableClass"), BuildableClass->GetPathName());
		// speed: AFGBuildableConveyorBase::GetSpeed(), same unit-ambiguity
		// caveat as belts (see LogConveyorBeltTiersAsJson) - "Speed of
		// this conveyor", no documented unit.
		TierObject->SetNumberField(TEXT("speed"), LiftCDO->GetSpeed());

		TierJsonArray.Add(MakeShared<FJsonValueObject>(TierObject));
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetArrayField(TEXT("tiers"), TierJsonArray);

	FString JsonString;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonString);
	FJsonSerializer::Serialize(RootObject, Writer);

	UE_LOG(LogAIModAI, Display, TEXT("LogConveyorLiftTiersAsJson: %s"), *JsonString);

	return JsonString;
}

// ConstructConveyorLift (2026-08-25, vertical conveyor groundwork, per
// explicit user request: "add support for vertical conveyors, these can
// be used strategically to transition from miners locked to the terrain
// and raised foundations providing a cleaner build area"). Deliberate
// near-mirror of ConstructConveyorBelt's two-click TrySnapToActor flow -
// AFGConveyorLiftHologram is NOT a spline hologram (confirmed from
// source: AFGConveyorLiftHologram : AFGBuildableHologram directly, NOT
// AFGSplineHologram like belts/pipes - a vertical lift is a straight
// column, no bending), but it DOES override TrySnapToActor/
// DoMultiStepPlacement itself, so the same click-driven pattern applies:
// UpdateHologramPlacement() before each TrySnapToActor(), the connector's
// real GetConnectorNormal() (not a placeholder) in the synthetic hit.
// Reuses FindFreeFactoryConnection/UFGFactoryConnectionComponent -
// AFGBuildableConveyorLift shares the exact same connection component
// type as regular belts (both derive from AFGBuildableConveyorBase).
// NOT YET LIVE-TESTED. No post-end-click connectivity diagnostic is
// available here (unlike belts' GetAnyConnectedBuildables() or pipes'
// IsConnectionSnapped(), inherited from AFGSplineHologram which this
// hologram does NOT derive from) - only the disqualifier list is logged.
// No RouteMode param - lifts are a fixed vertical column, no bend/curve
// concept applies.
void UAIModFunctionLibrary::ConstructConveyorLift(UObject* WorldContextObject, const FString& SourceBuildableId, const FString& DestBuildableId, const FString& RecipeClassPath, bool bDryRun, TFunction<void(const FAIModOperationResult&)> OnComplete)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context")));
		return;
	}

	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Character)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_PLAYER"), TEXT("No local AFGCharacterPlayer (player index 0)")));
		return;
	}

	AFGBuildable* SourceBuildable = FindBuildableById(World, SourceBuildableId);
	if (!SourceBuildable)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No buildable found with id '%s'"), *SourceBuildableId)));
		return;
	}
	AFGBuildable* DestBuildable = FindBuildableById(World, DestBuildableId);
	if (!DestBuildable)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No buildable found with id '%s'"), *DestBuildableId)));
		return;
	}

	UFGFactoryConnectionComponent* SourceConnection = FindFreeFactoryConnection(SourceBuildable, EFactoryConnectionDirection::FCD_OUTPUT);
	if (!SourceConnection)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_FACTORY_CONNECTION"), FString::Printf(TEXT("'%s' has no free Output factory connection component"), *SourceBuildableId)));
		return;
	}
	UFGFactoryConnectionComponent* DestConnection = FindFreeFactoryConnection(DestBuildable, EFactoryConnectionDirection::FCD_INPUT);
	if (!DestConnection)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_FACTORY_CONNECTION"), FString::Printf(TEXT("'%s' has no free Input factory connection component"), *DestBuildableId)));
		return;
	}

	UClass* LiftRecipeClass = LoadObject<UClass>(nullptr, *RecipeClassPath);
	if (!LiftRecipeClass || !LiftRecipeClass->IsChildOf(UFGRecipe::StaticClass()))
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("INVALID_RECIPE"), FString::Printf(TEXT("'%s' did not resolve to a UFGRecipe subclass"), *RecipeClassPath)));
		return;
	}
	const TSubclassOf<UFGRecipe> RecipeClass = LiftRecipeClass;

	Character->HotKeyRecipe(RecipeClass);

	AFGBuildGun* BuildGun = Character->GetBuildGun();
	if (!BuildGun)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_BUILD_GUN"), TEXT("AFGCharacterPlayer::GetBuildGun() returned null")));
		return;
	}

	UFGBuildGunStateBuild* BuildState = Cast<UFGBuildGunStateBuild>(BuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD));
	if (!BuildState)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_BUILD_STATE"), TEXT("Could not resolve UFGBuildGunStateBuild from the build gun")));
		return;
	}

	AFGHologram* Hologram = BuildState->GetHologram();
	AFGConveyorLiftHologram* LiftHologram = Cast<AFGConveyorLiftHologram>(Hologram);
	if (!LiftHologram)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("HOLOGRAM_SPAWN_FAILED"),
			FString::Printf(TEXT("HotKeyRecipe(%s) did not result in an AFGConveyorLiftHologram (got %s)"),
				*RecipeClassPath, Hologram ? *Hologram->GetClass()->GetName() : TEXT("null"))));
		return;
	}

	auto MakeHitAt = [](AFGBuildable* Buildable, UFGFactoryConnectionComponent* Connection) -> FHitResult
	{
		FHitResult Hit;
		Hit.Location = Connection->GetConnectorLocation();
		Hit.ImpactPoint = Hit.Location;
		Hit.Normal = Connection->GetConnectorNormal();
		Hit.ImpactNormal = Hit.Normal;
		Hit.HitObjectHandle = FActorInstanceHandle(Buildable);
		Hit.bBlockingHit = true;
		return Hit;
	};

	auto SummarizeDisqualifiers = [](AFGConveyorLiftHologram* H) -> FString
	{
		TArray<TSubclassOf<UFGConstructDisqualifier>> Disqualifiers;
		H->GetConstructDisqualifiers(Disqualifiers);
		TArray<FString> Texts;
		for (const TSubclassOf<UFGConstructDisqualifier>& D : Disqualifiers)
		{
			Texts.Add(FString::Printf(TEXT("%s (%s)"), *UFGConstructDisqualifier::GetDisqualifyingText(D).ToString(),
				UFGConstructDisqualifier::GetIsSoftDisqualifier(D) ? TEXT("soft") : TEXT("hard")));
		}
		return Texts.IsEmpty() ? TEXT("<none>") : FString::Join(Texts, TEXT("; "));
	};

	UE_LOG(LogAIModAI, Display, TEXT("ConstructConveyorLift diagnostic: sourceConnectorLoc=%s sourceConnectorNormal=%s destConnectorLoc=%s destConnectorNormal=%s"),
		*SourceConnection->GetConnectorLocation().ToString(), *SourceConnection->GetConnectorNormal().ToString(),
		*DestConnection->GetConnectorLocation().ToString(), *DestConnection->GetConnectorNormal().ToString());

	// Player-independence, take 2 (2026-08-27): see ConstructConveyorBelt's
	// identical comment/incident - the disqualifier-ignore alone was not
	// sufficient, since the belt/lift's internal pathing separately reads
	// the player controller's live rotation. Point the controller at a
	// deterministic target computed from the two connectors (never the
	// player's real aim), reasserted every poll tick below.
	const FRotator LiftDeterministicLook = (DestConnection->GetConnectorLocation() - SourceConnection->GetConnectorLocation()).Rotation();
	if (AController* LiftController = Character->GetController())
	{
		LiftController->SetControlRotation(LiftDeterministicLook);
	}

	const FHitResult StartHit = MakeHitAt(SourceBuildable, SourceConnection);
	LiftHologram->UpdateHologramPlacement(StartHit);
	LiftHologram->TrySnapToActor(StartHit);
	const bool bStartStepComplete = LiftHologram->DoMultiStepPlacement(true);

	UE_LOG(LogAIModAI, Display, TEXT("ConstructConveyorLift: source=%s dest=%s after start click: stepComplete=%s disqualifiers=[%s]"),
		*SourceBuildableId, *DestBuildableId, bStartStepComplete ? TEXT("true") : TEXT("false"), *SummarizeDisqualifiers(LiftHologram));

	if (bStartStepComplete)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("UNEXPECTED_STEP_COMPLETE"), TEXT("DoMultiStepPlacement() reported complete after only the start click")));
		return;
	}

	const FHitResult EndHit = MakeHitAt(DestBuildable, DestConnection);
	LiftHologram->UpdateHologramPlacement(EndHit);
	LiftHologram->TrySnapToActor(EndHit);
	const bool bEndStepComplete = LiftHologram->DoMultiStepPlacement(true);

	UE_LOG(LogAIModAI, Display, TEXT("ConstructConveyorLift: source=%s dest=%s after end click: stepComplete=%s disqualifiers=[%s]"),
		*SourceBuildableId, *DestBuildableId, bEndStepComplete ? TEXT("true") : TEXT("false"), *SummarizeDisqualifiers(LiftHologram));

	if (!bEndStepComplete)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("PLACEMENT_INCOMPLETE"), TEXT("DoMultiStepPlacement() did not report complete after the end click")));
		return;
	}

	struct FPollState
	{
		TWeakObjectPtr<AFGConveyorLiftHologram> Hologram;
		TWeakObjectPtr<AFGCharacterPlayer> Character;
		TWeakObjectPtr<UWorld> World;
		FString SourceBuildableId;
		FString DestBuildableId;
		bool bDryRun = true;
		FRotator DeterministicLook;
		int32 AttemptsRemaining = 120; // safety cap - real ticks, not a fixed duration
		int32 AttemptsTaken = 0;
		TFunction<void(const FAIModOperationResult&)> OnComplete;
	};
	const TSharedRef<FPollState> PollState = MakeShared<FPollState>();
	PollState->Hologram = LiftHologram;
	PollState->Character = Character;
	PollState->World = World;
	PollState->SourceBuildableId = SourceBuildableId;
	PollState->DestBuildableId = DestBuildableId;
	PollState->bDryRun = bDryRun;
	PollState->DeterministicLook = LiftDeterministicLook;
	PollState->OnComplete = MoveTemp(OnComplete);

	const TSharedRef<TFunction<void()>> PollFn = MakeShared<TFunction<void()>>();
	*PollFn = [PollState, PollFn]()
	{
		++PollState->AttemptsTaken;

		AFGConveyorLiftHologram* PollHologram = PollState->Hologram.Get();
		UWorld* PollWorld = PollState->World.Get();
		AFGCharacterPlayer* PollCharacter = PollState->Character.Get();
		if (!IsValid(PollHologram) || !PollWorld)
		{
			UE_LOG(LogAIModAI, Warning, TEXT("ConstructConveyorLift (deferred): hologram or world became invalid while polling (after %d tick(s))"), PollState->AttemptsTaken);
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("HOLOGRAM_INVALIDATED"), TEXT("Hologram or world became invalid while polling")));
			return;
		}

		// Re-assert every tick - see ConstructConveyorBelt's identical block
		// for the full rationale (player camera movement between ticks can
		// still drag the resolved path off a one-time value).
		if (IsValid(PollCharacter))
		{
			if (AController* PollController = PollCharacter->GetController())
			{
				PollController->SetControlRotation(PollState->DeterministicLook);
			}
		}

		TArray<TSubclassOf<UFGConstructDisqualifier>> Disqualifiers;
		PollHologram->GetConstructDisqualifiers(Disqualifiers);
		const bool bStillInitializing = Disqualifiers.Contains(TSubclassOf<UFGConstructDisqualifier>(UFGCDInitializing::StaticClass()));

		--PollState->AttemptsRemaining;
		if (bStillInitializing && PollState->AttemptsRemaining > 0)
		{
			PollWorld->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
			return;
		}

		// Player-independence fix - same rationale as ConstructConveyorBelt's
		// identical block above.
		// UnlimitedResources (2026-08-27) - see ConstructBuildingAtPosition's
		// comment on this being a player-controlled mod setting, not a
		// per-call flag.
		const bool bUnlimitedResources = UAIModFunctionLibrary::GetAIModConfigBool(PollWorld, TEXT("UnlimitedResources"), false);

		bool bCanConstruct = true;
		TArray<FString> DisqualifierTexts;
		for (const TSubclassOf<UFGConstructDisqualifier>& DisqualifierClass : Disqualifiers)
		{
			const bool bIgnoredForPlayerIndependence = (DisqualifierClass == UFGCDInvalidAimLocation::StaticClass())
				|| (bUnlimitedResources && DisqualifierClass == UFGCDUnaffordable::StaticClass());
			const bool bIsSoft = UFGConstructDisqualifier::GetIsSoftDisqualifier(DisqualifierClass);
			if (!bIgnoredForPlayerIndependence && !bIsSoft)
			{
				bCanConstruct = false;
			}
			DisqualifierTexts.Add(FString::Printf(TEXT("%s (%s%s)"),
				*UFGConstructDisqualifier::GetDisqualifyingText(DisqualifierClass).ToString(),
				bIsSoft ? TEXT("soft") : TEXT("hard"), bIgnoredForPlayerIndependence ? TEXT(", ignored") : TEXT("")));
		}
		const FString DisqualifierSummary = DisqualifierTexts.IsEmpty() ? TEXT("<none>") : FString::Join(DisqualifierTexts, TEXT("; "));

		UE_LOG(LogAIModAI, Display, TEXT("ConstructConveyorLift (deferred, resolved after %d real tick(s)): source=%s dest=%s dryRun=%s canConstruct=%s disqualifiers=[%s]"),
			PollState->AttemptsTaken, *PollState->SourceBuildableId, *PollState->DestBuildableId, PollState->bDryRun ? TEXT("true") : TEXT("false"),
			bCanConstruct ? TEXT("true") : TEXT("false"), *DisqualifierSummary);

		if (!bCanConstruct)
		{
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("CANNOT_CONSTRUCT"), DisqualifierSummary));
			return;
		}

		if (PollState->bDryRun)
		{
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Success());
			return;
		}

		AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(PollWorld);
		const FNetConstructionID ConstructionID = BuildableSubsystem ? BuildableSubsystem->GetNewNetConstructionID() : FNetConstructionID();

		AFGBuildGun* PollBuildGun = IsValid(PollCharacter) ? PollCharacter->GetBuildGun() : nullptr;
		UFGBuildGunStateBuild* PollBuildState = PollBuildGun ? Cast<UFGBuildGunStateBuild>(PollBuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD)) : nullptr;
		if (!PollBuildState)
		{
			UE_LOG(LogAIModAI, Error, TEXT("ConstructConveyorLift (deferred): lost the build state before constructing - aborting, nothing built"));
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("Lost the build state before constructing")));
			return;
		}

		PollBuildState->InternalConstructHologram(ConstructionID);

		UE_LOG(LogAIModAI, Display, TEXT("ConstructConveyorLift (deferred, resolved after %d real tick(s)): construction attempted via InternalConstructHologram - source=%s dest=%s"),
			PollState->AttemptsTaken, *PollState->SourceBuildableId, *PollState->DestBuildableId);

		if (IsValid(PollCharacter))
		{
			PollCharacter->UnequipBuildGun();
		}

		PollState->OnComplete(FAIModOperationResult::Success());
	};

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
}

// ConstructPipe (2026-08-25 pipe groundwork) - deliberate near-exact
// mirror of ConstructConveyorBelt above, same mechanism, different
// types: AFGPipelineHologram is a sibling of AFGConveyorBeltHologram
// (both derive directly from AFGSplineHologram - confirmed from
// source), and UFGPipeConnectionComponentBase/EPipeConnectionType is
// pipes' own parallel connection-type hierarchy, NOT
// UFGFactoryConnectionComponent/EFactoryConnectionDirection. Applies
// every fix discovered live for belts up front rather than
// rediscovering them - UpdateHologramPlacement() before
// TrySnapToActor() at each click, and the connector's REAL
// GetConnectorNormal() (not a placeholder UpVector) in the synthetic
// hit. NOT YET LIVE-TESTED - unlike ConstructConveyorBelt, none of
// this has been run against a real game session. Two known
// pipe-specific unknowns going in: (1) AFGSplineHologram (the shared
// base) has no GetAnyConnectedBuildables() - only
// AFGConveyorBeltHologram declares that method - so this uses
// IsConnectionSnapped(false) instead for the post-end-click diagnostic,
// an indicator already noted (see DebugCheckConveyorSnap's findings)
// as not fully reliable even for belts; (2) fluid type compatibility
// (UFGCDPipeFluidTypeMismatch, confirmed to exist in
// FGConstructDisqualifier.h) is NOT pre-validated here - the real
// CanConstruct() disqualifier check is trusted to catch it, same as
// every other disqualifier this function doesn't special-case.
void UAIModFunctionLibrary::ConstructPipe(UObject* WorldContextObject, const FString& SourceBuildableId, const FString& DestBuildableId, const FString& RecipeClassPath, bool bDryRun, TFunction<void(const FAIModOperationResult&)> OnComplete)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context")));
		return;
	}

	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Character)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_PLAYER"), TEXT("No local AFGCharacterPlayer (player index 0)")));
		return;
	}

	AFGBuildable* SourceBuildable = FindBuildableById(World, SourceBuildableId);
	if (!SourceBuildable)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No buildable found with id '%s'"), *SourceBuildableId)));
		return;
	}
	AFGBuildable* DestBuildable = FindBuildableById(World, DestBuildableId);
	if (!DestBuildable)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No buildable found with id '%s'"), *DestBuildableId)));
		return;
	}

	UFGPipeConnectionComponentBase* SourceConnection = FindFreeFluidPipeConnection(SourceBuildable, EPipeConnectionType::PCT_PRODUCER);
	if (!SourceConnection)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_PIPE_CONNECTION"), FString::Printf(TEXT("'%s' has no free Producer or Any pipe connection component"), *SourceBuildableId)));
		return;
	}
	UFGPipeConnectionComponentBase* DestConnection = FindFreeFluidPipeConnection(DestBuildable, EPipeConnectionType::PCT_CONSUMER);
	if (!DestConnection)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_PIPE_CONNECTION"), FString::Printf(TEXT("'%s' has no free Consumer or Any pipe connection component"), *DestBuildableId)));
		return;
	}

	// Caller-chosen pipe tier - see LogPipelineTiersAsJson for the two
	// real recipes (Recipe_Pipeline, Recipe_PipelineMK2). Same
	// validation posture as every other recipe param in this file.
	UClass* PipeRecipeClass = LoadObject<UClass>(nullptr, *RecipeClassPath);
	if (!PipeRecipeClass || !PipeRecipeClass->IsChildOf(UFGRecipe::StaticClass()))
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("INVALID_RECIPE"), FString::Printf(TEXT("'%s' did not resolve to a UFGRecipe subclass"), *RecipeClassPath)));
		return;
	}
	const TSubclassOf<UFGRecipe> RecipeClass = PipeRecipeClass;

	Character->HotKeyRecipe(RecipeClass);

	AFGBuildGun* BuildGun = Character->GetBuildGun();
	if (!BuildGun)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_BUILD_GUN"), TEXT("AFGCharacterPlayer::GetBuildGun() returned null")));
		return;
	}

	UFGBuildGunStateBuild* BuildState = Cast<UFGBuildGunStateBuild>(BuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD));
	if (!BuildState)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_BUILD_STATE"), TEXT("Could not resolve UFGBuildGunStateBuild from the build gun")));
		return;
	}

	AFGHologram* Hologram = BuildState->GetHologram();
	AFGPipelineHologram* PipeHologram = Cast<AFGPipelineHologram>(Hologram);
	if (!PipeHologram)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("HOLOGRAM_SPAWN_FAILED"),
			FString::Printf(TEXT("HotKeyRecipe(%s) did not result in an AFGPipelineHologram (got %s)"),
				*RecipeClassPath, Hologram ? *Hologram->GetClass()->GetName() : TEXT("null"))));
		return;
	}

	auto MakeHitAt = [](AFGBuildable* Buildable, UFGPipeConnectionComponentBase* Connection) -> FHitResult
	{
		FHitResult Hit;
		Hit.Location = Connection->GetConnectorLocation();
		Hit.ImpactPoint = Hit.Location;
		Hit.Normal = Connection->GetConnectorNormal();
		Hit.ImpactNormal = Hit.Normal;
		Hit.HitObjectHandle = FActorInstanceHandle(Buildable);
		Hit.bBlockingHit = true;
		return Hit;
	};

	auto SummarizeDisqualifiers = [](AFGPipelineHologram* H) -> FString
	{
		TArray<TSubclassOf<UFGConstructDisqualifier>> Disqualifiers;
		H->GetConstructDisqualifiers(Disqualifiers);
		TArray<FString> Texts;
		for (const TSubclassOf<UFGConstructDisqualifier>& D : Disqualifiers)
		{
			Texts.Add(FString::Printf(TEXT("%s (%s)"), *UFGConstructDisqualifier::GetDisqualifyingText(D).ToString(),
				UFGConstructDisqualifier::GetIsSoftDisqualifier(D) ? TEXT("soft") : TEXT("hard")));
		}
		return Texts.IsEmpty() ? TEXT("<none>") : FString::Join(Texts, TEXT("; "));
	};
	UE_LOG(LogAIModAI, Display, TEXT("ConstructPipe diagnostic: playerLoc=%s playerRot=%s sourceConnectorLoc=%s sourceConnectorNormal=%s sourceConnectorClearanceLoc=%s destConnectorLoc=%s destConnectorNormal=%s destConnectorClearanceLoc=%s"),
		*Character->GetActorLocation().ToString(), *Character->GetActorRotation().ToString(),
		*SourceConnection->GetConnectorLocation().ToString(), *SourceConnection->GetConnectorNormal().ToString(), *SourceConnection->GetConnectorLocation(true).ToString(),
		*DestConnection->GetConnectorLocation().ToString(), *DestConnection->GetConnectorNormal().ToString(), *DestConnection->GetConnectorLocation(true).ToString());

	// Player-independence (2026-08-27, applying the same fix already
	// proven for ConstructConveyorBelt/ConstructConveyorLift - see their
	// comments for the full incident): this function predates that fix
	// and was never updated - live-confirmed this session it fails with
	// "Invalid aim location!" the same way belts used to, even for a
	// completely valid connector pair with a real Distance apart. Point
	// the controller at a deterministic target computed from the two
	// connectors themselves (never the player's real aim), reasserted
	// every poll tick below.
	const FRotator PipeDeterministicLook = (DestConnection->GetConnectorLocation() - SourceConnection->GetConnectorLocation()).Rotation();
	if (AController* PipeController = Character->GetController())
	{
		PipeController->SetControlRotation(PipeDeterministicLook);
	}

	const FHitResult StartHit = MakeHitAt(SourceBuildable, SourceConnection);
	PipeHologram->UpdateHologramPlacement(StartHit);
	PipeHologram->TrySnapToActor(StartHit);
	const bool bStartStepComplete = PipeHologram->DoMultiStepPlacement(true);
	const ESplineHologramBuildStep StepAfterStart = PipeHologram->GetCurrentBuildStep();

	UE_LOG(LogAIModAI, Display, TEXT("ConstructPipe: source=%s dest=%s after start click: stepComplete=%s step=%d disqualifiers=[%s]"),
		*SourceBuildableId, *DestBuildableId, bStartStepComplete ? TEXT("true") : TEXT("false"), static_cast<int32>(StepAfterStart), *SummarizeDisqualifiers(PipeHologram));

	if (bStartStepComplete)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("UNEXPECTED_STEP_COMPLETE"), TEXT("DoMultiStepPlacement() reported complete after only the start click")));
		return;
	}

	const FHitResult EndHit = MakeHitAt(DestBuildable, DestConnection);
	PipeHologram->UpdateHologramPlacement(EndHit);
	PipeHologram->TrySnapToActor(EndHit);
	const bool bEndStepComplete = PipeHologram->DoMultiStepPlacement(true);
	const ESplineHologramBuildStep StepAfterEnd = PipeHologram->GetCurrentBuildStep();
	const bool bEndConnectionSnapped = PipeHologram->IsConnectionSnapped(false);

	UE_LOG(LogAIModAI, Display, TEXT("ConstructPipe: source=%s dest=%s after end click: stepComplete=%s step=%d connectionSnapped=%s disqualifiers=[%s]"),
		*SourceBuildableId, *DestBuildableId, bEndStepComplete ? TEXT("true") : TEXT("false"), static_cast<int32>(StepAfterEnd), bEndConnectionSnapped ? TEXT("true") : TEXT("false"), *SummarizeDisqualifiers(PipeHologram));

	if (!bEndStepComplete)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("PLACEMENT_INCOMPLETE"),
			FString::Printf(TEXT("DoMultiStepPlacement() did not report complete after the end click - step=%d connectionSnapped=%s, may need a third step"), static_cast<int32>(StepAfterEnd), bEndConnectionSnapped ? TEXT("true") : TEXT("false"))));
		return;
	}

	struct FPollState
	{
		TWeakObjectPtr<AFGPipelineHologram> Hologram;
		TWeakObjectPtr<AFGCharacterPlayer> Character;
		TWeakObjectPtr<UWorld> World;
		FString SourceBuildableId;
		FString DestBuildableId;
		bool bDryRun = true;
		FRotator DeterministicLook;
		int32 AttemptsRemaining = 120; // safety cap - real ticks, not a fixed duration
		int32 AttemptsTaken = 0;
		TFunction<void(const FAIModOperationResult&)> OnComplete;
	};
	const TSharedRef<FPollState> PollState = MakeShared<FPollState>();
	PollState->Hologram = PipeHologram;
	PollState->Character = Character;
	PollState->World = World;
	PollState->SourceBuildableId = SourceBuildableId;
	PollState->DestBuildableId = DestBuildableId;
	PollState->bDryRun = bDryRun;
	PollState->DeterministicLook = PipeDeterministicLook;
	PollState->OnComplete = MoveTemp(OnComplete);

	const TSharedRef<TFunction<void()>> PollFn = MakeShared<TFunction<void()>>();
	*PollFn = [PollState, PollFn]()
	{
		++PollState->AttemptsTaken;

		AFGPipelineHologram* PollHologram = PollState->Hologram.Get();
		UWorld* PollWorld = PollState->World.Get();
		AFGCharacterPlayer* PollCharacter = PollState->Character.Get();
		if (!IsValid(PollHologram) || !PollWorld)
		{
			UE_LOG(LogAIModAI, Warning, TEXT("ConstructPipe (deferred): hologram or world became invalid while polling (after %d tick(s))"), PollState->AttemptsTaken);
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("HOLOGRAM_INVALIDATED"), TEXT("Hologram or world became invalid while polling")));
			return;
		}

		// Re-assert every tick - see ConstructConveyorBelt's identical block
		// for the full rationale.
		if (IsValid(PollCharacter))
		{
			if (AController* PollController = PollCharacter->GetController())
			{
				PollController->SetControlRotation(PollState->DeterministicLook);
			}
		}

		TArray<TSubclassOf<UFGConstructDisqualifier>> Disqualifiers;
		PollHologram->GetConstructDisqualifiers(Disqualifiers);
		const bool bStillInitializing = Disqualifiers.Contains(TSubclassOf<UFGConstructDisqualifier>(UFGCDInitializing::StaticClass()));

		--PollState->AttemptsRemaining;
		if (bStillInitializing && PollState->AttemptsRemaining > 0)
		{
			PollWorld->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
			return;
		}

		// Player-independence (2026-08-27) - same manual disqualifier-ignore
		// pattern as ConstructConveyorBelt, replacing the real (opaque)
		// CanConstruct() this function used to call directly.
		// UnlimitedResources (2026-08-27) - see ConstructBuildingAtPosition's
		// comment on this being a player-controlled mod setting, not a
		// per-call flag.
		const bool bUnlimitedResources = UAIModFunctionLibrary::GetAIModConfigBool(PollWorld, TEXT("UnlimitedResources"), false);

		bool bCanConstruct = true;
		TArray<FString> DisqualifierTexts;
		for (const TSubclassOf<UFGConstructDisqualifier>& DisqualifierClass : Disqualifiers)
		{
			const bool bIgnoredForPlayerIndependence = (DisqualifierClass == UFGCDInvalidAimLocation::StaticClass())
				|| (bUnlimitedResources && DisqualifierClass == UFGCDUnaffordable::StaticClass());
			const bool bIsSoft = UFGConstructDisqualifier::GetIsSoftDisqualifier(DisqualifierClass);
			if (!bIgnoredForPlayerIndependence && !bIsSoft)
			{
				bCanConstruct = false;
			}
			DisqualifierTexts.Add(FString::Printf(TEXT("%s (%s%s)"),
				*UFGConstructDisqualifier::GetDisqualifyingText(DisqualifierClass).ToString(),
				bIsSoft ? TEXT("soft") : TEXT("hard"), bIgnoredForPlayerIndependence ? TEXT(", ignored") : TEXT("")));
		}
		const FString DisqualifierSummary = DisqualifierTexts.IsEmpty() ? TEXT("<none>") : FString::Join(DisqualifierTexts, TEXT("; "));

		UE_LOG(LogAIModAI, Display, TEXT("ConstructPipe (deferred, resolved after %d real tick(s)): source=%s dest=%s dryRun=%s canConstruct=%s disqualifiers=[%s]"),
			PollState->AttemptsTaken, *PollState->SourceBuildableId, *PollState->DestBuildableId, PollState->bDryRun ? TEXT("true") : TEXT("false"),
			bCanConstruct ? TEXT("true") : TEXT("false"), *DisqualifierSummary);

		if (!bCanConstruct)
		{
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("CANNOT_CONSTRUCT"), DisqualifierSummary));
			return;
		}

		if (PollState->bDryRun)
		{
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Success());
			return;
		}

		AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(PollWorld);
		const FNetConstructionID ConstructionID = BuildableSubsystem ? BuildableSubsystem->GetNewNetConstructionID() : FNetConstructionID();

		AFGBuildGun* PollBuildGun = IsValid(PollCharacter) ? PollCharacter->GetBuildGun() : nullptr;
		UFGBuildGunStateBuild* PollBuildState = PollBuildGun ? Cast<UFGBuildGunStateBuild>(PollBuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD)) : nullptr;
		if (!PollBuildState)
		{
			UE_LOG(LogAIModAI, Error, TEXT("ConstructPipe (deferred): lost the build state before constructing - aborting, nothing built"));
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("Lost the build state before constructing")));
			return;
		}

		PollBuildState->InternalConstructHologram(ConstructionID);

		UE_LOG(LogAIModAI, Display, TEXT("ConstructPipe (deferred, resolved after %d real tick(s)): construction attempted via InternalConstructHologram - source=%s dest=%s"),
			PollState->AttemptsTaken, *PollState->SourceBuildableId, *PollState->DestBuildableId);

		if (IsValid(PollCharacter))
		{
			PollCharacter->UnequipBuildGun();
		}

		PollState->OnComplete(FAIModOperationResult::Success());
	};

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
}

// ConstructHypertube (2026-08-27, per explicit user request to test/add
// hypertube support alongside longer pipe runs). Research finding (full
// detail in docs/hypertube-research.md): despite the separate-looking
// "Recipe_HyperTube*" family in the catalog (Junction/TJunction/
// WallSupport/WallHole - those are ATTACHMENTS, not the tube), the actual
// connecting tube is `Recipe_PipeHyper` -> AFGBuildablePipeHyper, and its
// hologram (`Holo_PipeHyper_C`) is a Blueprint child of the SAME
// AFGPipelineHologram class ConstructPipe already drives - confirmed from
// the hologram BP's own uasset name table. This is deliberately a
// near-mirror of ConstructPipe (same two-click TrySnapToActor +
// DoMultiStepPlacement flow, same deferred poll/disqualifier-ignore/
// deterministic-look pattern established this session), differing only
// in: (1) hardcoded to Recipe_PipeHyper - no tiers exist, unlike
// Recipe_Pipeline/PipelineMK2, so no recipeClass param; (2) connector
// lookup via FindFreeHyperPipeConnection instead of FindFreePipeConnection,
// since hypertube connectors are UFGPipeConnectionComponentHyper at
// PCT_ANY, not PCT_PRODUCER/PCT_CONSUMER - the exact-type match that
// works for fluid pipe machines finds nothing on any hypertube part; (3)
// no real producer/consumer distinction - hypertubes are bidirectional,
// so "source"/"dest" here are just which buildable's free connector each
// end can find, not a meaningful flow direction.
void UAIModFunctionLibrary::ConstructHypertube(UObject* WorldContextObject, const FString& SourceBuildableId, const FString& DestBuildableId, bool bDryRun, TFunction<void(const FAIModOperationResult&)> OnComplete)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context")));
		return;
	}

	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Character)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_PLAYER"), TEXT("No local AFGCharacterPlayer (player index 0)")));
		return;
	}

	AFGBuildable* SourceBuildable = FindBuildableById(World, SourceBuildableId);
	if (!SourceBuildable)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No buildable found with id '%s'"), *SourceBuildableId)));
		return;
	}
	AFGBuildable* DestBuildable = FindBuildableById(World, DestBuildableId);
	if (!DestBuildable)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No buildable found with id '%s'"), *DestBuildableId)));
		return;
	}

	UFGPipeConnectionComponentBase* SourceConnection = FindFreeHyperPipeConnection(SourceBuildable);
	if (!SourceConnection)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_PIPE_CONNECTION"), FString::Printf(TEXT("'%s' has no free hypertube connection component"), *SourceBuildableId)));
		return;
	}
	UFGPipeConnectionComponentBase* DestConnection = FindFreeHyperPipeConnection(DestBuildable);
	if (!DestConnection)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_PIPE_CONNECTION"), FString::Printf(TEXT("'%s' has no free hypertube connection component"), *DestBuildableId)));
		return;
	}

	UClass* HyperTubeRecipeClass = LoadObject<UClass>(nullptr, TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PipeHyper.Recipe_PipeHyper_C"));
	if (!HyperTubeRecipeClass || !HyperTubeRecipeClass->IsChildOf(UFGRecipe::StaticClass()))
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("RECIPE_LOAD_FAILED"), TEXT("Failed to load Recipe_PipeHyper as a UFGRecipe")));
		return;
	}
	const TSubclassOf<UFGRecipe> RecipeClass = HyperTubeRecipeClass;

	Character->HotKeyRecipe(RecipeClass);

	AFGBuildGun* BuildGun = Character->GetBuildGun();
	if (!BuildGun)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_BUILD_GUN"), TEXT("AFGCharacterPlayer::GetBuildGun() returned null")));
		return;
	}

	UFGBuildGunStateBuild* BuildState = Cast<UFGBuildGunStateBuild>(BuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD));
	if (!BuildState)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_BUILD_STATE"), TEXT("Could not resolve UFGBuildGunStateBuild from the build gun")));
		return;
	}

	AFGHologram* Hologram = BuildState->GetHologram();
	AFGPipelineHologram* HyperHologram = Cast<AFGPipelineHologram>(Hologram);
	if (!HyperHologram)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("HOLOGRAM_SPAWN_FAILED"),
			FString::Printf(TEXT("HotKeyRecipe(Recipe_PipeHyper) did not result in an AFGPipelineHologram (got %s)"),
				Hologram ? *Hologram->GetClass()->GetName() : TEXT("null"))));
		return;
	}

	auto MakeHitAt = [](AFGBuildable* Buildable, UFGPipeConnectionComponentBase* Connection) -> FHitResult
	{
		FHitResult Hit;
		Hit.Location = Connection->GetConnectorLocation();
		Hit.ImpactPoint = Hit.Location;
		Hit.Normal = Connection->GetConnectorNormal();
		Hit.ImpactNormal = Hit.Normal;
		Hit.HitObjectHandle = FActorInstanceHandle(Buildable);
		Hit.bBlockingHit = true;
		return Hit;
	};

	auto SummarizeDisqualifiers = [](AFGPipelineHologram* H) -> FString
	{
		TArray<TSubclassOf<UFGConstructDisqualifier>> Disqualifiers;
		H->GetConstructDisqualifiers(Disqualifiers);
		TArray<FString> Texts;
		for (const TSubclassOf<UFGConstructDisqualifier>& D : Disqualifiers)
		{
			Texts.Add(FString::Printf(TEXT("%s (%s)"), *UFGConstructDisqualifier::GetDisqualifyingText(D).ToString(),
				UFGConstructDisqualifier::GetIsSoftDisqualifier(D) ? TEXT("soft") : TEXT("hard")));
		}
		return Texts.IsEmpty() ? TEXT("<none>") : FString::Join(Texts, TEXT("; "));
	};
	UE_LOG(LogAIModAI, Display, TEXT("ConstructHypertube diagnostic: sourceConnectorLoc=%s sourceConnectorNormal=%s destConnectorLoc=%s destConnectorNormal=%s"),
		*SourceConnection->GetConnectorLocation().ToString(), *SourceConnection->GetConnectorNormal().ToString(),
		*DestConnection->GetConnectorLocation().ToString(), *DestConnection->GetConnectorNormal().ToString());

	// Player-independence, applied from day one here (not retrofitted like
	// ConstructPipe/ConstructConveyorBelt above) - see those functions'
	// comments for the full incident this pattern fixes.
	const FRotator HyperDeterministicLook = (DestConnection->GetConnectorLocation() - SourceConnection->GetConnectorLocation()).Rotation();
	if (AController* HyperController = Character->GetController())
	{
		HyperController->SetControlRotation(HyperDeterministicLook);
	}

	const FHitResult StartHit = MakeHitAt(SourceBuildable, SourceConnection);
	HyperHologram->UpdateHologramPlacement(StartHit);
	HyperHologram->TrySnapToActor(StartHit);
	const bool bStartStepComplete = HyperHologram->DoMultiStepPlacement(true);
	const ESplineHologramBuildStep StepAfterStart = HyperHologram->GetCurrentBuildStep();

	UE_LOG(LogAIModAI, Display, TEXT("ConstructHypertube: source=%s dest=%s after start click: stepComplete=%s step=%d disqualifiers=[%s]"),
		*SourceBuildableId, *DestBuildableId, bStartStepComplete ? TEXT("true") : TEXT("false"), static_cast<int32>(StepAfterStart), *SummarizeDisqualifiers(HyperHologram));

	if (bStartStepComplete)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("UNEXPECTED_STEP_COMPLETE"), TEXT("DoMultiStepPlacement() reported complete after only the start click")));
		return;
	}

	const FHitResult EndHit = MakeHitAt(DestBuildable, DestConnection);
	HyperHologram->UpdateHologramPlacement(EndHit);
	HyperHologram->TrySnapToActor(EndHit);
	const bool bEndStepComplete = HyperHologram->DoMultiStepPlacement(true);
	const ESplineHologramBuildStep StepAfterEnd = HyperHologram->GetCurrentBuildStep();
	const bool bEndConnectionSnapped = HyperHologram->IsConnectionSnapped(false);

	UE_LOG(LogAIModAI, Display, TEXT("ConstructHypertube: source=%s dest=%s after end click: stepComplete=%s step=%d connectionSnapped=%s disqualifiers=[%s]"),
		*SourceBuildableId, *DestBuildableId, bEndStepComplete ? TEXT("true") : TEXT("false"), static_cast<int32>(StepAfterEnd), bEndConnectionSnapped ? TEXT("true") : TEXT("false"), *SummarizeDisqualifiers(HyperHologram));

	if (!bEndStepComplete)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("PLACEMENT_INCOMPLETE"),
			FString::Printf(TEXT("DoMultiStepPlacement() did not report complete after the end click - step=%d connectionSnapped=%s, may need a third step"), static_cast<int32>(StepAfterEnd), bEndConnectionSnapped ? TEXT("true") : TEXT("false"))));
		return;
	}

	struct FPollState
	{
		TWeakObjectPtr<AFGPipelineHologram> Hologram;
		TWeakObjectPtr<AFGCharacterPlayer> Character;
		TWeakObjectPtr<UWorld> World;
		FString SourceBuildableId;
		FString DestBuildableId;
		bool bDryRun = true;
		FRotator DeterministicLook;
		int32 AttemptsRemaining = 120; // safety cap - real ticks, not a fixed duration
		int32 AttemptsTaken = 0;
		TFunction<void(const FAIModOperationResult&)> OnComplete;
	};
	const TSharedRef<FPollState> PollState = MakeShared<FPollState>();
	PollState->Hologram = HyperHologram;
	PollState->Character = Character;
	PollState->World = World;
	PollState->SourceBuildableId = SourceBuildableId;
	PollState->DestBuildableId = DestBuildableId;
	PollState->bDryRun = bDryRun;
	PollState->DeterministicLook = HyperDeterministicLook;
	PollState->OnComplete = MoveTemp(OnComplete);

	const TSharedRef<TFunction<void()>> PollFn = MakeShared<TFunction<void()>>();
	*PollFn = [PollState, PollFn]()
	{
		++PollState->AttemptsTaken;

		AFGPipelineHologram* PollHologram = PollState->Hologram.Get();
		UWorld* PollWorld = PollState->World.Get();
		AFGCharacterPlayer* PollCharacter = PollState->Character.Get();
		if (!IsValid(PollHologram) || !PollWorld)
		{
			UE_LOG(LogAIModAI, Warning, TEXT("ConstructHypertube (deferred): hologram or world became invalid while polling (after %d tick(s))"), PollState->AttemptsTaken);
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("HOLOGRAM_INVALIDATED"), TEXT("Hologram or world became invalid while polling")));
			return;
		}

		if (IsValid(PollCharacter))
		{
			if (AController* PollController = PollCharacter->GetController())
			{
				PollController->SetControlRotation(PollState->DeterministicLook);
			}
		}

		TArray<TSubclassOf<UFGConstructDisqualifier>> Disqualifiers;
		PollHologram->GetConstructDisqualifiers(Disqualifiers);
		const bool bStillInitializing = Disqualifiers.Contains(TSubclassOf<UFGConstructDisqualifier>(UFGCDInitializing::StaticClass()));

		--PollState->AttemptsRemaining;
		if (bStillInitializing && PollState->AttemptsRemaining > 0)
		{
			PollWorld->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
			return;
		}

		// UnlimitedResources (2026-08-27) - see ConstructBuildingAtPosition's
		// comment on this being a player-controlled mod setting, not a
		// per-call flag.
		const bool bUnlimitedResources = UAIModFunctionLibrary::GetAIModConfigBool(PollWorld, TEXT("UnlimitedResources"), false);

		bool bCanConstruct = true;
		TArray<FString> DisqualifierTexts;
		for (const TSubclassOf<UFGConstructDisqualifier>& DisqualifierClass : Disqualifiers)
		{
			const bool bIgnoredForPlayerIndependence = (DisqualifierClass == UFGCDInvalidAimLocation::StaticClass())
				|| (bUnlimitedResources && DisqualifierClass == UFGCDUnaffordable::StaticClass());
			const bool bIsSoft = UFGConstructDisqualifier::GetIsSoftDisqualifier(DisqualifierClass);
			if (!bIgnoredForPlayerIndependence && !bIsSoft)
			{
				bCanConstruct = false;
			}
			DisqualifierTexts.Add(FString::Printf(TEXT("%s (%s%s)"),
				*UFGConstructDisqualifier::GetDisqualifyingText(DisqualifierClass).ToString(),
				bIsSoft ? TEXT("soft") : TEXT("hard"), bIgnoredForPlayerIndependence ? TEXT(", ignored") : TEXT("")));
		}
		const FString DisqualifierSummary = DisqualifierTexts.IsEmpty() ? TEXT("<none>") : FString::Join(DisqualifierTexts, TEXT("; "));

		UE_LOG(LogAIModAI, Display, TEXT("ConstructHypertube (deferred, resolved after %d real tick(s)): source=%s dest=%s dryRun=%s canConstruct=%s disqualifiers=[%s]"),
			PollState->AttemptsTaken, *PollState->SourceBuildableId, *PollState->DestBuildableId, PollState->bDryRun ? TEXT("true") : TEXT("false"),
			bCanConstruct ? TEXT("true") : TEXT("false"), *DisqualifierSummary);

		if (!bCanConstruct)
		{
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("CANNOT_CONSTRUCT"), DisqualifierSummary));
			return;
		}

		if (PollState->bDryRun)
		{
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Success());
			return;
		}

		AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(PollWorld);
		const FNetConstructionID ConstructionID = BuildableSubsystem ? BuildableSubsystem->GetNewNetConstructionID() : FNetConstructionID();

		AFGBuildGun* PollBuildGun = IsValid(PollCharacter) ? PollCharacter->GetBuildGun() : nullptr;
		UFGBuildGunStateBuild* PollBuildState = PollBuildGun ? Cast<UFGBuildGunStateBuild>(PollBuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD)) : nullptr;
		if (!PollBuildState)
		{
			UE_LOG(LogAIModAI, Error, TEXT("ConstructHypertube (deferred): lost the build state before constructing - aborting, nothing built"));
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("Lost the build state before constructing")));
			return;
		}

		PollBuildState->InternalConstructHologram(ConstructionID);

		UE_LOG(LogAIModAI, Display, TEXT("ConstructHypertube (deferred, resolved after %d real tick(s)): construction attempted via InternalConstructHologram - source=%s dest=%s"),
			PollState->AttemptsTaken, *PollState->SourceBuildableId, *PollState->DestBuildableId);

		if (IsValid(PollCharacter))
		{
			PollCharacter->UnequipBuildGun();
		}

		PollState->OnComplete(FAIModOperationResult::Success());
	};

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
}
