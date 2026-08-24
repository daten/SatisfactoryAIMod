// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DocModFunctionLibrary.generated.h"

/**
 * Smallest possible C++ -> Blueprint smoke test for the DocMod AI
 * interface (PLAN.md Phase 3). Exposes only a static version string;
 * no world access, no game state, nothing that could be mistaken for
 * part of the actual telemetry/control protocol.
 */
UCLASS()
class DOCMOD_API UDocModFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Returns the DocMod AI interface version string. */
	UFUNCTION(BlueprintPure, Category = "DocMod|AI Interface", meta = (BlueprintThreadSafe))
	static FString GetInterfaceVersion();
};
