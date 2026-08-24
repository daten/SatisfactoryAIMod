// Copyright Epic Games, Inc. All Rights Reserved.

#include "DocModFunctionLibrary.h"
#include "DocMod.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Resources/FGResourceNode.h"
#include "Resources/FGItemDescriptor.h"
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
