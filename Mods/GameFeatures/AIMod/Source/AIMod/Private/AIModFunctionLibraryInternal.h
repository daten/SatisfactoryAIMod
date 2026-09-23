// Shared internal header for the UAIModFunctionLibrary implementation, which
// is split across several .cpp translation units (AIModFunctionLibrary*.cpp).
// Central include hub for those files, and home for the cross-file helpers
// (in namespace AIModInternal) that more than one of them share.
#pragma once

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
#include "FGClearanceInterface.h"
#include "FGClearanceData.h"
#include "Buildables/FGBuildableWalkway.h"
#include "Resources/FGBuildingDescriptor.h"
#include "Resources/FGBuildDescriptor.h"
#include "FGRecipeManager.h"
#include "Buildables/FGBuildableGenerator.h"
#include "Buildables/FGBuildableResourceExtractorBase.h"
#include "Hologram/FGBuildableHologram.h"
#include "FGFactoryColoringTypes.h"
#include "Buildables/FGBuildableDroneStation.h"
#include "Buildables/FGBuildableStorage.h"
#include "FGDroneStationInfo.h"
#include "FGVehicle.h"
#include "Resources/FGVehicleDescriptor.h"
#include "Hologram/FGVehicleHologram.h"
#include "FGRailroadTrackConnectionComponent.h"
#include "FGRailroadSubsystem.h"
#include "Hologram/FGRailroadTrackHologram.h"
#include "Hologram/FGRailroadVehicleHologram.h"
#include "Buildables/FGBuildableRailroadTrack.h"
#include "Components/SplineComponent.h"
#include "Hologram/FGVehiclePathSegmentHologram.h"
#include "WheeledVehicles/FGVehiclePathSegment.h"
#include "WheeledVehicles/FGVehiclePathNode.h"
#include "Buildables/FGBuildableSplineSnappedBase.h"
#include "FGTimeSubsystem.h"
#include "FGChatManager.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
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
#include "FGHealthComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
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
#include "Buildables/FGBuildableTrainPlatform.h"
#include "FGTrainPlatformConnection.h"
#include "Hologram/FGTrainPlatformHologram.h"
#include "FGDroneSubsystem.h"
#include "FGDroneStationInfo.h"
#include "Buildables/FGBuildableDockingStation.h"
#include "WheeledVehicles/FGWheeledVehicle.h"
#include "WheeledVehicles/FGWheeledVehicleMovementComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "WheeledVehicles/FGWheeledVehicleIdentifier.h"
#include "WheeledVehicles/FGVehicleAutopilotComponent.h"
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
#include "FGWaterVolume.h"
#include "FGDamageOverTimeVolume.h"
#include "FGDotComponent.h"
#include "FGDamageOverTime.h"
#include "DamageTypes/FGDamageType.h"
#include "FGMapFunctionLibrary.h"
#include "GameFramework/WorldSettings.h"
#include "FGProjectAssembly.h"
#include "FGGamePhase.h"
#include "FGGamePhaseManager.h"
#include "FGManta.h"
#include "Buildables/FGBuildableWaterPump.h"
#include "Buildables/FGBuildablePoleStackable.h"
#include "Hologram/FGStackablePoleHologram.h"


namespace AIModInternal
{
	// Forward declaration - the real definition lives further down;
	// ConstructBuildingAtPosition's faceBuildableId support needs to call it
	// earlier than that.
	// Anonymous namespaces in the same translation unit all merge into one,
	// so this and the later definition refer to the same function - only
	// textual order (declare before use) matters here.
	inline AFGBuildable* FindBuildableById(UWorld* World, const FString& BuildableId);
	inline FString WriteCondensedJson(const TSharedRef<FJsonObject>& RootObject);

	// "RealCharacter" instigator strategy - the proven-working
	// ConstructConveyorBelt body, kept as a fallback/comparison strategy
	// alongside the decoy-instigator strategies (see ConstructConveyorBelt's
	// own doc comment). Drives the REAL player's BuildGun - reliable belt
	// construction, but visibly moves the real camera, which is exactly what
	// the decoy strategies are trying to avoid. Kept selectable via
	// params.instigatorStrategy so competing fixes for the decoy path can be
	// tried without a fresh compile each time.
	inline void ConstructConveyorBelt_RealCharacterStrategy(UObject* WorldContextObject, const FString& SourceBuildableId, const FString& DestBuildableId, const FString& RecipeClassPath, const FString& RouteMode, const TOptional<FVector>& SourceConnectorPosition, const TOptional<FVector>& DestConnectorPosition, bool bDryRun, TFunction<void(const FAIModOperationResult&)> OnComplete);

	/**
	 * Shared ground-trace logic, factored out of ConstructBuildingAtPosition
	 * so world.groundHeight can expose the exact same real trace as a
	 * standalone, read-only query - makes placement Z deterministic without
	 * requiring the caller to already know that "z" is a +/-1000-unit
	 * search center, not a literal height (see docs/placement-lessons.md).
	 * A caller can query the real ground Z at an X/Y first, then pass
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
		// Which trace method produced the hit (diagnostic; surfaced by
		// world.groundHeight as "traceMethod"). Lets us confirm WHICH collision
		// path actually sees the landscape on this build, instead of guessing.
		FString HitMethod;
	};

	// Bounds a single world.terrainHeightGrid call's cost - every point
	// runs a real synchronous line trace on the game thread (same as
	// FindGroundAtXY below), so an unbounded grid risks a real frame
	// hitch. 10000 = a 100x100 grid, comfortably enough for "survey an
	// immediate build area" without needing this raised.
	constexpr int64 MaxTerrainHeightGridPoints = 10000;

	inline FGroundTraceResult FindGroundAtXY(UWorld* World, float X, float Y, float ZSearchCenter, AActor* IgnoreActor)
	{
		FGroundTraceResult Result;

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AIModGroundTrace), false);
		if (IgnoreActor)
		{
			QueryParams.AddIgnoredActor(IgnoreActor);
		}
		// The Satisfactory LANDSCAPE ignores ECC_Visibility (that is why the old
		// Visibility trace only ever hit placed buildables, never the natural
		// terrain - world.groundHeight / world.terrainHeightGrid "couldn't see
		// terrain", 2026-09-23). Rather than guess a single replacement channel
		// (each guess costs a full mod rebuild to test), try the paths most
		// likely to hit the real ground, in order, and RECORD which one worked
		// (Result.HitMethod, surfaced as world.groundHeight "traceMethod"):
		//   1) object-type query for WorldStatic/WorldDynamic - the landscape is
		//      a WorldStatic object, so this should hit it directly regardless of
		//      per-channel response quirks;
		//   2) the BuildGun channel (ECC_GameTraceChannel5 == "BuildGun" per
		//      Config/DefaultEngine.ini) - the build gun's own ground trace, how
		//      foundations snap to the ground;
		//   3) MapGeneration (ECC_GameTraceChannel12) - terrain-generation trace;
		//   4) ECC_Visibility - legacy; hits placed buildables (kept last so a
		//      building is still reported when nothing else blocks).
		// We use the raw ECC_GameTraceChannelN enums, not FactoryGame's TC_BuildGun
		// constant, which is declared FACTORYGAME_API but NOT exported by the CSS
		// FactoryGame binary (link error LNK2019). WIDE +/-100 km window so
		// SCANNING works without the caller already knowing the ground height (the
		// old +/-1000 window required a near-correct ZSearchCenter); +/-100 km
		// covers all real terrain, well below the space elevator / project
		// assembly station.
		const float TraceUpSpan = 100000.0f;
		const float TraceDownSpan = 100000.0f;
		const FVector TraceStart(X, Y, ZSearchCenter + TraceUpSpan);
		const FVector TraceEnd(X, Y, ZSearchCenter - TraceDownSpan);

		// (1) object-type query
		{
			FCollisionObjectQueryParams ObjParams;
			ObjParams.AddObjectTypesToQuery(ECC_WorldStatic);
			ObjParams.AddObjectTypesToQuery(ECC_WorldDynamic);
			if (World->LineTraceSingleByObjectType(Result.Hit, TraceStart, TraceEnd, ObjParams, QueryParams))
			{
				Result.bFound = true;
				Result.HitMethod = TEXT("ObjectType:WorldStatic/Dynamic");
				return Result;
			}
		}

		// (2)-(4) channel traces, first hit wins
		struct FTraceChan { ECollisionChannel Channel; const TCHAR* Name; };
		const FTraceChan TraceChans[] = {
			{ ECC_GameTraceChannel5,  TEXT("BuildGun") },
			{ ECC_GameTraceChannel12, TEXT("MapGeneration") },
			{ ECC_Visibility,         TEXT("Visibility") },
		};
		for (const FTraceChan& TC : TraceChans)
		{
			if (World->LineTraceSingleByChannel(Result.Hit, TraceStart, TraceEnd, TC.Channel, QueryParams))
			{
				Result.bFound = true;
				Result.HitMethod = TC.Name;
				return Result;
			}
		}

		// Fallback ConstructBuildingAtPosition always used: the literal
		// search-center point, facing straight up.
		Result.Hit.Location = FVector(X, Y, ZSearchCenter);
		Result.Hit.ImpactPoint = Result.Hit.Location;
		Result.Hit.Normal = FVector::UpVector;
		Result.Hit.ImpactNormal = FVector::UpVector;
		Result.Hit.bBlockingHit = true;
		Result.HitMethod = TEXT("none");
		return Result;
	}

	// GetResourcePurityText() looked like a plain display string but is
	// actually Slate rich-text markup meant for on-screen UI (its own doc
	// comment says "For UI") - it returns literal "<Bold>(Normal)</>"
	// instead of "Normal". Use the raw enum (GetResourcePurity(), also
	// "For UI" per its comment but returns the actual EResourcePurity value)
	// and map it ourselves, consistent with
	// ProductionStatusToString/FactoryConnectionDirectionToString below.
	inline FString ResourcePurityToString(EResourcePurity Purity)
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
	inline FString ResourceNodeTypeToString(EResourceNodeType Type)
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

	inline FString FrackingSatelliteStateToString(EFrackingSatelliteState State)
	{
		switch (State)
		{
		case EFrackingSatelliteState::FSS_Untouched: return TEXT("Untouched");
		case EFrackingSatelliteState::FSS_Active: return TEXT("Active");
		case EFrackingSatelliteState::FSS_Inactive: return TEXT("Inactive");
		default: return TEXT("Unknown");
		}
	}

	// Takes AFGResourceNodeBase so it also covers
	// AFGResourceNodeFrackingCore (a Resource Well Pressurizer's real
	// target, NOT an AFGResourceNode - see that function's doc comment).
	// GetResourcePurity() only exists on AFGResourceNode (per source -
	// FGResourceNode.h, not declared on the shared AFGResourceNodeBase),
	// so it's read conditionally here; a Fracking Core has no meaningful
	// purity of its own.
	inline FAIModResourceNodeTelemetry MakeResourceNodeTelemetry(AFGResourceNodeBase* Node)
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
	inline TArray<FAIModResourceNodeTelemetry> CollectResourceNodeTelemetry(UWorld* World)
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
	inline TArray<AFGBuildable*> CollectAllBuildables(UWorld* World)
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
	inline const TCHAR* const LightweightIdPrefix = TEXT("lightweight:");

	/**
	 * Lightweight buildables (see docs/lightweight-buildable-research.md)
	 * are NOT AFGBuildable actors at all - they're stored as
	 * FRuntimeBuildableInstanceData in AFGLightweightBuildableSubsystem,
	 * for performance at scale (thousands of foundation/wall pieces would
	 * be expensive as full actors). Placing a foundation via
	 * ConstructBuildingAtPosition reports success and the piece is
	 * visible in-game, but it appears in neither the
	 * proximity-based buildableId lookup nor the full world.buildables
	 * list (the real AFGBuildable actor count is unchanged before and
	 * after). GetPathName() is meaningless here - there's no actor - so
	 * these use "lightweight:<BuildableClassPath>|<Index>" instead,
	 * identity being (class, array index) into
	 * GetAllLightweightBuildableInstances(). '|' rather than a second
	 * ':' as the separator - Unreal object paths can themselves contain
	 * ':' (e.g. a level's "Persistent_Level:PersistentLevel" nesting),
	 * but never '|'.
	 */
	inline FString MakeLightweightBuildableId(const TSubclassOf<AFGBuildable>& BuildableClass, int32 Index)
	{
		return FString::Printf(TEXT("%s%s|%d"), LightweightIdPrefix, *BuildableClass->GetPathName(), Index);
	}

	inline bool IsLightweightBuildableId(const FString& BuildableId)
	{
		return BuildableId.StartsWith(LightweightIdPrefix);
	}

	// Splits a "lightweight:<ClassPath>|<Index>" id back into its class
	// path and index. Returns false (and leaves outputs unchanged) if
	// BuildableId isn't well-formed.
	inline bool ParseLightweightBuildableId(const FString& BuildableId, FString& OutClassPath, int32& OutIndex)
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
	inline TSubclassOf<UFGBuildingDescriptor> ResolveBuildingDescriptorClassForRecipe(const FString& RecipeClassPath)
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

	inline TSubclassOf<AFGBuildable> ResolveBuildableClassForRecipe(const FString& RecipeClassPath)
	{
		const TSubclassOf<UFGBuildingDescriptor> BuildingDescriptorClass = ResolveBuildingDescriptorClassForRecipe(RecipeClassPath);
		if (!BuildingDescriptorClass)
		{
			return nullptr;
		}
		return UFGBuildingDescriptor::GetBuildableClass(BuildingDescriptorClass);
	}

	// Vehicles - UFGVehicleDescriptor is a SIBLING of
	// UFGBuildingDescriptor (both derive from UFGBuildDescriptor
	// separately, confirmed from source), so a vehicle recipe's product
	// is never a UFGBuildingDescriptor and ResolveBuildableClassForRecipe
	// above naturally returns nullptr for one - this is the parallel
	// resolver for the vehicle case, same shape.
	inline TSubclassOf<AFGVehicle> ResolveVehicleClassForRecipe(const FString& RecipeClassPath)
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
	inline TSubclassOf<AFGConveyorBeltHologram> ResolveConveyorBeltHologramClassForRecipe(const FString& RecipeClassPath)
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
	// pipelines. AFGPipelineHologram is a
	// sibling of AFGConveyorBeltHologram - both derive directly from
	// AFGSplineHologram - confirmed from source, not assumed.
	inline TSubclassOf<AFGPipelineHologram> ResolvePipelineHologramClassForRecipe(const FString& RecipeClassPath)
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
	inline UFGPipeConnectionComponentBase* FindFreePipeConnection(AFGBuildable* Buildable, EPipeConnectionType Type)
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
	// exact PCT_PRODUCER/PCT_CONSUMER filter above. But several real,
	// common fluid-pipe buildables -
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
	inline UFGPipeConnectionComponentBase* FindFreeFluidPipeConnection(AFGBuildable* Buildable, EPipeConnectionType PreferredType)
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
	// (see docs/hypertube-research.md): they're all
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
	inline UFGPipeConnectionComponentBase* FindFreeHyperPipeConnection(AFGBuildable* Buildable)
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

	// Railroad tracks - UFGRailroadTrackConnectionComponent
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
	inline UFGRailroadTrackConnectionComponent* FindFreeRailroadConnection(AFGBuildable* Buildable)
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

	// The FREE railroad connection on a buildable closest to a world position -
	// lets a caller pick WHICH end of a multi-connector track (e.g. a station's
	// two ends) to join, so a loop's two curves join matching sides instead of
	// whatever "first free" happens to return.
	inline UFGRailroadTrackConnectionComponent* FindFreeRailroadConnectionNearest(AFGBuildable* Buildable, const FVector& WorldPos)
	{
		TArray<UFGRailroadTrackConnectionComponent*> Connections;
		Buildable->GetComponents<UFGRailroadTrackConnectionComponent>(Connections);
		UFGRailroadTrackConnectionComponent* Best = nullptr;
		float BestDistSq = TNumericLimits<float>::Max();
		for (UFGRailroadTrackConnectionComponent* Connection : Connections)
		{
			if (IsValid(Connection) && !Connection->IsConnected())
			{
				const float DistSq = FVector::DistSquared(Connection->GetConnectorLocation(), WorldPos);
				if (DistSq < BestDistSq)
				{
					BestDistSq = DistSq;
					Best = Connection;
				}
			}
		}
		return Best;
	}

	// World-space AABB of a buildable's clearance footprint, for
	// FAIModBuildableTelemetry.Bounds. A class's local clearance boxes (its
	// CDO FFGClearanceData) are constant per class, so cache the composed
	// LOCAL box per class and just TransformBy each instance/actor transform -
	// cheap enough to run over every buildable each world.buildables call.
	// Returns false (Out* untouched) when the class exposes no clearance data
	// on its CDO (dynamic-clearance buildables - conveyors, beams - compute it
	// per-instance, so it is empty on the CDO). The box is the CLEARANCE
	// footprint, not the render mesh, which is exactly what matters for
	// overhang/overlap/spacing checks and for a ramp's Z rise.
	inline bool TryComputeBuildableWorldBounds(UClass* BuildableClass, const FTransform& WorldTransform, FVector& OutMin, FVector& OutMax)
	{
		if (!BuildableClass)
		{
			return false;
		}
		static TMap<TWeakObjectPtr<UClass>, FBox> LocalBoxCache;
		FBox LocalBox(ForceInit);
		if (const FBox* Cached = LocalBoxCache.Find(BuildableClass))
		{
			LocalBox = *Cached;
		}
		else
		{
			if (BuildableClass->ImplementsInterface(UFGClearanceInterface::StaticClass()))
			{
				if (UObject* CDO = BuildableClass->GetDefaultObject())
				{
					TArray<FFGClearanceData> ClearanceData;
					IFGClearanceInterface::Execute_GetClearanceData(CDO, ClearanceData);
					for (const FFGClearanceData& Entry : ClearanceData)
					{
						LocalBox += Entry.GetTransformedClearanceBox();
					}
				}
			}
			LocalBoxCache.Add(BuildableClass, LocalBox);
		}
		if (!LocalBox.IsValid)
		{
			return false;
		}
		const FBox WorldBox = LocalBox.TransformBy(WorldTransform);
		OutMin = WorldBox.Min;
		OutMax = WorldBox.Max;
		return true;
	}

	inline TArray<FAIModBuildableTelemetry> CollectLightweightBuildableTelemetry(UWorld* World)
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
				// it. Note (see docs/lightweight-buildable-research.md
				// "Index stability"): this does NOT mean (class, index)
				// identity is stable over time - a whole batch of indices
				// can shift after unrelated deletes (a periodic compaction,
				// not a pure tombstone) - always re-resolve an id by
				// position before trusting an old one.
				if (!InstanceData.IsValid())
				{
					continue;
				}

				FAIModBuildableTelemetry Telemetry;
				Telemetry.Id = MakeLightweightBuildableId(BuildableClass, Index);
				Telemetry.BuildableClass = BuildableClass->GetPathName();
				Telemetry.Position = InstanceData.Transform.GetLocation();
				Telemetry.Rotation = InstanceData.Transform.Rotator();
				Telemetry.bHasBounds = TryComputeBuildableWorldBounds(BuildableClass, InstanceData.Transform, Telemetry.BoundsMin, Telemetry.BoundsMax);
				Result.Add(MoveTemp(Telemetry));
			}
		}
		return Result;
	}

	inline TArray<FAIModBuildableTelemetry> CollectBuildableTelemetry(UWorld* World)
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
			Telemetry.bHasBounds = TryComputeBuildableWorldBounds(Buildable->GetClass(), Buildable->GetActorTransform(), Telemetry.BoundsMin, Telemetry.BoundsMax);
			Result.Add(MoveTemp(Telemetry));
		}
		Result.Append(CollectLightweightBuildableTelemetry(World));
		return Result;
	}

	// EProductionStatus (FGBuildable.h) is a plain C++ enum class, not a
	// UENUM, so there is no UEnum::GetDisplayValueAsText reflection path.
	inline FString ProductionStatusToString(EProductionStatus Status)
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

	inline TArray<FAIModInventoryItemTelemetry> CollectInventoryTelemetry(UFGInventoryComponent* Inventory)
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

	inline FAIModManufacturerTelemetry MakeManufacturerTelemetry(AFGBuildableManufacturer* Manufacturer)
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

	inline TArray<FAIModManufacturerTelemetry> CollectManufacturerTelemetry(UWorld* World)
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
	inline FString FactoryConnectionDirectionToString(EFactoryConnectionDirection Direction)
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

	inline FAIModFactoryConnectionTelemetry MakeConnectionTelemetry(const FString& OwnerId, UFGFactoryConnectionComponent* Connection)
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

	inline TArray<FAIModFactoryConnectionTelemetry> CollectFactoryConnectionTelemetry(UWorld* World)
	{
		TArray<FAIModFactoryConnectionTelemetry> Result;

		// Discover UFGFactoryConnectionComponents generically via
		// AActor::GetComponents<>() rather than maintaining a per-class-
		// hierarchy enumeration list. AFGBuildableFactory (machines),
		// AFGBuildableConveyorBase (belts/lifts, via named
		// GetConnection0()/GetConnection1()), and
		// AFGBuildableConveyorAttachment (splitters/mergers, via protected
		// mInputs/mOutputs arrays with no public getter at all) are THREE
		// separate sibling hierarchies, none deriving from another, each
		// with its own connection storage and accessor (or none). Generic
		// component discovery is robust against any other sibling hierarchy
		// not yet found, since UFGFactoryConnectionComponent is always a
		// component on the owning AFGBuildable regardless of which subclass
		// it is.
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

	inline FString PipeConnectionTypeToString(EPipeConnectionType Type)
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

	inline FAIModPipeConnectionTelemetry MakePipeConnectionTelemetry(const FString& OwnerId, UFGPipeConnectionComponentBase* Connection)
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
	// hierarchies for factory connections; pipes
	// are a fourth, entirely separate type hierarchy - UFGPipeConnectionComponentBase,
	// not UFGFactoryConnectionComponent - covering both fluid pipes and
	// hypertubes, since UFGPipeConnectionComponentHyper is a subclass of
	// the same base.
	inline TArray<FAIModPipeConnectionTelemetry> CollectPipeConnectionTelemetry(UWorld* World)
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
	inline AFGBuildableManufacturer* FindManufacturerById(UWorld* World, const FString& BuildableId)
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

	inline UConfigManager* GetAIModConfigManager(UObject* WorldContextObject)
	{
		UGameInstance* GameInstance = Cast<UGameInstance>(WorldContextObject);
		if (!GameInstance)
		{
			UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
			GameInstance = World ? World->GetGameInstance() : nullptr;
		}
		return GameInstance ? GameInstance->GetSubsystem<UConfigManager>() : nullptr;
	}

	inline UConfigPropertySection* GetAIModConfigRootSection(UObject* WorldContextObject)
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

	// UFGDotComponent::mDotClass is a protected UPROPERTY with no public
	// accessor; reflection is the only non-invasive way to read it. This is
	// internal telemetry sourcing, not an externally-exposed generic
	// property reader (Safety and Stability Boundary stands).
	inline TSubclassOf<UFGDamageOverTime> GetDotClassOfComponent(const UFGDotComponent* DotComponent)
	{
		if (!DotComponent)
		{
			return nullptr;
		}
		const FClassProperty* DotClassProperty = FindFProperty<FClassProperty>(UFGDotComponent::StaticClass(), TEXT("mDotClass"));
		if (!DotClassProperty)
		{
			return nullptr;
		}
		return Cast<UClass>(DotClassProperty->GetPropertyValue_InContainer(DotComponent));
	}

	inline AFGDamageOverTimeVolume* FindDamageVolumeById(UWorld* World, const FString& VolumeId)
	{
		for (TActorIterator<AFGDamageOverTimeVolume> It(World); It; ++It)
		{
			if (IsValid(*It) && (*It)->GetPathName() == VolumeId)
			{
				return *It;
			}
		}
		return nullptr;
	}

	inline TSharedRef<FJsonObject> MakeVectorJson(const FVector& V)
	{
		const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetNumberField(TEXT("x"), V.X);
		Object->SetNumberField(TEXT("y"), V.Y);
		Object->SetNumberField(TEXT("z"), V.Z);
		return Object;
	}

	inline AFGProjectAssembly* FindProjectAssembly(UWorld* World)
	{
		for (TActorIterator<AFGProjectAssembly> It(World); It; ++It)
		{
			if (IsValid(*It))
			{
				return *It;
			}
		}
		return nullptr;
	}

	inline TSharedRef<FJsonObject> MakeGamePhaseJson(UFGGamePhase* Phase)
	{
		const TSharedRef<FJsonObject> PhaseObject = MakeShared<FJsonObject>();
		if (Phase)
		{
			PhaseObject->SetStringField(TEXT("assetPath"), Phase->GetPathName());
			PhaseObject->SetStringField(TEXT("displayName"), Phase->mDisplayName.ToString());
			PhaseObject->SetNumberField(TEXT("lastTierOfPhase"), Phase->mLastTierOfPhase);
			PhaseObject->SetNumberField(TEXT("gamePhaseEnum"), static_cast<int32>(Phase->mGamePhase.GetValue()));
		}
		return PhaseObject;
	}

	inline AFGManta* FindMantaById(UWorld* World, const FString& MantaId)
	{
		for (TActorIterator<AFGManta> It(World); It; ++It)
		{
			if (IsValid(*It) && (*It)->GetPathName() == MantaId)
			{
				return *It;
			}
		}
		return nullptr;
	}

	// AFGManta's tuning fields are private UPROPERTYs with no setters;
	// reflection is the only non-invasive access. Scoped to AFGManta only.
	inline float GetMantaFloat(const AFGManta* Manta, const TCHAR* PropName, float Fallback = 0.0f)
	{
		if (const FFloatProperty* Prop = FindFProperty<FFloatProperty>(AFGManta::StaticClass(), PropName))
		{
			return Prop->GetPropertyValue_InContainer(Manta);
		}
		return Fallback;
	}
	inline void SetMantaFloat(AFGManta* Manta, const TCHAR* PropName, float Value)
	{
		if (const FFloatProperty* Prop = FindFProperty<FFloatProperty>(AFGManta::StaticClass(), PropName))
		{
			Prop->SetPropertyValue_InContainer(Manta, Value);
		}
	}
	inline bool GetMantaBool(const AFGManta* Manta, const TCHAR* PropName, bool Fallback = false)
	{
		if (const FBoolProperty* Prop = FindFProperty<FBoolProperty>(AFGManta::StaticClass(), PropName))
		{
			return Prop->GetPropertyValue_InContainer(Manta);
		}
		return Fallback;
	}
	inline void SetMantaBool(AFGManta* Manta, const TCHAR* PropName, bool Value)
	{
		if (const FBoolProperty* Prop = FindFProperty<FBoolProperty>(AFGManta::StaticClass(), PropName))
		{
			Prop->SetPropertyValue_InContainer(Manta, Value);
		}
	}

	// Shared row filter for the 2a filtered-telemetry variants
	// (docs/build-efficiency-plan.md): id-substring OR-match plus an
	// optional AABB on position (Z participates only when the caller
	// supplied a non-degenerate Z range, so a flat XY box "just works").
	inline bool AIModTelemetryRowPasses(
		const FString& Id, const FVector& Position,
		const TArray<FString>& IdSubstrings, bool bBoundsSet,
		const FVector& BoundsMin, const FVector& BoundsMax)
	{
		if (IdSubstrings.Num() > 0)
		{
			bool bAnyMatch = false;
			for (const FString& Substring : IdSubstrings)
			{
				if (Id.Contains(Substring))
				{
					bAnyMatch = true;
					break;
				}
			}
			if (!bAnyMatch)
			{
				return false;
			}
		}
		if (bBoundsSet)
		{
			if (Position.X < BoundsMin.X || Position.X > BoundsMax.X ||
				Position.Y < BoundsMin.Y || Position.Y > BoundsMax.Y)
			{
				return false;
			}
			if (!FMath::IsNearlyEqual(BoundsMin.Z, BoundsMax.Z) &&
				(Position.Z < BoundsMin.Z || Position.Z > BoundsMax.Z))
			{
				return false;
			}
		}
		return true;
	}

inline UFGInventoryComponent* ResolveBuildableRoleInventory(AFGBuildable* Buildable, const FString& Role, FString& OutDesc, AFGBuildableDroneStation*& OutDroneFuelStation)
{
	OutDroneFuelStation = nullptr;
	if (AFGBuildableDroneStation* Drone = Cast<AFGBuildableDroneStation>(Buildable))
	{
		if (Role == TEXT("output")) { OutDesc = TEXT("droneStation.output"); return Drone->GetOutputInventory(); }
		if (Role == TEXT("fuel")) { OutDesc = TEXT("droneStation.fuel"); OutDroneFuelStation = Drone; return Drone->GetFuelInventory(); }
		OutDesc = TEXT("droneStation.input"); return Drone->GetInputInventory();
	}
	if (AFGBuildableDockingStation* Dock = Cast<AFGBuildableDockingStation>(Buildable))
	{
		if (Role == TEXT("fuel")) { OutDesc = TEXT("dockingStation.fuel"); return Dock->GetFuelInventory(); }
		OutDesc = TEXT("dockingStation.inventory"); return Dock->GetInventory();
	}
	if (AFGBuildableStorage* Storage = Cast<AFGBuildableStorage>(Buildable))
	{
		OutDesc = TEXT("storage.inventory"); return Storage->GetStorageInventory();
	}
	OutDesc = TEXT("firstInventoryComponent");
	return Buildable->FindComponentByClass<UFGInventoryComponent>();
}

inline bool IsProtectedAlienArtifactClass(const FString& ItemClassPath)
{
	return ItemClassPath.Contains(TEXT("/Prototype/WAT/"));
}

	inline TSharedRef<FJsonObject> InventoryItemToJson(const FAIModInventoryItemTelemetry& Item)
	{
		const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("itemClass"), Item.ItemClass);
		Object->SetStringField(TEXT("itemName"), Item.ItemName);
		Object->SetNumberField(TEXT("count"), Item.Count);
		return Object;
	}

	inline TArray<TSharedPtr<FJsonValue>> InventoryToJsonArray(const TArray<FAIModInventoryItemTelemetry>& Items)
	{
		TArray<TSharedPtr<FJsonValue>> Array;
		Array.Reserve(Items.Num());
		for (const FAIModInventoryItemTelemetry& Item : Items)
		{
			Array.Add(MakeShared<FJsonValueObject>(InventoryItemToJson(Item)));
		}
		return Array;
	}

	inline TSharedRef<FJsonObject> ManufacturerToJson(const FAIModManufacturerTelemetry& Manufacturer)
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

	inline FString WriteCondensedJson(const TSharedRef<FJsonObject>& RootObject)
	{
		FString JsonString;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonString);
		FJsonSerializer::Serialize(RootObject, Writer);
		return JsonString;
	}

	// One connector row for world.connectorLayout - LOCAL-frame data
	// derived from a class-default component template (never a live,
	// world-transformed instance).
	inline void AddConnectorLayoutRow(
		TArray<TSharedPtr<FJsonValue>>& OutRows, const UFGFactoryConnectionComponent* Template,
		const FTransform& ComposedLocalTransform, const TCHAR* SourceTag)
	{
		const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
		Row->SetStringField(TEXT("name"), Template->GetName());
		Row->SetStringField(TEXT("direction"), FactoryConnectionDirectionToString(Template->GetDirection()));
		Row->SetNumberField(TEXT("clearance"), Template->GetConnectorClearance());
		Row->SetStringField(TEXT("source"), SourceTag);

		const FVector LocalPosition = ComposedLocalTransform.GetLocation();
		const TSharedRef<FJsonObject> PositionObject = MakeShared<FJsonObject>();
		PositionObject->SetNumberField(TEXT("x"), LocalPosition.X);
		PositionObject->SetNumberField(TEXT("y"), LocalPosition.Y);
		PositionObject->SetNumberField(TEXT("z"), LocalPosition.Z);
		Row->SetObjectField(TEXT("localPosition"), PositionObject);

		// Mirrors the live path's GetConnectorNormal() (the component
		// rotation's forward vector), but in the actor's local frame.
		const FVector LocalNormal = ComposedLocalTransform.GetRotation().GetForwardVector();
		const TSharedRef<FJsonObject> NormalObject = MakeShared<FJsonObject>();
		NormalObject->SetNumberField(TEXT("x"), LocalNormal.X);
		NormalObject->SetNumberField(TEXT("y"), LocalNormal.Y);
		NormalObject->SetNumberField(TEXT("z"), LocalNormal.Z);
		Row->SetObjectField(TEXT("localNormal"), NormalObject);

		OutRows.Add(MakeShared<FJsonValueObject>(Row));
	}

	// Depth-first SCS walk composing each node's template transform onto
	// its parent chain (child-relative * parent, Unreal's compose order),
	// so a connector attached to a non-root scene component still reports
	// a correct actor-local transform.
	inline void WalkScsNode(
		TArray<TSharedPtr<FJsonValue>>& OutRows, const USCS_Node* Node, const FTransform& ParentTransform)
	{
		if (!Node)
		{
			return;
		}
		FTransform Composed = ParentTransform;
		if (const USceneComponent* SceneTemplate = Cast<USceneComponent>(Node->ComponentTemplate))
		{
			Composed = SceneTemplate->GetRelativeTransform() * ParentTransform;
			if (const UFGFactoryConnectionComponent* ConnectionTemplate = Cast<UFGFactoryConnectionComponent>(SceneTemplate))
			{
				AddConnectorLayoutRow(OutRows, ConnectionTemplate, Composed, TEXT("scs"));
			}
		}
		for (const USCS_Node* Child : Node->GetChildNodes())
		{
			WalkScsNode(OutRows, Child, Composed);
		}
	}

	inline FString CompassViewDistanceToString(ECompassViewDistance Distance)
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

	inline ECompassViewDistance ParseCompassViewDistance(const FString& Value)
	{
		if (Value.Equals(TEXT("Near"), ESearchCase::IgnoreCase)) { return ECompassViewDistance::CVD_Near; }
		if (Value.Equals(TEXT("Mid"), ESearchCase::IgnoreCase)) { return ECompassViewDistance::CVD_Mid; }
		if (Value.Equals(TEXT("Far"), ESearchCase::IgnoreCase)) { return ECompassViewDistance::CVD_Far; }
		if (Value.Equals(TEXT("Always"), ESearchCase::IgnoreCase)) { return ECompassViewDistance::CVD_Always; }
		return ECompassViewDistance::CVD_Off;
	}

	inline FString ChatMessageTypeToString(EFGChatMessageType Type)
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

	inline AFGBuildable* FindBuildableById(UWorld* World, const FString& BuildableId)
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

	// Power connector selection must be EPowerConnectionType-aware
	// (FGPowerConnectionComponent.h: "Power connections of different
	// types are incompatible", real PCT_Default/PCT_PowerTower/PCT_Any
	// enum). A Power Tower (AFGBuildablePowerPole with
	// mPowerPoleType==PPT_TOWER - per source, AFGBuildablePowerTower is a
	// separate near-empty class, unused anywhere else in the header tree
	// and NOT the real buildable) genuinely has TWO power connectors: one
	// PCT_PowerTower (the real, per-instance-configurable long-range link
	// to another tower - AFGBuildablePowerPole::GetPowerTowerWireMaxLength(),
	// distinct from AFGBuildableWire::mMaxPowerTowerLength on the wire
	// recipe itself) and one PCT_Default (short range, for a nearby
	// pole/machine, same type ordinary poles/machines use). Picking the
	// first free connector per buildable independently can silently pair a
	// tower's short-range connector for what should be a long-range
	// tower-to-tower link (or vice versa), either failing outright (type
	// mismatch) or succeeding against the wrong distance limit.
	//
	// So connector selection is a joint decision over BOTH
	// buildables at once: pass 1 requires an EXACT GetPowerConnectionType()
	// match on both sides (so two Power Towers pick their PCT_PowerTower
	// connectors, and everything else - poles, machines, a tower's own
	// short-range side - pairs PCT_Default to PCT_Default); pass 2 falls
	// back to any pairing where at least one side is the real PCT_Any
	// wildcard type, per the enum's own "incompatible" doc comment
	// implying Any is the one documented exception. Tower-Tower and
	// Default-Default pairings live-verified in factory builds; the
	// mixed tower/pole short-range cases remain unverified (see
	// docs/test-backlog.md).
	// Optional connector pinning (docs/build-efficiency-plan.md 2d):
	// like connectConveyor's sourceConnectorPosition, a pin
	// restricts that side's candidates to connections within
	// PinTolerance of the given world position - deterministic per-port
	// selection on multi-connector buildables (a Power Tower's dual
	// connectors, a wall outlet bank). Unset pins keep the old joint
	// type-aware selection unchanged.
	inline bool FindPowerConnectionPair(AFGBuildable* BuildableA, AFGBuildable* BuildableB, UFGPowerConnectionComponent*& OutConnectionA, UFGPowerConnectionComponent*& OutConnectionB,
		const TOptional<FVector>& PinA = TOptional<FVector>(), const TOptional<FVector>& PinB = TOptional<FVector>())
	{
		OutConnectionA = nullptr;
		OutConnectionB = nullptr;
		constexpr float PinTolerance = 150.0f;

		TArray<UFGPowerConnectionComponent*> ConnectionsA;
		BuildableA->GetComponents<UFGPowerConnectionComponent>(ConnectionsA);
		TArray<UFGPowerConnectionComponent*> ConnectionsB;
		BuildableB->GetComponents<UFGPowerConnectionComponent>(ConnectionsB);

		auto IsFree = [](const UFGPowerConnectionComponent* Connection) { return IsValid(Connection) && Connection->GetNumFreeConnections() > 0; };
		auto MatchesPin = [PinTolerance](const UFGPowerConnectionComponent* Connection, const TOptional<FVector>& Pin)
		{
			return !Pin.IsSet() || FVector::Dist(Connection->GetComponentLocation(), Pin.GetValue()) <= PinTolerance;
		};

		for (UFGPowerConnectionComponent* CandidateA : ConnectionsA)
		{
			if (!IsFree(CandidateA) || !MatchesPin(CandidateA, PinA)) { continue; }
			for (UFGPowerConnectionComponent* CandidateB : ConnectionsB)
			{
				if (!IsFree(CandidateB) || !MatchesPin(CandidateB, PinB)) { continue; }
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
			if (!IsFree(CandidateA) || !MatchesPin(CandidateA, PinA)) { continue; }
			for (UFGPowerConnectionComponent* CandidateB : ConnectionsB)
			{
				if (!IsFree(CandidateB) || !MatchesPin(CandidateB, PinB)) { continue; }
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
	// SnapOnly fallback (see docs/placement-lessons.md's "Conveyor walls
	// are real connectors" section): conveyor walls/poles expose exactly
	// one UFGFactoryConnectionComponent with GetDirection()==FCD_SNAP_ONLY
	// (per world.connections on a wall-lift-wall structure and source -
	// FGFactoryConnectionComponent.h: "Special case for conveyor poles").
	// The strict `== Direction` match above always skips these, so without
	// this fallback ConstructConveyorLift/ConnectConveyor could never
	// target a wall at all (NO_FACTORY_CONNECTION). SnapOnly's
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
	inline UFGFactoryConnectionComponent* FindFreeFactoryConnection(AFGBuildable* Buildable, EFactoryConnectionDirection Direction)
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

	// Position-targeted variant: deterministic selection of ONE SPECIFIC
	// connector on a multi-port buildable like a splitter/merger, by its
	// real world position - never
	// "first free"/"nearest"/component-array-order. The caller (Python
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
	// SnapOnly fallback: same reasoning as FindFreeFactoryConnection
	// above - a free exact-direction match wins if one is within tolerance,
	// otherwise the nearest SnapOnly connector (wall/pole) within tolerance
	// is used, without gating on IsConnected() since that's always false
	// for SnapOnly by engine design.
	inline UFGFactoryConnectionComponent* FindFreeFactoryConnectionNear(AFGBuildable* Buildable, EFactoryConnectionDirection Direction, const FVector& TargetWorldPosition, float ToleranceCm = 10.0f)
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

	// Populate the camera-ray fields (TraceStart/TraceEnd/Distance/Time) a
	// real build-gun trace ALWAYS carries and a bare synthetic hit would
	// otherwise leave at zero-vectors. Load-bearing for the conveyor lift's
	// free-end height (see ConstructConveyorLift's doc comment), and the
	// prime suspect for the belt-path player-distance failures: the
	// identical world.connectConveyor call fails "Conveyor Belt is too
	// long!" when the real player stands beyond ~5000 units from the
	// connection and succeeds after nothing but a teleport closer
	// (threshold consistent with the belt's own maxSplineLength, 5600) -
	// i.e. some part of the spline routing falls back to the REAL
	// camera/trace when the synthetic hit carries no ray. The synthetic
	// ray here mimics a player standing a short distance back from the
	// connector, slightly above it, aiming at it - fully determined by
	// the connector itself, independent of where the real player is.
	// Not yet confirmed whether this alone removes the player-distance
	// dependence - until then, keep teleporting the player near belt
	// connections (see RPC_REFERENCE.md).
	inline void PopulateSyntheticTraceRay(FHitResult& Hit)
	{
		FVector BackDir = Hit.Normal;
		BackDir.Z = 0.0;
		if (!BackDir.Normalize())
		{
			BackDir = FVector::ForwardVector;
		}
		Hit.TraceStart = Hit.Location + BackDir * 1200.0 + FVector(0.0, 0.0, 400.0);
		const FVector RayDir = (Hit.Location - Hit.TraceStart).GetSafeNormal();
		Hit.TraceEnd = Hit.Location + RayDir * 600.0;
		Hit.Distance = static_cast<float>(FVector::Dist(Hit.TraceStart, Hit.Location));
		const float TraceLength = static_cast<float>(FVector::Dist(Hit.TraceStart, Hit.TraceEnd));
		Hit.Time = TraceLength > KINDA_SMALL_NUMBER ? Hit.Distance / TraceLength : 0.0f;
	}

	// Restores a previously-saved build gun mBuildDistanceMax (protected
	// float UPROPERTY, accessed by reflection). A sentinel SavedRange < 0
	// means "nothing to restore" and this is a no-op. Its former companion
	// setter was removed: raising the clamp to fix far belts
	// did NOT work (the real limit is the camera AIM, addressed by the
	// auto-teleport in ConstructConveyorBelt_RealCharacterStrategy) and
	// actively REGRESSED near belts - an effectively-unlimited trace made
	// AutoRouteSpline route absurdly long splines. The restore is kept as
	// a harmless no-op so the existing belt call sites stay valid.
	inline void RestoreBuildGunTraceRange(AFGBuildGun* BuildGun, float SavedRange)
	{
		if (!BuildGun || SavedRange < 0.0f) { return; }
		if (FFloatProperty* Prop = FindFProperty<FFloatProperty>(BuildGun->GetClass(), TEXT("mBuildDistanceMax")))
		{
			Prop->SetPropertyValue_InContainer(BuildGun, SavedRange);
		}
	}

	// Shared by ConstructWaterPumpNearReference and
	// ConstructWaterPumpAtPosition - everything downstream of "we have a
	// candidate world position, now find the real AFGWaterVolume and
	// drive the hologram" is identical between the two; only HOW the
	// candidate position is computed differs (reference pump + offset,
	// vs. a literal position). See ConstructWaterPumpNearReference's doc
	// comment in the header for the full sourcing
	// (IFGExtractableResourceInterface/AFGResourceExtractorHologram
	// reasoning) - not repeated here.
	inline void ConstructWaterPumpAtCandidatePosition(UWorld* World, AFGCharacterPlayer* Character, const FVector& CandidatePosition, const FString& RecipeClassPath, const FString& ContextLabel, TFunction<void(const FAIModOperationResult&)> OnComplete)
	{
	// AFGWaterVolume::EncompassesPoint (IInterface_PostProcessVolume) - a
	// real, public containment check, not a distance guess.
	//
	// EncompassesPoint() is a HARD requirement, not a best-effort
	// preference, because a best-effort NEAREST-volume fallback is a CRASH.
	// Falling back to the nearest volume by actor-location distance whenever
	// no volume's EncompassesPoint() matches accepts a literal on-land
	// candidate position (well outside any real water, e.g. standing on a
	// placed foundation near the shore), which still has SOME nearest ocean
	// volume. CanPlaceResourceExtractor() is a volume-level flag (true for
	// the whole ocean), not a check of this specific point, so it passes
	// too. The hologram is then driven at a synthetic hit that doesn't
	// correspond to real water geometry -
	// TrySnapToActor()/TrySnapToExtractableResource() fail to populate
	// mSnappedExtractableResource, but GetConstructDisqualifiers() does NOT
	// reliably flag this (same "disqualifier isn't a reliable gate for this
	// precondition" failure mode documented above ConstructBuildingAtPosition's
	// extractor refusal) - bCanConstruct comes out true, construction is
	// attempted, and AFGResourceExtractorHologram::ConfigureActor()'s
	// unconditional mSnappedExtractableResource assert takes the whole
	// game process down (Assertion failed: mSnappedExtractableResource,
	// FGResourceExtractorHologram.cpp:235).
	//
	// So a point outside every real water volume's
	// actual (possibly non-box) collision shape fails cleanly with
	// NO_WATER_VOLUME_FOUND before any hologram is ever touched, exactly
	// the same "refuse outright rather than gamble on disqualifiers"
	// posture already used for extractor recipes going through
	// world.placeBuilding. The nearest-by-distance volume is still
	// computed, but ONLY to name it in the error message (e.g. "did you
	// mean X's bounds") - never as an actual construction target.
	AFGWaterVolume* TargetVolume = nullptr;
	AFGWaterVolume* NearestVolumeForDiagnostics = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (TActorIterator<AFGWaterVolume> It(World); It; ++It)
	{
		AFGWaterVolume* Candidate = *It;
		if (!IsValid(Candidate)) { continue; }
		if (!TargetVolume && Candidate->EncompassesPoint(CandidatePosition))
		{
			TargetVolume = Candidate;
		}
		const float DistSq = FVector::DistSquared(Candidate->GetActorLocation(), CandidatePosition);
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			NearestVolumeForDiagnostics = Candidate;
		}
	}
	if (!TargetVolume)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_WATER_VOLUME_FOUND"),
			FString::Printf(TEXT("No AFGWaterVolume actually contains (%.0f, %.0f, %.0f) - see world.waterVolumes for real, discoverable water bodies.%s"),
				CandidatePosition.X, CandidatePosition.Y, CandidatePosition.Z,
				NearestVolumeForDiagnostics
					? *FString::Printf(TEXT(" Nearest volume by distance: '%s' (not used - EncompassesPoint()=false, this point is not really in that water body)."), *NearestVolumeForDiagnostics->GetPathName())
					: TEXT(""))));
		return;
	}

	if (!TargetVolume->CanPlaceResourceExtractor())
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("WATER_VOLUME_CANNOT_PLACE_EXTRACTOR"),
			FString::Printf(TEXT("'%s' reports CanPlaceResourceExtractor()=false"), *TargetVolume->GetPathName())));
		return;
	}

	UClass* ResolvedRecipeClass = LoadObject<UClass>(nullptr, RecipeClassPath.IsEmpty() ? TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_WaterPump.Recipe_WaterPump_C") : *RecipeClassPath);
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

	// Same real IFGExtractableResourceInterface contract
	// ConstructExtractorOnNode uses for a resource node - "Used by
	// holograms to get the correct location/rotation for snapping when
	// placed on this extractable resource" (own doc comments).
	const FVector PlacementLocation = TargetVolume->GetPlacementLocation(CandidatePosition);
	const FRotator PlacementRotation = TargetVolume->GetPlacementRotation(CandidatePosition);

	FHitResult SyntheticHit;
	SyntheticHit.Location = PlacementLocation;
	SyntheticHit.ImpactPoint = PlacementLocation;
	SyntheticHit.Normal = PlacementRotation.RotateVector(FVector::UpVector);
	SyntheticHit.ImpactNormal = SyntheticHit.Normal;
	SyntheticHit.HitObjectHandle = FActorInstanceHandle(TargetVolume);
	SyntheticHit.bBlockingHit = true;
	if (UPrimitiveComponent* VolumePrimitive = Cast<UPrimitiveComponent>(TargetVolume->GetRootComponent()))
	{
		SyntheticHit.Component = VolumePrimitive;
	}
	// Non-zero Distance - see ConstructExtractorOnNode's own comment: a
	// zero-distance synthetic hit fails UFGCDNeedsResourceNode for a node
	// target, so apply the same fix here for water.
	SyntheticHit.Distance = FVector::Dist(Character->GetActorLocation(), PlacementLocation);

	BuildGun->GetHitResult() = SyntheticHit;

	const FRotator DeterministicLook = (PlacementLocation - Character->GetActorLocation()).Rotation();
	if (AController* Controller = Character->GetController())
	{
		Controller->SetControlRotation(DeterministicLook);
	}

	Hologram->UpdateHologramPlacement(SyntheticHit);
	Hologram->TrySnapToActor(SyntheticHit);

	struct FPollState
	{
		TWeakObjectPtr<AFGHologram> Hologram;
		TWeakObjectPtr<AFGCharacterPlayer> Character;
		TWeakObjectPtr<AFGWaterVolume> TargetVolume;
		TWeakObjectPtr<UWorld> World;
		FString ContextLabel;
		FHitResult SyntheticHit;
		FRotator DeterministicLook;
		TFunction<void(const FAIModOperationResult&)> OnComplete;
		int32 AttemptsRemaining = 120;
		int32 AttemptsTaken = 0;
	};
	const TSharedRef<FPollState> PollState = MakeShared<FPollState>();
	PollState->Hologram = Hologram;
	PollState->Character = Character;
	PollState->TargetVolume = TargetVolume;
	PollState->World = World;
	PollState->ContextLabel = ContextLabel;
	PollState->SyntheticHit = SyntheticHit;
	PollState->DeterministicLook = DeterministicLook;
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
			UE_LOG(LogAIModAI, Warning, TEXT("ConstructWaterPumpAtCandidatePosition (deferred): hologram or world became invalid while polling (after %d tick(s)) - nothing built"), PollState->AttemptsTaken);
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
			// UFGCDEncroachingPlayer: "A player is in the
			// way!" is exactly as player-dependent as aim location for an
			// autonomous RPC build - the idle real character standing
			// somewhere near a remote build site blocked real placements
			// (worked around
			// with world.teleportPlayer). Same accepted risk
			// as ignoring aim location: the build may intersect the
			// player's capsule.
			const bool bIgnoredForPlayerIndependence = (DisqualifierClass == UFGCDInvalidAimLocation::StaticClass())
				|| (DisqualifierClass == UFGCDEncroachingPlayer::StaticClass())
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
			UE_LOG(LogAIModAI, Display, TEXT("ConstructWaterPumpAtCandidatePosition (deferred, resolved after %d real tick(s)): CanConstruct()=false, NOT constructing - context=%s disqualifiers=[%s]"),
				PollState->AttemptsTaken, *PollState->ContextLabel, *DisqualifierSummary);
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
			UE_LOG(LogAIModAI, Error, TEXT("ConstructWaterPumpAtCandidatePosition (deferred): lost the build state before constructing - aborting, nothing built"));
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("Lost the build state before constructing")));
			return;
		}

		const FVector ConstructLocation = PollHologram->GetActorLocation();
		PollBuildState->InternalConstructHologram(ConstructionID);

		// Confirmation via proximity, NOT volume occupancy - real,
		// flagged uncertainty (see this function's header doc comment):
		// AFGWaterVolume's CanBecomeOccupied()/IsOccupied() may not
		// behave like a solid node's exclusive occupancy at all (the
		// interface's own doc comment: "Return false for resources that
		// can hold many extractors" - plausible a whole lake is exactly
		// that kind of resource), so unlike ConstructExtractorOnNode this
		// does NOT gate success/failure on occupancy - only on finding a
		// real, newly-constructed buildable near the intended location,
		// same pattern ConstructBuildingAtPosition already uses.
		FString ConstructedBuildableId;
		if (BuildableSubsystem)
		{
			float BestMatchDistSq = TNumericLimits<float>::Max();
			AFGBuildable* BestMatch = nullptr;
			for (AFGBuildable* Candidate : BuildableSubsystem->GetAllBuildablesRef())
			{
				if (!IsValid(Candidate)) { continue; }
				const float DistSq = FVector::DistSquared(Candidate->GetActorLocation(), ConstructLocation);
				if (DistSq < BestMatchDistSq)
				{
					BestMatchDistSq = DistSq;
					BestMatch = Candidate;
				}
			}
			if (BestMatch && BestMatchDistSq < FMath::Square(200.0f))
			{
				ConstructedBuildableId = BestMatch->GetPathName();
			}
		}

		UE_LOG(LogAIModAI, Display, TEXT("ConstructWaterPumpAtCandidatePosition (deferred, resolved after %d real tick(s)): construction attempted via InternalConstructHologram - context=%s id=%s"),
			PollState->AttemptsTaken, *PollState->ContextLabel, *ConstructedBuildableId);

		if (IsValid(PollCharacter))
		{
			PollCharacter->UnequipBuildGun();
		}

		if (ConstructedBuildableId.IsEmpty())
		{
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("CONSTRUCTION_UNCONFIRMED"), TEXT("InternalConstructHologram was called but no new buildable was found near the intended location afterward")));
			return;
		}

		PollState->OnComplete(FAIModOperationResult::SuccessWithBuildableId(ConstructedBuildableId));
	};

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
	}

	inline AFGPortableMiner* FindPortableMinerById(UWorld* World, const FString& Id)
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

	// Server_SpawnPortableMiner is a protected UFUNCTION(Server, Reliable).
	// Calling it via FindFunction+ProcessEvent reflection executes with no
	// error and correct parameters, but no real AFGPortableMiner ever
	// appears (the _Implementation UFUNCTION doesn't exist, since
	// _Implementation methods for Server RPCs are NOT separately
	// reflected): AActor::ProcessEvent's own net-function
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

	inline FString PowerPoleTypeToString(EPowerPoleType Type)
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

	inline FString PowerConnectionTypeToString(EPowerConnectionType Type)
	{
		switch (Type)
		{
		case EPowerConnectionType::PCT_Default: return TEXT("Default");
		case EPowerConnectionType::PCT_PowerTower: return TEXT("PowerTower");
		case EPowerConnectionType::PCT_Any: return TEXT("Any");
		default: return TEXT("Unknown");
		}
	}

	// Shared by LogRecipeCatalogAsJson/LogBuildableCatalogAsJson. Amount is
	// FItemAmount's raw internal unit (ItemAmount.h) - for RF_LIQUID/RF_GAS
	// items this is thousandths of a cubic meter (same convention
	// UFGItemDescriptor::GetStackSizeConverted()'s own doc comment
	// describes for fluid stack sizes), NOT pre-converted here. Callers
	// should check the item's "form" field (world.itemCatalog) before
	// interpreting the number - deliberately not doing that conversion in
	// C++, per this project's toolkit-not-solver preference (the AI/Python
	// side should do unit interpretation, not have it baked in here).
	inline TArray<TSharedPtr<FJsonValue>> ItemAmountsToJsonArray(const TArray<FItemAmount>& Amounts)
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

	inline FString ResourceFormToString(EResourceForm Form)
	{
		switch (Form)
		{
		case EResourceForm::RF_SOLID: return TEXT("Solid");
		case EResourceForm::RF_LIQUID: return TEXT("Liquid");
		case EResourceForm::RF_GAS: return TEXT("Gas");
		default: return TEXT("Invalid");
		}
	}

	inline bool IsBuildingRecipe(const TArray<FItemAmount>& Products)
	{
		return Products.Num() > 0 && Products[0].ItemClass && Products[0].ItemClass->IsChildOf(UFGBuildingDescriptor::StaticClass());
	}

	inline FString ClearanceTypeToString(EClearanceType Type)
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
	inline FString EventToString(EEvents Event)
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

	inline TArray<TSharedPtr<FJsonValue>> RelevantEventsToJsonArray(const TArray<EEvents>& Events)
	{
		TArray<TSharedPtr<FJsonValue>> Result;
		for (const EEvents Event : Events)
		{
			Result.Add(MakeShared<FJsonValueString>(EventToString(Event)));
		}
		return Result;
	}

	inline TSharedRef<FJsonObject> VectorToJson(const FVector& V)
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
	inline TArray<TSharedPtr<FJsonValue>> ClearanceDataToJsonArray(const AFGBuildable* BuildableCDO)
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

	// Accepts a display name ("Christmas"/"Anniversary"/"CSSBirthday"/
	// "FirstOfApril"/"None"), the enum token ("EV_Christmas"), or a
	// numeric index (0=None,1=Christmas,2=Anniversary,3=CSSBirthday,
	// 4=FirstOfApril). Returns true on a recognized value.
	inline bool StringToEvent(const FString& In, EEvents& OutEvent)
	{
		const FString S = In.TrimStartAndEnd();
		if (S.IsNumeric())
		{
			const int32 Idx = FCString::Atoi(*S);
			if (Idx >= 0 && Idx < static_cast<int32>(EEvents::EV_MAX))
			{
				OutEvent = static_cast<EEvents>(Idx);
				return true;
			}
			return false;
		}
		const FString L = S.ToLower().Replace(TEXT("ev_"), TEXT(""));
		if (L == TEXT("none")) { OutEvent = EEvents::EV_None; return true; }
		if (L == TEXT("christmas")) { OutEvent = EEvents::EV_Christmas; return true; }
		if (L == TEXT("anniversary") || L == TEXT("birthday")) { OutEvent = EEvents::EV_Birthday; return true; }
		if (L == TEXT("cssbirthday")) { OutEvent = EEvents::EV_CSSBirthday; return true; }
		if (L == TEXT("firstofapril") || L == TEXT("aprilfools")) { OutEvent = EEvents::EV_FirstOfApril; return true; }
		return false;
	}

// "RealCharacter" instigator strategy - see forward declaration's doc
// comment near the top of this file. Drives the REAL player's BuildGun.
inline void ConstructConveyorBelt_RealCharacterStrategy(UObject* WorldContextObject, const FString& SourceBuildableId, const FString& DestBuildableId, const FString& RecipeClassPath, const FString& RouteMode, const TOptional<FVector>& SourceConnectorPosition, const TOptional<FVector>& DestConnectorPosition, bool bDryRun, TFunction<void(const FAIModOperationResult&)> OnComplete)
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

	// Position-targeted selection when the caller supplied
	// one - see FindFreeFactoryConnectionNear's own comment for why this
	// exists. Falls back to "first free of this direction"
	// when no position is given, so a caller that only cares "connect these
	// two buildables" (manifold-building scripts) still works.
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
		// See PopulateSyntheticTraceRay's
		// doc comment (load-bearing for the lift's height; suspected fix for
		// the belt player-distance "too long" failures).
		PopulateSyntheticTraceRay(Hit);
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

	// Do NOT raise the build gun's mBuildDistanceMax here to fix far belts.
	// It does NOT fix them (the real limit is the camera AIM, not the clamp
	// - see the auto-teleport below) and it REGRESSES near belts: with an
	// effectively-unlimited trace range, the belt's AutoRouteSpline traces
	// far past the target along the aim and routes an absurdly long spline,
	// so even a 500-unit level belt fails "too long"/"Missing materials"
	// unless the player stands almost exactly on it. The RestoreBuildGunTraceRange
	// helper is kept only so the restore calls below stay valid no-ops
	// (SavedBuildGunRange = -1 makes every RestoreBuildGunTraceRange a no-op).
	const float SavedBuildGunRange = -1.0f;

	// Auto-teleport the real player next to the connection - the reliable
	// fix for the belt "too long" failures at distance.
	// The RealCharacter strategy fundamentally relies on the real build
	// gun's real camera trace, which cannot reach the connectors when the
	// player stands far away: from ~14k units a connector-sized target is
	// on no camera ray, and neither raising mBuildDistanceMax nor pinning
	// GetHitResult() fixes it. What works is standing the player next to
	// the work. So do that
	// automatically: teleport to the connection midpoint (a bit above it),
	// build, and restore the player's real position at every exit so an
	// interactive player is left where they were. Only moves the player
	// when they are actually far (>2000 units), so a near player - and the
	// interactive case - is untouched.
	const FVector ConnMidpoint = (SourceConnection->GetConnectorLocation() + DestConnection->GetConnectorLocation()) * 0.5;
	const FVector SavedPlayerLocation = Character->GetActorLocation();
	const bool bTeleportedPlayer = FVector::Dist(SavedPlayerLocation, ConnMidpoint) > 2000.0;
	if (bTeleportedPlayer)
	{
		Character->TeleportTo(ConnMidpoint + FVector(0.0, 0.0, 500.0), Character->GetActorRotation(), false, true);
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			Movement->StopMovementImmediately(); // don't carry fall velocity into the work site
		}
	}

	// Build-gun cached-trace injection for belts - the fix
	// for the player-distance-dependent "Conveyor Belt is too steep!"/
	// "too long!" failures (see docs/hmf-factory-plan.md finding #3).
	// PopulateSyntheticTraceRay fixes pure horizontal "too long" by giving
	// the hit a real camera ray, but INCLINED belts still fail based only on
	// how far the real player stands: AFGBuildGun::TraceForBuilding()
	// clamps its trace to mBuildDistanceMax, so when the player is far,
	// any internal read of BuildGun->GetHitResult() (which the belt's
	// spline routing/incline validation can consult, not just the
	// hitResult we pass to UpdateHologramPlacement) returns a point
	// short of the real destination along the aim - producing a spline
	// whose middle over-inclines. The conveyor LIFT path already writes
	// its synthetic hit into GetHitResult() for exactly this reason.
	// Writing the real target here (and re-asserting it
	// every poll tick below) makes any such internal read see the true
	// destination regardless of player distance. Not yet verified to remove
	// the distance dependence alone - keep teleporting near belt connections.
	const FHitResult StartHit = MakeHitAt(SourceBuildable, SourceConnection);
	BuildGun->GetHitResult() = StartHit;
	BeltHologram->UpdateHologramPlacement(StartHit);
	BeltHologram->TrySnapToActor(StartHit);
	const bool bStartStepComplete = BeltHologram->DoMultiStepPlacement(true);
	const ESplineHologramBuildStep StepAfterStart = BeltHologram->GetCurrentBuildStep();

	UE_LOG(LogAIModAI, Display, TEXT("ConstructConveyorBelt: source=%s dest=%s after start click: stepComplete=%s step=%d disqualifiers=[%s]"),
		*SourceBuildableId, *DestBuildableId, bStartStepComplete ? TEXT("true") : TEXT("false"), static_cast<int32>(StepAfterStart), *SummarizeDisqualifiers(BeltHologram));

	if (bStartStepComplete)
	{
		RestoreBuildGunTraceRange(BuildGun, SavedBuildGunRange);
		if (bTeleportedPlayer) { Character->TeleportTo(SavedPlayerLocation, Character->GetActorRotation(), false, true); }
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("UNEXPECTED_STEP_COMPLETE"), TEXT("DoMultiStepPlacement() reported complete after only the start click")));
		return;
	}

	const FHitResult EndHit = MakeHitAt(DestBuildable, DestConnection);
	BuildGun->GetHitResult() = EndHit; // see the StartHit injection comment above
	BeltHologram->UpdateHologramPlacement(EndHit);
	BeltHologram->TrySnapToActor(EndHit);
	const bool bEndStepComplete = BeltHologram->DoMultiStepPlacement(true);
	const ESplineHologramBuildStep StepAfterEnd = BeltHologram->GetCurrentBuildStep();
	const TArray<AFGBuildable*> ConnectedBuildables = BeltHologram->GetAnyConnectedBuildables();

	UE_LOG(LogAIModAI, Display, TEXT("ConstructConveyorBelt: source=%s dest=%s after end click: stepComplete=%s step=%d connectedCount=%d disqualifiers=[%s]"),
		*SourceBuildableId, *DestBuildableId, bEndStepComplete ? TEXT("true") : TEXT("false"), static_cast<int32>(StepAfterEnd), ConnectedBuildables.Num(), *SummarizeDisqualifiers(BeltHologram));

	if (!bEndStepComplete)
	{
		RestoreBuildGunTraceRange(BuildGun, SavedBuildGunRange);
		if (bTeleportedPlayer) { Character->TeleportTo(SavedPlayerLocation, Character->GetActorRotation(), false, true); }
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("PLACEMENT_INCOMPLETE"),
			FString::Printf(TEXT("DoMultiStepPlacement() did not report complete after the end click - step=%d connectedCount=%d, may need a third step"), static_cast<int32>(StepAfterEnd), ConnectedBuildables.Num())));
		return;
	}

	struct FPollState
	{
		TWeakObjectPtr<AFGConveyorBeltHologram> Hologram;
		TWeakObjectPtr<AFGCharacterPlayer> Character;
		TWeakObjectPtr<AFGBuildGun> BuildGun; // hit-result re-assertion, see below
		TWeakObjectPtr<UWorld> World;
		FString SourceBuildableId;
		FString DestBuildableId;
		bool bDryRun = true;
		FRotator DeterministicLook;
		FHitResult EndHit; // re-asserted every poll tick, see below
		float SavedBuildGunRange = -1.0f; // restored at every poll terminal
		FVector SavedPlayerLocation = FVector::ZeroVector; // restored at every terminal
		bool bTeleportedPlayer = false;
		int32 AttemptsRemaining = 120; // safety cap - real ticks, not a fixed duration
		int32 AttemptsTaken = 0;
		TFunction<void(const FAIModOperationResult&)> OnComplete;
	};
	const TSharedRef<FPollState> PollState = MakeShared<FPollState>();
	PollState->Hologram = BeltHologram;
	PollState->Character = Character;
	PollState->BuildGun = BuildGun;
	PollState->World = World;
	PollState->SourceBuildableId = SourceBuildableId;
	PollState->DestBuildableId = DestBuildableId;
	PollState->bDryRun = bDryRun;
	PollState->DeterministicLook = BeltDeterministicLook;
	PollState->EndHit = EndHit;
	PollState->SavedBuildGunRange = SavedBuildGunRange;
	PollState->SavedPlayerLocation = SavedPlayerLocation;
	PollState->bTeleportedPlayer = bTeleportedPlayer;
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
			RestoreBuildGunTraceRange(PollState->BuildGun.Get(), PollState->SavedBuildGunRange);
			if (PollState->bTeleportedPlayer && IsValid(PollCharacter)) { PollCharacter->TeleportTo(PollState->SavedPlayerLocation, PollCharacter->GetActorRotation(), false, true); }
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

		// Re-assert the build gun's cached trace every tick -
		// see the StartHit injection comment in the synchronous section
		// above. Keeps any internal spline/incline read pinned to the real
		// destination instead of the player's distance-clamped live trace.
		if (AFGBuildGun* PollBuildGunForHit = PollState->BuildGun.Get())
		{
			PollBuildGunForHit->GetHitResult() = PollState->EndHit;
		}

		// Re-assert the end hit every poll tick - matching
		// the fix for point holograms in
		// ConstructBuildingNearPlayer/ConstructExtractorOnTargetedNode
		// (see docs/buildgun-driven-placement-research.md's "§3
		// correction"). UFGBuildGunStateBuild::TickState_Implementation
		// runs its own real AFGBuildGun::TraceForBuilding() every tick
		// from the REAL player's live camera aim and silently overwrites
		// whatever hit/placement state this function set up - seen there as
		// a ~4000-unit drift and a "Surface is too uneven!"
		// failure at a location nowhere near the intended one. This poll
		// loop already re-asserts rotation every tick for the same
		// reason; the analogous placement re-assert here is the likely
		// explanation for intermittent "Conveyor Belt is too long!"/"Surface
		// is too uneven!" failures that distance, AFK state, or leftover
		// geometry alone don't explain.
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
			// UFGCDEncroachingPlayer: "A player is in the
			// way!" is exactly as player-dependent as aim location for an
			// autonomous RPC build - the idle real character standing
			// somewhere near a remote build site blocked real placements
			// (worked around
			// with world.teleportPlayer). Same accepted risk
			// as ignoring aim location: the build may intersect the
			// player's capsule.
			const bool bIgnoredForPlayerIndependence = (DisqualifierClass == UFGCDInvalidAimLocation::StaticClass())
				|| (DisqualifierClass == UFGCDEncroachingPlayer::StaticClass())
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
			RestoreBuildGunTraceRange(PollState->BuildGun.Get(), PollState->SavedBuildGunRange);
			if (PollState->bTeleportedPlayer && IsValid(PollCharacter)) { PollCharacter->TeleportTo(PollState->SavedPlayerLocation, PollCharacter->GetActorRotation(), false, true); }
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("CANNOT_CONSTRUCT"), DisqualifierSummary));
			return;
		}

		if (PollState->bDryRun)
		{
			RestoreBuildGunTraceRange(PollState->BuildGun.Get(), PollState->SavedBuildGunRange);
			if (PollState->bTeleportedPlayer && IsValid(PollCharacter)) { PollCharacter->TeleportTo(PollState->SavedPlayerLocation, PollCharacter->GetActorRotation(), false, true); }
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
			RestoreBuildGunTraceRange(PollState->BuildGun.Get(), PollState->SavedBuildGunRange);
			if (PollState->bTeleportedPlayer && IsValid(PollCharacter)) { PollCharacter->TeleportTo(PollState->SavedPlayerLocation, PollCharacter->GetActorRotation(), false, true); }
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("Lost the build state before constructing")));
			return;
		}

		PollBuildState->InternalConstructHologram(ConstructionID);

		UE_LOG(LogAIModAI, Display, TEXT("ConstructConveyorBelt (deferred, resolved after %d real tick(s)): construction attempted via InternalConstructHologram - source=%s dest=%s"),
			PollState->AttemptsTaken, *PollState->SourceBuildableId, *PollState->DestBuildableId);

		RestoreBuildGunTraceRange(PollState->BuildGun.Get(), PollState->SavedBuildGunRange);
		// Restore the player's real position AFTER construction (the build
		// needed them near; now put them back where they were).
		if (PollState->bTeleportedPlayer && IsValid(PollCharacter)) { PollCharacter->TeleportTo(PollState->SavedPlayerLocation, PollCharacter->GetActorRotation(), false, true); }
		if (IsValid(PollCharacter))
		{
			PollCharacter->UnequipBuildGun();
		}

		PollState->OnComplete(FAIModOperationResult::Success());
	};

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
}

inline FString SerializeHologramSplineJson(AFGSplineHologram* Hologram)
{
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetBoolField(TEXT("predicted"), true);
	TArray<TSharedPtr<FJsonValue>> Points;
	USplineComponent* Spline = IsValid(Hologram) ? Hologram->FindComponentByClass<USplineComponent>() : nullptr;
	if (Spline)
	{
		Root->SetNumberField(TEXT("splineLength"), Spline->GetSplineLength());
		const int32 NumPoints = Spline->GetNumberOfSplinePoints();
		Points.Reserve(NumPoints);
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
			Points.Add(MakeShared<FJsonValueObject>(PointObject));
		}
	}
	Root->SetArrayField(TEXT("points"), Points);
	FString JsonString;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonString);
	FJsonSerializer::Serialize(Root, Writer);
	return JsonString;
}

	// Beam build-mode fields (mBuildModeDiagonal/mBuildModeFreeForm) are
	// protected TSubclassOf<UFGHologramBuildModeDescriptor> with no public
	// getter - reflected as FObjectPropertyBase (TSubclassOf boxes to a
	// UClass* value), same reflection posture as every other
	// no-public-getter field this project reads (mMaxIncline,
	// mPotentialShardSlots, etc), just the first FObjectPropertyBase
	// instance rather than FFloatProperty/FIntProperty/FBoolProperty.
	inline TSubclassOf<UFGHologramBuildModeDescriptor> ReadBeamBuildModeProperty(const AFGBeamHologram* Hologram, const TCHAR* PropertyName)
	{
		if (!Hologram) { return nullptr; }
		const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Hologram->GetClass(), PropertyName);
		if (!Property) { return nullptr; }
		return TSubclassOf<UFGHologramBuildModeDescriptor>(Cast<UClass>(Property->GetObjectPropertyValue_InContainer(Hologram)));
	}

	// Shared by ConstructStackableSupport and ConstructStackableSupportOnTop
	// - everything downstream of "we have a candidate world position" is
	// identical between the two; only how the candidate is computed
	// differs (literal X/Y/Z-or-ground-trace, vs. a reference stackable's
	// real position + its own GetStackHeight()). See
	// ConstructStackableSupport's doc comment in the header for the full
	// Zoop/SetZoopAmount sourcing - not repeated here.
	inline void ConstructStackableSupportAtCandidatePosition(UWorld* World, AFGCharacterPlayer* Character, const FVector& CandidatePosition, const FString& RecipeClassPath, int32 StackCount, const FString& ContextLabel, TFunction<void(const FAIModOperationResult&)> OnComplete)
	{
	UClass* ResolvedClass = LoadObject<UClass>(nullptr, *RecipeClassPath);
	if (!ResolvedClass || !ResolvedClass->IsChildOf(UFGRecipe::StaticClass()))
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("INVALID_RECIPE"), FString::Printf(TEXT("'%s' did not resolve to a UFGRecipe subclass"), *RecipeClassPath)));
		return;
	}
	const TSubclassOf<UFGRecipe> RecipeClass = ResolvedClass;

	FHitResult SyntheticHit;
	SyntheticHit.Location = CandidatePosition;
	SyntheticHit.ImpactPoint = SyntheticHit.Location;
	SyntheticHit.Normal = FVector::UpVector;
	SyntheticHit.ImpactNormal = FVector::UpVector;
	SyntheticHit.bBlockingHit = true;

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

	AFGStackablePoleHologram* Hologram = Cast<AFGStackablePoleHologram>(BuildState->GetHologram());
	if (!Hologram)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("HOLOGRAM_SPAWN_FAILED"),
			FString::Printf(TEXT("HotKeyRecipe(%s) did not result in an AFGStackablePoleHologram (got %s)"),
				*RecipeClassPath, BuildState->GetHologram() ? *BuildState->GetHologram()->GetClass()->GetName() : TEXT("null"))));
		return;
	}

	auto SummarizeDisqualifiers = [](AFGStackablePoleHologram* H) -> FString
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

	const FRotator DeterministicLook = (SyntheticHit.Location - Character->GetActorLocation()).Rotation();
	if (AController* Controller = Character->GetController())
	{
		Controller->SetControlRotation(DeterministicLook);
	}

	// First click: placement/rotation - transitions
	// EBuildableHologramBuildStep::BHBS_PlacementAndRotation -> BHBS_Zoop
	// (FGBuildableHologram.h), same two-step shape ConstructBeam already
	// drives successfully.
	Hologram->UpdateHologramPlacement(SyntheticHit);
	Hologram->TrySnapToActor(SyntheticHit);
	const bool bStartStepComplete = Hologram->DoMultiStepPlacement(true);

	UE_LOG(LogAIModAI, Display, TEXT("ConstructStackableSupportAtCandidatePosition: recipe=%s context=%s stackCount=%d after start click: stepComplete=%s disqualifiers=[%s]"),
		*RecipeClassPath, *ContextLabel, StackCount, bStartStepComplete ? TEXT("true") : TEXT("false"), *SummarizeDisqualifiers(Hologram));

	if (bStartStepComplete)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("UNEXPECTED_STEP_COMPLETE"), TEXT("DoMultiStepPlacement() reported complete after only the start click")));
		return;
	}

	// StackCount additional instances stacked in the hologram's local
	// "Up" direction - see this function's header doc comment for why
	// the FIntVector's Z component is used (inferred from
	// EHologramZoopDirections' HZD_Up/HZD_Down naming, not independently
	// confirmed) and the real SetZoopFromHitresult-may-overwrite-it risk
	// this is reasserted against every poll tick below.
	const FIntVector DesiredZoop(0, 0, FMath::Max(0, StackCount));
	Hologram->SetZoopAmount(DesiredZoop);

	const bool bEndStepComplete = Hologram->DoMultiStepPlacement(true);

	UE_LOG(LogAIModAI, Display, TEXT("ConstructStackableSupportAtCandidatePosition: recipe=%s context=%s stackCount=%d after zoop click: stepComplete=%s disqualifiers=[%s]"),
		*RecipeClassPath, *ContextLabel, StackCount, bEndStepComplete ? TEXT("true") : TEXT("false"), *SummarizeDisqualifiers(Hologram));

	if (!bEndStepComplete)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("PLACEMENT_INCOMPLETE"), TEXT("DoMultiStepPlacement() did not report complete after the zoop click")));
		return;
	}

	struct FPollState
	{
		TWeakObjectPtr<AFGStackablePoleHologram> Hologram;
		TWeakObjectPtr<AFGCharacterPlayer> Character;
		TWeakObjectPtr<UWorld> World;
		FHitResult SyntheticHit;
		FIntVector DesiredZoop;
		FRotator DeterministicLook;
		TSubclassOf<AFGBuildable> ExpectedBuildableClass;
		FVector TargetPosition;
		int32 RequestedCount = 1;
		int32 AttemptsRemaining = 120;
		int32 AttemptsTaken = 0;
		TFunction<void(const FAIModOperationResult&)> OnComplete;
	};
	const TSharedRef<FPollState> PollState = MakeShared<FPollState>();
	PollState->Hologram = Hologram;
	PollState->Character = Character;
	PollState->World = World;
	PollState->SyntheticHit = SyntheticHit;
	PollState->DesiredZoop = DesiredZoop;
	PollState->DeterministicLook = DeterministicLook;
	PollState->ExpectedBuildableClass = ResolveBuildableClassForRecipe(RecipeClassPath);
	PollState->TargetPosition = SyntheticHit.Location;
	PollState->RequestedCount = FMath::Max(0, StackCount) + 1;
	PollState->OnComplete = MoveTemp(OnComplete);

	const TSharedRef<TFunction<void()>> PollFn = MakeShared<TFunction<void()>>();
	*PollFn = [PollState, PollFn]()
	{
		++PollState->AttemptsTaken;

		AFGStackablePoleHologram* PollHologram = PollState->Hologram.Get();
		UWorld* PollWorld = PollState->World.Get();
		AFGCharacterPlayer* PollCharacter = PollState->Character.Get();
		if (!IsValid(PollHologram) || !PollWorld)
		{
			UE_LOG(LogAIModAI, Warning, TEXT("ConstructStackableSupportAtCandidatePosition (deferred): hologram or world became invalid while polling (after %d tick(s))"), PollState->AttemptsTaken);
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
		// Reassert every tick - see this function's header doc comment on
		// why UpdateHologramPlacement() may internally re-derive (and
		// overwrite) the zoop amount from the hit result while in the
		// Zoop build step.
		PollHologram->SetZoopAmount(PollState->DesiredZoop);

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
			// UFGCDEncroachingPlayer: "A player is in the
			// way!" is exactly as player-dependent as aim location for an
			// autonomous RPC build - the idle real character standing
			// somewhere near a remote build site blocked real placements
			// (worked around
			// with world.teleportPlayer). Same accepted risk
			// as ignoring aim location: the build may intersect the
			// player's capsule.
			const bool bIgnoredForPlayerIndependence = (DisqualifierClass == UFGCDInvalidAimLocation::StaticClass())
				|| (DisqualifierClass == UFGCDEncroachingPlayer::StaticClass())
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
			UE_LOG(LogAIModAI, Error, TEXT("ConstructStackableSupportAtCandidatePosition (deferred): lost the build state before constructing - aborting, nothing built"));
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("Lost the build state before constructing")));
			return;
		}

		PollBuildState->InternalConstructHologram(ConstructionID);

		// A Zoop placement can legitimately construct MULTIPLE separate
		// buildable actors in one call - unlike every other Construct*
		// function here, a single ResultBuildableId can't represent the
		// whole stack. Count real buildables of the resolved class found
		// near the target X/Y (any Z - they're stacked vertically) and
		// report foundCount/requestedCount in ResultDetailJson rather
		// than silently only acknowledging one.
		TArray<FString> FoundBuildableIds;
		if (BuildableSubsystem && PollState->ExpectedBuildableClass)
		{
			for (AFGBuildable* Candidate : BuildableSubsystem->GetAllBuildablesRef())
			{
				if (!IsValid(Candidate) || !Candidate->IsA(PollState->ExpectedBuildableClass)) { continue; }
				const FVector CandidateLocation = Candidate->GetActorLocation();
				const float HorizontalDistSq = FVector::DistSquared2D(CandidateLocation, PollState->TargetPosition);
				if (HorizontalDistSq < FMath::Square(200.0f))
				{
					FoundBuildableIds.Add(Candidate->GetPathName());
				}
			}
		}

		UE_LOG(LogAIModAI, Display, TEXT("ConstructStackableSupportAtCandidatePosition (deferred, resolved after %d real tick(s)): construction attempted via InternalConstructHologram - found=%d requested=%d"),
			PollState->AttemptsTaken, FoundBuildableIds.Num(), PollState->RequestedCount);

		if (IsValid(PollCharacter))
		{
			PollCharacter->UnequipBuildGun();
		}

		if (FoundBuildableIds.IsEmpty())
		{
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("CONSTRUCTION_UNCONFIRMED"), TEXT("InternalConstructHologram was called but no new buildable was found near the intended location afterward")));
			return;
		}

		const TSharedRef<FJsonObject> DetailObject = MakeShared<FJsonObject>();
		DetailObject->SetNumberField(TEXT("requestedCount"), PollState->RequestedCount);
		DetailObject->SetNumberField(TEXT("foundCount"), FoundBuildableIds.Num());
		TArray<TSharedPtr<FJsonValue>> IdsJsonArray;
		for (const FString& Id : FoundBuildableIds)
		{
			IdsJsonArray.Add(MakeShared<FJsonValueString>(Id));
		}
		DetailObject->SetArrayField(TEXT("buildableIds"), IdsJsonArray);

		FAIModOperationResult Result = FAIModOperationResult::SuccessWithBuildableId(FoundBuildableIds[0]);
		Result.ResultDetailJson = WriteCondensedJson(DetailObject);
		PollState->OnComplete(Result);
	};

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
	}

	inline FString SchematicTypeToString(ESchematicType Type)
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

	inline FString TechTierStateToString(ETechTierState State)
	{
		switch (State)
		{
		case ETechTierState::ETTS_Locked: return TEXT("Locked");
		case ETechTierState::ETTS_Available: return TEXT("Available");
		case ETechTierState::ETTS_FullyPurchased: return TEXT("FullyPurchased");
		default: return TEXT("Unknown");
		}
	}

	inline FString SerializeJsonObject(const TSharedRef<FJsonObject>& Object)
	{
		FString OutString;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutString);
		FJsonSerializer::Serialize(Object, Writer);
		return OutString;
	}

	inline FString SchematicStateToString(ESchematicState State)
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

	inline FString ResearchTreeStatusToString(EResearchTreeStatus Status)
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
	inline TArray<FResearchTime> CollectOngoingResearch(AFGResearchManager* Manager)
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
	inline UFGHardDrive* FindUnclaimedHardDriveOfferingSchematic(AFGResearchManager* Manager, const TSubclassOf<UFGSchematic>& RewardSchematic)
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

	inline FString TrainStatusToString(ETrainStatus Status)
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

	inline FString SelfDrivingErrorToString(ESelfDrivingLocomotiveError Error)
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

	// Road-vehicle (truck) autopilot error status - EVehicleAutopilotErrorStatus
	// from FGWheeledVehicleIdentifier.h. Distinct enum from the train's
	// ESelfDrivingLocomotiveError above.
	inline FString VehicleAutopilotErrorToString(EVehicleAutopilotErrorStatus Status)
	{
		switch (Status)
		{
		case EVehicleAutopilotErrorStatus::None: return TEXT("None");
		case EVehicleAutopilotErrorStatus::StationUnreachable: return TEXT("StationUnreachable");
		case EVehicleAutopilotErrorStatus::NotOnPath: return TEXT("NotOnPath");
		case EVehicleAutopilotErrorStatus::TooFewStations: return TEXT("TooFewStations");
		case EVehicleAutopilotErrorStatus::NoFuel: return TEXT("NoFuel");
		case EVehicleAutopilotErrorStatus::Deadlocked: return TEXT("Deadlocked");
		default: return TEXT("Unknown");
		}
	}

	inline FString TrainDockingStateToString(ETrainDockingState State)
	{
		switch (State)
		{
		case ETrainDockingState::TDS_None: return TEXT("None");
		case ETrainDockingState::TDS_ReadyToDock: return TEXT("ReadyToDock");
		case ETrainDockingState::TDS_Docked: return TEXT("Docked");
		default: return TEXT("Unknown");
		}
	}

	inline FString DroneStatusToString(EDroneStatus Status)
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
