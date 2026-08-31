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
#include "FGSplineBuildableInterface.h"
#include "AIController.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "Buildables/FGBuildableWire.h"
#include "Buildables/FGBuildablePipeline.h"
#include "FGFluidIntegrantInterface.h"
#include "Buildables/FGBuildableConveyorAttachment.h"
#include "Buildables/FGBuildableSplitterSmart.h"
#include "Buildables/FGBuildableConveyorLift.h"
#include "Hologram/FGConveyorLiftHologram.h"
#include "Hologram/FGHologramBuildModeDescriptor.h"
#include "Hologram/FGPipelineHologram.h"
#include "Buildables/FGBuildablePipelinePump.h"
#include "Buildables/FGBuildablePipeReservoir.h"
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
#include "Hologram/FGBuildableHologram.h"
#include "FGFactoryColoringTypes.h"
#include "Buildables/FGBuildableDroneStation.h"
#include "FGVehicle.h"
#include "Resources/FGVehicleDescriptor.h"
#include "Hologram/FGVehicleHologram.h"
#include "FGRailroadTrackConnectionComponent.h"
#include "Hologram/FGRailroadTrackHologram.h"
#include "Buildables/FGBuildableRailroadTrack.h"
#include "Hologram/FGVehiclePathSegmentHologram.h"
#include "WheeledVehicles/FGVehiclePathSegment.h"
#include "WheeledVehicles/FGVehiclePathNode.h"
#include "Buildables/FGBuildableSplineSnappedBase.h"
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
#include "FGSchematicManager.h"
#include "FGSchematic.h"
#include "FGAdminInterface.h"
#include "FGPlayerControllerBase.h"
#include "FGGameState.h"
#include "Buildables/FGBuildableSpaceElevator.h"
#include "FGResearchManager.h"
#include "FGResearchTree.h"
#include "FGResearchTreeNode.h"
#include "FGHardDrive.h"
#include "FGPlayerController.h"
#include "FGRailroadSubsystem.h"
#include "FGTrain.h"
#include "FGRailroadTimeTable.h"
#include "FGTrainStationIdentifier.h"
#include "FGTrainDockingRules.h"
#include "Buildables/FGBuildableRailroadStation.h"
#include "Buildables/FGBuildableTrainPlatformCargo.h"
#include "FGDroneSubsystem.h"
#include "FGDroneStationInfo.h"
#include "Buildables/FGBuildableDockingStation.h"
#include "WheeledVehicles/FGWheeledVehicle.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "FGMapManager.h"
#include "FGIconDatabaseSubsystem.h"
#include "FGEventSubsystem.h"
#include "FGBuildableBeam.h"
#include "Hologram/FGBeamHologram.h"
#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Buildables/FGBuildablePriorityPowerSwitch.h"
#include "FGPriorityPowerSwitchInfo.h"
#include "FGBuildingTagInterface.h"
#include "Resources/FGWildCardDescriptor.h"
#include "Buildables/FGBuildablePowerPole.h"

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

	// "RealCharacter" instigator strategy (2026-08-30) - the ORIGINAL,
	// proven-working ConstructConveyorBelt body, preserved verbatim as a
	// fallback/comparison strategy alongside the newer decoy-instigator
	// strategies (see ConstructConveyorBelt's own doc comment). Drives
	// the REAL player's BuildGun - reliable belt construction, but visibly
	// moves the real camera, which is exactly what the decoy strategies
	// are trying to avoid. Kept selectable via params.instigatorStrategy
	// so multiple competing fixes for the decoy path can be tried without
	// a fresh compile each time, per explicit user request.
	void ConstructConveyorBelt_RealCharacterStrategy(UObject* WorldContextObject, const FString& SourceBuildableId, const FString& DestBuildableId, const FString& RecipeClassPath, const FString& RouteMode, const TOptional<FVector>& SourceConnectorPosition, const TOptional<FVector>& DestConnectorPosition, bool bDryRun, TFunction<void(const FAIModOperationResult&)> OnComplete);

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

	// Bounds a single world.terrainHeightGrid call's cost - every point
	// runs a real synchronous line trace on the game thread (same as
	// FindGroundAtXY below), so an unbounded grid risks a real frame
	// hitch. 10000 = a 100x100 grid, comfortably enough for "survey an
	// immediate build area" without needing this raised.
	constexpr int64 MaxTerrainHeightGridPoints = 10000;

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

	// Vehicles (2026-08-29) - UFGVehicleDescriptor is a SIBLING of
	// UFGBuildingDescriptor (both derive from UFGBuildDescriptor
	// separately, confirmed from source), so a vehicle recipe's product
	// is never a UFGBuildingDescriptor and ResolveBuildableClassForRecipe
	// above naturally returns nullptr for one - this is the parallel
	// resolver for the vehicle case, same shape.
	TSubclassOf<AFGVehicle> ResolveVehicleClassForRecipe(const FString& RecipeClassPath)
	{
		UClass* RecipeClass = LoadObject<UClass>(nullptr, *RecipeClassPath);
		if (!RecipeClass || !RecipeClass->IsChildOf(UFGRecipe::StaticClass()))
		{
			return nullptr;
		}
		const TArray<FItemAmount> Products = UFGRecipe::GetProducts(RecipeClass);
		if (Products.Num() == 0 || !Products[0].ItemClass || !Products[0].ItemClass->IsChildOf(UFGVehicleDescriptor::StaticClass()))
		{
			return nullptr;
		}
		return UFGVehicleDescriptor::GetVehicleClass(TSubclassOf<UFGVehicleDescriptor>(Products[0].ItemClass.Get()));
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

	// Railroad tracks (2026-08-29) - UFGRailroadTrackConnectionComponent
	// is a real UFGConnectionComponent subclass with the same
	// GetConnectorLocation/GetConnectorNormal/IsConnected shape belt/pipe
	// connectors already use (confirmed from source), so this mirrors
	// FindFreeFactoryConnection/FindFreePipeConnection exactly. No
	// producer/consumer distinction - track connectors are bidirectional.
	// IsConnected() is `mConnectedComponents.Num() > 0` - true for a
	// switch (3+ pieces meeting at one point) even with further switch
	// positions open, so this only finds genuinely unconnected simple
	// connectors, matching this project's deliberate first-pass scope of
	// point-to-point track only (see docs/placement-lessons.md - switches
	// and signals are a real, separate future capability).
	UFGRailroadTrackConnectionComponent* FindFreeRailroadConnection(AFGBuildable* Buildable)
	{
		TArray<UFGRailroadTrackConnectionComponent*> Connections;
		Buildable->GetComponents<UFGRailroadTrackConnectionComponent>(Connections);
		for (UFGRailroadTrackConnectionComponent* Connection : Connections)
		{
			if (IsValid(Connection) && !Connection->IsConnected())
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

FString UAIModFunctionLibrary::LogTerrainHeightGridAsJson(UObject* WorldContextObject, float MinX, float MinY, float MaxX, float MaxY, float StepSize, float ReferenceZ)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		UE_LOG(LogAIModAI, Warning, TEXT("LogTerrainHeightGridAsJson: no valid world context"));
		return TEXT("{\"protocolVersion\":1,\"countX\":0,\"countY\":0,\"heights\":[],\"found\":[]}");
	}

	if (StepSize <= 0.0f || MaxX <= MinX || MaxY <= MinY)
	{
		UE_LOG(LogAIModAI, Warning, TEXT("LogTerrainHeightGridAsJson: invalid bounds/step (minX=%.1f maxX=%.1f minY=%.1f maxY=%.1f step=%.1f)"),
			MinX, MaxX, MinY, MaxY, StepSize);
		return TEXT("{\"protocolVersion\":1,\"countX\":0,\"countY\":0,\"heights\":[],\"found\":[]}");
	}

	const int32 CountX = FMath::FloorToInt((MaxX - MinX) / StepSize) + 1;
	const int32 CountY = FMath::FloorToInt((MaxY - MinY) / StepSize) + 1;
	const int64 TotalPoints = static_cast<int64>(CountX) * static_cast<int64>(CountY);

	if (TotalPoints > MaxTerrainHeightGridPoints)
	{
		UE_LOG(LogAIModAI, Warning, TEXT("LogTerrainHeightGridAsJson: requested grid has %lld points, exceeds the %lld limit - use a larger stepSize or smaller area"),
			TotalPoints, MaxTerrainHeightGridPoints);

		const TSharedRef<FJsonObject> ErrorRoot = MakeShared<FJsonObject>();
		ErrorRoot->SetNumberField(TEXT("protocolVersion"), 1);
		ErrorRoot->SetBoolField(TEXT("tooManyPoints"), true);
		ErrorRoot->SetNumberField(TEXT("requestedPoints"), static_cast<double>(TotalPoints));
		ErrorRoot->SetNumberField(TEXT("maxPoints"), static_cast<double>(MaxTerrainHeightGridPoints));
		ErrorRoot->SetNumberField(TEXT("countX"), 0);
		ErrorRoot->SetNumberField(TEXT("countY"), 0);
		ErrorRoot->SetArrayField(TEXT("heights"), TArray<TSharedPtr<FJsonValue>>());
		ErrorRoot->SetArrayField(TEXT("found"), TArray<TSharedPtr<FJsonValue>>());
		return WriteCondensedJson(ErrorRoot);
	}

	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
	const float ZSearchCenter = (ReferenceZ > -1000000.0f) ? ReferenceZ : (Character ? Character->GetActorLocation().Z : 0.0f);

	TArray<TSharedPtr<FJsonValue>> HeightsJsonArray;
	TArray<TSharedPtr<FJsonValue>> FoundJsonArray;
	HeightsJsonArray.Reserve(TotalPoints);
	FoundJsonArray.Reserve(TotalPoints);

	int32 FoundCount = 0;
	for (int32 RowIndex = 0; RowIndex < CountY; ++RowIndex)
	{
		const float Y = MinY + RowIndex * StepSize;
		for (int32 ColIndex = 0; ColIndex < CountX; ++ColIndex)
		{
			const float X = MinX + ColIndex * StepSize;
			const FGroundTraceResult GroundTrace = FindGroundAtXY(World, X, Y, ZSearchCenter, Character);
			HeightsJsonArray.Add(MakeShared<FJsonValueNumber>(GroundTrace.Hit.Location.Z));
			FoundJsonArray.Add(MakeShared<FJsonValueBoolean>(GroundTrace.bFound));
			FoundCount += GroundTrace.bFound ? 1 : 0;
		}
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetNumberField(TEXT("minX"), MinX);
	RootObject->SetNumberField(TEXT("minY"), MinY);
	RootObject->SetNumberField(TEXT("stepSize"), StepSize);
	RootObject->SetNumberField(TEXT("countX"), CountX);
	RootObject->SetNumberField(TEXT("countY"), CountY);
	RootObject->SetArrayField(TEXT("heights"), HeightsJsonArray);
	RootObject->SetArrayField(TEXT("found"), FoundJsonArray);

	const FString JsonString = WriteCondensedJson(RootObject);

	UE_LOG(LogAIModAI, Display, TEXT("LogTerrainHeightGridAsJson: %dx%d grid (%lld points, %d found), bounds=(%.1f,%.1f)-(%.1f,%.1f) step=%.1f"),
		CountX, CountY, TotalPoints, FoundCount, MinX, MinY, MaxX, MaxY, StepSize);

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

// world.vehicles (2026-08-29) - discovered live during unattended vehicle
// testing: AFGVehicle is not an AFGBuildable (AFGDriveablePawn, a separate
// hierarchy), so a constructed vehicle is completely invisible to
// world.buildables - there was no way at all to read back a vehicle
// world.constructVehicle just built. Minimal id/class/position/rotation,
// same shape as world.buildables, via a real TActorIterator<AFGVehicle>
// scan (the same pattern ConstructVehicle's own construction-confirmation
// step already uses). Richer per-vehicle state (fuel, cargo, docking
// status - real getters found in source research:
// AFGDroneVehicle::GetDockingState/GetHomeStation,
// AFGWheeledVehicle::GetFuelInventory) is real future work, not
// included here - this is deliberately just enough to find an id to pass
// to world.deleteBuilding (now vehicle-aware too, see DismantleBuildable).
FString UAIModFunctionLibrary::LogVehiclesAsJson(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;

	TArray<TSharedPtr<FJsonValue>> VehicleJsonArray;
	if (World)
	{
		for (TActorIterator<AFGVehicle> It(World); It; ++It)
		{
			if (!IsValid(*It)) { continue; }

			const TSharedRef<FJsonObject> VehicleObject = MakeShared<FJsonObject>();
			VehicleObject->SetStringField(TEXT("id"), It->GetPathName());
			VehicleObject->SetStringField(TEXT("buildableClass"), It->GetClass()->GetPathName());

			const FVector Location = It->GetActorLocation();
			const TSharedRef<FJsonObject> PositionObject = MakeShared<FJsonObject>();
			PositionObject->SetNumberField(TEXT("x"), Location.X);
			PositionObject->SetNumberField(TEXT("y"), Location.Y);
			PositionObject->SetNumberField(TEXT("z"), Location.Z);
			VehicleObject->SetObjectField(TEXT("position"), PositionObject);

			const FRotator Rotation = It->GetActorRotation();
			const TSharedRef<FJsonObject> RotationObject = MakeShared<FJsonObject>();
			RotationObject->SetNumberField(TEXT("pitch"), Rotation.Pitch);
			RotationObject->SetNumberField(TEXT("yaw"), Rotation.Yaw);
			RotationObject->SetNumberField(TEXT("roll"), Rotation.Roll);
			VehicleObject->SetObjectField(TEXT("rotation"), RotationObject);

			VehicleJsonArray.Add(MakeShared<FJsonValueObject>(VehicleObject));
		}
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetArrayField(TEXT("vehicles"), VehicleJsonArray);

	FString JsonString;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonString);
	FJsonSerializer::Serialize(RootObject, Writer);

	UE_LOG(LogAIModAI, Display, TEXT("LogVehiclesAsJson: %d vehicle(s)"), VehicleJsonArray.Num());

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

// See TeleportPlayer's doc comment in the header for the real
// TeleportTo/StopMovementImmediately/FindGroundAtXY sourcing behind this.
FAIModOperationResult UAIModFunctionLibrary::TeleportPlayer(UObject* WorldContextObject, float X, float Y, float Z, bool bIgnoreGroundTrace, bool bHasTargetYaw, float TargetYawDegrees)
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

	// Same ground-trace-or-literal-Z sentinel convention as
	// ConstructVehicle/ConstructBuildingAtPosition.
	if (bIgnoreGroundTrace && Z <= -1000000.0f)
	{
		return FAIModOperationResult::Failure(TEXT("MISSING_REFERENCE_Z"),
			TEXT("bIgnoreGroundTrace requires an explicit z - there is no ground trace to fall back to"));
	}

	FVector DestLocation;
	if (bIgnoreGroundTrace)
	{
		DestLocation = FVector(X, Y, Z);
	}
	else
	{
		const float ZSearchCenter = (Z > -1000000.0f) ? Z : Character->GetActorLocation().Z;
		const FGroundTraceResult GroundTrace = FindGroundAtXY(World, X, Y, ZSearchCenter, Character);
		// +100cm margin above the traced ground point - FindTeleportSpot
		// (inside TeleportTo) still resolves any remaining capsule
		// overlap, this just reduces reliance on it for the common case.
		DestLocation = GroundTrace.Hit.Location + FVector(0.0f, 0.0f, 100.0f);
	}

	const FRotator DestRotation = bHasTargetYaw
		? FRotator(Character->GetActorRotation().Pitch, TargetYawDegrees, Character->GetActorRotation().Roll)
		: Character->GetActorRotation();

	const bool bTeleportSucceeded = Character->TeleportTo(DestLocation, DestRotation, false, false);
	if (!bTeleportSucceeded)
	{
		UE_LOG(LogAIModAI, Warning, TEXT("TeleportPlayer: TeleportTo(%s) failed - no clear destination found nearby"), *DestLocation.ToString());
		return FAIModOperationResult::Failure(TEXT("TELEPORT_BLOCKED"), TEXT("TeleportTo found no clear destination near the requested location"));
	}

	if (UCharacterMovementComponent* MovementComponent = Character->GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
	}

	if (AController* Controller = Character->GetController())
	{
		Controller->SetControlRotation(DestRotation);
	}

	UE_LOG(LogAIModAI, Display, TEXT("TeleportPlayer: moved to %s"), *DestLocation.ToString());

	return FAIModOperationResult::Success();
}

namespace
{
	FString CompassViewDistanceToString(ECompassViewDistance Distance)
	{
		switch (Distance)
		{
		case ECompassViewDistance::CVD_Off: return TEXT("Off");
		case ECompassViewDistance::CVD_Near: return TEXT("Near");
		case ECompassViewDistance::CVD_Mid: return TEXT("Mid");
		case ECompassViewDistance::CVD_Far: return TEXT("Far");
		case ECompassViewDistance::CVD_Always: return TEXT("Always");
		default: return TEXT("Off");
		}
	}

	ECompassViewDistance ParseCompassViewDistance(const FString& Value)
	{
		if (Value.Equals(TEXT("Near"), ESearchCase::IgnoreCase)) { return ECompassViewDistance::CVD_Near; }
		if (Value.Equals(TEXT("Mid"), ESearchCase::IgnoreCase)) { return ECompassViewDistance::CVD_Mid; }
		if (Value.Equals(TEXT("Far"), ESearchCase::IgnoreCase)) { return ECompassViewDistance::CVD_Far; }
		if (Value.Equals(TEXT("Always"), ESearchCase::IgnoreCase)) { return ECompassViewDistance::CVD_Always; }
		return ECompassViewDistance::CVD_Off;
	}
}

// See LogMapMarkerIconsAsJson's doc comment in the header for the real
// AFGIconDatabaseSubsystem/ESIT_MapStamp sourcing behind this.
FString UAIModFunctionLibrary::LogMapMarkerIconsAsJson(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	AFGIconDatabaseSubsystem* IconDatabase = World ? AFGIconDatabaseSubsystem::Get(World) : nullptr;
	if (!IconDatabase)
	{
		UE_LOG(LogAIModAI, Warning, TEXT("LogMapMarkerIconsAsJson: no valid world context or AFGIconDatabaseSubsystem"));
		return TEXT("{}");
	}

	TArray<FIconData> IconDataArray;
	IconDatabase->GetAllIconDataForType(EIconType::ESIT_MapStamp, /*includeHidden=*/false, IconDataArray);

	TArray<TSharedPtr<FJsonValue>> IconsJsonArray;
	for (const FIconData& IconData : IconDataArray)
	{
		const TSharedRef<FJsonObject> IconObject = MakeShared<FJsonObject>();
		IconObject->SetNumberField(TEXT("iconId"), IconData.ID);
		IconObject->SetStringField(TEXT("name"), IconData.IconName.ToString());
		IconObject->SetBoolField(TEXT("animated"), IconData.Animated);
		IconsJsonArray.Add(MakeShared<FJsonValueObject>(IconObject));
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetArrayField(TEXT("icons"), IconsJsonArray);

	const FString JsonString = WriteCondensedJson(RootObject);

	UE_LOG(LogAIModAI, Display, TEXT("LogMapMarkerIconsAsJson: %d map stamp icon(s)"), IconsJsonArray.Num());

	return JsonString;
}

// See LogMapMarkersAsJson's doc comment in the header for the
// StaticEnum<ERepresentationType> reasoning.
FString UAIModFunctionLibrary::LogMapMarkersAsJson(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	AFGMapManager* MapManager = World ? AFGMapManager::Get(World) : nullptr;
	if (!MapManager)
	{
		UE_LOG(LogAIModAI, Warning, TEXT("LogMapMarkersAsJson: no valid world context or AFGMapManager"));
		return TEXT("{}");
	}

	TArray<FMapMarker> Markers;
	MapManager->GetMapMarkers(Markers);

	const UEnum* RepresentationTypeEnum = StaticEnum<ERepresentationType>();

	TArray<TSharedPtr<FJsonValue>> MarkersJsonArray;
	for (const FMapMarker& Marker : Markers)
	{
		const TSharedRef<FJsonObject> MarkerObject = MakeShared<FJsonObject>();
		MarkerObject->SetStringField(TEXT("id"), Marker.MarkerGUID.ToString());
		MarkerObject->SetStringField(TEXT("name"), Marker.Name);
		MarkerObject->SetStringField(TEXT("categoryName"), Marker.CategoryName);
		MarkerObject->SetNumberField(TEXT("iconId"), Marker.IconID);
		MarkerObject->SetStringField(TEXT("mapMarkerType"), RepresentationTypeEnum
			? RepresentationTypeEnum->GetNameStringByValue(static_cast<int64>(Marker.MapMarkerType))
			: FString());

		const TSharedRef<FJsonObject> PositionObject = MakeShared<FJsonObject>();
		PositionObject->SetNumberField(TEXT("x"), Marker.Location.X);
		PositionObject->SetNumberField(TEXT("y"), Marker.Location.Y);
		PositionObject->SetNumberField(TEXT("z"), Marker.Location.Z);
		MarkerObject->SetObjectField(TEXT("position"), PositionObject);

		const TSharedRef<FJsonObject> ColorObject = MakeShared<FJsonObject>();
		ColorObject->SetNumberField(TEXT("r"), Marker.Color.R);
		ColorObject->SetNumberField(TEXT("g"), Marker.Color.G);
		ColorObject->SetNumberField(TEXT("b"), Marker.Color.B);
		MarkerObject->SetObjectField(TEXT("color"), ColorObject);

		MarkerObject->SetNumberField(TEXT("scale"), Marker.Scale);
		MarkerObject->SetStringField(TEXT("compassViewDistance"), CompassViewDistanceToString(Marker.CompassViewDistance));

		MarkersJsonArray.Add(MakeShared<FJsonValueObject>(MarkerObject));
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetArrayField(TEXT("markers"), MarkersJsonArray);

	const FString JsonString = WriteCondensedJson(RootObject);

	UE_LOG(LogAIModAI, Display, TEXT("LogMapMarkersAsJson: %d marker(s)"), MarkersJsonArray.Num());

	return JsonString;
}

// See PlaceMapMarker's doc comment in the header for the real
// AddNewMapMarker sourcing and the white-vs-black color default reasoning.
FAIModOperationResult UAIModFunctionLibrary::PlaceMapMarker(UObject* WorldContextObject, float X, float Y, float Z, bool bIgnoreGroundTrace, int32 IconId, const FString& Name, bool bHasColor, float ColorR, float ColorG, float ColorB, float Scale, const FString& CompassViewDistance)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}

	AFGMapManager* MapManager = AFGMapManager::Get(World);
	if (!MapManager)
	{
		return FAIModOperationResult::Failure(TEXT("NO_MAP_MANAGER"), TEXT("AFGMapManager::Get() returned null"));
	}

	if (!MapManager->CanAddNewMapMarker())
	{
		return FAIModOperationResult::Failure(TEXT("MAP_MARKER_LIMIT_REACHED"),
			FString::Printf(TEXT("AFGMapManager::CanAddNewMapMarker() returned false - at or near the %d marker limit"), MapManager->GetMaxNumMapMarkers()));
	}

	// Same ground-trace-or-literal-Z sentinel convention as
	// ConstructVehicle/TeleportPlayer.
	if (bIgnoreGroundTrace && Z <= -1000000.0f)
	{
		return FAIModOperationResult::Failure(TEXT("MISSING_REFERENCE_Z"),
			TEXT("bIgnoreGroundTrace requires an explicit z - there is no ground trace to fall back to"));
	}

	FVector MarkerLocation;
	if (bIgnoreGroundTrace)
	{
		MarkerLocation = FVector(X, Y, Z);
	}
	else
	{
		AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
		const float ZSearchCenter = (Z > -1000000.0f) ? Z : (Character ? Character->GetActorLocation().Z : 0.0f);
		const FGroundTraceResult GroundTrace = FindGroundAtXY(World, X, Y, ZSearchCenter, Character);
		MarkerLocation = GroundTrace.Hit.Location;
	}

	FMapMarker NewMarker;
	NewMarker.Location = MarkerLocation;
	NewMarker.Name = Name;
	NewMarker.MapMarkerType = ERepresentationType::RT_Default;
	NewMarker.IconID = IconId;
	// White, NOT FMapMarker's own literal FLinearColor::Black default -
	// see this function's header doc comment for why.
	NewMarker.Color = bHasColor ? FLinearColor(ColorR, ColorG, ColorB, 1.0f) : FLinearColor::White;
	NewMarker.Scale = Scale;
	NewMarker.CompassViewDistance = ParseCompassViewDistance(CompassViewDistance);

	FMapMarker CreatedMarker;
	const bool bAdded = MapManager->AddNewMapMarker(NewMarker, CreatedMarker);
	if (!bAdded)
	{
		return FAIModOperationResult::Failure(TEXT("MAP_MARKER_ADD_FAILED"), TEXT("AFGMapManager::AddNewMapMarker() returned false"));
	}

	const TSharedRef<FJsonObject> DetailObject = MakeShared<FJsonObject>();
	DetailObject->SetStringField(TEXT("markerId"), CreatedMarker.MarkerGUID.ToString());

	UE_LOG(LogAIModAI, Display, TEXT("PlaceMapMarker: created '%s' (icon %d) at %s, guid=%s"),
		*Name, IconId, *MarkerLocation.ToString(), *CreatedMarker.MarkerGUID.ToString());

	FAIModOperationResult Result = FAIModOperationResult::Success();
	Result.ResultDetailJson = WriteCondensedJson(DetailObject);
	return Result;
}

// See RemoveMapMarker's doc comment in the header for why this looks the
// marker up (and re-verifies removal) rather than trusting a bare-GUID
// FMapMarker's stub-sourced operator== blindly.
FAIModOperationResult UAIModFunctionLibrary::RemoveMapMarker(UObject* WorldContextObject, const FString& MarkerId)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}

	AFGMapManager* MapManager = AFGMapManager::Get(World);
	if (!MapManager)
	{
		return FAIModOperationResult::Failure(TEXT("NO_MAP_MANAGER"), TEXT("AFGMapManager::Get() returned null"));
	}

	FGuid TargetGuid;
	if (!FGuid::Parse(MarkerId, TargetGuid))
	{
		return FAIModOperationResult::Failure(TEXT("INVALID_MARKER_ID"), FString::Printf(TEXT("'%s' is not a valid GUID"), *MarkerId));
	}

	TArray<FMapMarker> Markers;
	MapManager->GetMapMarkers(Markers);
	const FMapMarker* FoundMarker = Markers.FindByPredicate([&TargetGuid](const FMapMarker& Marker) { return Marker.MarkerGUID == TargetGuid; });
	if (!FoundMarker)
	{
		return FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No map marker with id '%s'"), *MarkerId));
	}

	MapManager->RemoveMapMarker(*FoundMarker);

	TArray<FMapMarker> MarkersAfterRemoval;
	MapManager->GetMapMarkers(MarkersAfterRemoval);
	const bool bStillPresent = MarkersAfterRemoval.ContainsByPredicate([&TargetGuid](const FMapMarker& Marker) { return Marker.MarkerGUID == TargetGuid; });
	if (bStillPresent)
	{
		return FAIModOperationResult::Failure(TEXT("MAP_MARKER_REMOVE_FAILED"), TEXT("Marker still present after AFGMapManager::RemoveMapMarker() - real removal not confirmed"));
	}

	UE_LOG(LogAIModAI, Display, TEXT("RemoveMapMarker: removed %s"), *MarkerId);

	return FAIModOperationResult::Success();
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

	// Widened 2026-08-30 (was AFGBuildableManufacturer-only, via
	// FindManufacturerById) - GetCanChangePotential/GetCurrentMinPotential/
	// GetCurrentMaxPotential/SetPendingPotential are all declared on the
	// shared AFGBuildableFactory base, confirmed from source
	// (AFGBuildableResourceExtractorBase, a Miner's real base class, IS an
	// AFGBuildableFactory) - found live while planning an overclocked
	// Miner test that needed this to work on a Miner, not just
	// Smelters/Constructors. FindBuildableById + Cast, not
	// FindManufacturerById, so this now covers both.
	AFGBuildableFactory* Factory = Cast<AFGBuildableFactory>(FindBuildableById(World, BuildableId));
	if (!Factory)
	{
		return FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"),
			FString::Printf(TEXT("No manufacturer or extractor found with id '%s'"), *BuildableId));
	}

	if (!Factory->GetCanChangePotential())
	{
		return FAIModOperationResult::Failure(TEXT("OPERATION_NOT_PERMITTED"),
			TEXT("This building does not allow changing clock speed"));
	}

	const float RequestedPotential = ClockSpeedPercent / 100.0f;
	const float MinPotential = Factory->GetCurrentMinPotential();
	const float MaxPotential = Factory->GetCurrentMaxPotential();
	if (RequestedPotential < MinPotential || RequestedPotential > MaxPotential)
	{
		return FAIModOperationResult::Failure(TEXT("INVALID_CLOCK_SPEED"),
			FString::Printf(TEXT("clockSpeedPercent %.1f is outside the valid range [%.1f, %.1f]"),
				ClockSpeedPercent, MinPotential * 100.0f, MaxPotential * 100.0f));
	}

	// Takes effect at the next production cycle, not instantly - see
	// AFGBuildableFactory::SetPendingPotential's doc comment.
	Factory->SetPendingPotential(RequestedPotential);

	UE_LOG(LogAIModAI, Display, TEXT("SetManufacturerClockSpeed: %s -> %.1f%% (pending)"), *BuildableId, ClockSpeedPercent);

	return FAIModOperationResult::Success();
}

FAIModOperationResult UAIModFunctionLibrary::InstallPowerShard(UObject* WorldContextObject, const FString& BuildableId, int32 Count)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}

	if (Count <= 0)
	{
		return FAIModOperationResult::Failure(TEXT("INVALID_REQUEST"), TEXT("count must be a positive integer"));
	}

	AFGBuildableFactory* Factory = Cast<AFGBuildableFactory>(FindBuildableById(World, BuildableId));
	if (!Factory)
	{
		return FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"),
			FString::Printf(TEXT("No manufacturer or extractor found with id '%s'"), *BuildableId));
	}

	if (!Factory->GetCanChangePotential())
	{
		return FAIModOperationResult::Failure(TEXT("OPERATION_NOT_PERMITTED"),
			TEXT("This building does not allow changing clock speed, so it has no potential/shard inventory"));
	}

	UFGInventoryComponent* PotentialInventory = Factory->GetPotentialInventory();
	if (!PotentialInventory)
	{
		return FAIModOperationResult::Failure(TEXT("OPERATION_NOT_PERMITTED"),
			FString::Printf(TEXT("'%s' has no potential/overclock shard inventory"), *BuildableId));
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

	// Real, verified item class (world.recipeCatalog) - there is exactly
	// one real overclock shard item in the game, hardcoded the same way
	// DebugCheckPowerConnection hardcodes Recipe_PowerLine.
	UClass* ShardClass = LoadObject<UClass>(nullptr, TEXT("/Game/FactoryGame/Resource/Environment/Crystal/Desc_CrystalShard.Desc_CrystalShard_C"));
	if (!ShardClass || !ShardClass->IsChildOf(UFGItemDescriptor::StaticClass()))
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("Failed to load the real Power Shard item class"));
	}

	if (!PlayerInventory->HasItems(ShardClass, Count))
	{
		const int32 Have = PlayerInventory->GetNumItems(ShardClass);
		return FAIModOperationResult::Failure(TEXT("INSUFFICIENT_INGREDIENTS"),
			FString::Printf(TEXT("Need %d Power Shard(s), player carries %d"), Count, Have));
	}

	// Verify-then-remove-then-add, restoring any excess that didn't fit -
	// same discipline as the dismantle refund fix, never destroys real
	// items on a partial add.
	PlayerInventory->Remove(ShardClass, Count);
	const int32 ActuallyAdded = PotentialInventory->AddStack(FInventoryStack(Count, ShardClass), /*allowPartialAdd=*/true);
	if (ActuallyAdded < Count)
	{
		PlayerInventory->AddStack(FInventoryStack(Count - ActuallyAdded, ShardClass), /*allowPartialAdd=*/true);
	}

	if (ActuallyAdded == 0)
	{
		return FAIModOperationResult::Failure(TEXT("OPERATION_NOT_PERMITTED"),
			FString::Printf(TEXT("'%s' has no free slots in its potential inventory - 0 of %d Power Shard(s) could be added, all restored to player"), *BuildableId, Count));
	}

	const TSharedRef<FJsonObject> DetailObject = MakeShared<FJsonObject>();
	DetailObject->SetNumberField(TEXT("shardsAdded"), ActuallyAdded);
	DetailObject->SetNumberField(TEXT("newMaxPotentialPercent"), Factory->GetCurrentMaxPotential() * 100.0);

	UE_LOG(LogAIModAI, Display, TEXT("InstallPowerShard: %s +%d shard(s) (requested %d) -> newMaxPotential=%.1f%%"),
		*BuildableId, ActuallyAdded, Count, Factory->GetCurrentMaxPotential() * 100.0f);

	FAIModOperationResult Result = FAIModOperationResult::Success();
	Result.ResultDetailJson = WriteCondensedJson(DetailObject);
	return Result;
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

	// Vehicles (2026-08-29) - same conservative posture as the extractor
	// refusal above, by structural analogy rather than a confirmed crash:
	// a Drone hologram has a mandatory mSnappedStation reference (see
	// UFGCDMustSnapStation in FGConstructDisqualifier.h) this generic
	// path never populates, exactly the shape of precondition the
	// extractor crash came from (a disqualifier existing specifically to
	// block construction without a reference the real Construct() path
	// may not itself defensively re-check). Route through
	// world.constructVehicle (ConstructVehicle) instead, which resolves
	// and snaps to a real Drone Station for drone recipes.
	if (const TSubclassOf<AFGVehicle> ResolvedVehicleClass = ResolveVehicleClassForRecipe(RecipeClassPath))
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("WRONG_METHOD_FOR_VEHICLE"),
			FString::Printf(TEXT("'%s' is a vehicle recipe - use world.constructVehicle (ConstructVehicle) instead. Drone recipes specifically require snapping to a real Drone Station reference this generic path never provides."), *RecipeClassPath)));
		return;
	}

	// Spline-snapped buildables (2026-08-29) - CONFIRMED LIVE CRASH, not
	// just a structural analogy this time: a real unattended test run
	// placed Build_ConveyorMonitor through this exact generic path and
	// crashed the whole game process (EXCEPTION_ACCESS_VIOLATION reading
	// address 0x10). Root-caused from source, not guessed:
	// AFGBuildableSplineSnappedBase::SetSnappedSplineBuildable() (the
	// base class's own real, non-stub inline implementation, confirmed
	// in FGBuildableSplineSnappedBase.h) unconditionally calls
	// buildable->Implements<UFGSplineBuildableInterface>() on its
	// parameter with no null check - AFGBuildableHologram::ConstructInstance()
	// calls this during Construct() with whatever got snapped during
	// placement, which is nullptr when there was never a real belt/pipe
	// to snap to (this generic path never attempts that snap at all).
	// Refuse unconditionally, same posture as the extractor refusal
	// above - there is currently no dedicated ConstructSplineSnapped*
	// entry point that does the real snap (only Build_ConveyorMonitor is
	// known to derive from this base in the installed headers, but the
	// refusal is keyed on the base class, not the one known subclass, so
	// it also covers any other spline-snapped buildable this project
	// hasn't encountered yet).
	if (ResolvedBuildableClass && ResolvedBuildableClass->IsChildOf(AFGBuildableSplineSnappedBase::StaticClass()))
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("WRONG_METHOD_FOR_SPLINE_SNAPPED"),
			FString::Printf(TEXT("'%s' derives from AFGBuildableSplineSnappedBase (e.g. Conveyor Monitor) - it must be snapped to a real belt/pipe buildable to construct safely. world.placeBuilding without one is a CONFIRMED CRASH (AFGBuildableSplineSnappedBase::SetSnappedSplineBuildable() dereferences a null snap target), not just a bad placement. No dedicated RPC for this exists yet - do not attempt to bypass this refusal."), *RecipeClassPath)));
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

	// Fix (2026-08-31, real correctness bug found while investigating a
	// user question about Power Tower support, not a regression - this
	// was wrong from the day it was written): the OLD version of this
	// helper picked the FIRST free UFGPowerConnectionComponent on a
	// buildable, completely ignoring EPowerConnectionType
	// (FGPowerConnectionComponent.h: "Power connections of different
	// types are incompatible", real PCT_Default/PCT_PowerTower/PCT_Any
	// enum). A Power Tower (AFGBuildablePowerPole with
	// mPowerPoleType==PPT_TOWER - confirmed from source that
	// AFGBuildablePowerTower, a separate near-empty class, is unused
	// anywhere else in the header tree and is NOT the real buildable)
	// genuinely has TWO power connectors: one PCT_PowerTower (the real,
	// per-instance-configurable long-range link to another tower -
	// AFGBuildablePowerPole::GetPowerTowerWireMaxLength(), distinct from
	// AFGBuildableWire::mMaxPowerTowerLength on the wire recipe itself)
	// and one PCT_Default (short range, for a nearby pole/machine, same
	// type ordinary poles/machines use). Called independently per
	// buildable, the old function had no way to know WHICH of a tower's
	// two connectors the other side actually needed - it just returned
	// whichever GetComponents<>() happened to enumerate first, which is
	// not a meaningful/stable order. This could silently pick a tower's
	// short-range connector for what should be a long-range tower-to-
	// tower link (or vice versa), either failing outright (type
	// mismatch) or succeeding against the wrong distance limit.
	//
	// Fixed by making connector selection a joint decision over BOTH
	// buildables at once: pass 1 requires an EXACT GetPowerConnectionType()
	// match on both sides (so two Power Towers pick their PCT_PowerTower
	// connectors, and everything else - poles, machines, a tower's own
	// short-range side - pairs PCT_Default to PCT_Default); pass 2 falls
	// back to any pairing where at least one side is the real PCT_Any
	// wildcard type, per the enum's own "incompatible" doc comment
	// implying Any is the one documented exception. NOT YET LIVE-TESTED
	// - no game running this session - but this is a real, source-
	// grounded fix to logic that was never type-aware, not a guess.
	bool FindPowerConnectionPair(AFGBuildable* BuildableA, AFGBuildable* BuildableB, UFGPowerConnectionComponent*& OutConnectionA, UFGPowerConnectionComponent*& OutConnectionB)
	{
		OutConnectionA = nullptr;
		OutConnectionB = nullptr;

		TArray<UFGPowerConnectionComponent*> ConnectionsA;
		BuildableA->GetComponents<UFGPowerConnectionComponent>(ConnectionsA);
		TArray<UFGPowerConnectionComponent*> ConnectionsB;
		BuildableB->GetComponents<UFGPowerConnectionComponent>(ConnectionsB);

		auto IsFree = [](const UFGPowerConnectionComponent* Connection) { return IsValid(Connection) && Connection->GetNumFreeConnections() > 0; };

		for (UFGPowerConnectionComponent* CandidateA : ConnectionsA)
		{
			if (!IsFree(CandidateA)) { continue; }
			for (UFGPowerConnectionComponent* CandidateB : ConnectionsB)
			{
				if (!IsFree(CandidateB)) { continue; }
				if (CandidateA->GetPowerConnectionType() == CandidateB->GetPowerConnectionType())
				{
					OutConnectionA = CandidateA;
					OutConnectionB = CandidateB;
					return true;
				}
			}
		}

		for (UFGPowerConnectionComponent* CandidateA : ConnectionsA)
		{
			if (!IsFree(CandidateA)) { continue; }
			for (UFGPowerConnectionComponent* CandidateB : ConnectionsB)
			{
				if (!IsFree(CandidateB)) { continue; }
				if (CandidateA->GetPowerConnectionType() == EPowerConnectionType::PCT_Any || CandidateB->GetPowerConnectionType() == EPowerConnectionType::PCT_Any)
				{
					OutConnectionA = CandidateA;
					OutConnectionB = CandidateB;
					return true;
				}
			}
		}

		return false;
	}

	// Same pattern as FindFreePowerConnection, for the conveyor-belt
	// snap-target experiment - matches the given direction (Output for
	// the belt's start point) and isn't already connected.
	//
	// SnapOnly fallback (2026-08-30, correcting a real misconception, not
	// a regression - see docs/placement-lessons.md's "Conveyor walls are
	// real connectors" section): conveyor walls/poles expose exactly one
	// UFGFactoryConnectionComponent with GetDirection()==FCD_SNAP_ONLY,
	// confirmed both live (world.connections on a player-built wall-lift-
	// wall structure) and from source (FGFactoryConnectionComponent.h:
	// "Special case for conveyor poles"). The strict `== Direction` match
	// above always skipped these, so ConstructConveyorLift/ConnectConveyor
	// could never target a wall at all - confirmed live, reproduced the
	// exact NO_FACTORY_CONNECTION error before this fix. SnapOnly's
	// IsConnected() is ALWAYS false by engine design (see the header's
	// IsConnected() doc comment: "Always false if attached to hologram,
	// snap only..."), so it can't be used to test whether a SnapOnly point
	// is already occupied - the real game handles that via a separate
	// overlap check (CheckIfSnapOnlyIsBlockedByOtherConnection, private to
	// the engine) inside TrySnapToActor/DoMultiStepPlacement once fed the
	// real connector position/normal, which this function does not need to
	// duplicate. Only used as a fallback when no exact Input/Output match
	// exists, so ordinary machines (which never have SnapOnly connectors)
	// are unaffected.
	UFGFactoryConnectionComponent* FindFreeFactoryConnection(AFGBuildable* Buildable, EFactoryConnectionDirection Direction)
	{
		TArray<UFGFactoryConnectionComponent*> Connections;
		Buildable->GetComponents<UFGFactoryConnectionComponent>(Connections);
		UFGFactoryConnectionComponent* SnapOnlyFallback = nullptr;
		for (UFGFactoryConnectionComponent* Connection : Connections)
		{
			if (!IsValid(Connection))
			{
				continue;
			}
			if (Connection->GetDirection() == Direction && !Connection->IsConnected())
			{
				return Connection;
			}
			if (!SnapOnlyFallback && Connection->GetDirection() == EFactoryConnectionDirection::FCD_SNAP_ONLY)
			{
				SnapOnlyFallback = Connection;
			}
		}
		return SnapOnlyFallback;
	}

	// Position-targeted variant (2026-08-30, explicit user requirement:
	// deterministic selection of ONE SPECIFIC connector on a multi-port
	// buildable like a splitter/merger, by its real world position - never
	// "first free"/"nearest"/component-array-order). The caller (Python
	// controller side) is expected to have already queried world.connections
	// for the real connector position it wants (e.g. via
	// satisfactory_ai.splitters.get_splitter_output_facing(), which
	// resolves a cardinal direction to an exact real position/normal), and
	// pass that exact position back in. A small tolerance (not an exact
	// float match) accounts for the caller having read the position from
	// a prior world.connections call - same connector, same real
	// transform, but float round-tripping through JSON. Returns nullptr
	// (caller reports a clear error) rather than silently falling back to
	// ANY other free connector if nothing matches within tolerance - this
	// is the single change that makes deterministic per-port selection
	// possible at all; FindFreeFactoryConnection above has no direction-
	// vs-position awareness and was never meant to guarantee which of
	// several free connectors of the same Direction gets picked.
	// SnapOnly fallback (2026-08-30): same reasoning as FindFreeFactoryConnection
	// above - a free exact-direction match wins if one is within tolerance,
	// otherwise the nearest SnapOnly connector (wall/pole) within tolerance
	// is used, without gating on IsConnected() since that's always false
	// for SnapOnly by engine design.
	UFGFactoryConnectionComponent* FindFreeFactoryConnectionNear(AFGBuildable* Buildable, EFactoryConnectionDirection Direction, const FVector& TargetWorldPosition, float ToleranceCm = 10.0f)
	{
		TArray<UFGFactoryConnectionComponent*> Connections;
		Buildable->GetComponents<UFGFactoryConnectionComponent>(Connections);
		UFGFactoryConnectionComponent* Best = nullptr;
		float BestDistSq = FMath::Square(ToleranceCm);
		UFGFactoryConnectionComponent* SnapOnlyBest = nullptr;
		float SnapOnlyBestDistSq = FMath::Square(ToleranceCm);
		for (UFGFactoryConnectionComponent* Connection : Connections)
		{
			if (!IsValid(Connection))
			{
				continue;
			}
			const float DistSq = FVector::DistSquared(Connection->GetConnectorLocation(), TargetWorldPosition);
			if (Connection->GetDirection() == Direction && !Connection->IsConnected() && DistSq <= BestDistSq)
			{
				Best = Connection;
				BestDistSq = DistSq;
			}
			else if (Connection->GetDirection() == EFactoryConnectionDirection::FCD_SNAP_ONLY && DistSq <= SnapOnlyBestDistSq)
			{
				SnapOnlyBest = Connection;
				SnapOnlyBestDistSq = DistSq;
			}
		}
		return Best ? Best : SnapOnlyBest;
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

	// Vehicles (2026-08-29, discovered live during unattended vehicle
	// testing): world.constructVehicle produces a real AFGVehicle, but
	// AFGVehicle is not an AFGBuildable (confirmed from source -
	// AFGDriveablePawn, a separate hierarchy - same reason
	// world.buildables can't see it either), so FindBuildableById above
	// always misses it, leaving no RPC way to remove a constructed
	// vehicle at all. AFGVehicle DOES implement IFGDismantleInterface
	// (same interface AFGBuildable does), so the exact same
	// Execute_CanDismantle/Execute_GetChildDismantleActors/Execute_Dismantle
	// calls below work unchanged once a target actor is found - this
	// only needed a second id-resolution path, not new dismantle logic.
	AActor* DismantleTarget = Buildable;
	if (!DismantleTarget)
	{
		for (TActorIterator<AFGVehicle> It(World); It; ++It)
		{
			if (IsValid(*It) && It->GetPathName() == BuildableId)
			{
				DismantleTarget = *It;
				break;
			}
		}
	}

	if (!DismantleTarget)
	{
		return FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"),
			FString::Printf(TEXT("No buildable or vehicle found with id '%s'"), *BuildableId));
	}

	if (!IFGDismantleInterface::Execute_CanDismantle(DismantleTarget))
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
	IFGDismantleInterface::Execute_GetChildDismantleActors(DismantleTarget, ChildDismantleActors);
	for (AActor* ChildActor : ChildDismantleActors)
	{
		if (IsValid(ChildActor) && ChildActor->Implements<UFGDismantleInterface>()
			&& IFGDismantleInterface::Execute_CanDismantle(ChildActor))
		{
			IFGDismantleInterface::Execute_Dismantle(ChildActor);
		}
	}

	// Real construction-cost refund - added 2026-08-30 after live user
	// confirmation this was NEVER happening (a real, costly bug: this
	// function only ever called Execute_Dismantle(), which does not
	// refund anything itself - GetDismantleRefund() is a SEPARATE
	// interface function the real player-driven dismantle path
	// (UFGBuildGunStateDismantle) calls independently, confirmed from
	// FGDismantleInterface.h's own doc comments. RPC_REFERENCE.md's
	// long-standing "refunds construction cost" claim for
	// world.deleteBuilding was therefore never actually true - a
	// documentation error that cost the user several thousand real Iron
	// Plates across this session's repeated platform rebuilds before
	// being caught.
	//
	// Computed BEFORE dismantling (the target must still be valid), then
	// applied directly to the player's carried inventory via AddStack -
	// same direct-inventory-manipulation pattern already proven reliable
	// elsewhere in this file (SimulatedCraft, MovePortableMinerToInventory),
	// deliberately NOT relying on FDismantleHelpers::DropRefundOnGround
	// (an uncertain, stub-bodied ground-spawn path - direct inventory
	// credit is simpler and more predictable for an autonomous caller
	// with no camera/aim location to drop a crate near anyway).
	TArray<FInventoryStack> RefundStacks;
	if (DismantleTarget->Implements<UFGDismantleInterface>())
	{
		IFGDismantleInterface::Execute_GetDismantleRefund(DismantleTarget, RefundStacks, /*noBuildCostEnabled=*/false);
	}

	// Real, safe dismantle - see this function's header doc comment.
	// AFGBuildable::Dismantle_Implementation()/AFGVehicle's own
	// implementation handle connection cleanup, inventory locking/
	// emptying, subsystem deregistration, and network-replicated actor
	// destruction; this is not AActor::Destroy().
	IFGDismantleInterface::Execute_Dismantle(DismantleTarget);

	int32 RefundedStackCount = 0;
	if (RefundStacks.Num() > 0)
	{
		if (AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0)))
		{
			if (UFGInventoryComponent* PlayerInventory = Character->GetInventory())
			{
				for (const FInventoryStack& Stack : RefundStacks)
				{
					if (Stack.HasItems())
					{
						PlayerInventory->AddStack(Stack, /*allowPartialAdd=*/true);
						++RefundedStackCount;
					}
				}
			}
			else
			{
				UE_LOG(LogAIModAI, Warning, TEXT("DismantleBuildable: %s had a real refund (%d stack(s)) but no player inventory was found to credit it to - refund lost"), *BuildableId, RefundStacks.Num());
			}
		}
		else
		{
			UE_LOG(LogAIModAI, Warning, TEXT("DismantleBuildable: %s had a real refund (%d stack(s)) but no local player was found to credit it to - refund lost"), *BuildableId, RefundStacks.Num());
		}
	}

	UE_LOG(LogAIModAI, Display, TEXT("DismantleBuildable: %s (%d child actor(s) dismantled, %d refund stack(s) credited)"), *BuildableId, ChildDismantleActors.Num(), RefundedStackCount);

	return FAIModOperationResult::Success();
}

FAIModOperationResult UAIModFunctionLibrary::SetBuildableRotation(UObject* WorldContextObject, const FString& BuildableId, float Yaw)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}

	AFGBuildable* Buildable = FindBuildableById(World, BuildableId);
	if (!Buildable)
	{
		return FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No buildable found with id '%s'"), *BuildableId));
	}

	const FRotator OldRotation = Buildable->GetActorRotation();
	FRotator NewRotation = OldRotation;
	NewRotation.Yaw = Yaw;

	// Diagnostic (2026-08-31): SetActorRotation() alone reported success
	// but produced ZERO actual change live (GetActorRotation() read back
	// identical afterward, in the same process/call - not a replication
	// issue). Real Unreal buildables commonly mark their root/mesh
	// components Static for lighting/rendering optimization, which
	// silently refuses runtime SetWorldRotation() (the warning macros for
	// this are usually compiled out of Shipping, explaining the total
	// silence in the log). Testing by forcing every scene component to
	// Movable first, logging each one's ORIGINAL mobility so this either
	// confirms the theory or rules it out.
	TArray<USceneComponent*> SceneComponents;
	Buildable->GetComponents<USceneComponent>(SceneComponents);
	for (USceneComponent* Component : SceneComponents)
	{
		if (IsValid(Component))
		{
			UE_LOG(LogAIModAI, Display, TEXT("SetBuildableRotation: component %s original mobility=%d"), *Component->GetName(), static_cast<int32>(Component->Mobility.GetValue()));
			Component->SetMobility(EComponentMobility::Movable);
		}
	}

	Buildable->SetActorRotation(NewRotation);

	const FRotator ActualRotation = Buildable->GetActorRotation();
	UE_LOG(LogAIModAI, Display, TEXT("SetBuildableRotation: %s yaw %.2f -> requested %.2f, actual after SetActorRotation=%.2f"), *BuildableId, OldRotation.Yaw, Yaw, ActualRotation.Yaw);

	return FAIModOperationResult::Success();
}

FAIModOperationResult UAIModFunctionLibrary::SetBuildableColor(UObject* WorldContextObject, const FString& BuildableId, float PrimaryR, float PrimaryG, float PrimaryB, float SecondaryR, float SecondaryG, float SecondaryB, bool bHasSecondaryColor)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}

	AFGBuildable* Buildable = FindBuildableById(World, BuildableId);
	if (!Buildable)
	{
		return FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No buildable found with id '%s'"), *BuildableId));
	}

	if (!Buildable->GetCanBeColored_Native())
	{
		return FAIModOperationResult::Failure(TEXT("NOT_COLORABLE"), FString::Printf(TEXT("'%s' reports GetCanBeColored_Native()=false"), *BuildableId));
	}

	// Start from the EXISTING data (not a fresh default-constructed
	// struct) so SwatchDesc/PatternDesc/MaterialDesc/SkinDesc are
	// preserved - only the color override and slot are touched. See
	// this function's header doc comment for the full source-confirmed
	// rationale (FGColorInterface.h/FGFactoryColoringTypes.h).
	FFactoryCustomizationData NewData = Buildable->GetCustomizationData_Native();
	NewData.OverrideColorData.PrimaryColor = FLinearColor(PrimaryR, PrimaryG, PrimaryB, 1.0f);
	NewData.OverrideColorData.SecondaryColor = bHasSecondaryColor
		? FLinearColor(SecondaryR, SecondaryG, SecondaryB, 1.0f)
		: FLinearColor(PrimaryR, PrimaryG, PrimaryB, 1.0f);
	NewData.ColorSlot = INDEX_CUSTOM_COLOR_SLOT;

	Buildable->SetCustomizationData_Native(NewData);

	UE_LOG(LogAIModAI, Display, TEXT("SetBuildableColor: %s primary=(%.2f,%.2f,%.2f) secondary=(%.2f,%.2f,%.2f)"),
		*BuildableId, PrimaryR, PrimaryG, PrimaryB,
		NewData.OverrideColorData.SecondaryColor.R, NewData.OverrideColorData.SecondaryColor.G, NewData.OverrideColorData.SecondaryColor.B);

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

	// FindPowerConnectionPair (2026-08-31 fix) - a joint, type-aware
	// selection over both buildables at once, see its own comment for
	// why a Power Tower's dual PCT_PowerTower/PCT_Default connectors
	// need this instead of two independent single-buildable lookups.
	UFGPowerConnectionComponent* ConnectionA = nullptr;
	UFGPowerConnectionComponent* ConnectionB = nullptr;
	if (!FindPowerConnectionPair(BuildableA, BuildableB, ConnectionA, ConnectionB))
	{
		return FAIModOperationResult::Failure(TEXT("NO_POWER_CONNECTION"),
			FString::Printf(TEXT("No compatible free power connection pair between '%s' and '%s' - connection types (Default/PowerTower/Any) must match, or one side must be Any"), *BuildableIdA, *BuildableIdB));
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

void UAIModFunctionLibrary::ConstructVehicle(UObject* WorldContextObject, const FString& RecipeClassPath, const FString& DroneStationId, float X, float Y, float Z, bool bIgnoreGroundTrace, bool bHasTargetYaw, float TargetYawDegrees, TFunction<void(const FAIModOperationResult&)> OnComplete)
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

	UClass* ResolvedClass = LoadObject<UClass>(nullptr, *RecipeClassPath);
	if (!ResolvedClass || !ResolvedClass->IsChildOf(UFGRecipe::StaticClass()))
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("INVALID_RECIPE"), FString::Printf(TEXT("'%s' did not resolve to a UFGRecipe subclass"), *RecipeClassPath)));
		return;
	}
	const TSubclassOf<UFGRecipe> RecipeClass = ResolvedClass;

	// AFGBuildableDroneStation is a real AFGBuildable (AFGBuildableFactory),
	// so the existing generic id resolver already works here - resolved
	// BEFORE spawning any hologram so a bad id fails cheaply.
	AFGBuildableDroneStation* TargetStation = nullptr;
	if (!DroneStationId.IsEmpty())
	{
		AFGBuildable* StationBuildable = FindBuildableById(World, DroneStationId);
		TargetStation = Cast<AFGBuildableDroneStation>(StationBuildable);
		if (!TargetStation)
		{
			OnComplete(FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("'%s' did not resolve to an AFGBuildableDroneStation"), *DroneStationId)));
			return;
		}
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

	// Confirms RecipeClassPath genuinely produced a vehicle hologram - a
	// mismatched recipe (e.g. a normal building) naturally fails here
	// instead of being driven through vehicle-specific snap/construct
	// logic it was never designed for.
	AFGVehicleHologram* Hologram = Cast<AFGVehicleHologram>(BuildState->GetHologram());
	if (!Hologram)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("HOLOGRAM_SPAWN_FAILED"),
			FString::Printf(TEXT("HotKeyRecipe(%s) did not result in an AFGVehicleHologram - is this actually a vehicle recipe?"), *RecipeClassPath)));
		return;
	}

	FHitResult SyntheticHit;
	if (TargetStation)
	{
		// Drone: snap to the station, same synthetic-hit-at-target-actor
		// shape ConstructExtractorOnNode uses for resource nodes -
		// Distance/Component/HitObjectHandle all populated for the same
		// reason documented there (a zero-distance synthetic hit was
		// confirmed live to fail a real placement-validation sanity
		// check).
		const FVector StationLocation = TargetStation->GetActorLocation();
		SyntheticHit.Location = StationLocation;
		SyntheticHit.ImpactPoint = StationLocation;
		SyntheticHit.Normal = FVector::UpVector;
		SyntheticHit.ImpactNormal = FVector::UpVector;
		SyntheticHit.HitObjectHandle = FActorInstanceHandle(TargetStation);
		SyntheticHit.bBlockingHit = true;
		if (UPrimitiveComponent* StationPrimitive = Cast<UPrimitiveComponent>(TargetStation->GetRootComponent()))
		{
			SyntheticHit.Component = StationPrimitive;
		}
		SyntheticHit.Distance = FVector::Dist(Character->GetActorLocation(), StationLocation);
	}
	else
	{
		// Wheeled vehicle (or any non-drone vehicle recipe): free
		// placement at literal X/Y, same ground-trace-or-literal-Z choice
		// as ConstructBuildingAtPosition's bIgnoreGroundTrace.
		if (bIgnoreGroundTrace && Z <= -1000000.0f)
		{
			Character->UnequipBuildGun();
			OnComplete(FAIModOperationResult::Failure(TEXT("MISSING_REFERENCE_Z"),
				TEXT("bIgnoreGroundTrace requires an explicit z - there is no ground trace to fall back to")));
			return;
		}
		if (bIgnoreGroundTrace)
		{
			SyntheticHit.Location = FVector(X, Y, Z);
			SyntheticHit.ImpactPoint = SyntheticHit.Location;
			SyntheticHit.Normal = FVector::UpVector;
			SyntheticHit.ImpactNormal = FVector::UpVector;
			SyntheticHit.bBlockingHit = true;
		}
		else
		{
			const float ZSearchCenter = (Z > -1000000.0f) ? Z : Character->GetActorLocation().Z;
			const FGroundTraceResult GroundTrace = FindGroundAtXY(World, X, Y, ZSearchCenter, Character);
			SyntheticHit = GroundTrace.Hit;
		}
	}

	// Player-independence - same deterministic-look-at-target fix already
	// proven for every other click/snap-driven Construct* function in
	// this file.
	const FRotator DeterministicLook = (SyntheticHit.Location - Character->GetActorLocation()).Rotation();
	if (AController* Controller = Character->GetController())
	{
		Controller->SetControlRotation(DeterministicLook);
	}

	Hologram->UpdateHologramPlacement(SyntheticHit);
	if (TargetStation)
	{
		Hologram->TrySnapToActor(SyntheticHit);
	}
	if (bHasTargetYaw)
	{
		Hologram->SetActorRotation(FRotator(0.0f, TargetYawDegrees, 0.0f));
	}

	struct FPollState
	{
		TWeakObjectPtr<AFGVehicleHologram> Hologram;
		TWeakObjectPtr<AFGCharacterPlayer> Character;
		TWeakObjectPtr<UWorld> World;
		FHitResult SyntheticHit;
		FRotator DeterministicLook;
		bool bHasTargetYaw = false;
		float TargetYawDegrees = 0.0f;
		bool bSnappedToStation = false;
		TFunction<void(const FAIModOperationResult&)> OnComplete;
		int32 AttemptsRemaining = 120;
		int32 AttemptsTaken = 0;
	};
	const TSharedRef<FPollState> PollState = MakeShared<FPollState>();
	PollState->Hologram = Hologram;
	PollState->Character = Character;
	PollState->World = World;
	PollState->SyntheticHit = SyntheticHit;
	PollState->DeterministicLook = DeterministicLook;
	PollState->bHasTargetYaw = bHasTargetYaw;
	PollState->TargetYawDegrees = TargetYawDegrees;
	PollState->bSnappedToStation = (TargetStation != nullptr);
	PollState->OnComplete = MoveTemp(OnComplete);

	const TSharedRef<TFunction<void()>> PollFn = MakeShared<TFunction<void()>>();
	*PollFn = [PollState, PollFn]()
	{
		++PollState->AttemptsTaken;

		AFGVehicleHologram* PollHologram = PollState->Hologram.Get();
		UWorld* PollWorld = PollState->World.Get();
		AFGCharacterPlayer* PollCharacter = PollState->Character.Get();
		if (!IsValid(PollHologram) || !PollWorld)
		{
			UE_LOG(LogAIModAI, Warning, TEXT("ConstructVehicle (deferred): hologram or world became invalid while polling (after %d tick(s)) - nothing built"), PollState->AttemptsTaken);
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

		PollHologram->UpdateHologramPlacement(PollState->SyntheticHit);
		if (PollState->bSnappedToStation)
		{
			PollHologram->TrySnapToActor(PollState->SyntheticHit);
		}
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

		// No bIgnore* bypass flags here, deliberately - the drone-specific
		// disqualifiers (UFGCDMustSnapStation/UFGCDOccupiedStation/
		// UFGCDDroneStationHasDrone) must always block construction, same
		// posture as UFGCDWireTooLong elsewhere in this file. Only
		// UnlimitedResources (a player-controlled mod setting, not a
		// per-call flag) and the always-ignored aim-location disqualifier
		// get any leniency, matching every other Construct* function.
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
			UE_LOG(LogAIModAI, Display, TEXT("ConstructVehicle (deferred, resolved after %d real tick(s)): CanConstruct()=false, NOT constructing - disqualifiers=[%s]"),
				PollState->AttemptsTaken, *DisqualifierSummary);
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
			UE_LOG(LogAIModAI, Error, TEXT("ConstructVehicle (deferred): lost the build state before constructing - aborting, nothing built"));
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("Lost the build state before constructing")));
			return;
		}

		const FVector ConstructLocation = PollHologram->GetActorLocation();
		PollBuildState->InternalConstructHologram(ConstructionID);

		// AFGVehicle is not an AFGBuildable (confirmed from source -
		// AFGVehicle : AFGDriveablePawn, a completely separate hierarchy),
		// so AFGBuildableSubsystem's registry (used for this same
		// confirmation step in every other Construct* function) cannot
		// find it - a real actor-iterator proximity scan over AFGVehicle
		// is the only way to confirm construction genuinely happened,
		// same "never just trust success" posture as everywhere else in
		// this file.
		FString ConstructedVehicleId;
		if (PollWorld)
		{
			float BestDistSq = TNumericLimits<float>::Max();
			AFGVehicle* BestMatch = nullptr;
			for (TActorIterator<AFGVehicle> It(PollWorld); It; ++It)
			{
				if (!IsValid(*It)) { continue; }
				const float DistSq = FVector::DistSquared(It->GetActorLocation(), ConstructLocation);
				if (DistSq < BestDistSq)
				{
					BestDistSq = DistSq;
					BestMatch = *It;
				}
			}
			if (BestMatch && BestDistSq < FMath::Square(500.0f))
			{
				ConstructedVehicleId = BestMatch->GetPathName();
			}
		}

		UE_LOG(LogAIModAI, Display, TEXT("ConstructVehicle (deferred, resolved after %d real tick(s)): construction attempted via InternalConstructHologram - id=%s"),
			PollState->AttemptsTaken, *ConstructedVehicleId);

		if (IsValid(PollCharacter))
		{
			PollCharacter->UnequipBuildGun();
		}

		if (ConstructedVehicleId.IsEmpty())
		{
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("CONSTRUCTION_UNCONFIRMED"), TEXT("InternalConstructHologram was called but no real AFGVehicle was found near the construct location afterward")));
			return;
		}

		PollState->OnComplete(FAIModOperationResult::SuccessWithBuildableId(ConstructedVehicleId));
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

	// Real bug found and fixed 2026-08-30: this previously gated the
	// actual item lookup behind IsCentralStorageBuilt() (which reports
	// mCentralStorages.Num() > 0, a SEPARATE container-registration
	// bookkeeping array) - so on a save with real, already-built
	// AFGCentralStorageContainer buildables (confirmed live via
	// world.buildables), IsCentralStorageBuilt() still reported false
	// (registration apparently doesn't reliably re-fire for containers
	// loaded from a save, not just ones built fresh this session - stub
	// .cpp source, exact mechanism unconfirmed) and this function never
	// even attempted the real item lookup, silently reporting an empty
	// Depot the whole time. GetAllItemsFromCentralStorage() has no
	// documented precondition and is safe to call unconditionally -
	// call it directly instead of trusting the unreliable gate.
	TArray<FItemAmount> AllItems;
	if (CentralStorage)
	{
		CentralStorage->GetAllItemsFromCentralStorage(AllItems);
	}

	TArray<TSharedPtr<FJsonValue>> ItemsArray;
	for (const FItemAmount& Item : AllItems)
	{
		const TSharedRef<FJsonObject> ItemObject = MakeShared<FJsonObject>();
		ItemObject->SetStringField(TEXT("itemClass"), Item.ItemClass ? Item.ItemClass->GetPathName() : TEXT(""));
		ItemObject->SetStringField(TEXT("itemName"), Item.ItemClass ? Item.ItemClass->GetName() : TEXT(""));
		ItemObject->SetNumberField(TEXT("amount"), Item.Amount);
		ItemsArray.Add(MakeShared<FJsonValueObject>(ItemObject));
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetBoolField(TEXT("isCentralStorageBuilt"), CentralStorage && CentralStorage->IsCentralStorageBuilt());
	RootObject->SetArrayField(TEXT("items"), ItemsArray);

	const FString JsonString = WriteCondensedJson(RootObject);

	UE_LOG(LogAIModAI, Display, TEXT("LogCentralStorageAsJson: %d item type(s)"), ItemsArray.Num());

	return JsonString;
}

FString UAIModFunctionLibrary::LogPlayerInventoryAsJson(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		UE_LOG(LogAIModAI, Warning, TEXT("LogPlayerInventoryAsJson: no valid world context"));
		return TEXT("{}");
	}

	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
	UFGInventoryComponent* PlayerInventory = Character ? Character->GetInventory() : nullptr;

	// Aggregates by item class across every stack (a real inventory can
	// hold the same item split across multiple slots) - reports one
	// entry per distinct item, not a raw per-slot dump, matching
	// LogCentralStorageAsJson's shape so both can be summed by callers
	// wanting a "combined" carried+Depot count (2026-08-30, user
	// request - verifying a suspected inventory-refund bug needs a
	// reliable before/after carried count, which no RPC previously
	// exposed at all; only Depot contents were readable).
	TMap<TSubclassOf<UFGItemDescriptor>, int32> Totals;
	if (PlayerInventory)
	{
		TArray<FInventoryStack> Stacks;
		PlayerInventory->GetInventoryStacks(Stacks, false);
		for (const FInventoryStack& Stack : Stacks)
		{
			if (!Stack.HasItems())
			{
				continue;
			}
			const TSubclassOf<UFGItemDescriptor> ItemClass = Stack.Item.GetItemClass();
			Totals.FindOrAdd(ItemClass) += Stack.NumItems;
		}
	}

	TArray<TSharedPtr<FJsonValue>> ItemsArray;
	for (const TPair<TSubclassOf<UFGItemDescriptor>, int32>& Pair : Totals)
	{
		const TSharedRef<FJsonObject> ItemObject = MakeShared<FJsonObject>();
		ItemObject->SetStringField(TEXT("itemClass"), Pair.Key ? Pair.Key->GetPathName() : TEXT(""));
		ItemObject->SetStringField(TEXT("itemName"), Pair.Key ? Pair.Key->GetName() : TEXT(""));
		ItemObject->SetNumberField(TEXT("amount"), Pair.Value);
		ItemsArray.Add(MakeShared<FJsonValueObject>(ItemObject));
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetBoolField(TEXT("hasPlayer"), PlayerInventory != nullptr);
	RootObject->SetArrayField(TEXT("items"), ItemsArray);

	const FString JsonString = WriteCondensedJson(RootObject);

	UE_LOG(LogAIModAI, Display, TEXT("LogPlayerInventoryAsJson: %d item type(s)"), ItemsArray.Num());

	return JsonString;
}

void UAIModSaveGameCallbackProxy::HandleSaveComplete(bool bSuccess, const FText& ErrorMessage)
{
	if (OnComplete)
	{
		OnComplete(bSuccess
			? FAIModOperationResult::Success()
			: FAIModOperationResult::Failure(TEXT("SAVE_FAILED"), ErrorMessage.ToString()));
	}
	RemoveFromRoot();
}

void UAIModFunctionLibrary::SaveGame(UObject* WorldContextObject, const FString& SaveName, TFunction<void(const FAIModOperationResult&)> OnComplete)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context")));
		return;
	}

	AFGPlayerControllerBase* PlayerController = Cast<AFGPlayerControllerBase>(UGameplayStatics::GetPlayerController(World, 0));
	if (!PlayerController)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_PLAYER"), TEXT("No local AFGPlayerControllerBase (player index 0)")));
		return;
	}

	AFGAdminInterface* AdminInterface = PlayerController->GetAdminInterface();
	if (!AdminInterface)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_ADMIN_INTERFACE"), TEXT("AFGPlayerControllerBase::GetAdminInterface() returned null")));
		return;
	}

	FString ResolvedSaveName = SaveName;
	if (ResolvedSaveName.IsEmpty())
	{
		const AFGGameState* GameState = World->GetGameState<AFGGameState>();
		ResolvedSaveName = GameState ? GameState->GetSessionName() : FString();
		if (ResolvedSaveName.IsEmpty())
		{
			OnComplete(FAIModOperationResult::Failure(TEXT("NO_SESSION_NAME"), TEXT("saveName was empty and the current session has no name to fall back to")));
			return;
		}
	}

	UAIModSaveGameCallbackProxy* Proxy = NewObject<UAIModSaveGameCallbackProxy>();
	Proxy->AddToRoot();
	Proxy->OnComplete = OnComplete;

	FOnAdminSaveGameComplete Delegate;
	Delegate.BindDynamic(Proxy, &UAIModSaveGameCallbackProxy::HandleSaveComplete);

	UE_LOG(LogAIModAI, Display, TEXT("SaveGame: saving locally as '%s'"), *ResolvedSaveName);
	AdminInterface->SaveGame(true, ResolvedSaveName, Delegate);
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

	// Same fix as LogCentralStorageAsJson (2026-08-30): don't gate on
	// IsCentralStorageBuilt() - confirmed live unreliable even with real,
	// already-built AFGCentralStorageContainer buildables in the world.
	// TryRemoveItemsFromCentralStorage has no documented precondition and
	// itself safely clamps to whatever is actually available.
	AFGCentralStorageSubsystem* CentralStorage = AFGCentralStorageSubsystem::Get(World);
	if (!CentralStorage)
	{
		return FAIModOperationResult::Failure(TEXT("NO_CENTRAL_STORAGE"), TEXT("No AFGCentralStorageSubsystem found for this world"));
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
	//
	// FindPowerConnectionPair (2026-08-31 fix) - a joint, type-aware
	// selection over both buildables at once, see its own comment for
	// why a Power Tower's dual PCT_PowerTower/PCT_Default connectors
	// need this instead of two independent single-buildable lookups.
	UFGPowerConnectionComponent* ConnectionA = nullptr;
	UFGPowerConnectionComponent* ConnectionB = nullptr;
	if (!FindPowerConnectionPair(BuildableA, BuildableB, ConnectionA, ConnectionB))
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_POWER_CONNECTION"),
			FString::Printf(TEXT("No compatible free power connection pair between '%s' and '%s' - connection types (Default/PowerTower/Any) must match, or one side must be Any"), *BuildableIdA, *BuildableIdB)));
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

namespace
{
	FString PowerPoleTypeToString(EPowerPoleType Type)
	{
		switch (Type)
		{
		case EPowerPoleType::PPT_POLE: return TEXT("Pole");
		case EPowerPoleType::PPT_WALL: return TEXT("WallPlug");
		case EPowerPoleType::PPT_WALL_DOUBLE: return TEXT("WallPlugDouble");
		case EPowerPoleType::PPT_TOWER: return TEXT("PowerTower");
		default: return TEXT("Unknown");
		}
	}

	FString PowerConnectionTypeToString(EPowerConnectionType Type)
	{
		switch (Type)
		{
		case EPowerConnectionType::PCT_Default: return TEXT("Default");
		case EPowerConnectionType::PCT_PowerTower: return TEXT("PowerTower");
		case EPowerConnectionType::PCT_Any: return TEXT("Any");
		default: return TEXT("Unknown");
		}
	}
}

// See LogPowerPolesAsJson's doc comment in the header for the real
// AFGBuildablePowerPole/EPowerPoleType/EPowerConnectionType sourcing and
// how this connects to the FindPowerConnectionPair bugfix above.
FString UAIModFunctionLibrary::LogPowerPolesAsJson(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		UE_LOG(LogAIModAI, Warning, TEXT("LogPowerPolesAsJson: no valid world context"));
		return TEXT("{}");
	}

	TArray<TSharedPtr<FJsonValue>> PolesJsonArray;
	for (TActorIterator<AFGBuildablePowerPole> It(World); It; ++It)
	{
		AFGBuildablePowerPole* Pole = *It;
		if (!IsValid(Pole))
		{
			continue;
		}

		TArray<TSharedPtr<FJsonValue>> ConnectionsJsonArray;
		for (UFGPowerConnectionComponent* Connection : Pole->GetPowerConnections())
		{
			if (!IsValid(Connection)) { continue; }

			const TSharedRef<FJsonObject> ConnectionObject = MakeShared<FJsonObject>();
			ConnectionObject->SetStringField(TEXT("powerConnectionType"), PowerConnectionTypeToString(Connection->GetPowerConnectionType()));
			ConnectionObject->SetNumberField(TEXT("numFreeConnections"), Connection->GetNumFreeConnections());
			ConnectionsJsonArray.Add(MakeShared<FJsonValueObject>(ConnectionObject));
		}

		const TSharedRef<FJsonObject> PoleObject = MakeShared<FJsonObject>();
		PoleObject->SetStringField(TEXT("id"), Pole->GetPathName());
		PoleObject->SetStringField(TEXT("buildableClass"), Pole->GetClass()->GetPathName());
		PoleObject->SetStringField(TEXT("powerPoleType"), PowerPoleTypeToString(Pole->GetPowerPoleType()));
		PoleObject->SetBoolField(TEXT("hasPower"), Pole->HasPower());
		PoleObject->SetNumberField(TEXT("powerTowerWireMaxLength"), Pole->GetPowerTowerWireMaxLength());
		PoleObject->SetArrayField(TEXT("connections"), ConnectionsJsonArray);

		PolesJsonArray.Add(MakeShared<FJsonValueObject>(PoleObject));
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetArrayField(TEXT("powerPoles"), PolesJsonArray);

	const FString JsonString = WriteCondensedJson(RootObject);

	UE_LOG(LogAIModAI, Display, TEXT("LogPowerPolesAsJson: %d pole(s)"), PolesJsonArray.Num());

	return JsonString;
}

// See LogPriorityPowerSwitchesAsJson's doc comment in the header for the
// real GetInfo()/circuit-group/building-tag sourcing and the "no
// separate Smart Power Switch buildable" finding.
FString UAIModFunctionLibrary::LogPriorityPowerSwitchesAsJson(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		UE_LOG(LogAIModAI, Warning, TEXT("LogPriorityPowerSwitchesAsJson: no valid world context"));
		return TEXT("{}");
	}

	TArray<TSharedPtr<FJsonValue>> SwitchesJsonArray;
	for (TActorIterator<AFGBuildablePriorityPowerSwitch> It(World); It; ++It)
	{
		AFGBuildablePriorityPowerSwitch* Switch = *It;
		if (!IsValid(Switch))
		{
			continue;
		}

		const TSharedRef<FJsonObject> SwitchObject = MakeShared<FJsonObject>();
		SwitchObject->SetStringField(TEXT("id"), Switch->GetPathName());
		SwitchObject->SetStringField(TEXT("buildableClass"), Switch->GetClass()->GetPathName());
		SwitchObject->SetNumberField(TEXT("priority"), Switch->GetPriority());
		SwitchObject->SetBoolField(TEXT("isSwitchOn"), Switch->IsSwitchOn());
		SwitchObject->SetBoolField(TEXT("isSwitchConnected"), Switch->IsSwitchConnected());
		SwitchObject->SetBoolField(TEXT("hasBuildingTag"), IFGBuildingTagInterface::Execute_HasBuildingTag(Switch));
		SwitchObject->SetStringField(TEXT("buildingTag"), IFGBuildingTagInterface::Execute_GetBuildingTag(Switch));

		if (const AFGPriorityPowerSwitchInfo* Info = Switch->GetInfo())
		{
			SwitchObject->SetStringField(TEXT("switchName"), Info->GetSwitchName());
			SwitchObject->SetNumberField(TEXT("circuitGroupID0"), Info->GetCircuitGroupID0());
			SwitchObject->SetNumberField(TEXT("circuitGroupID1"), Info->GetCircuitGroupID1());
		}
		else
		{
			SwitchObject->SetStringField(TEXT("switchName"), FString());
			SwitchObject->SetNumberField(TEXT("circuitGroupID0"), -1);
			SwitchObject->SetNumberField(TEXT("circuitGroupID1"), -1);
		}

		SwitchesJsonArray.Add(MakeShared<FJsonValueObject>(SwitchObject));
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetArrayField(TEXT("prioritySwitches"), SwitchesJsonArray);

	const FString JsonString = WriteCondensedJson(RootObject);

	UE_LOG(LogAIModAI, Display, TEXT("LogPriorityPowerSwitchesAsJson: %d switch(es)"), SwitchesJsonArray.Num());

	return JsonString;
}

// See SetPowerSwitchOn's doc comment in the header for why this targets
// the base AFGBuildableCircuitSwitch rather than just the priority
// subclass.
FAIModOperationResult UAIModFunctionLibrary::SetPowerSwitchOn(UObject* WorldContextObject, const FString& BuildableId, bool bSwitchOn)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}

	AFGBuildable* Buildable = FindBuildableById(World, BuildableId);
	if (!Buildable)
	{
		return FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No buildable found with id '%s'"), *BuildableId));
	}

	AFGBuildableCircuitSwitch* Switch = Cast<AFGBuildableCircuitSwitch>(Buildable);
	if (!Switch)
	{
		return FAIModOperationResult::Failure(TEXT("WRONG_TYPE"), FString::Printf(TEXT("'%s' is a %s, not an AFGBuildableCircuitSwitch"), *BuildableId, *Buildable->GetClass()->GetName()));
	}

	const bool bWasOn = Switch->IsSwitchOn();
	Switch->SetSwitchOn(bSwitchOn);

	const TSharedRef<FJsonObject> DetailObject = MakeShared<FJsonObject>();
	DetailObject->SetBoolField(TEXT("wasOn"), bWasOn);
	DetailObject->SetBoolField(TEXT("isOn"), Switch->IsSwitchOn());

	UE_LOG(LogAIModAI, Display, TEXT("SetPowerSwitchOn: '%s' %s -> %s"), *BuildableId, bWasOn ? TEXT("on") : TEXT("off"), Switch->IsSwitchOn() ? TEXT("on") : TEXT("off"));

	FAIModOperationResult Result = FAIModOperationResult::Success();
	Result.ResultDetailJson = WriteCondensedJson(DetailObject);
	return Result;
}

// See SetPriorityPowerSwitchPriority's doc comment in the header for the
// real SetPriority() semantics, quoted from source.
FAIModOperationResult UAIModFunctionLibrary::SetPriorityPowerSwitchPriority(UObject* WorldContextObject, const FString& BuildableId, int32 Priority)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}

	AFGBuildable* Buildable = FindBuildableById(World, BuildableId);
	if (!Buildable)
	{
		return FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No buildable found with id '%s'"), *BuildableId));
	}

	AFGBuildablePriorityPowerSwitch* Switch = Cast<AFGBuildablePriorityPowerSwitch>(Buildable);
	if (!Switch)
	{
		return FAIModOperationResult::Failure(TEXT("WRONG_TYPE"), FString::Printf(TEXT("'%s' is a %s, not an AFGBuildablePriorityPowerSwitch"), *BuildableId, *Buildable->GetClass()->GetName()));
	}

	const int32 OldPriority = Switch->GetPriority();
	Switch->SetPriority(Priority);

	const TSharedRef<FJsonObject> DetailObject = MakeShared<FJsonObject>();
	DetailObject->SetNumberField(TEXT("oldPriority"), OldPriority);
	DetailObject->SetNumberField(TEXT("newPriority"), Switch->GetPriority());

	UE_LOG(LogAIModAI, Display, TEXT("SetPriorityPowerSwitchPriority: '%s' %d -> %d"), *BuildableId, OldPriority, Switch->GetPriority());

	FAIModOperationResult Result = FAIModOperationResult::Success();
	Result.ResultDetailJson = WriteCondensedJson(DetailObject);
	return Result;
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

FString UAIModFunctionLibrary::LogPipelinePumpTiersAsJson(UObject* WorldContextObject)
{
	// Read-only telemetry, no World/player needed - mirrors
	// LogPipelineTiersAsJson's structure. Recipe_PipelinePump (Mk1) and
	// Recipe_PipelinePumpMK2 (capital "MK2", confirmed from the real
	// asset filenames on disk, matching the pipe tiers' own naming) are
	// the two real pump tiers.
	//
	// Recipe_Valve (2026-08-31, offline research per explicit user
	// request) is included here too, NOT a separate tier list - confirmed
	// directly from the binary asset (Build_Valve.uasset references the
	// literal class name "FGBuildablePipelinePump", grepped from the
	// .uasset itself) that the in-game Valve IS a Blueprint variant of
	// AFGBuildablePipelinePump, not a separate C++ class. This also
	// explains why AFGBuildablePipelinePump.h's own SetUserFlowLimit()
	// doc comment already used valve terminology ("Set this to -1 to use
	// the max limit, i.e. valve is fully opened") before any of this was
	// investigated - the class was always dual-purpose. A "kind" field
	// distinguishes Pump vs Valve entries for the caller since the RPC
	// name itself still says "pump" tiers.
	static const TCHAR* PumpTierRecipePaths[] = {
		TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PipelinePump.Recipe_PipelinePump_C"),
		TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PipelinePumpMK2.Recipe_PipelinePumpMK2_C"),
	};
	static const TCHAR* ValveRecipePath = TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_Valve.Recipe_Valve_C");

	TArray<TSharedPtr<FJsonValue>> TierJsonArray;
	for (const TCHAR* RecipePath : PumpTierRecipePaths)
	{
		const TSubclassOf<AFGBuildable> BuildableClass = ResolveBuildableClassForRecipe(RecipePath);
		const AFGBuildablePipelinePump* PumpCDO = BuildableClass ? Cast<AFGBuildablePipelinePump>(BuildableClass->GetDefaultObject()) : nullptr;
		if (!PumpCDO)
		{
			UE_LOG(LogAIModAI, Warning, TEXT("LogPipelinePumpTiersAsJson: could not resolve a AFGBuildablePipelinePump CDO for '%s' - omitting"), RecipePath);
			continue;
		}

		const TSharedRef<FJsonObject> TierObject = MakeShared<FJsonObject>();
		TierObject->SetStringField(TEXT("kind"), TEXT("Pump"));
		TierObject->SetStringField(TEXT("recipeClass"), RecipePath);
		TierObject->SetStringField(TEXT("buildableClass"), BuildableClass->GetPathName());
		// All three real public BlueprintPure getters - no reflection
		// needed, unlike the pipe tier's maxSplineLength/bendRadius/
		// minBendRadius. Units per AFGBuildablePipelinePump.h's own doc
		// comments: headlift in meters, flow in [m^3/s].
		TierObject->SetNumberField(TEXT("maxHeadLift"), PumpCDO->GetMaxHeadLift());
		TierObject->SetNumberField(TEXT("designHeadLift"), PumpCDO->GetDesignHeadLift());
		TierObject->SetNumberField(TEXT("defaultFlowLimit"), PumpCDO->GetDefaultFlowLimit());

		TierJsonArray.Add(MakeShared<FJsonValueObject>(TierObject));
	}

	{
		const TSubclassOf<AFGBuildable> ValveBuildableClass = ResolveBuildableClassForRecipe(ValveRecipePath);
		const AFGBuildablePipelinePump* ValveCDO = ValveBuildableClass ? Cast<AFGBuildablePipelinePump>(ValveBuildableClass->GetDefaultObject()) : nullptr;
		if (ValveCDO)
		{
			const TSharedRef<FJsonObject> ValveObject = MakeShared<FJsonObject>();
			ValveObject->SetStringField(TEXT("kind"), TEXT("Valve"));
			ValveObject->SetStringField(TEXT("recipeClass"), ValveRecipePath);
			ValveObject->SetStringField(TEXT("buildableClass"), ValveBuildableClass->GetPathName());
			ValveObject->SetNumberField(TEXT("maxHeadLift"), ValveCDO->GetMaxHeadLift());
			ValveObject->SetNumberField(TEXT("designHeadLift"), ValveCDO->GetDesignHeadLift());
			ValveObject->SetNumberField(TEXT("defaultFlowLimit"), ValveCDO->GetDefaultFlowLimit());
			TierJsonArray.Add(MakeShared<FJsonValueObject>(ValveObject));
		}
		else
		{
			UE_LOG(LogAIModAI, Warning, TEXT("LogPipelinePumpTiersAsJson: could not resolve a AFGBuildablePipelinePump CDO for the Valve recipe '%s' - omitting"), ValveRecipePath);
		}
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetArrayField(TEXT("tiers"), TierJsonArray);

	FString JsonString;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonString);
	FJsonSerializer::Serialize(RootObject, Writer);

	UE_LOG(LogAIModAI, Display, TEXT("LogPipelinePumpTiersAsJson: %s"), *JsonString);

	return JsonString;
}

FString UAIModFunctionLibrary::LogPipeFluidBoxesAsJson(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		UE_LOG(LogAIModAI, Warning, TEXT("LogPipeFluidBoxesAsJson: no valid world context"));
		return TEXT("{}");
	}

	// Scoped to AFGBuildablePipeline (real pipe SEGMENTS) specifically -
	// IFGFluidIntegrantInterface is also implemented by pumps/storage
	// tanks/etc, but segments are what the user's own volume/fill
	// question was about. Widening to other fluid integrants is a real,
	// separate future addition if needed, not done here.
	TArray<TSharedPtr<FJsonValue>> BoxJsonArray;
	for (TActorIterator<AFGBuildablePipeline> It(World); It; ++It)
	{
		AFGBuildablePipeline* Pipe = *It;
		if (!IsValid(Pipe))
		{
			continue;
		}
		FFluidBox* Box = Pipe->GetFluidBox();
		if (!Box)
		{
			continue;
		}

		const TSharedRef<FJsonObject> BoxObject = MakeShared<FJsonObject>();
		BoxObject->SetStringField(TEXT("id"), Pipe->GetPathName());
		// GetLength() is documented "Length of the pipe in centimeters"
		// (FGBuildablePipeBase.h) - real unit, paired here with the
		// fluid box's own real volume for the length-to-volume
		// relationship the user asked about.
		BoxObject->SetNumberField(TEXT("lengthCm"), Pipe->GetLength());
		BoxObject->SetNumberField(TEXT("contentM3"), Box->Content);
		BoxObject->SetNumberField(TEXT("maxContentM3"), Box->MaxContent);
		BoxObject->SetNumberField(TEXT("fillPct"), Box->MaxContent > 0.f ? (Box->Content / Box->MaxContent) : 0.f);
		BoxObject->SetNumberField(TEXT("maxOverfillPct"), Box->MaxOverfillPct);
		BoxObject->SetNumberField(TEXT("flowThrough"), Box->FlowThrough);
		BoxObject->SetNumberField(TEXT("flowFill"), Box->FlowFill);
		BoxObject->SetNumberField(TEXT("flowDrain"), Box->FlowDrain);
		BoxObject->SetNumberField(TEXT("flowLimit"), Box->FlowLimit);
		BoxObject->SetNumberField(TEXT("pressureColumn"), Box->PressureColumn);
		BoxObject->SetNumberField(TEXT("elevationPressureColumn"), Box->ElevationPressureColumn);
		BoxObject->SetNumberField(TEXT("addedPressure"), Box->GetCurrentAddedPressure());
		BoxObject->SetNumberField(TEXT("pressureGroup"), Box->PressureGroup);
		BoxObject->SetNumberField(TEXT("z"), Box->Z);

		BoxJsonArray.Add(MakeShared<FJsonValueObject>(BoxObject));
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetArrayField(TEXT("pipes"), BoxJsonArray);

	const FString JsonString = WriteCondensedJson(RootObject);

	UE_LOG(LogAIModAI, Display, TEXT("LogPipeFluidBoxesAsJson: %d pipe segment(s)"), BoxJsonArray.Num());

	return JsonString;
}

FString UAIModFunctionLibrary::LogPipeReservoirTiersAsJson(UObject* WorldContextObject)
{
	// Read-only telemetry, no World/player needed - mirrors
	// LogPipelinePumpTiersAsJson's structure. Both recipes confirmed to
	// resolve to AFGBuildablePipeReservoir via a direct grep of each
	// .uasset's binary for the embedded class name string.
	static const TCHAR* TierRecipePaths[] = {
		TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PipeStorageTank.Recipe_PipeStorageTank_C"),
		TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_IndustrialTank.Recipe_IndustrialTank_C"),
	};

	TArray<TSharedPtr<FJsonValue>> TierJsonArray;
	for (const TCHAR* RecipePath : TierRecipePaths)
	{
		const TSubclassOf<AFGBuildable> BuildableClass = ResolveBuildableClassForRecipe(RecipePath);
		const AFGBuildablePipeReservoir* ReservoirCDO = BuildableClass ? Cast<AFGBuildablePipeReservoir>(BuildableClass->GetDefaultObject()) : nullptr;
		if (!ReservoirCDO)
		{
			UE_LOG(LogAIModAI, Warning, TEXT("LogPipeReservoirTiersAsJson: could not resolve a AFGBuildablePipeReservoir CDO for '%s' - omitting"), RecipePath);
			continue;
		}

		const TSharedRef<FJsonObject> TierObject = MakeShared<FJsonObject>();
		TierObject->SetStringField(TEXT("recipeClass"), RecipePath);
		TierObject->SetStringField(TEXT("buildableClass"), BuildableClass->GetPathName());
		// Real public BlueprintPure getters - no reflection needed.
		// Units per AFGBuildablePipeReservoir.h's own doc comments:
		// capacity in [m^3], flow limit in [m^3/s].
		TierObject->SetNumberField(TEXT("maxContentM3"), ReservoirCDO->GetFluidContentMax());
		TierObject->SetNumberField(TEXT("flowLimit"), ReservoirCDO->GetFlowLimit());

		TierJsonArray.Add(MakeShared<FJsonValueObject>(TierObject));
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetArrayField(TEXT("tiers"), TierJsonArray);

	const FString JsonString = WriteCondensedJson(RootObject);

	UE_LOG(LogAIModAI, Display, TEXT("LogPipeReservoirTiersAsJson: %s"), *JsonString);

	return JsonString;
}

FString UAIModFunctionLibrary::LogTrainCargoPlatformsAsJson(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		UE_LOG(LogAIModAI, Warning, TEXT("LogTrainCargoPlatformsAsJson: no valid world context"));
		return TEXT("{}");
	}

	TArray<TSharedPtr<FJsonValue>> PlatformsJsonArray;
	for (TActorIterator<AFGBuildableTrainPlatformCargo> It(World); It; ++It)
	{
		AFGBuildableTrainPlatformCargo* Platform = *It;
		if (!IsValid(Platform))
		{
			continue;
		}

		const TSharedRef<FJsonObject> PlatformObject = MakeShared<FJsonObject>();
		PlatformObject->SetStringField(TEXT("id"), Platform->GetPathName());
		PlatformObject->SetStringField(TEXT("buildableClass"), Platform->GetClass()->GetPathName());

		// mFreightCargoType has no public getter on the PLATFORM class
		// (unlike AFGFreightWagon, which has GetFreightCargoType()) -
		// read via FindFProperty<FEnumProperty> reflection, the first
		// enum (not float) field this codebase has read this way. NOT
		// YET LIVE-VERIFIED that this correctly resolves - if it fails
		// to resolve, "freightCargoType" is simply omitted rather than
		// erroring the whole call.
		if (const FEnumProperty* CargoTypeProperty = FindFProperty<FEnumProperty>(Platform->GetClass(), TEXT("mFreightCargoType")))
		{
			const void* ValuePtr = CargoTypeProperty->ContainerPtrToValuePtr<void>(Platform);
			const int64 RawValue = CargoTypeProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(ValuePtr);
			const EFreightCargoType CargoType = static_cast<EFreightCargoType>(RawValue);
			FString CargoTypeString = TEXT("None");
			switch (CargoType)
			{
			case EFreightCargoType::FCT_Standard: CargoTypeString = TEXT("Standard"); break;
			case EFreightCargoType::FCT_Liquid: CargoTypeString = TEXT("Liquid"); break;
			default: break;
			}
			PlatformObject->SetStringField(TEXT("freightCargoType"), CargoTypeString);
		}

		// GetOutflowRate()/GetInflowRate() [m^3/s] - real public getters,
		// own doc comments say "Only valid for Liquid Freight Platforms"
		// - the key data for observing a real long-distance fluid-by-
		// rail network's station-side load/unload rate.
		PlatformObject->SetNumberField(TEXT("outflowRate"), Platform->GetOutflowRate());
		PlatformObject->SetNumberField(TEXT("inflowRate"), Platform->GetInflowRate());
		PlatformObject->SetBoolField(TEXT("isInLoadMode"), Platform->GetIsInLoadMode());
		PlatformObject->SetBoolField(TEXT("isLoadUnloading"), Platform->IsLoadUnloading());
		PlatformObject->SetBoolField(TEXT("isFullLoad"), Platform->IsFullLoad() != 0);
		PlatformObject->SetBoolField(TEXT("isFullUnload"), Platform->IsFullUnload() != 0);

		AFGRailroadVehicle* DockedVehicle = Platform->GetDockedActor();
		PlatformObject->SetStringField(TEXT("dockedVehicleId"), IsValid(DockedVehicle) ? DockedVehicle->GetPathName() : FString());

		PlatformsJsonArray.Add(MakeShared<FJsonValueObject>(PlatformObject));
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetArrayField(TEXT("platforms"), PlatformsJsonArray);

	const FString JsonString = WriteCondensedJson(RootObject);

	UE_LOG(LogAIModAI, Display, TEXT("LogTrainCargoPlatformsAsJson: %d cargo platform(s)"), PlatformsJsonArray.Num());

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

// See LogSplitterSortRulesAsJson's doc comment in the header for the
// real GetSortRules()/UFGWildCardDescriptor sourcing.
FString UAIModFunctionLibrary::LogSplitterSortRulesAsJson(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		UE_LOG(LogAIModAI, Warning, TEXT("LogSplitterSortRulesAsJson: no valid world context"));
		return TEXT("{}");
	}

	TArray<TSharedPtr<FJsonValue>> SplittersJsonArray;
	for (TActorIterator<AFGBuildableSplitterSmart> It(World); It; ++It)
	{
		AFGBuildableSplitterSmart* Splitter = *It;
		if (!IsValid(Splitter))
		{
			continue;
		}

		TArray<TSharedPtr<FJsonValue>> RulesJsonArray;
		for (const FSplitterSortRule& Rule : Splitter->GetSortRules())
		{
			const TSharedRef<FJsonObject> RuleObject = MakeShared<FJsonObject>();
			RuleObject->SetNumberField(TEXT("outputIndex"), Rule.OutputIndex);
			RuleObject->SetStringField(TEXT("itemClass"), Rule.ItemClass ? Rule.ItemClass->GetPathName() : FString());
			RuleObject->SetStringField(TEXT("itemName"), Rule.ItemClass ? UFGItemDescriptor::GetItemName(Rule.ItemClass).ToString() : FString());
			RuleObject->SetBoolField(TEXT("isWildcard"), Rule.ItemClass && Rule.ItemClass->IsChildOf(UFGWildCardDescriptor::StaticClass()));
			RulesJsonArray.Add(MakeShared<FJsonValueObject>(RuleObject));
		}

		const TSharedRef<FJsonObject> SplitterObject = MakeShared<FJsonObject>();
		SplitterObject->SetStringField(TEXT("id"), Splitter->GetPathName());
		SplitterObject->SetStringField(TEXT("buildableClass"), Splitter->GetClass()->GetPathName());
		SplitterObject->SetNumberField(TEXT("maxNumSortRules"), Splitter->GetMaxNumSortRules());
		SplitterObject->SetArrayField(TEXT("sortRules"), RulesJsonArray);

		SplittersJsonArray.Add(MakeShared<FJsonValueObject>(SplitterObject));
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetArrayField(TEXT("splitters"), SplittersJsonArray);

	const FString JsonString = WriteCondensedJson(RootObject);

	UE_LOG(LogAIModAI, Display, TEXT("LogSplitterSortRulesAsJson: %d splitter(s)"), SplittersJsonArray.Num());

	return JsonString;
}

// See SetSplitterSortRules's doc comment in the header for the real
// SetSortRules()/wildcard-sentinel sourcing.
FAIModOperationResult UAIModFunctionLibrary::SetSplitterSortRules(UObject* WorldContextObject, const FString& BuildableId, const FString& RulesJson)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}

	AFGBuildable* Buildable = FindBuildableById(World, BuildableId);
	if (!Buildable)
	{
		return FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No buildable found with id '%s'"), *BuildableId));
	}

	AFGBuildableSplitterSmart* Splitter = Cast<AFGBuildableSplitterSmart>(Buildable);
	if (!Splitter)
	{
		return FAIModOperationResult::Failure(TEXT("WRONG_TYPE"), FString::Printf(TEXT("'%s' is a %s, not an AFGBuildableSplitterSmart (only Smart/Programmable splitters support sort rules)"), *BuildableId, *Buildable->GetClass()->GetName()));
	}

	TArray<TSharedPtr<FJsonValue>> RulesArray;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RulesJson);
	if (!FJsonSerializer::Deserialize(Reader, RulesArray))
	{
		return FAIModOperationResult::Failure(TEXT("INVALID_REQUEST"), TEXT("rules must be a JSON array"));
	}

	TArray<FSplitterSortRule> NewRules;
	for (const TSharedPtr<FJsonValue>& RuleValue : RulesArray)
	{
		const TSharedPtr<FJsonObject> RuleObject = RuleValue.IsValid() ? RuleValue->AsObject() : nullptr;
		if (!RuleObject.IsValid())
		{
			return FAIModOperationResult::Failure(TEXT("INVALID_REQUEST"), TEXT("Each rule must be a JSON object"));
		}

		double OutputIndex = 0.0;
		if (!RuleObject->TryGetNumberField(TEXT("outputIndex"), OutputIndex))
		{
			return FAIModOperationResult::Failure(TEXT("INVALID_REQUEST"), TEXT("Each rule requires a numeric outputIndex"));
		}

		FString ItemClassPath;
		RuleObject->TryGetStringField(TEXT("itemClass"), ItemClassPath);

		TSubclassOf<UFGItemDescriptor> ItemClass;
		if (ItemClassPath.IsEmpty() || ItemClassPath.Equals(TEXT("Wildcard"), ESearchCase::IgnoreCase))
		{
			ItemClass = UFGWildCardDescriptor::StaticClass();
		}
		else
		{
			UClass* ResolvedClass = LoadObject<UClass>(nullptr, *ItemClassPath);
			if (!ResolvedClass || !ResolvedClass->IsChildOf(UFGItemDescriptor::StaticClass()))
			{
				return FAIModOperationResult::Failure(TEXT("INVALID_ITEM_CLASS"), FString::Printf(TEXT("'%s' did not resolve to a UFGItemDescriptor subclass"), *ItemClassPath));
			}
			ItemClass = ResolvedClass;
		}

		NewRules.Add(FSplitterSortRule(ItemClass, static_cast<int32>(OutputIndex)));
	}

	Splitter->SetSortRules(NewRules);

	const TSharedRef<FJsonObject> DetailObject = MakeShared<FJsonObject>();
	DetailObject->SetNumberField(TEXT("numRules"), Splitter->GetSortRules().Num());

	UE_LOG(LogAIModAI, Display, TEXT("SetSplitterSortRules: '%s' now has %d rule(s)"), *BuildableId, Splitter->GetSortRules().Num());

	FAIModOperationResult Result = FAIModOperationResult::Success();
	Result.ResultDetailJson = WriteCondensedJson(DetailObject);
	return Result;
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

	// "Christmas" is FactoryGame's own internal name for the event
	// players see in-game as "FICSMAS" - deliberately not renamed here,
	// this string must match what UFGRecipe::GetRelevantEvents() and
	// AFGEventSubsystem::GetCurrentEvents() actually mean.
	FString EventToString(EEvents Event)
	{
		switch (Event)
		{
		case EEvents::EV_Christmas: return TEXT("Christmas");
		case EEvents::EV_Birthday: return TEXT("Anniversary");
		case EEvents::EV_CSSBirthday: return TEXT("CSSBirthday");
		case EEvents::EV_FirstOfApril: return TEXT("FirstOfApril");
		default: return TEXT("None");
		}
	}

	TArray<TSharedPtr<FJsonValue>> RelevantEventsToJsonArray(const TArray<EEvents>& Events)
	{
		TArray<TSharedPtr<FJsonValue>> Result;
		for (const EEvents Event : Events)
		{
			Result.Add(MakeShared<FJsonValueString>(EventToString(Event)));
		}
		return Result;
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
			EntryObject->SetBoolField(TEXT("isAvailable"), RecipeManager->IsRecipeAvailable(RecipeClass));
			EntryObject->SetArrayField(TEXT("relevantEvents"), RelevantEventsToJsonArray(UFGRecipe::GetRelevantEvents(RecipeClass)));

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
			EntryObject->SetBoolField(TEXT("isAvailable"), RecipeManager->IsItemDescriptorAvailable(ItemClass));
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

// See LogTruckStationsAsJson's doc comment in the header for the real
// Recipe_TruckStation/Recipe_FluidTruckStation -> AFGBuildableDockingStation
// unified-class finding this mirrors from world.trainCargoPlatforms.
FString UAIModFunctionLibrary::LogTruckStationsAsJson(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		UE_LOG(LogAIModAI, Warning, TEXT("LogTruckStationsAsJson: no valid world context"));
		return TEXT("{}");
	}

	TArray<TSharedPtr<FJsonValue>> StationsJsonArray;
	for (TActorIterator<AFGBuildableDockingStation> It(World); It; ++It)
	{
		AFGBuildableDockingStation* Station = *It;
		if (!IsValid(Station))
		{
			continue;
		}

		const TSharedRef<FJsonObject> StationObject = MakeShared<FJsonObject>();
		StationObject->SetStringField(TEXT("id"), Station->GetPathName());
		StationObject->SetStringField(TEXT("buildableClass"), Station->GetClass()->GetPathName());
		StationObject->SetStringField(TEXT("resourceForm"), ResourceFormToString(Station->GetDockingStationResourceForm()));

		const TSubclassOf<UFGItemDescriptor> FluidDescriptor = Station->GetCurrentFluidDescriptor();
		StationObject->SetStringField(TEXT("currentFluidDescriptor"), FluidDescriptor ? FluidDescriptor->GetPathName() : FString());

		StationObject->SetBoolField(TEXT("isInLoadMode"), Station->GetIsInLoadMode());
		StationObject->SetBoolField(TEXT("isLoadUnloading"), Station->IsLoadUnloading());
		StationObject->SetNumberField(TEXT("loadUnloadCycleProgress"), Station->GetLoadUnloadCycleProgress());
		StationObject->SetNumberField(TEXT("loadUnloadCycleLength"), Station->GetLoadUnloadCycleLength());

		// Combined, station-level rates "for all vehicles that dock to
		// this station" (own doc comments) - not per-vehicle.
		StationObject->SetNumberField(TEXT("vehicleFuelConsumptionRate"), Station->GetVehicleFuelConsumptionRate());
		StationObject->SetNumberField(TEXT("itemTransferRate"), Station->GetItemTransferRate());
		StationObject->SetNumberField(TEXT("maximumStackTransferRate"), Station->GetMaximumStackTransferRate());

		AActor* DockedActor = Station->GetDockedActor();
		StationObject->SetStringField(TEXT("dockedVehicleId"), IsValid(DockedActor) ? DockedActor->GetPathName() : FString());
		StationObject->SetStringField(TEXT("dockedVehicleClass"), IsValid(DockedActor) ? DockedActor->GetClass()->GetPathName() : FString());

		StationsJsonArray.Add(MakeShared<FJsonValueObject>(StationObject));
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetArrayField(TEXT("truckStations"), StationsJsonArray);

	const FString JsonString = WriteCondensedJson(RootObject);

	UE_LOG(LogAIModAI, Display, TEXT("LogTruckStationsAsJson: %d truck station(s)"), StationsJsonArray.Num());

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
			EntryObject->SetBoolField(TEXT("isAvailable"), RecipeManager->IsBuildingAvailable(BuildableClass));
			EntryObject->SetArrayField(TEXT("relevantEvents"), RelevantEventsToJsonArray(UFGRecipe::GetRelevantEvents(RecipeClass)));

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

// See LogActiveEventsAsJson's doc comment in the header for the real
// AFGEventSubsystem::GetCurrentEvents() sourcing and why "Christmas"
// (not "FICSMAS") is the string used here.
FString UAIModFunctionLibrary::LogActiveEventsAsJson(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	AFGEventSubsystem* EventSubsystem = World ? AFGEventSubsystem::Get(World) : nullptr;
	if (!EventSubsystem)
	{
		UE_LOG(LogAIModAI, Warning, TEXT("LogActiveEventsAsJson: no valid world context or AFGEventSubsystem"));
		return TEXT("{}");
	}

	static const EEvents AllEvents[] = { EEvents::EV_Christmas, EEvents::EV_Birthday, EEvents::EV_CSSBirthday, EEvents::EV_FirstOfApril };

	const TArray<EEvents>& CurrentEvents = EventSubsystem->GetCurrentEvents();

	TArray<TSharedPtr<FJsonValue>> EventsJsonArray;
	for (const EEvents Event : AllEvents)
	{
		const TSharedRef<FJsonObject> EventObject = MakeShared<FJsonObject>();
		EventObject->SetStringField(TEXT("event"), EventToString(Event));
		EventObject->SetBoolField(TEXT("isActive"), CurrentEvents.Contains(Event));
		EventsJsonArray.Add(MakeShared<FJsonValueObject>(EventObject));
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetArrayField(TEXT("events"), EventsJsonArray);

	const FString JsonString = WriteCondensedJson(RootObject);

	UE_LOG(LogAIModAI, Display, TEXT("LogActiveEventsAsJson: %s"), *JsonString);

	return JsonString;
}

FString UAIModFunctionLibrary::LogConstructionCostAsJson(UObject* WorldContextObject, const FString& RecipeClassPath)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	UClass* ResolvedClass = World ? LoadObject<UClass>(nullptr, *RecipeClassPath) : nullptr;
	const TSubclassOf<UFGRecipe> RecipeClass = (ResolvedClass && ResolvedClass->IsChildOf(UFGRecipe::StaticClass())) ? ResolvedClass : nullptr;
	if (!RecipeClass)
	{
		UE_LOG(LogAIModAI, Warning, TEXT("LogConstructionCostAsJson: '%s' did not resolve to a UFGRecipe subclass, or no valid world context"), *RecipeClassPath);
		return TEXT("{\"protocolVersion\":1,\"recipeClass\":\"\",\"baseIngredients\":[],\"appliedCustomizationRecipes\":[],\"totalIngredients\":[]}");
	}

	const TArray<FItemAmount> BaseIngredients = UFGRecipe::GetIngredients(World, RecipeClass);
	TArray<TSubclassOf<UFGRecipe>> AppliedCustomizationRecipes;

	// Extractor recipes are refused here the same way ConstructBuildingAtPosition
	// refuses to CONSTRUCT them through the generic path - a confirmed hard
	// engine crash (AFGResourceExtractorHologram::ConfigureActor's
	// mSnappedExtractableResource assertion) lives inside Construct(), which
	// this function never calls, but the hologram this function DOES spawn
	// (to read its auto-applied customization state) is the same class, and
	// there is no evidence UpdateHologramPlacement() alone is safe for it
	// without a real snapped node. Not worth the risk for a read-only query -
	// extractors realistically do not carry meaningful swatch costs anyway,
	// so this just reports the base recipe cost for them.
	const TSubclassOf<AFGBuildable> ResolvedBuildableClass = ResolveBuildableClassForRecipe(RecipeClassPath);
	const bool bIsExtractorRecipe = ResolvedBuildableClass && ResolvedBuildableClass->IsChildOf(AFGBuildableResourceExtractorBase::StaticClass());

	if (!bIsExtractorRecipe)
	{
		if (AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0)))
		{
			Character->HotKeyRecipe(RecipeClass);
			AFGBuildGun* BuildGun = Character->GetBuildGun();
			UFGBuildGunStateBuild* BuildState = BuildGun ? Cast<UFGBuildGunStateBuild>(BuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD)) : nullptr;
			AFGHologram* Hologram = BuildState ? BuildState->GetHologram() : nullptr;
			if (Hologram)
			{
				// Settle the hologram once, synchronously - the same
				// UpdateHologramPlacement() call every Construct* function's
				// poll loop uses, just once here since this only needs
				// customization state to have applied, not a real placement.
				FHitResult SyntheticHit;
				SyntheticHit.Location = Character->GetActorLocation();
				SyntheticHit.ImpactPoint = SyntheticHit.Location;
				SyntheticHit.Normal = FVector::UpVector;
				SyntheticHit.ImpactNormal = FVector::UpVector;
				SyntheticHit.bBlockingHit = true;
				Hologram->UpdateHologramPlacement(SyntheticHit);

				// mCustomizationData is protected, no public getter - same
				// FStructProperty reflection pattern already used elsewhere
				// in this file for other protected UPROPERTYs.
				if (AFGBuildableHologram* BuildableHologram = Cast<AFGBuildableHologram>(Hologram))
				{
					if (const FStructProperty* CustomizationProperty = FindFProperty<FStructProperty>(BuildableHologram->GetClass(), TEXT("mCustomizationData")))
					{
						if (const FFactoryCustomizationData* CustomizationData = CustomizationProperty->ContainerPtrToValuePtr<FFactoryCustomizationData>(BuildableHologram))
						{
							CustomizationData->GetAppliedRecipes(World, AppliedCustomizationRecipes);
						}
					}
				}
			}
			Character->UnequipBuildGun();
		}
	}

	// Merge base + every applied customization recipe's own ingredients,
	// summing amounts for the same item class - the real total a player
	// pays, per this function's header doc comment.
	TArray<FItemAmount> TotalIngredients = BaseIngredients;
	for (const TSubclassOf<UFGRecipe>& CustomizationRecipeClass : AppliedCustomizationRecipes)
	{
		if (!CustomizationRecipeClass) { continue; }
		for (const FItemAmount& Extra : UFGRecipe::GetIngredients(World, CustomizationRecipeClass))
		{
			FItemAmount* Existing = TotalIngredients.FindByPredicate([&Extra](const FItemAmount& Candidate) { return Candidate.ItemClass == Extra.ItemClass; });
			if (Existing)
			{
				Existing->Amount += Extra.Amount;
			}
			else
			{
				TotalIngredients.Add(Extra);
			}
		}
	}

	TArray<TSharedPtr<FJsonValue>> AppliedRecipesJsonArray;
	for (const TSubclassOf<UFGRecipe>& CustomizationRecipeClass : AppliedCustomizationRecipes)
	{
		if (CustomizationRecipeClass) { AppliedRecipesJsonArray.Add(MakeShared<FJsonValueString>(CustomizationRecipeClass->GetPathName())); }
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetStringField(TEXT("recipeClass"), RecipeClass->GetPathName());
	RootObject->SetArrayField(TEXT("baseIngredients"), ItemAmountsToJsonArray(BaseIngredients));
	RootObject->SetArrayField(TEXT("appliedCustomizationRecipes"), AppliedRecipesJsonArray);
	RootObject->SetArrayField(TEXT("totalIngredients"), ItemAmountsToJsonArray(TotalIngredients));

	FString JsonString;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonString);
	FJsonSerializer::Serialize(RootObject, Writer);

	UE_LOG(LogAIModAI, Display, TEXT("LogConstructionCostAsJson: recipe=%s isExtractor=%s baseIngredients=%d appliedCustomizationRecipes=%d totalIngredients=%d"),
		*RecipeClassPath, bIsExtractorRecipe ? TEXT("true") : TEXT("false"), BaseIngredients.Num(), AppliedCustomizationRecipes.Num(), TotalIngredients.Num());

	return JsonString;
}

namespace
{
// "RealCharacter" instigator strategy - see forward declaration's doc
// comment near the top of this file. Verbatim copy of this function's
// pre-2026-08-30 body (git commit be42e1595f), just renamed - drives the
// REAL player's BuildGun exactly as before.
void ConstructConveyorBelt_RealCharacterStrategy(UObject* WorldContextObject, const FString& SourceBuildableId, const FString& DestBuildableId, const FString& RecipeClassPath, const FString& RouteMode, const TOptional<FVector>& SourceConnectorPosition, const TOptional<FVector>& DestConnectorPosition, bool bDryRun, TFunction<void(const FAIModOperationResult&)> OnComplete)
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

	// Position-targeted selection (2026-08-30) when the caller supplied
	// one - see FindFreeFactoryConnectionNear's own comment for why this
	// exists. Falls back to the old "first free of this direction"
	// behavior when no position is given, so every pre-existing caller
	// (manifold-building scripts that only care "connect these two
	// buildables") keeps working unchanged.
	UFGFactoryConnectionComponent* SourceConnection = SourceConnectorPosition.IsSet()
		? FindFreeFactoryConnectionNear(SourceBuildable, EFactoryConnectionDirection::FCD_OUTPUT, SourceConnectorPosition.GetValue())
		: FindFreeFactoryConnection(SourceBuildable, EFactoryConnectionDirection::FCD_OUTPUT);
	if (!SourceConnection)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_FACTORY_CONNECTION"),
			SourceConnectorPosition.IsSet()
				? FString::Printf(TEXT("'%s' has no free Output factory connection within tolerance of the requested position %s"), *SourceBuildableId, *SourceConnectorPosition.GetValue().ToString())
				: FString::Printf(TEXT("'%s' has no free Output factory connection component"), *SourceBuildableId)));
		return;
	}
	UFGFactoryConnectionComponent* DestConnection = DestConnectorPosition.IsSet()
		? FindFreeFactoryConnectionNear(DestBuildable, EFactoryConnectionDirection::FCD_INPUT, DestConnectorPosition.GetValue())
		: FindFreeFactoryConnection(DestBuildable, EFactoryConnectionDirection::FCD_INPUT);
	if (!DestConnection)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_FACTORY_CONNECTION"),
			DestConnectorPosition.IsSet()
				? FString::Printf(TEXT("'%s' has no free Input factory connection within tolerance of the requested position %s"), *DestBuildableId, *DestConnectorPosition.GetValue().ToString())
				: FString::Printf(TEXT("'%s' has no free Input factory connection component"), *DestBuildableId)));
		return;
	}

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

	const FRotator BeltDeterministicLook = (DestConnection->GetConnectorLocation() - SourceConnection->GetConnectorLocation()).Rotation();
	if (AController* BeltController = Character->GetController())
	{
		BeltController->SetControlRotation(BeltDeterministicLook);
	}

	const FHitResult StartHit = MakeHitAt(SourceBuildable, SourceConnection);
	BeltHologram->UpdateHologramPlacement(StartHit);
	BeltHologram->TrySnapToActor(StartHit);
	const bool bStartStepComplete = BeltHologram->DoMultiStepPlacement(true);
	const ESplineHologramBuildStep StepAfterStart = BeltHologram->GetCurrentBuildStep();

	UE_LOG(LogAIModAI, Display, TEXT("ConstructConveyorBelt: source=%s dest=%s after start click: stepComplete=%s step=%d disqualifiers=[%s]"),
		*SourceBuildableId, *DestBuildableId, bStartStepComplete ? TEXT("true") : TEXT("false"), static_cast<int32>(StepAfterStart), *SummarizeDisqualifiers(BeltHologram));

	if (bStartStepComplete)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("UNEXPECTED_STEP_COMPLETE"), TEXT("DoMultiStepPlacement() reported complete after only the start click")));
		return;
	}

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
		FHitResult EndHit; // re-asserted every poll tick, see below
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
	PollState->EndHit = EndHit;
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

		if (IsValid(PollCharacter))
		{
			if (AController* PollController = PollCharacter->GetController())
			{
				PollController->SetControlRotation(PollState->DeterministicLook);
			}
		}

		// Re-assert the end hit every poll tick (2026-08-30) - matching
		// the fix already proven for point holograms in
		// ConstructBuildingNearPlayer/ConstructExtractorOnTargetedNode
		// (see docs/buildgun-driven-placement-research.md's "§3
		// correction"). UFGBuildGunStateBuild::TickState_Implementation
		// runs its own real AFGBuildGun::TraceForBuilding() every tick
		// from the REAL player's live camera aim and silently overwrites
		// whatever hit/placement state this function set up - confirmed
		// there via a ~4000-unit drift and a "Surface is too uneven!"
		// failure at a location nowhere near the intended one. This poll
		// loop already re-asserts rotation every tick for the same
		// reason but was never given the analogous placement fix -
		// live-suspected 2026-08-30 as the real explanation for
		// intermittent "Conveyor Belt is too long!"/"Surface is too
		// uneven!" failures that couldn't be explained by distance,
		// AFK state, or leftover geometry alone.
		PollHologram->UpdateHologramPlacement(PollState->EndHit);

		TArray<TSubclassOf<UFGConstructDisqualifier>> Disqualifiers;
		PollHologram->GetConstructDisqualifiers(Disqualifiers);
		const bool bStillInitializing = Disqualifiers.Contains(TSubclassOf<UFGConstructDisqualifier>(UFGCDInitializing::StaticClass()));

		--PollState->AttemptsRemaining;
		if (bStillInitializing && PollState->AttemptsRemaining > 0)
		{
			PollWorld->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
			return;
		}

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
} // namespace

// Decoy-instigator rewrite (2026-08-30, explicit user direction after the
// original SetControlRotation-based fix below was confirmed live to
// visibly hijack the REAL player's camera for the full duration of every
// call - "hijack the player camera for minutes at a time in intermittent
// bursts" on any multi-belt build). Root cause established by the
// original fix's own comments: AutoRouteSpline()'s routing (stub source,
// unverifiable directly) empirically depends on the CONSTRUCTION
// INSTIGATOR's controller rotation, not just the connector geometry -
// the previous fix worked around this by forcing the REAL Character's
// controller rotation every poll tick, which is exactly what was visible
// to the user. This version spawns the hologram via the real, public,
// non-stub AFGHologram::SpawnHologramFromRecipe() (FGHologram.h) with an
// explicit throwaway APawn+AController as the CONSTRUCTION INSTIGATOR,
// instead of implicitly using Character->GetBuildGun()'s real equipped
// hologram - the real player's BuildGun/camera is never touched at all
// (no HotKeyRecipe/GetBuildGun/UnequipBuildGun anywhere in this
// function). The deterministic-look rotation still gets set, but on the
// DECOY controller only.
//
// Trade-off this introduces: swapping the instigator away from Character
// means CanConstruct()'s real UFGCDUnaffordable check would resolve
// against the decoy's (nonexistent) inventory, not the player's - so
// this function now manually verifies and charges the recipe's real
// ingredient cost from the player's OWN inventory (same "verify every
// ingredient before touching any of them" pattern as SimulatedCraft
// above), and always ignores UFGCDUnaffordable in the poll loop's
// disqualifier check (not just when the UnlimitedResources setting is
// on) since affordability is now handled explicitly, before Construct()
// is ever called. NOT YET LIVE-VERIFIED - AutoRouteSpline/
// GenerateAndUpdateSpline/ConfigureActor/Construct are all stub source
// in this SDK, so whether a decoy instigator produces correct routing
// and a correctly-owned/replicated real belt actor is a live-test
// question, not something readable from source. If this regresses
// routing correctness, the previous real-Character-rotation approach is
// preserved in git history and this comment documents exactly what
// changed and why, to make reverting fast.
void UAIModFunctionLibrary::ConstructConveyorBelt(UObject* WorldContextObject, const FString& SourceBuildableId, const FString& DestBuildableId, const FString& RecipeClassPath, const FString& RouteMode, const FString& InstigatorStrategy, const TOptional<FVector>& SourceConnectorPosition, const TOptional<FVector>& DestConnectorPosition, bool bDryRun, TFunction<void(const FAIModOperationResult&)> OnComplete)
{
	// Strategy dispatch (2026-08-30, explicit user request: "implement
	// multiple competing strategies per-compile... so if one test fails,
	// you can attempt other theories before requiring a fresh build").
	// "RealCharacter" delegates to the untouched original implementation
	// (see its own comment) - everything below this point is the newer
	// decoy-instigator path, parameterized only by which controller class
	// possesses the decoy pawn.
	//
	// SourceConnectorPosition/DestConnectorPosition (2026-08-30, explicit
	// user requirement - see FindFreeFactoryConnectionNear's comment):
	// when provided, target one SPECIFIC connector by its real world
	// position instead of "the first free one of the right direction" -
	// required for deterministic per-port selection on a multi-output
	// buildable like a splitter. Optional and backward-compatible -
	// omitting them keeps every existing caller's behavior unchanged.
	const FString Strategy = InstigatorStrategy.IsEmpty() ? TEXT("PlayerController") : InstigatorStrategy;
	if (Strategy.Equals(TEXT("RealCharacter"), ESearchCase::IgnoreCase))
	{
		ConstructConveyorBelt_RealCharacterStrategy(WorldContextObject, SourceBuildableId, DestBuildableId, RecipeClassPath, RouteMode, SourceConnectorPosition, DestConnectorPosition, bDryRun, MoveTemp(OnComplete));
		return;
	}
	const bool bUseAIController = Strategy.Equals(TEXT("AIController"), ESearchCase::IgnoreCase);
	const bool bUsePlayerController = Strategy.Equals(TEXT("PlayerController"), ESearchCase::IgnoreCase);
	// "LocalPlayer" (2026-08-30) - NOT YET LIVE-TESTED (written and
	// compiled while the running game couldn't be redeployed - see this
	// strategy's own comment below, and docs/camera-hijack-and-second-
	// player-research.md, for the full research this is based on). Spawns
	// a GENUINE second ULocalPlayer via UGameInstance::CreateLocalPlayer()
	// - confirmed-real, non-stub engine mechanism that routes through the
	// same Login/PostLogin path a real multiplayer join uses - instead of
	// a bare decoy actor, on the hypothesis that whatever gates
	// UFGCDInitializing cares about genuine local-player identity, not
	// just controller class (both AIController and PlayerController
	// decoys already conclusively failed identically).
	const bool bUseLocalPlayer = Strategy.Equals(TEXT("LocalPlayer"), ESearchCase::IgnoreCase);
	if (!bUseAIController && !bUsePlayerController && !bUseLocalPlayer)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("INVALID_INSTIGATOR_STRATEGY"),
			FString::Printf(TEXT("'%s' is not one of \"RealCharacter\", \"AIController\", \"PlayerController\", \"LocalPlayer\""), *Strategy)));
		return;
	}

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
	UFGInventoryComponent* PlayerInventory = Character->GetInventory();
	if (!PlayerInventory)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No player inventory found")));
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

	UFGFactoryConnectionComponent* SourceConnection = SourceConnectorPosition.IsSet()
		? FindFreeFactoryConnectionNear(SourceBuildable, EFactoryConnectionDirection::FCD_OUTPUT, SourceConnectorPosition.GetValue())
		: FindFreeFactoryConnection(SourceBuildable, EFactoryConnectionDirection::FCD_OUTPUT);
	if (!SourceConnection)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_FACTORY_CONNECTION"), FString::Printf(TEXT("'%s' has no free Output factory connection component"), *SourceBuildableId)));
		return;
	}
	UFGFactoryConnectionComponent* DestConnection = DestConnectorPosition.IsSet()
		? FindFreeFactoryConnectionNear(DestBuildable, EFactoryConnectionDirection::FCD_INPUT, DestConnectorPosition.GetValue())
		: FindFreeFactoryConnection(DestBuildable, EFactoryConnectionDirection::FCD_INPUT);
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

	// Verify affordability up front (never partially consume ingredients
	// for a belt that can't complete) - same pattern as SimulatedCraft.
	// Actual deduction happens later, immediately before Construct(), once
	// every other disqualifier has been confirmed clear.
	const TArray<FItemAmount> BeltIngredients = UFGRecipe::GetIngredients(World, RecipeClass);
	{
		TArray<FString> ShortfallDescriptions;
		for (const FItemAmount& Ingredient : BeltIngredients)
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
			OnComplete(FAIModOperationResult::Failure(TEXT("INSUFFICIENT_INGREDIENTS"),
				FString::Printf(TEXT("Missing: %s"), *FString::Join(ShortfallDescriptions, TEXT("; ")))));
			return;
		}
	}

	// Decoy pawn+controller - stands in for the real player as the
	// hologram's construction instigator so nothing here ever touches
	// Character's actual camera/equipped item. Cleaned up on every exit
	// path below via CleanupScratch(). Three concrete candidates,
	// selectable via params.instigatorStrategy without a recompile:
	// AIController and PlayerController (both live-confirmed, conclusively,
	// to leave the hologram permanently stuck on UFGCDInitializing -
	// present immediately after the first click, never clears across the
	// full 120-tick poll, even though stepComplete/connectedCount both
	// look correct - controller CLASS is not the variable, ruled out with
	// both tested back-to-back in one session) and LocalPlayer (below).
	APawn* DecoyPawn = nullptr;
	AController* DecoyController = nullptr;
	ULocalPlayer* NewLocalPlayer = nullptr; // only set for the LocalPlayer strategy - drives cleanup below

	if (bUseLocalPlayer)
	{
		// GENUINELY LOCAL second player (2026-08-30, NOT YET LIVE-TESTED -
		// written from source research done while a redeploy wasn't
		// possible, see docs/camera-hijack-and-second-player-research.md
		// for the full citations behind every claim in this comment).
		// UGameInstance::CreateLocalPlayer() is confirmed-real, non-stub
		// engine source (Engine\Private\GameInstance.cpp) - with
		// bSpawnPlayerController=true and NM_Standalone (true for this
		// project's single-player target), it drives the SAME
		// AGameModeBase::Login()/PostLogin() path a real multiplayer
		// client join uses, which - per AFGGameMode's real confirmed
		// default (FGGameMode.cpp) - spawns and possesses a genuine
		// AFGCharacterPlayer via DefaultPawnClass. That's the whole
		// reason to try this: AIController/PlayerController decoys are
		// bare, never-joined actors; this one goes through the actual
		// join flow FactoryGame itself uses.
		UGameInstance* GameInstance = World->GetGameInstance();
		if (!GameInstance)
		{
			OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No UGameInstance for this world")));
			return;
		}
		FString CreateLocalPlayerError;
		NewLocalPlayer = GameInstance->CreateLocalPlayer(FPlatformUserId::CreateFromInternalId(1), CreateLocalPlayerError, /*bSpawnPlayerController=*/true);
		if (!NewLocalPlayer)
		{
			OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), FString::Printf(TEXT("CreateLocalPlayer failed: %s"), *CreateLocalPlayerError)));
			return;
		}
		// Headless - confirmed-real engine mechanism
		// (GameViewportClient.cpp): forces every non-primary local
		// player's viewport rect to zero size, so nothing is ever
		// rendered for this second player and no split-screen ever
		// appears. Applied immediately, before anything else can render
		// a frame with the new player in it.
		if (UGameViewportClient* ViewportClient = World->GetGameViewport())
		{
			ViewportClient->SetForceDisableSplitscreen(true);
		}

		APlayerController* NewPC = NewLocalPlayer->GetPlayerController(World);
		APawn* NewPawn = NewPC ? NewPC->GetPawn() : nullptr;
		if (!NewPC || !NewPawn)
		{
			GameInstance->RemoveLocalPlayer(NewLocalPlayer);
			OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"),
				FString::Printf(TEXT("CreateLocalPlayer succeeded but no PlayerController/Pawn resulted (PC=%s pawn=%s) - login/possession may not have completed synchronously"),
					NewPC ? TEXT("valid") : TEXT("null"), NewPawn ? TEXT("valid") : TEXT("null"))));
			return;
		}
		DecoyController = NewPC;
		DecoyPawn = NewPawn;
		UE_LOG(LogAIModAI, Display, TEXT("ConstructConveyorBelt: LocalPlayer strategy - spawned real second local player, pawn class=%s"), *NewPawn->GetClass()->GetName());
	}
	else
	{
		DecoyPawn = World->SpawnActor<APawn>(APawn::StaticClass(), SourceConnection->GetConnectorLocation(), FRotator::ZeroRotator);
		// Plain AController is abstract in this engine build (live-confirmed:
		// "SpawnActor failed because class Controller is abstract").
		DecoyController = bUseAIController
			? Cast<AController>(World->SpawnActor<AAIController>(AAIController::StaticClass()))
			: Cast<AController>(World->SpawnActor<APlayerController>(APlayerController::StaticClass()));
		if (!DecoyPawn || !DecoyController)
		{
			if (DecoyPawn) { DecoyPawn->Destroy(); }
			if (DecoyController) { DecoyController->Destroy(); }
			OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("Failed to spawn the decoy instigator pawn/controller")));
			return;
		}
		DecoyController->Possess(DecoyPawn);
	}

	// Replaces every former Character->UnequipBuildGun() call. For the two
	// bare-decoy strategies, cleanup is just destroying our own scratch
	// actors. For LocalPlayer, destroying the PlayerController directly
	// would leave the ULocalPlayer wrapper itself dangling/leaked -
	// GameInstance->RemoveLocalPlayer() is the confirmed-real, proper
	// engine teardown (destroys the PlayerController AND unregisters the
	// ULocalPlayer) - see docs/camera-hijack-and-second-player-research.md.
	// Defined before the SpawnHologramFromRecipe call so it covers that
	// call's own failure path too, not just later ones.
	UGameInstance* CleanupGameInstance = World->GetGameInstance();
	auto CleanupScratch = [CleanupGameInstance, bUseLocalPlayer](AFGHologram* H, APawn* DPawn, AController* DController, ULocalPlayer* LP)
	{
		if (IsValid(H)) { H->Destroy(); }
		if (bUseLocalPlayer)
		{
			if (LP && CleanupGameInstance) { CleanupGameInstance->RemoveLocalPlayer(LP); }
		}
		else
		{
			if (IsValid(DController)) { DController->Destroy(); }
			if (IsValid(DPawn)) { DPawn->Destroy(); }
		}
	};

	// hologramOwner is ALSO the decoy now (2026-08-30, live-confirmed
	// necessary): passing Character here - even with DecoyPawn already
	// used as the instigator - still visibly swung the REAL player's
	// camera (confirmed live: "the player was facing approximately south
	// when you started, and automatically turned due east"). Something in
	// the hologram's construction/camera-preview logic evidently reads
	// the OWNER, not just the instigator, for whatever drives that. Fully
	// decoupling Character from both parameters is the only way to be
	// sure nothing in this call can reach the real player's camera - the
	// belt's material cost is already charged from Character's inventory
	// manually (see BeltIngredients above), so nothing here still needs
	// Character to be the owner for cost/affordability purposes either.
	AFGHologram* Hologram = AFGHologram::SpawnHologramFromRecipe(RecipeClass, DecoyPawn, SourceConnection->GetConnectorLocation(), DecoyPawn);
	AFGConveyorBeltHologram* BeltHologram = Cast<AFGConveyorBeltHologram>(Hologram);
	if (!BeltHologram)
	{
		CleanupScratch(Hologram, DecoyPawn, DecoyController, NewLocalPlayer);
		OnComplete(FAIModOperationResult::Failure(TEXT("HOLOGRAM_SPAWN_FAILED"),
			FString::Printf(TEXT("SpawnHologramFromRecipe(%s) did not result in an AFGConveyorBeltHologram (got %s)"),
				*RecipeClassPath, Hologram ? *Hologram->GetClass()->GetName() : TEXT("null"))));
		return;
	}

	// Live-diagnosed (2026-08-30): a hologram spawned via
	// SpawnHologramFromRecipe (bypassing the real BuildGun's equip flow)
	// stayed stuck on UFGCDInitializing indefinitely - CANNOT_CONSTRUCT
	// "Initializing (hard)" after the full poll window elapsed, unlike
	// the BuildGun-driven path this replaces, which always cleared it
	// within a tick or two, for BOTH bare-decoy strategies. Explicitly
	// enabling tick here is cheap and safe even though it didn't fix that
	// on its own.
	BeltHologram->SetActorTickEnabled(true);

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
			CleanupScratch(BeltHologram, DecoyPawn, DecoyController, NewLocalPlayer);
			OnComplete(FAIModOperationResult::Failure(TEXT("INVALID_ROUTE_MODE"), FString::Printf(TEXT("'%s' is not one of \"Straight\", \"Curve\", \"Auto\""), *RouteMode)));
			return;
		}

		UClass* RouteModeClass = LoadObject<UClass>(nullptr, *RouteModeAssetPath);
		if (!RouteModeClass || !RouteModeClass->IsChildOf(UFGHologramBuildModeDescriptor::StaticClass()))
		{
			CleanupScratch(BeltHologram, DecoyPawn, DecoyController, NewLocalPlayer);
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

	// Player-independence, take 3 (2026-08-30): the underlying dependency
	// (see the function's top comment) is real and still needs a
	// deterministic rotation hint - but it now targets the DECOY
	// controller instead of the real Character's, so the real player's
	// camera never moves. Reasserted every poll tick below for the same
	// "state re-derived each tick" reason as before, just on DecoyController.
	const FRotator BeltDeterministicLook = (DestConnection->GetConnectorLocation() - SourceConnection->GetConnectorLocation()).Rotation();
	DecoyController->SetControlRotation(BeltDeterministicLook);

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
		CleanupScratch(BeltHologram, DecoyPawn, DecoyController, NewLocalPlayer);
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
		CleanupScratch(BeltHologram, DecoyPawn, DecoyController, NewLocalPlayer);
		OnComplete(FAIModOperationResult::Failure(TEXT("PLACEMENT_INCOMPLETE"),
			FString::Printf(TEXT("DoMultiStepPlacement() did not report complete after the end click - step=%d connectedCount=%d, may need a third step"), static_cast<int32>(StepAfterEnd), ConnectedBuildables.Num())));
		return;
	}

	struct FPollState
	{
		TWeakObjectPtr<AFGConveyorBeltHologram> Hologram;
		TWeakObjectPtr<APawn> DecoyPawn;
		TWeakObjectPtr<AController> DecoyController;
		TWeakObjectPtr<ULocalPlayer> NewLocalPlayer;
		TWeakObjectPtr<UGameInstance> GameInstance;
		bool bUseLocalPlayer = false;
		TWeakObjectPtr<AFGCharacterPlayer> Character;
		TWeakObjectPtr<UFGInventoryComponent> PlayerInventory;
		TArray<FItemAmount> Ingredients;
		TWeakObjectPtr<UWorld> World;
		FString SourceBuildableId;
		FString DestBuildableId;
		bool bDryRun = true;
		FRotator DeterministicLook;
		FHitResult EndHit;
		int32 AttemptsRemaining = 120; // safety cap - real ticks, not a fixed duration
		int32 AttemptsTaken = 0;
		TFunction<void(const FAIModOperationResult&)> OnComplete;
	};
	const TSharedRef<FPollState> PollState = MakeShared<FPollState>();
	PollState->Hologram = BeltHologram;
	PollState->DecoyPawn = DecoyPawn;
	PollState->DecoyController = DecoyController;
	PollState->NewLocalPlayer = NewLocalPlayer;
	PollState->GameInstance = CleanupGameInstance;
	PollState->bUseLocalPlayer = bUseLocalPlayer;
	PollState->Character = Character;
	PollState->PlayerInventory = PlayerInventory;
	PollState->Ingredients = BeltIngredients;
	PollState->World = World;
	PollState->SourceBuildableId = SourceBuildableId;
	PollState->DestBuildableId = DestBuildableId;
	PollState->bDryRun = bDryRun;
	PollState->DeterministicLook = BeltDeterministicLook;
	PollState->EndHit = EndHit;
	PollState->OnComplete = MoveTemp(OnComplete);

	auto PollCleanup = [](const TSharedRef<FPollState>& S)
	{
		if (AFGConveyorBeltHologram* H = S->Hologram.Get()) { H->Destroy(); }
		if (S->bUseLocalPlayer)
		{
			if (ULocalPlayer* LP = S->NewLocalPlayer.Get())
			{
				if (UGameInstance* GI = S->GameInstance.Get()) { GI->RemoveLocalPlayer(LP); }
			}
		}
		else
		{
			if (AController* C = S->DecoyController.Get()) { C->Destroy(); }
			if (APawn* P = S->DecoyPawn.Get()) { P->Destroy(); }
		}
	};

	const TSharedRef<TFunction<void()>> PollFn = MakeShared<TFunction<void()>>();
	*PollFn = [PollState, PollFn, PollCleanup]()
	{
		++PollState->AttemptsTaken;

		AFGConveyorBeltHologram* PollHologram = PollState->Hologram.Get();
		UWorld* PollWorld = PollState->World.Get();
		AController* PollDecoyController = PollState->DecoyController.Get();
		if (!IsValid(PollHologram) || !PollWorld)
		{
			UE_LOG(LogAIModAI, Warning, TEXT("ConstructConveyorBelt (deferred): hologram or world became invalid while polling (after %d tick(s))"), PollState->AttemptsTaken);
			PollCleanup(PollState);
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("HOLOGRAM_INVALIDATED"), TEXT("Hologram or world became invalid while polling")));
			return;
		}

		// Re-assert every tick, not just once before the poll started -
		// AutoRouteSpline()/UpdateHologramPlacement() (stub source) re-reads
		// the controller's CURRENT rotation each tick. Targets the DECOY
		// controller only - see this function's top comment.
		if (IsValid(PollDecoyController))
		{
			PollDecoyController->SetControlRotation(PollState->DeterministicLook);
		}

		// Live-diagnosed (2026-08-30): UFGCDInitializing never cleared on
		// its own even with SetActorTickEnabled(true) - it stayed present
		// for the full 120-tick poll window and the call failed
		// CANNOT_CONSTRUCT "Initializing (hard)". The real BuildGun-driven
		// flow calls UpdateHologramPlacement() continuously every frame
		// while the player aims, not just once per click - reassert it
		// here too, same "state re-derived each tick" shape as the
		// rotation reassert above, in case whatever UFGCDInitializing
		// gates only advances in response to a fresh placement update.
		PollHologram->UpdateHologramPlacement(PollState->EndHit);

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
		// disqualifier loop.
		//
		// UFGCDUnaffordable (2026-08-30, changed): now ALWAYS ignored, not
		// just under the UnlimitedResources setting - the construction
		// instigator is the decoy pawn (see top comment), which has no
		// inventory of its own, so this disqualifier would otherwise fire
		// unconditionally regardless of the real player's actual
		// inventory. Affordability against the REAL player's inventory is
		// verified up front (see BeltIngredients above) and charged
		// explicitly right before Construct() below - this disqualifier
		// genuinely has nothing meaningful left to check here.
		bool bCanConstruct = true;
		TArray<FString> DisqualifierTexts;
		for (const TSubclassOf<UFGConstructDisqualifier>& DisqualifierClass : Disqualifiers)
		{
			const bool bIgnoredForPlayerIndependence = (DisqualifierClass == UFGCDInvalidAimLocation::StaticClass())
				|| (DisqualifierClass == UFGCDUnaffordable::StaticClass());
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
			PollCleanup(PollState);
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("CANNOT_CONSTRUCT"), DisqualifierSummary));
			return;
		}

		if (PollState->bDryRun)
		{
			PollCleanup(PollState);
			PollState->OnComplete(FAIModOperationResult::Success());
			return;
		}

		// Re-verify affordability right before charging - real time has
		// passed since the up-front check (other calls may have spent the
		// same materials in the meantime). Still never partially consumes.
		UFGInventoryComponent* PollInventory = PollState->PlayerInventory.Get();
		if (!PollInventory)
		{
			PollCleanup(PollState);
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("Player inventory became invalid before constructing")));
			return;
		}
		TArray<FString> ShortfallDescriptions;
		for (const FItemAmount& Ingredient : PollState->Ingredients)
		{
			if (!Ingredient.ItemClass || !PollInventory->HasItems(Ingredient.ItemClass, Ingredient.Amount))
			{
				const int32 Have = Ingredient.ItemClass ? PollInventory->GetNumItems(Ingredient.ItemClass) : 0;
				ShortfallDescriptions.Add(FString::Printf(TEXT("%s (need %d, have %d)"),
					Ingredient.ItemClass ? *Ingredient.ItemClass->GetName() : TEXT("<null>"), Ingredient.Amount, Have));
			}
		}
		if (!ShortfallDescriptions.IsEmpty())
		{
			PollCleanup(PollState);
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("INSUFFICIENT_INGREDIENTS"),
				FString::Printf(TEXT("Missing: %s"), *FString::Join(ShortfallDescriptions, TEXT("; ")))));
			return;
		}

		AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(PollWorld);
		const FNetConstructionID ConstructionID = BuildableSubsystem ? BuildableSubsystem->GetNewNetConstructionID() : FNetConstructionID();

		TArray<AActor*> OutChildren;
		PollHologram->Construct(OutChildren, ConstructionID);

		UE_LOG(LogAIModAI, Display, TEXT("ConstructConveyorBelt (deferred, resolved after %d real tick(s)): construction attempted via Hologram->Construct() - source=%s dest=%s children=%d"),
			PollState->AttemptsTaken, *PollState->SourceBuildableId, *PollState->DestBuildableId, OutChildren.Num());

		for (const FItemAmount& Ingredient : PollState->Ingredients)
		{
			PollInventory->Remove(Ingredient.ItemClass, Ingredient.Amount);
		}

		PollCleanup(PollState);
		PollState->OnComplete(FAIModOperationResult::Success());
	};

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
}

// LogSplineGeometryAsJson (2026-08-30) - see header doc comment for the
// full rationale (added to diagnose world.connectConveyor's unpredictable
// curving by comparing its output against a normally-placed belt's real
// geometry). "found"/"isSplineBuildable" embedded in the payload, not a
// thrown RPC error - same convention as LogGroundHeightAsJson's "found".
FString UAIModFunctionLibrary::LogSplineGeometryAsJson(UObject* WorldContextObject, const FString& BuildableId)
{
	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetStringField(TEXT("buildableId"), BuildableId);
	RootObject->SetBoolField(TEXT("found"), false);
	RootObject->SetBoolField(TEXT("isSplineBuildable"), false);

	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	AFGBuildable* Buildable = World ? FindBuildableById(World, BuildableId) : nullptr;

	if (Buildable)
	{
		RootObject->SetBoolField(TEXT("found"), true);
		RootObject->SetStringField(TEXT("buildableClass"), Buildable->GetClass()->GetPathName());

		if (IFGSplineBuildableInterface* SplineBuildable = Cast<IFGSplineBuildableInterface>(Buildable))
		{
			RootObject->SetBoolField(TEXT("isSplineBuildable"), true);
			RootObject->SetNumberField(TEXT("meshLength"), SplineBuildable->GetMeshLength());

			TArray<TSharedPtr<FJsonValue>> PointsJsonArray;
			if (USplineComponent* Spline = SplineBuildable->GetSplineComponent())
			{
				RootObject->SetNumberField(TEXT("splineLength"), Spline->GetSplineLength());
				const int32 NumPoints = Spline->GetNumberOfSplinePoints();
				PointsJsonArray.Reserve(NumPoints);
				for (int32 i = 0; i < NumPoints; ++i)
				{
					const FVector Location = Spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World);
					const FVector Tangent = Spline->GetTangentAtSplinePoint(i, ESplineCoordinateSpace::World);

					const TSharedRef<FJsonObject> PointObject = MakeShared<FJsonObject>();
					const TSharedRef<FJsonObject> LocationObject = MakeShared<FJsonObject>();
					LocationObject->SetNumberField(TEXT("x"), Location.X);
					LocationObject->SetNumberField(TEXT("y"), Location.Y);
					LocationObject->SetNumberField(TEXT("z"), Location.Z);
					PointObject->SetObjectField(TEXT("location"), LocationObject);

					const TSharedRef<FJsonObject> TangentObject = MakeShared<FJsonObject>();
					TangentObject->SetNumberField(TEXT("x"), Tangent.X);
					TangentObject->SetNumberField(TEXT("y"), Tangent.Y);
					TangentObject->SetNumberField(TEXT("z"), Tangent.Z);
					PointObject->SetObjectField(TEXT("tangent"), TangentObject);

					PointsJsonArray.Add(MakeShared<FJsonValueObject>(PointObject));
				}
			}
			else
			{
				UE_LOG(LogAIModAI, Warning, TEXT("LogSplineGeometryAsJson: '%s' implements IFGSplineBuildableInterface but GetSplineComponent() returned null"), *BuildableId);
			}
			RootObject->SetArrayField(TEXT("points"), PointsJsonArray);
		}
	}

	FString JsonString;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonString);
	FJsonSerializer::Serialize(RootObject, Writer);

	UE_LOG(LogAIModAI, Display, TEXT("LogSplineGeometryAsJson: %s"), *JsonString);

	return JsonString;
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
// No RouteMode param - lifts are a fixed vertical column, no bend/curve
// concept applies.
//
// CONNECTOR-FINDING: FIXED 2026-08-30 (see FindFreeFactoryConnection/
// FindFreeFactoryConnectionNear's doc comments above for the full story).
// A wall/pole's FCD_SNAP_ONLY connector - a real, valid attachment point
// ("special case for conveyor poles" per FGFactoryConnectionComponent.h)
// - was never found by either finder, which required an exact
// Input/Output direction match, so any call targeting a wall failed
// immediately with NO_FACTORY_CONNECTION. Both finders now fall back to
// a free SnapOnly connector when no exact-direction match exists.
// IsConnected() is documented to always read false for SnapOnly
// regardless of real attachment state - not a bug, don't use it to judge
// whether a wall/pole slot is free.
//
// HEIGHT: an open, actively-investigated gap - a single call always
// lands at the hologram's ~400-unit default regardless of the real
// target's distance, where a real player can build an arbitrary height
// in one piece by wherever their camera is aimed when they click (no
// scroll wheel - scroll only rotates the destination end's Input/Output,
// per the user). SIX hypotheses tried, all with real log evidence, five
// ruled out (findings from 2026-08-30, kept here since each cost a full
// live-test cycle and re-deriving any of them would waste another):
//   1. Rotation origin point (source connector position vs the player's
//      real actor location, as the vector origin for the deterministic
//      look rotation) - no effect either way. Refined further
//      (2026-08-31, offline research, NOT YET LIVE-TESTED): the
//      "player's real location" used Character->GetActorLocation() (the
//      pawn's root/capsule), not AFGCharacterPlayer::
//      GetCameraComponentWorldLocation() (a real, dedicated accessor for
//      the actual first-person camera position, offset upward from the
//      capsule by roughly eye height) - now uses the real camera
//      position, since any camera-position-based logic (per #6/#7)
//      would otherwise have a systematic vertical offset error baked in
//      regardless of whether #6/#7 themselves pan out.
//   2. Connector type (SnapOnly wall vs a real machine Input/Output pair,
//      e.g. splitter Output -> merger Input) - identical ~400-unit-stuck
//      failure regardless, ruling out walls/SnapOnly as a factor.
//   3. Absolute vs incremental hit updates - 40 repeated IDENTICAL target
//      hits (no movement, unsurprising) vs a smooth 40-step Lerp sweep
//      from the start position to the end position (ALSO zero movement
//      at every single step) - rules out both an absolute-jump model and
//      a delta/incremental model.
//   4. Physical surface reference - Hit.Component set to a real
//      UPrimitiveComponent from the target buildable (previously only
//      Hit.HitObjectHandle, the actor, was set) - no effect,
//      TrySnapToActor still returns false either way.
//   5. Genuinely elapsed real time - held for an EXTRA 60 real ticks
//      (~500ms, confirmed via real log timestamps) after construction
//      would otherwise proceed, SetControlRotation+UpdateHologramPlacement
//      reasserted every tick, height logged every 10 ticks - stayed at
//      EXACTLY 400.0 the entire time. Retested hands-off (a possible
//      mouse bump was flagged on the first run) - identical result.
//   6. CURRENTLY BEING TESTED (2026-08-31, not yet live-verified): all
//      five hypotheses above modified the FHitResult passed directly
//      into LiftHologram->UpdateHologramPlacement()/TrySnapToActor() -
//      but AFGBuildGun owns its OWN separate cached trace
//      (FHitResult& GetHitResult(), a MUTABLE reference getter - a
//      strong signal external code is meant to write to it, not just
//      read it; TraceForBuilding() refreshes it from the REAL camera
//      every AFGBuildGun::Tick()). If height is read from
//      BuildGun->GetHitResult() internally rather than from whatever
//      this mod passes into UpdateHologramPlacement() directly, none of
//      hypotheses 1-5 could ever have worked, since none of them touched
//      that member at all. Now writes directly into
//      BuildGun->GetHitResult() before each click, reasserted every poll
//      tick alongside the existing rotation/hit reassertion. WEAKENED
//      by further header research the same day (see #7) - FGHologram.h
//      documents SetHologramLocationAndRotation as taking hitResult by
//      EXPLICIT PARAMETER, not silently reading a BuildGun member - but
//      kept and tested anyway since it's cheap and something else in the
//      per-frame orchestration might still read it.
//   7. CURRENTLY BEING TESTED (2026-08-31, not yet live-verified, found
//      via FGHologram.h's doc comments rather than trial-and-error):
//      TrySnapToActor()'s doc comment says returning true means "no
//      further location and rotation will be updated this frame by the
//      build gun" - implying SetHologramLocationAndRotation() (which
//      almost certainly does the real height computation for lifts,
//      via the private UpdateTopTransform()) is called AUTOMATICALLY,
//      ONLY when TrySnapToActor() returns false, as part of
//      UpdateHologramPlacement()'s OWN internal orchestration - not
//      something this function needs to trigger separately at all. This
//      function calls UpdateHologramPlacement(hit) AND THEN a separate,
//      explicit LiftHologram->TrySnapToActor(hit) (kept for its own
//      return value, used in the diagnostic log) - if a failed
//      TrySnapToActor() resets height/transform state in preparation
//      for a fresh attempt, this REDUNDANT second call could be undoing
//      a correct height UpdateHologramPlacement() already computed
//      internally, on the very same line, one call earlier. Now logs
//      GetHeight() immediately after UpdateHologramPlacement(), BEFORE
//      the explicit TrySnapToActor() call, for both StartHit and EndHit
//      - if that log ever shows a non-400 height that then reverts to
//      400.0 by the very next log line, this is confirmed and the fix
//      is simply deleting the redundant explicit TrySnapToActor() calls.
//   8. CURRENTLY BEING TESTED (2026-08-31, not yet live-verified) - a
//      STRONGER, more literal reading of the SAME doc comments that
//      motivated #7, and probably the more likely of the two: "no
//      further location and rotation will be updated this frame BY THE
//      BUILD GUN" names the BUILD GUN, not the hologram itself, as the
//      thing that calls SetHologramLocationAndRotation() when
//      TrySnapToActor() fails - meaning that call may live inside
//      UFGBuildGunStateBuild::TickState_Implementation() (part of the
//      REAL per-frame build gun tick this function bypasses entirely by
//      calling hologram functions directly), NOT inside
//      UpdateHologramPlacement() itself as #7 assumed. If so,
//      UpdateHologramPlacement() genuinely never calls
//      SetHologramLocationAndRotation() on its own, TrySnapToActor()
//      returning false is a dead end with nothing following up, and
//      NOTHING in this function's code path has EVER called
//      SetHologramLocationAndRotation() at all, in any of the five
//      already-ruled-out hypotheses either - which would explain the
//      100%-consistent stuck-at-400 result far more directly than #7's
//      "redundant reset" theory does. Now calls
//      LiftHologram->SetHologramLocationAndRotation(hit) explicitly,
//      immediately after a failed TrySnapToActor(), matching the
//      documented precondition exactly ("will only be called if we have
//      a valid hit result and did not snap") - logs height right after,
//      for both StartHit and EndHit.
//
// Standing recommendation until #6/#7/#8 are confirmed either way:
// design platform/miner-interface heights as multiples of the ~400-unit
// default so no bridging is needed, per RPC_REFERENCE.md's
// world.connectConveyorLift section.
//
// Still true regardless of the above: a lift travels straight up/down
// only, X/Y locked to SourceConnection's real position - if the real
// destination isn't directly above/below the source, a separate
// ConstructConveyorBelt call is still needed to bridge the horizontal
// gap.
void UAIModFunctionLibrary::ConstructConveyorLift(UObject* WorldContextObject, const FString& SourceBuildableId, const FString& DestBuildableId, const FString& RecipeClassPath, int32 FreeEndRotationSteps, bool bDryRun, TFunction<void(const FAIModOperationResult&)> OnComplete)
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

	// Sets Hit.Component to a real UPrimitiveComponent (hypothesis #4 in
	// this function's doc comment - ruled out, kept since it's harmless
	// and matches what a real trace would populate anyway).
	auto MakeHitAt = [](AFGBuildable* Buildable, UFGFactoryConnectionComponent* Connection) -> FHitResult
	{
		FHitResult Hit;
		Hit.Location = Connection->GetConnectorLocation();
		Hit.ImpactPoint = Hit.Location;
		Hit.Normal = Connection->GetConnectorNormal();
		Hit.ImpactNormal = Hit.Normal;
		Hit.HitObjectHandle = FActorInstanceHandle(Buildable);
		Hit.bBlockingHit = true;
		if (UPrimitiveComponent* RealPrimitive = Buildable->FindComponentByClass<UPrimitiveComponent>())
		{
			Hit.Component = RealPrimitive;
		}
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

	// Player-independence rotation (hypothesis #1 in this function's doc
	// comment - ruled out on its own, kept since a deterministic look
	// direction is still needed for general player-independence, matching
	// ConstructConveyorBelt's identical pattern). Computed from the
	// PLAYER's real location, not the source connector's - see hypothesis
	// #1's writeup for why that distinction mattered.
	//
	// Take 4 (2026-08-31, found during offline research, NOT YET
	// LIVE-TESTED): take 3 used Character->GetActorLocation(), the
	// pawn's root/capsule position - NOT the same as the camera. Found
	// AFGCharacterPlayer::GetCameraComponentWorldLocation(), a real,
	// dedicated accessor for the actual first-person camera position,
	// which is offset upward from the capsule root by roughly eye
	// height. If any camera-position-based logic is involved in height/
	// placement (per hypotheses #6/#7's reasoning), using the capsule
	// root instead of the real eye position bakes in a systematic
	// vertical error on top of whatever #6/#7 do or don't fix - using
	// the real camera location now regardless.
	const FRotator LiftDeterministicLook = (DestConnection->GetConnectorLocation() - Character->GetCameraComponentWorldLocation()).Rotation();
	if (AController* LiftController = Character->GetController())
	{
		LiftController->SetControlRotation(LiftDeterministicLook);
	}

	const FHitResult StartHit = MakeHitAt(SourceBuildable, SourceConnection);
	const bool bStartHitValid = LiftHologram->IsValidHitResult(StartHit);
	LiftHologram->UpdateHologramPlacement(StartHit);
	// Hypothesis #7 (2026-08-31, NOT YET LIVE-TESTED) - see this
	// function's doc comment. UpdateHologramPlacement() is documented to
	// already call TrySnapToActor()/SetHologramLocationAndRotation()
	// internally with whatever hit it's given - the explicit, SEPARATE
	// TrySnapToActor() call below (kept for its own return value, used
	// in the diagnostic log) may be redundant, and if a failed
	// TrySnapToActor() resets height/transform state in preparation for
	// a fresh attempt, this redundant second call could be undoing a
	// correct height UpdateHologramPlacement() just computed internally.
	// Logging height BEFORE the explicit call to check.
	UE_LOG(LogAIModAI, Display, TEXT("ConstructConveyorLift: height immediately after UpdateHologramPlacement(StartHit), before explicit TrySnapToActor=%.1f"), LiftHologram->GetHeight());
	// Hypothesis #6 (2026-08-31, NOT YET LIVE-TESTED) - see this
	// function's doc comment. Injects the same hit directly into the
	// build gun's own cached trace, in case height is read from there
	// rather than from whatever's passed into UpdateHologramPlacement().
	BuildGun->GetHitResult() = StartHit;
	const bool bStartSnapped = LiftHologram->TrySnapToActor(StartHit);
	// Hypothesis #8 (2026-08-31, NOT YET LIVE-TESTED) - see this
	// function's doc comment. TrySnapToActor()'s doc comment names "the
	// build gun" (not the hologram itself) as whatever calls
	// SetHologramLocationAndRotation() when snapping fails - meaning
	// UpdateHologramPlacement() may NOT call it internally after all
	// (weakening #7's "redundant call" theory), and since this function
	// bypasses the real build gun's TickState_Implementation entirely,
	// NOTHING may ever call SetHologramLocationAndRotation() in this
	// code path at all. Calling it explicitly here, matching the
	// documented precondition exactly ("only be called if we have a
	// valid hit result and did not snap").
	if (bStartHitValid && !bStartSnapped)
	{
		LiftHologram->SetHologramLocationAndRotation(StartHit);
		UE_LOG(LogAIModAI, Display, TEXT("ConstructConveyorLift: height after explicit SetHologramLocationAndRotation(StartHit)=%.1f"), LiftHologram->GetHeight());
	}
	const bool bStartStepComplete = LiftHologram->DoMultiStepPlacement(true);

	UE_LOG(LogAIModAI, Display, TEXT("ConstructConveyorLift: source=%s dest=%s after start click: hitValid=%s snapped=%s stepComplete=%s height=%.1f disqualifiers=[%s]"),
		*SourceBuildableId, *DestBuildableId, bStartHitValid ? TEXT("true") : TEXT("false"), bStartSnapped ? TEXT("true") : TEXT("false"), bStartStepComplete ? TEXT("true") : TEXT("false"), LiftHologram->GetHeight(), *SummarizeDisqualifiers(LiftHologram));

	if (bStartStepComplete)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("UNEXPECTED_STEP_COMPLETE"), TEXT("DoMultiStepPlacement() reported complete after only the start click")));
		return;
	}

	const FHitResult EndHit = MakeHitAt(DestBuildable, DestConnection);
	const bool bEndHitValid = LiftHologram->IsValidHitResult(EndHit);
	LiftHologram->UpdateHologramPlacement(EndHit);
	// Hypothesis #7 (2026-08-31) - same check as on StartHit above.
	UE_LOG(LogAIModAI, Display, TEXT("ConstructConveyorLift: height immediately after UpdateHologramPlacement(EndHit), before explicit TrySnapToActor=%.1f"), LiftHologram->GetHeight());
	// Hypothesis #6 (2026-08-31, NOT YET LIVE-TESTED) - see this
	// function's doc comment and the identical injection on StartHit
	// above.
	BuildGun->GetHitResult() = EndHit;
	const bool bEndSnapped = LiftHologram->TrySnapToActor(EndHit);
	// Hypothesis #8 (2026-08-31, NOT YET LIVE-TESTED) - same as on
	// StartHit above, see this function's doc comment.
	if (bEndHitValid && !bEndSnapped)
	{
		LiftHologram->SetHologramLocationAndRotation(EndHit);
		UE_LOG(LogAIModAI, Display, TEXT("ConstructConveyorLift: height after explicit SetHologramLocationAndRotation(EndHit)=%.1f"), LiftHologram->GetHeight());
	}

	// FreeEndRotationSteps (2026-08-31): rotates the still-unconnected
	// end in 90-degree increments via ScrollRotate() BEFORE the final
	// click, mirroring ConstructBuildingAtPosition's established
	// Scroll()-called-N-times-per-notch pattern (see its comment) - a
	// real player can only do this while the hologram is still being
	// placed, per the user, matching why world.setBuildableRotation
	// failed silently on an already-built lift.
	//
	// Explicit zero-reset FIRST, unconditionally (2026-08-31, per the
	// user's own description: a fresh lift's free-end orientation "may
	// default to the orientation of the last-placed lift" - if
	// mScrollRotation is a value that persists/carries forward across
	// hologram instances rather than resetting per-spawn, ScrollRotate()
	// calls here would land on top of an unpredictable inherited
	// baseline, not a clean zero - defeating the whole point of exposing
	// this as a deterministic RPC param. SetScrollRotateValue(0) forces a
	// known starting point every call, whether or not rotation was
	// requested, so the NO-rotation-requested default also becomes
	// deterministic instead of inheriting whatever a previous build left
	// behind. NOT YET LIVE-TESTED - including whether 0 is really the
	// hologram's own "natural"/unrotated baseline value.
	LiftHologram->SetScrollRotateValue(0);
	if (FreeEndRotationSteps != 0)
	{
		const int32 ScrollRotateStep = FreeEndRotationSteps > 0 ? 1 : -1;
		for (int32 i = 0; i < FMath::Abs(FreeEndRotationSteps); ++i)
		{
			LiftHologram->ScrollRotate(ScrollRotateStep, 90);
		}
	}
	UE_LOG(LogAIModAI, Display, TEXT("ConstructConveyorLift: freeEndRotationSteps=%d scrollRotateValue after reset+rotate=%d"),
		FreeEndRotationSteps, LiftHologram->GetScrollRotateValue());

	const bool bEndStepComplete = LiftHologram->DoMultiStepPlacement(true);

	UE_LOG(LogAIModAI, Display, TEXT("ConstructConveyorLift: source=%s dest=%s after end click: hitValid=%s snapped=%s stepComplete=%s height=%.1f expectedHeight=%.1f disqualifiers=[%s]"),
		*SourceBuildableId, *DestBuildableId, bEndHitValid ? TEXT("true") : TEXT("false"), bEndSnapped ? TEXT("true") : TEXT("false"), bEndStepComplete ? TEXT("true") : TEXT("false"), LiftHologram->GetHeight(),
		DestConnection->GetConnectorLocation().Z - SourceConnection->GetConnectorLocation().Z, *SummarizeDisqualifiers(LiftHologram));

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
		TWeakObjectPtr<AFGBuildGun> BuildGun; // hypothesis #6, see below
		TWeakObjectPtr<UWorld> World;
		FString SourceBuildableId;
		FString DestBuildableId;
		bool bDryRun = true;
		FRotator DeterministicLook;
		FHitResult EndHit; // re-asserted every poll tick, see below
		int32 AttemptsRemaining = 120; // safety cap - real ticks, not a fixed duration
		int32 AttemptsTaken = 0;
		TFunction<void(const FAIModOperationResult&)> OnComplete;
	};
	const TSharedRef<FPollState> PollState = MakeShared<FPollState>();
	PollState->Hologram = LiftHologram;
	PollState->Character = Character;
	PollState->BuildGun = BuildGun;
	PollState->World = World;
	PollState->SourceBuildableId = SourceBuildableId;
	PollState->DestBuildableId = DestBuildableId;
	PollState->bDryRun = bDryRun;
	PollState->DeterministicLook = LiftDeterministicLook;
	PollState->EndHit = EndHit;
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

		// Re-assert the end hit every poll tick (2026-08-30) - same fix
		// as ConstructConveyorBelt_RealCharacterStrategy, applied here
		// for the same reason (see that function's comment for the full
		// TickState_Implementation live-camera-trace rationale).
		PollHologram->UpdateHologramPlacement(PollState->EndHit);

		// Hypothesis #6 (2026-08-31, NOT YET LIVE-TESTED) - see this
		// function's doc comment. Reasserted every tick alongside the
		// above, same rationale.
		if (AFGBuildGun* PollBuildGun = PollState->BuildGun.Get())
		{
			PollBuildGun->GetHitResult() = PollState->EndHit;
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

		UE_LOG(LogAIModAI, Display, TEXT("ConstructConveyorLift (initializing cleared): height=%.1f"), PollHologram->GetHeight());

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

		UE_LOG(LogAIModAI, Display, TEXT("ConstructConveyorLift (deferred): height right before InternalConstructHologram=%.1f"), PollHologram->GetHeight());

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
		FHitResult EndHit; // re-asserted every poll tick, see below
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
	PollState->EndHit = EndHit;
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

		// Re-assert the end hit every poll tick (2026-08-30) - same fix
		// as ConstructConveyorBelt_RealCharacterStrategy, applied here
		// for the same reason.
		PollHologram->UpdateHologramPlacement(PollState->EndHit);

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
		FHitResult EndHit; // re-asserted every poll tick, see below
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
	PollState->EndHit = EndHit;
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

		// Re-assert the end hit every poll tick (2026-08-30) - same fix
		// as ConstructConveyorBelt_RealCharacterStrategy, applied here
		// for the same reason.
		PollHologram->UpdateHologramPlacement(PollState->EndHit);

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

// Railroad tracks (2026-08-29) - researched from source before
// implementing: AFGRailroadTrackHologram : AFGSplineHologram, the exact
// same base ConstructPipe/ConstructConveyorBelt already drive
// (GetConstructDisqualifiers/CanConstruct/TrySnapToActor/
// DoMultiStepPlacement/GetCurrentBuildStep are all AFGSplineHologram
// members) - this is a near-mirror of ConstructPipe, same two-click
// snap-to-connector-component flow, differing only in the connector type
// (UFGRailroadTrackConnectionComponent via FindFreeRailroadConnection,
// bidirectional - no producer/consumer split). Switches and signals are
// deliberately out of scope (see FindFreeRailroadConnection's comment) -
// this only builds a single point-to-point segment between two existing
// connector-bearing buildables (e.g. two Train Station platforms, or an
// existing track's open end).
void UAIModFunctionLibrary::ConstructRailroadTrack(UObject* WorldContextObject, const FString& SourceBuildableId, const FString& DestBuildableId, const FString& RecipeClassPath, bool bDryRun, TFunction<void(const FAIModOperationResult&)> OnComplete)
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

	UFGRailroadTrackConnectionComponent* SourceConnection = FindFreeRailroadConnection(SourceBuildable);
	if (!SourceConnection)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_RAILROAD_CONNECTION"), FString::Printf(TEXT("'%s' has no free railroad track connection component"), *SourceBuildableId)));
		return;
	}
	UFGRailroadTrackConnectionComponent* DestConnection = FindFreeRailroadConnection(DestBuildable);
	if (!DestConnection)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_RAILROAD_CONNECTION"), FString::Printf(TEXT("'%s' has no free railroad track connection component"), *DestBuildableId)));
		return;
	}

	UClass* TrackRecipeClass = LoadObject<UClass>(nullptr, *RecipeClassPath);
	if (!TrackRecipeClass || !TrackRecipeClass->IsChildOf(UFGRecipe::StaticClass()))
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("INVALID_RECIPE"), FString::Printf(TEXT("'%s' did not resolve to a UFGRecipe subclass"), *RecipeClassPath)));
		return;
	}
	const TSubclassOf<UFGRecipe> RecipeClass = TrackRecipeClass;

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
	AFGRailroadTrackHologram* TrackHologram = Cast<AFGRailroadTrackHologram>(Hologram);
	if (!TrackHologram)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("HOLOGRAM_SPAWN_FAILED"),
			FString::Printf(TEXT("HotKeyRecipe(%s) did not result in an AFGRailroadTrackHologram (got %s)"),
				*RecipeClassPath, Hologram ? *Hologram->GetClass()->GetName() : TEXT("null"))));
		return;
	}

	auto MakeHitAt = [](AFGBuildable* Buildable, UFGRailroadTrackConnectionComponent* Connection) -> FHitResult
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

	auto SummarizeDisqualifiers = [](AFGRailroadTrackHologram* H) -> FString
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

	// Player-independence from day one - see ConstructPipe's comment for
	// the full incident this pattern fixes.
	const FRotator TrackDeterministicLook = (DestConnection->GetConnectorLocation() - SourceConnection->GetConnectorLocation()).Rotation();
	if (AController* TrackController = Character->GetController())
	{
		TrackController->SetControlRotation(TrackDeterministicLook);
	}

	const FHitResult StartHit = MakeHitAt(SourceBuildable, SourceConnection);
	TrackHologram->UpdateHologramPlacement(StartHit);
	TrackHologram->TrySnapToActor(StartHit);
	const bool bStartStepComplete = TrackHologram->DoMultiStepPlacement(true);
	const ESplineHologramBuildStep StepAfterStart = TrackHologram->GetCurrentBuildStep();

	UE_LOG(LogAIModAI, Display, TEXT("ConstructRailroadTrack: source=%s dest=%s after start click: stepComplete=%s step=%d disqualifiers=[%s]"),
		*SourceBuildableId, *DestBuildableId, bStartStepComplete ? TEXT("true") : TEXT("false"), static_cast<int32>(StepAfterStart), *SummarizeDisqualifiers(TrackHologram));

	if (bStartStepComplete)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("UNEXPECTED_STEP_COMPLETE"), TEXT("DoMultiStepPlacement() reported complete after only the start click")));
		return;
	}

	const FHitResult EndHit = MakeHitAt(DestBuildable, DestConnection);
	TrackHologram->UpdateHologramPlacement(EndHit);
	TrackHologram->TrySnapToActor(EndHit);
	const bool bEndStepComplete = TrackHologram->DoMultiStepPlacement(true);
	const ESplineHologramBuildStep StepAfterEnd = TrackHologram->GetCurrentBuildStep();
	const bool bEndConnectionSnapped = TrackHologram->IsConnectionSnapped(false);

	UE_LOG(LogAIModAI, Display, TEXT("ConstructRailroadTrack: source=%s dest=%s after end click: stepComplete=%s step=%d connectionSnapped=%s disqualifiers=[%s]"),
		*SourceBuildableId, *DestBuildableId, bEndStepComplete ? TEXT("true") : TEXT("false"), static_cast<int32>(StepAfterEnd), bEndConnectionSnapped ? TEXT("true") : TEXT("false"), *SummarizeDisqualifiers(TrackHologram));

	if (!bEndStepComplete)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("PLACEMENT_INCOMPLETE"),
			FString::Printf(TEXT("DoMultiStepPlacement() did not report complete after the end click - step=%d connectionSnapped=%s, may need a third step"), static_cast<int32>(StepAfterEnd), bEndConnectionSnapped ? TEXT("true") : TEXT("false"))));
		return;
	}

	struct FPollState
	{
		TWeakObjectPtr<AFGRailroadTrackHologram> Hologram;
		TWeakObjectPtr<AFGCharacterPlayer> Character;
		TWeakObjectPtr<UWorld> World;
		FString SourceBuildableId;
		FString DestBuildableId;
		bool bDryRun = true;
		FRotator DeterministicLook;
		FHitResult EndHit; // re-asserted every poll tick, see below
		int32 AttemptsRemaining = 120;
		int32 AttemptsTaken = 0;
		TFunction<void(const FAIModOperationResult&)> OnComplete;
	};
	const TSharedRef<FPollState> PollState = MakeShared<FPollState>();
	PollState->Hologram = TrackHologram;
	PollState->Character = Character;
	PollState->World = World;
	PollState->SourceBuildableId = SourceBuildableId;
	PollState->DestBuildableId = DestBuildableId;
	PollState->bDryRun = bDryRun;
	PollState->DeterministicLook = TrackDeterministicLook;
	PollState->EndHit = EndHit;
	PollState->OnComplete = MoveTemp(OnComplete);

	const TSharedRef<TFunction<void()>> PollFn = MakeShared<TFunction<void()>>();
	*PollFn = [PollState, PollFn]()
	{
		++PollState->AttemptsTaken;

		AFGRailroadTrackHologram* PollHologram = PollState->Hologram.Get();
		UWorld* PollWorld = PollState->World.Get();
		AFGCharacterPlayer* PollCharacter = PollState->Character.Get();
		if (!IsValid(PollHologram) || !PollWorld)
		{
			UE_LOG(LogAIModAI, Warning, TEXT("ConstructRailroadTrack (deferred): hologram or world became invalid while polling (after %d tick(s))"), PollState->AttemptsTaken);
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

		// Re-assert the end hit every poll tick (2026-08-30) - same fix
		// as ConstructConveyorBelt_RealCharacterStrategy, applied here
		// for the same reason.
		PollHologram->UpdateHologramPlacement(PollState->EndHit);

		TArray<TSubclassOf<UFGConstructDisqualifier>> Disqualifiers;
		PollHologram->GetConstructDisqualifiers(Disqualifiers);
		const bool bStillInitializing = Disqualifiers.Contains(TSubclassOf<UFGConstructDisqualifier>(UFGCDInitializing::StaticClass()));

		--PollState->AttemptsRemaining;
		if (bStillInitializing && PollState->AttemptsRemaining > 0)
		{
			PollWorld->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
			return;
		}

		// No bIgnore* bypass flags here, deliberately - UFGCDTrackTooLong/
		// TooShort/TooSteep/TrunToSharp (sic - real name typo in source)
		// must always block construction, matching UFGCDWireTooLong
		// elsewhere in this file. Only UnlimitedResources (a player-
		// controlled mod setting) and the always-ignored aim-location
		// disqualifier get any leniency.
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

		UE_LOG(LogAIModAI, Display, TEXT("ConstructRailroadTrack (deferred, resolved after %d real tick(s)): source=%s dest=%s dryRun=%s canConstruct=%s disqualifiers=[%s]"),
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
			UE_LOG(LogAIModAI, Error, TEXT("ConstructRailroadTrack (deferred): lost the build state before constructing - aborting, nothing built"));
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("Lost the build state before constructing")));
			return;
		}

		PollBuildState->InternalConstructHologram(ConstructionID);

		UE_LOG(LogAIModAI, Display, TEXT("ConstructRailroadTrack (deferred, resolved after %d real tick(s)): construction attempted via InternalConstructHologram - source=%s dest=%s"),
			PollState->AttemptsTaken, *PollState->SourceBuildableId, *PollState->DestBuildableId);

		if (IsValid(PollCharacter))
		{
			PollCharacter->UnequipBuildGun();
		}

		PollState->OnComplete(FAIModOperationResult::Success());
	};

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
}

// Vehicle path segments (2026-08-29) - researched from source before
// implementing: AFGVehiclePathSegmentHologram : AFGBuildableHologram
// directly (NOT AFGSplineHologram, unlike belts/pipes/tracks), but
// implements the identical TrySnapToActor+DoMultiStepPlacement two-click
// contract on its own terms (EVehiclePathBuildStep{StartPoint,EndPoint}).
// Unlike every other spline-ish Construct* function here, source/dest are
// NOT existing buildables with connectors - path nodes are free and
// auto-created by segment placement (confirmed from source:
// AFGVehiclePathSegment::SetNodeConnections's own doc comment, "Null
// connections will be automatically initialized to fresh nodes"), so this
// takes literal X/Y/Z for both ends instead, same ignoreGroundTrace/
// literal-Z convention as ConstructVehicle's free-placement branch -
// directly serves the "lay it on a flat platform, not raw terrain"
// approach. Passing a point near an existing AFGVehiclePathNode/
// AFGVehiclePathSegment (within mSegmentEndPointSnapDistance, 800cm per
// source) lets the hologram's own TrySnapToActor connect to it instead of
// creating a new node - not specially handled here, same "let the real
// engine trace decide" posture as ConstructExtractorOnNode.
void UAIModFunctionLibrary::ConstructVehiclePathSegment(UObject* WorldContextObject, const FString& RecipeClassPath, float StartX, float StartY, float StartZ, float EndX, float EndY, float EndZ, bool bIgnoreGroundTrace, TFunction<void(const FAIModOperationResult&)> OnComplete)
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

	UClass* ResolvedClass = LoadObject<UClass>(nullptr, *RecipeClassPath);
	if (!ResolvedClass || !ResolvedClass->IsChildOf(UFGRecipe::StaticClass()))
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("INVALID_RECIPE"), FString::Printf(TEXT("'%s' did not resolve to a UFGRecipe subclass"), *RecipeClassPath)));
		return;
	}
	const TSubclassOf<UFGRecipe> RecipeClass = ResolvedClass;

	if (bIgnoreGroundTrace && (StartZ <= -1000000.0f || EndZ <= -1000000.0f))
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("MISSING_REFERENCE_Z"),
			TEXT("bIgnoreGroundTrace requires explicit startZ and endZ - there is no ground trace to fall back to")));
		return;
	}

	auto MakeHit = [World, Character, bIgnoreGroundTrace](float X, float Y, float Z) -> FHitResult
	{
		FHitResult Hit;
		if (bIgnoreGroundTrace)
		{
			Hit.Location = FVector(X, Y, Z);
			Hit.ImpactPoint = Hit.Location;
			Hit.Normal = FVector::UpVector;
			Hit.ImpactNormal = FVector::UpVector;
			Hit.bBlockingHit = true;
		}
		else
		{
			const float ZSearchCenter = (Z > -1000000.0f) ? Z : Character->GetActorLocation().Z;
			const FGroundTraceResult GroundTrace = FindGroundAtXY(World, X, Y, ZSearchCenter, Character);
			Hit = GroundTrace.Hit;
		}
		return Hit;
	};

	const FHitResult StartHitPreview = MakeHit(StartX, StartY, StartZ);
	const FHitResult EndHitPreview = MakeHit(EndX, EndY, EndZ);

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

	AFGVehiclePathSegmentHologram* PathHologram = Cast<AFGVehiclePathSegmentHologram>(BuildState->GetHologram());
	if (!PathHologram)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("HOLOGRAM_SPAWN_FAILED"),
			FString::Printf(TEXT("HotKeyRecipe(%s) did not result in an AFGVehiclePathSegmentHologram (got %s)"),
				*RecipeClassPath, BuildState->GetHologram() ? *BuildState->GetHologram()->GetClass()->GetName() : TEXT("null"))));
		return;
	}

	auto SummarizeDisqualifiers = [](AFGVehiclePathSegmentHologram* H) -> FString
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

	const FRotator PathDeterministicLook = (EndHitPreview.Location - StartHitPreview.Location).Rotation();
	if (AController* PathController = Character->GetController())
	{
		PathController->SetControlRotation(PathDeterministicLook);
	}

	PathHologram->UpdateHologramPlacement(StartHitPreview);
	PathHologram->TrySnapToActor(StartHitPreview);
	const bool bStartStepComplete = PathHologram->DoMultiStepPlacement(true);

	UE_LOG(LogAIModAI, Display, TEXT("ConstructVehiclePathSegment: start=(%.0f,%.0f,%.0f) end=(%.0f,%.0f,%.0f) after start click: stepComplete=%s disqualifiers=[%s]"),
		StartX, StartY, StartZ, EndX, EndY, EndZ, bStartStepComplete ? TEXT("true") : TEXT("false"), *SummarizeDisqualifiers(PathHologram));

	if (bStartStepComplete)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("UNEXPECTED_STEP_COMPLETE"), TEXT("DoMultiStepPlacement() reported complete after only the start click")));
		return;
	}

	PathHologram->UpdateHologramPlacement(EndHitPreview);
	PathHologram->TrySnapToActor(EndHitPreview);
	const bool bEndStepComplete = PathHologram->DoMultiStepPlacement(true);

	UE_LOG(LogAIModAI, Display, TEXT("ConstructVehiclePathSegment: start=(%.0f,%.0f,%.0f) end=(%.0f,%.0f,%.0f) after end click: stepComplete=%s disqualifiers=[%s]"),
		StartX, StartY, StartZ, EndX, EndY, EndZ, bEndStepComplete ? TEXT("true") : TEXT("false"), *SummarizeDisqualifiers(PathHologram));

	if (!bEndStepComplete)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("PLACEMENT_INCOMPLETE"), TEXT("DoMultiStepPlacement() did not report complete after the end click")));
		return;
	}

	struct FPollState
	{
		TWeakObjectPtr<AFGVehiclePathSegmentHologram> Hologram;
		TWeakObjectPtr<AFGCharacterPlayer> Character;
		TWeakObjectPtr<UWorld> World;
		FRotator DeterministicLook;
		FHitResult EndHit; // re-asserted every poll tick, see below
		int32 AttemptsRemaining = 120;
		int32 AttemptsTaken = 0;
		TFunction<void(const FAIModOperationResult&)> OnComplete;
	};
	const TSharedRef<FPollState> PollState = MakeShared<FPollState>();
	PollState->Hologram = PathHologram;
	PollState->Character = Character;
	PollState->World = World;
	PollState->DeterministicLook = PathDeterministicLook;
	PollState->EndHit = EndHitPreview;
	PollState->OnComplete = MoveTemp(OnComplete);

	const TSharedRef<TFunction<void()>> PollFn = MakeShared<TFunction<void()>>();
	*PollFn = [PollState, PollFn]()
	{
		++PollState->AttemptsTaken;

		AFGVehiclePathSegmentHologram* PollHologram = PollState->Hologram.Get();
		UWorld* PollWorld = PollState->World.Get();
		AFGCharacterPlayer* PollCharacter = PollState->Character.Get();
		if (!IsValid(PollHologram) || !PollWorld)
		{
			UE_LOG(LogAIModAI, Warning, TEXT("ConstructVehiclePathSegment (deferred): hologram or world became invalid while polling (after %d tick(s))"), PollState->AttemptsTaken);
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

		// Re-assert the end hit every poll tick (2026-08-30) - same fix
		// as ConstructConveyorBelt_RealCharacterStrategy, applied here
		// for the same reason.
		PollHologram->UpdateHologramPlacement(PollState->EndHit);

		TArray<TSubclassOf<UFGConstructDisqualifier>> Disqualifiers;
		PollHologram->GetConstructDisqualifiers(Disqualifiers);
		const bool bStillInitializing = Disqualifiers.Contains(TSubclassOf<UFGConstructDisqualifier>(UFGCDInitializing::StaticClass()));

		--PollState->AttemptsRemaining;
		if (bStillInitializing && PollState->AttemptsRemaining > 0)
		{
			PollWorld->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
			return;
		}

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

		UE_LOG(LogAIModAI, Display, TEXT("ConstructVehiclePathSegment (deferred, resolved after %d real tick(s)): canConstruct=%s disqualifiers=[%s]"),
			PollState->AttemptsTaken, bCanConstruct ? TEXT("true") : TEXT("false"), *DisqualifierSummary);

		if (!bCanConstruct)
		{
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
			UE_LOG(LogAIModAI, Error, TEXT("ConstructVehiclePathSegment (deferred): lost the build state before constructing - aborting, nothing built"));
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("Lost the build state before constructing")));
			return;
		}

		PollBuildState->InternalConstructHologram(ConstructionID);

		UE_LOG(LogAIModAI, Display, TEXT("ConstructVehiclePathSegment (deferred, resolved after %d real tick(s)): construction attempted via InternalConstructHologram"),
			PollState->AttemptsTaken);

		if (IsValid(PollCharacter))
		{
			PollCharacter->UnequipBuildGun();
		}

		PollState->OnComplete(FAIModOperationResult::Success());
	};

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
}

namespace
{
	// Beam build-mode fields (mBuildModeDiagonal/mBuildModeFreeForm) are
	// protected TSubclassOf<UFGHologramBuildModeDescriptor> with no public
	// getter - reflected as FObjectPropertyBase (TSubclassOf boxes to a
	// UClass* value), same reflection posture as every other
	// no-public-getter field this project reads (mMaxIncline,
	// mPotentialShardSlots, etc), just the first FObjectPropertyBase
	// instance rather than FFloatProperty/FIntProperty/FBoolProperty.
	TSubclassOf<UFGHologramBuildModeDescriptor> ReadBeamBuildModeProperty(const AFGBeamHologram* Hologram, const TCHAR* PropertyName)
	{
		if (!Hologram) { return nullptr; }
		const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Hologram->GetClass(), PropertyName);
		if (!Property) { return nullptr; }
		return TSubclassOf<UFGHologramBuildModeDescriptor>(Cast<UClass>(Property->GetObjectPropertyValue_InContainer(Hologram)));
	}
}

// See ConstructBeam's doc comment in the header for the real
// AFGBeamHologram sourcing, the build-mode reflection reasoning, and the
// (unconfirmed) RotationScrollSteps timing/meaning.
void UAIModFunctionLibrary::ConstructBeam(UObject* WorldContextObject, const FString& RecipeClassPath, float StartX, float StartY, float StartZ, float EndX, float EndY, float EndZ, bool bIgnoreGroundTrace, bool bFreeformMode, int32 RotationScrollSteps, TFunction<void(const FAIModOperationResult&)> OnComplete)
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

	UClass* ResolvedClass = LoadObject<UClass>(nullptr, *RecipeClassPath);
	if (!ResolvedClass || !ResolvedClass->IsChildOf(UFGRecipe::StaticClass()))
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("INVALID_RECIPE"), FString::Printf(TEXT("'%s' did not resolve to a UFGRecipe subclass"), *RecipeClassPath)));
		return;
	}
	const TSubclassOf<UFGRecipe> RecipeClass = ResolvedClass;

	if (bIgnoreGroundTrace && (StartZ <= -1000000.0f || EndZ <= -1000000.0f))
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("MISSING_REFERENCE_Z"),
			TEXT("bIgnoreGroundTrace requires explicit startZ and endZ - there is no ground trace to fall back to")));
		return;
	}

	auto MakeHit = [World, Character, bIgnoreGroundTrace](float X, float Y, float Z) -> FHitResult
	{
		FHitResult Hit;
		if (bIgnoreGroundTrace)
		{
			Hit.Location = FVector(X, Y, Z);
			Hit.ImpactPoint = Hit.Location;
			Hit.Normal = FVector::UpVector;
			Hit.ImpactNormal = FVector::UpVector;
			Hit.bBlockingHit = true;
		}
		else
		{
			const float ZSearchCenter = (Z > -1000000.0f) ? Z : Character->GetActorLocation().Z;
			const FGroundTraceResult GroundTrace = FindGroundAtXY(World, X, Y, ZSearchCenter, Character);
			Hit = GroundTrace.Hit;
		}
		return Hit;
	};

	const FHitResult StartHitPreview = MakeHit(StartX, StartY, StartZ);
	const FHitResult EndHitPreview = MakeHit(EndX, EndY, EndZ);

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

	AFGBeamHologram* BeamHologram = Cast<AFGBeamHologram>(BuildState->GetHologram());
	if (!BeamHologram)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("HOLOGRAM_SPAWN_FAILED"),
			FString::Printf(TEXT("HotKeyRecipe(%s) did not result in an AFGBeamHologram (got %s)"),
				*RecipeClassPath, BuildState->GetHologram() ? *BuildState->GetHologram()->GetClass()->GetName() : TEXT("null"))));
		return;
	}

	// Build-mode selection - see header doc comment. Missing property
	// (nullptr) just means the override is skipped, not a hard failure -
	// the hologram's own mDefaultBuildMode still applies.
	const TSubclassOf<UFGHologramBuildModeDescriptor> RequestedBuildMode = ReadBeamBuildModeProperty(
		BeamHologram, bFreeformMode ? TEXT("mBuildModeFreeForm") : TEXT("mBuildModeDiagonal"));
	if (RequestedBuildMode)
	{
		BeamHologram->SetBuildModeOverride(RequestedBuildMode);
	}

	auto SummarizeDisqualifiers = [](AFGBeamHologram* H) -> FString
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

	const FRotator BeamDeterministicLook = (EndHitPreview.Location - StartHitPreview.Location).Rotation();
	if (AController* BeamController = Character->GetController())
	{
		BeamController->SetControlRotation(BeamDeterministicLook);
	}

	BeamHologram->UpdateHologramPlacement(StartHitPreview);
	BeamHologram->TrySnapToActor(StartHitPreview);
	const bool bStartStepComplete = BeamHologram->DoMultiStepPlacement(true);

	UE_LOG(LogAIModAI, Display, TEXT("ConstructBeam: start=(%.0f,%.0f,%.0f) end=(%.0f,%.0f,%.0f) freeform=%s after start click: stepComplete=%s disqualifiers=[%s]"),
		StartX, StartY, StartZ, EndX, EndY, EndZ, bFreeformMode ? TEXT("true") : TEXT("false"), bStartStepComplete ? TEXT("true") : TEXT("false"), *SummarizeDisqualifiers(BeamHologram));

	if (bStartStepComplete)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("UNEXPECTED_STEP_COMPLETE"), TEXT("DoMultiStepPlacement() reported complete after only the start click")));
		return;
	}

	BeamHologram->UpdateHologramPlacement(EndHitPreview);
	BeamHologram->TrySnapToActor(EndHitPreview);

	// RotationScrollSteps - see header doc comment for why this timing
	// (after the end hit, before finalizing) and the real
	// GetRotationStep() query (0/negative means "no override", per its
	// own doc comment) instead of a hardcoded 90.
	const int32 RawRotationStep = BeamHologram->GetRotationStep();
	const int32 EffectiveRotationStep = RawRotationStep > 0 ? RawRotationStep : 90;
	BeamHologram->SetScrollRotateValue(0);
	if (RotationScrollSteps != 0)
	{
		const int32 ScrollDirection = RotationScrollSteps > 0 ? 1 : -1;
		for (int32 i = 0; i < FMath::Abs(RotationScrollSteps); ++i)
		{
			BeamHologram->ScrollRotate(ScrollDirection, EffectiveRotationStep);
		}
	}

	const bool bEndStepComplete = BeamHologram->DoMultiStepPlacement(true);

	UE_LOG(LogAIModAI, Display, TEXT("ConstructBeam: start=(%.0f,%.0f,%.0f) end=(%.0f,%.0f,%.0f) rotationStep=%d rotationScrollSteps=%d after end click: stepComplete=%s disqualifiers=[%s]"),
		StartX, StartY, StartZ, EndX, EndY, EndZ, RawRotationStep, RotationScrollSteps, bEndStepComplete ? TEXT("true") : TEXT("false"), *SummarizeDisqualifiers(BeamHologram));

	if (!bEndStepComplete)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("PLACEMENT_INCOMPLETE"), TEXT("DoMultiStepPlacement() did not report complete after the end click")));
		return;
	}

	struct FPollState
	{
		TWeakObjectPtr<AFGBeamHologram> Hologram;
		TWeakObjectPtr<AFGCharacterPlayer> Character;
		TWeakObjectPtr<UWorld> World;
		FRotator DeterministicLook;
		FHitResult EndHit; // re-asserted every poll tick, see below
		int32 AttemptsRemaining = 120;
		int32 AttemptsTaken = 0;
		TFunction<void(const FAIModOperationResult&)> OnComplete;
	};
	const TSharedRef<FPollState> PollState = MakeShared<FPollState>();
	PollState->Hologram = BeamHologram;
	PollState->Character = Character;
	PollState->World = World;
	PollState->DeterministicLook = BeamDeterministicLook;
	PollState->EndHit = EndHitPreview;
	PollState->OnComplete = MoveTemp(OnComplete);

	const TSharedRef<TFunction<void()>> PollFn = MakeShared<TFunction<void()>>();
	*PollFn = [PollState, PollFn]()
	{
		++PollState->AttemptsTaken;

		AFGBeamHologram* PollHologram = PollState->Hologram.Get();
		UWorld* PollWorld = PollState->World.Get();
		AFGCharacterPlayer* PollCharacter = PollState->Character.Get();
		if (!IsValid(PollHologram) || !PollWorld)
		{
			UE_LOG(LogAIModAI, Warning, TEXT("ConstructBeam (deferred): hologram or world became invalid while polling (after %d tick(s))"), PollState->AttemptsTaken);
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

		// Re-assert the end hit every poll tick - same fix as
		// ConstructConveyorBelt_RealCharacterStrategy/
		// ConstructVehiclePathSegment, applied here for the same reason.
		PollHologram->UpdateHologramPlacement(PollState->EndHit);

		TArray<TSubclassOf<UFGConstructDisqualifier>> Disqualifiers;
		PollHologram->GetConstructDisqualifiers(Disqualifiers);
		const bool bStillInitializing = Disqualifiers.Contains(TSubclassOf<UFGConstructDisqualifier>(UFGCDInitializing::StaticClass()));

		--PollState->AttemptsRemaining;
		if (bStillInitializing && PollState->AttemptsRemaining > 0)
		{
			PollWorld->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
			return;
		}

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

		UE_LOG(LogAIModAI, Display, TEXT("ConstructBeam (deferred, resolved after %d real tick(s)): canConstruct=%s disqualifiers=[%s]"),
			PollState->AttemptsTaken, bCanConstruct ? TEXT("true") : TEXT("false"), *DisqualifierSummary);

		if (!bCanConstruct)
		{
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
			UE_LOG(LogAIModAI, Error, TEXT("ConstructBeam (deferred): lost the build state before constructing - aborting, nothing built"));
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("Lost the build state before constructing")));
			return;
		}

		PollBuildState->InternalConstructHologram(ConstructionID);

		UE_LOG(LogAIModAI, Display, TEXT("ConstructBeam (deferred, resolved after %d real tick(s)): construction attempted via InternalConstructHologram"),
			PollState->AttemptsTaken);

		if (IsValid(PollCharacter))
		{
			PollCharacter->UnequipBuildGun();
		}

		PollState->OnComplete(FAIModOperationResult::Success());
	};

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
}

// See SetBeamLength's doc comment in the header for the real
// AFGBuildableBeam::SetLength() sourcing and the lightweight-instance
// persistence caveat.
FAIModOperationResult UAIModFunctionLibrary::SetBeamLength(UObject* WorldContextObject, const FString& BuildableId, float NewLength)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}

	AFGBuildable* Buildable = nullptr;

	if (IsLightweightBuildableId(BuildableId))
	{
		// Same resolution as DismantleBuildable's lightweight branch -
		// see that function's comment for the full rationale.
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
		return FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No buildable found with id '%s'"), *BuildableId));
	}

	AFGBuildableBeam* Beam = Cast<AFGBuildableBeam>(Buildable);
	if (!Beam)
	{
		return FAIModOperationResult::Failure(TEXT("WRONG_TYPE"), FString::Printf(TEXT("'%s' is a %s, not an AFGBuildableBeam"), *BuildableId, *Buildable->GetClass()->GetName()));
	}

	if (NewLength <= 0.0f || NewLength > Beam->GetMaxLength())
	{
		return FAIModOperationResult::Failure(TEXT("INVALID_LENGTH"),
			FString::Printf(TEXT("newLength %.1f must be > 0 and <= this beam's real maxLength %.1f"), NewLength, Beam->GetMaxLength()));
	}

	const float OldLength = Beam->GetLength();
	Beam->SetLength(NewLength);

	const TSharedRef<FJsonObject> DetailObject = MakeShared<FJsonObject>();
	DetailObject->SetNumberField(TEXT("oldLength"), OldLength);
	DetailObject->SetNumberField(TEXT("newLength"), Beam->GetLength());
	DetailObject->SetNumberField(TEXT("maxLength"), Beam->GetMaxLength());

	UE_LOG(LogAIModAI, Display, TEXT("SetBeamLength: '%s' %.1f -> %.1f (maxLength=%.1f)"), *BuildableId, OldLength, Beam->GetLength(), Beam->GetMaxLength());

	FAIModOperationResult Result = FAIModOperationResult::Success();
	Result.ResultDetailJson = WriteCondensedJson(DetailObject);
	return Result;
}

namespace
{
	FString SchematicTypeToString(ESchematicType Type)
	{
		switch (Type)
		{
		case ESchematicType::EST_Custom: return TEXT("Custom");
		case ESchematicType::EST_Cheat: return TEXT("Cheat");
		case ESchematicType::EST_Tutorial: return TEXT("Tutorial");
		case ESchematicType::EST_Milestone: return TEXT("Milestone");
		case ESchematicType::EST_Alternate: return TEXT("Alternate");
		case ESchematicType::EST_Story: return TEXT("Story");
		case ESchematicType::EST_MAM: return TEXT("MAM");
		case ESchematicType::EST_ResourceSink: return TEXT("ResourceSink");
		case ESchematicType::EST_HardDrive: return TEXT("HardDrive");
		case ESchematicType::EST_Prototype: return TEXT("Prototype");
		case ESchematicType::EST_Customization: return TEXT("Customization");
		default: return TEXT("Unknown");
		}
	}

	FString TechTierStateToString(ETechTierState State)
	{
		switch (State)
		{
		case ETechTierState::ETTS_Locked: return TEXT("Locked");
		case ETechTierState::ETTS_Available: return TEXT("Available");
		case ETechTierState::ETTS_FullyPurchased: return TEXT("FullyPurchased");
		default: return TEXT("Unknown");
		}
	}

	FString SerializeJsonObject(const TSharedRef<FJsonObject>& Object)
	{
		FString OutString;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutString);
		FJsonSerializer::Serialize(Object, Writer);
		return OutString;
	}
}

FString UAIModFunctionLibrary::LogMilestoneProgressAsJson(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	AFGSchematicManager* SchematicManager = World ? AFGSchematicManager::Get(World) : nullptr;
	if (!SchematicManager)
	{
		UE_LOG(LogAIModAI, Warning, TEXT("LogMilestoneProgressAsJson: no valid world context or AFGSchematicManager::Get returned null"));
		return TEXT("{\"protocolVersion\":1,\"highestAvailableTechTier\":0,\"maxAllowedTechTier\":0,\"activeSchematic\":\"\",\"tiers\":[],\"spaceElevators\":[]}");
	}

	const TSubclassOf<UFGSchematic> ActiveSchematic = SchematicManager->GetActiveSchematic();

	// Tiers 0-14 comfortably covers every real game tier - see this
	// function's header doc comment. A tier is only included if it has
	// any real HUB milestone/tutorial schematics.
	TArray<TSharedPtr<FJsonValue>> TiersJsonArray;
	for (int32 Tier = 0; Tier <= 14; ++Tier)
	{
		TArray<TSubclassOf<UFGSchematic>> TierSchematics;
		SchematicManager->GetHubSchematicsForTier(Tier, TierSchematics);
		if (TierSchematics.Num() == 0)
		{
			continue;
		}

		TArray<TSharedPtr<FJsonValue>> SchematicsJsonArray;
		for (const TSubclassOf<UFGSchematic>& SchematicClass : TierSchematics)
		{
			if (!SchematicClass) { continue; }

			const TSharedRef<FJsonObject> SchematicObject = MakeShared<FJsonObject>();
			SchematicObject->SetStringField(TEXT("schematicClass"), SchematicClass->GetPathName());
			SchematicObject->SetStringField(TEXT("displayName"), UFGSchematic::GetSchematicDisplayName(SchematicClass).ToString());
			SchematicObject->SetStringField(TEXT("type"), SchematicTypeToString(UFGSchematic::GetType(SchematicClass)));
			SchematicObject->SetBoolField(TEXT("purchased"), SchematicManager->IsSchematicPurchased(SchematicClass));
			SchematicObject->SetBoolField(TEXT("isActive"), SchematicClass == ActiveSchematic);
			SchematicObject->SetArrayField(TEXT("cost"), ItemAmountsToJsonArray(UFGSchematic::GetCost(SchematicClass)));
			SchematicObject->SetArrayField(TEXT("remainingCost"), ItemAmountsToJsonArray(SchematicManager->GetRemainingCostFor(SchematicClass)));
			SchematicObject->SetArrayField(TEXT("paidOffCost"), ItemAmountsToJsonArray(SchematicManager->GetPaidOffCostFor(SchematicClass)));
			SchematicsJsonArray.Add(MakeShared<FJsonValueObject>(SchematicObject));
		}

		const TSharedRef<FJsonObject> TierObject = MakeShared<FJsonObject>();
		TierObject->SetNumberField(TEXT("tier"), Tier);
		TierObject->SetStringField(TEXT("techTierState"), TechTierStateToString(SchematicManager->GetTechTierState(Tier)));
		TierObject->SetArrayField(TEXT("schematics"), SchematicsJsonArray);
		TiersJsonArray.Add(MakeShared<FJsonValueObject>(TierObject));
	}

	// AFGBuildableSpaceElevator is a normal AFGBuildableFactory - already
	// visible to world.buildables and already belt-connectable via the
	// existing generic world.connectConveyor path (FindFreeFactoryConnection
	// scans any AFGBuildable's UFGFactoryConnectionComponents, no special
	// case needed). Reported here too since its phase-upgrade progress is
	// the direct Space-Elevator analogue of HUB milestone progress above.
	TArray<TSharedPtr<FJsonValue>> SpaceElevatorsJsonArray;
	for (TActorIterator<AFGBuildableSpaceElevator> It(World); It; ++It)
	{
		AFGBuildableSpaceElevator* Elevator = *It;
		if (!IsValid(Elevator)) { continue; }

		TArray<FItemAmount> NextPhaseCost;
		Elevator->GetNextPhaseCost(NextPhaseCost);

		const TSharedRef<FJsonObject> ElevatorObject = MakeShared<FJsonObject>();
		ElevatorObject->SetStringField(TEXT("id"), Elevator->GetPathName());
		ElevatorObject->SetStringField(TEXT("buildableClass"), Elevator->GetClass()->GetPathName());
		ElevatorObject->SetBoolField(TEXT("isFullyUpgraded"), Elevator->IsFullyUpgraded());
		ElevatorObject->SetBoolField(TEXT("isReadyToUpgrade"), Elevator->IsReadyToUpgrade());
		ElevatorObject->SetArrayField(TEXT("nextPhaseCost"), ItemAmountsToJsonArray(NextPhaseCost));
		ElevatorObject->SetArrayField(TEXT("inputInventory"), InventoryToJsonArray(CollectInventoryTelemetry(Elevator->GetInputInventory())));
		SpaceElevatorsJsonArray.Add(MakeShared<FJsonValueObject>(ElevatorObject));
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetNumberField(TEXT("highestAvailableTechTier"), SchematicManager->GetHighestAvailableTechTier());
	RootObject->SetNumberField(TEXT("maxAllowedTechTier"), SchematicManager->GetMaxAllowedTechTier());
	RootObject->SetStringField(TEXT("activeSchematic"), ActiveSchematic ? ActiveSchematic->GetPathName() : FString());
	RootObject->SetArrayField(TEXT("tiers"), TiersJsonArray);
	RootObject->SetArrayField(TEXT("spaceElevators"), SpaceElevatorsJsonArray);

	const FString JsonString = SerializeJsonObject(RootObject);

	UE_LOG(LogAIModAI, Display, TEXT("LogMilestoneProgressAsJson: tiers=%d spaceElevators=%d activeSchematic=%s"),
		TiersJsonArray.Num(), SpaceElevatorsJsonArray.Num(), ActiveSchematic ? *ActiveSchematic->GetName() : TEXT("<none>"));

	return JsonString;
}

FAIModOperationResult UAIModFunctionLibrary::PayOffMilestone(UObject* WorldContextObject, const FString& SchematicClassPath, bool bDryRun)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}

	AFGSchematicManager* SchematicManager = AFGSchematicManager::Get(World);
	if (!SchematicManager)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("AFGSchematicManager::Get returned null"));
	}

	TSubclassOf<UFGSchematic> SchematicClass;
	if (!SchematicClassPath.IsEmpty())
	{
		UClass* ResolvedClass = LoadObject<UClass>(nullptr, *SchematicClassPath);
		if (!ResolvedClass || !ResolvedClass->IsChildOf(UFGSchematic::StaticClass()))
		{
			return FAIModOperationResult::Failure(TEXT("INVALID_SCHEMATIC"),
				FString::Printf(TEXT("'%s' did not resolve to a UFGSchematic subclass"), *SchematicClassPath));
		}
		SchematicClass = ResolvedClass;
	}
	else
	{
		SchematicClass = SchematicManager->GetActiveSchematic();
		if (!SchematicClass)
		{
			return FAIModOperationResult::Failure(TEXT("NO_ACTIVE_SCHEMATIC"),
				TEXT("params.schematicClass was empty and AFGSchematicManager::GetActiveSchematic() is null - set an active schematic in the real HUB widget first, or pass schematicClass explicitly"));
		}
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

	// Deliberately CARRIED inventory only, same scope as SimulatedCraft -
	// NOT the Dimensional Depot (see LogCentralStorageAsJson's doc
	// comment for that established, separate gap). Use
	// WithdrawFromCentralStorage first if the needed items are in the Depot.
	const TArray<FItemAmount> RemainingCost = SchematicManager->GetRemainingCostFor(SchematicClass);

	TArray<FItemAmount> Submission;
	TArray<FItemAmount> Shortfall;
	for (const FItemAmount& Owed : RemainingCost)
	{
		if (!Owed.ItemClass || Owed.Amount <= 0) { continue; }
		const int32 Have = PlayerInventory->GetNumItems(Owed.ItemClass);
		const int32 ToSubmit = FMath::Min(Owed.Amount, Have);
		if (ToSubmit > 0)
		{
			Submission.Add(FItemAmount(Owed.ItemClass, ToSubmit));
		}
		if (ToSubmit < Owed.Amount)
		{
			Shortfall.Add(FItemAmount(Owed.ItemClass, Owed.Amount - ToSubmit));
		}
	}

	auto BuildDetailObject = [&]() -> TSharedRef<FJsonObject>
	{
		const TSharedRef<FJsonObject> DetailObject = MakeShared<FJsonObject>();
		DetailObject->SetStringField(TEXT("schematicClass"), SchematicClass->GetPathName());
		DetailObject->SetBoolField(TEXT("dryRun"), bDryRun);
		DetailObject->SetArrayField(bDryRun ? TEXT("wouldSubmit") : TEXT("submitted"), ItemAmountsToJsonArray(Submission));
		DetailObject->SetArrayField(TEXT("shortfall"), ItemAmountsToJsonArray(Shortfall));
		return DetailObject;
	};

	if (bDryRun)
	{
		UE_LOG(LogAIModAI, Display, TEXT("PayOffMilestone (dry run): schematic=%s wouldSubmit=%d item type(s), shortfall=%d item type(s)"),
			*SchematicClass->GetName(), Submission.Num(), Shortfall.Num());
		FAIModOperationResult Result = FAIModOperationResult::Success();
		Result.ResultDetailJson = SerializeJsonObject(BuildDetailObject());
		return Result;
	}

	if (Submission.Num() == 0)
	{
		FAIModOperationResult Result = FAIModOperationResult::Failure(TEXT("NOTHING_TO_SUBMIT"),
			FString::Printf(TEXT("Player inventory has none of what schematic '%s' still needs (%d item type(s) owed) - never a silent no-op success"),
				*SchematicClass->GetName(), RemainingCost.Num()));
		Result.ResultDetailJson = SerializeJsonObject(BuildDetailObject());
		return Result;
	}

	// Real mutation from here. Verify-then-remove already happened above
	// (Submission only ever contains min(owed, carried) per item) - restore
	// on any rejection below, same discipline as
	// MovePortableMinerToInventory's ARMS-slot restore-on-failure.
	for (const FItemAmount& Item : Submission)
	{
		PlayerInventory->Remove(Item.ItemClass, Item.Amount);
	}

	TArray<FItemAmount> AmountToPay = Submission;
	const bool bPaid = SchematicManager->PayOffOnSchematic(SchematicClass, AmountToPay);

	if (!bPaid)
	{
		for (const FItemAmount& Item : Submission)
		{
			PlayerInventory->AddStack(FInventoryStack(Item.Amount, Item.ItemClass), /*allowPartialAdd=*/true);
		}
		UE_LOG(LogAIModAI, Warning, TEXT("PayOffMilestone: PayOffOnSchematic('%s') returned false - restored %d submitted item type(s) to player inventory"),
			*SchematicClass->GetName(), Submission.Num());
		FAIModOperationResult Result = FAIModOperationResult::Failure(TEXT("PAYOFF_REJECTED"),
			TEXT("AFGSchematicManager::PayOffOnSchematic returned false - items restored to inventory. Real behavior unconfirmed live (first attempt at this RPC); this may mean the schematic isn't accepting payment right now, is already fully paid, or was never eligible."));
		Result.ResultDetailJson = SerializeJsonObject(BuildDetailObject());
		return Result;
	}

	// PayOffOnSchematic takes 'amount' by reference (UPARAM(ref)) - unknown
	// from source whether it mutates it (e.g. to report leftover/excess).
	// Logged for the first live test to actually observe this, not guessed.
	UE_LOG(LogAIModAI, Display, TEXT("PayOffMilestone: schematic=%s submitted=%d item type(s), shortfall=%d item type(s), amountArray after call has %d entries"),
		*SchematicClass->GetName(), Submission.Num(), Shortfall.Num(), AmountToPay.Num());

	const TSharedRef<FJsonObject> DetailObject = BuildDetailObject();
	DetailObject->SetArrayField(TEXT("amountArrayAfterCall"), ItemAmountsToJsonArray(AmountToPay));

	FAIModOperationResult Result = FAIModOperationResult::Success();
	Result.ResultDetailJson = SerializeJsonObject(DetailObject);
	return Result;
}

namespace
{
	FString SchematicStateToString(ESchematicState State)
	{
		switch (State)
		{
		case ESchematicState::ESS_Locked: return TEXT("Locked");
		case ESchematicState::ESS_Purchased: return TEXT("Purchased");
		case ESchematicState::ESS_Available: return TEXT("Available");
		case ESchematicState::ESS_Hidden: return TEXT("Hidden");
		default: return TEXT("Unknown");
		}
	}

	FString ResearchTreeStatusToString(EResearchTreeStatus Status)
	{
		switch (Status)
		{
		case ERTS_Locked: return TEXT("Locked");
		case ERTS_Unlocked: return TEXT("Unlocked");
		case ERTS_StartedResearch: return TEXT("StartedResearch");
		case ERTS_FinishedAllResearch: return TEXT("FinishedAllResearch");
		default: return TEXT("Unknown");
		}
	}

	// AFGResearchManager::mOngoingResearch is a protected (but reflected -
	// real UPROPERTY) TArray<FResearchTime>, with no public getter that
	// returns the full list (GetResearchBeingConducted() only returns a
	// single schematic, insufficient when mCanConductMultipleResearch is
	// true). FResearchTime/FResearchData are fully public struct
	// definitions (FGResearchManager.h) - only the CONTAINER field access is
	// blocked by C++ access rules, not the struct layout itself - so a raw
	// FScriptArrayHelper walk + reinterpret_cast to the known, real struct
	// type is safe here, same category of technique as this file's other
	// FindFProperty-based reads of protected/private UPROPERTYs.
	TArray<FResearchTime> CollectOngoingResearch(AFGResearchManager* Manager)
	{
		TArray<FResearchTime> Result;
		if (!Manager) { return Result; }

		const FArrayProperty* ArrayProp = FindFProperty<FArrayProperty>(Manager->GetClass(), TEXT("mOngoingResearch"));
		if (!ArrayProp) { return Result; }

		const void* ArrayAddr = ArrayProp->ContainerPtrToValuePtr<void>(Manager);
		FScriptArrayHelper ArrayHelper(ArrayProp, ArrayAddr);
		for (int32 Index = 0; Index < ArrayHelper.Num(); ++Index)
		{
			if (const FResearchTime* Entry = reinterpret_cast<const FResearchTime*>(ArrayHelper.GetRawPtr(Index)))
			{
				Result.Add(*Entry);
			}
		}
		return Result;
	}

	// Shared by ClaimMamHardDriveReward/RerollMamHardDrive - both identify
	// their target hard drive by one of its CURRENT reward schematics
	// rather than a numeric id, see ClaimMamHardDriveReward's header doc
	// comment for why a real stable id isn't accessible here.
	UFGHardDrive* FindUnclaimedHardDriveOfferingSchematic(AFGResearchManager* Manager, const TSubclassOf<UFGSchematic>& RewardSchematic)
	{
		if (!Manager || !RewardSchematic) { return nullptr; }

		TArray<UFGHardDrive*> HardDrives;
		Manager->GetUnclaimedHardDrives(HardDrives);
		for (UFGHardDrive* HardDrive : HardDrives)
		{
			if (!IsValid(HardDrive)) { continue; }
			TArray<TSubclassOf<UFGSchematic>> RewardSchematics;
			HardDrive->GetSchematics(RewardSchematics);
			if (RewardSchematics.Contains(RewardSchematic))
			{
				return HardDrive;
			}
		}
		return nullptr;
	}
}

FString UAIModFunctionLibrary::LogMamStatusAsJson(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	AFGResearchManager* ResearchManager = World ? AFGResearchManager::Get(World) : nullptr;
	if (!ResearchManager)
	{
		UE_LOG(LogAIModAI, Warning, TEXT("LogMamStatusAsJson: no valid world context or AFGResearchManager::Get returned null"));
		return TEXT("{\"protocolVersion\":1,\"researchState\":\"NotResearching\",\"canConductMultipleResearch\":false,\"ongoingResearch\":[],\"completedResearch\":[],\"unclaimedHardDrives\":[],\"researchTrees\":[]}");
	}

	TArray<TSharedPtr<FJsonValue>> OngoingJsonArray;
	for (const FResearchTime& Entry : CollectOngoingResearch(ResearchManager))
	{
		const TSubclassOf<UFGSchematic> Schematic = Entry.ResearchData.Schematic;
		if (!Schematic) { continue; }

		const TSharedRef<FJsonObject> EntryObject = MakeShared<FJsonObject>();
		EntryObject->SetStringField(TEXT("schematicClass"), Schematic->GetPathName());
		EntryObject->SetStringField(TEXT("displayName"), UFGSchematic::GetSchematicDisplayName(Schematic).ToString());
		EntryObject->SetStringField(TEXT("type"), SchematicTypeToString(UFGSchematic::GetType(Schematic)));
		EntryObject->SetStringField(TEXT("initiatingResearchTree"), Entry.ResearchData.InitiatingResearchTree ? Entry.ResearchData.InitiatingResearchTree->GetPathName() : FString());
		EntryObject->SetNumberField(TEXT("timeLeftSeconds"), ResearchManager->GetOngoingResearchTimeLeft(Schematic));
		OngoingJsonArray.Add(MakeShared<FJsonValueObject>(EntryObject));
	}

	TArray<TSubclassOf<UFGSchematic>> CompletedSchematics;
	ResearchManager->GetAllCompletedResearch(CompletedSchematics);
	TArray<TSharedPtr<FJsonValue>> CompletedJsonArray;
	for (const TSubclassOf<UFGSchematic>& Schematic : CompletedSchematics)
	{
		if (!Schematic) { continue; }
		const TSubclassOf<UFGResearchTree> InitiatingTree = ResearchManager->GetInitiatingResearchTree(Schematic);

		const TSharedRef<FJsonObject> EntryObject = MakeShared<FJsonObject>();
		EntryObject->SetStringField(TEXT("schematicClass"), Schematic->GetPathName());
		EntryObject->SetStringField(TEXT("displayName"), UFGSchematic::GetSchematicDisplayName(Schematic).ToString());
		EntryObject->SetStringField(TEXT("type"), SchematicTypeToString(UFGSchematic::GetType(Schematic)));
		EntryObject->SetStringField(TEXT("initiatingResearchTree"), InitiatingTree ? InitiatingTree->GetPathName() : FString());
		CompletedJsonArray.Add(MakeShared<FJsonValueObject>(EntryObject));
	}

	TArray<UFGHardDrive*> HardDrives;
	ResearchManager->GetUnclaimedHardDrives(HardDrives);
	TArray<TSharedPtr<FJsonValue>> HardDrivesJsonArray;
	for (UFGHardDrive* HardDrive : HardDrives)
	{
		if (!IsValid(HardDrive)) { continue; }

		TArray<TSubclassOf<UFGSchematic>> RewardSchematics;
		HardDrive->GetSchematics(RewardSchematics);

		TArray<TSharedPtr<FJsonValue>> RewardsJsonArray;
		for (const TSubclassOf<UFGSchematic>& Reward : RewardSchematics)
		{
			if (!Reward) { continue; }
			const TSharedRef<FJsonObject> RewardObject = MakeShared<FJsonObject>();
			RewardObject->SetStringField(TEXT("schematicClass"), Reward->GetPathName());
			RewardObject->SetStringField(TEXT("displayName"), UFGSchematic::GetSchematicDisplayName(Reward).ToString());
			RewardsJsonArray.Add(MakeShared<FJsonValueObject>(RewardObject));
		}

		const TSharedRef<FJsonObject> HardDriveObject = MakeShared<FJsonObject>();
		HardDriveObject->SetArrayField(TEXT("pendingRewards"), RewardsJsonArray);
		HardDriveObject->SetBoolField(TEXT("canReroll"), HardDrive->CanReroll());
		HardDriveObject->SetBoolField(TEXT("hasReroll"), HardDrive->HasReroll());
		HardDrivesJsonArray.Add(MakeShared<FJsonValueObject>(HardDriveObject));
	}

	TArray<TSubclassOf<UFGResearchTree>> AllTrees;
	ResearchManager->GetAllResearchTrees(AllTrees);
	TArray<TSharedPtr<FJsonValue>> TreesJsonArray;
	for (const TSubclassOf<UFGResearchTree>& TreeClass : AllTrees)
	{
		if (!TreeClass) { continue; }
		const EResearchTreeStatus TreeStatus = UFGResearchTree::GetResearchTreeStatus(TreeClass, WorldContextObject);

		const TSharedRef<FJsonObject> TreeObject = MakeShared<FJsonObject>();
		TreeObject->SetStringField(TEXT("researchTreeClass"), TreeClass->GetPathName());
		TreeObject->SetStringField(TEXT("displayName"), UFGResearchTree::GetDisplayName(TreeClass).ToString());
		TreeObject->SetStringField(TEXT("status"), ResearchTreeStatusToString(TreeStatus));

		// A fully locked tree isn't visible to the real player either - its
		// nodes would just be noise (and every node's schematic state would
		// misleadingly read "Locked" for a reason unrelated to the node
		// itself).
		TArray<TSharedPtr<FJsonValue>> NodesJsonArray;
		if (TreeStatus != ERTS_Locked)
		{
			for (UFGResearchTreeNode* Node : UFGResearchTree::GetNodes(TreeClass))
			{
				if (!IsValid(Node)) { continue; }
				const TSubclassOf<UFGSchematic> NodeSchematic = Node->GetNodeSchematic();
				if (!NodeSchematic) { continue; }

				const TSharedRef<FJsonObject> NodeObject = MakeShared<FJsonObject>();
				NodeObject->SetStringField(TEXT("schematicClass"), NodeSchematic->GetPathName());
				NodeObject->SetStringField(TEXT("displayName"), UFGSchematic::GetSchematicDisplayName(NodeSchematic).ToString());
				NodeObject->SetStringField(TEXT("type"), SchematicTypeToString(UFGSchematic::GetType(NodeSchematic)));
				NodeObject->SetStringField(TEXT("schematicState"), SchematicStateToString(UFGSchematic::GetSchematicState(NodeSchematic, WorldContextObject)));
				NodeObject->SetArrayField(TEXT("cost"), ItemAmountsToJsonArray(UFGSchematic::GetCost(NodeSchematic)));
				NodesJsonArray.Add(MakeShared<FJsonValueObject>(NodeObject));
			}
		}
		TreeObject->SetArrayField(TEXT("nodes"), NodesJsonArray);
		TreesJsonArray.Add(MakeShared<FJsonValueObject>(TreeObject));
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetStringField(TEXT("researchState"), ResearchManager->GetCurrentResearchState() == EResearchState::ERS_Researching ? TEXT("Researching") : TEXT("NotResearching"));
	RootObject->SetBoolField(TEXT("canConductMultipleResearch"), ResearchManager->CanConductMultipleResearch());
	RootObject->SetArrayField(TEXT("ongoingResearch"), OngoingJsonArray);
	RootObject->SetArrayField(TEXT("completedResearch"), CompletedJsonArray);
	RootObject->SetArrayField(TEXT("unclaimedHardDrives"), HardDrivesJsonArray);
	RootObject->SetArrayField(TEXT("researchTrees"), TreesJsonArray);

	const FString JsonString = SerializeJsonObject(RootObject);

	UE_LOG(LogAIModAI, Display, TEXT("LogMamStatusAsJson: ongoing=%d completed=%d unclaimedHardDrives=%d trees=%d"),
		OngoingJsonArray.Num(), CompletedJsonArray.Num(), HardDrivesJsonArray.Num(), TreesJsonArray.Num());

	return JsonString;
}

FAIModOperationResult UAIModFunctionLibrary::StartMamResearch(UObject* WorldContextObject, const FString& SchematicClassPath, const FString& ResearchTreeClassPath, bool bDryRun)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}

	AFGResearchManager* ResearchManager = AFGResearchManager::Get(World);
	if (!ResearchManager)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("AFGResearchManager::Get returned null"));
	}

	if (SchematicClassPath.IsEmpty())
	{
		return FAIModOperationResult::Failure(TEXT("INVALID_REQUEST"), TEXT("schematicClass must be a non-empty string"));
	}
	UClass* ResolvedSchematicClass = LoadObject<UClass>(nullptr, *SchematicClassPath);
	if (!ResolvedSchematicClass || !ResolvedSchematicClass->IsChildOf(UFGSchematic::StaticClass()))
	{
		return FAIModOperationResult::Failure(TEXT("INVALID_SCHEMATIC"),
			FString::Printf(TEXT("'%s' did not resolve to a UFGSchematic subclass"), *SchematicClassPath));
	}
	const TSubclassOf<UFGSchematic> SchematicClass = ResolvedSchematicClass;

	if (ResearchTreeClassPath.IsEmpty())
	{
		return FAIModOperationResult::Failure(TEXT("INVALID_REQUEST"),
			TEXT("researchTreeClass must be a non-empty string - InitiateResearch requires the initiating tree"));
	}
	UClass* ResolvedTreeClass = LoadObject<UClass>(nullptr, *ResearchTreeClassPath);
	if (!ResolvedTreeClass || !ResolvedTreeClass->IsChildOf(UFGResearchTree::StaticClass()))
	{
		return FAIModOperationResult::Failure(TEXT("INVALID_RESEARCH_TREE"),
			FString::Printf(TEXT("'%s' did not resolve to a UFGResearchTree subclass"), *ResearchTreeClassPath));
	}
	const TSubclassOf<UFGResearchTree> ResearchTreeClass = ResolvedTreeClass;

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

	AFGPlayerController* Controller = Cast<AFGPlayerController>(UGameplayStatics::GetPlayerController(World, 0));
	if (!Controller)
	{
		return FAIModOperationResult::Failure(TEXT("NO_PLAYER"), TEXT("No local AFGPlayerController (player index 0)"));
	}

	const bool bCanInitiate = ResearchManager->CanResearchBeInitiated(SchematicClass);
	const bool bCanAfford = ResearchManager->CanAffordResearch(PlayerInventory, SchematicClass);
	const TArray<FItemAmount> Cost = UFGSchematic::GetCost(SchematicClass);

	const TSharedRef<FJsonObject> DetailObject = MakeShared<FJsonObject>();
	DetailObject->SetStringField(TEXT("schematicClass"), SchematicClass->GetPathName());
	DetailObject->SetStringField(TEXT("researchTreeClass"), ResearchTreeClass->GetPathName());
	DetailObject->SetBoolField(TEXT("dryRun"), bDryRun);
	DetailObject->SetBoolField(TEXT("canResearchBeInitiated"), bCanInitiate);
	DetailObject->SetBoolField(TEXT("canAfford"), bCanAfford);
	DetailObject->SetArrayField(TEXT("cost"), ItemAmountsToJsonArray(Cost));

	if (!bCanInitiate)
	{
		FAIModOperationResult Result = FAIModOperationResult::Failure(TEXT("CANNOT_RESEARCH"),
			TEXT("AFGResearchManager::CanResearchBeInitiated returned false - already researching/researched, tree not unlocked, or dependencies not met"));
		Result.ResultDetailJson = SerializeJsonObject(DetailObject);
		return Result;
	}
	if (!bCanAfford)
	{
		FAIModOperationResult Result = FAIModOperationResult::Failure(TEXT("INSUFFICIENT_INGREDIENTS"),
			TEXT("AFGResearchManager::CanAffordResearch returned false - carried inventory does not cover the full cost, see result.detail.cost"));
		Result.ResultDetailJson = SerializeJsonObject(DetailObject);
		return Result;
	}

	if (bDryRun)
	{
		FAIModOperationResult Result = FAIModOperationResult::Success();
		Result.ResultDetailJson = SerializeJsonObject(DetailObject);
		return Result;
	}

	// Atomic pay-and-start, per this function's header doc comment - no
	// partial-submission step exists to expose separately.
	ResearchManager->InitiateResearch(Controller, SchematicClass, ResearchTreeClass);

	if (!ResearchManager->IsResearchBeingConducted(SchematicClass))
	{
		FAIModOperationResult Result = FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"),
			TEXT("InitiateResearch was called but IsResearchBeingConducted still returns false afterward"));
		Result.ResultDetailJson = SerializeJsonObject(DetailObject);
		return Result;
	}

	UE_LOG(LogAIModAI, Display, TEXT("StartMamResearch: schematic=%s tree=%s - research started"),
		*SchematicClass->GetName(), *ResearchTreeClass->GetName());

	FAIModOperationResult Result = FAIModOperationResult::Success();
	Result.ResultDetailJson = SerializeJsonObject(DetailObject);
	return Result;
}

FAIModOperationResult UAIModFunctionLibrary::ClaimMamResearch(UObject* WorldContextObject, const FString& SchematicClassPath)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}

	AFGResearchManager* ResearchManager = AFGResearchManager::Get(World);
	if (!ResearchManager)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("AFGResearchManager::Get returned null"));
	}

	if (SchematicClassPath.IsEmpty())
	{
		return FAIModOperationResult::Failure(TEXT("INVALID_REQUEST"), TEXT("schematicClass must be a non-empty string"));
	}
	UClass* ResolvedClass = LoadObject<UClass>(nullptr, *SchematicClassPath);
	if (!ResolvedClass || !ResolvedClass->IsChildOf(UFGSchematic::StaticClass()))
	{
		return FAIModOperationResult::Failure(TEXT("INVALID_SCHEMATIC"),
			FString::Printf(TEXT("'%s' did not resolve to a UFGSchematic subclass"), *SchematicClassPath));
	}
	const TSubclassOf<UFGSchematic> SchematicClass = ResolvedClass;

	if (!ResearchManager->IsResearchComplete(SchematicClass))
	{
		return FAIModOperationResult::Failure(TEXT("NOT_COMPLETE"),
			FString::Printf(TEXT("'%s' is not a completed, unclaimed research (still ongoing, not yet started, or already claimed)"), *SchematicClassPath));
	}

	AFGPlayerController* Controller = Cast<AFGPlayerController>(UGameplayStatics::GetPlayerController(World, 0));
	if (!Controller)
	{
		return FAIModOperationResult::Failure(TEXT("NO_PLAYER"), TEXT("No local AFGPlayerController (player index 0)"));
	}

	ResearchManager->ClaimResearchResults(Controller, SchematicClass);

	if (ResearchManager->IsResearchComplete(SchematicClass))
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"),
			TEXT("ClaimResearchResults was called but IsResearchComplete still returns true afterward"));
	}

	UE_LOG(LogAIModAI, Display, TEXT("ClaimMamResearch: schematic=%s claimed"), *SchematicClass->GetName());

	return FAIModOperationResult::Success();
}

FAIModOperationResult UAIModFunctionLibrary::ClaimMamHardDriveReward(UObject* WorldContextObject, const FString& RewardSchematicClassPath)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}

	AFGResearchManager* ResearchManager = AFGResearchManager::Get(World);
	if (!ResearchManager)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("AFGResearchManager::Get returned null"));
	}

	if (RewardSchematicClassPath.IsEmpty())
	{
		return FAIModOperationResult::Failure(TEXT("INVALID_REQUEST"), TEXT("schematicClass must be a non-empty string"));
	}
	UClass* ResolvedClass = LoadObject<UClass>(nullptr, *RewardSchematicClassPath);
	if (!ResolvedClass || !ResolvedClass->IsChildOf(UFGSchematic::StaticClass()))
	{
		return FAIModOperationResult::Failure(TEXT("INVALID_SCHEMATIC"),
			FString::Printf(TEXT("'%s' did not resolve to a UFGSchematic subclass"), *RewardSchematicClassPath));
	}
	const TSubclassOf<UFGSchematic> RewardSchematic = ResolvedClass;

	UFGHardDrive* TargetHardDrive = FindUnclaimedHardDriveOfferingSchematic(ResearchManager, RewardSchematic);
	if (!TargetHardDrive)
	{
		return FAIModOperationResult::Failure(TEXT("REWARD_NOT_FOUND"),
			FString::Printf(TEXT("No unclaimed hard drive currently offers '%s' as a reward choice - re-query world.mamStatus"), *RewardSchematicClassPath));
	}

	AFGPlayerController* Controller = Cast<AFGPlayerController>(UGameplayStatics::GetPlayerController(World, 0));
	if (!Controller)
	{
		return FAIModOperationResult::Failure(TEXT("NO_PLAYER"), TEXT("No local AFGPlayerController (player index 0)"));
	}

	TargetHardDrive->ClaimSchematic(Controller, RewardSchematic);

	UE_LOG(LogAIModAI, Display, TEXT("ClaimMamHardDriveReward: claimed %s"), *RewardSchematic->GetName());

	// TargetHardDrive's own wrapper object may now be stale/claimed - don't
	// probe it further, re-query world.mamStatus for authoritative post-
	// claim state (same "don't trust a mutated-away handle" posture as the
	// rest of this project's write operations).
	return FAIModOperationResult::Success();
}

FAIModOperationResult UAIModFunctionLibrary::RerollMamHardDrive(UObject* WorldContextObject, const FString& AnyCurrentRewardSchematicClassPath)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}

	AFGResearchManager* ResearchManager = AFGResearchManager::Get(World);
	if (!ResearchManager)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("AFGResearchManager::Get returned null"));
	}

	if (AnyCurrentRewardSchematicClassPath.IsEmpty())
	{
		return FAIModOperationResult::Failure(TEXT("INVALID_REQUEST"), TEXT("schematicClass must be a non-empty string"));
	}
	UClass* ResolvedClass = LoadObject<UClass>(nullptr, *AnyCurrentRewardSchematicClassPath);
	if (!ResolvedClass || !ResolvedClass->IsChildOf(UFGSchematic::StaticClass()))
	{
		return FAIModOperationResult::Failure(TEXT("INVALID_SCHEMATIC"),
			FString::Printf(TEXT("'%s' did not resolve to a UFGSchematic subclass"), *AnyCurrentRewardSchematicClassPath));
	}
	const TSubclassOf<UFGSchematic> RewardSchematic = ResolvedClass;

	UFGHardDrive* TargetHardDrive = FindUnclaimedHardDriveOfferingSchematic(ResearchManager, RewardSchematic);
	if (!TargetHardDrive)
	{
		return FAIModOperationResult::Failure(TEXT("REWARD_NOT_FOUND"),
			FString::Printf(TEXT("No unclaimed hard drive currently offers '%s' - re-query world.mamStatus"), *AnyCurrentRewardSchematicClassPath));
	}

	if (!TargetHardDrive->CanReroll())
	{
		return FAIModOperationResult::Failure(TEXT("CANNOT_REROLL"),
			TargetHardDrive->HasReroll()
				? TEXT("UFGHardDrive::CanReroll() is false: no alternate recipes are currently available to reroll into")
				: TEXT("UFGHardDrive::CanReroll() is false: no rerolls left for this hard drive"));
	}

	AFGPlayerController* Controller = Cast<AFGPlayerController>(UGameplayStatics::GetPlayerController(World, 0));
	if (!Controller)
	{
		return FAIModOperationResult::Failure(TEXT("NO_PLAYER"), TEXT("No local AFGPlayerController (player index 0)"));
	}

	TargetHardDrive->Reroll(Controller);

	UE_LOG(LogAIModAI, Display, TEXT("RerollMamHardDrive: rerolled a hard drive that was offering %s - re-query world.mamStatus for new choices"),
		*RewardSchematic->GetName());

	return FAIModOperationResult::Success();
}

namespace
{
	FString TrainStatusToString(ETrainStatus Status)
	{
		switch (Status)
		{
		case ETrainStatus::TS_Parked: return TEXT("Parked");
		case ETrainStatus::TS_ManualDriving: return TEXT("ManualDriving");
		case ETrainStatus::TS_SelfDriving: return TEXT("SelfDriving");
		case ETrainStatus::TS_Derailed: return TEXT("Derailed");
		default: return TEXT("Unknown");
		}
	}

	FString SelfDrivingErrorToString(ESelfDrivingLocomotiveError Error)
	{
		switch (Error)
		{
		case ESelfDrivingLocomotiveError::SDLE_NoError: return TEXT("NoError");
		case ESelfDrivingLocomotiveError::SDLE_NoPower: return TEXT("NoPower");
		case ESelfDrivingLocomotiveError::SDLE_NoTimeTable: return TEXT("NoTimeTable");
		case ESelfDrivingLocomotiveError::SDLE_InvalidNextStop: return TEXT("InvalidNextStop");
		case ESelfDrivingLocomotiveError::SDLE_InvalidLocomotivePlacement: return TEXT("InvalidLocomotivePlacement");
		case ESelfDrivingLocomotiveError::SDLE_NoPath: return TEXT("NoPath");
		case ESelfDrivingLocomotiveError::SDLE_StationUnreachable: return TEXT("StationUnreachable");
		case ESelfDrivingLocomotiveError::SDLE_StationUnreachableWithSignals: return TEXT("StationUnreachableWithSignals");
		case ESelfDrivingLocomotiveError::SDLE_LongWaitAtSignal: return TEXT("LongWaitAtSignal");
		default: return TEXT("Unknown");
		}
	}

	FString TrainDockingStateToString(ETrainDockingState State)
	{
		switch (State)
		{
		case ETrainDockingState::TDS_None: return TEXT("None");
		case ETrainDockingState::TDS_ReadyToDock: return TEXT("ReadyToDock");
		case ETrainDockingState::TDS_Docked: return TEXT("Docked");
		default: return TEXT("Unknown");
		}
	}

	FString DroneStatusToString(EDroneStatus Status)
	{
		switch (Status)
		{
		case EDroneStatus::EDS_NO_DRONE: return TEXT("NoDrone");
		case EDroneStatus::EDS_DOCKED: return TEXT("Docked");
		case EDroneStatus::EDS_LOADING: return TEXT("Loading");
		case EDroneStatus::EDS_TAKEOFF: return TEXT("Takeoff");
		case EDroneStatus::EDS_EN_ROUTE: return TEXT("EnRoute");
		case EDroneStatus::EDS_DOCKING: return TEXT("Docking");
		case EDroneStatus::EDS_UNLOADING: return TEXT("Unloading");
		case EDroneStatus::EDS_NOT_ENOUGH_FUEL: return TEXT("NotEnoughFuel");
		case EDroneStatus::EDS_CANNOT_UNLOAD: return TEXT("CannotUnload");
		default: return TEXT("Unknown");
		}
	}
}

FString UAIModFunctionLibrary::LogTrainStationsAsJson(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	AFGRailroadSubsystem* RailroadSubsystem = World ? AFGRailroadSubsystem::Get(World) : nullptr;
	if (!RailroadSubsystem)
	{
		UE_LOG(LogAIModAI, Warning, TEXT("LogTrainStationsAsJson: no valid world context or AFGRailroadSubsystem::Get returned null"));
		return TEXT("{\"protocolVersion\":1,\"stations\":[]}");
	}

	TArray<AFGTrainStationIdentifier*> Identifiers;
	RailroadSubsystem->GetAllTrainStations(Identifiers);

	TArray<TSharedPtr<FJsonValue>> StationsJsonArray;
	for (AFGTrainStationIdentifier* Identifier : Identifiers)
	{
		if (!IsValid(Identifier)) { continue; }
		AFGBuildableRailroadStation* Station = Identifier->GetStation();
		if (!IsValid(Station)) { continue; }

		const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("id"), Station->GetPathName());
		Entry->SetStringField(TEXT("name"), Identifier->GetStationName().ToString());
		Entry->SetNumberField(TEXT("trackGraphId"), Identifier->GetTrackGraphID());
		Entry->SetStringField(TEXT("buildableClass"), Station->GetClass()->GetPathName());
		StationsJsonArray.Add(MakeShared<FJsonValueObject>(Entry));
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetArrayField(TEXT("stations"), StationsJsonArray);

	const FString JsonString = SerializeJsonObject(RootObject);
	UE_LOG(LogAIModAI, Display, TEXT("LogTrainStationsAsJson: stations=%d"), StationsJsonArray.Num());
	return JsonString;
}

FString UAIModFunctionLibrary::LogTrainsAsJson(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	AFGRailroadSubsystem* RailroadSubsystem = World ? AFGRailroadSubsystem::Get(World) : nullptr;
	if (!RailroadSubsystem)
	{
		UE_LOG(LogAIModAI, Warning, TEXT("LogTrainsAsJson: no valid world context or AFGRailroadSubsystem::Get returned null"));
		return TEXT("{\"protocolVersion\":1,\"trains\":[]}");
	}

	TArray<AFGTrain*> Trains;
	RailroadSubsystem->GetAllTrains(Trains);

	TArray<TSharedPtr<FJsonValue>> TrainsJsonArray;
	for (AFGTrain* Train : Trains)
	{
		if (!IsValid(Train)) { continue; }

		TArray<TSharedPtr<FJsonValue>> StopsJsonArray;
		if (Train->HasTimeTable())
		{
			AFGRailroadTimeTable* TimeTable = Train->GetTimeTable();
			TArray<FTimeTableStop> Stops;
			if (TimeTable) { TimeTable->GetStops(Stops); }

			for (const FTimeTableStop& Stop : Stops)
			{
				const TSharedRef<FJsonObject> StopObject = MakeShared<FJsonObject>();
				const bool bHasStation = IsValid(Stop.Station);
				AFGBuildableRailroadStation* StopStation = bHasStation ? Stop.Station->GetStation() : nullptr;
				StopObject->SetStringField(TEXT("stationId"), IsValid(StopStation) ? StopStation->GetPathName() : FString());
				StopObject->SetStringField(TEXT("stationName"), bHasStation ? Stop.Station->GetStationName().ToString() : FString());
				StopObject->SetStringField(TEXT("dockingDefinition"),
					Stop.DockingRuleSet.DockingDefinition == ETrainDockingDefinition::TDD_FullyLoadUnload ? TEXT("FullyLoadUnload") : TEXT("LoadUnloadOnce"));
				StopObject->SetNumberField(TEXT("dockForDuration"), Stop.DockingRuleSet.DockForDuration);
				StopObject->SetBoolField(TEXT("isDurationAndRule"), Stop.DockingRuleSet.IsDurationAndRule);
				StopObject->SetBoolField(TEXT("ignoreFullLoadUnloadIfTransferBlockedByFilters"), Stop.DockingRuleSet.IgnoreFullLoadUnloadIfTransferBlockedByFilters);

				TArray<TSharedPtr<FJsonValue>> LoadFilterJsonArray;
				for (const TSubclassOf<UFGItemDescriptor>& ItemClass : Stop.DockingRuleSet.LoadFilterDescriptors)
				{
					if (ItemClass) { LoadFilterJsonArray.Add(MakeShared<FJsonValueString>(ItemClass->GetPathName())); }
				}
				StopObject->SetArrayField(TEXT("loadFilter"), LoadFilterJsonArray);

				TArray<TSharedPtr<FJsonValue>> UnloadFilterJsonArray;
				for (const TSubclassOf<UFGItemDescriptor>& ItemClass : Stop.DockingRuleSet.UnloadFilterDescriptors)
				{
					if (ItemClass) { UnloadFilterJsonArray.Add(MakeShared<FJsonValueString>(ItemClass->GetPathName())); }
				}
				StopObject->SetArrayField(TEXT("unloadFilter"), UnloadFilterJsonArray);

				StopsJsonArray.Add(MakeShared<FJsonValueObject>(StopObject));
			}
		}

		const TSharedRef<FJsonObject> TrainObject = MakeShared<FJsonObject>();
		TrainObject->SetStringField(TEXT("id"), Train->GetPathName());
		TrainObject->SetStringField(TEXT("name"), Train->GetTrainName().ToString());
		TrainObject->SetStringField(TEXT("status"), TrainStatusToString(Train->GetTrainStatus()));
		TrainObject->SetBoolField(TEXT("selfDrivingEnabled"), Train->IsSelfDrivingEnabled());
		TrainObject->SetStringField(TEXT("selfDrivingError"), SelfDrivingErrorToString(Train->GetSelfDrivingError()));
		TrainObject->SetStringField(TEXT("dockingState"), TrainDockingStateToString(Train->GetDockingState()));
		TrainObject->SetBoolField(TEXT("hasTimeTable"), Train->HasTimeTable());
		TrainObject->SetArrayField(TEXT("timetable"), StopsJsonArray);
		TrainsJsonArray.Add(MakeShared<FJsonValueObject>(TrainObject));
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetArrayField(TEXT("trains"), TrainsJsonArray);

	const FString JsonString = SerializeJsonObject(RootObject);
	UE_LOG(LogAIModAI, Display, TEXT("LogTrainsAsJson: trains=%d"), TrainsJsonArray.Num());
	return JsonString;
}

FAIModOperationResult UAIModFunctionLibrary::SetTrainTimetable(UObject* WorldContextObject, const FString& TrainId, const FString& StopsJson)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}

	if (TrainId.IsEmpty())
	{
		return FAIModOperationResult::Failure(TEXT("INVALID_REQUEST"), TEXT("trainId must be a non-empty string"));
	}

	AFGTrain* TargetTrain = nullptr;
	for (TActorIterator<AFGTrain> It(World); It; ++It)
	{
		if (IsValid(*It) && It->GetPathName() == TrainId)
		{
			TargetTrain = *It;
			break;
		}
	}
	if (!TargetTrain)
	{
		return FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No train found with id '%s'"), *TrainId));
	}

	TArray<TSharedPtr<FJsonValue>> StopsArray;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(StopsJson);
	if (!FJsonSerializer::Deserialize(Reader, StopsArray) || StopsArray.Num() == 0)
	{
		return FAIModOperationResult::Failure(TEXT("INVALID_REQUEST"), TEXT("stops must be a non-empty JSON array"));
	}

	TArray<FTimeTableStop> NewStops;
	for (const TSharedPtr<FJsonValue>& StopValue : StopsArray)
	{
		const TSharedPtr<FJsonObject> StopObject = StopValue.IsValid() ? StopValue->AsObject() : nullptr;
		if (!StopObject.IsValid())
		{
			return FAIModOperationResult::Failure(TEXT("INVALID_REQUEST"), TEXT("Each stop must be a JSON object"));
		}

		FString StationBuildableId;
		if (!StopObject->TryGetStringField(TEXT("stationBuildableId"), StationBuildableId) || StationBuildableId.IsEmpty())
		{
			return FAIModOperationResult::Failure(TEXT("INVALID_REQUEST"), TEXT("Each stop requires a non-empty stationBuildableId"));
		}

		AFGBuildableRailroadStation* Station = Cast<AFGBuildableRailroadStation>(FindBuildableById(World, StationBuildableId));
		if (!Station)
		{
			return FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"),
				FString::Printf(TEXT("'%s' is not a real, currently-existing AFGBuildableRailroadStation"), *StationBuildableId));
		}
		AFGTrainStationIdentifier* Identifier = Station->GetStationIdentifier();
		if (!Identifier)
		{
			return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"),
				FString::Printf(TEXT("'%s' has no AFGTrainStationIdentifier yet"), *StationBuildableId));
		}

		FTimeTableStop Stop;
		Stop.Station = Identifier;

		FString DockingDefinitionString;
		StopObject->TryGetStringField(TEXT("dockingDefinition"), DockingDefinitionString);
		Stop.DockingRuleSet.DockingDefinition = (DockingDefinitionString == TEXT("FullyLoadUnload"))
			? ETrainDockingDefinition::TDD_FullyLoadUnload
			: ETrainDockingDefinition::TDD_LoadUnloadOnce;

		double DockForDuration = 15.0;
		StopObject->TryGetNumberField(TEXT("dockForDuration"), DockForDuration);
		Stop.DockingRuleSet.DockForDuration = static_cast<float>(DockForDuration);

		bool bIsDurationAndRule = false;
		StopObject->TryGetBoolField(TEXT("isDurationAndRule"), bIsDurationAndRule);
		Stop.DockingRuleSet.IsDurationAndRule = bIsDurationAndRule;

		bool bIgnoreFilters = false;
		StopObject->TryGetBoolField(TEXT("ignoreFullLoadUnloadIfTransferBlockedByFilters"), bIgnoreFilters);
		Stop.DockingRuleSet.IgnoreFullLoadUnloadIfTransferBlockedByFilters = bIgnoreFilters;

		const TArray<TSharedPtr<FJsonValue>>* LoadFilterArray = nullptr;
		if (StopObject->TryGetArrayField(TEXT("loadFilter"), LoadFilterArray) && LoadFilterArray)
		{
			for (const TSharedPtr<FJsonValue>& ItemValue : *LoadFilterArray)
			{
				FString ItemClassPath;
				if (ItemValue.IsValid() && ItemValue->TryGetString(ItemClassPath) && !ItemClassPath.IsEmpty())
				{
					UClass* ResolvedItemClass = LoadObject<UClass>(nullptr, *ItemClassPath);
					if (ResolvedItemClass && ResolvedItemClass->IsChildOf(UFGItemDescriptor::StaticClass()))
					{
						Stop.DockingRuleSet.LoadFilterDescriptors.Add(ResolvedItemClass);
					}
				}
			}
		}

		const TArray<TSharedPtr<FJsonValue>>* UnloadFilterArray = nullptr;
		if (StopObject->TryGetArrayField(TEXT("unloadFilter"), UnloadFilterArray) && UnloadFilterArray)
		{
			for (const TSharedPtr<FJsonValue>& ItemValue : *UnloadFilterArray)
			{
				FString ItemClassPath;
				if (ItemValue.IsValid() && ItemValue->TryGetString(ItemClassPath) && !ItemClassPath.IsEmpty())
				{
					UClass* ResolvedItemClass = LoadObject<UClass>(nullptr, *ItemClassPath);
					if (ResolvedItemClass && ResolvedItemClass->IsChildOf(UFGItemDescriptor::StaticClass()))
					{
						Stop.DockingRuleSet.UnloadFilterDescriptors.Add(ResolvedItemClass);
					}
				}
			}
		}

		NewStops.Add(Stop);
	}

	AFGRailroadTimeTable* TimeTable = TargetTrain->HasTimeTable() ? TargetTrain->GetTimeTable() : TargetTrain->NewTimeTable();
	if (!TimeTable)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("Failed to get or create a time table for this train"));
	}

	TimeTable->SetStops(NewStops);

	if (TimeTable->GetNumStops() != NewStops.Num())
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"),
			FString::Printf(TEXT("SetStops was called with %d stops but GetNumStops() now reports %d"), NewStops.Num(), TimeTable->GetNumStops()));
	}

	UE_LOG(LogAIModAI, Display, TEXT("SetTrainTimetable: train=%s stops=%d"), *TargetTrain->GetTrainName().ToString(), NewStops.Num());

	return FAIModOperationResult::Success();
}

FAIModOperationResult UAIModFunctionLibrary::SetTrainSelfDriving(UObject* WorldContextObject, const FString& TrainId, bool bEnabled)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}

	if (TrainId.IsEmpty())
	{
		return FAIModOperationResult::Failure(TEXT("INVALID_REQUEST"), TEXT("trainId must be a non-empty string"));
	}

	AFGTrain* TargetTrain = nullptr;
	for (TActorIterator<AFGTrain> It(World); It; ++It)
	{
		if (IsValid(*It) && It->GetPathName() == TrainId)
		{
			TargetTrain = *It;
			break;
		}
	}
	if (!TargetTrain)
	{
		return FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No train found with id '%s'"), *TrainId));
	}

	TargetTrain->SetSelfDrivingEnabled(bEnabled);

	if (TargetTrain->IsSelfDrivingEnabled() != bEnabled)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"),
			FString::Printf(TEXT("SetSelfDrivingEnabled(%s) was called but IsSelfDrivingEnabled() still reports %s"),
				bEnabled ? TEXT("true") : TEXT("false"), TargetTrain->IsSelfDrivingEnabled() ? TEXT("true") : TEXT("false")));
	}

	const TSharedRef<FJsonObject> DetailObject = MakeShared<FJsonObject>();
	DetailObject->SetBoolField(TEXT("selfDrivingEnabled"), TargetTrain->IsSelfDrivingEnabled());
	DetailObject->SetStringField(TEXT("selfDrivingError"), SelfDrivingErrorToString(TargetTrain->GetSelfDrivingError()));

	UE_LOG(LogAIModAI, Display, TEXT("SetTrainSelfDriving: train=%s enabled=%s error=%s"),
		*TargetTrain->GetTrainName().ToString(), bEnabled ? TEXT("true") : TEXT("false"), *SelfDrivingErrorToString(TargetTrain->GetSelfDrivingError()));

	FAIModOperationResult Result = FAIModOperationResult::Success();
	Result.ResultDetailJson = SerializeJsonObject(DetailObject);
	return Result;
}

FString UAIModFunctionLibrary::LogDroneStationsAsJson(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	AFGDroneSubsystem* DroneSubsystem = World ? AFGDroneSubsystem::Get(World) : nullptr;
	if (!DroneSubsystem)
	{
		UE_LOG(LogAIModAI, Warning, TEXT("LogDroneStationsAsJson: no valid world context or AFGDroneSubsystem::Get returned null"));
		return TEXT("{\"protocolVersion\":1,\"droneStations\":[]}");
	}

	TArray<TSharedPtr<FJsonValue>> StationsJsonArray;
	for (AFGDroneStationInfo* Info : DroneSubsystem->GetAllStations())
	{
		if (!IsValid(Info)) { continue; }
		AFGBuildableDroneStation* Station = Info->GetStation();
		if (!IsValid(Station)) { continue; }

		AFGDroneStationInfo* PairedInfo = Info->GetPairedStation();
		AFGBuildableDroneStation* PairedStation = (PairedInfo && IsValid(PairedInfo)) ? PairedInfo->GetStation() : nullptr;

		TArray<TSharedPtr<FJsonValue>> AllowedFuelJsonArray;
		for (const FFGDroneFuelType& FuelType : Info->GetDroneFuelTypes())
		{
			if (FuelType.Item) { AllowedFuelJsonArray.Add(MakeShared<FJsonValueString>(FuelType.Item->GetPathName())); }
		}

		const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("id"), Station->GetPathName());
		Entry->SetStringField(TEXT("pairedStationId"), (PairedStation && IsValid(PairedStation)) ? PairedStation->GetPathName() : FString());
		Entry->SetStringField(TEXT("droneStatus"), DroneStatusToString(Info->GetDroneStatus()));
		Entry->SetStringField(TEXT("activeFuelType"), Info->GetDroneActiveFuelType() ? Info->GetDroneActiveFuelType()->GetPathName() : FString());
		Entry->SetArrayField(TEXT("allowedFuelTypes"), AllowedFuelJsonArray);
		Entry->SetNumberField(TEXT("latestRoundTripTimeSeconds"), Info->GetLatestRoundTripTime());
		Entry->SetNumberField(TEXT("averageIncomingItemRate"), Info->GetAverageIncomingItemRate());
		Entry->SetNumberField(TEXT("averageOutgoingItemRate"), Info->GetAverageOutgoingItemRate());
		Entry->SetArrayField(TEXT("inputInventory"), InventoryToJsonArray(CollectInventoryTelemetry(Station->GetInputInventory())));
		Entry->SetArrayField(TEXT("outputInventory"), InventoryToJsonArray(CollectInventoryTelemetry(Station->GetOutputInventory())));
		Entry->SetArrayField(TEXT("fuelInventory"), InventoryToJsonArray(CollectInventoryTelemetry(Station->GetFuelInventory())));
		StationsJsonArray.Add(MakeShared<FJsonValueObject>(Entry));
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetArrayField(TEXT("droneStations"), StationsJsonArray);

	const FString JsonString = SerializeJsonObject(RootObject);
	UE_LOG(LogAIModAI, Display, TEXT("LogDroneStationsAsJson: droneStations=%d"), StationsJsonArray.Num());
	return JsonString;
}

FAIModOperationResult UAIModFunctionLibrary::PairDroneStations(UObject* WorldContextObject, const FString& StationBuildableId, const FString& TargetStationBuildableId)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}

	if (StationBuildableId.IsEmpty())
	{
		return FAIModOperationResult::Failure(TEXT("INVALID_REQUEST"), TEXT("stationBuildableId must be a non-empty string"));
	}

	AFGBuildableDroneStation* Station = Cast<AFGBuildableDroneStation>(FindBuildableById(World, StationBuildableId));
	if (!Station)
	{
		return FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"),
			FString::Printf(TEXT("'%s' is not a real, currently-existing AFGBuildableDroneStation"), *StationBuildableId));
	}
	AFGDroneStationInfo* Info = Station->GetInfo();
	if (!Info)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), FString::Printf(TEXT("'%s' has no AFGDroneStationInfo yet"), *StationBuildableId));
	}

	AFGDroneStationInfo* TargetInfo = nullptr;
	if (!TargetStationBuildableId.IsEmpty())
	{
		AFGBuildableDroneStation* TargetStation = Cast<AFGBuildableDroneStation>(FindBuildableById(World, TargetStationBuildableId));
		if (!TargetStation)
		{
			return FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"),
				FString::Printf(TEXT("'%s' is not a real, currently-existing AFGBuildableDroneStation"), *TargetStationBuildableId));
		}
		TargetInfo = TargetStation->GetInfo();
		if (!TargetInfo)
		{
			return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), FString::Printf(TEXT("'%s' has no AFGDroneStationInfo yet"), *TargetStationBuildableId));
		}
	}

	Info->PairStation(TargetInfo);

	if (Info->GetPairedStation() != TargetInfo)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"),
			TEXT("PairStation was called but GetPairedStation() does not reflect the requested pairing afterward"));
	}

	UE_LOG(LogAIModAI, Display, TEXT("PairDroneStations: %s -> %s"),
		*StationBuildableId, TargetStationBuildableId.IsEmpty() ? TEXT("<unpaired>") : *TargetStationBuildableId);

	return FAIModOperationResult::Success();
}
