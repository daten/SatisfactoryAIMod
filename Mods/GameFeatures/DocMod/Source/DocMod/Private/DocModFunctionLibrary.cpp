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
#include "Components/PrimitiveComponent.h"
#include "Engine/HitResult.h"
#include "Engine/ActorInstanceHandle.h"

namespace
{
	// GetResourcePurityText() looked like a plain display string but is
	// actually Slate rich-text markup meant for on-screen UI (its own doc
	// comment says "For UI") - confirmed against a real save, it returned
	// literal "<Bold>(Normal)</>" instead of "Normal", caught by
	// DocModSelfTest on its first real run. Use the raw enum
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

	FDocModResourceNodeTelemetry MakeResourceNodeTelemetry(AFGResourceNode* Node)
	{
		const TSubclassOf<UFGResourceDescriptor> ResourceClass = Node->GetResourceClass();

		FDocModResourceNodeTelemetry Telemetry;
		// Session-local only - see FDocModResourceNodeTelemetry's comment.
		Telemetry.Id = Node->GetPathName();
		Telemetry.Resource = ResourceClass ? UFGItemDescriptor::GetItemName(ResourceClass).ToString() : TEXT("Unknown");
		Telemetry.ResourceClass = ResourceClass ? ResourceClass->GetPathName() : FString();
		Telemetry.Purity = ResourcePurityToString(Node->GetResourcePurity());
		Telemetry.Position = Node->GetActorLocation();
		Telemetry.bOccupied = Node->IsOccupied();
		return Telemetry;
	}

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

	FDocModManufacturerTelemetry MakeManufacturerTelemetry(AFGBuildableManufacturer* Manufacturer)
	{
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
		return Telemetry;
	}

	TArray<FDocModManufacturerTelemetry> CollectManufacturerTelemetry(UWorld* World)
	{
		TArray<FDocModManufacturerTelemetry> Result;
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

	FDocModFactoryConnectionTelemetry MakeConnectionTelemetry(const FString& OwnerId, UFGFactoryConnectionComponent* Connection)
	{
		FDocModFactoryConnectionTelemetry Telemetry;
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

	TArray<FDocModFactoryConnectionTelemetry> CollectFactoryConnectionTelemetry(UWorld* World)
	{
		TArray<FDocModFactoryConnectionTelemetry> Result;

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
	// Condensed, not the default TPrettyJsonPrintPolicy - a real save's
	// resourceNodes payload pretty-printed to over 8000 log lines for one
	// call (631 nodes), confirmed against an actual FactoryGame.log.
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonString);
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
	// Condensed, not the default TPrettyJsonPrintPolicy - a real save's
	// resourceNodes payload pretty-printed to over 8000 log lines for one
	// call (631 nodes), confirmed against an actual FactoryGame.log.
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonString);
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

	TSharedRef<FJsonObject> ManufacturerToJson(const FDocModManufacturerTelemetry& Manufacturer)
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

FString UDocModFunctionLibrary::LogManufacturersAsJson(UObject* WorldContextObject)
{
	const TArray<FDocModManufacturerTelemetry> Manufacturers = GetManufacturerTelemetry(WorldContextObject);

	TArray<TSharedPtr<FJsonValue>> ManufacturerJsonArray;
	ManufacturerJsonArray.Reserve(Manufacturers.Num());
	for (const FDocModManufacturerTelemetry& Manufacturer : Manufacturers)
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

	UE_LOG(LogDocModAI, Display, TEXT("LogManufacturersAsJson: %s"), *JsonString);

	return JsonString;
}

FString UDocModFunctionLibrary::LogTargetedManufacturerAsJson(UObject* WorldContextObject)
{
	const FDocModManufacturerTelemetry Manufacturer = GetTargetedManufacturer(WorldContextObject);

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

	UE_LOG(LogDocModAI, Display, TEXT("LogTargetedManufacturerAsJson: %s"), *JsonString);

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
		UE_LOG(LogDocModAI, Display, TEXT("Connection: owner=%s direction=%s connected=%s connectedTo=%s position=%s normal=%s"),
			*Connection.OwnerBuildableId, *Connection.Direction, Connection.bConnected ? TEXT("true") : TEXT("false"),
			*Connection.ConnectedBuildableId, *Connection.Position.ToString(), *Connection.Normal.ToString());
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

	UE_LOG(LogDocModAI, Display, TEXT("LogFactoryConnectionsAsJson: %s"), *JsonString);

	return JsonString;
}

FDocModPlayerTelemetry UDocModFunctionLibrary::GetPlayerTelemetry(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		UE_LOG(LogDocModAI, Warning, TEXT("GetPlayerTelemetry: no valid world context"));
		return FDocModPlayerTelemetry();
	}

	// Player index 0 only - single-player/local session scope, per
	// PLAN.md/CLAUDE.md's multiplayer stance (same as GetTargetedManufacturer).
	const AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Character)
	{
		UE_LOG(LogDocModAI, Warning, TEXT("GetPlayerTelemetry: no local AFGCharacterPlayer (player index 0)"));
		return FDocModPlayerTelemetry();
	}

	FDocModPlayerTelemetry Telemetry;
	Telemetry.Position = Character->GetActorLocation();
	Telemetry.Rotation = Character->GetActorRotation();
	return Telemetry;
}

FString UDocModFunctionLibrary::LogPlayerAsJson(UObject* WorldContextObject)
{
	const FDocModPlayerTelemetry Player = GetPlayerTelemetry(WorldContextObject);

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

	UE_LOG(LogDocModAI, Display, TEXT("LogPlayerAsJson: %s"), *JsonString);

	return JsonString;
}

FDocModManufacturerTelemetry UDocModFunctionLibrary::GetTargetedManufacturer(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		UE_LOG(LogDocModAI, Warning, TEXT("GetTargetedManufacturer: no valid world context"));
		return FDocModManufacturerTelemetry();
	}

	// Player index 0 only - single-player/local session scope, per
	// PLAN.md/CLAUDE.md's multiplayer stance.
	const AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Character)
	{
		UE_LOG(LogDocModAI, Warning, TEXT("GetTargetedManufacturer: no local AFGCharacterPlayer (player index 0)"));
		return FDocModManufacturerTelemetry();
	}

	// GetBestUsableActor() is the game's own "what am I looking at / can
	// interact with" state (drives the "Press E to interact" prompt) -
	// not a reimplemented line trace.
	AFGBuildableManufacturer* Manufacturer = Cast<AFGBuildableManufacturer>(Character->GetBestUsableActor());
	if (!Manufacturer)
	{
		// Not an error - the player just isn't looking at a manufacturer
		// right now. Empty Id signals "none" to the caller.
		return FDocModManufacturerTelemetry();
	}

	return MakeManufacturerTelemetry(Manufacturer);
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

FDocModResourceNodeTelemetry UDocModFunctionLibrary::GetTargetedResourceNode(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		UE_LOG(LogDocModAI, Warning, TEXT("GetTargetedResourceNode: no valid world context"));
		return FDocModResourceNodeTelemetry();
	}

	// Player index 0 only - single-player/local session scope, per
	// PLAN.md/CLAUDE.md's multiplayer stance (same as GetTargetedManufacturer).
	const AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Character)
	{
		UE_LOG(LogDocModAI, Warning, TEXT("GetTargetedResourceNode: no local AFGCharacterPlayer (player index 0)"));
		return FDocModResourceNodeTelemetry();
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
		return FDocModResourceNodeTelemetry();
	}

	return MakeResourceNodeTelemetry(Node);
}

FDocModOperationResult UDocModFunctionLibrary::DebugCheckExtractorPlacementOnTargetedNode(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FDocModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}

	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Character)
	{
		return FDocModOperationResult::Failure(TEXT("NO_PLAYER"), TEXT("No local AFGCharacterPlayer (player index 0)"));
	}

	const FDocModResourceNodeTelemetry TargetTelemetry = GetTargetedResourceNode(WorldContextObject);
	if (TargetTelemetry.Id.IsEmpty())
	{
		return FDocModOperationResult::Failure(TEXT("NO_TARGET_NODE"), TEXT("Not currently looking at a resource node"));
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
		return FDocModOperationResult::Failure(TEXT("NODE_NOT_FOUND"), TEXT("Targeted node id could not be re-resolved"));
	}

	if (TargetNode->IsOccupied())
	{
		return FDocModOperationResult::Failure(TEXT("NODE_OCCUPIED"), TEXT("Targeted node is already occupied"));
	}

	// Scoped to solid-resource extraction (Miner Mk1) only for this first
	// experiment - see this function's header comment. Liquid/gas nodes
	// need a different buildable/recipe (Water/Oil Extractor) and are
	// deliberately out of scope here.
	const EResourceForm Form = UFGItemDescriptor::GetForm(TargetNode->GetResourceClass());
	if (Form != EResourceForm::RF_SOLID)
	{
		return FDocModOperationResult::Failure(TEXT("UNSUPPORTED_RESOURCE_FORM"),
			TEXT("This experiment only supports solid resource nodes (Miner Mk1) so far"));
	}

	// Diagnostic: IFGExtractableResourceInterface (FGExtractableResourceInterface.h,
	// which AFGResourceNodeBase implements - same interface IsOccupied()
	// above already calls) exposes a node-level "can an extractor even go
	// here" check, independent of the hologram's own clearance system
	// (already confirmed working - see the clearanceDetector diagnostic
	// added earlier). If this reports false, the real answer is here, not
	// in anything hologram-related.
	UE_LOG(LogDocModAI, Display, TEXT("DebugCheckExtractorPlacementOnTargetedNode: node diagnostics - CanPlaceResourceExtractor=%s HasAnyResources=%s CanBecomeOccupied=%s"),
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
		return FDocModOperationResult::Failure(TEXT("RECIPE_LOAD_FAILED"), TEXT("Failed to load Recipe_MinerMk1 as a UFGRecipe"));
	}
	const TSubclassOf<UFGRecipe> RecipeClass = MinerRecipeClass;

	// AFGHologram::SpawnHologramFromRecipe resolves the descriptor's
	// hologram class internally and spawns it - the one real "spawn a
	// hologram correctly" API found in docs/building-placement-research.md.
	AFGHologram* Hologram = AFGHologram::SpawnHologramFromRecipe(RecipeClass, Character, TargetNode->GetActorLocation(), Character);
	if (!Hologram)
	{
		return FDocModOperationResult::Failure(TEXT("HOLOGRAM_SPAWN_FAILED"), TEXT("SpawnHologramFromRecipe returned null"));
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
	UE_LOG(LogDocModAI, Display, TEXT("DebugCheckExtractorPlacementOnTargetedNode: rawLocation=%s placementLocation=%s placementRotation=%s"),
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
		UE_LOG(LogDocModAI, Warning, TEXT("DebugCheckExtractorPlacementOnTargetedNode: resource node's root component is not a UPrimitiveComponent - synthetic hit result has no Component set"));
	}

	if (!Hologram->IsValidHitResult(SyntheticHit))
	{
		Hologram->Destroy();
		return FDocModOperationResult::Failure(TEXT("INVALID_HIT_RESULT"), TEXT("Hologram::IsValidHitResult rejected the synthetic hit result"));
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
			UE_LOG(LogDocModAI, Warning, TEXT("DebugCheckExtractorPlacementOnTargetedNode (deferred): hologram or world became invalid while polling (after %d tick(s))"), PollState->AttemptsTaken);
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
		UE_LOG(LogDocModAI, Display, TEXT("DebugCheckExtractorPlacementOnTargetedNode (deferred, resolved after %d real tick(s)): node=%s canConstruct=%s disqualifiers=[%s] clearanceDetector=%s hasClearance=%s"),
			PollState->AttemptsTaken, *PollState->NodeId, bCanConstruct ? TEXT("true") : TEXT("false"), *DisqualifierSummary,
			PollHologram->GetClearanceDetector() ? TEXT("set") : TEXT("null"),
			PollHologram->HasClearance() ? TEXT("true") : TEXT("false"));

		PollHologram->Destroy();
	};

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));

	return FDocModOperationResult::Failure(TEXT("PENDING"),
		TEXT("Scheduled - polling real ticks until UFGCDInitializing clears (or a safety cap is hit); see LogDocModAI for the real result"));
}

FDocModOperationResult UDocModFunctionLibrary::DebugCheckExtractorPlacementViaBuildGun(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FDocModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}

	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Character)
	{
		return FDocModOperationResult::Failure(TEXT("NO_PLAYER"), TEXT("No local AFGCharacterPlayer (player index 0)"));
	}

	const FDocModResourceNodeTelemetry TargetTelemetry = GetTargetedResourceNode(WorldContextObject);
	if (TargetTelemetry.Id.IsEmpty())
	{
		return FDocModOperationResult::Failure(TEXT("NO_TARGET_NODE"), TEXT("Not currently looking at a resource node"));
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
		return FDocModOperationResult::Failure(TEXT("NODE_NOT_FOUND"), TEXT("Targeted node id could not be re-resolved"));
	}

	if (TargetNode->IsOccupied())
	{
		return FDocModOperationResult::Failure(TEXT("NODE_OCCUPIED"), TEXT("Targeted node is already occupied"));
	}

	const EResourceForm Form = UFGItemDescriptor::GetForm(TargetNode->GetResourceClass());
	if (Form != EResourceForm::RF_SOLID)
	{
		return FDocModOperationResult::Failure(TEXT("UNSUPPORTED_RESOURCE_FORM"),
			TEXT("This experiment only supports solid resource nodes (Miner Mk1) so far"));
	}

	UClass* MinerRecipeClass = LoadObject<UClass>(nullptr, TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_MinerMk1.Recipe_MinerMk1_C"));
	if (!MinerRecipeClass || !MinerRecipeClass->IsChildOf(UFGRecipe::StaticClass()))
	{
		return FDocModOperationResult::Failure(TEXT("RECIPE_LOAD_FAILED"), TEXT("Failed to load Recipe_MinerMk1 as a UFGRecipe"));
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
		return FDocModOperationResult::Failure(TEXT("NO_BUILD_GUN"), TEXT("AFGCharacterPlayer::GetBuildGun() returned null"));
	}

	UFGBuildGunStateBuild* BuildState = Cast<UFGBuildGunStateBuild>(BuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD));
	if (!BuildState)
	{
		Character->UnequipBuildGun();
		return FDocModOperationResult::Failure(TEXT("NO_BUILD_STATE"), TEXT("Could not resolve UFGBuildGunStateBuild from the build gun"));
	}

	AFGHologram* Hologram = BuildState->GetHologram();
	if (!Hologram)
	{
		Character->UnequipBuildGun();
		return FDocModOperationResult::Failure(TEXT("HOLOGRAM_SPAWN_FAILED"), TEXT("HotKeyRecipe did not result in a spawned hologram - build gun may not have entered build mode as expected"));
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
			UE_LOG(LogDocModAI, Warning, TEXT("DebugCheckExtractorPlacementViaBuildGun (deferred): hologram or world became invalid while polling (after %d tick(s))"), PollState->AttemptsTaken);
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

		UE_LOG(LogDocModAI, Display, TEXT("DebugCheckExtractorPlacementViaBuildGun (deferred, resolved after %d real tick(s)): node=%s canConstruct=%s disqualifiers=[%s]"),
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

	return FDocModOperationResult::Failure(TEXT("PENDING"),
		TEXT("Scheduled via the real build gun - polling real ticks until UFGCDInitializing clears (or a safety cap is hit); see LogDocModAI for the real result"));
}

FDocModOperationResult UDocModFunctionLibrary::ConstructExtractorOnTargetedNode(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FDocModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}

	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Character)
	{
		return FDocModOperationResult::Failure(TEXT("NO_PLAYER"), TEXT("No local AFGCharacterPlayer (player index 0)"));
	}

	const FDocModResourceNodeTelemetry TargetTelemetry = GetTargetedResourceNode(WorldContextObject);
	if (TargetTelemetry.Id.IsEmpty())
	{
		return FDocModOperationResult::Failure(TEXT("NO_TARGET_NODE"), TEXT("Not currently looking at a resource node"));
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
		return FDocModOperationResult::Failure(TEXT("NODE_NOT_FOUND"), TEXT("Targeted node id could not be re-resolved"));
	}

	if (TargetNode->IsOccupied())
	{
		return FDocModOperationResult::Failure(TEXT("NODE_OCCUPIED"), TEXT("Targeted node is already occupied"));
	}

	const EResourceForm Form = UFGItemDescriptor::GetForm(TargetNode->GetResourceClass());
	if (Form != EResourceForm::RF_SOLID)
	{
		return FDocModOperationResult::Failure(TEXT("UNSUPPORTED_RESOURCE_FORM"),
			TEXT("This experiment only supports solid resource nodes (Miner Mk1) so far"));
	}

	UClass* MinerRecipeClass = LoadObject<UClass>(nullptr, TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_MinerMk1.Recipe_MinerMk1_C"));
	if (!MinerRecipeClass || !MinerRecipeClass->IsChildOf(UFGRecipe::StaticClass()))
	{
		return FDocModOperationResult::Failure(TEXT("RECIPE_LOAD_FAILED"), TEXT("Failed to load Recipe_MinerMk1 as a UFGRecipe"));
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
		return FDocModOperationResult::Failure(TEXT("NO_BUILD_GUN"), TEXT("AFGCharacterPlayer::GetBuildGun() returned null"));
	}

	UFGBuildGunStateBuild* BuildState = Cast<UFGBuildGunStateBuild>(BuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD));
	if (!BuildState)
	{
		Character->UnequipBuildGun();
		return FDocModOperationResult::Failure(TEXT("NO_BUILD_STATE"), TEXT("Could not resolve UFGBuildGunStateBuild from the build gun"));
	}

	AFGHologram* Hologram = BuildState->GetHologram();
	if (!Hologram)
	{
		Character->UnequipBuildGun();
		return FDocModOperationResult::Failure(TEXT("HOLOGRAM_SPAWN_FAILED"), TEXT("HotKeyRecipe did not result in a spawned hologram"));
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
			UE_LOG(LogDocModAI, Warning, TEXT("ConstructExtractorOnTargetedNode (deferred): hologram or world became invalid while polling (after %d tick(s)) - nothing built"), PollState->AttemptsTaken);
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
			UE_LOG(LogDocModAI, Display, TEXT("ConstructExtractorOnTargetedNode (deferred, resolved after %d real tick(s)): CanConstruct()=false, NOT constructing - node=%s disqualifiers=[%s]"),
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
			UE_LOG(LogDocModAI, Error, TEXT("ConstructExtractorOnTargetedNode (deferred): lost the build state before constructing - aborting, nothing built"));
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			return;
		}

		PollBuildState->InternalConstructHologram(ConstructionID);

		// Confirmation, not just trust: if construction genuinely
		// succeeded, the node should now report occupied.
		const bool bNowOccupied = IsValid(PollTargetNode) && PollTargetNode->IsOccupied();
		UE_LOG(LogDocModAI, Display, TEXT("ConstructExtractorOnTargetedNode (deferred, resolved after %d real tick(s)): construction attempted via InternalConstructHologram - node=%s nodeNowOccupied=%s"),
			PollState->AttemptsTaken, *PollState->NodeId, bNowOccupied ? TEXT("true") : TEXT("false"));

		if (IsValid(PollCharacter))
		{
			PollCharacter->UnequipBuildGun();
		}
	};

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));

	return FDocModOperationResult::Failure(TEXT("PENDING"),
		TEXT("Scheduled via the real build gun - if CanConstruct() resolves true, a real Miner Mk1 WILL be constructed; see LogDocModAI for the real result"));
}

FDocModOperationResult UDocModFunctionLibrary::ConstructBuildingNearPlayer(UObject* WorldContextObject, const FString& RecipeClassPath)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FDocModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}

	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Character)
	{
		return FDocModOperationResult::Failure(TEXT("NO_PLAYER"), TEXT("No local AFGCharacterPlayer (player index 0)"));
	}

	// Same validation as SetManufacturerRecipe - deliberately narrow, not
	// a generic "load any class by path" capability.
	UClass* ResolvedClass = LoadObject<UClass>(nullptr, *RecipeClassPath);
	if (!ResolvedClass || !ResolvedClass->IsChildOf(UFGRecipe::StaticClass()))
	{
		return FDocModOperationResult::Failure(TEXT("INVALID_RECIPE"),
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

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DocModConstructBuildingNearPlayer), false);
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
		UE_LOG(LogDocModAI, Warning, TEXT("ConstructBuildingNearPlayer: ground trace found nothing at (%.0f, %.0f) - falling back to player Z"), CandidateXY.X, CandidateXY.Y);
		SyntheticHit.Location = FVector(CandidateXY.X, CandidateXY.Y, PlayerLocation.Z);
		SyntheticHit.ImpactPoint = SyntheticHit.Location;
		SyntheticHit.Normal = FVector::UpVector;
		SyntheticHit.ImpactNormal = FVector::UpVector;
		SyntheticHit.bBlockingHit = true;
	}

	UE_LOG(LogDocModAI, Display, TEXT("ConstructBuildingNearPlayer: recipe=%s groundTraceHit=%s location=%s"),
		*RecipeClassPath, bFoundGround ? TEXT("true") : TEXT("false"), *SyntheticHit.Location.ToString());

	// Real, ordinary player-facing hotkey - see
	// ConstructExtractorOnTargetedNode's matching comment. VISIBLE SIDE
	// EFFECT, always restored via UnequipBuildGun().
	Character->HotKeyRecipe(RecipeClass);

	AFGBuildGun* BuildGun = Character->GetBuildGun();
	if (!BuildGun)
	{
		Character->UnequipBuildGun();
		return FDocModOperationResult::Failure(TEXT("NO_BUILD_GUN"), TEXT("AFGCharacterPlayer::GetBuildGun() returned null"));
	}

	UFGBuildGunStateBuild* BuildState = Cast<UFGBuildGunStateBuild>(BuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD));
	if (!BuildState)
	{
		Character->UnequipBuildGun();
		return FDocModOperationResult::Failure(TEXT("NO_BUILD_STATE"), TEXT("Could not resolve UFGBuildGunStateBuild from the build gun"));
	}

	AFGHologram* Hologram = BuildState->GetHologram();
	if (!Hologram)
	{
		Character->UnequipBuildGun();
		return FDocModOperationResult::Failure(TEXT("HOLOGRAM_SPAWN_FAILED"),
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
			UE_LOG(LogDocModAI, Warning, TEXT("ConstructBuildingNearPlayer (deferred): hologram or world became invalid while polling (after %d tick(s)) - nothing built"), PollState->AttemptsTaken);
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
			UE_LOG(LogDocModAI, Display, TEXT("ConstructBuildingNearPlayer (deferred, resolved after %d real tick(s)): CanConstruct()=false, NOT constructing - recipe=%s disqualifiers=[%s]"),
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
			UE_LOG(LogDocModAI, Error, TEXT("ConstructBuildingNearPlayer (deferred): lost the build state before constructing - aborting, nothing built"));
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			return;
		}

		const FVector ConstructLocation = PollHologram->GetActorLocation();
		PollBuildState->InternalConstructHologram(ConstructionID);

		UE_LOG(LogDocModAI, Display, TEXT("ConstructBuildingNearPlayer (deferred, resolved after %d real tick(s)): construction attempted via InternalConstructHologram - recipe=%s location=%s"),
			PollState->AttemptsTaken, *PollState->RecipeClassPath, *ConstructLocation.ToString());

		if (IsValid(PollCharacter))
		{
			PollCharacter->UnequipBuildGun();
		}
	};

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));

	return FDocModOperationResult::Failure(TEXT("PENDING"),
		TEXT("Scheduled via the real build gun - if CanConstruct() resolves true, the building WILL be constructed; see LogDocModAI for the real result"));
}

void UDocModFunctionLibrary::ConstructBuildingAtPosition(UObject* WorldContextObject, const FString& RecipeClassPath, float X, float Y, int32 RotationScrollDelta, TFunction<void(const FDocModOperationResult&)> OnComplete)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		OnComplete(FDocModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context")));
		return;
	}

	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Character)
	{
		OnComplete(FDocModOperationResult::Failure(TEXT("NO_PLAYER"), TEXT("No local AFGCharacterPlayer (player index 0)")));
		return;
	}

	UClass* ResolvedClass = LoadObject<UClass>(nullptr, *RecipeClassPath);
	if (!ResolvedClass || !ResolvedClass->IsChildOf(UFGRecipe::StaticClass()))
	{
		OnComplete(FDocModOperationResult::Failure(TEXT("INVALID_RECIPE"),
			FString::Printf(TEXT("'%s' did not resolve to a UFGRecipe subclass"), *RecipeClassPath)));
		return;
	}
	const TSubclassOf<UFGRecipe> RecipeClass = ResolvedClass;

	// Same ground-finding approach as ConstructBuildingNearPlayer, just
	// driven by an explicit X/Y instead of a player-relative offset - see
	// that function's header comment for why this stays simple rather
	// than a real "solve valid placement" algorithm.
	const FVector PlayerLocation = Character->GetActorLocation();
	const FVector CandidateXY(X, Y, PlayerLocation.Z);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DocModConstructBuildingAtPosition), false);
	QueryParams.AddIgnoredActor(Character);
	const FVector TraceStart(X, Y, PlayerLocation.Z + 1000.0f);
	const FVector TraceEnd(X, Y, PlayerLocation.Z - 1000.0f);

	FHitResult GroundHit;
	const bool bFoundGround = World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, QueryParams);

	FHitResult SyntheticHit;
	if (bFoundGround)
	{
		SyntheticHit = GroundHit;
	}
	else
	{
		UE_LOG(LogDocModAI, Warning, TEXT("ConstructBuildingAtPosition: ground trace found nothing at (%.0f, %.0f) - falling back to player Z"), X, Y);
		SyntheticHit.Location = CandidateXY;
		SyntheticHit.ImpactPoint = SyntheticHit.Location;
		SyntheticHit.Normal = FVector::UpVector;
		SyntheticHit.ImpactNormal = FVector::UpVector;
		SyntheticHit.bBlockingHit = true;
	}

	UE_LOG(LogDocModAI, Display, TEXT("ConstructBuildingAtPosition: recipe=%s groundTraceHit=%s location=%s"),
		*RecipeClassPath, bFoundGround ? TEXT("true") : TEXT("false"), *SyntheticHit.Location.ToString());

	Character->HotKeyRecipe(RecipeClass);

	AFGBuildGun* BuildGun = Character->GetBuildGun();
	if (!BuildGun)
	{
		Character->UnequipBuildGun();
		OnComplete(FDocModOperationResult::Failure(TEXT("NO_BUILD_GUN"), TEXT("AFGCharacterPlayer::GetBuildGun() returned null")));
		return;
	}

	UFGBuildGunStateBuild* BuildState = Cast<UFGBuildGunStateBuild>(BuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD));
	if (!BuildState)
	{
		Character->UnequipBuildGun();
		OnComplete(FDocModOperationResult::Failure(TEXT("NO_BUILD_STATE"), TEXT("Could not resolve UFGBuildGunStateBuild from the build gun")));
		return;
	}

	AFGHologram* Hologram = BuildState->GetHologram();
	if (!Hologram)
	{
		Character->UnequipBuildGun();
		OnComplete(FDocModOperationResult::Failure(TEXT("HOLOGRAM_SPAWN_FAILED"),
			TEXT("HotKeyRecipe did not result in a spawned hologram - recipe may not be a simple single-step buildable")));
		return;
	}

	BuildGun->GetHitResult() = SyntheticHit;

	// Calibration (2026-08-25): applied once, before the poll starts -
	// Scroll()'s effect is expected to persist across the repeated
	// UpdateHologramPlacement() calls in the poll below (mScrollRotation
	// is a UPROPERTY member, not re-derived from the hit each tick).
	// Logging rotation before/after so the RPC caller can read the real
	// effect back via LogDocModAI without a separate throwaway experiment.
	const FRotator RotationBeforeScroll = Hologram->GetActorRotation();
	if (RotationScrollDelta != 0)
	{
		Hologram->Scroll(RotationScrollDelta);
	}
	UE_LOG(LogDocModAI, Display, TEXT("ConstructBuildingAtPosition: rotationScrollDelta=%d rotationBeforeScroll=%s rotationAfterScroll=%s"),
		RotationScrollDelta, *RotationBeforeScroll.ToString(), *Hologram->GetActorRotation().ToString());

	struct FPollState
	{
		TWeakObjectPtr<AFGHologram> Hologram;
		TWeakObjectPtr<AFGCharacterPlayer> Character;
		TWeakObjectPtr<UWorld> World;
		FString RecipeClassPath;
		FHitResult SyntheticHit;
		TFunction<void(const FDocModOperationResult&)> OnComplete;
		int32 AttemptsRemaining = 120; // safety cap - real ticks, not a fixed duration
		int32 AttemptsTaken = 0;
	};
	const TSharedRef<FPollState> PollState = MakeShared<FPollState>();
	PollState->Hologram = Hologram;
	PollState->Character = Character;
	PollState->World = World;
	PollState->RecipeClassPath = RecipeClassPath;
	PollState->SyntheticHit = SyntheticHit;
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
			UE_LOG(LogDocModAI, Warning, TEXT("ConstructBuildingAtPosition (deferred): hologram or world became invalid while polling (after %d tick(s)) - nothing built"), PollState->AttemptsTaken);
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FDocModOperationResult::Failure(TEXT("HOLOGRAM_INVALIDATED"), TEXT("Hologram or world became invalid while polling")));
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

		UE_LOG(LogDocModAI, Display, TEXT("ConstructBuildingAtPosition (deferred, resolved after %d real tick(s)): resolvedRotation=%s canConstruct=%s"),
			PollState->AttemptsTaken, *PollHologram->GetActorRotation().ToString(), bCanConstruct ? TEXT("true") : TEXT("false"));

		if (!bCanConstruct)
		{
			UE_LOG(LogDocModAI, Display, TEXT("ConstructBuildingAtPosition (deferred, resolved after %d real tick(s)): CanConstruct()=false, NOT constructing - recipe=%s disqualifiers=[%s]"),
				PollState->AttemptsTaken, *PollState->RecipeClassPath, *DisqualifierSummary);
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FDocModOperationResult::Failure(TEXT("CANNOT_CONSTRUCT"), DisqualifierSummary));
			return;
		}

		AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(PollWorld);
		const FNetConstructionID ConstructionID = BuildableSubsystem ? BuildableSubsystem->GetNewNetConstructionID() : FNetConstructionID();

		AFGBuildGun* PollBuildGun = IsValid(PollCharacter) ? PollCharacter->GetBuildGun() : nullptr;
		UFGBuildGunStateBuild* PollBuildState = PollBuildGun ? Cast<UFGBuildGunStateBuild>(PollBuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD)) : nullptr;
		if (!PollBuildState)
		{
			UE_LOG(LogDocModAI, Error, TEXT("ConstructBuildingAtPosition (deferred): lost the build state before constructing - aborting, nothing built"));
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FDocModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("Lost the build state before constructing")));
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

		UE_LOG(LogDocModAI, Display, TEXT("ConstructBuildingAtPosition (deferred, resolved after %d real tick(s)): construction attempted via InternalConstructHologram - recipe=%s location=%s id=%s"),
			PollState->AttemptsTaken, *PollState->RecipeClassPath, *ConstructLocation.ToString(), *ConstructedBuildableId);

		if (IsValid(PollCharacter))
		{
			PollCharacter->UnequipBuildGun();
		}

		PollState->OnComplete(ConstructedBuildableId.IsEmpty()
			? FDocModOperationResult::Success()
			: FDocModOperationResult::SuccessWithBuildableId(ConstructedBuildableId));
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

FDocModOperationResult UDocModFunctionLibrary::DebugCheckPowerConnection(UObject* WorldContextObject, const FString& BuildableIdA, const FString& BuildableIdB)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FDocModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}

	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Character)
	{
		return FDocModOperationResult::Failure(TEXT("NO_PLAYER"), TEXT("No local AFGCharacterPlayer (player index 0)"));
	}

	AFGBuildable* BuildableA = FindBuildableById(World, BuildableIdA);
	if (!BuildableA)
	{
		return FDocModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No buildable found with id '%s'"), *BuildableIdA));
	}
	AFGBuildable* BuildableB = FindBuildableById(World, BuildableIdB);
	if (!BuildableB)
	{
		return FDocModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No buildable found with id '%s'"), *BuildableIdB));
	}

	UFGPowerConnectionComponent* ConnectionA = FindFreePowerConnection(BuildableA);
	if (!ConnectionA)
	{
		return FDocModOperationResult::Failure(TEXT("NO_POWER_CONNECTION"), FString::Printf(TEXT("'%s' has no free power connection component"), *BuildableIdA));
	}
	UFGPowerConnectionComponent* ConnectionB = FindFreePowerConnection(BuildableB);
	if (!ConnectionB)
	{
		return FDocModOperationResult::Failure(TEXT("NO_POWER_CONNECTION"), FString::Printf(TEXT("'%s' has no free power connection component"), *BuildableIdB));
	}

	// Verified to exist as a real asset in Content/FactoryGame/Recipes/Buildings/
	// (see docs/conveyor-power-connection-research.md) - the build-cost
	// recipe for a plain power line/wire, not guessed from memory.
	UClass* PowerLineRecipeClass = LoadObject<UClass>(nullptr, TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PowerLine.Recipe_PowerLine_C"));
	if (!PowerLineRecipeClass || !PowerLineRecipeClass->IsChildOf(UFGRecipe::StaticClass()))
	{
		return FDocModOperationResult::Failure(TEXT("RECIPE_LOAD_FAILED"), TEXT("Failed to load Recipe_PowerLine as a UFGRecipe"));
	}
	const TSubclassOf<UFGRecipe> RecipeClass = PowerLineRecipeClass;

	Character->HotKeyRecipe(RecipeClass);

	AFGBuildGun* BuildGun = Character->GetBuildGun();
	if (!BuildGun)
	{
		Character->UnequipBuildGun();
		return FDocModOperationResult::Failure(TEXT("NO_BUILD_GUN"), TEXT("AFGCharacterPlayer::GetBuildGun() returned null"));
	}

	UFGBuildGunStateBuild* BuildState = Cast<UFGBuildGunStateBuild>(BuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD));
	if (!BuildState)
	{
		Character->UnequipBuildGun();
		return FDocModOperationResult::Failure(TEXT("NO_BUILD_STATE"), TEXT("Could not resolve UFGBuildGunStateBuild from the build gun"));
	}

	AFGHologram* Hologram = BuildState->GetHologram();
	AFGWireHologram* WireHologram = Cast<AFGWireHologram>(Hologram);
	if (!WireHologram)
	{
		Character->UnequipBuildGun();
		return FDocModOperationResult::Failure(TEXT("HOLOGRAM_SPAWN_FAILED"),
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
			UE_LOG(LogDocModAI, Warning, TEXT("DebugCheckPowerConnection (deferred): hologram or world became invalid while polling (after %d tick(s))"), PollState->AttemptsTaken);
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

		UE_LOG(LogDocModAI, Display, TEXT("DebugCheckPowerConnection (deferred, resolved after %d real tick(s)): a=%s b=%s canConstruct=%s disqualifiers=[%s]"),
			PollState->AttemptsTaken, *PollState->BuildableIdA, *PollState->BuildableIdB, bCanConstruct ? TEXT("true") : TEXT("false"), *DisqualifierSummary);

		if (IsValid(PollCharacter))
		{
			PollCharacter->UnequipBuildGun();
		}
	};

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));

	return FDocModOperationResult::Failure(TEXT("PENDING"),
		TEXT("Scheduled via the real build gun - dry-run only, never constructs; see LogDocModAI for the real result"));
}

void UDocModFunctionLibrary::ConstructExtractorOnNode(UObject* WorldContextObject, const FString& NodeId, TFunction<void(const FDocModOperationResult&)> OnComplete)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		OnComplete(FDocModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context")));
		return;
	}

	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Character)
	{
		OnComplete(FDocModOperationResult::Failure(TEXT("NO_PLAYER"), TEXT("No local AFGCharacterPlayer (player index 0)")));
		return;
	}

	AFGResourceNode* TargetNode = nullptr;
	for (TActorIterator<AFGResourceNode> It(World); It; ++It)
	{
		if (IsValid(*It) && It->GetPathName() == NodeId)
		{
			TargetNode = *It;
			break;
		}
	}
	if (!TargetNode)
	{
		OnComplete(FDocModOperationResult::Failure(TEXT("NODE_NOT_FOUND"), FString::Printf(TEXT("No resource node found with id '%s'"), *NodeId)));
		return;
	}

	if (TargetNode->IsOccupied())
	{
		OnComplete(FDocModOperationResult::Failure(TEXT("NODE_OCCUPIED"), TEXT("Targeted node is already occupied")));
		return;
	}

	const EResourceForm Form = UFGItemDescriptor::GetForm(TargetNode->GetResourceClass());
	if (Form != EResourceForm::RF_SOLID)
	{
		OnComplete(FDocModOperationResult::Failure(TEXT("UNSUPPORTED_RESOURCE_FORM"),
			TEXT("This experiment only supports solid resource nodes (Miner Mk1) so far")));
		return;
	}

	UClass* MinerRecipeClass = LoadObject<UClass>(nullptr, TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_MinerMk1.Recipe_MinerMk1_C"));
	if (!MinerRecipeClass || !MinerRecipeClass->IsChildOf(UFGRecipe::StaticClass()))
	{
		OnComplete(FDocModOperationResult::Failure(TEXT("RECIPE_LOAD_FAILED"), TEXT("Failed to load Recipe_MinerMk1 as a UFGRecipe")));
		return;
	}
	const TSubclassOf<UFGRecipe> RecipeClass = MinerRecipeClass;

	Character->HotKeyRecipe(RecipeClass);

	AFGBuildGun* BuildGun = Character->GetBuildGun();
	if (!BuildGun)
	{
		Character->UnequipBuildGun();
		OnComplete(FDocModOperationResult::Failure(TEXT("NO_BUILD_GUN"), TEXT("AFGCharacterPlayer::GetBuildGun() returned null")));
		return;
	}

	UFGBuildGunStateBuild* BuildState = Cast<UFGBuildGunStateBuild>(BuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD));
	if (!BuildState)
	{
		Character->UnequipBuildGun();
		OnComplete(FDocModOperationResult::Failure(TEXT("NO_BUILD_STATE"), TEXT("Could not resolve UFGBuildGunStateBuild from the build gun")));
		return;
	}

	AFGHologram* Hologram = BuildState->GetHologram();
	if (!Hologram)
	{
		Character->UnequipBuildGun();
		OnComplete(FDocModOperationResult::Failure(TEXT("HOLOGRAM_SPAWN_FAILED"), TEXT("HotKeyRecipe did not result in a spawned hologram")));
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

	BuildGun->GetHitResult() = SyntheticHit;

	struct FPollState
	{
		TWeakObjectPtr<AFGHologram> Hologram;
		TWeakObjectPtr<AFGCharacterPlayer> Character;
		TWeakObjectPtr<AFGResourceNode> TargetNode;
		TWeakObjectPtr<UWorld> World;
		FString NodeId;
		FHitResult SyntheticHit;
		TFunction<void(const FDocModOperationResult&)> OnComplete;
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
	PollState->OnComplete = MoveTemp(OnComplete);

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
			UE_LOG(LogDocModAI, Warning, TEXT("ConstructExtractorOnNode (deferred): hologram or world became invalid while polling (after %d tick(s)) - nothing built"), PollState->AttemptsTaken);
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FDocModOperationResult::Failure(TEXT("HOLOGRAM_INVALIDATED"), TEXT("Hologram or world became invalid while polling")));
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
			UE_LOG(LogDocModAI, Display, TEXT("ConstructExtractorOnNode (deferred, resolved after %d real tick(s)): CanConstruct()=false, NOT constructing - node=%s disqualifiers=[%s]"),
				PollState->AttemptsTaken, *PollState->NodeId, *DisqualifierSummary);
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FDocModOperationResult::Failure(TEXT("CANNOT_CONSTRUCT"), DisqualifierSummary));
			return;
		}

		AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(PollWorld);
		const FNetConstructionID ConstructionID = BuildableSubsystem ? BuildableSubsystem->GetNewNetConstructionID() : FNetConstructionID();

		AFGBuildGun* PollBuildGun = IsValid(PollCharacter) ? PollCharacter->GetBuildGun() : nullptr;
		UFGBuildGunStateBuild* PollBuildState = PollBuildGun ? Cast<UFGBuildGunStateBuild>(PollBuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD)) : nullptr;
		if (!PollBuildState)
		{
			UE_LOG(LogDocModAI, Error, TEXT("ConstructExtractorOnNode (deferred): lost the build state before constructing - aborting, nothing built"));
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FDocModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("Lost the build state before constructing")));
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

		UE_LOG(LogDocModAI, Display, TEXT("ConstructExtractorOnNode (deferred, resolved after %d real tick(s)): construction attempted via InternalConstructHologram - node=%s nodeNowOccupied=%s id=%s"),
			PollState->AttemptsTaken, *PollState->NodeId, bNowOccupied ? TEXT("true") : TEXT("false"), *ConstructedBuildableId);

		if (IsValid(PollCharacter))
		{
			PollCharacter->UnequipBuildGun();
		}

		if (!bNowOccupied)
		{
			PollState->OnComplete(FDocModOperationResult::Failure(TEXT("CONSTRUCTION_UNCONFIRMED"), TEXT("InternalConstructHologram was called but the node does not report occupied afterward")));
			return;
		}

		PollState->OnComplete(ConstructedBuildableId.IsEmpty()
			? FDocModOperationResult::Success()
			: FDocModOperationResult::SuccessWithBuildableId(ConstructedBuildableId));
	};

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
}

void UDocModFunctionLibrary::ConstructPowerConnection(UObject* WorldContextObject, const FString& BuildableIdA, const FString& BuildableIdB, bool bDryRun, TFunction<void(const FDocModOperationResult&)> OnComplete)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		OnComplete(FDocModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context")));
		return;
	}

	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Character)
	{
		OnComplete(FDocModOperationResult::Failure(TEXT("NO_PLAYER"), TEXT("No local AFGCharacterPlayer (player index 0)")));
		return;
	}

	AFGBuildable* BuildableA = FindBuildableById(World, BuildableIdA);
	if (!BuildableA)
	{
		OnComplete(FDocModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No buildable found with id '%s'"), *BuildableIdA)));
		return;
	}
	AFGBuildable* BuildableB = FindBuildableById(World, BuildableIdB);
	if (!BuildableB)
	{
		OnComplete(FDocModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No buildable found with id '%s'"), *BuildableIdB)));
		return;
	}

	// See this function's header comment on the pole-vs-daisy-chain
	// gameplay constraint - a NO_POWER_CONNECTION result here may
	// correctly reflect that the save's progression hasn't unlocked
	// direct machine-to-machine wiring yet, not a bug.
	UFGPowerConnectionComponent* ConnectionA = FindFreePowerConnection(BuildableA);
	if (!ConnectionA)
	{
		OnComplete(FDocModOperationResult::Failure(TEXT("NO_POWER_CONNECTION"), FString::Printf(TEXT("'%s' has no free power connection component"), *BuildableIdA)));
		return;
	}
	UFGPowerConnectionComponent* ConnectionB = FindFreePowerConnection(BuildableB);
	if (!ConnectionB)
	{
		OnComplete(FDocModOperationResult::Failure(TEXT("NO_POWER_CONNECTION"), FString::Printf(TEXT("'%s' has no free power connection component"), *BuildableIdB)));
		return;
	}

	UClass* PowerLineRecipeClass = LoadObject<UClass>(nullptr, TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PowerLine.Recipe_PowerLine_C"));
	if (!PowerLineRecipeClass || !PowerLineRecipeClass->IsChildOf(UFGRecipe::StaticClass()))
	{
		OnComplete(FDocModOperationResult::Failure(TEXT("RECIPE_LOAD_FAILED"), TEXT("Failed to load Recipe_PowerLine as a UFGRecipe")));
		return;
	}
	const TSubclassOf<UFGRecipe> RecipeClass = PowerLineRecipeClass;

	Character->HotKeyRecipe(RecipeClass);

	AFGBuildGun* BuildGun = Character->GetBuildGun();
	if (!BuildGun)
	{
		Character->UnequipBuildGun();
		OnComplete(FDocModOperationResult::Failure(TEXT("NO_BUILD_GUN"), TEXT("AFGCharacterPlayer::GetBuildGun() returned null")));
		return;
	}

	UFGBuildGunStateBuild* BuildState = Cast<UFGBuildGunStateBuild>(BuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD));
	if (!BuildState)
	{
		Character->UnequipBuildGun();
		OnComplete(FDocModOperationResult::Failure(TEXT("NO_BUILD_STATE"), TEXT("Could not resolve UFGBuildGunStateBuild from the build gun")));
		return;
	}

	AFGHologram* Hologram = BuildState->GetHologram();
	AFGWireHologram* WireHologram = Cast<AFGWireHologram>(Hologram);
	if (!WireHologram)
	{
		Character->UnequipBuildGun();
		OnComplete(FDocModOperationResult::Failure(TEXT("HOLOGRAM_SPAWN_FAILED"),
			FString::Printf(TEXT("HotKeyRecipe(Recipe_PowerLine) did not result in an AFGWireHologram (got %s)"),
				Hologram ? *Hologram->GetClass()->GetName() : TEXT("null"))));
		return;
	}

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
		int32 AttemptsRemaining = 120; // safety cap - real ticks, not a fixed duration
		int32 AttemptsTaken = 0;
		TFunction<void(const FDocModOperationResult&)> OnComplete;
	};
	const TSharedRef<FPollState> PollState = MakeShared<FPollState>();
	PollState->Hologram = WireHologram;
	PollState->Character = Character;
	PollState->World = World;
	PollState->SyntheticHit = SyntheticHit;
	PollState->BuildableIdA = BuildableIdA;
	PollState->BuildableIdB = BuildableIdB;
	PollState->bDryRun = bDryRun;
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
			UE_LOG(LogDocModAI, Warning, TEXT("ConstructPowerConnection (deferred): hologram or world became invalid while polling (after %d tick(s))"), PollState->AttemptsTaken);
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FDocModOperationResult::Failure(TEXT("HOLOGRAM_INVALIDATED"), TEXT("Hologram or world became invalid while polling")));
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

		UE_LOG(LogDocModAI, Display, TEXT("ConstructPowerConnection (deferred, resolved after %d real tick(s)): a=%s b=%s dryRun=%s canConstruct=%s disqualifiers=[%s]"),
			PollState->AttemptsTaken, *PollState->BuildableIdA, *PollState->BuildableIdB, PollState->bDryRun ? TEXT("true") : TEXT("false"),
			bCanConstruct ? TEXT("true") : TEXT("false"), *DisqualifierSummary);

		if (!bCanConstruct)
		{
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FDocModOperationResult::Failure(TEXT("CANNOT_CONSTRUCT"), DisqualifierSummary));
			return;
		}

		if (PollState->bDryRun)
		{
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FDocModOperationResult::Success());
			return;
		}

		AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(PollWorld);
		const FNetConstructionID ConstructionID = BuildableSubsystem ? BuildableSubsystem->GetNewNetConstructionID() : FNetConstructionID();

		AFGBuildGun* PollBuildGun = IsValid(PollCharacter) ? PollCharacter->GetBuildGun() : nullptr;
		UFGBuildGunStateBuild* PollBuildState = PollBuildGun ? Cast<UFGBuildGunStateBuild>(PollBuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD)) : nullptr;
		if (!PollBuildState)
		{
			UE_LOG(LogDocModAI, Error, TEXT("ConstructPowerConnection (deferred): lost the build state before constructing - aborting, nothing built"));
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FDocModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("Lost the build state before constructing")));
			return;
		}

		PollBuildState->InternalConstructHologram(ConstructionID);

		UE_LOG(LogDocModAI, Display, TEXT("ConstructPowerConnection (deferred, resolved after %d real tick(s)): construction attempted via InternalConstructHologram - a=%s b=%s"),
			PollState->AttemptsTaken, *PollState->BuildableIdA, *PollState->BuildableIdB);

		if (IsValid(PollCharacter))
		{
			PollCharacter->UnequipBuildGun();
		}

		PollState->OnComplete(FDocModOperationResult::Success());
	};

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
}

FDocModOperationResult UDocModFunctionLibrary::DebugCheckConveyorSnap(UObject* WorldContextObject, const FString& SourceBuildableId)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FDocModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}

	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Character)
	{
		return FDocModOperationResult::Failure(TEXT("NO_PLAYER"), TEXT("No local AFGCharacterPlayer (player index 0)"));
	}

	AFGBuildable* SourceBuildable = FindBuildableById(World, SourceBuildableId);
	if (!SourceBuildable)
	{
		return FDocModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No buildable found with id '%s'"), *SourceBuildableId));
	}

	UFGFactoryConnectionComponent* SourceConnection = FindFreeFactoryConnection(SourceBuildable, EFactoryConnectionDirection::FCD_OUTPUT);
	if (!SourceConnection)
	{
		return FDocModOperationResult::Failure(TEXT("NO_FACTORY_CONNECTION"), FString::Printf(TEXT("'%s' has no free Output factory connection component"), *SourceBuildableId));
	}

	UClass* BeltRecipeClass = LoadObject<UClass>(nullptr, TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorBeltMk1.Recipe_ConveyorBeltMk1_C"));
	if (!BeltRecipeClass || !BeltRecipeClass->IsChildOf(UFGRecipe::StaticClass()))
	{
		return FDocModOperationResult::Failure(TEXT("RECIPE_LOAD_FAILED"), TEXT("Failed to load Recipe_ConveyorBeltMk1 as a UFGRecipe"));
	}
	const TSubclassOf<UFGRecipe> RecipeClass = BeltRecipeClass;

	Character->HotKeyRecipe(RecipeClass);

	AFGBuildGun* BuildGun = Character->GetBuildGun();
	if (!BuildGun)
	{
		Character->UnequipBuildGun();
		return FDocModOperationResult::Failure(TEXT("NO_BUILD_GUN"), TEXT("AFGCharacterPlayer::GetBuildGun() returned null"));
	}

	UFGBuildGunStateBuild* BuildState = Cast<UFGBuildGunStateBuild>(BuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD));
	if (!BuildState)
	{
		Character->UnequipBuildGun();
		return FDocModOperationResult::Failure(TEXT("NO_BUILD_STATE"), TEXT("Could not resolve UFGBuildGunStateBuild from the build gun"));
	}

	AFGHologram* Hologram = BuildState->GetHologram();
	AFGConveyorBeltHologram* BeltHologram = Cast<AFGConveyorBeltHologram>(Hologram);
	if (!BeltHologram)
	{
		Character->UnequipBuildGun();
		return FDocModOperationResult::Failure(TEXT("HOLOGRAM_SPAWN_FAILED"),
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

	UE_LOG(LogDocModAI, Display, TEXT("DebugCheckConveyorSnap: source=%s connectorLocation=%s stepInitial=%d | afterUpdateHologramPlacement: step=%d snapped=%s | afterTrySnapToActor: result=%s step=%d snapped=%s connectedCount=%d disqualifiers=[%s] | afterDoMultiStepPlacement(true): stepComplete=%s step=%d snapped=%s connectedCount=%d"),
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
		return FDocModOperationResult::Failure(TEXT("SNAP_FAILED"), TEXT("No snap indicator was ever true - see LogDocModAI for the full step-by-step trace"));
	}

	return FDocModOperationResult::Success();
}

void UDocModFunctionLibrary::ConstructConveyorBelt(UObject* WorldContextObject, const FString& SourceBuildableId, const FString& DestBuildableId, bool bDryRun, TFunction<void(const FDocModOperationResult&)> OnComplete)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		OnComplete(FDocModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context")));
		return;
	}

	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Character)
	{
		OnComplete(FDocModOperationResult::Failure(TEXT("NO_PLAYER"), TEXT("No local AFGCharacterPlayer (player index 0)")));
		return;
	}

	AFGBuildable* SourceBuildable = FindBuildableById(World, SourceBuildableId);
	if (!SourceBuildable)
	{
		OnComplete(FDocModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No buildable found with id '%s'"), *SourceBuildableId)));
		return;
	}
	AFGBuildable* DestBuildable = FindBuildableById(World, DestBuildableId);
	if (!DestBuildable)
	{
		OnComplete(FDocModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No buildable found with id '%s'"), *DestBuildableId)));
		return;
	}

	UFGFactoryConnectionComponent* SourceConnection = FindFreeFactoryConnection(SourceBuildable, EFactoryConnectionDirection::FCD_OUTPUT);
	if (!SourceConnection)
	{
		OnComplete(FDocModOperationResult::Failure(TEXT("NO_FACTORY_CONNECTION"), FString::Printf(TEXT("'%s' has no free Output factory connection component"), *SourceBuildableId)));
		return;
	}
	UFGFactoryConnectionComponent* DestConnection = FindFreeFactoryConnection(DestBuildable, EFactoryConnectionDirection::FCD_INPUT);
	if (!DestConnection)
	{
		OnComplete(FDocModOperationResult::Failure(TEXT("NO_FACTORY_CONNECTION"), FString::Printf(TEXT("'%s' has no free Input factory connection component"), *DestBuildableId)));
		return;
	}

	UClass* BeltRecipeClass = LoadObject<UClass>(nullptr, TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorBeltMk1.Recipe_ConveyorBeltMk1_C"));
	if (!BeltRecipeClass || !BeltRecipeClass->IsChildOf(UFGRecipe::StaticClass()))
	{
		OnComplete(FDocModOperationResult::Failure(TEXT("RECIPE_LOAD_FAILED"), TEXT("Failed to load Recipe_ConveyorBeltMk1 as a UFGRecipe")));
		return;
	}
	const TSubclassOf<UFGRecipe> RecipeClass = BeltRecipeClass;

	Character->HotKeyRecipe(RecipeClass);

	AFGBuildGun* BuildGun = Character->GetBuildGun();
	if (!BuildGun)
	{
		Character->UnequipBuildGun();
		OnComplete(FDocModOperationResult::Failure(TEXT("NO_BUILD_GUN"), TEXT("AFGCharacterPlayer::GetBuildGun() returned null")));
		return;
	}

	UFGBuildGunStateBuild* BuildState = Cast<UFGBuildGunStateBuild>(BuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD));
	if (!BuildState)
	{
		Character->UnequipBuildGun();
		OnComplete(FDocModOperationResult::Failure(TEXT("NO_BUILD_STATE"), TEXT("Could not resolve UFGBuildGunStateBuild from the build gun")));
		return;
	}

	AFGHologram* Hologram = BuildState->GetHologram();
	AFGConveyorBeltHologram* BeltHologram = Cast<AFGConveyorBeltHologram>(Hologram);
	if (!BeltHologram)
	{
		Character->UnequipBuildGun();
		OnComplete(FDocModOperationResult::Failure(TEXT("HOLOGRAM_SPAWN_FAILED"),
			FString::Printf(TEXT("HotKeyRecipe(Recipe_ConveyorBeltMk1) did not result in an AFGConveyorBeltHologram (got %s)"),
				Hologram ? *Hologram->GetClass()->GetName() : TEXT("null"))));
		return;
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
	UE_LOG(LogDocModAI, Display, TEXT("ConstructConveyorBelt diagnostic: playerLoc=%s playerRot=%s sourceConnectorLoc=%s sourceConnectorNormal=%s sourceConnectorClearanceLoc=%s destConnectorLoc=%s destConnectorNormal=%s destConnectorClearanceLoc=%s"),
		*Character->GetActorLocation().ToString(), *Character->GetActorRotation().ToString(),
		*SourceConnection->GetConnectorLocation().ToString(), *SourceConnection->GetConnectorNormal().ToString(), *SourceConnection->GetConnectorLocation(true).ToString(),
		*DestConnection->GetConnectorLocation().ToString(), *DestConnection->GetConnectorNormal().ToString(), *DestConnection->GetConnectorLocation(true).ToString());

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

	UE_LOG(LogDocModAI, Display, TEXT("ConstructConveyorBelt: source=%s dest=%s after start click: stepComplete=%s step=%d disqualifiers=[%s]"),
		*SourceBuildableId, *DestBuildableId, bStartStepComplete ? TEXT("true") : TEXT("false"), static_cast<int32>(StepAfterStart), *SummarizeDisqualifiers(BeltHologram));

	if (bStartStepComplete)
	{
		// Unexpected - a two-endpoint belt shouldn't complete on the
		// first click. Report exactly what happened rather than
		// guessing further; do not proceed to a second click on an
		// already-"complete" hologram.
		Character->UnequipBuildGun();
		OnComplete(FDocModOperationResult::Failure(TEXT("UNEXPECTED_STEP_COMPLETE"), TEXT("DoMultiStepPlacement() reported complete after only the start click")));
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

	UE_LOG(LogDocModAI, Display, TEXT("ConstructConveyorBelt: source=%s dest=%s after end click: stepComplete=%s step=%d connectedCount=%d disqualifiers=[%s]"),
		*SourceBuildableId, *DestBuildableId, bEndStepComplete ? TEXT("true") : TEXT("false"), static_cast<int32>(StepAfterEnd), ConnectedBuildables.Num(), *SummarizeDisqualifiers(BeltHologram));

	if (!bEndStepComplete)
	{
		Character->UnequipBuildGun();
		OnComplete(FDocModOperationResult::Failure(TEXT("PLACEMENT_INCOMPLETE"),
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
		int32 AttemptsRemaining = 120; // safety cap - real ticks, not a fixed duration
		int32 AttemptsTaken = 0;
		TFunction<void(const FDocModOperationResult&)> OnComplete;
	};
	const TSharedRef<FPollState> PollState = MakeShared<FPollState>();
	PollState->Hologram = BeltHologram;
	PollState->Character = Character;
	PollState->World = World;
	PollState->SourceBuildableId = SourceBuildableId;
	PollState->DestBuildableId = DestBuildableId;
	PollState->bDryRun = bDryRun;
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
			UE_LOG(LogDocModAI, Warning, TEXT("ConstructConveyorBelt (deferred): hologram or world became invalid while polling (after %d tick(s))"), PollState->AttemptsTaken);
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FDocModOperationResult::Failure(TEXT("HOLOGRAM_INVALIDATED"), TEXT("Hologram or world became invalid while polling")));
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

		UE_LOG(LogDocModAI, Display, TEXT("ConstructConveyorBelt (deferred, resolved after %d real tick(s)): source=%s dest=%s dryRun=%s canConstruct=%s disqualifiers=[%s]"),
			PollState->AttemptsTaken, *PollState->SourceBuildableId, *PollState->DestBuildableId, PollState->bDryRun ? TEXT("true") : TEXT("false"),
			bCanConstruct ? TEXT("true") : TEXT("false"), *DisqualifierSummary);

		if (!bCanConstruct)
		{
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FDocModOperationResult::Failure(TEXT("CANNOT_CONSTRUCT"), DisqualifierSummary));
			return;
		}

		if (PollState->bDryRun)
		{
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FDocModOperationResult::Success());
			return;
		}

		AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(PollWorld);
		const FNetConstructionID ConstructionID = BuildableSubsystem ? BuildableSubsystem->GetNewNetConstructionID() : FNetConstructionID();

		AFGBuildGun* PollBuildGun = IsValid(PollCharacter) ? PollCharacter->GetBuildGun() : nullptr;
		UFGBuildGunStateBuild* PollBuildState = PollBuildGun ? Cast<UFGBuildGunStateBuild>(PollBuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD)) : nullptr;
		if (!PollBuildState)
		{
			UE_LOG(LogDocModAI, Error, TEXT("ConstructConveyorBelt (deferred): lost the build state before constructing - aborting, nothing built"));
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FDocModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("Lost the build state before constructing")));
			return;
		}

		PollBuildState->InternalConstructHologram(ConstructionID);

		UE_LOG(LogDocModAI, Display, TEXT("ConstructConveyorBelt (deferred, resolved after %d real tick(s)): construction attempted via InternalConstructHologram - source=%s dest=%s"),
			PollState->AttemptsTaken, *PollState->SourceBuildableId, *PollState->DestBuildableId);

		if (IsValid(PollCharacter))
		{
			PollCharacter->UnequipBuildGun();
		}

		PollState->OnComplete(FDocModOperationResult::Success());
	};

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
}
