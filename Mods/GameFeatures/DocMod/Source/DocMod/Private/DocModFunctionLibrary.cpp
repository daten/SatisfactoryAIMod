// Copyright Epic Games, Inc. All Rights Reserved.

#include "DocModFunctionLibrary.h"
#include "DocMod.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Resources/FGResourceNode.h"
#include "Resources/FGItemDescriptor.h"

FString UDocModFunctionLibrary::GetInterfaceVersion()
{
	UE_LOG(LogDocModAI, Verbose, TEXT("GetInterfaceVersion called"));
	return TEXT("0.1.0");
}

void UDocModFunctionLibrary::LogResourceNodes(UObject* WorldContextObject)
{
	// AFGResourceNodeManager exists but its node array has no public
	// getter and its .cpp is a stub (see docs/resource-node-research.md),
	// so a plain actor-iterator world scan is the only evidenced way to
	// enumerate nodes right now. Fine for a debug/Phase-4 entry point;
	// CLAUDE.md steers production code toward a subsystem/event-driven
	// approach instead of scanning every frame.
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		UE_LOG(LogDocModAI, Warning, TEXT("LogResourceNodes: no valid world context"));
		return;
	}

	int32 NodeCount = 0;
	for (TActorIterator<AFGResourceNode> It(World); It; ++It)
	{
		AFGResourceNode* Node = *It;
		if (!IsValid(Node))
		{
			continue;
		}

		const TSubclassOf<UFGResourceDescriptor> ResourceClass = Node->GetResourceClass();
		const FText ResourceName = ResourceClass ? UFGItemDescriptor::GetItemName(ResourceClass) : FText::FromString(TEXT("Unknown"));
		const FText PurityText = Node->GetResourcePurityText();
		const FVector Location = Node->GetActorLocation();
		const bool bOccupied = Node->IsOccupied();

		UE_LOG(LogDocModAI, Display, TEXT("ResourceNode: resource=\"%s\" purity=%s pos=(%.1f, %.1f, %.1f) occupied=%s"),
			*ResourceName.ToString(), *PurityText.ToString(), Location.X, Location.Y, Location.Z, bOccupied ? TEXT("true") : TEXT("false"));

		++NodeCount;
	}

	UE_LOG(LogDocModAI, Display, TEXT("LogResourceNodes: enumerated %d resource node(s)"), NodeCount);
}
