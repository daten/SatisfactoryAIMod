// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DocModTelemetryTypes.h"
#include "DocModFunctionLibrary.generated.h"

/**
 * DocMod AI interface: minimal Blueprint entry points. PLAN.md Phase 3-6 -
 * a version smoke test, read-only resource-node telemetry, and JSON
 * serialization for local debug testing. Read-only, log-only; no network
 * transport exists yet (Phase 9) and no game state mutation exists yet
 * (Phase 12+).
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
	 * Enumerates all AFGResourceNode actors in the world via TActorIterator
	 * (see docs/resource-node-research.md for why - no working manager API
	 * was found) and returns them as normalized, protocol-facing telemetry
	 * structs (PLAN.md Phase 5, Task 8). Id is session-local only; see
	 * FDocModResourceNodeTelemetry's comment.
	 */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static TArray<FDocModResourceNodeTelemetry> GetResourceNodeTelemetry(UObject* WorldContextObject);

	/** Debug entry point: logs one human-readable line per resource node via LogDocModAI. */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static void LogResourceNodes(UObject* WorldContextObject);

	/**
	 * Serializes resource node telemetry to the Phase 6 JSON protocol
	 * shape ({"protocolVersion":1,"resourceNodes":[...]}), logs it via
	 * LogDocModAI, and returns the JSON string. For local debug testing
	 * only - no network transport sends this anywhere yet (Phase 9).
	 */
	UFUNCTION(BlueprintCallable, Category = "DocMod|AI Interface", meta = (WorldContext = "WorldContextObject"))
	static FString LogResourceNodesAsJson(UObject* WorldContextObject);
};
