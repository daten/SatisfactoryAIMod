// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIModOperationTypes.generated.h"

/**
 * Structured result for AIMod's write/mutation operations (PLAN.md
 * Phase 12+). Mirrors CLAUDE.md's required error shape
 * ({"success":false,"error":{"code","message"}}).
 *
 * Every write operation must, in order: validate input, verify target
 * identity, verify target type, verify the requested operation is
 * permitted, invoke the game operation, and report the ACTUAL result -
 * never assume success just because earlier validation passed.
 */
USTRUCT(BlueprintType)
struct FAIModOperationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AIMod|Operation")
	bool bSuccess = false;

	/** Machine-readable error code, e.g. "TARGET_NOT_FOUND". Empty on success. */
	UPROPERTY(BlueprintReadOnly, Category = "AIMod|Operation")
	FString ErrorCode;

	/** Human-readable error message. Empty on success. */
	UPROPERTY(BlueprintReadOnly, Category = "AIMod|Operation")
	FString ErrorMessage;

	/**
	 * Session-local id of a buildable this operation created (e.g.
	 * ConstructBuildingAtPosition) - so a caller can act on it further
	 * (SetManufacturerRecipe, future connect-conveyor/connect-power
	 * operations) without a separate world.buildables round-trip. Empty
	 * for operations that don't create a new buildable, and on failure.
	 * Same session-local-only caveat as FAIModResourceNodeTelemetry's Id.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "AIMod|Operation")
	FString ResultBuildableId;

	static FAIModOperationResult Success()
	{
		FAIModOperationResult Result;
		Result.bSuccess = true;
		return Result;
	}

	static FAIModOperationResult SuccessWithBuildableId(const FString& BuildableId)
	{
		FAIModOperationResult Result;
		Result.bSuccess = true;
		Result.ResultBuildableId = BuildableId;
		return Result;
	}

	static FAIModOperationResult Failure(const FString& Code, const FString& Message)
	{
		FAIModOperationResult Result;
		Result.bSuccess = false;
		Result.ErrorCode = Code;
		Result.ErrorMessage = Message;
		return Result;
	}
};
