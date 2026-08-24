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

/**
 * Normalized telemetry for any placed AFGBuildable (PLAN.md Phase 10,
 * "buildings"). Generic fields only - see FDocModManufacturerTelemetry
 * for recipe/clock/production-status fields, which only apply to
 * manufacturing buildings (Constructor/Assembler/Manufacturer/...).
 *
 * Id has the same session-local-only caveat as
 * FDocModResourceNodeTelemetry - see that struct's comment.
 */
USTRUCT(BlueprintType)
struct FDocModBuildableTelemetry
{
	GENERATED_BODY()

	/** Session-local only, NOT save-stable. */
	UPROPERTY(BlueprintReadOnly, Category = "DocMod|Telemetry")
	FString Id;

	/** Buildable class path, e.g. "/Game/.../Build_ConstructorMk1.Build_ConstructorMk1_C". */
	UPROPERTY(BlueprintReadOnly, Category = "DocMod|Telemetry")
	FString BuildableClass;

	UPROPERTY(BlueprintReadOnly, Category = "DocMod|Telemetry")
	FVector Position = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "DocMod|Telemetry")
	FRotator Rotation = FRotator::ZeroRotator;
};

/** One item stack in an inventory, for FDocModManufacturerTelemetry. */
USTRUCT(BlueprintType)
struct FDocModInventoryItemTelemetry
{
	GENERATED_BODY()

	/** Item descriptor class path. */
	UPROPERTY(BlueprintReadOnly, Category = "DocMod|Telemetry")
	FString ItemClass;

	/** Human-readable item name, e.g. "Iron Plate". */
	UPROPERTY(BlueprintReadOnly, Category = "DocMod|Telemetry")
	FString ItemName;

	UPROPERTY(BlueprintReadOnly, Category = "DocMod|Telemetry")
	int32 Count = 0;
};

/**
 * Normalized telemetry for manufacturing buildings (AFGBuildableManufacturer
 * and siblings: Constructor/Assembler/Manufacturer/Smelter/Refinery/...) -
 * PLAN.md Phase 10, "machine recipes" / "machine inventories" / "machine
 * production status". Id has the same session-local-only caveat as
 * FDocModResourceNodeTelemetry.
 */
USTRUCT(BlueprintType)
struct FDocModManufacturerTelemetry
{
	GENERATED_BODY()

	/** Session-local only, NOT save-stable. */
	UPROPERTY(BlueprintReadOnly, Category = "DocMod|Telemetry")
	FString Id;

	UPROPERTY(BlueprintReadOnly, Category = "DocMod|Telemetry")
	FString BuildableClass;

	UPROPERTY(BlueprintReadOnly, Category = "DocMod|Telemetry")
	FVector Position = FVector::ZeroVector;

	/** Display name of the currently-set recipe, or empty if none is set. */
	UPROPERTY(BlueprintReadOnly, Category = "DocMod|Telemetry")
	FString Recipe;

	/** Currently-applied clock speed, as a percentage (100 = normal/1.0 potential). */
	UPROPERTY(BlueprintReadOnly, Category = "DocMod|Telemetry")
	float ClockSpeedPercent = 0.0f;

	/** "None" / "Producing" / "ProducingWithCrystal" / "Standby" / "Error" (from EProductionStatus). */
	UPROPERTY(BlueprintReadOnly, Category = "DocMod|Telemetry")
	FString ProductionStatus;

	/** Percent in [0,1] of the current production cycle. */
	UPROPERTY(BlueprintReadOnly, Category = "DocMod|Telemetry")
	float ProductionProgress = 0.0f;

	/** FactoryGame's own "how productive is this factory" measure. */
	UPROPERTY(BlueprintReadOnly, Category = "DocMod|Telemetry")
	float Productivity = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "DocMod|Telemetry")
	TArray<FDocModInventoryItemTelemetry> InputInventory;

	UPROPERTY(BlueprintReadOnly, Category = "DocMod|Telemetry")
	TArray<FDocModInventoryItemTelemetry> OutputInventory;
};
