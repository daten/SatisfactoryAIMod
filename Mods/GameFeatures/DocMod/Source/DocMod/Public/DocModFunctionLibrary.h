// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DocModFunctionLibrary.generated.h"

/**
 * DocMod AI interface: minimal Blueprint entry points. PLAN.md Phase 3/4 -
 * a version smoke test plus a debug resource-node enumeration/logging
 * entry point. Read-only, log-only; no telemetry protocol or JSON exists
 * yet (Phases 5/6).
 */
UCLASS()
class DOCMOD_API UDocModFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Returns the DocMod AI interface version string. */
	UFUNCTION(BlueprintPure, Category = "DocMod|AI Interface", meta = (BlueprintThreadSafe))
	static FString GetInterfaceVersion();

	/**
	 * Debug entry point (PLAN.md Phase 4): enumerates all AFGResourceNode
	 * actors in the world via TActorIterator and logs resource type,
	 * purity, world position, and occupied state to LogDocModAI. No
	 * normalized telemetry struct or stable identifier exists yet
	 * (Phases 5/7) - this only proves the data is reachable and correct.
	 */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static void LogResourceNodes(UObject* WorldContextObject);
};
