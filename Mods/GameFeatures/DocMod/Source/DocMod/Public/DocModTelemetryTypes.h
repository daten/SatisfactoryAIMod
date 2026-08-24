// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DocModTelemetryTypes.generated.h"

/**
 * Normalized, protocol-facing resource node telemetry (PLAN.md Phase 5).
 * Deliberately independent of any Unreal actor/UObject reference so it is
 * safe to copy, log, serialize, and eventually pass across the external
 * protocol boundary, per CLAUDE.md's "Unreal Object Rules" - no raw
 * pointers - AActor pointers or UObject pointers - in the external-facing
 * data model.
 *
 * Id is NOT a save-stable identifier. FactoryGame's resource node API
 * exposes nothing suitable (see docs/resource-node-research.md, section 4)
 * - no GUID, no persistent ID. This is currently derived from the node
 * actor's UObject path name (GetPathName()), which is unique within a
 * single running session/map load but is NOT guaranteed to survive
 * save/load, actor respawn, or map transition. Do not persist this value
 * across sessions or treat it as a database key. PLAN.md Phase 7 must
 * design a real stable identifier before this telemetry is used for
 * anything beyond debug logging/local testing.
 */
USTRUCT(BlueprintType)
struct FDocModResourceNodeTelemetry
{
	GENERATED_BODY()

	/** Session-local only, NOT save-stable. See struct comment above. */
	UPROPERTY(BlueprintReadOnly, Category = "DocMod|Telemetry")
	FString Id;

	/** Human-readable resource name, e.g. "Iron Ore". */
	UPROPERTY(BlueprintReadOnly, Category = "DocMod|Telemetry")
	FString Resource;

	/** Resource descriptor class path, e.g. "/Game/.../Desc_OreIron.Desc_OreIron_C". */
	UPROPERTY(BlueprintReadOnly, Category = "DocMod|Telemetry")
	FString ResourceClass;

	/** "Impure" / "Normal" / "Pure" (FactoryGame's EResourcePurity display text). */
	UPROPERTY(BlueprintReadOnly, Category = "DocMod|Telemetry")
	FString Purity;

	UPROPERTY(BlueprintReadOnly, Category = "DocMod|Telemetry")
	FVector Position = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "DocMod|Telemetry")
	bool bOccupied = false;
};
