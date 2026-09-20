// Copyright Epic Games, Inc. All Rights Reserved.

#include "AIModFunctionLibraryInternal.h"
using namespace AIModInternal;

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


// See LogWaterVolumesAsJson's doc comment in the header for the real
// AFGWaterVolume/IFGExtractableResourceInterface sourcing and why
// world.resourceNodes could never see these.
FString UAIModFunctionLibrary::LogWaterVolumesAsJson(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		UE_LOG(LogAIModAI, Warning, TEXT("LogWaterVolumesAsJson: no valid world context"));
		return TEXT("{}");
	}

	TArray<TSharedPtr<FJsonValue>> VolumesJsonArray;
	for (TActorIterator<AFGWaterVolume> It(World); It; ++It)
	{
		AFGWaterVolume* Volume = *It;
		if (!IsValid(Volume))
		{
			continue;
		}

		const TSharedRef<FJsonObject> PositionObject = MakeShared<FJsonObject>();
		const FVector Location = Volume->GetActorLocation();
		PositionObject->SetNumberField(TEXT("x"), Location.X);
		PositionObject->SetNumberField(TEXT("y"), Location.Y);
		PositionObject->SetNumberField(TEXT("z"), Location.Z);

		const FBox Bounds = Volume->GetComponentsBoundingBox(false);
		const TSharedRef<FJsonObject> BoundsObject = MakeShared<FJsonObject>();
		const TSharedRef<FJsonObject> BoundsMinObject = MakeShared<FJsonObject>();
		BoundsMinObject->SetNumberField(TEXT("x"), Bounds.Min.X);
		BoundsMinObject->SetNumberField(TEXT("y"), Bounds.Min.Y);
		BoundsMinObject->SetNumberField(TEXT("z"), Bounds.Min.Z);
		const TSharedRef<FJsonObject> BoundsMaxObject = MakeShared<FJsonObject>();
		BoundsMaxObject->SetNumberField(TEXT("x"), Bounds.Max.X);
		BoundsMaxObject->SetNumberField(TEXT("y"), Bounds.Max.Y);
		BoundsMaxObject->SetNumberField(TEXT("z"), Bounds.Max.Z);
		const FVector BoundsSize = Bounds.GetSize();
		const TSharedRef<FJsonObject> BoundsSizeObject = MakeShared<FJsonObject>();
		BoundsSizeObject->SetNumberField(TEXT("x"), BoundsSize.X);
		BoundsSizeObject->SetNumberField(TEXT("y"), BoundsSize.Y);
		BoundsSizeObject->SetNumberField(TEXT("z"), BoundsSize.Z);
		BoundsObject->SetObjectField(TEXT("min"), BoundsMinObject);
		BoundsObject->SetObjectField(TEXT("max"), BoundsMaxObject);
		BoundsObject->SetObjectField(TEXT("size"), BoundsSizeObject);

		const TSubclassOf<UFGResourceDescriptor> ResourceClass = Volume->GetResourceClass();

		const TSharedRef<FJsonObject> VolumeObject = MakeShared<FJsonObject>();
		VolumeObject->SetStringField(TEXT("id"), Volume->GetPathName());
		VolumeObject->SetObjectField(TEXT("position"), PositionObject);
		VolumeObject->SetObjectField(TEXT("bounds"), BoundsObject);
		VolumeObject->SetBoolField(TEXT("isOccupied"), Volume->IsOccupied());
		VolumeObject->SetBoolField(TEXT("canBecomeOccupied"), Volume->CanBecomeOccupied());
		VolumeObject->SetBoolField(TEXT("canPlaceResourceExtractor"), Volume->CanPlaceResourceExtractor());
		VolumeObject->SetBoolField(TEXT("hasAnyResources"), Volume->HasAnyResources());
		VolumeObject->SetStringField(TEXT("resourceClass"), ResourceClass ? ResourceClass->GetPathName() : FString());

		VolumesJsonArray.Add(MakeShared<FJsonValueObject>(VolumeObject));
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetArrayField(TEXT("waterVolumes"), VolumesJsonArray);

	const FString JsonString = WriteCondensedJson(RootObject);

	UE_LOG(LogAIModAI, Display, TEXT("LogWaterVolumesAsJson: %d water volume(s)"), VolumesJsonArray.Num());

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
	return LogBuildablesAsJsonFiltered(WorldContextObject, TArray<FString>(), false, FVector::ZeroVector, FVector::ZeroVector);
}



FString UAIModFunctionLibrary::LogBuildablesAsJsonFiltered(UObject* WorldContextObject, const TArray<FString>& IdSubstrings, bool bBoundsSet, const FVector& BoundsMin, const FVector& BoundsMax)
{
	const TArray<FAIModBuildableTelemetry> Buildables = GetBuildableTelemetry(WorldContextObject);

	TArray<TSharedPtr<FJsonValue>> BuildableJsonArray;
	BuildableJsonArray.Reserve(Buildables.Num());

	for (const FAIModBuildableTelemetry& Buildable : Buildables)
	{
		if (!AIModTelemetryRowPasses(Buildable.Id, Buildable.Position, IdSubstrings, bBoundsSet, BoundsMin, BoundsMax))
		{
			continue;
		}
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

		// World-space clearance AABB (min/max/size). Absent when the class
		// has no CDO clearance data - callers key off its presence.
		if (Buildable.bHasBounds)
		{
			const FVector BoundsSize = Buildable.BoundsMax - Buildable.BoundsMin;
			const TSharedRef<FJsonObject> BoundsObject = MakeShared<FJsonObject>();
			const TSharedRef<FJsonObject> BoundsMinObject = MakeShared<FJsonObject>();
			BoundsMinObject->SetNumberField(TEXT("x"), Buildable.BoundsMin.X);
			BoundsMinObject->SetNumberField(TEXT("y"), Buildable.BoundsMin.Y);
			BoundsMinObject->SetNumberField(TEXT("z"), Buildable.BoundsMin.Z);
			BoundsObject->SetObjectField(TEXT("min"), BoundsMinObject);
			const TSharedRef<FJsonObject> BoundsMaxObject = MakeShared<FJsonObject>();
			BoundsMaxObject->SetNumberField(TEXT("x"), Buildable.BoundsMax.X);
			BoundsMaxObject->SetNumberField(TEXT("y"), Buildable.BoundsMax.Y);
			BoundsMaxObject->SetNumberField(TEXT("z"), Buildable.BoundsMax.Z);
			BoundsObject->SetObjectField(TEXT("max"), BoundsMaxObject);
			const TSharedRef<FJsonObject> BoundsSizeObject = MakeShared<FJsonObject>();
			BoundsSizeObject->SetNumberField(TEXT("x"), BoundsSize.X);
			BoundsSizeObject->SetNumberField(TEXT("y"), BoundsSize.Y);
			BoundsSizeObject->SetNumberField(TEXT("z"), BoundsSize.Z);
			BoundsObject->SetObjectField(TEXT("size"), BoundsSizeObject);
			BuildableObject->SetObjectField(TEXT("bounds"), BoundsObject);
		}

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


// world.vehicles - AFGVehicle is not an AFGBuildable (AFGDriveablePawn, a
// separate hierarchy), so a constructed vehicle is completely invisible to
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
	return LogFactoryConnectionsAsJsonFiltered(WorldContextObject, TArray<FString>(), false, FVector::ZeroVector, FVector::ZeroVector);
}


FString UAIModFunctionLibrary::LogFactoryConnectionsAsJsonFiltered(UObject* WorldContextObject, const TArray<FString>& IdSubstrings, bool bBoundsSet, const FVector& BoundsMin, const FVector& BoundsMax)
{
	const TArray<FAIModFactoryConnectionTelemetry> Connections = GetFactoryConnectionTelemetry(WorldContextObject);

	TArray<TSharedPtr<FJsonValue>> ConnectionJsonArray;
	ConnectionJsonArray.Reserve(Connections.Num());

	for (const FAIModFactoryConnectionTelemetry& Connection : Connections)
	{
		if (!AIModTelemetryRowPasses(Connection.OwnerBuildableId, Connection.Position, IdSubstrings, bBoundsSet, BoundsMin, BoundsMax))
		{
			continue;
		}
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

	// FactoryGame spells the interface "IFGUseableInterface" (NOT
	// "IFGUsableInterface" - grep both spellings), and AFGResourceNodeBase
	// (FGResourceNodeBase.h:93) implements it, so the same
	// GetBestUsableActor() GetTargetedManufacturer trusts also finds
	// resource nodes, exactly matching the game's own "Press E to start
	// mining" prompt. Preferred over a hand-rolled view-angle-cone heuristic.
	AFGResourceNode* Node = Cast<AFGResourceNode>(Character->GetBestUsableActor());
	if (!Node)
	{
		// Not an error - the player just isn't looking at a resource
		// node right now. Empty Id signals "none" to the caller.
		return FAIModResourceNodeTelemetry();
	}

	return MakeResourceNodeTelemetry(Node);
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
	// wanting a "combined" carried+Depot count (e.g. verifying an
	// inventory-refund bug needs a reliable before/after carried count).
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
				// UPROPERTY meta) - most buildings
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
