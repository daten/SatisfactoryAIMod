// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIModTelemetryTypes.generated.h"

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
struct FAIModResourceNodeTelemetry
{
	GENERATED_BODY()

	/** Session-local only, NOT save-stable. See struct comment above. */
	UPROPERTY(BlueprintReadOnly, Category = "AIMod|Telemetry")
	FString Id;

	/** Human-readable resource name, e.g. "Iron Ore". */
	UPROPERTY(BlueprintReadOnly, Category = "AIMod|Telemetry")
	FString Resource;

	/** Resource descriptor class path, e.g. "/Game/.../Desc_OreIron.Desc_OreIron_C". */
	UPROPERTY(BlueprintReadOnly, Category = "AIMod|Telemetry")
	FString ResourceClass;

	/** "Impure" / "Normal" / "Pure" (FactoryGame's EResourcePurity display text). */
	UPROPERTY(BlueprintReadOnly, Category = "AIMod|Telemetry")
	FString Purity;

	UPROPERTY(BlueprintReadOnly, Category = "AIMod|Telemetry")
	FVector Position = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "AIMod|Telemetry")
	bool bOccupied = false;

	/**
	 * "Node" (a normal single-resource node - Miners/Water/Oil Pumps go
	 * here), "FrackingCore" (a Resource Well Pressurizer's real target -
	 * Recipe_FrackingSmasher), "FrackingSatellite" (a Resource Well
	 * Extractor's target - Recipe_FrackingExtractor), "Geyser", "Deposit",
	 * or "Invalid" (EResourceNodeType). Added 2026-08-27 alongside CoreId/
	 * SatelliteState, per explicit user request to support Resource Well
	 * Pressurizers/Extractors - see docs/resource-well-research.md.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "AIMod|Telemetry")
	FString NodeType = TEXT("Node");

	/**
	 * For a "FrackingSatellite" node only: the id of its
	 * AFGResourceNodeFrackingCore (the node a Pressurizer must be built
	 * on to eventually activate this satellite). Empty for every other
	 * NodeType. Session-local only, same caveat as Id.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "AIMod|Telemetry")
	FString CoreId;

	/**
	 * For a "FrackingSatellite" node only: "Untouched" / "Active" /
	 * "Inactive" (EFrackingSatelliteState). A Resource Well Extractor
	 * cannot be constructed on a satellite while this is "Untouched" -
	 * confirmed from source (UFGCDNeedsFrackingSatelliteNode,
	 * docs/resource-well-research.md) - poll this after powering a
	 * Pressurizer built on the satellite's core, rather than guessing
	 * when it's safe to build extractors. Empty for every other NodeType.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "AIMod|Telemetry")
	FString SatelliteState;
};

/**
 * Normalized telemetry for any placed AFGBuildable (PLAN.md Phase 10,
 * "buildings"). Generic fields only - see FAIModManufacturerTelemetry
 * for recipe/clock/production-status fields, which only apply to
 * manufacturing buildings (Constructor/Assembler/Manufacturer/...).
 *
 * Id has the same session-local-only caveat as
 * FAIModResourceNodeTelemetry - see that struct's comment.
 */
USTRUCT(BlueprintType)
struct FAIModBuildableTelemetry
{
	GENERATED_BODY()

	/** Session-local only, NOT save-stable. */
	UPROPERTY(BlueprintReadOnly, Category = "AIMod|Telemetry")
	FString Id;

	/** Buildable class path, e.g. "/Game/.../Build_ConstructorMk1.Build_ConstructorMk1_C". */
	UPROPERTY(BlueprintReadOnly, Category = "AIMod|Telemetry")
	FString BuildableClass;

	UPROPERTY(BlueprintReadOnly, Category = "AIMod|Telemetry")
	FVector Position = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "AIMod|Telemetry")
	FRotator Rotation = FRotator::ZeroRotator;
};

/** One item stack in an inventory, for FAIModManufacturerTelemetry. */
USTRUCT(BlueprintType)
struct FAIModInventoryItemTelemetry
{
	GENERATED_BODY()

	/** Item descriptor class path. */
	UPROPERTY(BlueprintReadOnly, Category = "AIMod|Telemetry")
	FString ItemClass;

	/** Human-readable item name, e.g. "Iron Plate". */
	UPROPERTY(BlueprintReadOnly, Category = "AIMod|Telemetry")
	FString ItemName;

	UPROPERTY(BlueprintReadOnly, Category = "AIMod|Telemetry")
	int32 Count = 0;
};

/**
 * Normalized telemetry for manufacturing buildings (AFGBuildableManufacturer
 * and siblings: Constructor/Assembler/Manufacturer/Smelter/Refinery/...) -
 * PLAN.md Phase 10, "machine recipes" / "machine inventories" / "machine
 * production status". Id has the same session-local-only caveat as
 * FAIModResourceNodeTelemetry.
 */
USTRUCT(BlueprintType)
struct FAIModManufacturerTelemetry
{
	GENERATED_BODY()

	/** Session-local only, NOT save-stable. */
	UPROPERTY(BlueprintReadOnly, Category = "AIMod|Telemetry")
	FString Id;

	UPROPERTY(BlueprintReadOnly, Category = "AIMod|Telemetry")
	FString BuildableClass;

	UPROPERTY(BlueprintReadOnly, Category = "AIMod|Telemetry")
	FVector Position = FVector::ZeroVector;

	/** Display name of the currently-set recipe, or empty if none is set. */
	UPROPERTY(BlueprintReadOnly, Category = "AIMod|Telemetry")
	FString Recipe;

	/** Currently-applied clock speed, as a percentage (100 = normal/1.0 potential). */
	UPROPERTY(BlueprintReadOnly, Category = "AIMod|Telemetry")
	float ClockSpeedPercent = 0.0f;

	/** "None" / "Producing" / "ProducingWithCrystal" / "Standby" / "Error" (from EProductionStatus). */
	UPROPERTY(BlueprintReadOnly, Category = "AIMod|Telemetry")
	FString ProductionStatus;

	/** Percent in [0,1] of the current production cycle. */
	UPROPERTY(BlueprintReadOnly, Category = "AIMod|Telemetry")
	float ProductionProgress = 0.0f;

	/** FactoryGame's own "how productive is this factory" measure. */
	UPROPERTY(BlueprintReadOnly, Category = "AIMod|Telemetry")
	float Productivity = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "AIMod|Telemetry")
	TArray<FAIModInventoryItemTelemetry> InputInventory;

	UPROPERTY(BlueprintReadOnly, Category = "AIMod|Telemetry")
	TArray<FAIModInventoryItemTelemetry> OutputInventory;
};

/**
 * One factory connection point (PLAN.md Phase 10, "conveyor connection
 * components" - and, combined with FAIModBuildableTelemetry, the raw
 * material for Phase 11's "conveyor topology"/world graph, which PLAN.md
 * says the EXTERNAL CONTROLLER should build - this struct only exposes
 * the facts, one row per connection point, not a constructed graph).
 *
 * A physical belt/pipe link between two buildings shows up as two rows:
 * one Output-direction row on the source buildable and one Input-direction
 * row on the destination, each pointing at the other via
 * ConnectedBuildableId. Id fields have the same session-local-only caveat
 * as the rest of this file's structs.
 */
USTRUCT(BlueprintType)
struct FAIModFactoryConnectionTelemetry
{
	GENERATED_BODY()

	/** Id of the AFGBuildable that owns this connection component. Session-local only. */
	UPROPERTY(BlueprintReadOnly, Category = "AIMod|Telemetry")
	FString OwnerBuildableId;

	/** "Input" / "Output" / "Any" / "SnapOnly" (EFactoryConnectionDirection). */
	UPROPERTY(BlueprintReadOnly, Category = "AIMod|Telemetry")
	FString Direction;

	UPROPERTY(BlueprintReadOnly, Category = "AIMod|Telemetry")
	bool bConnected = false;

	/** Id of the AFGBuildable on the other end, or empty if bConnected is false. Session-local only. */
	UPROPERTY(BlueprintReadOnly, Category = "AIMod|Telemetry")
	FString ConnectedBuildableId;

	/**
	 * The connector's real world position (UFGFactoryConnectionComponent::
	 * GetConnectorLocation(), no clearance offset). Added 2026-08-25 after
	 * a live belt-routing investigation (docs/demo-production-chain.md)
	 * needed this exact data ad hoc, via one-off diagnostic UE_LOG calls,
	 * to explain a "belt geometrically impossible" failure - this should
	 * be ordinary queryable telemetry, not something re-derived per
	 * experiment. Without it, an external planner has no way to know
	 * where a machine's connectors actually are relative to its own
	 * placement position/rotation.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "AIMod|Telemetry")
	FVector Position = FVector::ZeroVector;

	/**
	 * The connector's real outward-facing world normal
	 * (UFGFactoryConnectionComponent::GetConnectorNormal()). For an
	 * Output connection, items leave moving in this direction; for an
	 * Input connection, items must arrive moving in the OPPOSITE
	 * direction (approaching from outside, along +Normal, then entering
	 * the building along -Normal) - confirmed live: this is exactly what
	 * made a straight Smelter(output, faces +Y)-to-Constructor(input,
	 * faced -Y at the time) belt geometrically infeasible even though
	 * both connectors snapped correctly. An external planner needs this
	 * to choose a target position/orientation where two connectors'
	 * normals are compatible before ever calling world.placeBuilding.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "AIMod|Telemetry")
	FVector Normal = FVector::ZeroVector;
};

/**
 * Same shape/purpose as FAIModFactoryConnectionTelemetry, for pipes
 * (UFGPipeConnectionComponentBase) - added 2026-08-27 after discovering
 * live that "world.connections" only ever covered
 * UFGFactoryConnectionComponent (belts/machines/splitters), leaving no
 * way to read a fluid pipe's or hypertube's real connector
 * position/normal/facing - the exact information needed to plan a
 * straight (non-curving) pipe/tube run instead of guessing rotation, the
 * same way FAIModFactoryConnectionTelemetry already lets a caller do for
 * belts. Pipes and hypertubes share this one struct/RPC method rather
 * than getting separate ones, since they share the same real component
 * base class (UFGPipeConnectionComponentHyper adds no members of its own -
 * see docs/hypertube-research.md) - BIsHypertube distinguishes them.
 */
USTRUCT(BlueprintType)
struct FAIModPipeConnectionTelemetry
{
	GENERATED_BODY()

	/** Id of the AFGBuildable that owns this connection component. Session-local only. */
	UPROPERTY(BlueprintReadOnly, Category = "AIMod|Telemetry")
	FString OwnerBuildableId;

	/** "Any" / "Producer" / "Consumer" / "SnapOnly" (EPipeConnectionType). */
	UPROPERTY(BlueprintReadOnly, Category = "AIMod|Telemetry")
	FString ConnectionType;

	/**
	 * True if this connector is a UFGPipeConnectionComponentHyper
	 * (hypertube) rather than a regular fluid UFGPipeConnectionComponent -
	 * both derive from the same UFGPipeConnectionComponentBase this
	 * struct otherwise reads generically.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "AIMod|Telemetry")
	bool bIsHypertube = false;

	UPROPERTY(BlueprintReadOnly, Category = "AIMod|Telemetry")
	bool bConnected = false;

	/** Id of the AFGBuildable on the other end, or empty if bConnected is false. Session-local only. */
	UPROPERTY(BlueprintReadOnly, Category = "AIMod|Telemetry")
	FString ConnectedBuildableId;

	/** The connector's real world position (GetConnectorLocation(), no clearance offset). */
	UPROPERTY(BlueprintReadOnly, Category = "AIMod|Telemetry")
	FVector Position = FVector::ZeroVector;

	/**
	 * The connector's real outward-facing world normal
	 * (GetConnectorNormal()) - same "opposite normals dock cleanly"
	 * convention as FAIModFactoryConnectionTelemetry::Normal above.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "AIMod|Telemetry")
	FVector Normal = FVector::ZeroVector;
};

/**
 * The local player character's current position/rotation (PLAN.md
 * Phase 13/14). Exists specifically so RPC-driven placement
 * (ConstructBuildingAtPosition/"world.placeBuilding") has a real
 * reference point to place buildings near - an arbitrary existing
 * buildable's position is NOT a safe substitute, found live
 * (2026-08-25): the world can span thousands of units of elevation
 * across its map, and a buildable far from the player will make
 * ConstructBuildingAtPosition's ground trace (which searches only
 * within the PLAYER's current +/-1000 unit Z range) miss real terrain
 * entirely.
 */
USTRUCT(BlueprintType)
struct FAIModPlayerTelemetry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AIMod|Telemetry")
	FVector Position = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "AIMod|Telemetry")
	FRotator Rotation = FRotator::ZeroRotator;
};
