// Copyright Epic Games, Inc. All Rights Reserved.

#include "AIModFunctionLibraryInternal.h"
using namespace AIModInternal;


FString UAIModFunctionLibrary::GetInterfaceVersion()
{
	UE_LOG(LogAIModAI, Verbose, TEXT("GetInterfaceVersion called"));
	return TEXT("0.1.0");
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



FString UAIModFunctionLibrary::LogDamageVolumesAsJson(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		UE_LOG(LogAIModAI, Warning, TEXT("LogDamageVolumesAsJson: no valid world context"));
		return TEXT("{}");
	}

	TArray<TSharedPtr<FJsonValue>> VolumesJsonArray;
	for (TActorIterator<AFGDamageOverTimeVolume> It(World); It; ++It)
	{
		AFGDamageOverTimeVolume* Volume = *It;
		if (!IsValid(Volume))
		{
			continue;
		}

		const FBox Bounds = Volume->GetComponentsBoundingBox(false);
		const TSharedRef<FJsonObject> BoundsObject = MakeShared<FJsonObject>();
		BoundsObject->SetObjectField(TEXT("min"), MakeVectorJson(Bounds.Min));
		BoundsObject->SetObjectField(TEXT("max"), MakeVectorJson(Bounds.Max));
		BoundsObject->SetObjectField(TEXT("size"), MakeVectorJson(Bounds.GetSize()));

		const UFGDotComponent* DotComponent = Volume->FindComponentByClass<UFGDotComponent>();
		const TSubclassOf<UFGDamageOverTime> DotClass = GetDotClassOfComponent(DotComponent);

		TArray<TSharedPtr<FJsonValue>> DamageTypesJsonArray;
		for (const UFGDamageType* DamageType : UFGDamageOverTime::GetDamageTypes(DotClass))
		{
			if (!DamageType)
			{
				continue;
			}
			const TSharedRef<FJsonObject> DamageTypeObject = MakeShared<FJsonObject>();
			DamageTypeObject->SetStringField(TEXT("class"), DamageType->GetClass()->GetPathName());
			DamageTypeObject->SetNumberField(TEXT("damageAmount"), DamageType->mDamageAmount);
			DamageTypeObject->SetBoolField(TEXT("destroysVehicles"), DamageType->mDestroyVehicles);
			DamageTypeObject->SetBoolField(TEXT("playerAlwaysVulnerable"), DamageType->mPlayerIsAlwaysVulnerable);
			DamageTypesJsonArray.Add(MakeShared<FJsonValueObject>(DamageTypeObject));
		}

		const TSharedRef<FJsonObject> VolumeObject = MakeShared<FJsonObject>();
		VolumeObject->SetStringField(TEXT("id"), Volume->GetPathName());
		VolumeObject->SetObjectField(TEXT("position"), MakeVectorJson(Volume->GetActorLocation()));
		VolumeObject->SetObjectField(TEXT("bounds"), BoundsObject);
		VolumeObject->SetStringField(TEXT("dotClass"), DotClass ? DotClass->GetPathName() : FString());
		// -1 when the dot class is unset, per the accessor's own contract.
		VolumeObject->SetNumberField(TEXT("damageInterval"), UFGDamageOverTime::GetDamageInterval(DotClass));
		VolumeObject->SetArrayField(TEXT("damageTypes"), DamageTypesJsonArray);
		VolumeObject->SetBoolField(TEXT("dotActive"), DotComponent ? DotComponent->IsActive() : false);
		VolumeObject->SetBoolField(TEXT("collisionEnabled"), Volume->GetActorEnableCollision());
		VolumesJsonArray.Add(MakeShared<FJsonValueObject>(VolumeObject));
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetArrayField(TEXT("damageVolumes"), VolumesJsonArray);

	// The other two boundary mechanisms, for one-call context: the void
	// death plane and the minimap's 2D extent (informational - the map
	// extent is not the damage line).
	if (const AWorldSettings* WorldSettings = World->GetWorldSettings())
	{
		RootObject->SetNumberField(TEXT("killZ"), WorldSettings->KillZ);
	}
	FVector2D WorldBoundsMin = FVector2D::ZeroVector;
	FVector2D WorldBoundsMax = FVector2D::ZeroVector;
	UFGMapFunctionLibrary::GetWorldBounds(World, WorldBoundsMin, WorldBoundsMax);
	const TSharedRef<FJsonObject> WorldBoundsObject = MakeShared<FJsonObject>();
	const TSharedRef<FJsonObject> WorldBoundsMinObject = MakeShared<FJsonObject>();
	WorldBoundsMinObject->SetNumberField(TEXT("x"), WorldBoundsMin.X);
	WorldBoundsMinObject->SetNumberField(TEXT("y"), WorldBoundsMin.Y);
	const TSharedRef<FJsonObject> WorldBoundsMaxObject = MakeShared<FJsonObject>();
	WorldBoundsMaxObject->SetNumberField(TEXT("x"), WorldBoundsMax.X);
	WorldBoundsMaxObject->SetNumberField(TEXT("y"), WorldBoundsMax.Y);
	WorldBoundsObject->SetObjectField(TEXT("min"), WorldBoundsMinObject);
	WorldBoundsObject->SetObjectField(TEXT("max"), WorldBoundsMaxObject);
	RootObject->SetObjectField(TEXT("worldBounds2D"), WorldBoundsObject);

	const FString JsonString = WriteCondensedJson(RootObject);

	UE_LOG(LogAIModAI, Display, TEXT("LogDamageVolumesAsJson: %d damage volume(s)"), VolumesJsonArray.Num());

	return JsonString;
}


FString UAIModFunctionLibrary::ProbeHazardAsJson(UObject* WorldContextObject, float X, float Y, float Z)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		UE_LOG(LogAIModAI, Warning, TEXT("ProbeHazardAsJson: no valid world context"));
		return TEXT("{}");
	}

	const FVector Point(X, Y, Z);
	TArray<TSharedPtr<FJsonValue>> ContainingJsonArray;
	FString NearestVolumeId;
	double NearestDistance = -1.0;
	for (TActorIterator<AFGDamageOverTimeVolume> It(World); It; ++It)
	{
		AFGDamageOverTimeVolume* Volume = *It;
		if (!IsValid(Volume))
		{
			continue;
		}
		float DistanceToPoint = 0.f;
		if (Volume->EncompassesPoint(Point, 0.f, &DistanceToPoint))
		{
			ContainingJsonArray.Add(MakeShared<FJsonValueString>(Volume->GetPathName()));
		}
		else if (NearestDistance < 0.0 || DistanceToPoint < NearestDistance)
		{
			NearestDistance = DistanceToPoint;
			NearestVolumeId = Volume->GetPathName();
		}
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetObjectField(TEXT("point"), MakeVectorJson(Point));
	RootObject->SetBoolField(TEXT("insideDamageVolume"), ContainingJsonArray.Num() > 0);
	RootObject->SetArrayField(TEXT("containingVolumeIds"), ContainingJsonArray);
	if (NearestDistance >= 0.0)
	{
		RootObject->SetStringField(TEXT("nearestOtherVolumeId"), NearestVolumeId);
		RootObject->SetNumberField(TEXT("nearestOtherVolumeDistance"), NearestDistance);
	}
	if (const AWorldSettings* WorldSettings = World->GetWorldSettings())
	{
		RootObject->SetNumberField(TEXT("killZ"), WorldSettings->KillZ);
		RootObject->SetBoolField(TEXT("belowKillZ"), Point.Z < WorldSettings->KillZ);
	}
	FVector2D WorldBoundsMin = FVector2D::ZeroVector;
	FVector2D WorldBoundsMax = FVector2D::ZeroVector;
	UFGMapFunctionLibrary::GetWorldBounds(World, WorldBoundsMin, WorldBoundsMax);
	RootObject->SetBoolField(TEXT("insideWorldBounds2D"),
		Point.X >= WorldBoundsMin.X && Point.X <= WorldBoundsMax.X &&
		Point.Y >= WorldBoundsMin.Y && Point.Y <= WorldBoundsMax.Y);

	return WriteCondensedJson(RootObject);
}


FAIModOperationResult UAIModFunctionLibrary::SetDamageVolumeEnabled(UObject* WorldContextObject, const FString& VolumeId, bool bEnabled)
{
	FAIModOperationResult Result;
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		Result.ErrorCode = TEXT("NO_WORLD");
		Result.ErrorMessage = TEXT("No valid world context");
		return Result;
	}

	AFGDamageOverTimeVolume* Volume = FindDamageVolumeById(World, VolumeId);
	if (!Volume)
	{
		Result.ErrorCode = TEXT("TARGET_NOT_FOUND");
		Result.ErrorMessage = FString::Printf(TEXT("No AFGDamageOverTimeVolume exists with id '%s' (ids come from world.damageVolumes)"), *VolumeId);
		return Result;
	}

	UFGDotComponent* DotComponent = Volume->FindComponentByClass<UFGDotComponent>();
	if (!bEnabled)
	{
		// Collision off FIRST: the end-overlap events this fires are what
		// unregister the DOT from actors currently standing inside the
		// volume, and the component must still be active to process them.
		Volume->SetActorEnableCollision(false);
		if (DotComponent)
		{
			DotComponent->Deactivate();
		}
	}
	else
	{
		if (DotComponent)
		{
			DotComponent->Activate(true);
		}
		Volume->SetActorEnableCollision(true);
	}

	UE_LOG(LogAIModAI, Display, TEXT("SetDamageVolumeEnabled: %s -> %s"), *VolumeId, bEnabled ? TEXT("enabled") : TEXT("disabled"));
	Result.bSuccess = true;
	return Result;
}


FAIModOperationResult UAIModFunctionLibrary::DespawnDamageVolume(UObject* WorldContextObject, const FString& VolumeId)
{
	FAIModOperationResult Result;
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		Result.ErrorCode = TEXT("NO_WORLD");
		Result.ErrorMessage = TEXT("No valid world context");
		return Result;
	}

	AFGDamageOverTimeVolume* Volume = FindDamageVolumeById(World, VolumeId);
	if (!Volume)
	{
		Result.ErrorCode = TEXT("TARGET_NOT_FOUND");
		Result.ErrorMessage = FString::Printf(TEXT("No AFGDamageOverTimeVolume exists with id '%s' (ids come from world.damageVolumes)"), *VolumeId);
		return Result;
	}

	// Same latched-DOT ordering rationale as SetDamageVolumeEnabled.
	Volume->SetActorEnableCollision(false);
	if (!Volume->Destroy())
	{
		Result.ErrorCode = TEXT("OPERATION_FAILED");
		Result.ErrorMessage = TEXT("Destroy() refused the volume actor (collision was still disabled - the hazard is inert but the actor remains)");
		return Result;
	}

	UE_LOG(LogAIModAI, Display, TEXT("DespawnDamageVolume: destroyed %s (session-only; the level actor returns on save load)"), *VolumeId);
	Result.bSuccess = true;
	return Result;
}



FString UAIModFunctionLibrary::LogProjectAssemblyAsJson(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		UE_LOG(LogAIModAI, Warning, TEXT("LogProjectAssemblyAsJson: no valid world context"));
		return TEXT("{}");
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);

	// Phase catalog first - valid even if the station actor doesn't exist
	// yet. Array order IS the phaseIndex contract for
	// SetProjectAssemblyVisualPhase.
	TArray<UFGGamePhase*> AllPhases = UFGGamePhase::GetAllGamePhaseAssetsSorted();
	TArray<TSharedPtr<FJsonValue>> PhasesJsonArray;
	for (int32 Index = 0; Index < AllPhases.Num(); ++Index)
	{
		const TSharedRef<FJsonObject> PhaseObject = MakeGamePhaseJson(AllPhases[Index]);
		PhaseObject->SetNumberField(TEXT("phaseIndex"), Index);
		PhasesJsonArray.Add(MakeShared<FJsonValueObject>(PhaseObject));
	}
	RootObject->SetArrayField(TEXT("allPhases"), PhasesJsonArray);

	if (AFGGamePhaseManager* PhaseManager = AFGGamePhaseManager::Get(World))
	{
		RootObject->SetObjectField(TEXT("currentGamePhase"), MakeGamePhaseJson(PhaseManager->GetCurrentGamePhase()));
		RootObject->SetObjectField(TEXT("targetGamePhase"), MakeGamePhaseJson(PhaseManager->GetTargetGamePhase()));
	}

	AFGProjectAssembly* Station = FindProjectAssembly(World);
	RootObject->SetBoolField(TEXT("found"), Station != nullptr);
	if (Station)
	{
		RootObject->SetStringField(TEXT("id"), Station->GetPathName());
		RootObject->SetObjectField(TEXT("position"), MakeVectorJson(Station->GetActorLocation()));
		RootObject->SetBoolField(TEXT("isPlayingLaunchSequence"), Station->IsPlayingLaunchSequence());

		// Protected UPROPERTYs - reflection-read for telemetry only.
		if (const FBoolProperty* MovingProperty = FindFProperty<FBoolProperty>(AFGProjectAssembly::StaticClass(), TEXT("mIsMovingToTarget")))
		{
			RootObject->SetBoolField(TEXT("isMovingToTarget"), MovingProperty->GetPropertyValue_InContainer(Station));
		}
		if (const FStructProperty* TargetProperty = FindFProperty<FStructProperty>(AFGProjectAssembly::StaticClass(), TEXT("mTargetLocation")))
		{
			RootObject->SetObjectField(TEXT("targetLocation"), MakeVectorJson(*TargetProperty->ContainerPtrToValuePtr<FVector>(Station)));
		}
		if (const FFloatProperty* SpeedProperty = FindFProperty<FFloatProperty>(AFGProjectAssembly::StaticClass(), TEXT("mMovementSpeed")))
		{
			RootObject->SetNumberField(TEXT("movementSpeed"), SpeedProperty->GetPropertyValue_InContainer(Station));
		}
		if (const FFloatProperty* HeightProperty = FindFProperty<FFloatProperty>(AFGProjectAssembly::StaticClass(), TEXT("mProjectAssemblyHeight")))
		{
			RootObject->SetNumberField(TEXT("projectAssemblyHeight"), HeightProperty->GetPropertyValue_InContainer(Station));
		}

		// mGamePhaseMap: phase class -> visual stage index ("from start(0)
		// to end") - the station's own notion of its build stages.
		TArray<TSharedPtr<FJsonValue>> StageMapJsonArray;
		if (const FMapProperty* MapProperty = FindFProperty<FMapProperty>(AFGProjectAssembly::StaticClass(), TEXT("mGamePhaseMap")))
		{
			FScriptMapHelper MapHelper(MapProperty, MapProperty->ContainerPtrToValuePtr<void>(Station));
			const FClassProperty* KeyProperty = CastField<FClassProperty>(MapProperty->KeyProp);
			const FIntProperty* ValueProperty = CastField<FIntProperty>(MapProperty->ValueProp);
			if (KeyProperty && ValueProperty)
			{
				for (FScriptMapHelper::FIterator MapIt(MapHelper); MapIt; ++MapIt)
				{
					const UClass* PhaseClass = Cast<UClass>(KeyProperty->GetPropertyValue(MapHelper.GetKeyPtr(*MapIt)));
					const int32 StageIndex = ValueProperty->GetPropertyValue(MapHelper.GetValuePtr(*MapIt));
					const TSharedRef<FJsonObject> EntryObject = MakeShared<FJsonObject>();
					EntryObject->SetStringField(TEXT("phaseClass"), PhaseClass ? PhaseClass->GetPathName() : FString());
					EntryObject->SetNumberField(TEXT("visualStage"), StageIndex);
					StageMapJsonArray.Add(MakeShared<FJsonValueObject>(EntryObject));
				}
			}
		}
		RootObject->SetArrayField(TEXT("phaseStageMap"), StageMapJsonArray);
	}

	UE_LOG(LogAIModAI, Display, TEXT("LogProjectAssemblyAsJson: station %s, %d phase asset(s)"),
		Station ? TEXT("found") : TEXT("NOT found"), AllPhases.Num());

	return WriteCondensedJson(RootObject);
}


FAIModOperationResult UAIModFunctionLibrary::SetProjectAssemblyVisualPhase(UObject* WorldContextObject, int32 PhaseIndex, const FString& PhaseAssetPath)
{
	FAIModOperationResult Result;
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		Result.ErrorCode = TEXT("NO_WORLD");
		Result.ErrorMessage = TEXT("No valid world context");
		return Result;
	}

	AFGProjectAssembly* Station = FindProjectAssembly(World);
	if (!Station)
	{
		Result.ErrorCode = TEXT("TARGET_NOT_FOUND");
		Result.ErrorMessage = TEXT("No AFGProjectAssembly actor exists in the world (it may only spawn once the Space Elevator is built)");
		return Result;
	}

	TArray<UFGGamePhase*> AllPhases = UFGGamePhase::GetAllGamePhaseAssetsSorted();
	UFGGamePhase* Phase = nullptr;
	if (!PhaseAssetPath.IsEmpty())
	{
		for (UFGGamePhase* Candidate : AllPhases)
		{
			if (Candidate && Candidate->GetPathName() == PhaseAssetPath)
			{
				Phase = Candidate;
				break;
			}
		}
		if (!Phase)
		{
			Result.ErrorCode = TEXT("INVALID_PHASE");
			Result.ErrorMessage = FString::Printf(TEXT("No game phase asset with path '%s' (paths come from world.projectAssembly allPhases)"), *PhaseAssetPath);
			return Result;
		}
	}
	else if (PhaseIndex >= 0)
	{
		if (!AllPhases.IsValidIndex(PhaseIndex))
		{
			Result.ErrorCode = TEXT("INVALID_PHASE");
			Result.ErrorMessage = FString::Printf(TEXT("phaseIndex %d out of range (0..%d, per world.projectAssembly allPhases)"), PhaseIndex, AllPhases.Num() - 1);
			return Result;
		}
		Phase = AllPhases[PhaseIndex];
	}
	else
	{
		Result.ErrorCode = TEXT("INVALID_REQUEST");
		Result.ErrorMessage = TEXT("Provide phaseIndex or phaseAssetPath");
		return Result;
	}

	// OnGamePhaseChanged is a protected BlueprintNativeEvent; ProcessEvent
	// on the event UFunction dispatches to the BP override that owns the
	// station visuals. This deliberately bypasses OnGamePhaseChangedInternal
	// and the phase manager: no narrative messages, no progression change.
	UFunction* PhaseChangedFunction = Station->FindFunction(FName(TEXT("OnGamePhaseChanged")));
	if (!PhaseChangedFunction)
	{
		Result.ErrorCode = TEXT("OPERATION_FAILED");
		Result.ErrorMessage = TEXT("AFGProjectAssembly has no OnGamePhaseChanged function (FactoryGame API changed?)");
		return Result;
	}

	struct FPhaseChangedParams
	{
		UFGGamePhase* currentGamePhase = nullptr;
	};
	FPhaseChangedParams Params;
	Params.currentGamePhase = Phase;
	Station->ProcessEvent(PhaseChangedFunction, &Params);

	UE_LOG(LogAIModAI, Display, TEXT("SetProjectAssemblyVisualPhase: fired OnGamePhaseChanged with '%s' (visual-only; real phase untouched)"),
		Phase ? *Phase->GetPathName() : TEXT("null"));
	Result.bSuccess = true;
	return Result;
}


FAIModOperationResult UAIModFunctionLibrary::SetProjectAssemblyHeight(UObject* WorldContextObject, float NewHeight)
{
	FAIModOperationResult Result;
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		Result.ErrorCode = TEXT("NO_WORLD");
		Result.ErrorMessage = TEXT("No valid world context");
		return Result;
	}

	AFGProjectAssembly* Station = FindProjectAssembly(World);
	if (!Station)
	{
		Result.ErrorCode = TEXT("TARGET_NOT_FOUND");
		Result.ErrorMessage = TEXT("No AFGProjectAssembly actor exists in the world (it may only spawn once the Space Elevator is built)");
		return Result;
	}

	// (1) Set the protected resting-height field via reflection, in case
	// the BP UpdatePosition re-derives location from it.
	if (const FFloatProperty* HeightProperty = FindFProperty<FFloatProperty>(AFGProjectAssembly::StaticClass(), TEXT("mProjectAssemblyHeight")))
	{
		HeightProperty->SetPropertyValue_InContainer(Station, NewHeight);
	}

	// (2) Move the actor directly, keeping its current XY (which tracks
	// the Space Elevator). (3) then call the BP UpdatePosition event.
	const FVector CurrentLocation = Station->GetActorLocation();
	const FVector NewLocation(CurrentLocation.X, CurrentLocation.Y, NewHeight);
	Station->SetActorLocation(NewLocation, /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);

	if (UFunction* UpdatePositionFunction = Station->FindFunction(FName(TEXT("UpdatePosition"))))
	{
		Station->ProcessEvent(UpdatePositionFunction, nullptr);
	}

	// Report where it actually ended up (a BP tick may have already moved
	// it back - the caller/live test compares this to NewHeight).
	const FVector ResultingLocation = Station->GetActorLocation();
	const TSharedRef<FJsonObject> Detail = MakeShared<FJsonObject>();
	Detail->SetObjectField(TEXT("position"), MakeVectorJson(ResultingLocation));
	Detail->SetNumberField(TEXT("requestedHeight"), NewHeight);
	Result.ResultDetailJson = WriteCondensedJson(Detail);

	UE_LOG(LogAIModAI, Display, TEXT("SetProjectAssemblyHeight: requested z=%.0f, actor now at z=%.0f (session-only; not saved)"),
		NewHeight, ResultingLocation.Z);
	Result.bSuccess = true;
	return Result;
}


FString UAIModFunctionLibrary::LogCreaturesAsJson(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;

	TArray<TSharedPtr<FJsonValue>> CreatureJsonArray;
	if (World)
	{
		for (TActorIterator<AFGCreature> It(World); It; ++It)
		{
			AFGCreature* Creature = *It;
			if (!IsValid(Creature)) { continue; }

			const TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
			Obj->SetStringField(TEXT("id"), Creature->GetPathName());
			// Full class PATH (not short name) so it can be fed back to
			// world.spawnCreature's creatureClass param.
			Obj->SetStringField(TEXT("class"), Creature->GetClass()->GetPathName());

			const FVector Loc = Creature->GetActorLocation();
			const TSharedRef<FJsonObject> Pos = MakeShared<FJsonObject>();
			Pos->SetNumberField(TEXT("x"), Loc.X); Pos->SetNumberField(TEXT("y"), Loc.Y); Pos->SetNumberField(TEXT("z"), Loc.Z);
			Obj->SetObjectField(TEXT("position"), Pos);

			const FVector Vel = Creature->GetVelocity();
			Obj->SetNumberField(TEXT("speed"), Vel.Size());
			const TSharedRef<FJsonObject> VelObj = MakeShared<FJsonObject>();
			VelObj->SetNumberField(TEXT("x"), Vel.X); VelObj->SetNumberField(TEXT("y"), Vel.Y); VelObj->SetNumberField(TEXT("z"), Vel.Z);
			Obj->SetObjectField(TEXT("velocity"), VelObj);

			Obj->SetStringField(TEXT("behaviorState"), CreatureStateEnumToString(Creature->GetCurrentBehaviorState()));
			AController* Ctrl = Creature->GetController();
			Obj->SetBoolField(TEXT("hasController"), Ctrl != nullptr);
			Obj->SetStringField(TEXT("controllerClass"), Ctrl ? Ctrl->GetClass()->GetName() : FString());
			Obj->SetBoolField(TEXT("isPassive"), Creature->IsPassiveCreature());
			Obj->SetBoolField(TEXT("isPersistent"), Creature->IsPersistent());
			Obj->SetBoolField(TEXT("isAliveAndWell"), Creature->IsAliveAndWell());

			if (UFGHealthComponent* Health = Creature->GetHealthComponent())
			{
				Obj->SetNumberField(TEXT("currentHealth"), Health->GetCurrentHealth());
				Obj->SetNumberField(TEXT("maxHealth"), Health->GetMaxHealth());
			}

			// Animation liveness: a frozen (pre-FinishSpawning) creature has no
			// AnimInstance driving its skeletal mesh.
			bool bHasAnimInstance = false;
			FString AnimClass;
			if (USkeletalMeshComponent* Mesh = Creature->GetMesh())
			{
				if (UAnimInstance* Anim = Mesh->GetAnimInstance())
				{
					bHasAnimInstance = true;
					AnimClass = Anim->GetClass()->GetName();
				}
			}
			Obj->SetBoolField(TEXT("hasAnimInstance"), bHasAnimInstance);
			Obj->SetStringField(TEXT("animInstanceClass"), AnimClass);

			CreatureJsonArray.Add(MakeShared<FJsonValueObject>(Obj));
		}
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetArrayField(TEXT("creatures"), CreatureJsonArray);

	FString JsonString;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonString);
	FJsonSerializer::Serialize(RootObject, Writer);

	UE_LOG(LogAIModAI, Display, TEXT("LogCreaturesAsJson: %d creature(s)"), CreatureJsonArray.Num());
	return JsonString;
}


// Shared by AddItemsToInventory / RemoveItemsFromInventory: resolve which of a
// buildable's inventories a role names. Deliberately a small whitelist of known
// factory inventories (drone station input/output/fuel, docking/truck station
// fuel/inventory, storage container, else the first inventory component) rather
// than a generic "any component" reach. OutDroneFuelStation is set only when a
// drone-station FUEL inventory is chosen, so the caller (add only) can arm the
// station's active fuel type afterward.

// Somersloop (Desc_WAT1) and Mercer Sphere (Desc_WAT2) are the deliberately
// scarce alien artifacts under "/Prototype/WAT/". The item-injection RPCs create
// items from NOTHING, so they must not fabricate these unless the player opts in
// via the "Allow Spawning Alien Artifacts" mod setting - a protection independent
// of UnlimitedResources (which only bypasses build-material cost).

FAIModOperationResult UAIModFunctionLibrary::AddItemsToInventory(UObject* WorldContextObject, const FString& BuildableId, const FString& InventoryRole, const FString& ItemClassPath, int32 Amount)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}
	if (BuildableId.IsEmpty())
	{
		return FAIModOperationResult::Failure(TEXT("INVALID_REQUEST"), TEXT("buildableId must be a non-empty string"));
	}
	if (Amount <= 0)
	{
		return FAIModOperationResult::Failure(TEXT("INVALID_REQUEST"), TEXT("amount must be a positive integer"));
	}

	UClass* ItemClassResolved = LoadObject<UClass>(nullptr, *ItemClassPath);
	if (!ItemClassResolved || !ItemClassResolved->IsChildOf(UFGItemDescriptor::StaticClass()))
	{
		return FAIModOperationResult::Failure(TEXT("INVALID_REQUEST"),
			FString::Printf(TEXT("itemClass '%s' did not resolve to a UFGItemDescriptor subclass"), *ItemClassPath));
	}
	const TSubclassOf<UFGItemDescriptor> ItemDesc = ItemClassResolved;

	if (IsProtectedAlienArtifactClass(ItemClassPath) && !GetAIModConfigBool(World, TEXT("AllowSpawningAlienArtifacts"), false))
	{
		return FAIModOperationResult::Failure(TEXT("ARTIFACT_SPAWN_BLOCKED"),
			FString::Printf(TEXT("'%s' is a deliberately-limited alien artifact (Somersloop/Mercer Sphere); creating it from nothing is blocked. Enable 'Allow Spawning Alien Artifacts' in AIMod settings to override (independent of Unlimited Resources)."), *ItemClassPath));
	}

	AFGBuildable* Buildable = FindBuildableById(World, BuildableId);
	if (!IsValid(Buildable))
	{
		return FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No buildable found with id '%s'"), *BuildableId));
	}

	const FString Role = InventoryRole.IsEmpty() ? TEXT("auto") : InventoryRole.ToLower();

	// When fuel is loaded into a drone station we must also fire the station's
	// OnFuelItemAdded handler afterwards (a raw AddStack does not arm the active
	// fuel type) - ResolveBuildableRoleInventory reports
	// that case via DroneStationToArmFuel.
	FString ResolvedRoleDesc;
	AFGBuildableDroneStation* DroneStationToArmFuel = nullptr;
	UFGInventoryComponent* TargetInventory = ResolveBuildableRoleInventory(Buildable, Role, ResolvedRoleDesc, DroneStationToArmFuel);

	if (!IsValid(TargetInventory))
	{
		return FAIModOperationResult::Failure(TEXT("INVALID_TARGET"),
			FString::Printf(TEXT("Could not resolve a '%s' inventory on buildable '%s' (%s)"), *Role, *BuildableId, *Buildable->GetClass()->GetName()));
	}

	const int32 Added = TargetInventory->AddStack(FInventoryStack(Amount, ItemDesc), /*allowPartialAdd*/ true);

	// Arm the drone station's active fuel type by invoking its OnFuelItemAdded
	// UFUNCTION (the handler a belt-fed fuel delivery would trigger). Targeted,
	// explicit call to a known handler for the fuel we just added - not a
	// generic reflection interface.
	FString ArmedFuelType;
	FString FuelArmMethod;
	if (DroneStationToArmFuel && Added > 0)
	{
		AFGDroneStationInfo* Info = DroneStationToArmFuel->GetInfo();
		// 1) Preferred: run the station's own OnFuelItemAdded handler (the
		//    belt-delivery path), now with the fuel inventory as the source.
		UFunction* Fn = DroneStationToArmFuel->FindFunction(FName(TEXT("OnFuelItemAdded")));
		if (Fn)
		{
			struct FOnFuelItemAddedParams { UClass* Item; int32 Amount; UFGInventoryComponent* Source; };
			FOnFuelItemAddedParams Params;
			Params.Item = ItemClassResolved;
			Params.Amount = Added;
			Params.Source = TargetInventory;
			DroneStationToArmFuel->ProcessEvent(Fn, &Params);
		}
		bool bArmed = (Info && Info->GetDroneActiveFuelType() != nullptr);
		FuelArmMethod = Fn ? (bArmed ? TEXT("OnFuelItemAdded") : TEXT("OnFuelItemAdded(no-effect)")) : TEXT("handler-not-found");

		// 2) Fallback: directly set the info's active/last-inserted fuel type
		//    (both TSubclassOf<UFGItemDescriptor>, reflected as FClassProperty).
		//    The working reference stations show mActiveDroneFuelType populated;
		//    a docked drone will not depart while it is None.
		if (Info && !bArmed)
		{
			for (const TCHAR* PropName : { TEXT("mLastInsertedFuelType"), TEXT("mActiveDroneFuelType") })
			{
				if (FClassProperty* Prop = CastField<FClassProperty>(Info->GetClass()->FindPropertyByName(FName(PropName))))
				{
					Prop->SetObjectPropertyValue_InContainer(Info, ItemClassResolved);
				}
			}
			bArmed = (Info->GetDroneActiveFuelType() != nullptr);
			if (bArmed) { FuelArmMethod = TEXT("directPropertySet"); }
		}
		if (Info)
		{
			ArmedFuelType = Info->GetDroneActiveFuelType() ? Info->GetDroneActiveFuelType()->GetPathName() : FString();
		}
	}

	const TSharedRef<FJsonObject> DetailObject = MakeShared<FJsonObject>();
	DetailObject->SetStringField(TEXT("resolvedInventory"), ResolvedRoleDesc);
	DetailObject->SetNumberField(TEXT("requested"), Amount);
	DetailObject->SetNumberField(TEXT("itemsAdded"), Added);
	DetailObject->SetNumberField(TEXT("inventorySlots"), TargetInventory->GetSizeLinear());
	if (DroneStationToArmFuel)
	{
		DetailObject->SetStringField(TEXT("armedActiveFuelType"), ArmedFuelType);
		DetailObject->SetStringField(TEXT("fuelArmMethod"), FuelArmMethod);
	}

	UE_LOG(LogAIModAI, Display, TEXT("AddItemsToInventory: %s <- %d x %s into %s (added %d)"),
		*BuildableId, Amount, *ItemClassPath, *ResolvedRoleDesc, Added);

	FString DetailJson;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> DetailWriter =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&DetailJson);
	FJsonSerializer::Serialize(DetailObject, DetailWriter);

	FAIModOperationResult Result = FAIModOperationResult::Success();
	Result.ResultDetailJson = DetailJson;
	return Result;
}


FAIModOperationResult UAIModFunctionLibrary::RemoveItemsFromInventory(UObject* WorldContextObject, const FString& BuildableId, const FString& InventoryRole, const FString& ItemClassPath, int32 Amount)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}
	if (BuildableId.IsEmpty())
	{
		return FAIModOperationResult::Failure(TEXT("INVALID_REQUEST"), TEXT("buildableId must be a non-empty string"));
	}
	if (Amount <= 0)
	{
		return FAIModOperationResult::Failure(TEXT("INVALID_REQUEST"), TEXT("amount must be a positive integer"));
	}

	UClass* ItemClassResolved = LoadObject<UClass>(nullptr, *ItemClassPath);
	if (!ItemClassResolved || !ItemClassResolved->IsChildOf(UFGItemDescriptor::StaticClass()))
	{
		return FAIModOperationResult::Failure(TEXT("INVALID_REQUEST"),
			FString::Printf(TEXT("itemClass '%s' did not resolve to a UFGItemDescriptor subclass"), *ItemClassPath));
	}
	const TSubclassOf<UFGItemDescriptor> ItemDesc = ItemClassResolved;

	AFGBuildable* Buildable = FindBuildableById(World, BuildableId);
	if (!IsValid(Buildable))
	{
		return FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No buildable found with id '%s'"), *BuildableId));
	}

	const FString Role = InventoryRole.IsEmpty() ? TEXT("auto") : InventoryRole.ToLower();
	FString ResolvedRoleDesc;
	AFGBuildableDroneStation* UnusedDroneFuel = nullptr;
	UFGInventoryComponent* TargetInventory = ResolveBuildableRoleInventory(Buildable, Role, ResolvedRoleDesc, UnusedDroneFuel);
	if (!IsValid(TargetInventory))
	{
		return FAIModOperationResult::Failure(TEXT("INVALID_TARGET"),
			FString::Printf(TEXT("Could not resolve a '%s' inventory on buildable '%s' (%s)"), *Role, *BuildableId, *Buildable->GetClass()->GetName()));
	}

	// Clamp to what's actually present and delta-measure the real removal, so
	// the reported count is exact and we never claim to remove more than existed.
	// UFGInventoryComponent::Remove destroys the items (they are not returned to
	// the player) - this is a deletion from that inventory, matching how the
	// caller would empty a chest.
	const int32 Have = TargetInventory->GetNumItems(ItemDesc);
	if (Have <= 0)
	{
		return FAIModOperationResult::Failure(TEXT("NOTHING_TO_REMOVE"),
			FString::Printf(TEXT("Inventory '%s' on '%s' holds none of '%s'"), *ResolvedRoleDesc, *BuildableId, *ItemClassPath));
	}
	const int32 ToRemove = FMath::Min(Amount, Have);
	TargetInventory->Remove(ItemDesc, ToRemove);
	const int32 Removed = Have - TargetInventory->GetNumItems(ItemDesc);

	const TSharedRef<FJsonObject> DetailObject = MakeShared<FJsonObject>();
	DetailObject->SetStringField(TEXT("resolvedInventory"), ResolvedRoleDesc);
	DetailObject->SetNumberField(TEXT("requested"), Amount);
	DetailObject->SetNumberField(TEXT("itemsRemoved"), Removed);
	DetailObject->SetNumberField(TEXT("remaining"), TargetInventory->GetNumItems(ItemDesc));

	UE_LOG(LogAIModAI, Display, TEXT("RemoveItemsFromInventory: %s <- removed %d x %s from %s (had %d)"),
		*BuildableId, Removed, *ItemClassPath, *ResolvedRoleDesc, Have);

	FString DetailJson;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> DetailWriter =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&DetailJson);
	FJsonSerializer::Serialize(DetailObject, DetailWriter);

	FAIModOperationResult Result = FAIModOperationResult::Success();
	Result.ResultDetailJson = DetailJson;
	return Result;
}


FAIModOperationResult UAIModFunctionLibrary::AddItemsToPlayerInventory(UObject* WorldContextObject, const FString& ItemClassPath, int32 Amount)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}
	if (Amount <= 0)
	{
		return FAIModOperationResult::Failure(TEXT("INVALID_REQUEST"), TEXT("amount must be a positive integer"));
	}

	UClass* ItemClassResolved = LoadObject<UClass>(nullptr, *ItemClassPath);
	if (!ItemClassResolved || !ItemClassResolved->IsChildOf(UFGItemDescriptor::StaticClass()))
	{
		return FAIModOperationResult::Failure(TEXT("INVALID_REQUEST"),
			FString::Printf(TEXT("itemClass '%s' did not resolve to a UFGItemDescriptor subclass"), *ItemClassPath));
	}
	const TSubclassOf<UFGItemDescriptor> ItemDesc = ItemClassResolved;

	if (IsProtectedAlienArtifactClass(ItemClassPath) && !GetAIModConfigBool(World, TEXT("AllowSpawningAlienArtifacts"), false))
	{
		return FAIModOperationResult::Failure(TEXT("ARTIFACT_SPAWN_BLOCKED"),
			FString::Printf(TEXT("'%s' is a deliberately-limited alien artifact (Somersloop/Mercer Sphere); creating it from nothing is blocked. Enable 'Allow Spawning Alien Artifacts' in AIMod settings to override (independent of Unlimited Resources)."), *ItemClassPath));
	}

	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Character)
	{
		return FAIModOperationResult::Failure(TEXT("NO_PLAYER"), TEXT("No local AFGCharacterPlayer (player index 0)"));
	}
	UFGInventoryComponent* Inventory = Character->GetInventory();
	if (!IsValid(Inventory))
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("Player has no inventory component"));
	}

	// Same creative injection as AddItemsToInventory (buildables), targeting the
	// player's own inventory. AddStack respects slot count / stack limits, so a
	// full inventory returns a partial (or zero) add - reported, not an error.
	// This unblocks flows that need a held ITEM (e.g. placePortableMiner needs a
	// portable-miner item; hand-loading fuel) which no other RPC could provide.
	const int32 Added = Inventory->AddStack(FInventoryStack(Amount, ItemDesc), /*allowPartialAdd*/ true);

	const TSharedRef<FJsonObject> DetailObject = MakeShared<FJsonObject>();
	DetailObject->SetNumberField(TEXT("requested"), Amount);
	DetailObject->SetNumberField(TEXT("itemsAdded"), Added);
	DetailObject->SetNumberField(TEXT("inventorySlots"), Inventory->GetSizeLinear());

	UE_LOG(LogAIModAI, Display, TEXT("AddItemsToPlayerInventory: %d x %s -> player (added %d)"), Amount, *ItemClassPath, Added);

	FString DetailJson;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> DetailWriter =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&DetailJson);
	FJsonSerializer::Serialize(DetailObject, DetailWriter);

	FAIModOperationResult Result = FAIModOperationResult::Success();
	Result.ResultDetailJson = DetailJson;
	return Result;
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

	// Multiplayer safety: by default, suppress chat messages
	// typed by anyone OTHER than the host player, so a guest in a
	// multiplayer session cannot issue instructions to an external AI
	// controller that naively treats every PlayerMessage in this history
	// as a command from its operator. The discriminator is the game's own
	// FChatMessageStruct::bIsLocalPlayerMessage ("True if this message has
	// been instigated by a local player. Automatically set by the Chat
	// Manager", NotReplicated) - AIMod's RPC server runs in the HOST's
	// process, so in that process only the host's own messages carry the
	// flag; messages replicated from remote clients do not. Suppression is
	// done HERE, at the mod's security boundary, rather than trusting
	// every external agent to check a field - a suppressed message never
	// reaches the protocol at all. The host can opt other players in via
	// the AllowNonHostChatMessages mod setting (AIModConfiguration.cpp);
	// an RPC caller can never enable it. System/Ada/Custom messages are
	// never suppressed - they are game/mod output, not player input.
	// NOTE: on a dedicated server there is no local player, so with the
	// setting off NO player chat would come through at all - a dedicated
	// server operator must enable the setting (documented in
	// RPC_REFERENCE.md; dedicated servers are outside this project's
	// current single-player/listen-host target).
	const bool bAllowNonHostChat = GetAIModConfigBool(WorldContextObject, TEXT("AllowNonHostChatMessages"), false);
	int32 SuppressedRemotePlayerMessages = 0;

	TArray<TSharedPtr<FJsonValue>> MessageArray;
	for (const FChatMessageStruct& Message : Messages)
	{
		const bool bIsPlayerMessage = Message.MessageType == EFGChatMessageType::CMT_PlayerMessage;
		const bool bIsRemotePlayerMessage = bIsPlayerMessage && !Message.bIsLocalPlayerMessage;
		if (bIsRemotePlayerMessage && !bAllowNonHostChat)
		{
			++SuppressedRemotePlayerMessages;
			continue;
		}

		const TSharedRef<FJsonObject> MessageObject = MakeShared<FJsonObject>();
		MessageObject->SetStringField(TEXT("sender"), Message.MessageSender.ToString());
		MessageObject->SetStringField(TEXT("text"), Message.MessageText.ToString());
		MessageObject->SetStringField(TEXT("type"), ChatMessageTypeToString(Message.MessageType));
		MessageObject->SetNumberField(TEXT("timestamp"), Message.ServerTimeStamp);
		MessageObject->SetBoolField(TEXT("isLocalPlayerMessage"), Message.bIsLocalPlayerMessage);
		// Explicit host-attribution field so an agent can (and should) key
		// its "instruction from my operator" logic on this rather than
		// re-deriving it from type+isLocalPlayerMessage. When
		// AllowNonHostChatMessages is on, remote players' messages appear
		// with fromHostPlayer=false - the agent can then decide per-message
		// how much authority to grant them.
		MessageObject->SetBoolField(TEXT("fromHostPlayer"), bIsPlayerMessage && Message.bIsLocalPlayerMessage);
		MessageArray.Add(MakeShared<FJsonValueObject>(MessageObject));
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetArrayField(TEXT("messages"), MessageArray);
	RootObject->SetBoolField(TEXT("nonHostChatAllowed"), bAllowNonHostChat);
	RootObject->SetNumberField(TEXT("suppressedRemotePlayerMessages"), SuppressedRemotePlayerMessages);

	const FString JsonString = WriteCondensedJson(RootObject);

	UE_LOG(LogAIModAI, Verbose, TEXT("LogChatHistoryAsJson: %d message(s), %d remote-player message(s) suppressed (AllowNonHostChatMessages=%s)"),
		Messages.Num(), SuppressedRemotePlayerMessages, bAllowNonHostChat ? TEXT("true") : TEXT("false"));

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
	// NOT visible in the actual in-game chat
	// UI. BroadcastChatMessage ("Broadcasts a chat message to all
	// connected players") is the real public entry point - it calls the
	// NetMulticast Multicast_BroadcastChatMessage internally, which is
	// almost certainly what actually drives the on-screen chat widget
	// for a normal player-typed message too.
	ChatManager->BroadcastChatMessage(ChatMessage, nullptr);

	UE_LOG(LogAIModAI, Display, TEXT("SendChatMessage: [%s] %s"), *ChatMessage.MessageSender.ToString(), *Message);

	return FAIModOperationResult::Success();
}


FAIModOperationResult UAIModFunctionLibrary::SetManufacturerClockSpeed(UObject* WorldContextObject, const FString& BuildableId, float ClockSpeedPercent)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}

	// Uses AFGBuildableFactory, not AFGBuildableManufacturer, so this covers
	// both manufacturers and extractors:
	// GetCanChangePotential/GetCurrentMinPotential/GetCurrentMaxPotential/
	// SetPendingPotential are all declared on the shared AFGBuildableFactory
	// base (per source - AFGBuildableResourceExtractorBase, a Miner's real
	// base class, IS an AFGBuildableFactory), so overclocking works on a
	// Miner as well as Smelters/Constructors. FindBuildableById + Cast, not
	// FindManufacturerById.
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
	// pattern (AActor::FinishSpawning, Actor.h). Without this call the
	// creature spawns but stays frozen
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

	// SetActorRotation() alone can report success but produce ZERO actual
	// change (GetActorRotation() reads back identical afterward, in the same
	// process/call - not a replication issue). Real Unreal buildables
	// commonly mark their root/mesh components Static for lighting/rendering
	// optimization, which silently refuses runtime SetWorldRotation() (the
	// warning macros for this are usually compiled out of Shipping,
	// explaining the total silence in the log). Force every scene component
	// to Movable first, logging each one's ORIGINAL mobility.
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

	// Do NOT gate the item lookup behind IsCentralStorageBuilt() (which
	// reports mCentralStorages.Num() > 0, a SEPARATE container-registration
	// bookkeeping array): on a save with real, already-built
	// AFGCentralStorageContainer buildables, IsCentralStorageBuilt() can
	// still report false (registration doesn't reliably re-fire for
	// containers loaded from a save - stub .cpp source, exact mechanism
	// unconfirmed), which would make this function skip the real item lookup
	// and silently report an empty Depot. GetAllItemsFromCentralStorage()
	// has no documented precondition and is safe to call unconditionally -
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

	// Same as LogCentralStorageAsJson: don't gate on
	// IsCentralStorageBuilt() - it's unreliable even with real,
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

	// Add-to-player BEFORE remove-from-Depot. The
	// Dimensional Depot has no API to deposit a raw amount back
	// (UploadItemFromInventoryToCentralStorage needs the item already sitting
	// in a real inventory slot), so a remove-then-add ordering would
	// silently DESTROY any part of the withdrawal that didn't fit in a full
	// player inventory. Instead: clamp to what the Depot actually holds, add
	// only what fits to the player, then remove from the Depot EXACTLY what
	// landed in the inventory - anything that didn't fit is never removed and
	// stays safely in the Depot.
	const int32 Available = CentralStorage->GetNumItemsFromCentralStorage(ItemClass);
	if (Available <= 0)
	{
		return FAIModOperationResult::Failure(TEXT("NOTHING_WITHDRAWN"),
			FString::Printf(TEXT("Dimensional Depot has none of '%s'"), *ItemClassPath));
	}

	const int32 ToWithdraw = FMath::Min(Amount, Available);
	const int32 NumAdded = PlayerInventory->AddStack(FInventoryStack(ToWithdraw, ItemClass), /*allowPartialAdd=*/true);
	if (NumAdded <= 0)
	{
		return FAIModOperationResult::Failure(TEXT("INVENTORY_FULL"),
			FString::Printf(TEXT("Player inventory has no room for '%s' - nothing withdrawn (all items remain safely in the Dimensional Depot)"), *ItemClassPath));
	}

	// Remove from the Depot exactly what actually landed in the player's
	// inventory. In this synchronous single-player RPC nothing can drain the
	// Depot between the read above and this call, so this removes NumAdded;
	// the guard below defends conservation regardless (never conjure items).
	const int32 NumRemoved = CentralStorage->TryRemoveItemsFromCentralStorage(ItemClass, NumAdded);
	if (NumRemoved < NumAdded)
	{
		PlayerInventory->Remove(ItemClass, NumAdded - NumRemoved);
	}

	UE_LOG(LogAIModAI, Display, TEXT("WithdrawFromCentralStorage: withdrew %d of %s from Dimensional Depot to player inventory (requested %d, Depot held %d)"),
		NumRemoved, *ItemClassPath, Amount, Available);

	if (NumAdded < ToWithdraw)
	{
		// The player's inventory filled before the full available amount
		// could be withdrawn. The un-withdrawn remainder is safely still in
		// the Depot - nothing was lost. Reported as a soft failure so the
		// caller knows it didn't receive everything it asked for.
		return FAIModOperationResult::Failure(TEXT("INVENTORY_FULL"),
			FString::Printf(TEXT("Withdrew %d of %d requested - player inventory filled up; the remainder is safely still in the Dimensional Depot, not lost"), NumRemoved, Amount));
	}

	return FAIModOperationResult::Success();
}


FAIModOperationResult UAIModFunctionLibrary::UploadToCentralStorage(UObject* WorldContextObject, const FString& ItemClassPath, int32 Amount)
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
	if (!IsValid(PlayerInventory))
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No player inventory found"));
	}

	UClass* ResolvedClass = LoadObject<UClass>(nullptr, *ItemClassPath);
	if (!ResolvedClass || !ResolvedClass->IsChildOf(UFGItemDescriptor::StaticClass()))
	{
		return FAIModOperationResult::Failure(TEXT("INVALID_ITEM_CLASS"),
			FString::Printf(TEXT("'%s' did not resolve to a UFGItemDescriptor subclass"), *ItemClassPath));
	}
	const TSubclassOf<UFGItemDescriptor> ItemClass = ResolvedClass;

	const int32 Have = PlayerInventory->GetNumItems(ItemClass);
	if (Have <= 0)
	{
		return FAIModOperationResult::Failure(TEXT("NOTHING_TO_UPLOAD"),
			FString::Printf(TEXT("Player inventory has none of '%s'"), *ItemClassPath));
	}
	// Remaining Depot capacity for this item (limit - already-stored). The Depot
	// caps per-item; do not attempt to exceed it.
	const int32 Room = FMath::Max(0, CentralStorage->GetCentralStorageItemLimit(ItemClass) - CentralStorage->GetNumItemsFromCentralStorage(ItemClass));
	if (Room <= 0)
	{
		return FAIModOperationResult::Failure(TEXT("DEPOT_FULL"),
			FString::Printf(TEXT("Dimensional Depot is at capacity for '%s'"), *ItemClassPath));
	}
	const int32 Cap = FMath::Min3(Amount, Have, Room);

	// UploadItemFromInventoryToCentralStorage deposits ONE item per call (a
	// single call on a 100-stack moves 1), so loop: each
	// pass find a slot still holding the item and upload once, until we've moved
	// Cap items or nothing more can move. Delta-measured against the Depot count
	// so the reported total is exact; a zero-progress call breaks the loop
	// (never spin). Precise to the item - no overshoot, nothing destroyed.
	int32 Uploaded = 0;
	int32 Guard = 0;
	const int32 GuardMax = Cap + 8;
	while (Uploaded < Cap && Guard++ < GuardMax)
	{
		int32 SlotIdx = INDEX_NONE;
		const int32 SlotCount = PlayerInventory->GetSizeLinear();
		for (int32 Idx = 0; Idx < SlotCount; ++Idx)
		{
			FInventoryStack Stack;
			if (PlayerInventory->GetStackFromIndex(Idx, Stack) && Stack.Item.GetItemClass() == ItemClass
				&& Stack.NumItems > 0 && CentralStorage->CanUploadInventoryItemToCentralStorage(Stack.Item))
			{
				SlotIdx = Idx;
				break;
			}
		}
		if (SlotIdx == INDEX_NONE) { break; }
		const int32 Before = CentralStorage->GetNumItemsFromCentralStorage(ItemClass);
		if (!CentralStorage->UploadItemFromInventoryToCentralStorage(PlayerInventory, SlotIdx)) { break; }
		const int32 Moved = CentralStorage->GetNumItemsFromCentralStorage(ItemClass) - Before;
		if (Moved <= 0) { break; }
		Uploaded += Moved;
	}

	const TSharedRef<FJsonObject> DetailObject = MakeShared<FJsonObject>();
	DetailObject->SetNumberField(TEXT("requested"), Amount);
	DetailObject->SetNumberField(TEXT("itemsUploaded"), Uploaded);
	DetailObject->SetNumberField(TEXT("depotNowHolds"), CentralStorage->GetNumItemsFromCentralStorage(ItemClass));

	UE_LOG(LogAIModAI, Display, TEXT("UploadToCentralStorage: uploaded %d of %s to Dimensional Depot (requested %d, player held %d, room %d)"),
		Uploaded, *ItemClassPath, Amount, Have, Room);

	FString DetailJson;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> DetailWriter =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&DetailJson);
	FJsonSerializer::Serialize(DetailObject, DetailWriter);

	if (Uploaded <= 0)
	{
		return FAIModOperationResult::Failure(TEXT("NOTHING_UPLOADED"),
			FString::Printf(TEXT("Nothing uploaded - the smallest matching stack exceeds the remaining cap (%d), or the item cannot be stored. Items remain safely in the player inventory."), Cap));
	}

	FAIModOperationResult Result = FAIModOperationResult::Success();
	Result.ResultDetailJson = DetailJson;
	return Result;
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



// LogRecipeCatalogAsJson/world.recipeCatalog, LogItemCatalogAsJson/
// world.itemCatalog, LogBuildableCatalogAsJson/world.buildableCatalog
// support pre-planning complex builds: what items can be built, what
// recipes/alternates build each item, what machines are needed,
// resource/power requirements, input/output counts and types, rates
// including power shards/Somersloop. Belt/pipe rates are covered by
// world.conveyorBeltTiers/world.pipelineTiers - these three cover the rest.
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



FAIModOperationResult UAIModFunctionLibrary::SetActiveEvent(UObject* WorldContextObject, const FString& EventNameOrIndex)
{
	FAIModOperationResult Result;
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	AFGEventSubsystem* EventSubsystem = World ? AFGEventSubsystem::Get(World) : nullptr;
	if (!EventSubsystem)
	{
		Result.ErrorCode = TEXT("NO_WORLD");
		Result.ErrorMessage = TEXT("No valid world context or AFGEventSubsystem");
		return Result;
	}

	EEvents Event = EEvents::EV_None;
	if (!StringToEvent(EventNameOrIndex, Event))
	{
		Result.ErrorCode = TEXT("INVALID_EVENT");
		Result.ErrorMessage = FString::Printf(TEXT("Unknown event '%s' (use None/Christmas/Anniversary/CSSBirthday/FirstOfApril or index 0-4)"), *EventNameOrIndex);
		return Result;
	}

	// mCurrentEvents is a public replicated TArray<EEvents> - modify
	// directly (server authority replicates it). This mirrors
	// setProjectAssemblyVisualPhase: change state, then fire the BP
	// visual hook. Real progression/calendar unlocks untouched.
	if (Event == EEvents::EV_None)
	{
		// Clear all forced events. NOTE: there is no OnEndEvent hook in
		// the API, so already-spawned HUB decorations may persist until a
		// reload; clearing only stops IsEventActive returning true.
		EventSubsystem->mCurrentEvents.Empty();
	}
	else
	{
		EventSubsystem->mCurrentEvents.AddUnique(Event);
		// OnBeginEvent is the BlueprintImplementableEvent the BP subclass
		// uses to spawn event visuals (same path BeginPlay uses per active
		// event). Fire it via ProcessEvent. (OnBeginEvent_Native is
		// protected AND not a UFUNCTION, so it can't be invoked here.)
		if (UFunction* BeginFn = EventSubsystem->FindFunction(FName(TEXT("OnBeginEvent"))))
		{
			struct FBeginEventParams { EEvents event; };
			FBeginEventParams P; P.event = Event;
			EventSubsystem->ProcessEvent(BeginFn, &P);
		}
	}

	const TSharedRef<FJsonObject> Detail = MakeShared<FJsonObject>();
	Detail->SetStringField(TEXT("event"), EventToString(Event));
	TArray<TSharedPtr<FJsonValue>> ActiveNow;
	for (const EEvents E : EventSubsystem->GetCurrentEvents())
	{
		ActiveNow.Add(MakeShared<FJsonValueString>(EventToString(E)));
	}
	Detail->SetArrayField(TEXT("activeEvents"), ActiveNow);
	Result.ResultDetailJson = WriteCondensedJson(Detail);
	UE_LOG(LogAIModAI, Display, TEXT("SetActiveEvent: %s (session-only; reverts on reload)"), *EventToString(Event));
	Result.bSuccess = true;
	return Result;
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


FAIModOperationResult UAIModFunctionLibrary::PayOffMilestone(UObject* WorldContextObject, const FString& SchematicClassPath, bool bDryRun, bool bFromDepot)
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

	// fromDepot: auto-withdraw the shortfall from the
	// Dimensional Depot into the carried inventory before submitting, so a
	// produce->upload->pay loop is one call. Same conservative add-then-
	// remove pattern as WithdrawFromCentralStorage (never conjure items).
	// On a dryRun we don't mutate; instead the submission preview below
	// counts Depot stock as effectively available.
	AFGCentralStorageSubsystem* CentralStorage = nullptr;
	TArray<FItemAmount> WithdrawnFromDepot;
	if (bFromDepot)
	{
		CentralStorage = AFGCentralStorageSubsystem::Get(World);
		if (!CentralStorage)
		{
			return FAIModOperationResult::Failure(TEXT("NO_CENTRAL_STORAGE"), TEXT("fromDepot requested but no AFGCentralStorageSubsystem for this world"));
		}
		if (!bDryRun)
		{
			for (const FItemAmount& Owed : RemainingCost)
			{
				if (!Owed.ItemClass || Owed.Amount <= 0) { continue; }
				const int32 Need = Owed.Amount - PlayerInventory->GetNumItems(Owed.ItemClass);
				if (Need <= 0) { continue; }
				const int32 ToPull = FMath::Min(Need, CentralStorage->GetNumItemsFromCentralStorage(Owed.ItemClass));
				if (ToPull <= 0) { continue; }
				const int32 Added = PlayerInventory->AddStack(FInventoryStack(ToPull, Owed.ItemClass), /*allowPartialAdd=*/true);
				if (Added > 0)
				{
					const int32 Removed = CentralStorage->TryRemoveItemsFromCentralStorage(Owed.ItemClass, Added);
					if (Removed < Added) { PlayerInventory->Remove(Owed.ItemClass, Added - Removed); }
					if (Removed > 0) { WithdrawnFromDepot.Add(FItemAmount(Owed.ItemClass, Removed)); }
				}
			}
		}
	}

	TArray<FItemAmount> Submission;
	TArray<FItemAmount> Shortfall;
	for (const FItemAmount& Owed : RemainingCost)
	{
		if (!Owed.ItemClass || Owed.Amount <= 0) { continue; }
		int32 Have = PlayerInventory->GetNumItems(Owed.ItemClass);
		// dryRun preview counts Depot stock as available (real withdraw
		// above already moved it into the inventory for the live path).
		if (bFromDepot && bDryRun && CentralStorage)
		{
			Have += CentralStorage->GetNumItemsFromCentralStorage(Owed.ItemClass);
		}
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
		DetailObject->SetBoolField(TEXT("fromDepot"), bFromDepot);
		if (bFromDepot)
		{
			DetailObject->SetArrayField(TEXT("withdrawnFromDepot"), ItemAmountsToJsonArray(WithdrawnFromDepot));
		}
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

FAIModOperationResult UAIModFunctionLibrary::SetActiveMilestone(UObject* WorldContextObject, const FString& SchematicClassPath)
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

	UClass* ResolvedClass = LoadObject<UClass>(nullptr, *SchematicClassPath);
	if (!ResolvedClass || !ResolvedClass->IsChildOf(UFGSchematic::StaticClass()))
	{
		return FAIModOperationResult::Failure(TEXT("INVALID_SCHEMATIC"),
			FString::Printf(TEXT("'%s' did not resolve to a UFGSchematic subclass"), *SchematicClassPath));
	}
	const TSubclassOf<UFGSchematic> SchematicClass = ResolvedClass;
	const TSubclassOf<UFGSchematic> PreviousActive = SchematicManager->GetActiveSchematic();

	if (!SchematicManager->CanSetAsActiveSchematic(SchematicClass))
	{
		return FAIModOperationResult::Failure(TEXT("CANNOT_SET_ACTIVE"),
			FString::Printf(TEXT("CanSetAsActiveSchematic('%s') is false - already purchased, wrong type, or tier not available"), *SchematicClass->GetName()));
	}

	const bool bSet = SchematicManager->SetActiveSchematic(SchematicClass);
	const TSubclassOf<UFGSchematic> NowActive = SchematicManager->GetActiveSchematic();
	if (!bSet || NowActive != SchematicClass)
	{
		return FAIModOperationResult::Failure(TEXT("SET_ACTIVE_REJECTED"),
			FString::Printf(TEXT("SetActiveSchematic('%s') %s and GetActiveSchematic() now reads '%s'"),
				*SchematicClass->GetName(), bSet ? TEXT("returned true") : TEXT("returned false"),
				NowActive ? *NowActive->GetName() : TEXT("<none>")));
	}

	UE_LOG(LogAIModAI, Display, TEXT("SetActiveMilestone: active schematic %s -> %s"),
		PreviousActive ? *PreviousActive->GetName() : TEXT("<none>"), *SchematicClass->GetName());

	const TSharedRef<FJsonObject> DetailObject = MakeShared<FJsonObject>();
	DetailObject->SetStringField(TEXT("activeSchematic"), SchematicClass->GetPathName());
	DetailObject->SetStringField(TEXT("previousActiveSchematic"), PreviousActive ? PreviousActive->GetPathName() : TEXT(""));
	FAIModOperationResult Result = FAIModOperationResult::Success();
	Result.ResultDetailJson = SerializeJsonObject(DetailObject);
	return Result;
}

FAIModOperationResult UAIModFunctionLibrary::LaunchHubShip(UObject* WorldContextObject)
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

	const TSubclassOf<UFGSchematic> ActiveSchematic = SchematicManager->GetActiveSchematic();
	if (!ActiveSchematic)
	{
		return FAIModOperationResult::Failure(TEXT("NO_ACTIVE_SCHEMATIC"),
			TEXT("No active schematic - select one via world.setActiveMilestone (or the HUB terminal) before launching"));
	}

	if (!SchematicManager->IsSchematicPaidOff(ActiveSchematic))
	{
		const TArray<FItemAmount> Remaining = SchematicManager->GetRemainingCostFor(ActiveSchematic);
		const TSharedRef<FJsonObject> DetailObject = MakeShared<FJsonObject>();
		DetailObject->SetStringField(TEXT("activeSchematic"), ActiveSchematic->GetPathName());
		DetailObject->SetArrayField(TEXT("remainingCost"), ItemAmountsToJsonArray(Remaining));
		FAIModOperationResult Result = FAIModOperationResult::Failure(TEXT("NOT_PAID_OFF"),
			FString::Printf(TEXT("Active schematic '%s' is not fully paid off - pay via world.payMilestone first (the real launch button is likewise disabled until paid)"), *ActiveSchematic->GetName()));
		Result.ResultDetailJson = SerializeJsonObject(DetailObject);
		return Result;
	}

	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Character)
	{
		return FAIModOperationResult::Failure(TEXT("NO_PLAYER"), TEXT("No local AFGCharacterPlayer (player index 0)"));
	}

	SchematicManager->LaunchShip(Character);

	// LaunchShip returns void and its .cpp is a stub - report the
	// observable post-call state instead of a hard verify; the milestone's
	// purchased flag flips when the ship RETURNS, not here.
	const float TimeUntilReturn = SchematicManager->GetTimeUntilShipReturn();
	const bool bAtTradingPost = SchematicManager->IsShipAtTradingPost();
	UE_LOG(LogAIModAI, Display, TEXT("LaunchHubShip: launched for '%s'; timeUntilShipReturn=%.1fs shipAtTradingPost=%s"),
		*ActiveSchematic->GetName(), TimeUntilReturn, bAtTradingPost ? TEXT("true") : TEXT("false"));

	const TSharedRef<FJsonObject> DetailObject = MakeShared<FJsonObject>();
	DetailObject->SetStringField(TEXT("activeSchematic"), ActiveSchematic->GetPathName());
	DetailObject->SetNumberField(TEXT("timeUntilShipReturn"), TimeUntilReturn);
	DetailObject->SetBoolField(TEXT("shipAtTradingPost"), bAtTradingPost);
	FAIModOperationResult Result = FAIModOperationResult::Success();
	Result.ResultDetailJson = SerializeJsonObject(DetailObject);
	return Result;
}

FAIModOperationResult UAIModFunctionLibrary::ReprocessMilestone(UObject* WorldContextObject, const FString& SchematicClassPath, int32 Tier, bool bAllTiers)
{
	FAIModOperationResult Result;
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		Result.ErrorCode = TEXT("NO_WORLD");
		Result.ErrorMessage = TEXT("No valid world context");
		return Result;
	}
	AFGSchematicManager* SchematicManager = AFGSchematicManager::Get(World);
	if (!SchematicManager)
	{
		Result.ErrorCode = TEXT("INTERNAL_ERROR");
		Result.ErrorMessage = TEXT("AFGSchematicManager::Get returned null");
		return Result;
	}

	// Build the target list: one explicit schematic, a whole tier, or all
	// tiers' HUB milestone/tutorial schematics.
	TArray<TSubclassOf<UFGSchematic>> Targets;
	if (!SchematicClassPath.IsEmpty())
	{
		UClass* Resolved = LoadObject<UClass>(nullptr, *SchematicClassPath);
		if (!Resolved || !Resolved->IsChildOf(UFGSchematic::StaticClass()))
		{
			Result.ErrorCode = TEXT("INVALID_SCHEMATIC");
			Result.ErrorMessage = FString::Printf(TEXT("'%s' did not resolve to a UFGSchematic subclass"), *SchematicClassPath);
			return Result;
		}
		Targets.Add(Resolved);
	}
	else if (bAllTiers)
	{
		const int32 MaxTier = SchematicManager->GetHighestAvailableTechTier();
		for (int32 T = 0; T <= MaxTier; ++T)
		{
			TArray<TSubclassOf<UFGSchematic>> TierSchematics;
			SchematicManager->GetHubSchematicsForTier(T, TierSchematics);
			Targets.Append(TierSchematics);
		}
	}
	else if (Tier >= 0)
	{
		SchematicManager->GetHubSchematicsForTier(Tier, Targets);
	}
	else
	{
		Result.ErrorCode = TEXT("INVALID_REQUEST");
		Result.ErrorMessage = TEXT("Provide schematicClass, tier (>=0), or allTiers=true");
		return Result;
	}

	// Only reprocess schematics that are actually purchased (reprocessing
	// re-runs the unlock/completion flow to re-fire achievements). Reset
	// does NOT revoke unlocks (per the API comment), so recipes are kept.
	TArray<TSubclassOf<UFGSchematic>> Purchased;
	for (const TSubclassOf<UFGSchematic>& S : Targets)
	{
		if (S && SchematicManager->IsSchematicPurchased(S))
		{
			Purchased.Add(S);
		}
	}
	if (Purchased.Num() == 0)
	{
		Result.ErrorCode = TEXT("NOTHING_TO_DO");
		Result.ErrorMessage = TEXT("No purchased schematics matched the request");
		return Result;
	}

	// Reset the purchased bookkeeping (keeps unlocks) then re-give access,
	// which re-runs the completion flow (and its inline achievement check).
	SchematicManager->ResetPurchasedSchematics(Purchased);
	SchematicManager->GiveAccessToSchematics(Purchased, nullptr, ESchematicUnlockFlags::None);

	const TSharedRef<FJsonObject> Detail = MakeShared<FJsonObject>();
	Detail->SetNumberField(TEXT("reprocessed"), Purchased.Num());
	TArray<TSharedPtr<FJsonValue>> Names;
	for (const TSubclassOf<UFGSchematic>& S : Purchased)
	{
		Names.Add(MakeShared<FJsonValueString>(S->GetPathName()));
	}
	Detail->SetArrayField(TEXT("schematics"), Names);
	Result.ResultDetailJson = WriteCondensedJson(Detail);
	UE_LOG(LogAIModAI, Display, TEXT("ReprocessMilestone: reset+re-gave %d schematic(s) to re-fire achievements"), Purchased.Num());
	Result.bSuccess = true;
	return Result;
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
