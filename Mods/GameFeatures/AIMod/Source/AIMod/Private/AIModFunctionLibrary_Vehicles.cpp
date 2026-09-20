// Copyright Epic Games, Inc. All Rights Reserved.

#include "AIModFunctionLibraryInternal.h"
using namespace AIModInternal;

FAIModOperationResult UAIModFunctionLibrary::SetVehicleEngineParams(UObject* WorldContextObject, const FString& VehicleId, float MaxEngineTorque, float DragCoefficient)
{
	FAIModOperationResult Result;
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		Result.ErrorCode = TEXT("NO_WORLD");
		Result.ErrorMessage = TEXT("No valid world context");
		return Result;
	}

	// Target: explicit id, else nearest wheeled vehicle to the local player.
	AFGWheeledVehicle* Target = nullptr;
	if (!VehicleId.IsEmpty())
	{
		for (TActorIterator<AFGWheeledVehicle> It(World); It; ++It)
		{
			if (IsValid(*It) && (*It)->GetPathName() == VehicleId)
			{
				Target = *It;
				break;
			}
		}
		if (!Target)
		{
			Result.ErrorCode = TEXT("TARGET_NOT_FOUND");
			Result.ErrorMessage = FString::Printf(TEXT("No AFGWheeledVehicle with id '%s' (ids from world.vehicles)"), *VehicleId);
			return Result;
		}
	}
	else
	{
		FVector PlayerLocation = FVector::ZeroVector;
		if (const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0))
		{
			PlayerLocation = PlayerPawn->GetActorLocation();
		}
		double BestDistSq = -1.0;
		for (TActorIterator<AFGWheeledVehicle> It(World); It; ++It)
		{
			if (!IsValid(*It))
			{
				continue;
			}
			const double DistSq = FVector::DistSquared((*It)->GetActorLocation(), PlayerLocation);
			if (BestDistSq < 0.0 || DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				Target = *It;
			}
		}
		if (!Target)
		{
			Result.ErrorCode = TEXT("TARGET_NOT_FOUND");
			Result.ErrorMessage = TEXT("No AFGWheeledVehicle found in the world");
			return Result;
		}
	}

	UFGWheeledVehicleMovementComponent* Movement = Target->GetVehicleMovementComponent();
	if (!Movement)
	{
		Result.ErrorCode = TEXT("OPERATION_FAILED");
		Result.ErrorMessage = TEXT("Target vehicle has no movement component");
		return Result;
	}

	// UFGWheeledVehicleMovementComponent derives from
	// UChaosWheeledVehicleMovementComponent; these are its real runtime
	// setters (apply to the live physics vehicle, no rebuild needed).
	bool bAppliedTorque = false;
	bool bAppliedDrag = false;
	if (MaxEngineTorque >= 0.0f)
	{
		Movement->SetMaxEngineTorque(MaxEngineTorque);
		bAppliedTorque = true;
	}
	if (DragCoefficient >= 0.0f)
	{
		Movement->SetDragCoefficient(DragCoefficient);
		bAppliedDrag = true;
	}

	const TSharedRef<FJsonObject> Detail = MakeShared<FJsonObject>();
	Detail->SetStringField(TEXT("vehicleId"), Target->GetPathName());
	Detail->SetBoolField(TEXT("appliedTorque"), bAppliedTorque);
	if (bAppliedTorque) Detail->SetNumberField(TEXT("maxEngineTorque"), MaxEngineTorque);
	Detail->SetBoolField(TEXT("appliedDrag"), bAppliedDrag);
	if (bAppliedDrag) Detail->SetNumberField(TEXT("dragCoefficient"), DragCoefficient);
	Result.ResultDetailJson = WriteCondensedJson(Detail);

	UE_LOG(LogAIModAI, Display, TEXT("SetVehicleEngineParams: %s torque=%s drag=%s"),
		*Target->GetPathName(),
		bAppliedTorque ? *FString::SanitizeFloat(MaxEngineTorque) : TEXT("(unchanged)"),
		bAppliedDrag ? *FString::SanitizeFloat(DragCoefficient) : TEXT("(unchanged)"));
	Result.bSuccess = true;
	return Result;
}



FString UAIModFunctionLibrary::LogMantasAsJson(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		UE_LOG(LogAIModAI, Warning, TEXT("LogMantasAsJson: no valid world context"));
		return TEXT("{}");
	}

	TArray<TSharedPtr<FJsonValue>> Arr;
	for (TActorIterator<AFGManta> It(World); It; ++It)
	{
		AFGManta* M = *It;
		if (!IsValid(M)) { continue; }
		const TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("id"), M->GetPathName());
		Obj->SetStringField(TEXT("class"), M->GetClass()->GetPathName());
		Obj->SetObjectField(TEXT("position"), MakeVectorJson(M->GetActorLocation()));
		Obj->SetNumberField(TEXT("currentTime"), M->GetCurrentTime());
		Obj->SetNumberField(TEXT("secondsPerLoop"), GetMantaFloat(M, TEXT("mSecondsPerLoop")));
		Obj->SetNumberField(TEXT("offsetMagnitude"), GetMantaFloat(M, TEXT("mOffsetMagnitude")));
		Obj->SetBoolField(TEXT("tickTransform"), GetMantaBool(M, TEXT("mTickTransform")));
		Obj->SetBoolField(TEXT("isClosedSplineLoop"), GetMantaBool(M, TEXT("mIsClosedSplineLoop")));
		if (USplineComponent* Spline = M->GetSpline())
		{
			Obj->SetBoolField(TEXT("hasSpline"), true);
			Obj->SetNumberField(TEXT("splineLength"), Spline->GetSplineLength());
		}
		else
		{
			Obj->SetBoolField(TEXT("hasSpline"), false);
		}
		Arr.Add(MakeShared<FJsonValueObject>(Obj));
	}

	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("protocolVersion"), 1);
	Root->SetArrayField(TEXT("mantas"), Arr);
	UE_LOG(LogAIModAI, Display, TEXT("LogMantasAsJson: %d manta(s)"), Arr.Num());
	return WriteCondensedJson(Root);
}


FAIModOperationResult UAIModFunctionLibrary::SetManta(UObject* WorldContextObject, const FString& MantaId, bool bDespawn, bool bHasFreeze, bool bFreeze, float SecondsPerLoop, float CurrentTime)
{
	FAIModOperationResult Result;
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		Result.ErrorCode = TEXT("NO_WORLD");
		Result.ErrorMessage = TEXT("No valid world context");
		return Result;
	}

	AFGManta* Manta = FindMantaById(World, MantaId);
	if (!Manta)
	{
		Result.ErrorCode = TEXT("TARGET_NOT_FOUND");
		Result.ErrorMessage = FString::Printf(TEXT("No AFGManta with id '%s' (ids from world.mantas)"), *MantaId);
		return Result;
	}

	if (bDespawn)
	{
		const FString Id = Manta->GetPathName();
		Manta->Destroy();
		UE_LOG(LogAIModAI, Display, TEXT("SetManta: despawned %s (session-only; returns on reload)"), *Id);
		Result.bSuccess = true;
		return Result;
	}

	if (bHasFreeze)
	{
		// mTickTransform true = advances along spline; false = frozen.
		SetMantaBool(Manta, TEXT("mTickTransform"), !bFreeze);
	}
	if (SecondsPerLoop > 0.0f)
	{
		SetMantaFloat(Manta, TEXT("mSecondsPerLoop"), SecondsPerLoop);
	}
	if (CurrentTime >= 0.0f)
	{
		SetMantaFloat(Manta, TEXT("mCurrentTime"), CurrentTime);
	}

	const TSharedRef<FJsonObject> Detail = MakeShared<FJsonObject>();
	Detail->SetStringField(TEXT("mantaId"), Manta->GetPathName());
	Detail->SetBoolField(TEXT("tickTransform"), GetMantaBool(Manta, TEXT("mTickTransform")));
	Detail->SetNumberField(TEXT("secondsPerLoop"), GetMantaFloat(Manta, TEXT("mSecondsPerLoop")));
	Detail->SetNumberField(TEXT("currentTime"), Manta->GetCurrentTime());
	Result.ResultDetailJson = WriteCondensedJson(Detail);
	UE_LOG(LogAIModAI, Display, TEXT("SetManta: %s updated"), *Manta->GetPathName());
	Result.bSuccess = true;
	return Result;
}


FAIModOperationResult UAIModFunctionLibrary::SpawnManta(UObject* WorldContextObject, const FString& SourceMantaId, float TimeOffsetSeconds)
{
	FAIModOperationResult Result;
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		Result.ErrorCode = TEXT("NO_WORLD");
		Result.ErrorMessage = TEXT("No valid world context");
		return Result;
	}

	// Source manta: explicit id, else the first one in the world.
	AFGManta* Source = nullptr;
	if (!SourceMantaId.IsEmpty())
	{
		Source = FindMantaById(World, SourceMantaId);
	}
	else
	{
		for (TActorIterator<AFGManta> It(World); It; ++It)
		{
			if (IsValid(*It)) { Source = *It; break; }
		}
	}
	if (!Source)
	{
		Result.ErrorCode = TEXT("TARGET_NOT_FOUND");
		Result.ErrorMessage = TEXT("No source AFGManta to copy a spline path from (need an existing manta)");
		return Result;
	}

	// Copy the spline-path object ref via reflection so the new manta
	// flies the SAME route.
	const FObjectProperty* SplinePathProp = FindFProperty<FObjectProperty>(AFGManta::StaticClass(), TEXT("mSplinePath"));
	UObject* SplinePath = SplinePathProp ? SplinePathProp->GetObjectPropertyValue_InContainer(Source) : nullptr;
	if (!SplinePath)
	{
		Result.ErrorCode = TEXT("OPERATION_FAILED");
		Result.ErrorMessage = TEXT("Source manta has no mSplinePath to share (cannot spawn a routeless manta)");
		return Result;
	}

	// Deferred spawn of the source's OWN class (its BP subclass carries
	// the mesh) so we can set mSplinePath BEFORE BeginPlay caches it.
	const FTransform SpawnTransform(Source->GetActorRotation(), Source->GetActorLocation());
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AFGManta* NewManta = World->SpawnActorDeferred<AFGManta>(Source->GetClass(), SpawnTransform,
		nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!NewManta)
	{
		Result.ErrorCode = TEXT("OPERATION_FAILED");
		Result.ErrorMessage = TEXT("SpawnActorDeferred returned null");
		return Result;
	}
	if (SplinePathProp)
	{
		SplinePathProp->SetObjectPropertyValue_InContainer(NewManta, SplinePath);
	}
	SetMantaFloat(NewManta, TEXT("mSecondsPerLoop"), GetMantaFloat(Source, TEXT("mSecondsPerLoop")));
	const float Loop = FMath::Max(1.0f, GetMantaFloat(Source, TEXT("mSecondsPerLoop"), 900.0f));
	float NewTime = Source->GetCurrentTime() + TimeOffsetSeconds;
	NewTime = FMath::Fmod(FMath::Max(0.0f, NewTime), Loop);
	SetMantaFloat(NewManta, TEXT("mCurrentTime"), NewTime);
	UGameplayStatics::FinishSpawningActor(NewManta, SpawnTransform);

	const TSharedRef<FJsonObject> Detail = MakeShared<FJsonObject>();
	Detail->SetStringField(TEXT("sourceMantaId"), Source->GetPathName());
	Detail->SetNumberField(TEXT("timeOffsetSeconds"), TimeOffsetSeconds);
	Result.ResultDetailJson = WriteCondensedJson(Detail);
	Result.ResultBuildableId = NewManta->GetPathName();
	UE_LOG(LogAIModAI, Display, TEXT("SpawnManta: spawned %s sharing %s's spline (EXPERIMENTAL; session-only)"),
		*NewManta->GetPathName(), *Source->GetPathName());
	Result.bSuccess = true;
	return Result;
}


FString UAIModFunctionLibrary::LogVehiclePathNodesAsJson(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;

	TArray<TSharedPtr<FJsonValue>> NodeJsonArray;
	if (World)
	{
		for (TActorIterator<AFGVehiclePathNode> It(World); It; ++It)
		{
			AFGVehiclePathNode* Node = *It;
			if (!IsValid(Node)) { continue; }

			const TSharedRef<FJsonObject> NodeObject = MakeShared<FJsonObject>();
			NodeObject->SetStringField(TEXT("id"), Node->GetPathName());
			NodeObject->SetStringField(TEXT("class"), Node->GetClass()->GetName());
			NodeObject->SetStringField(TEXT("guid"), Node->GetPathNodeGUID().ToString());
			NodeObject->SetNumberField(TEXT("pathNetworkId"), Node->GetPathNetworkID());
			NodeObject->SetNumberField(TEXT("arrivingConnections"), Node->GetArrivingConnections().Num());
			NodeObject->SetNumberField(TEXT("leavingConnections"), Node->GetLeavingConnections().Num());

			TArray<AFGVehiclePathNode*> Connected;
			Node->GetConnectedNodes(Connected);
			NodeObject->SetNumberField(TEXT("connectedNodeCount"), Connected.Num());

			const FVector Location = Node->GetActorLocation();
			const TSharedRef<FJsonObject> PositionObject = MakeShared<FJsonObject>();
			PositionObject->SetNumberField(TEXT("x"), Location.X);
			PositionObject->SetNumberField(TEXT("y"), Location.Y);
			PositionObject->SetNumberField(TEXT("z"), Location.Z);
			NodeObject->SetObjectField(TEXT("position"), PositionObject);

			NodeJsonArray.Add(MakeShared<FJsonValueObject>(NodeObject));
		}
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetArrayField(TEXT("vehiclePathNodes"), NodeJsonArray);

	FString JsonString;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonString);
	FJsonSerializer::Serialize(RootObject, Writer);

	UE_LOG(LogAIModAI, Display, TEXT("LogVehiclePathNodesAsJson: %d node(s)"), NodeJsonArray.Num());

	return JsonString;
}


FAIModOperationResult UAIModFunctionLibrary::MergeVehiclePathNodes(UObject* WorldContextObject, const FString& SourceNodeId, const FString& DestNodeId)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}
	if (SourceNodeId.IsEmpty() || DestNodeId.IsEmpty())
	{
		return FAIModOperationResult::Failure(TEXT("INVALID_REQUEST"), TEXT("sourceNodeId and destNodeId must both be non-empty strings"));
	}
	if (SourceNodeId == DestNodeId)
	{
		return FAIModOperationResult::Failure(TEXT("INVALID_REQUEST"), TEXT("sourceNodeId and destNodeId must differ"));
	}

	AFGVehiclePathNode* SourceNode = Cast<AFGVehiclePathNode>(FindBuildableById(World, SourceNodeId));
	if (!IsValid(SourceNode))
	{
		return FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No vehicle path node found with id '%s'"), *SourceNodeId));
	}
	AFGVehiclePathNode* DestNode = Cast<AFGVehiclePathNode>(FindBuildableById(World, DestNodeId));
	if (!IsValid(DestNode))
	{
		return FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No vehicle path node found with id '%s'"), *DestNodeId));
	}

	const int32 SourceConnsBefore = SourceNode->GetArrivingConnections().Num() + SourceNode->GetLeavingConnections().Num();
	const int32 DestConnsBefore = DestNode->GetArrivingConnections().Num() + DestNode->GetLeavingConnections().Num();

	// Moves the source node's connections onto DestNode and removes the source
	// node. After this SourceNode is expected to be destroyed - do not touch it.
	SourceNode->MoveConnectionsToNode(DestNode);

	const int32 DestConnsAfter = IsValid(DestNode) ? (DestNode->GetArrivingConnections().Num() + DestNode->GetLeavingConnections().Num()) : -1;

	const TSharedRef<FJsonObject> DetailObject = MakeShared<FJsonObject>();
	DetailObject->SetNumberField(TEXT("sourceConnectionsBefore"), SourceConnsBefore);
	DetailObject->SetNumberField(TEXT("destConnectionsBefore"), DestConnsBefore);
	DetailObject->SetNumberField(TEXT("destConnectionsAfter"), DestConnsAfter);
	DetailObject->SetBoolField(TEXT("sourceStillValid"), IsValid(SourceNode));

	UE_LOG(LogAIModAI, Display, TEXT("MergeVehiclePathNodes: src conns %d -> dest conns %d=>%d (srcStillValid=%s)"),
		SourceConnsBefore, DestConnsBefore, DestConnsAfter, IsValid(SourceNode) ? TEXT("true") : TEXT("false"));

	FString DetailJson;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> DetailWriter =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&DetailJson);
	FJsonSerializer::Serialize(DetailObject, DetailWriter);

	FAIModOperationResult Result = FAIModOperationResult::Success();
	Result.ResultDetailJson = DetailJson;
	return Result;
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

	// Rail vehicles (Locomotive/FreightWagon) drive a AFGRailroadVehicleHologram
	// whose SetHologramLocationAndRotation snaps to a track spline FROM the hit -
	// a bare free-placement hit (no track referenced) reads as "no track under
	// it" -> "Not enough space on track!". Detected here so the
	// hit-building below can point at the nearest track, and so the target-yaw
	// override is skipped (rail orientation follows the track, not the caller).
	const bool bIsRailVehicle = (Cast<AFGRailroadVehicleHologram>(Hologram) != nullptr);

	FHitResult SyntheticHit;
	if (TargetStation)
	{
		// Drone: snap to the station, same synthetic-hit-at-target-actor
		// shape ConstructExtractorOnNode uses for resource nodes -
		// Distance/Component/HitObjectHandle all populated for the same
		// reason documented there (a zero-distance synthetic hit fails a
		// real placement-validation sanity check).
		// Snap the hit at the station's actual DRONE DOCKING location (the port
		// pad), not the station's base actor origin - AFGBuildableDroneHologram::
		// TrySnapToActor leaves mSnappedStation null (=> "Must snap to a
		// Drone Port!") when handed a hit at the base.
		const FVector StationLocation = TargetStation->GetDroneDockingLocation();
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
		PopulateSyntheticTraceRay(SyntheticHit);
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

	// Rail vehicle: re-point the hit at the nearest railroad track spline so
	// the hologram can snap to it (see bIsRailVehicle note above). Not for the
	// drone/station-snap path.
	if (bIsRailVehicle && !TargetStation)
	{
		const FVector Desired(X, Y, (Z > -1000000.0f) ? Z : Character->GetActorLocation().Z);
		AFGBuildableRailroadTrack* BestTrack = nullptr;
		FVector BestPoint = Desired;
		float BestDistSq = TNumericLimits<float>::Max();
		for (TActorIterator<AFGBuildableRailroadTrack> It(World); It; ++It)
		{
			AFGBuildableRailroadTrack* Track = *It;
			if (!IsValid(Track))
			{
				continue;
			}
			USplineComponent* Spline = Track->GetSplineComponent();
			if (!Spline)
			{
				continue;
			}
			const FVector ClosestWorld = Spline->FindLocationClosestToWorldLocation(Desired, ESplineCoordinateSpace::World);
			const float DistSq = FVector::DistSquared(ClosestWorld, Desired);
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				BestPoint = ClosestWorld;
				BestTrack = Track;
			}
		}
		if (BestTrack)
		{
			SyntheticHit = FHitResult();
			SyntheticHit.Location = BestPoint;
			SyntheticHit.ImpactPoint = BestPoint;
			SyntheticHit.Normal = FVector::UpVector;
			SyntheticHit.ImpactNormal = FVector::UpVector;
			SyntheticHit.HitObjectHandle = FActorInstanceHandle(BestTrack);
			SyntheticHit.bBlockingHit = true;
			if (UPrimitiveComponent* TrackPrim = Cast<UPrimitiveComponent>(BestTrack->GetRootComponent()))
			{
				SyntheticHit.Component = TrackPrim;
			}
			PopulateSyntheticTraceRay(SyntheticHit);
			UE_LOG(LogAIModAI, Display, TEXT("ConstructVehicle: rail vehicle snapped hit to track %s at (%.0f,%.0f,%.0f) dist=%.0f"),
				*BestTrack->GetName(), BestPoint.X, BestPoint.Y, BestPoint.Z, FMath::Sqrt(BestDistSq));
		}
		else
		{
			UE_LOG(LogAIModAI, Warning, TEXT("ConstructVehicle: rail vehicle recipe but no AFGBuildableRailroadTrack found near (%.0f,%.0f,%.0f) - placement will likely fail"), Desired.X, Desired.Y, Desired.Z);
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
		const bool bSnapped = Hologram->TrySnapToActor(SyntheticHit);
		UE_LOG(LogAIModAI, Display, TEXT("ConstructVehicle: drone TrySnapToActor(station %s @ dockLoc %.0f,%.0f,%.0f) returned %s"),
			*TargetStation->GetName(), SyntheticHit.Location.X, SyntheticHit.Location.Y, SyntheticHit.Location.Z, bSnapped ? TEXT("true") : TEXT("false"));
	}
	if (bHasTargetYaw && !bIsRailVehicle)
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
	PollState->bHasTargetYaw = bHasTargetYaw && !bIsRailVehicle;
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

		if (PollState->bSnappedToStation)
		{
			// Drone: do NOT re-run UpdateHologramPlacement each tick - it
			// re-traces the synthetic hit and the subsequent CheckValidPlacement
			// clears the station snap (a synthetic hit has no real port-collider
			// overlap), which was observed live to lose a snap that TrySnapToActor
			// had just returned true for. Only (re)assert the snap.
			const bool bPollSnap = PollHologram->TrySnapToActor(PollState->SyntheticHit);
			UE_LOG(LogAIModAI, Display, TEXT("ConstructVehicle drone poll tick %d: TrySnapToActor=%s"),
				PollState->AttemptsTaken, bPollSnap ? TEXT("true") : TEXT("false"));
		}
		else
		{
			PollHologram->UpdateHologramPlacement(PollState->SyntheticHit);
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
		// enum (not float) field this codebase reads this way. Not yet
		// verified at runtime that this correctly resolves - if it fails
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


// Train freight/empty platforms - a train platform
// (AFGBuildableTrainPlatform: Freight/Empty/Liquid docking platform) is NOT a
// free-placed building. AFGTrainPlatformHologram has mRequireSnapToPlatform and
// SNAPS its near-end UFGTrainPlatformConnection onto an existing station/platform's
// free platform connection (which also carries the rail-track link) and owns a
// child rail-track hologram that extends the platform's integrated track. A raw
// placeBuilding therefore fails hard "This must be placed inline with another
// train platform!", and force-placing past that disqualifier would leave a
// DISCONNECTED, non-loading platform (mConnectedPlatformComponents + child track
// unlinked - the same failure class as the old force-linked rail joints). So we
// deliberately do NOT bypass the disqualifier: we drive the real snap. Same
// manual-hologram lineage as ConstructBuildingAtPosition, but WITHOUT the
// position/yaw pin (the snap must own the transform so its connection aligns
// inline), and with a synthetic hit aimed at the target's free platform
// connection so the hologram's own FindOverlappingConnectionComponent /
// SetHologramLocationAndRotation finds it and SnapToConnection wires it up during
// InternalConstructHologram. It only constructs once the "must be inline" (and
// every other hard) disqualifier has cleared - i.e. it genuinely snapped - so a
// failed snap places nothing.
void UAIModFunctionLibrary::ConstructTrainPlatform(UObject* WorldContextObject, const FString& TargetBuildableId, const FString& RecipeClassPath, bool bDryRun, const FVector& ConnectorPos, bool bHasConnectorPos, TFunction<void(const FAIModOperationResult&)> OnComplete)
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

	AFGBuildable* TargetBuildable = FindBuildableById(World, TargetBuildableId);
	if (!TargetBuildable)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No buildable found with id '%s'"), *TargetBuildableId)));
		return;
	}
	AFGBuildableTrainPlatform* TargetPlatform = Cast<AFGBuildableTrainPlatform>(TargetBuildable);
	if (!TargetPlatform)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NOT_A_PLATFORM"), FString::Printf(TEXT("'%s' is not a train station/platform (AFGBuildableTrainPlatform) - freight platforms attach to a station or another platform"), *TargetBuildableId)));
		return;
	}

	// Find a FREE platform connection on the target to snap onto. Prefer the
	// tail (ETPC_Out) for the first platform off a station; a caller pin selects
	// a specific end (e.g. the free end of the last platform when chaining).
	UFGTrainPlatformConnection* TargetConn = nullptr;
	{
		TArray<UFGTrainPlatformConnection*> Conns;
		TargetBuildable->GetComponents<UFGTrainPlatformConnection>(Conns);
		float BestDistSq = TNumericLimits<float>::Max();
		for (UFGTrainPlatformConnection* C : Conns)
		{
			if (!C || C->IsConnected()) { continue; }
			if (bHasConnectorPos)
			{
				const float DistSq = FVector::DistSquared(C->GetComponentLocation(), ConnectorPos);
				if (DistSq < BestDistSq) { BestDistSq = DistSq; TargetConn = C; }
			}
			else
			{
				if (!TargetConn) { TargetConn = C; }
				if (C->GetConnectionType() == ETrainPlatformConnectionType::ETPC_Out) { TargetConn = C; break; }
			}
		}
	}
	if (!TargetConn)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_FREE_PLATFORM_CONNECTION"), FString::Printf(TEXT("'%s' has no free platform connection to attach to (all sides already have platforms?)"), *TargetBuildableId)));
		return;
	}

	const FVector SnapLocation = TargetConn->GetComponentLocation();
	const FVector SnapNormal = TargetConn->GetForwardVector();

	UClass* PlatformRecipeClass = LoadObject<UClass>(nullptr, *RecipeClassPath);
	if (!PlatformRecipeClass || !PlatformRecipeClass->IsChildOf(UFGRecipe::StaticClass()))
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("INVALID_RECIPE"), FString::Printf(TEXT("'%s' did not resolve to a UFGRecipe subclass"), *RecipeClassPath)));
		return;
	}
	const TSubclassOf<UFGRecipe> RecipeClass = PlatformRecipeClass;

	// Aim the synthetic hit AT the target's free platform connection so the
	// platform hologram's overlap-snap finds it. Component/HitObjectHandle point
	// at the target buildable's root primitive (TrySnapToActor keys off the hit's
	// actor/component, same lesson as the rail hologram).
	FHitResult SnapHit;
	SnapHit.Location = SnapLocation;
	SnapHit.ImpactPoint = SnapLocation;
	SnapHit.Normal = SnapNormal;
	SnapHit.ImpactNormal = SnapNormal;
	SnapHit.HitObjectHandle = FActorInstanceHandle(TargetBuildable);
	SnapHit.bBlockingHit = true;
	if (UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(TargetBuildable->GetRootComponent()))
	{
		SnapHit.Component = RootPrim;
	}
	PopulateSyntheticTraceRay(SnapHit);

	if (AController* Controller = Character->GetController())
	{
		const FRotator LookAtTarget = (SnapLocation - Character->GetActorLocation()).Rotation();
		Controller->SetControlRotation(FRotator(LookAtTarget.Pitch, 0.0f, 0.0f));
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
	AFGTrainPlatformHologram* PlatformHologram = Cast<AFGTrainPlatformHologram>(Hologram);
	if (!PlatformHologram)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("HOLOGRAM_SPAWN_FAILED"),
			FString::Printf(TEXT("HotKeyRecipe(%s) did not result in an AFGTrainPlatformHologram (got %s)"),
				*RecipeClassPath, Hologram ? *Hologram->GetClass()->GetName() : TEXT("null"))));
		return;
	}

	BuildGun->GetHitResult() = SnapHit;

	struct FPlatformPollState
	{
		TWeakObjectPtr<AFGTrainPlatformHologram> Hologram;
		TWeakObjectPtr<AFGCharacterPlayer> Character;
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<UFGTrainPlatformConnection> TargetConnection;
		FString RecipeClassPath;
		FString TargetBuildableId;
		FHitResult SnapHit;
		bool bDryRun = false;
		int32 AttemptsRemaining = 120;
		int32 AttemptsTaken = 0;
		TFunction<void(const FAIModOperationResult&)> OnComplete;
	};
	const TSharedRef<FPlatformPollState> PollState = MakeShared<FPlatformPollState>();
	PollState->Hologram = PlatformHologram;
	PollState->Character = Character;
	PollState->World = World;
	PollState->TargetConnection = TargetConn;
	PollState->RecipeClassPath = RecipeClassPath;
	PollState->TargetBuildableId = TargetBuildableId;
	PollState->SnapHit = SnapHit;
	PollState->bDryRun = bDryRun;
	PollState->OnComplete = MoveTemp(OnComplete);

	const TSharedRef<TFunction<void()>> PollFn = MakeShared<TFunction<void()>>();
	*PollFn = [PollState, PollFn]()
	{
		++PollState->AttemptsTaken;

		AFGTrainPlatformHologram* PollHologram = PollState->Hologram.Get();
		UWorld* PollWorld = PollState->World.Get();
		AFGCharacterPlayer* PollCharacter = PollState->Character.Get();
		if (!IsValid(PollHologram) || !PollWorld)
		{
			UE_LOG(LogAIModAI, Warning, TEXT("ConstructTrainPlatform (deferred): hologram or world became invalid while polling (after %d tick(s)) - nothing built"), PollState->AttemptsTaken);
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("HOLOGRAM_INVALIDATED"), TEXT("Hologram or world became invalid while polling")));
			return;
		}

		// Re-assert the look direction at the snap target each tick (same reason
		// as ConstructBuildingAtPosition). NOTE: no position/yaw pin here - the
		// platform snap must own the hologram transform so its connection aligns
		// inline with the target's; pinning would defeat the snap.
		if (IsValid(PollCharacter))
		{
			if (AController* PollController = PollCharacter->GetController())
			{
				const FRotator PollLookAtTarget = (PollState->SnapHit.Location - PollCharacter->GetActorLocation()).Rotation();
				PollController->SetControlRotation(FRotator(PollLookAtTarget.Pitch, 0.0f, 0.0f));
			}
		}

		PollHologram->UpdateHologramPlacement(PollState->SnapHit);

		TArray<TSubclassOf<UFGConstructDisqualifier>> Disqualifiers;
		PollHologram->GetConstructDisqualifiers(Disqualifiers);
		const bool bStillInitializing = Disqualifiers.Contains(TSubclassOf<UFGConstructDisqualifier>(UFGCDInitializing::StaticClass()));

		--PollState->AttemptsRemaining;
		if (bStillInitializing && PollState->AttemptsRemaining > 0)
		{
			PollWorld->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
			return;
		}

		// The ONE hard gate we honor is UFGCDMustAttachToTrainPlatform ("must be
		// placed inline with another train platform") - it means the platform did
		// NOT snap onto the target's connection, i.e. it would be a disconnected,
		// non-loading placement, which we refuse. This is the whole point: we do
		// not bypass the snap. Every OTHER hard disqualifier here is an
		// environment gate (invalid aim, uneven/absent surface, clearance,
		// encroachment) that RPC placement bypasses by design - a floating RPC
		// loop build has no ground under it, same as every other Construct* here.
		// Unaffordable still blocks unless the player enabled UnlimitedResources.
		const bool bUnlimitedResources = UAIModFunctionLibrary::GetAIModConfigBool(PollWorld, TEXT("UnlimitedResources"), false);
		bool bCanConstruct = true;
		bool bNeedsSnap = false;
		TArray<FString> DisqualifierTexts;
		for (const TSubclassOf<UFGConstructDisqualifier>& DisqualifierClass : Disqualifiers)
		{
			const bool bIsSnapRequirement = (DisqualifierClass == UFGCDMustAttachToTrainPlatform::StaticClass());
			const bool bIsUnaffordableBlock = (DisqualifierClass == UFGCDUnaffordable::StaticClass()) && !bUnlimitedResources;
			const bool bIsSoft = UFGConstructDisqualifier::GetIsSoftDisqualifier(DisqualifierClass);
			const bool bBlocks = bIsSnapRequirement || bIsUnaffordableBlock;
			if (bBlocks) { bCanConstruct = false; }
			if (bIsSnapRequirement) { bNeedsSnap = true; }
			DisqualifierTexts.Add(FString::Printf(TEXT("%s (%s%s)"),
				*UFGConstructDisqualifier::GetDisqualifyingText(DisqualifierClass).ToString(),
				bIsSoft ? TEXT("soft") : TEXT("hard"), bBlocks ? TEXT("") : TEXT(", ignored")));
		}
		const FString DisqualifierSummary = DisqualifierTexts.IsEmpty() ? TEXT("<none>") : FString::Join(DisqualifierTexts, TEXT("; "));

		UE_LOG(LogAIModAI, Display, TEXT("ConstructTrainPlatform (deferred, resolved after %d real tick(s)): recipe=%s target=%s canConstruct=%s disqualifiers=[%s]"),
			PollState->AttemptsTaken, *PollState->RecipeClassPath, *PollState->TargetBuildableId, bCanConstruct ? TEXT("true") : TEXT("false"), *DisqualifierSummary);

		if (!bCanConstruct)
		{
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("CANNOT_CONSTRUCT"),
				FString::Printf(TEXT("%s: %s"), bNeedsSnap ? TEXT("platform did NOT snap onto the target connection") : TEXT("blocked (unaffordable)"), *DisqualifierSummary)));
			return;
		}

		// Dry run: it WOULD snap+construct. Report success without building.
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
			UE_LOG(LogAIModAI, Error, TEXT("ConstructTrainPlatform (deferred): lost the build state before constructing - aborting, nothing built"));
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("Lost the build state before constructing")));
			return;
		}

		const FVector ConstructLocation = PollHologram->GetActorLocation();
		PollBuildState->InternalConstructHologram(ConstructionID);

		// Resolve the new platform by proximity, filtered to the recipe's class.
		FString ConstructedBuildableId;
		const TSubclassOf<AFGBuildable> ConstructedBuildableClass = ResolveBuildableClassForRecipe(PollState->RecipeClassPath);
		if (BuildableSubsystem)
		{
			float BestDistSq = TNumericLimits<float>::Max();
			AFGBuildable* BestMatch = nullptr;
			for (AFGBuildable* Candidate : BuildableSubsystem->GetAllBuildablesRef())
			{
				if (!IsValid(Candidate)) { continue; }
				if (ConstructedBuildableClass && !Candidate->IsA(ConstructedBuildableClass)) { continue; }
				const float DistSq = FVector::DistSquared(Candidate->GetActorLocation(), ConstructLocation);
				if (DistSq < BestDistSq) { BestDistSq = DistSq; BestMatch = Candidate; }
			}
			if (BestMatch && BestDistSq < FMath::Square(600.0f)) { ConstructedBuildableId = BestMatch->GetPathName(); }
		}

		// Verify the snap actually took: the target's free connection should now
		// report connected. This is the real success signal (vs a placed-but-
		// disconnected platform, which we design against but confirm anyway).
		const bool bSnapConfirmed = PollState->TargetConnection.IsValid() && PollState->TargetConnection->IsConnected();

		UE_LOG(LogAIModAI, Display, TEXT("ConstructTrainPlatform (deferred, resolved after %d real tick(s)): built recipe=%s at %s id=%s snapConfirmed=%s"),
			PollState->AttemptsTaken, *PollState->RecipeClassPath, *ConstructLocation.ToString(), *ConstructedBuildableId, bSnapConfirmed ? TEXT("true") : TEXT("false"));

		if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }

		if (!bSnapConfirmed)
		{
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("SNAP_UNCONFIRMED"),
				FString::Printf(TEXT("platform constructed (id=%s) but the target connection did not report connected - possible disconnected platform"), *ConstructedBuildableId)));
			return;
		}
		PollState->OnComplete(ConstructedBuildableId.IsEmpty()
			? FAIModOperationResult::Success()
			: FAIModOperationResult::SuccessWithBuildableId(ConstructedBuildableId));
	};

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
}


// Vehicle path segments - researched from source before
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

		// Re-assert the end hit every poll tick - same fix
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


FAIModOperationResult UAIModFunctionLibrary::SetTruckAutopilot(UObject* WorldContextObject, const FString& VehicleId, bool bEnabled, const FString& StationIdsJson, const FString& FuelItemClass, int32 FuelAmount)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}

	if (VehicleId.IsEmpty())
	{
		return FAIModOperationResult::Failure(TEXT("INVALID_REQUEST"), TEXT("vehicleId must be a non-empty string"));
	}

	// Resolve the truck. AFGWheeledVehicle is an AFGVehicle (a pawn), NOT an
	// AFGBuildable, so it is not in the buildable subsystem and FindBuildableById
	// cannot see it - iterate the actor world by GetPathName() the same way
	// SetTrainSelfDriving resolves an AFGTrain.
	AFGWheeledVehicle* TargetVehicle = nullptr;
	for (TActorIterator<AFGWheeledVehicle> It(World); It; ++It)
	{
		if (IsValid(*It) && It->GetPathName() == VehicleId)
		{
			TargetVehicle = *It;
			break;
		}
	}
	if (!TargetVehicle)
	{
		return FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No wheeled vehicle found with id '%s'"), *VehicleId));
	}

	AFGWheeledVehicleIdentifier* Identifier = TargetVehicle->GetVehicleIdentifier();
	if (!IsValid(Identifier))
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("Vehicle has no AFGWheeledVehicleIdentifier (route/autopilot state lives on the identifier)"));
	}

	// Optionally overwrite the route from a JSON array of docking-station
	// buildable ids. Each stop resolves to that station's docking path node
	// GUID (the waypoint the autopilot actually navigates to).
	int32 RouteWaypointsSet = -1;
	if (!StationIdsJson.IsEmpty())
	{
		TArray<TSharedPtr<FJsonValue>> StationIdArray;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(StationIdsJson);
		if (!FJsonSerializer::Deserialize(Reader, StationIdArray))
		{
			return FAIModOperationResult::Failure(TEXT("INVALID_REQUEST"), TEXT("stationIds must be a JSON array of docking-station buildable id strings"));
		}

		TArray<FGuid> RouteGuids;
		for (const TSharedPtr<FJsonValue>& Value : StationIdArray)
		{
			FString StationId;
			if (!Value.IsValid() || !Value->TryGetString(StationId) || StationId.IsEmpty())
			{
				return FAIModOperationResult::Failure(TEXT("INVALID_REQUEST"), TEXT("every stationIds entry must be a non-empty string"));
			}

			AFGBuildableDockingStation* Station = Cast<AFGBuildableDockingStation>(FindBuildableById(World, StationId));
			if (!IsValid(Station))
			{
				return FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"),
					FString::Printf(TEXT("No docking station found with id '%s'"), *StationId));
			}

			AFGVehiclePathNode* DockNode = Station->GetDockingPathNode();
			if (!IsValid(DockNode))
			{
				return FAIModOperationResult::Failure(TEXT("INVALID_TARGET"),
					FString::Printf(TEXT("Docking station '%s' has no docking path node yet (GetDockingPathNode() null)"), *StationId));
			}

			const FGuid NodeGuid = DockNode->GetPathNodeGUID();
			if (!NodeGuid.IsValid())
			{
				return FAIModOperationResult::Failure(TEXT("INVALID_TARGET"),
					FString::Printf(TEXT("Docking station '%s' docking node has an invalid GUID"), *StationId));
			}
			RouteGuids.Add(NodeGuid);
		}

		Identifier->SetVehicleRoute(RouteGuids);
		RouteWaypointsSet = RouteGuids.Num();
	}

	// Optionally load fuel into the vehicle's fuel inventory. A freshly
	// constructed truck has no fuel, and the autopilot will not drive (and may
	// refuse to enable) without it. FuelItemClass empty = don't touch fuel.
	int32 FuelActuallyAdded = -1;
	FString FuelAddNote;
	if (!FuelItemClass.IsEmpty() && FuelAmount > 0)
	{
		UClass* FuelClassResolved = LoadObject<UClass>(nullptr, *FuelItemClass);
		if (!FuelClassResolved || !FuelClassResolved->IsChildOf(UFGItemDescriptor::StaticClass()))
		{
			return FAIModOperationResult::Failure(TEXT("INVALID_REQUEST"),
				FString::Printf(TEXT("fuelItemClass '%s' did not resolve to a UFGItemDescriptor subclass"), *FuelItemClass));
		}
		UFGInventoryComponent* FuelInv = TargetVehicle->GetFuelInventory();
		if (!IsValid(FuelInv))
		{
			FuelAddNote = TEXT("vehicle has no fuel inventory component");
		}
		else
		{
			const TSubclassOf<UFGItemDescriptor> FuelDesc = FuelClassResolved;
			// AddStack respects the inventory's allowed-item filter, so a fuel
			// the truck can't burn returns 0 added (reported, not fatal).
			FuelActuallyAdded = FuelInv->AddStack(FInventoryStack(FuelAmount, FuelDesc), /*allowPartialAdd*/ true);
		}
	}

	// Make sure the truck is registered on the path segment under it before
	// enabling, otherwise the autopilot reports NotOnPath. Harmless when the
	// truck is already on a segment or when disabling.
	//
	// Auto-detection (UpdateCurrentVehiclePathSegmentFromVehicleLocation) relies
	// on the segment's virtualized collision being streamed in / significant and
	// was observed to leave GetCurrentVehiclePathSegment() null for an idle
	// just-built truck. So if it comes back null, explicitly find the nearest
	// AFGVehiclePathSegment spline and pin the truck onto it with the public
	// SetCurrentVehiclePathSegment().
	float NearestSegmentDist = -1.0f;
	int32 SegmentCount = 0;
	int32 ValidSegmentsForPreset = 0;
	bool bCurrentSegmentValidForPreset = false;
	if (bEnabled)
	{
		// RPC-built path segments may never have had their per-vehicle-type
		// traversability/validation data computed (it is significance-gated and
		// streamed over frames). Without it CanVehicleTraverseSegment is false
		// and the autopilot finds no route (idles with error None). Force an
		// immediate rebuild on every segment so the network is traversable now.
		UFGVehiclePathPreset* TruckPreset = TargetVehicle->GetVehiclePathPreset();
		for (TActorIterator<AFGVehiclePathSegment> It(World); It; ++It)
		{
			AFGVehiclePathSegment* Segment = *It;
			if (!IsValid(Segment)) { continue; }
			++SegmentCount;
			Segment->ImmediateRebuildPathValidationData();
			if (TruckPreset && Segment->IsPathValidForPreset(TruckPreset))
			{
				++ValidSegmentsForPreset;
			}
		}

		TargetVehicle->UpdateCurrentVehiclePathSegmentFromVehicleLocation();
		if (TargetVehicle->GetCurrentVehiclePathSegment() == nullptr)
		{
			const FVector TruckLoc = TargetVehicle->GetActorLocation();
			AFGVehiclePathSegment* BestSegment = nullptr;
			float BestDistSq = TNumericLimits<float>::Max();
			for (TActorIterator<AFGVehiclePathSegment> It(World); It; ++It)
			{
				AFGVehiclePathSegment* Segment = *It;
				if (!IsValid(Segment)) { continue; }
				USplineComponent* Spline = Segment->GetSplineComponent();
				if (!Spline) { continue; }
				const FVector Closest = Spline->FindLocationClosestToWorldLocation(TruckLoc, ESplineCoordinateSpace::World);
				const float DistSq = FVector::DistSquared(Closest, TruckLoc);
				if (DistSq < BestDistSq)
				{
					BestDistSq = DistSq;
					BestSegment = Segment;
				}
			}
			if (BestSegment)
			{
				NearestSegmentDist = FMath::Sqrt(BestDistSq);
				// Only pin if the truck is genuinely close to the segment (a
				// couple of foundation cells) - past that it is not really "on"
				// the path and pinning would be a lie.
				if (NearestSegmentDist <= 800.0f)
				{
					TargetVehicle->SetCurrentVehiclePathSegment(BestSegment);
				}
			}
		}
	}

	if (bEnabled)
	{
		UFGVehiclePathPreset* TruckPreset = TargetVehicle->GetVehiclePathPreset();
		AFGVehiclePathSegment* CurSeg = TargetVehicle->GetCurrentVehiclePathSegment();
		bCurrentSegmentValidForPreset = (CurSeg && TruckPreset) ? CurSeg->IsPathValidForPreset(TruckPreset) : false;
	}

	const bool bCanEnable = Identifier->CanEnableAutopilot();
	Identifier->SetAutopilotEnabled(bEnabled);

	// Give the autopilot an explicit first destination. A bare SetAutopilotEnabled
	// was observed to leave the truck armed but idle (error None) - setting the
	// current target waypoint to the first route stop kicks route calculation.
	if (bEnabled && Identifier->IsAutopilotEnabled() && Identifier->GetVehicleRoute().Num() >= 2)
	{
		Identifier->SetCurrentTargetWaypoint(0);
	}

	// Full diagnostic detail, always returned. Like setTrainSelfDriving, a
	// truck that refuses to drive is real configuration state for the caller
	// to act on (add fuel, fix the path), not a failure of this RPC - so we
	// no longer hard-fail when the enable flag doesn't stick, we report why.
	const bool bOnPath = (TargetVehicle->GetCurrentVehiclePathSegment() != nullptr);
	const bool bHasFuel = TargetVehicle->HasFuel();
	const bool bAutopilotAvailableForType = AFGWheeledVehicle::IsAutopilotAvailableForVehicleType(TargetVehicle->GetClass());

	const TSharedRef<FJsonObject> DetailObject = MakeShared<FJsonObject>();
	DetailObject->SetBoolField(TEXT("autopilotEnabled"), Identifier->IsAutopilotEnabled());
	DetailObject->SetBoolField(TEXT("autopilotTookEffect"), Identifier->IsAutopilotEnabled() == bEnabled);
	DetailObject->SetBoolField(TEXT("canEnableAutopilot"), bCanEnable);
	DetailObject->SetBoolField(TEXT("onPath"), bOnPath);
	DetailObject->SetBoolField(TEXT("hasFuel"), bHasFuel);
	DetailObject->SetBoolField(TEXT("autopilotAvailableForType"), bAutopilotAvailableForType);
	DetailObject->SetStringField(TEXT("autopilotError"), VehicleAutopilotErrorToString(Identifier->GetAutopilotErrorStatus()));
	DetailObject->SetNumberField(TEXT("routeWaypoints"), Identifier->GetVehicleRoute().Num());
	if (RouteWaypointsSet >= 0)
	{
		DetailObject->SetNumberField(TEXT("routeWaypointsSet"), RouteWaypointsSet);
	}
	if (FuelActuallyAdded >= 0)
	{
		DetailObject->SetNumberField(TEXT("fuelAdded"), FuelActuallyAdded);
	}
	if (!FuelAddNote.IsEmpty())
	{
		DetailObject->SetStringField(TEXT("fuelNote"), FuelAddNote);
	}
	if (NearestSegmentDist >= 0.0f)
	{
		DetailObject->SetNumberField(TEXT("nearestSegmentDist"), NearestSegmentDist);
	}
	if (bEnabled)
	{
		DetailObject->SetNumberField(TEXT("segmentCount"), SegmentCount);
		DetailObject->SetNumberField(TEXT("validSegmentsForPreset"), ValidSegmentsForPreset);
		DetailObject->SetBoolField(TEXT("currentSegmentValidForPreset"), bCurrentSegmentValidForPreset);
		DetailObject->SetNumberField(TEXT("currentTargetWaypointIndex"), Identifier->GetCurrentTargetWaypointIndex());
		DetailObject->SetBoolField(TEXT("vehicleInProxyMode"), TargetVehicle->IsVehicleInProxyMode());
		if (UFGVehicleAutopilotComponent* Autopilot = TargetVehicle->GetVehicleAutopilotComponent())
		{
			DetailObject->SetBoolField(TEXT("shouldTickAutopilot"), Autopilot->ShouldTickAutopilotComponent());
			DetailObject->SetNumberField(TEXT("autopilotForwardSpeed"), Autopilot->GetCurrentForwardSpeed());
		}
		else
		{
			DetailObject->SetStringField(TEXT("autopilotComponentNote"), TEXT("GetVehicleAutopilotComponent() returned null"));
		}
	}

	UE_LOG(LogAIModAI, Display, TEXT("SetTruckAutopilot: vehicle=%s enabled=%s took=%s canEnable=%s onPath=%s hasFuel=%s availForType=%s error=%s routeLen=%d fuelAdded=%d"),
		*Identifier->GetVehicleName().ToString(), bEnabled ? TEXT("true") : TEXT("false"),
		(Identifier->IsAutopilotEnabled() == bEnabled) ? TEXT("true") : TEXT("false"),
		bCanEnable ? TEXT("true") : TEXT("false"), bOnPath ? TEXT("true") : TEXT("false"),
		bHasFuel ? TEXT("true") : TEXT("false"), bAutopilotAvailableForType ? TEXT("true") : TEXT("false"),
		*VehicleAutopilotErrorToString(Identifier->GetAutopilotErrorStatus()), Identifier->GetVehicleRoute().Num(), FuelActuallyAdded);

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
