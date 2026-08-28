// Copyright Epic Games, Inc. All Rights Reserved.

#include "AIModSelfTest.h"
#include "AIMod.h"
#include "AIModFunctionLibrary.h"
#include "AIModTelemetryTypes.h"
#include "AIModOperationTypes.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace AIModSelfTest
{
	namespace
	{
		struct FCheckResult
		{
			FString Name;
			bool bPassed = false;
			FString Detail;
		};

		// File-local, not thread-local - RunAll() is only ever called from
		// the game thread (see the FWorldDelegates::OnWorldInitializedActors
		// binding in AIMod.cpp), and each call resets this at the start.
		TArray<FCheckResult> GResults;

		void Record(const FString& Name, bool bPassed, const FString& Detail = FString())
		{
			GResults.Add(FCheckResult{Name, bPassed, Detail});
			if (!bPassed)
			{
				UE_LOG(LogAIModAI, Error, TEXT("SelfTest FAILED: %s - %s"), *Name, *Detail);
			}
		}

		bool IsValidJson(const FString& JsonString)
		{
			TSharedPtr<FJsonObject> Parsed;
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
			return FJsonSerializer::Deserialize(Reader, Parsed) && Parsed.IsValid();
		}

		// The Detail message must depend on the actual result - a bug
		// caught on the first real run against a save (2026-08-24): all
		// four .json checks previously passed the same hardcoded
		// "did not parse as JSON" string as Detail regardless of
		// bPassed, so a PASS line read as if it had failed.
		void RecordJsonValid(const FString& Name, const FString& Json)
		{
			const bool bValid = IsValidJson(Json);
			Record(Name, bValid, bValid
				? FString::Printf(TEXT("%d bytes"), Json.Len())
				: TEXT("output did not parse as JSON"));
		}

		void CheckInterfaceVersion()
		{
			const FString Version = UAIModFunctionLibrary::GetInterfaceVersion();
			Record(TEXT("GetInterfaceVersion"), Version == TEXT("0.1.0"),
				FString::Printf(TEXT("expected \"0.1.0\", got \"%s\""), *Version));
		}

		void CheckResourceNodes(UWorld* World)
		{
			const TArray<FAIModResourceNodeTelemetry> Nodes = UAIModFunctionLibrary::GetResourceNodeTelemetry(World);

			bool bAllValid = true;
			FString FirstIssue;
			for (const FAIModResourceNodeTelemetry& Node : Nodes)
			{
				const bool bValidPurity = Node.Purity == TEXT("Impure") || Node.Purity == TEXT("Normal") || Node.Purity == TEXT("Pure");
				if (Node.Id.IsEmpty() || Node.Resource.IsEmpty() || !bValidPurity)
				{
					bAllValid = false;
					FirstIssue = FString::Printf(TEXT("node id=\"%s\" resource=\"%s\" purity=\"%s\""), *Node.Id, *Node.Resource, *Node.Purity);
					break;
				}
			}
			Record(TEXT("ResourceNodeTelemetry.shape"), bAllValid,
				bAllValid ? FString::Printf(TEXT("%d node(s)"), Nodes.Num()) : FirstIssue);

			const FString Json = UAIModFunctionLibrary::LogResourceNodesAsJson(World);
			RecordJsonValid(TEXT("ResourceNodeTelemetry.json"), Json);
		}

		void CheckBuildables(UWorld* World)
		{
			const TArray<FAIModBuildableTelemetry> Buildables = UAIModFunctionLibrary::GetBuildableTelemetry(World);

			bool bAllValid = true;
			for (const FAIModBuildableTelemetry& Buildable : Buildables)
			{
				if (Buildable.Id.IsEmpty() || Buildable.BuildableClass.IsEmpty())
				{
					bAllValid = false;
					break;
				}
			}
			Record(TEXT("BuildableTelemetry.shape"), bAllValid, FString::Printf(TEXT("%d buildable(s)"), Buildables.Num()));

			const FString Json = UAIModFunctionLibrary::LogBuildablesAsJson(World);
			RecordJsonValid(TEXT("BuildableTelemetry.json"), Json);
		}

		void CheckManufacturers(UWorld* World)
		{
			static const TArray<FString> ValidStatuses = {
				TEXT("None"), TEXT("Producing"), TEXT("ProducingWithCrystal"), TEXT("Standby"), TEXT("Error")
			};

			const TArray<FAIModManufacturerTelemetry> Manufacturers = UAIModFunctionLibrary::GetManufacturerTelemetry(World);

			bool bAllValid = true;
			FString FirstIssue;
			for (const FAIModManufacturerTelemetry& Manufacturer : Manufacturers)
			{
				if (!ValidStatuses.Contains(Manufacturer.ProductionStatus))
				{
					bAllValid = false;
					FirstIssue = FString::Printf(TEXT("unexpected productionStatus \"%s\""), *Manufacturer.ProductionStatus);
					break;
				}
			}
			Record(TEXT("ManufacturerTelemetry.shape"), bAllValid,
				bAllValid ? FString::Printf(TEXT("%d manufacturer(s)"), Manufacturers.Num()) : FirstIssue);

			const FString Json = UAIModFunctionLibrary::LogManufacturersAsJson(World);
			RecordJsonValid(TEXT("ManufacturerTelemetry.json"), Json);
		}

		void CheckFactoryConnections(UWorld* World)
		{
			const TArray<FAIModFactoryConnectionTelemetry> Connections = UAIModFunctionLibrary::GetFactoryConnectionTelemetry(World);

			bool bAllValid = true;
			for (const FAIModFactoryConnectionTelemetry& Connection : Connections)
			{
				if (Connection.bConnected && Connection.ConnectedBuildableId.IsEmpty())
				{
					bAllValid = false;
					break;
				}
			}
			Record(TEXT("FactoryConnectionTelemetry.shape"), bAllValid, FString::Printf(TEXT("%d connection point(s)"), Connections.Num()));

			// Real data-integrity check, not just "did it not crash": a
			// physical belt/pipe link produces two rows (an Output row on
			// the source, an Input row on the destination, each naming the
			// other - see docs/telemetry-protocol.md's "connections"
			// section). Every connected Output row should have a
			// reciprocal Input row on its peer.
			int32 UnmatchedCount = 0;
			for (const FAIModFactoryConnectionTelemetry& Connection : Connections)
			{
				if (Connection.Direction != TEXT("Output") || !Connection.bConnected)
				{
					continue;
				}
				const bool bFoundReciprocal = Connections.ContainsByPredicate([&Connection](const FAIModFactoryConnectionTelemetry& Other)
				{
					return Other.Direction == TEXT("Input")
						&& Other.OwnerBuildableId == Connection.ConnectedBuildableId
						&& Other.ConnectedBuildableId == Connection.OwnerBuildableId;
				});
				if (!bFoundReciprocal)
				{
					++UnmatchedCount;
				}
			}
			Record(TEXT("FactoryConnectionTelemetry.reciprocity"), UnmatchedCount == 0,
				FString::Printf(TEXT("%d Output connection(s) with no matching Input row on the peer"), UnmatchedCount));

			const FString Json = UAIModFunctionLibrary::LogFactoryConnectionsAsJson(World);
			RecordJsonValid(TEXT("FactoryConnectionTelemetry.json"), Json);
		}

		void CheckWriteOperationValidation(UWorld* World)
		{
			// Negative/validation-path only - see this file's header
			// comment. Never a positive-path mutation here.
			const FAIModOperationResult ClockResult = UAIModFunctionLibrary::SetManufacturerClockSpeed(
				World, TEXT("__AIModSelfTest_NonexistentId__"), 100.0f);
			Record(TEXT("SetManufacturerClockSpeed.rejectsUnknownTarget"),
				!ClockResult.bSuccess && ClockResult.ErrorCode == TEXT("TARGET_NOT_FOUND"),
				FString::Printf(TEXT("success=%s code=\"%s\""), ClockResult.bSuccess ? TEXT("true") : TEXT("false"), *ClockResult.ErrorCode));

			const FAIModOperationResult RecipeResult = UAIModFunctionLibrary::SetManufacturerRecipe(
				World, TEXT("__AIModSelfTest_NonexistentId__"), TEXT("/Game/DoesNotExist.DoesNotExist_C"));
			Record(TEXT("SetManufacturerRecipe.rejectsUnknownTarget"),
				!RecipeResult.bSuccess && RecipeResult.ErrorCode == TEXT("TARGET_NOT_FOUND"),
				FString::Printf(TEXT("success=%s code=\"%s\""), RecipeResult.bSuccess ? TEXT("true") : TEXT("false"), *RecipeResult.ErrorCode));
		}

		void LogSummary()
		{
			int32 PassCount = 0;
			for (const FCheckResult& Result : GResults)
			{
				if (Result.bPassed)
				{
					++PassCount;
				}
			}
			const int32 FailCount = GResults.Num() - PassCount;

			UE_LOG(LogAIModAI, Display, TEXT("===== AIMod self-test: %d passed, %d failed (of %d) ====="), PassCount, FailCount, GResults.Num());
			for (const FCheckResult& Result : GResults)
			{
				UE_LOG(LogAIModAI, Display, TEXT("  [%s] %s%s%s"),
					Result.bPassed ? TEXT("PASS") : TEXT("FAIL"), *Result.Name,
					Result.Detail.IsEmpty() ? TEXT("") : TEXT(" - "), *Result.Detail);
			}
			UE_LOG(LogAIModAI, Display, TEXT("============================================================"));
		}
	}

	void RunAll(UWorld* World)
	{
		if (!World)
		{
			return;
		}

		GResults.Reset();

		CheckInterfaceVersion();
		CheckResourceNodes(World);
		CheckBuildables(World);
		CheckManufacturers(World);
		CheckFactoryConnections(World);
		CheckWriteOperationValidation(World);

		LogSummary();
	}
}
