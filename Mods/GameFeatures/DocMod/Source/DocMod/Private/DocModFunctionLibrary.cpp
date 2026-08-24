// Copyright Epic Games, Inc. All Rights Reserved.

#include "DocModFunctionLibrary.h"
#include "DocMod.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Resources/FGResourceNode.h"
#include "Resources/FGItemDescriptor.h"
#include "FGBuildableSubsystem.h"
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableFactory.h"
#include "Buildables/FGBuildableManufacturer.h"
#include "FGRecipe.h"
#include "FGInventoryComponent.h"
#include "FGFactoryConnectionComponent.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	// AFGResourceNodeManager exists but its node array has no public
	// getter and its .cpp is a stub (see docs/resource-node-research.md),
	// so a plain actor-iterator world scan is the only evidenced way to
	// enumerate nodes right now. Fine for a debug/Phase-4 entry point;
	// CLAUDE.md steers production code toward a subsystem/event-driven
	// approach instead of scanning every frame.
	TArray<FDocModResourceNodeTelemetry> CollectResourceNodeTelemetry(UWorld* World)
	{
		TArray<FDocModResourceNodeTelemetry> Nodes;
		for (TActorIterator<AFGResourceNode> It(World); It; ++It)
		{
			AFGResourceNode* Node = *It;
			if (!IsValid(Node))
			{
				continue;
			}

			const TSubclassOf<UFGResourceDescriptor> ResourceClass = Node->GetResourceClass();

			FDocModResourceNodeTelemetry Telemetry;
			// Session-local only - see FDocModResourceNodeTelemetry's comment.
			Telemetry.Id = Node->GetPathName();
			Telemetry.Resource = ResourceClass ? UFGItemDescriptor::GetItemName(ResourceClass).ToString() : TEXT("Unknown");
			Telemetry.ResourceClass = ResourceClass ? ResourceClass->GetPathName() : FString();
			Telemetry.Purity = Node->GetResourcePurityText().ToString();
			Telemetry.Position = Node->GetActorLocation();
			Telemetry.bOccupied = Node->IsOccupied();

			Nodes.Add(MoveTemp(Telemetry));
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

		UE_LOG(LogDocModAI, Warning, TEXT("CollectAllBuildables: AFGBuildableSubsystem unavailable, falling back to TActorIterator"));
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

	TArray<FDocModBuildableTelemetry> CollectBuildableTelemetry(UWorld* World)
	{
		TArray<FDocModBuildableTelemetry> Result;
		for (AFGBuildable* Buildable : CollectAllBuildables(World))
		{
			if (!IsValid(Buildable))
			{
				continue;
			}

			FDocModBuildableTelemetry Telemetry;
			Telemetry.Id = Buildable->GetPathName();
			Telemetry.BuildableClass = Buildable->GetClass()->GetPathName();
			Telemetry.Position = Buildable->GetActorLocation();
			Telemetry.Rotation = Buildable->GetActorRotation();
			Result.Add(MoveTemp(Telemetry));
		}
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

	TArray<FDocModInventoryItemTelemetry> CollectInventoryTelemetry(UFGInventoryComponent* Inventory)
	{
		TArray<FDocModInventoryItemTelemetry> Result;
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

			FDocModInventoryItemTelemetry ItemTelemetry;
			ItemTelemetry.ItemClass = ItemClass->GetPathName();
			ItemTelemetry.ItemName = UFGItemDescriptor::GetItemName(ItemClass).ToString();
			ItemTelemetry.Count = Stack.NumItems;
			Result.Add(MoveTemp(ItemTelemetry));
		}
		return Result;
	}

	TArray<FDocModManufacturerTelemetry> CollectManufacturerTelemetry(UWorld* World)
	{
		TArray<FDocModManufacturerTelemetry> Result;
		for (TActorIterator<AFGBuildableManufacturer> It(World); It; ++It)
		{
			AFGBuildableManufacturer* Manufacturer = *It;
			if (!IsValid(Manufacturer))
			{
				continue;
			}

			FDocModManufacturerTelemetry Telemetry;
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

			Result.Add(MoveTemp(Telemetry));
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

	TArray<FDocModFactoryConnectionTelemetry> CollectFactoryConnectionTelemetry(UWorld* World)
	{
		TArray<FDocModFactoryConnectionTelemetry> Result;
		for (TActorIterator<AFGBuildableFactory> It(World); It; ++It)
		{
			AFGBuildableFactory* Factory = *It;
			if (!IsValid(Factory))
			{
				continue;
			}

			const FString OwnerId = Factory->GetPathName();
			for (UFGFactoryConnectionComponent* Connection : Factory->GetConnectionComponents())
			{
				if (!IsValid(Connection))
				{
					continue;
				}

				FDocModFactoryConnectionTelemetry Telemetry;
				Telemetry.OwnerBuildableId = OwnerId;
				Telemetry.Direction = FactoryConnectionDirectionToString(Connection->GetDirection());
				Telemetry.bConnected = Connection->IsConnected();

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

				Result.Add(MoveTemp(Telemetry));
			}
		}
		return Result;
	}

	// Id is the session-local GetPathName() (see DocModTelemetryTypes.h) -
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

FString UDocModFunctionLibrary::GetInterfaceVersion()
{
	UE_LOG(LogDocModAI, Verbose, TEXT("GetInterfaceVersion called"));
	return TEXT("0.1.0");
}

TArray<FDocModResourceNodeTelemetry> UDocModFunctionLibrary::GetResourceNodeTelemetry(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		UE_LOG(LogDocModAI, Warning, TEXT("GetResourceNodeTelemetry: no valid world context"));
		return {};
	}

	return CollectResourceNodeTelemetry(World);
}

void UDocModFunctionLibrary::LogResourceNodes(UObject* WorldContextObject)
{
	const TArray<FDocModResourceNodeTelemetry> Nodes = GetResourceNodeTelemetry(WorldContextObject);

	for (const FDocModResourceNodeTelemetry& Node : Nodes)
	{
		UE_LOG(LogDocModAI, Display, TEXT("ResourceNode: id=%s resource=\"%s\" purity=%s pos=(%.1f, %.1f, %.1f) occupied=%s"),
			*Node.Id, *Node.Resource, *Node.Purity, Node.Position.X, Node.Position.Y, Node.Position.Z,
			Node.bOccupied ? TEXT("true") : TEXT("false"));
	}

	UE_LOG(LogDocModAI, Display, TEXT("LogResourceNodes: enumerated %d resource node(s)"), Nodes.Num());
}

FString UDocModFunctionLibrary::LogResourceNodesAsJson(UObject* WorldContextObject)
{
	const TArray<FDocModResourceNodeTelemetry> Nodes = GetResourceNodeTelemetry(WorldContextObject);

	TArray<TSharedPtr<FJsonValue>> NodeJsonArray;
	NodeJsonArray.Reserve(Nodes.Num());

	for (const FDocModResourceNodeTelemetry& Node : Nodes)
	{
		const TSharedRef<FJsonObject> NodeObject = MakeShared<FJsonObject>();
		NodeObject->SetStringField(TEXT("id"), Node.Id);
		NodeObject->SetStringField(TEXT("resource"), Node.Resource);
		NodeObject->SetStringField(TEXT("resourceClass"), Node.ResourceClass);
		NodeObject->SetStringField(TEXT("purity"), Node.Purity);

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
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(RootObject, Writer);

	UE_LOG(LogDocModAI, Display, TEXT("LogResourceNodesAsJson: %s"), *JsonString);

	return JsonString;
}

TArray<FDocModBuildableTelemetry> UDocModFunctionLibrary::GetBuildableTelemetry(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		UE_LOG(LogDocModAI, Warning, TEXT("GetBuildableTelemetry: no valid world context"));
		return {};
	}

	return CollectBuildableTelemetry(World);
}

void UDocModFunctionLibrary::LogBuildables(UObject* WorldContextObject)
{
	const TArray<FDocModBuildableTelemetry> Buildables = GetBuildableTelemetry(WorldContextObject);

	for (const FDocModBuildableTelemetry& Buildable : Buildables)
	{
		UE_LOG(LogDocModAI, Display, TEXT("Buildable: id=%s class=%s pos=(%.1f, %.1f, %.1f) rot=(%.1f, %.1f, %.1f)"),
			*Buildable.Id, *Buildable.BuildableClass, Buildable.Position.X, Buildable.Position.Y, Buildable.Position.Z,
			Buildable.Rotation.Pitch, Buildable.Rotation.Yaw, Buildable.Rotation.Roll);
	}

	UE_LOG(LogDocModAI, Display, TEXT("LogBuildables: enumerated %d buildable(s)"), Buildables.Num());
}

FString UDocModFunctionLibrary::LogBuildablesAsJson(UObject* WorldContextObject)
{
	const TArray<FDocModBuildableTelemetry> Buildables = GetBuildableTelemetry(WorldContextObject);

	TArray<TSharedPtr<FJsonValue>> BuildableJsonArray;
	BuildableJsonArray.Reserve(Buildables.Num());

	for (const FDocModBuildableTelemetry& Buildable : Buildables)
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
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(RootObject, Writer);

	UE_LOG(LogDocModAI, Display, TEXT("LogBuildablesAsJson: %s"), *JsonString);

	return JsonString;
}

TArray<FDocModManufacturerTelemetry> UDocModFunctionLibrary::GetManufacturerTelemetry(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		UE_LOG(LogDocModAI, Warning, TEXT("GetManufacturerTelemetry: no valid world context"));
		return {};
	}

	return CollectManufacturerTelemetry(World);
}

void UDocModFunctionLibrary::LogManufacturers(UObject* WorldContextObject)
{
	const TArray<FDocModManufacturerTelemetry> Manufacturers = GetManufacturerTelemetry(WorldContextObject);

	for (const FDocModManufacturerTelemetry& Manufacturer : Manufacturers)
	{
		UE_LOG(LogDocModAI, Display,
			TEXT("Manufacturer: id=%s class=%s recipe=\"%s\" clock=%.0f%% status=%s progress=%.2f productivity=%.2f inputItems=%d outputItems=%d"),
			*Manufacturer.Id, *Manufacturer.BuildableClass, *Manufacturer.Recipe, Manufacturer.ClockSpeedPercent,
			*Manufacturer.ProductionStatus, Manufacturer.ProductionProgress, Manufacturer.Productivity,
			Manufacturer.InputInventory.Num(), Manufacturer.OutputInventory.Num());
	}

	UE_LOG(LogDocModAI, Display, TEXT("LogManufacturers: enumerated %d manufacturer(s)"), Manufacturers.Num());
}

namespace
{
	TSharedRef<FJsonObject> InventoryItemToJson(const FDocModInventoryItemTelemetry& Item)
	{
		const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("itemClass"), Item.ItemClass);
		Object->SetStringField(TEXT("itemName"), Item.ItemName);
		Object->SetNumberField(TEXT("count"), Item.Count);
		return Object;
	}

	TArray<TSharedPtr<FJsonValue>> InventoryToJsonArray(const TArray<FDocModInventoryItemTelemetry>& Items)
	{
		TArray<TSharedPtr<FJsonValue>> Array;
		Array.Reserve(Items.Num());
		for (const FDocModInventoryItemTelemetry& Item : Items)
		{
			Array.Add(MakeShared<FJsonValueObject>(InventoryItemToJson(Item)));
		}
		return Array;
	}
}

FString UDocModFunctionLibrary::LogManufacturersAsJson(UObject* WorldContextObject)
{
	const TArray<FDocModManufacturerTelemetry> Manufacturers = GetManufacturerTelemetry(WorldContextObject);

	TArray<TSharedPtr<FJsonValue>> ManufacturerJsonArray;
	ManufacturerJsonArray.Reserve(Manufacturers.Num());

	for (const FDocModManufacturerTelemetry& Manufacturer : Manufacturers)
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

		ManufacturerJsonArray.Add(MakeShared<FJsonValueObject>(ManufacturerObject));
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetArrayField(TEXT("manufacturers"), ManufacturerJsonArray);

	FString JsonString;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(RootObject, Writer);

	UE_LOG(LogDocModAI, Display, TEXT("LogManufacturersAsJson: %s"), *JsonString);

	return JsonString;
}

TArray<FDocModFactoryConnectionTelemetry> UDocModFunctionLibrary::GetFactoryConnectionTelemetry(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		UE_LOG(LogDocModAI, Warning, TEXT("GetFactoryConnectionTelemetry: no valid world context"));
		return {};
	}

	return CollectFactoryConnectionTelemetry(World);
}

void UDocModFunctionLibrary::LogFactoryConnections(UObject* WorldContextObject)
{
	const TArray<FDocModFactoryConnectionTelemetry> Connections = GetFactoryConnectionTelemetry(WorldContextObject);

	for (const FDocModFactoryConnectionTelemetry& Connection : Connections)
	{
		UE_LOG(LogDocModAI, Display, TEXT("Connection: owner=%s direction=%s connected=%s connectedTo=%s"),
			*Connection.OwnerBuildableId, *Connection.Direction, Connection.bConnected ? TEXT("true") : TEXT("false"),
			*Connection.ConnectedBuildableId);
	}

	UE_LOG(LogDocModAI, Display, TEXT("LogFactoryConnections: enumerated %d connection point(s)"), Connections.Num());
}

FString UDocModFunctionLibrary::LogFactoryConnectionsAsJson(UObject* WorldContextObject)
{
	const TArray<FDocModFactoryConnectionTelemetry> Connections = GetFactoryConnectionTelemetry(WorldContextObject);

	TArray<TSharedPtr<FJsonValue>> ConnectionJsonArray;
	ConnectionJsonArray.Reserve(Connections.Num());

	for (const FDocModFactoryConnectionTelemetry& Connection : Connections)
	{
		const TSharedRef<FJsonObject> ConnectionObject = MakeShared<FJsonObject>();
		ConnectionObject->SetStringField(TEXT("ownerBuildableId"), Connection.OwnerBuildableId);
		ConnectionObject->SetStringField(TEXT("direction"), Connection.Direction);
		ConnectionObject->SetBoolField(TEXT("connected"), Connection.bConnected);
		ConnectionObject->SetStringField(TEXT("connectedBuildableId"), Connection.ConnectedBuildableId);

		ConnectionJsonArray.Add(MakeShared<FJsonValueObject>(ConnectionObject));
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetArrayField(TEXT("connections"), ConnectionJsonArray);

	FString JsonString;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(RootObject, Writer);

	UE_LOG(LogDocModAI, Display, TEXT("LogFactoryConnectionsAsJson: %s"), *JsonString);

	return JsonString;
}

FDocModOperationResult UDocModFunctionLibrary::SetManufacturerClockSpeed(UObject* WorldContextObject, const FString& BuildableId, float ClockSpeedPercent)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FDocModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}

	AFGBuildableManufacturer* Manufacturer = FindManufacturerById(World, BuildableId);
	if (!Manufacturer)
	{
		return FDocModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"),
			FString::Printf(TEXT("No manufacturer found with id '%s'"), *BuildableId));
	}

	if (!Manufacturer->GetCanChangePotential())
	{
		return FDocModOperationResult::Failure(TEXT("OPERATION_NOT_PERMITTED"),
			TEXT("This building does not allow changing clock speed"));
	}

	const float RequestedPotential = ClockSpeedPercent / 100.0f;
	const float MinPotential = Manufacturer->GetCurrentMinPotential();
	const float MaxPotential = Manufacturer->GetCurrentMaxPotential();
	if (RequestedPotential < MinPotential || RequestedPotential > MaxPotential)
	{
		return FDocModOperationResult::Failure(TEXT("INVALID_CLOCK_SPEED"),
			FString::Printf(TEXT("clockSpeedPercent %.1f is outside the valid range [%.1f, %.1f]"),
				ClockSpeedPercent, MinPotential * 100.0f, MaxPotential * 100.0f));
	}

	// Takes effect at the next production cycle, not instantly - see
	// AFGBuildableFactory::SetPendingPotential's doc comment.
	Manufacturer->SetPendingPotential(RequestedPotential);

	UE_LOG(LogDocModAI, Display, TEXT("SetManufacturerClockSpeed: %s -> %.1f%% (pending)"), *BuildableId, ClockSpeedPercent);

	return FDocModOperationResult::Success();
}

FDocModOperationResult UDocModFunctionLibrary::SetManufacturerRecipe(UObject* WorldContextObject, const FString& BuildableId, const FString& RecipeClassPath)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FDocModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}

	AFGBuildableManufacturer* Manufacturer = FindManufacturerById(World, BuildableId);
	if (!Manufacturer)
	{
		return FDocModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"),
			FString::Printf(TEXT("No manufacturer found with id '%s'"), *BuildableId));
	}

	// Resolve the path to a class and require it to actually be a
	// UFGRecipe subclass before doing anything else with it. This is
	// deliberately narrow - not a generic "load any class by path"
	// capability - per CLAUDE.md's Safety and Stability Boundary.
	UClass* ResolvedClass = LoadObject<UClass>(nullptr, *RecipeClassPath);
	if (!ResolvedClass || !ResolvedClass->IsChildOf(UFGRecipe::StaticClass()))
	{
		return FDocModOperationResult::Failure(TEXT("INVALID_RECIPE"),
			FString::Printf(TEXT("'%s' did not resolve to a UFGRecipe subclass"), *RecipeClassPath));
	}
	const TSubclassOf<UFGRecipe> RecipeClass = ResolvedClass;

	if (!UFGRecipe::IsProducedIn(RecipeClass, Manufacturer->GetClass()))
	{
		return FDocModOperationResult::Failure(TEXT("RECIPE_NOT_COMPATIBLE"),
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
		return FDocModOperationResult::Failure(TEXT("INVENTORY_NOT_EMPTY"),
			TEXT("Input and output inventories must be empty before changing recipe"));
	}

	Manufacturer->SetRecipe(RecipeClass);

	UE_LOG(LogDocModAI, Display, TEXT("SetManufacturerRecipe: %s -> %s"), *BuildableId, *RecipeClassPath);

	return FDocModOperationResult::Success();
}
