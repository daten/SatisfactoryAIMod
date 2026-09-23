// Copyright Epic Games, Inc. All Rights Reserved.

#include "AIModFunctionLibraryInternal.h"
using namespace AIModInternal;

FAIModOperationResult UAIModFunctionLibrary::DebugCheckExtractorPlacementOnTargetedNode(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}

	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Character)
	{
		return FAIModOperationResult::Failure(TEXT("NO_PLAYER"), TEXT("No local AFGCharacterPlayer (player index 0)"));
	}

	const FAIModResourceNodeTelemetry TargetTelemetry = GetTargetedResourceNode(WorldContextObject);
	if (TargetTelemetry.Id.IsEmpty())
	{
		return FAIModOperationResult::Failure(TEXT("NO_TARGET_NODE"), TEXT("Not currently looking at a resource node"));
	}

	// GetTargetedResourceNode returns normalized telemetry, not a
	// pointer (CLAUDE.md: no raw AActor*/UObject* across the
	// external-facing data model) - re-find the actual actor by id.
	AFGResourceNode* TargetNode = nullptr;
	for (TActorIterator<AFGResourceNode> It(World); It; ++It)
	{
		if (IsValid(*It) && It->GetPathName() == TargetTelemetry.Id)
		{
			TargetNode = *It;
			break;
		}
	}
	if (!TargetNode)
	{
		return FAIModOperationResult::Failure(TEXT("NODE_NOT_FOUND"), TEXT("Targeted node id could not be re-resolved"));
	}

	if (TargetNode->IsOccupied())
	{
		return FAIModOperationResult::Failure(TEXT("NODE_OCCUPIED"), TEXT("Targeted node is already occupied"));
	}

	// Scoped to solid-resource extraction (Miner Mk1) only for this first
	// experiment - see this function's header comment. Liquid/gas nodes
	// need a different buildable/recipe (Water/Oil Extractor) and are
	// deliberately out of scope here.
	const EResourceForm Form = UFGItemDescriptor::GetForm(TargetNode->GetResourceClass());
	if (Form != EResourceForm::RF_SOLID)
	{
		return FAIModOperationResult::Failure(TEXT("UNSUPPORTED_RESOURCE_FORM"),
			TEXT("This experiment only supports solid resource nodes (Miner Mk1) so far"));
	}

	// Diagnostic: IFGExtractableResourceInterface (FGExtractableResourceInterface.h,
	// which AFGResourceNodeBase implements - same interface IsOccupied()
	// above already calls) exposes a node-level "can an extractor even go
	// here" check, independent of the hologram's own clearance system
	// (see the clearanceDetector diagnostic). If this reports false, the
	// real answer is here, not in anything hologram-related.
	UE_LOG(LogAIModAI, Display, TEXT("DebugCheckExtractorPlacementOnTargetedNode: node diagnostics - CanPlaceResourceExtractor=%s HasAnyResources=%s CanBecomeOccupied=%s"),
		TargetNode->CanPlaceResourceExtractor() ? TEXT("true") : TEXT("false"),
		TargetNode->HasAnyResources() ? TEXT("true") : TEXT("false"),
		TargetNode->CanBecomeOccupied() ? TEXT("true") : TEXT("false"));

	// A real asset in Content/FactoryGame/Recipes/Buildings/
	// (see docs/extractor-placement-research.md).
	// This is the building's BUILD-COST recipe (what it costs to
	// construct), not a production recipe - Miner Mk1 has no production
	// recipe, it extracts automatically based on the node's purity.
	UClass* MinerRecipeClass = LoadObject<UClass>(nullptr, TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_MinerMk1.Recipe_MinerMk1_C"));
	if (!MinerRecipeClass || !MinerRecipeClass->IsChildOf(UFGRecipe::StaticClass()))
	{
		return FAIModOperationResult::Failure(TEXT("RECIPE_LOAD_FAILED"), TEXT("Failed to load Recipe_MinerMk1 as a UFGRecipe"));
	}
	const TSubclassOf<UFGRecipe> RecipeClass = MinerRecipeClass;

	// AFGHologram::SpawnHologramFromRecipe resolves the descriptor's
	// hologram class internally and spawns it - the one real "spawn a
	// hologram correctly" API found in docs/building-placement-research.md.
	AFGHologram* Hologram = AFGHologram::SpawnHologramFromRecipe(RecipeClass, Character, TargetNode->GetActorLocation(), Character);
	if (!Hologram)
	{
		return FAIModOperationResult::Failure(TEXT("HOLOGRAM_SPAWN_FAILED"), TEXT("SpawnHologramFromRecipe returned null"));
	}

	Hologram->SetConstructionInstigator(Character);

	// Synthetic FHitResult - see docs/extractor-placement-research.md §2:
	// no direct "set target node" setter exists on the hologram, only
	// FHitResult-driven entry points. This is the single most
	// load-bearing unverified assumption in this whole experiment -
	// whether a synthetic (non-traced) hit result produces correct
	// snapping is exactly what this function exists to find out.
	// Use the interface's own placement helpers rather than the node's
	// raw actor location/no rotation - FGExtractableResourceInterface.h's
	// own doc comments say these are "used by holograms to get the
	// correct location/rotation for snapping when placed on this
	// extractable resource," which is a closer match to what a real
	// AFGBuildGun-produced hit result would carry than a raw actor
	// transform.
	const FVector RawLocation = TargetNode->GetActorLocation();
	const FVector PlacementLocation = TargetNode->GetPlacementLocation(RawLocation);
	const FRotator PlacementRotation = TargetNode->GetPlacementRotation(RawLocation);
	UE_LOG(LogAIModAI, Display, TEXT("DebugCheckExtractorPlacementOnTargetedNode: rawLocation=%s placementLocation=%s placementRotation=%s"),
		*RawLocation.ToString(), *PlacementLocation.ToString(), *PlacementRotation.ToString());

	FHitResult SyntheticHit;
	SyntheticHit.Location = PlacementLocation;
	SyntheticHit.ImpactPoint = PlacementLocation;
	SyntheticHit.Normal = PlacementRotation.RotateVector(FVector::UpVector);
	SyntheticHit.ImpactNormal = SyntheticHit.Normal;
	SyntheticHit.HitObjectHandle = FActorInstanceHandle(TargetNode);
	SyntheticHit.bBlockingHit = true;
	if (UPrimitiveComponent* NodePrimitive = Cast<UPrimitiveComponent>(TargetNode->GetRootComponent()))
	{
		SyntheticHit.Component = NodePrimitive;
	}
	else
	{
		UE_LOG(LogAIModAI, Warning, TEXT("DebugCheckExtractorPlacementOnTargetedNode: resource node's root component is not a UPrimitiveComponent - synthetic hit result has no Component set"));
	}

	if (!Hologram->IsValidHitResult(SyntheticHit))
	{
		Hologram->Destroy();
		return FAIModOperationResult::Failure(TEXT("INVALID_HIT_RESULT"), TEXT("Hologram::IsValidHitResult rejected the synthetic hit result"));
	}

	// The single external trigger point for placement updates
	// (docs/extractor-placement-research.md §2) - handles snapping
	// internally rather than calling TrySnapToActor/SetHologramLocationAndRotation
	// separately.
	Hologram->UpdateHologramPlacement(SyntheticHit);

	// Checking CanConstruct() immediately, and even after several
	// manually-invoked Hologram->Tick(0.1f) calls, both
	// report a hard UFGCDInitializing ("Initializing") disqualifier.
	// FGHologram.h's InitializeClearanceData()/PostInitializeClearanceData()
	// split (:535-536) suggests clearance checking kicks off a world
	// query - most plausibly an async overlap - that doesn't resolve
	// within the same frame it starts in. A manually-invoked Tick() call
	// never gives an actual engine frame boundary a chance to complete
	// that query; polling across real World Tick cycles (via
	// SetTimerForNextTick) does. Poll every real tick rather than
	// blind-waiting a fixed duration, so this resolves in as few real
	// frames as the engine actually needs - MaxPollAttempts is only a
	// safety cap, not the expected case.
	struct FPollState
	{
		TWeakObjectPtr<AFGHologram> Hologram;
		TWeakObjectPtr<UWorld> World;
		FString NodeId;
		int32 AttemptsRemaining = 120; // safety cap - real ticks, not a fixed duration
		int32 AttemptsTaken = 0;
	};
	const TSharedRef<FPollState> PollState = MakeShared<FPollState>();
	PollState->Hologram = Hologram;
	PollState->World = World;
	PollState->NodeId = TargetTelemetry.Id;

	const TSharedRef<TFunction<void()>> PollFn = MakeShared<TFunction<void()>>();
	*PollFn = [PollState, PollFn]()
	{
		++PollState->AttemptsTaken;

		AFGHologram* PollHologram = PollState->Hologram.Get();
		UWorld* PollWorld = PollState->World.Get();
		if (!IsValid(PollHologram) || !PollWorld)
		{
			UE_LOG(LogAIModAI, Warning, TEXT("DebugCheckExtractorPlacementOnTargetedNode (deferred): hologram or world became invalid while polling (after %d tick(s))"), PollState->AttemptsTaken);
			return;
		}

		TArray<TSubclassOf<UFGConstructDisqualifier>> Disqualifiers;
		PollHologram->GetConstructDisqualifiers(Disqualifiers);
		const bool bStillInitializing = Disqualifiers.Contains(TSubclassOf<UFGConstructDisqualifier>(UFGCDInitializing::StaticClass()));

		--PollState->AttemptsRemaining;
		if (bStillInitializing && PollState->AttemptsRemaining > 0)
		{
			PollWorld->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
			return;
		}

		const bool bCanConstruct = PollHologram->CanConstruct();
		TArray<FString> DisqualifierTexts;
		for (const TSubclassOf<UFGConstructDisqualifier>& DisqualifierClass : Disqualifiers)
		{
			DisqualifierTexts.Add(FString::Printf(TEXT("%s (%s)"),
				*UFGConstructDisqualifier::GetDisqualifyingText(DisqualifierClass).ToString(),
				UFGConstructDisqualifier::GetIsSoftDisqualifier(DisqualifierClass) ? TEXT("soft") : TEXT("hard")));
		}
		const FString DisqualifierSummary = DisqualifierTexts.IsEmpty() ? TEXT("<none>") : FString::Join(DisqualifierTexts, TEXT("; "));

		// Diagnostic only, both public on AFGHologram (FGHologram.h:363,370)
		// - checks whether UFGCDInitializing never clearing
		// (see docs/extractor-placement-research.md) is
		// because the protected SetupClearanceDetector() (:540), which the
		// real AFGBuildGun explicitly calls after spawning a hologram
		// (paired with its own CleanupHologramClearanceDetection()), never
		// ran for a hologram spawned via SpawnHologramFromRecipe. A null
		// GetClearanceDetector() here would directly confirm that.
		UE_LOG(LogAIModAI, Display, TEXT("DebugCheckExtractorPlacementOnTargetedNode (deferred, resolved after %d real tick(s)): node=%s canConstruct=%s disqualifiers=[%s] clearanceDetector=%s hasClearance=%s"),
			PollState->AttemptsTaken, *PollState->NodeId, bCanConstruct ? TEXT("true") : TEXT("false"), *DisqualifierSummary,
			PollHologram->GetClearanceDetector() ? TEXT("set") : TEXT("null"),
			PollHologram->HasClearance() ? TEXT("true") : TEXT("false"));

		PollHologram->Destroy();
	};

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));

	return FAIModOperationResult::Failure(TEXT("PENDING"),
		TEXT("Scheduled - polling real ticks until UFGCDInitializing clears (or a safety cap is hit); see LogAIModAI for the real result"));
}


FAIModOperationResult UAIModFunctionLibrary::DebugCheckExtractorPlacementViaBuildGun(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}

	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Character)
	{
		return FAIModOperationResult::Failure(TEXT("NO_PLAYER"), TEXT("No local AFGCharacterPlayer (player index 0)"));
	}

	const FAIModResourceNodeTelemetry TargetTelemetry = GetTargetedResourceNode(WorldContextObject);
	if (TargetTelemetry.Id.IsEmpty())
	{
		return FAIModOperationResult::Failure(TEXT("NO_TARGET_NODE"), TEXT("Not currently looking at a resource node"));
	}

	AFGResourceNode* TargetNode = nullptr;
	for (TActorIterator<AFGResourceNode> It(World); It; ++It)
	{
		if (IsValid(*It) && It->GetPathName() == TargetTelemetry.Id)
		{
			TargetNode = *It;
			break;
		}
	}
	if (!TargetNode)
	{
		return FAIModOperationResult::Failure(TEXT("NODE_NOT_FOUND"), TEXT("Targeted node id could not be re-resolved"));
	}

	if (TargetNode->IsOccupied())
	{
		return FAIModOperationResult::Failure(TEXT("NODE_OCCUPIED"), TEXT("Targeted node is already occupied"));
	}

	const EResourceForm Form = UFGItemDescriptor::GetForm(TargetNode->GetResourceClass());
	if (Form != EResourceForm::RF_SOLID)
	{
		return FAIModOperationResult::Failure(TEXT("UNSUPPORTED_RESOURCE_FORM"),
			TEXT("This experiment only supports solid resource nodes (Miner Mk1) so far"));
	}

	UClass* MinerRecipeClass = LoadObject<UClass>(nullptr, TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_MinerMk1.Recipe_MinerMk1_C"));
	if (!MinerRecipeClass || !MinerRecipeClass->IsChildOf(UFGRecipe::StaticClass()))
	{
		return FAIModOperationResult::Failure(TEXT("RECIPE_LOAD_FAILED"), TEXT("Failed to load Recipe_MinerMk1 as a UFGRecipe"));
	}
	const TSubclassOf<UFGRecipe> RecipeClass = MinerRecipeClass;

	// Real, ordinary player-facing hotkey
	// (docs/buildgun-driven-placement-research.md §1) - equips the build
	// gun and enters build mode with this recipe in one call, same as a
	// player pressing a recipe hotkey. VISIBLE SIDE EFFECT: genuinely
	// changes what the player has equipped for the duration of this call
	// - always restored via UnequipBuildGun() below, on every exit path
	// from here on (including inside the poll lambda).
	Character->HotKeyRecipe(RecipeClass);

	AFGBuildGun* BuildGun = Character->GetBuildGun();
	if (!BuildGun)
	{
		Character->UnequipBuildGun();
		return FAIModOperationResult::Failure(TEXT("NO_BUILD_GUN"), TEXT("AFGCharacterPlayer::GetBuildGun() returned null"));
	}

	UFGBuildGunStateBuild* BuildState = Cast<UFGBuildGunStateBuild>(BuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD));
	if (!BuildState)
	{
		Character->UnequipBuildGun();
		return FAIModOperationResult::Failure(TEXT("NO_BUILD_STATE"), TEXT("Could not resolve UFGBuildGunStateBuild from the build gun"));
	}

	AFGHologram* Hologram = BuildState->GetHologram();
	if (!Hologram)
	{
		Character->UnequipBuildGun();
		return FAIModOperationResult::Failure(TEXT("HOLOGRAM_SPAWN_FAILED"), TEXT("HotKeyRecipe did not result in a spawned hologram - build gun may not have entered build mode as expected"));
	}

	// Same GetPlacementLocation/GetPlacementRotation fix as the
	// standalone experiment (docs/extractor-placement-research.md).
	const FVector RawLocation = TargetNode->GetActorLocation();
	const FVector PlacementLocation = TargetNode->GetPlacementLocation(RawLocation);
	const FRotator PlacementRotation = TargetNode->GetPlacementRotation(RawLocation);

	FHitResult SyntheticHit;
	SyntheticHit.Location = PlacementLocation;
	SyntheticHit.ImpactPoint = PlacementLocation;
	SyntheticHit.Normal = PlacementRotation.RotateVector(FVector::UpVector);
	SyntheticHit.ImpactNormal = SyntheticHit.Normal;
	SyntheticHit.HitObjectHandle = FActorInstanceHandle(TargetNode);
	SyntheticHit.bBlockingHit = true;
	if (UPrimitiveComponent* NodePrimitive = Cast<UPrimitiveComponent>(TargetNode->GetRootComponent()))
	{
		SyntheticHit.Component = NodePrimitive;
	}

	// Feed our synthetic hit result through the build gun's own mutable
	// GetHitResult() reference. NOTE (see ConstructBuildingNearPlayer's
	// matching fix and docs/buildgun-driven-placement-research.md): setting
	// this ONCE is
	// not enough - UFGBuildGunStateBuild::TickState_Implementation runs
	// its own real TraceForBuilding() every tick and overwrites this with
	// the player's live aim. Set it here anyway (harmless, and covers the
	// very first frame before any real trace has run), but the poll below
	// re-asserts our placement directly via UpdateHologramPlacement()
	// every tick, which is what actually makes the position deterministic.
	BuildGun->GetHitResult() = SyntheticHit;

	struct FPollState
	{
		TWeakObjectPtr<AFGHologram> Hologram;
		TWeakObjectPtr<AFGCharacterPlayer> Character;
		TWeakObjectPtr<UWorld> World;
		FString NodeId;
		FHitResult SyntheticHit;
		int32 AttemptsRemaining = 120; // safety cap - real ticks, not a fixed duration
		int32 AttemptsTaken = 0;
	};
	const TSharedRef<FPollState> PollState = MakeShared<FPollState>();
	PollState->Hologram = Hologram;
	PollState->Character = Character;
	PollState->World = World;
	PollState->NodeId = TargetTelemetry.Id;
	PollState->SyntheticHit = SyntheticHit;

	const TSharedRef<TFunction<void()>> PollFn = MakeShared<TFunction<void()>>();
	*PollFn = [PollState, PollFn]()
	{
		++PollState->AttemptsTaken;

		AFGHologram* PollHologram = PollState->Hologram.Get();
		UWorld* PollWorld = PollState->World.Get();
		AFGCharacterPlayer* PollCharacter = PollState->Character.Get();
		if (!IsValid(PollHologram) || !PollWorld)
		{
			UE_LOG(LogAIModAI, Warning, TEXT("DebugCheckExtractorPlacementViaBuildGun (deferred): hologram or world became invalid while polling (after %d tick(s))"), PollState->AttemptsTaken);
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			return;
		}

		// Re-assert our intended placement, overriding whatever the build
		// gun's own real per-tick trace did to the hologram since we last
		// checked - see the comment on BuildGun->GetHitResult() above.
		PollHologram->UpdateHologramPlacement(PollState->SyntheticHit);

		TArray<TSubclassOf<UFGConstructDisqualifier>> Disqualifiers;
		PollHologram->GetConstructDisqualifiers(Disqualifiers);
		const bool bStillInitializing = Disqualifiers.Contains(TSubclassOf<UFGConstructDisqualifier>(UFGCDInitializing::StaticClass()));

		--PollState->AttemptsRemaining;
		if (bStillInitializing && PollState->AttemptsRemaining > 0)
		{
			PollWorld->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
			return;
		}

		const bool bCanConstruct = PollHologram->CanConstruct();
		TArray<FString> DisqualifierTexts;
		for (const TSubclassOf<UFGConstructDisqualifier>& DisqualifierClass : Disqualifiers)
		{
			DisqualifierTexts.Add(FString::Printf(TEXT("%s (%s)"),
				*UFGConstructDisqualifier::GetDisqualifyingText(DisqualifierClass).ToString(),
				UFGConstructDisqualifier::GetIsSoftDisqualifier(DisqualifierClass) ? TEXT("soft") : TEXT("hard")));
		}
		const FString DisqualifierSummary = DisqualifierTexts.IsEmpty() ? TEXT("<none>") : FString::Join(DisqualifierTexts, TEXT("; "));

		UE_LOG(LogAIModAI, Display, TEXT("DebugCheckExtractorPlacementViaBuildGun (deferred, resolved after %d real tick(s)): node=%s canConstruct=%s disqualifiers=[%s]"),
			PollState->AttemptsTaken, *PollState->NodeId, bCanConstruct ? TEXT("true") : TEXT("false"), *DisqualifierSummary);

		// Never calls Construct()/Server_ConstructHologram - see this
		// function's header comment. Always restore the player's prior
		// equipped state, regardless of outcome.
		if (IsValid(PollCharacter))
		{
			PollCharacter->UnequipBuildGun();
		}
	};

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));

	return FAIModOperationResult::Failure(TEXT("PENDING"),
		TEXT("Scheduled via the real build gun - polling real ticks until UFGCDInitializing clears (or a safety cap is hit); see LogAIModAI for the real result"));
}


FAIModOperationResult UAIModFunctionLibrary::ConstructExtractorOnTargetedNode(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}

	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Character)
	{
		return FAIModOperationResult::Failure(TEXT("NO_PLAYER"), TEXT("No local AFGCharacterPlayer (player index 0)"));
	}

	const FAIModResourceNodeTelemetry TargetTelemetry = GetTargetedResourceNode(WorldContextObject);
	if (TargetTelemetry.Id.IsEmpty())
	{
		return FAIModOperationResult::Failure(TEXT("NO_TARGET_NODE"), TEXT("Not currently looking at a resource node"));
	}

	AFGResourceNode* TargetNode = nullptr;
	for (TActorIterator<AFGResourceNode> It(World); It; ++It)
	{
		if (IsValid(*It) && It->GetPathName() == TargetTelemetry.Id)
		{
			TargetNode = *It;
			break;
		}
	}
	if (!TargetNode)
	{
		return FAIModOperationResult::Failure(TEXT("NODE_NOT_FOUND"), TEXT("Targeted node id could not be re-resolved"));
	}

	if (TargetNode->IsOccupied())
	{
		return FAIModOperationResult::Failure(TEXT("NODE_OCCUPIED"), TEXT("Targeted node is already occupied"));
	}

	const EResourceForm Form = UFGItemDescriptor::GetForm(TargetNode->GetResourceClass());
	if (Form != EResourceForm::RF_SOLID)
	{
		return FAIModOperationResult::Failure(TEXT("UNSUPPORTED_RESOURCE_FORM"),
			TEXT("This experiment only supports solid resource nodes (Miner Mk1) so far"));
	}

	UClass* MinerRecipeClass = LoadObject<UClass>(nullptr, TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_MinerMk1.Recipe_MinerMk1_C"));
	if (!MinerRecipeClass || !MinerRecipeClass->IsChildOf(UFGRecipe::StaticClass()))
	{
		return FAIModOperationResult::Failure(TEXT("RECIPE_LOAD_FAILED"), TEXT("Failed to load Recipe_MinerMk1 as a UFGRecipe"));
	}
	const TSubclassOf<UFGRecipe> RecipeClass = MinerRecipeClass;

	// Real, ordinary player-facing hotkey - see
	// DebugCheckExtractorPlacementViaBuildGun's matching comment. VISIBLE
	// SIDE EFFECT, always restored via UnequipBuildGun().
	Character->HotKeyRecipe(RecipeClass);

	AFGBuildGun* BuildGun = Character->GetBuildGun();
	if (!BuildGun)
	{
		Character->UnequipBuildGun();
		return FAIModOperationResult::Failure(TEXT("NO_BUILD_GUN"), TEXT("AFGCharacterPlayer::GetBuildGun() returned null"));
	}

	UFGBuildGunStateBuild* BuildState = Cast<UFGBuildGunStateBuild>(BuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD));
	if (!BuildState)
	{
		Character->UnequipBuildGun();
		return FAIModOperationResult::Failure(TEXT("NO_BUILD_STATE"), TEXT("Could not resolve UFGBuildGunStateBuild from the build gun"));
	}

	AFGHologram* Hologram = BuildState->GetHologram();
	if (!Hologram)
	{
		Character->UnequipBuildGun();
		return FAIModOperationResult::Failure(TEXT("HOLOGRAM_SPAWN_FAILED"), TEXT("HotKeyRecipe did not result in a spawned hologram"));
	}

	const FVector RawLocation = TargetNode->GetActorLocation();
	const FVector PlacementLocation = TargetNode->GetPlacementLocation(RawLocation);
	const FRotator PlacementRotation = TargetNode->GetPlacementRotation(RawLocation);

	FHitResult SyntheticHit;
	SyntheticHit.Location = PlacementLocation;
	SyntheticHit.ImpactPoint = PlacementLocation;
	SyntheticHit.Normal = PlacementRotation.RotateVector(FVector::UpVector);
	SyntheticHit.ImpactNormal = SyntheticHit.Normal;
	SyntheticHit.HitObjectHandle = FActorInstanceHandle(TargetNode);
	SyntheticHit.bBlockingHit = true;
	if (UPrimitiveComponent* NodePrimitive = Cast<UPrimitiveComponent>(TargetNode->GetRootComponent()))
	{
		SyntheticHit.Component = NodePrimitive;
	}

	// See DebugCheckExtractorPlacementViaBuildGun's matching comment:
	// setting this once is not enough - the poll below re-asserts it via
	// UpdateHologramPlacement() every tick to override the build gun's
	// own real per-tick trace.
	BuildGun->GetHitResult() = SyntheticHit;

	struct FPollState
	{
		TWeakObjectPtr<AFGHologram> Hologram;
		TWeakObjectPtr<AFGCharacterPlayer> Character;
		TWeakObjectPtr<AFGResourceNode> TargetNode;
		TWeakObjectPtr<UWorld> World;
		FString NodeId;
		FHitResult SyntheticHit;
		int32 AttemptsRemaining = 120; // safety cap - real ticks, not a fixed duration
		int32 AttemptsTaken = 0;
	};
	const TSharedRef<FPollState> PollState = MakeShared<FPollState>();
	PollState->Hologram = Hologram;
	PollState->Character = Character;
	PollState->TargetNode = TargetNode;
	PollState->World = World;
	PollState->NodeId = TargetTelemetry.Id;
	PollState->SyntheticHit = SyntheticHit;

	const TSharedRef<TFunction<void()>> PollFn = MakeShared<TFunction<void()>>();
	*PollFn = [PollState, PollFn]()
	{
		++PollState->AttemptsTaken;

		AFGHologram* PollHologram = PollState->Hologram.Get();
		UWorld* PollWorld = PollState->World.Get();
		AFGCharacterPlayer* PollCharacter = PollState->Character.Get();
		AFGResourceNode* PollTargetNode = PollState->TargetNode.Get();
		if (!IsValid(PollHologram) || !PollWorld)
		{
			UE_LOG(LogAIModAI, Warning, TEXT("ConstructExtractorOnTargetedNode (deferred): hologram or world became invalid while polling (after %d tick(s)) - nothing built"), PollState->AttemptsTaken);
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			return;
		}

		PollHologram->UpdateHologramPlacement(PollState->SyntheticHit);

		TArray<TSubclassOf<UFGConstructDisqualifier>> Disqualifiers;
		PollHologram->GetConstructDisqualifiers(Disqualifiers);
		const bool bStillInitializing = Disqualifiers.Contains(TSubclassOf<UFGConstructDisqualifier>(UFGCDInitializing::StaticClass()));

		--PollState->AttemptsRemaining;
		if (bStillInitializing && PollState->AttemptsRemaining > 0)
		{
			PollWorld->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
			return;
		}

		const bool bCanConstruct = PollHologram->CanConstruct();
		TArray<FString> DisqualifierTexts;
		for (const TSubclassOf<UFGConstructDisqualifier>& DisqualifierClass : Disqualifiers)
		{
			DisqualifierTexts.Add(FString::Printf(TEXT("%s (%s)"),
				*UFGConstructDisqualifier::GetDisqualifyingText(DisqualifierClass).ToString(),
				UFGConstructDisqualifier::GetIsSoftDisqualifier(DisqualifierClass) ? TEXT("soft") : TEXT("hard")));
		}
		const FString DisqualifierSummary = DisqualifierTexts.IsEmpty() ? TEXT("<none>") : FString::Join(DisqualifierTexts, TEXT("; "));

		if (!bCanConstruct)
		{
			UE_LOG(LogAIModAI, Display, TEXT("ConstructExtractorOnTargetedNode (deferred, resolved after %d real tick(s)): CanConstruct()=false, NOT constructing - node=%s disqualifiers=[%s]"),
				PollState->AttemptsTaken, *PollState->NodeId, *DisqualifierSummary);
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			return;
		}

		// The only point where this function actually differs from
		// DebugCheckExtractorPlacementViaBuildGun - everything above is
		// the identical validated dry-run flow. InternalConstructHologram
		// is the same function Server_ConstructHologram's RPC delegates
		// to server-side (both public - see
		// docs/buildgun-driven-placement-research.md §5); calling it
		// directly here skips only the client->server RPC serialization
		// round-trip, a no-op in this same-process singleplayer/
		// listen-server context, and operates on the same live hologram
		// this poll already validated with CanConstruct()==true.
		AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(PollWorld);
		const FNetConstructionID ConstructionID = BuildableSubsystem ? BuildableSubsystem->GetNewNetConstructionID() : FNetConstructionID();

		AFGBuildGun* PollBuildGun = IsValid(PollCharacter) ? PollCharacter->GetBuildGun() : nullptr;
		UFGBuildGunStateBuild* PollBuildState = PollBuildGun ? Cast<UFGBuildGunStateBuild>(PollBuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD)) : nullptr;
		if (!PollBuildState)
		{
			UE_LOG(LogAIModAI, Error, TEXT("ConstructExtractorOnTargetedNode (deferred): lost the build state before constructing - aborting, nothing built"));
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			return;
		}

		PollBuildState->InternalConstructHologram(ConstructionID);

		// Confirmation, not just trust: if construction genuinely
		// succeeded, the node should now report occupied.
		const bool bNowOccupied = IsValid(PollTargetNode) && PollTargetNode->IsOccupied();
		UE_LOG(LogAIModAI, Display, TEXT("ConstructExtractorOnTargetedNode (deferred, resolved after %d real tick(s)): construction attempted via InternalConstructHologram - node=%s nodeNowOccupied=%s"),
			PollState->AttemptsTaken, *PollState->NodeId, bNowOccupied ? TEXT("true") : TEXT("false"));

		if (IsValid(PollCharacter))
		{
			PollCharacter->UnequipBuildGun();
		}
	};

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));

	return FAIModOperationResult::Failure(TEXT("PENDING"),
		TEXT("Scheduled via the real build gun - if CanConstruct() resolves true, a real Miner Mk1 WILL be constructed; see LogAIModAI for the real result"));
}


FAIModOperationResult UAIModFunctionLibrary::ConstructBuildingNearPlayer(UObject* WorldContextObject, const FString& RecipeClassPath)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}

	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Character)
	{
		return FAIModOperationResult::Failure(TEXT("NO_PLAYER"), TEXT("No local AFGCharacterPlayer (player index 0)"));
	}

	// Same validation as SetManufacturerRecipe - deliberately narrow, not
	// a generic "load any class by path" capability.
	UClass* ResolvedClass = LoadObject<UClass>(nullptr, *RecipeClassPath);
	if (!ResolvedClass || !ResolvedClass->IsChildOf(UFGRecipe::StaticClass()))
	{
		return FAIModOperationResult::Failure(TEXT("INVALID_RECIPE"),
			FString::Printf(TEXT("'%s' did not resolve to a UFGRecipe subclass"), *RecipeClassPath));
	}
	const TSubclassOf<UFGRecipe> RecipeClass = ResolvedClass;

	// Deliberately simple position choice, not a real "solve valid
	// placement" algorithm - see this function's header comment. A
	// candidate X/Y 800 units in front of the player, then a single
	// vertical line trace to find real ground there. CanConstruct() is
	// still the real gate on whether this spot actually works.
	const FVector PlayerLocation = Character->GetActorLocation();
	const FVector PlayerForward2D = Character->GetActorForwardVector().GetSafeNormal2D();
	const FVector CandidateXY = PlayerLocation + PlayerForward2D * 800.0f;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AIModConstructBuildingNearPlayer), false);
	QueryParams.AddIgnoredActor(Character);
	const FVector TraceStart(CandidateXY.X, CandidateXY.Y, PlayerLocation.Z + 100000.0f);
	const FVector TraceEnd(CandidateXY.X, CandidateXY.Y, PlayerLocation.Z - 100000.0f);

	FHitResult GroundHit;
	// Object-type query (WorldStatic/Dynamic), NOT ECC_Visibility: the landscape
	// ignores Visibility (a Visibility trace here found only placed buildables,
	// never real terrain). Same rationale/approach as FindGroundAtXY in
	// AIModFunctionLibraryInternal.h - the landscape is a WorldStatic object, so
	// an object-type trace hits it directly. Wide +/-100km window.
	FCollisionObjectQueryParams GroundObjParams;
	GroundObjParams.AddObjectTypesToQuery(ECC_WorldStatic);
	GroundObjParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	const bool bFoundGround = World->LineTraceSingleByObjectType(GroundHit, TraceStart, TraceEnd, GroundObjParams, QueryParams);

	FHitResult SyntheticHit;
	if (bFoundGround)
	{
		SyntheticHit = GroundHit;
	}
	else
	{
		// Fallback: no real ground found within range - assume flat
		// ground at the player's own Z, same degraded pattern used
		// elsewhere in this file when a real reference isn't available.
		UE_LOG(LogAIModAI, Warning, TEXT("ConstructBuildingNearPlayer: ground trace found nothing at (%.0f, %.0f) - falling back to player Z"), CandidateXY.X, CandidateXY.Y);
		SyntheticHit.Location = FVector(CandidateXY.X, CandidateXY.Y, PlayerLocation.Z);
		SyntheticHit.ImpactPoint = SyntheticHit.Location;
		SyntheticHit.Normal = FVector::UpVector;
		SyntheticHit.ImpactNormal = FVector::UpVector;
		SyntheticHit.bBlockingHit = true;
	}

	UE_LOG(LogAIModAI, Display, TEXT("ConstructBuildingNearPlayer: recipe=%s groundTraceHit=%s location=%s"),
		*RecipeClassPath, bFoundGround ? TEXT("true") : TEXT("false"), *SyntheticHit.Location.ToString());

	// Real, ordinary player-facing hotkey - see
	// ConstructExtractorOnTargetedNode's matching comment. VISIBLE SIDE
	// EFFECT, always restored via UnequipBuildGun().
	Character->HotKeyRecipe(RecipeClass);

	AFGBuildGun* BuildGun = Character->GetBuildGun();
	if (!BuildGun)
	{
		Character->UnequipBuildGun();
		return FAIModOperationResult::Failure(TEXT("NO_BUILD_GUN"), TEXT("AFGCharacterPlayer::GetBuildGun() returned null"));
	}

	UFGBuildGunStateBuild* BuildState = Cast<UFGBuildGunStateBuild>(BuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD));
	if (!BuildState)
	{
		Character->UnequipBuildGun();
		return FAIModOperationResult::Failure(TEXT("NO_BUILD_STATE"), TEXT("Could not resolve UFGBuildGunStateBuild from the build gun"));
	}

	AFGHologram* Hologram = BuildState->GetHologram();
	if (!Hologram)
	{
		Character->UnequipBuildGun();
		return FAIModOperationResult::Failure(TEXT("HOLOGRAM_SPAWN_FAILED"),
			TEXT("HotKeyRecipe did not result in a spawned hologram - recipe may not be a simple single-step buildable"));
	}

	// See DebugCheckExtractorPlacementViaBuildGun's matching comment -
	// setting this once is not enough:
	// UFGBuildGunStateBuild::TickState_Implementation runs its own real
	// TraceForBuilding() every tick and overwrites GetHitResult() with
	// the player's live aim, silently discarding whatever we set here.
	// The poll below re-asserts our intended placement directly via
	// UpdateHologramPlacement() every tick, which is what actually makes
	// the position deterministic regardless of where the player is
	// looking.
	BuildGun->GetHitResult() = SyntheticHit;

	struct FPollState
	{
		TWeakObjectPtr<AFGHologram> Hologram;
		TWeakObjectPtr<AFGCharacterPlayer> Character;
		TWeakObjectPtr<UWorld> World;
		FString RecipeClassPath;
		FHitResult SyntheticHit;
		int32 AttemptsRemaining = 120; // safety cap - real ticks, not a fixed duration
		int32 AttemptsTaken = 0;
	};
	const TSharedRef<FPollState> PollState = MakeShared<FPollState>();
	PollState->Hologram = Hologram;
	PollState->Character = Character;
	PollState->World = World;
	PollState->RecipeClassPath = RecipeClassPath;
	PollState->SyntheticHit = SyntheticHit;

	const TSharedRef<TFunction<void()>> PollFn = MakeShared<TFunction<void()>>();
	*PollFn = [PollState, PollFn]()
	{
		++PollState->AttemptsTaken;

		AFGHologram* PollHologram = PollState->Hologram.Get();
		UWorld* PollWorld = PollState->World.Get();
		AFGCharacterPlayer* PollCharacter = PollState->Character.Get();
		if (!IsValid(PollHologram) || !PollWorld)
		{
			UE_LOG(LogAIModAI, Warning, TEXT("ConstructBuildingNearPlayer (deferred): hologram or world became invalid while polling (after %d tick(s)) - nothing built"), PollState->AttemptsTaken);
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			return;
		}

		// The actual fix (see comment above BuildGun->GetHitResult()):
		// re-assert our intended placement, overriding whatever the build
		// gun's own real per-tick trace did to the hologram since we last
		// checked.
		PollHologram->UpdateHologramPlacement(PollState->SyntheticHit);

		TArray<TSubclassOf<UFGConstructDisqualifier>> Disqualifiers;
		PollHologram->GetConstructDisqualifiers(Disqualifiers);
		const bool bStillInitializing = Disqualifiers.Contains(TSubclassOf<UFGConstructDisqualifier>(UFGCDInitializing::StaticClass()));

		--PollState->AttemptsRemaining;
		if (bStillInitializing && PollState->AttemptsRemaining > 0)
		{
			PollWorld->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
			return;
		}

		const bool bCanConstruct = PollHologram->CanConstruct();
		TArray<FString> DisqualifierTexts;
		for (const TSubclassOf<UFGConstructDisqualifier>& DisqualifierClass : Disqualifiers)
		{
			DisqualifierTexts.Add(FString::Printf(TEXT("%s (%s)"),
				*UFGConstructDisqualifier::GetDisqualifyingText(DisqualifierClass).ToString(),
				UFGConstructDisqualifier::GetIsSoftDisqualifier(DisqualifierClass) ? TEXT("soft") : TEXT("hard")));
		}
		const FString DisqualifierSummary = DisqualifierTexts.IsEmpty() ? TEXT("<none>") : FString::Join(DisqualifierTexts, TEXT("; "));

		if (!bCanConstruct)
		{
			UE_LOG(LogAIModAI, Display, TEXT("ConstructBuildingNearPlayer (deferred, resolved after %d real tick(s)): CanConstruct()=false, NOT constructing - recipe=%s disqualifiers=[%s]"),
				PollState->AttemptsTaken, *PollState->RecipeClassPath, *DisqualifierSummary);
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			return;
		}

		AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(PollWorld);
		const FNetConstructionID ConstructionID = BuildableSubsystem ? BuildableSubsystem->GetNewNetConstructionID() : FNetConstructionID();

		AFGBuildGun* PollBuildGun = IsValid(PollCharacter) ? PollCharacter->GetBuildGun() : nullptr;
		UFGBuildGunStateBuild* PollBuildState = PollBuildGun ? Cast<UFGBuildGunStateBuild>(PollBuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD)) : nullptr;
		if (!PollBuildState)
		{
			UE_LOG(LogAIModAI, Error, TEXT("ConstructBuildingNearPlayer (deferred): lost the build state before constructing - aborting, nothing built"));
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			return;
		}

		const FVector ConstructLocation = PollHologram->GetActorLocation();
		PollBuildState->InternalConstructHologram(ConstructionID);

		UE_LOG(LogAIModAI, Display, TEXT("ConstructBuildingNearPlayer (deferred, resolved after %d real tick(s)): construction attempted via InternalConstructHologram - recipe=%s location=%s"),
			PollState->AttemptsTaken, *PollState->RecipeClassPath, *ConstructLocation.ToString());

		if (IsValid(PollCharacter))
		{
			PollCharacter->UnequipBuildGun();
		}
	};

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));

	return FAIModOperationResult::Failure(TEXT("PENDING"),
		TEXT("Scheduled via the real build gun - if CanConstruct() resolves true, the building WILL be constructed; see LogAIModAI for the real result"));
}


void UAIModFunctionLibrary::ConstructBuildingAtPosition(UObject* WorldContextObject, const FString& RecipeClassPath, float X, float Y, int32 RotationScrollDelta, float GridSnapSize, float ReferenceZ, bool bIgnoreGroundTrace, bool bIgnoreAimLocation, bool bIgnorePlayerEncroachment, bool bIgnoreClearance, bool bIgnoreInvalidFloor, bool bHasTargetYaw, float TargetYawDegrees, const FString& FaceBuildableId, TFunction<void(const FAIModOperationResult&)> OnComplete)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context")));
		return;
	}

	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Character)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_PLAYER"), TEXT("No local AFGCharacterPlayer (player index 0)")));
		return;
	}

	// LimitBuildDistance - a synthetic restriction, not a
	// real FactoryGame disqualifier: FGConstructDisqualifier.h has no
	// "too far from player" class at all, and this codebase builds at
	// 100,000+ unit distances with nothing ever rejecting it (unlike a
	// real player's Build Gun, which has a real reach limit). Simulates a
	// build distance limit (so structures can't be built clear on the
	// other side of the map) as a player-controlled mod setting - off by
	// default, preserving unrestricted behavior. 2D distance
	// only (X/Y) - Z isn't resolved yet at this point (ground trace
	// hasn't run), and "how far away" is naturally a horizontal notion
	// for this use case anyway. Checked here, before any hologram/poll
	// work starts, so a rejected request never has any construction
	// side effects to clean up.
	if (UAIModFunctionLibrary::GetAIModConfigBool(World, TEXT("LimitBuildDistance"), false))
	{
		const float MaxBuildDistance = UAIModFunctionLibrary::GetAIModConfigFloat(World, TEXT("MaxBuildDistance"), 8000.0f);
		const float DistanceFromPlayer = FVector::Dist2D(Character->GetActorLocation(), FVector(X, Y, 0.0f));
		if (DistanceFromPlayer > MaxBuildDistance)
		{
			OnComplete(FAIModOperationResult::Failure(TEXT("BUILD_DISTANCE_EXCEEDED"),
				FString::Printf(TEXT("Target is %.0f units from the player, exceeding the configured Max Build Distance of %.0f units (Limit RPC Build Distance From Player is enabled in AIMod's mod settings)"),
					DistanceFromPlayer, MaxBuildDistance)));
			return;
		}
	}

	UClass* ResolvedClass = LoadObject<UClass>(nullptr, *RecipeClassPath);
	if (!ResolvedClass || !ResolvedClass->IsChildOf(UFGRecipe::StaticClass()))
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("INVALID_RECIPE"),
			FString::Printf(TEXT("'%s' did not resolve to a UFGRecipe subclass"), *RecipeClassPath)));
		return;
	}
	const TSubclassOf<UFGRecipe> RecipeClass = ResolvedClass;

	// Safety guard (a real game CRASH, not just a bad result): this generic
	// single-step placement path must never be used for extractor recipes
	// (Miners, Water/Oil Pumps, Fracking buildings) - placing Recipe_MinerMk2
	// through here (no real resource node under it) resolves canConstruct=true
	// (unlike Recipe_MinerMk1 at a different location,
	// which correctly refused with "Must be placed on a Resource Node!" -
	// the disqualifier is evidently not reliably present for every
	// extractor/location combination) and proceeded into
	// InternalConstructHologram() -> AFGBuildableHologram::ConstructInstance()
	// -> AFGResourceExtractorHologram::ConfigureActor(), which unconditionally
	// asserts on a valid mSnappedExtractableResource - a hard engine
	// assertion, not a catchable disqualifier, that takes the whole game
	// process down. Extractors have a dedicated, correct entry point
	// (ConstructExtractorOnNode / world.placeExtractor) that actually
	// snaps a real resource node reference before construction - refuse
	// here unconditionally (not just another disqualifier bIgnore* could
	// bypass) rather than gamble on GetConstructDisqualifiers() catching
	// every case.
	const TSubclassOf<AFGBuildable> ResolvedBuildableClass = ResolveBuildableClassForRecipe(RecipeClassPath);
	if (ResolvedBuildableClass && ResolvedBuildableClass->IsChildOf(AFGBuildableResourceExtractorBase::StaticClass()))
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("WRONG_METHOD_FOR_EXTRACTOR"),
			FString::Printf(TEXT("'%s' is an extractor recipe - use world.placeExtractor (ConstructExtractorOnNode) instead, which snaps a real resource node reference. Constructing an extractor through world.placeBuilding without one is a confirmed CRASH (AFGResourceExtractorHologram::ConfigureActor's mSnappedExtractableResource assertion), not just a bad placement."), *RecipeClassPath)));
		return;
	}

	// Vehicles - same conservative posture as the extractor
	// refusal above, by structural analogy rather than a confirmed crash:
	// a Drone hologram has a mandatory mSnappedStation reference (see
	// UFGCDMustSnapStation in FGConstructDisqualifier.h) this generic
	// path never populates, exactly the shape of precondition the
	// extractor crash came from (a disqualifier existing specifically to
	// block construction without a reference the real Construct() path
	// may not itself defensively re-check). Route through
	// world.constructVehicle (ConstructVehicle) instead, which resolves
	// and snaps to a real Drone Station for drone recipes.
	if (const TSubclassOf<AFGVehicle> ResolvedVehicleClass = ResolveVehicleClassForRecipe(RecipeClassPath))
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("WRONG_METHOD_FOR_VEHICLE"),
			FString::Printf(TEXT("'%s' is a vehicle recipe - use world.constructVehicle (ConstructVehicle) instead. Drone recipes specifically require snapping to a real Drone Station reference this generic path never provides."), *RecipeClassPath)));
		return;
	}

	// Spline-snapped buildables - a CONFIRMED CRASH, not
	// just a structural analogy: placing Build_ConveyorMonitor through this
	// exact generic path crashes the whole game process
	// (EXCEPTION_ACCESS_VIOLATION reading address 0x10). Root-caused from
	// source:
	// AFGBuildableSplineSnappedBase::SetSnappedSplineBuildable() (the
	// base class's own real, non-stub inline implementation, confirmed
	// in FGBuildableSplineSnappedBase.h) unconditionally calls
	// buildable->Implements<UFGSplineBuildableInterface>() on its
	// parameter with no null check - AFGBuildableHologram::ConstructInstance()
	// calls this during Construct() with whatever got snapped during
	// placement, which is nullptr when there was never a real belt/pipe
	// to snap to (this generic path never attempts that snap at all).
	// Refuse unconditionally, same posture as the extractor refusal
	// above - there is currently no dedicated ConstructSplineSnapped*
	// entry point that does the real snap (only Build_ConveyorMonitor is
	// known to derive from this base in the installed headers, but the
	// refusal is keyed on the base class, not the one known subclass, so
	// it also covers any other spline-snapped buildable this project
	// hasn't encountered yet).
	if (ResolvedBuildableClass && ResolvedBuildableClass->IsChildOf(AFGBuildableSplineSnappedBase::StaticClass()))
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("WRONG_METHOD_FOR_SPLINE_SNAPPED"),
			FString::Printf(TEXT("'%s' derives from AFGBuildableSplineSnappedBase (e.g. Conveyor Monitor) - it must be snapped to a real belt/pipe buildable to construct safely. world.placeBuilding without one is a CONFIRMED CRASH (AFGBuildableSplineSnappedBase::SetSnappedSplineBuildable() dereferences a null snap target), not just a bad placement. No dedicated RPC for this exists yet - do not attempt to bypass this refusal."), *RecipeClassPath)));
		return;
	}

	// Caller-chosen general-purpose grid snap - see this function's
	// header doc comment. Applied before ground-tracing so the trace
	// itself (and everything downstream) sees the snapped coordinate.
	if (GridSnapSize > 0.0f)
	{
		const float SnappedX = FMath::RoundToFloat(X / GridSnapSize) * GridSnapSize;
		const float SnappedY = FMath::RoundToFloat(Y / GridSnapSize) * GridSnapSize;
		UE_LOG(LogAIModAI, Display, TEXT("ConstructBuildingAtPosition: gridSnapSize=%.1f snapped (%.1f, %.1f) -> (%.1f, %.1f)"),
			GridSnapSize, X, Y, SnappedX, SnappedY);
		X = SnappedX;
		Y = SnappedY;
	}

	// Same ground-finding approach as ConstructBuildingNearPlayer, just
	// driven by an explicit X/Y instead of a player-relative offset - see
	// that function's header comment for why this stays simple rather
	// than a real "solve valid placement" algorithm.
	//
	// ZSearchCenter: the vertical search range for the
	// ground trace defaults to the PLAYER's current Z +/-1000 units -
	// a real reliability problem, not just a
	// theoretical one: placing a building far from the player's current
	// elevation (e.g. player standing on top of another building, or on
	// an unrelated walkway) can make the trace miss real terrain
	// entirely and fall back to the player's own (irrelevant) Z, putting
	// the result nowhere near the intended location on ANY axis - not
	// just wrong height, since a bad Z anchor can also make the trace
	// hit the wrong piece of geometry entirely. ReferenceZ lets the
	// caller anchor the search to a KNOWN, FIXED point instead (e.g. an
	// existing buildable's own Z from world.buildables) - deterministic
	// regardless of where the player happens to be standing at call
	// time, for intentional, planned placement rather than player-relative
	// guessing. Sentinel
	// -1000000 (an unrealistic in-game Z) means "not provided" and
	// preserves the prior player-Z-anchored behavior.
	const bool bHasReferenceZ = ReferenceZ > -1000000.0f;
	if (bIgnoreGroundTrace && !bHasReferenceZ)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("MISSING_REFERENCE_Z"),
			TEXT("bIgnoreGroundTrace requires an explicit referenceZ - there is no ground trace to fall back to")));
		return;
	}

	FHitResult SyntheticHit;
	bool bGroundTraceFound = false;
	if (bIgnoreGroundTrace)
	{
		// See this function's header doc comment - deliberately skips
		// FindGroundAtXY entirely, since the whole point is to place at a
		// caller-computed Z no line trace can perturb (edge non-determinism,
		// or open-interior-space fall-through).
		SyntheticHit.Location = FVector(X, Y, ReferenceZ);
		SyntheticHit.ImpactPoint = SyntheticHit.Location;
		SyntheticHit.Normal = FVector::UpVector;
		SyntheticHit.ImpactNormal = FVector::UpVector;
		SyntheticHit.bBlockingHit = true;
	}
	else
	{
		const FVector PlayerLocation = Character->GetActorLocation();
		const float ZSearchCenter = bHasReferenceZ ? ReferenceZ : PlayerLocation.Z;

		const FGroundTraceResult GroundTrace = FindGroundAtXY(World, X, Y, ZSearchCenter, Character);
		if (!GroundTrace.bFound)
		{
			UE_LOG(LogAIModAI, Warning, TEXT("ConstructBuildingAtPosition: ground trace found nothing at (%.0f, %.0f) around Z=%.0f - falling back to that Z"), X, Y, ZSearchCenter);
		}
		SyntheticHit = GroundTrace.Hit;
		bGroundTraceFound = GroundTrace.bFound;
	}

	UE_LOG(LogAIModAI, Display, TEXT("ConstructBuildingAtPosition: recipe=%s ignoreGroundTrace=%s groundTraceHit=%s location=%s"),
		*RecipeClassPath, bIgnoreGroundTrace ? TEXT("true") : TEXT("false"), bGroundTraceFound ? TEXT("true") : TEXT("false"), *SyntheticHit.Location.ToString());

	// faceBuildableId - computes TargetYawDegrees from the
	// REAL placement location and an existing buildable's REAL position,
	// instead of requiring the caller to fetch both separately and do
	// this vector math themselves externally (the exact manual dance
	// otherwise repeated for splitters/mergers/hypertube entrances - see
	// docs/placement-lessons.md). Takes priority over an explicit
	// bHasTargetYaw/TargetYawDegrees if both are somehow provided, since
	// a resolved real target is more specific than a raw number.
	if (!FaceBuildableId.IsEmpty())
	{
		AFGBuildable* FaceTarget = FindBuildableById(World, FaceBuildableId);
		if (!FaceTarget)
		{
			OnComplete(FAIModOperationResult::Failure(TEXT("FACE_TARGET_NOT_FOUND"),
				FString::Printf(TEXT("faceBuildableId '%s' did not resolve to an existing buildable"), *FaceBuildableId)));
			return;
		}
		const FRotator FaceRotation = (FaceTarget->GetActorLocation() - SyntheticHit.Location).Rotation();
		bHasTargetYaw = true;
		TargetYawDegrees = FaceRotation.Yaw;
		UE_LOG(LogAIModAI, Display, TEXT("ConstructBuildingAtPosition: faceBuildableId=%s resolved yaw=%.1f"), *FaceBuildableId, TargetYawDegrees);
	}

	// "Invalid aim location!" persists even at ~100 units distance, directly
	// along the character's own capsule-facing direction, with a valid
	// ground trace hit - ruling out both distance and horizontal capsule
	// yaw as the cause. world.player/AFGCharacterPlayer::GetActorRotation()
	// only reflects capsule yaw, not the actual camera pitch/yaw
	// (decoupled in this game, as in most third-person-capable
	// characters) - if the player's camera happens to be pointed
	// somewhere unrelated (e.g. looking down at inventory) when this RPC
	// fires, some disqualifier apparently still consults the real
	// camera/control rotation despite UpdateHologramPlacement()
	// overriding the hologram's *position* every poll tick (see
	// docs/buildgun-driven-placement-research.md's "GetHitResult() alone
	// does not control final placement" finding - this looks like the
	// same class of problem, one layer deeper). Fix: point the
	// controller's ControlRotation at the synthetic hit location before
	// placing, same spirit as overriding GetHitResult() - makes
	// placement work regardless of where the camera actually happens to
	// be aimed, which is the whole point of RPC-driven placement.
	if (AController* Controller = Character->GetController())
	{
		const FRotator LookAtTarget = (SyntheticHit.Location - Character->GetActorLocation()).Rotation();

		// Only LookAtTarget's PITCH is load-bearing for the "Invalid aim
		// location!" fix above - the YAW is not. But
		// AFGHologram::UpdateHologramPlacement() (called every poll tick
		// below, stub source / unreadable) evidently re-derives the
		// hologram's own default (pre-Scroll) facing from the controller's
		// CURRENT yaw each tick: the exact
		// same RotationScrollDelta produces a DIFFERENT resolved yaw
		// depending only on where the player character happens to be
		// standing relative to the target - including a "yaw=0 expected"
		// case with the player nowhere near the target, and even a
		// completely isolated placement far from all other geometry. The
		// resolved (pre-scroll) yaw is consistently just the compass
		// bearing FROM the player's position TO the target, i.e. exactly
		// what LookAtTarget.Yaw computes here. This makes automated
		// multi-building layouts non-deterministic (scattered/misrotated),
		// not terrain, not gridSnapSize, not
		// per-building randomness. Pinning yaw to a fixed 0 baseline here
		// (independent of player position) makes RotationScrollDelta
		// finally reproducible: delta=0 is always due north, and each
		// scroll click's effect is now relative to that same fixed origin
		// every time, regardless of where the player stands when the RPC
		// fires.
		const FRotator DeterministicLook(LookAtTarget.Pitch, 0.0f, 0.0f);
		Controller->SetControlRotation(DeterministicLook);
	}

	Character->HotKeyRecipe(RecipeClass);

	AFGBuildGun* BuildGun = Character->GetBuildGun();
	if (!BuildGun)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_BUILD_GUN"), TEXT("AFGCharacterPlayer::GetBuildGun() returned null")));
		return;
	}

	UFGBuildGunStateBuild* BuildState = Cast<UFGBuildGunStateBuild>(BuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD));
	if (!BuildState)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_BUILD_STATE"), TEXT("Could not resolve UFGBuildGunStateBuild from the build gun")));
		return;
	}

	AFGHologram* Hologram = BuildState->GetHologram();
	if (!Hologram)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("HOLOGRAM_SPAWN_FAILED"),
			TEXT("HotKeyRecipe did not result in a spawned hologram - recipe may not be a simple single-step buildable")));
		return;
	}

	BuildGun->GetHitResult() = SyntheticHit;

	// Calibration: a single Scroll(N) call with
	// |N|>1 behaves identically to Scroll(1) - delta=1,2,3
	// all produce the same resolved yaw, and delta=-1,-2 produce the
	// SAME (positive) result as +1,+2, not a negative rotation - the
	// signature of a per-call clamped/smoothed input handler built for
	// one mouse-wheel notch per call, not an arbitrary-magnitude delta
	// encoded in a single call. Calling Scroll(sign) REPEATEDLY,
	// |RotationScrollDelta| times, mimics how a real player's wheel
	// input actually arrives (one small event per call) - applied once,
	// before the poll starts; Scroll()'s effect is expected to persist
	// across the repeated UpdateHologramPlacement() calls in the poll
	// below (mScrollRotation is a UPROPERTY member, not re-derived from
	// the hit each tick). Logging rotation before/after so the RPC
	// caller can read the real effect back via LogAIModAI without a
	// separate throwaway experiment.
	const FRotator RotationBeforeScroll = Hologram->GetActorRotation();
	if (bHasTargetYaw)
	{
		// See this function's header doc for why: Scroll() called N times
		// synchronously is non-linear for |N|>1, so an exact target yaw is
		// set directly instead. Re-asserted every poll tick below since
		// UpdateHologramPlacement() may re-derive/reset yaw each tick.
		Hologram->SetActorRotation(FRotator(0.0f, TargetYawDegrees, 0.0f));
	}
	else if (RotationScrollDelta != 0)
	{
		const int32 ScrollStep = RotationScrollDelta > 0 ? 1 : -1;
		for (int32 i = 0; i < FMath::Abs(RotationScrollDelta); ++i)
		{
			Hologram->Scroll(ScrollStep);
		}
	}
	UE_LOG(LogAIModAI, Display, TEXT("ConstructBuildingAtPosition: rotationScrollDelta=%d hasTargetYaw=%s targetYaw=%.1f rotationBeforeScroll=%s rotationAfterScroll=%s"),
		RotationScrollDelta, bHasTargetYaw ? TEXT("true") : TEXT("false"), TargetYawDegrees, *RotationBeforeScroll.ToString(), *Hologram->GetActorRotation().ToString());

	struct FPollState
	{
		TWeakObjectPtr<AFGHologram> Hologram;
		TWeakObjectPtr<AFGCharacterPlayer> Character;
		TWeakObjectPtr<UWorld> World;
		FString RecipeClassPath;
		FHitResult SyntheticHit;
		TFunction<void(const FAIModOperationResult&)> OnComplete;
		int32 AttemptsRemaining = 120; // safety cap - real ticks, not a fixed duration
		int32 AttemptsTaken = 0;
		bool bIgnoreAimLocation = false;
		bool bIgnorePlayerEncroachment = false;
		bool bIgnoreClearance = false;
		bool bIgnoreInvalidFloor = false;
		bool bHasTargetYaw = false;
		float TargetYawDegrees = 0.0f;
		bool bIgnoreGroundTrace = false; // pin literal position, see below
	};
	const TSharedRef<FPollState> PollState = MakeShared<FPollState>();
	PollState->Hologram = Hologram;
	PollState->Character = Character;
	PollState->World = World;
	PollState->RecipeClassPath = RecipeClassPath;
	PollState->SyntheticHit = SyntheticHit;
	PollState->bIgnoreAimLocation = bIgnoreAimLocation;
	PollState->bIgnorePlayerEncroachment = bIgnorePlayerEncroachment;
	PollState->bIgnoreClearance = bIgnoreClearance;
	PollState->bIgnoreInvalidFloor = bIgnoreInvalidFloor;
	PollState->bHasTargetYaw = bHasTargetYaw;
	PollState->TargetYawDegrees = TargetYawDegrees;
	PollState->bIgnoreGroundTrace = bIgnoreGroundTrace;
	PollState->OnComplete = MoveTemp(OnComplete);

	const TSharedRef<TFunction<void()>> PollFn = MakeShared<TFunction<void()>>();
	*PollFn = [PollState, PollFn]()
	{
		++PollState->AttemptsTaken;

		AFGHologram* PollHologram = PollState->Hologram.Get();
		UWorld* PollWorld = PollState->World.Get();
		AFGCharacterPlayer* PollCharacter = PollState->Character.Get();
		if (!IsValid(PollHologram) || !PollWorld)
		{
			UE_LOG(LogAIModAI, Warning, TEXT("ConstructBuildingAtPosition (deferred): hologram or world became invalid while polling (after %d tick(s)) - nothing built"), PollState->AttemptsTaken);
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("HOLOGRAM_INVALIDATED"), TEXT("Hologram or world became invalid while polling")));
			return;
		}

		// Re-assert every tick, not just once before the loop:
		// the one-time SetControlRotation() before this poll loop starts
		// can get overridden by the game's own ongoing camera/flight input
		// before a LATER tick's UpdateHologramPlacement() call reads it,
		// since AFGHologram evidently re-derives its default yaw from
		// whatever the controller's CURRENT rotation is on every tick (see
		// the fix comment above this poll loop for the original diagnosis).
		// A player standing still never showed this because their control
		// rotation wasn't changing tick-to-tick regardless of whether this
		// was re-applied - it only surfaces when the player is actively
		// looking around (e.g. mid-flight) while a poll spans multiple
		// ticks (bStillInitializing retries).
		if (IsValid(PollCharacter))
		{
			if (AController* PollController = PollCharacter->GetController())
			{
				const FRotator PollLookAtTarget = (PollState->SyntheticHit.Location - PollCharacter->GetActorLocation()).Rotation();
				PollController->SetControlRotation(FRotator(PollLookAtTarget.Pitch, 0.0f, 0.0f));
			}
		}

		PollHologram->UpdateHologramPlacement(PollState->SyntheticHit);

		// COMPOSITE HOLOGRAM CHILD RE-SYNC (needed for RPC-built train
		// loops). A train station/platform hologram (AFGTrainPlatformHologram)
		// owns a CHILD AFGRailroadTrackHologram - the integrated platform track
		// that carries the station's rail connectors. UpdateHologramPlacement()
		// above synced that child to the parent's CURRENT transform, but the
		// location-pin and target-yaw re-assert below move/rotate ONLY the
		// parent actor (they do not re-run OnHologramTransformUpdated, which is
		// what repositions the children). Result: a station placed with a yaw
		// kept its integrated track - and every rail connector - pointing world
		// +X, so constructRailroadTrack between two stations could only ever
		// produce a straight east-west line and a curved/closed rail LOOP was
		// impossible to build (verifiable via world.splineGeometry). Capture
		// each child's transform RELATIVE to the parent now (while correctly
		// synced) so we can re-apply it against the parent's pinned/rotated
		// transform after the two calls below. Only needed when we move the
		// parent independently of the hit (a pinned position and/or a target
		// yaw); the child list is empty for ordinary single-actor buildables so
		// this is a no-op for them.
		TArray<TPair<TWeakObjectPtr<AFGHologram>, FTransform>> ChildRelativeTransforms;
		if (PollState->bHasTargetYaw || PollState->bIgnoreGroundTrace)
		{
			const FTransform ParentTransformWhenSynced = PollHologram->GetActorTransform();
			for (AFGHologram* ChildHologram : PollHologram->GetHologramChildren())
			{
				if (IsValid(ChildHologram))
				{
					ChildRelativeTransforms.Emplace(ChildHologram,
						ChildHologram->GetActorTransform().GetRelativeTransform(ParentTransformWhenSynced));
				}
			}
		}

		// Pin the literal requested position when the caller asked for
		// ignoreGroundTrace (guards a real bug: machines
		// and foundations embedding into each other on a supposedly-flat
		// platform). ignoreGroundTrace promises a literal (x, y, z)
		// placement, but UpdateHologramPlacement() STILL runs the
		// hologram's snap logic - and foundations snap-STACK vertically
		// onto adjacent foundations, so a whole row requested at one z
		// landed at a mix of z=151 and z=301 (a 150-unit foundation-height
		// snap), and machines then snapped onto whichever height was
		// under them. Forcing the actor's location back to the synthetic
		// hit every tick (after UpdateHologramPlacement re-snapped it, so
		// the disqualifier check below and the eventual construct both see
		// the pinned transform) defeats the snap-stack and makes
		// ignoreGroundTrace genuinely literal on all three axes. Only
		// applied under ignoreGroundTrace - the normal ground-trace path
		// still snaps as before, which is what a caller passing real
		// coordinates-to-resolve wants.
		if (PollState->bIgnoreGroundTrace)
		{
			PollHologram->SetActorLocation(PollState->SyntheticHit.Location);
		}

		// Re-assert the exact target yaw every tick too, for the same
		// reason the camera look direction is re-asserted above -
		// UpdateHologramPlacement() may have just reset it.
		if (PollState->bHasTargetYaw)
		{
			PollHologram->SetActorRotation(FRotator(0.0f, PollState->TargetYawDegrees, 0.0f));
		}

		// Re-sync composite child holograms captured above against the parent's
		// NOW pinned + rotated transform, so a station's integrated track (and
		// its rail connectors) follows the station's yaw. This is what makes a
		// rotated station usable for curved/looping rail: with connectors that
		// actually point along the station's facing, constructRailroadTrack can
		// form real arcs instead of only straight east-west lines. Re-applied
		// every tick (cheap) so the final pre-construct state is correct.
		if (ChildRelativeTransforms.Num() > 0)
		{
			const FTransform ParentTransformNow = PollHologram->GetActorTransform();
			for (const TPair<TWeakObjectPtr<AFGHologram>, FTransform>& ChildEntry : ChildRelativeTransforms)
			{
				if (AFGHologram* ChildHologram = ChildEntry.Key.Get())
				{
					ChildHologram->SetActorTransform(ChildEntry.Value * ParentTransformNow);
				}
			}
		}

		TArray<TSubclassOf<UFGConstructDisqualifier>> Disqualifiers;
		PollHologram->GetConstructDisqualifiers(Disqualifiers);
		const bool bStillInitializing = Disqualifiers.Contains(TSubclassOf<UFGConstructDisqualifier>(UFGCDInitializing::StaticClass()));

		--PollState->AttemptsRemaining;
		if (bStillInitializing && PollState->AttemptsRemaining > 0)
		{
			PollWorld->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
			return;
		}

		// Real CanConstruct() (whatever its internal logic actually is -
		// stub source, unreadable) is intentionally NOT used as the gate
		// here when any bIgnore* flag is set. Instead: replicate the
		// documented "any non-soft (hard) disqualifier blocks" rule
		// ourselves via the same GetIsSoftDisqualifier() query used for
		// logging everywhere else in this file, skipping specific
		// disqualifier classes the caller explicitly opted to ignore.
		// Player-proximity/camera-direction/clearance gates don't scale for
		// large, autonomous, multi-building layouts; ignoring them accepts
		// the risk of invalid terrain collisions in exchange.
		// This does NOT bypass FactoryGame's OWN validation inside
		// InternalConstructHologram() itself (unknown/unverified from
		// source) - only AIMod's decision to attempt construction.
		// UnlimitedResources - a player-controlled mod
		// setting (AIModConfiguration.h), NOT another bIgnore* request
		// param like the flags above - the caller can't opt into this,
		// only the player can via the settings menu. Computed once per
		// poll tick, not per disqualifier, to avoid a config lookup per
		// entry in Disqualifiers.
		const bool bUnlimitedResources = UAIModFunctionLibrary::GetAIModConfigBool(PollWorld, TEXT("UnlimitedResources"), false);

		bool bCanConstruct = true;
		TArray<FString> DisqualifierTexts;
		for (const TSubclassOf<UFGConstructDisqualifier>& DisqualifierClass : Disqualifiers)
		{
			const bool bIgnoredByFlag =
				(PollState->bIgnoreAimLocation && DisqualifierClass == UFGCDInvalidAimLocation::StaticClass()) ||
				(PollState->bIgnorePlayerEncroachment && DisqualifierClass == UFGCDEncroachingPlayer::StaticClass()) ||
				(PollState->bIgnoreClearance && DisqualifierClass == UFGCDEncroachingClearance::StaticClass()) ||
				(PollState->bIgnoreInvalidFloor && DisqualifierClass == UFGCDInvalidFloor::StaticClass()) ||
				(bUnlimitedResources && DisqualifierClass == UFGCDUnaffordable::StaticClass());
			const bool bIsSoft = UFGConstructDisqualifier::GetIsSoftDisqualifier(DisqualifierClass);
			if (!bIgnoredByFlag && !bIsSoft)
			{
				bCanConstruct = false;
			}
			DisqualifierTexts.Add(FString::Printf(TEXT("%s (%s%s)"),
				*UFGConstructDisqualifier::GetDisqualifyingText(DisqualifierClass).ToString(),
				bIsSoft ? TEXT("soft") : TEXT("hard"), bIgnoredByFlag ? TEXT(", ignored") : TEXT("")));
		}
		const FString DisqualifierSummary = DisqualifierTexts.IsEmpty() ? TEXT("<none>") : FString::Join(DisqualifierTexts, TEXT("; "));

		UE_LOG(LogAIModAI, Display, TEXT("ConstructBuildingAtPosition (deferred, resolved after %d real tick(s)): resolvedRotation=%s canConstruct=%s"),
			PollState->AttemptsTaken, *PollHologram->GetActorRotation().ToString(), bCanConstruct ? TEXT("true") : TEXT("false"));

		if (!bCanConstruct)
		{
			UE_LOG(LogAIModAI, Display, TEXT("ConstructBuildingAtPosition (deferred, resolved after %d real tick(s)): CanConstruct()=false, NOT constructing - recipe=%s disqualifiers=[%s]"),
				PollState->AttemptsTaken, *PollState->RecipeClassPath, *DisqualifierSummary);
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("CANNOT_CONSTRUCT"), DisqualifierSummary));
			return;
		}

		AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(PollWorld);
		const FNetConstructionID ConstructionID = BuildableSubsystem ? BuildableSubsystem->GetNewNetConstructionID() : FNetConstructionID();

		AFGBuildGun* PollBuildGun = IsValid(PollCharacter) ? PollCharacter->GetBuildGun() : nullptr;
		UFGBuildGunStateBuild* PollBuildState = PollBuildGun ? Cast<UFGBuildGunStateBuild>(PollBuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD)) : nullptr;
		if (!PollBuildState)
		{
			UE_LOG(LogAIModAI, Error, TEXT("ConstructBuildingAtPosition (deferred): lost the build state before constructing - aborting, nothing built"));
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("Lost the build state before constructing")));
			return;
		}

		const FVector ConstructLocation = PollHologram->GetActorLocation();
		PollBuildState->InternalConstructHologram(ConstructionID);

		// Identify the newly-constructed buildable by proximity to where
		// we just built - InternalConstructHologram is void and there's
		// no direct return value.
		//
		// CLASS FILTER (guards a real bug): "nearest buildable of ANY class
		// within 200 units" is wrong on a dense site - a foundation placed
		// directly
		// under an existing merger returned the MERGER's id as
		// result.buildableId (the merger's actor origin was ~15 units
		// from the foundation's center; the foundation's own origin was
		// farther). Filter candidates to the recipe's own resolved
		// buildable class, exactly like the lightweight branch below
		// already does - "nothing else should exist nearby" was never
		// true for attachments/machines stacked on the thing being
		// placed. Falls back to the unfiltered search only when the
		// class can't be resolved at all (better a possibly-wrong id
		// plus a warning than none).
		FString ConstructedBuildableId;
		const TSubclassOf<AFGBuildable> ConstructedBuildableClass = ResolveBuildableClassForRecipe(PollState->RecipeClassPath);
		if (BuildableSubsystem)
		{
			if (!ConstructedBuildableClass)
			{
				UE_LOG(LogAIModAI, Warning, TEXT("ConstructBuildingAtPosition (deferred): could not resolve a buildable class for recipe %s - the returned buildableId is a best-effort nearest-of-any-class match"), *PollState->RecipeClassPath);
			}
			float BestDistSq = TNumericLimits<float>::Max();
			AFGBuildable* BestMatch = nullptr;
			for (AFGBuildable* Candidate : BuildableSubsystem->GetAllBuildablesRef())
			{
				if (!IsValid(Candidate)) { continue; }
				if (ConstructedBuildableClass && !Candidate->IsA(ConstructedBuildableClass)) { continue; }
				const float DistSq = FVector::DistSquared(Candidate->GetActorLocation(), ConstructLocation);
				if (DistSq < BestDistSq)
				{
					BestDistSq = DistSq;
					BestMatch = Candidate;
				}
			}
			if (BestMatch && BestDistSq < FMath::Square(200.0f))
			{
				ConstructedBuildableId = BestMatch->GetPathName();
			}
		}

		if (ConstructedBuildableId.IsEmpty())
		{
			// The regular-actor search above found nothing - the recipe
			// may have produced a lightweight buildable instead
			// (foundations, likely other mass-placed pieces - see
			// docs/lightweight-buildable-research.md). Search
			// AFGLightweightBuildableSubsystem's instances for the
			// recipe's buildable class by proximity, same 200-unit
			// tolerance as the regular-actor search.
			if (AFGLightweightBuildableSubsystem* LightweightSubsystem = AFGLightweightBuildableSubsystem::Get(PollWorld))
			{
				const TSubclassOf<AFGBuildable> BuildableClass = ResolveBuildableClassForRecipe(PollState->RecipeClassPath);
				if (const TArray<FRuntimeBuildableInstanceData>* Instances = BuildableClass ? LightweightSubsystem->GetAllLightweightBuildableInstances().Find(BuildableClass) : nullptr)
				{
					float BestDistSq = TNumericLimits<float>::Max();
					int32 BestIndex = INDEX_NONE;
					for (int32 Index = 0; Index < Instances->Num(); ++Index)
					{
						const FRuntimeBuildableInstanceData& InstanceData = (*Instances)[Index];
						if (!InstanceData.IsValid()) { continue; }
						const float DistSq = FVector::DistSquared(InstanceData.Transform.GetLocation(), ConstructLocation);
						if (DistSq < BestDistSq)
						{
							BestDistSq = DistSq;
							BestIndex = Index;
						}
					}
					if (BestIndex != INDEX_NONE && BestDistSq < FMath::Square(200.0f))
					{
						ConstructedBuildableId = MakeLightweightBuildableId(BuildableClass, BestIndex);
					}
				}
			}
		}

		UE_LOG(LogAIModAI, Display, TEXT("ConstructBuildingAtPosition (deferred, resolved after %d real tick(s)): construction attempted via InternalConstructHologram - recipe=%s location=%s id=%s"),
			PollState->AttemptsTaken, *PollState->RecipeClassPath, *ConstructLocation.ToString(), *ConstructedBuildableId);

		if (IsValid(PollCharacter))
		{
			PollCharacter->UnequipBuildGun();
		}

		PollState->OnComplete(ConstructedBuildableId.IsEmpty()
			? FAIModOperationResult::Success()
			: FAIModOperationResult::SuccessWithBuildableId(ConstructedBuildableId));
	};

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
}



FAIModOperationResult UAIModFunctionLibrary::DismantleBuildable(UObject* WorldContextObject, const FString& BuildableId)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}

	AFGBuildable* Buildable = nullptr;

	if (IsLightweightBuildableId(BuildableId))
	{
		// Lightweight buildables (foundations, likely walls/other
		// mass-placed pieces - see docs/lightweight-buildable-research.md)
		// aren't actors, so there's nothing to look up directly. Resolve
		// the class+index, then materialize a real, temporary
		// AFGBuildable* via FindOrSpawnBuildableForRuntimeData() and reuse
		// the exact same Execute_Dismantle() call below -
		// AFGBuildable::Dismantle_Implementation() already has a
		// dedicated mIsLightweightTemporary branch that correctly calls
		// AFGLightweightBuildableSubsystem::RemoveByInstanceIndex() for
		// us (confirmed by reading FGBuildable.cpp), so there's no need
		// to duplicate that logic here.
		FString ClassPath;
		int32 Index = INDEX_NONE;
		if (!ParseLightweightBuildableId(BuildableId, ClassPath, Index))
		{
			return FAIModOperationResult::Failure(TEXT("INVALID_REQUEST"),
				FString::Printf(TEXT("'%s' is not a well-formed lightweight buildable id"), *BuildableId));
		}

		UClass* ResolvedClass = LoadObject<UClass>(nullptr, *ClassPath);
		const TSubclassOf<AFGBuildable> BuildableClass = (ResolvedClass && ResolvedClass->IsChildOf(AFGBuildable::StaticClass())) ? ResolvedClass : nullptr;
		if (!BuildableClass)
		{
			return FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"),
				FString::Printf(TEXT("'%s' did not resolve to an AFGBuildable subclass"), *ClassPath));
		}

		AFGLightweightBuildableSubsystem* LightweightSubsystem = AFGLightweightBuildableSubsystem::Get(World);
		FRuntimeBuildableInstanceData* RuntimeData = LightweightSubsystem ? LightweightSubsystem->GetRuntimeDataForBuildableClassAndIndex(BuildableClass, Index) : nullptr;
		if (!RuntimeData || !RuntimeData->IsValid())
		{
			return FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"),
				FString::Printf(TEXT("No lightweight buildable found with id '%s'"), *BuildableId));
		}

		bool bDidSpawn = false;
		FInstanceToTemporaryBuildable* Temporary = LightweightSubsystem->FindOrSpawnBuildableForRuntimeData(BuildableClass, RuntimeData, Index, bDidSpawn);
		if (!Temporary || !Temporary->IsValid())
		{
			return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"),
				FString::Printf(TEXT("Failed to materialize a temporary buildable for '%s'"), *BuildableId));
		}
		Buildable = Temporary->Buildable;
	}
	else
	{
		Buildable = FindBuildableById(World, BuildableId);
	}

	// Vehicles: world.constructVehicle produces a real AFGVehicle, but
	// AFGVehicle is not an AFGBuildable (per source -
	// AFGDriveablePawn, a separate hierarchy - same reason
	// world.buildables can't see it either), so FindBuildableById above
	// always misses it, leaving no RPC way to remove a constructed
	// vehicle at all. AFGVehicle DOES implement IFGDismantleInterface
	// (same interface AFGBuildable does), so the exact same
	// Execute_CanDismantle/Execute_GetChildDismantleActors/Execute_Dismantle
	// calls below work unchanged once a target actor is found - this
	// only needed a second id-resolution path, not new dismantle logic.
	AActor* DismantleTarget = Buildable;
	if (!DismantleTarget)
	{
		for (TActorIterator<AFGVehicle> It(World); It; ++It)
		{
			if (IsValid(*It) && It->GetPathName() == BuildableId)
			{
				DismantleTarget = *It;
				break;
			}
		}
	}

	if (!DismantleTarget)
	{
		return FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"),
			FString::Printf(TEXT("No buildable or vehicle found with id '%s'"), *BuildableId));
	}

	if (!IFGDismantleInterface::Execute_CanDismantle(DismantleTarget))
	{
		return FAIModOperationResult::Failure(TEXT("CANNOT_DISMANTLE"),
			FString::Printf(TEXT("'%s' cannot currently be dismantled (already dismantled, or has an un-dismantled parent)"), *BuildableId));
	}

	// Some buildables reference a separate, independently-existing child
	// actor that Dismantle_Implementation() itself does not clean up -
	// e.g. AFGBuildablePipeline's mFlowIndicator (a distinct
	// AFGBuildablePipelineFlowIndicator actor, not a child component that
	// would auto-destroy with its owner). GetChildDismantleActors exists
	// specifically for this ("If we want to dismantle something else
	// along with this, then add it through this" - FGDismantleInterface.h);
	// the real player-driven path (UFGBuildGunStateDismantle) consults it,
	// but Execute_Dismantle() on the buildable itself does not call it
	// automatically. Without this, deleting a pipe via world.deleteBuilding
	// leaves its fluid-fill indicator floating in place. Gather while the
	// buildable is still valid, dismantle each child first, then the
	// buildable itself.
	TArray<AActor*> ChildDismantleActors;
	IFGDismantleInterface::Execute_GetChildDismantleActors(DismantleTarget, ChildDismantleActors);
	for (AActor* ChildActor : ChildDismantleActors)
	{
		if (IsValid(ChildActor) && ChildActor->Implements<UFGDismantleInterface>()
			&& IFGDismantleInterface::Execute_CanDismantle(ChildActor))
		{
			IFGDismantleInterface::Execute_Dismantle(ChildActor);
		}
	}

	// Real construction-cost refund: Execute_Dismantle() does not refund
	// anything itself - GetDismantleRefund() is a SEPARATE interface
	// function the real player-driven dismantle path
	// (UFGBuildGunStateDismantle) calls independently (per
	// FGDismantleInterface.h's own doc comments). It must be called
	// explicitly here or world.deleteBuilding refunds nothing.
	//
	// Computed BEFORE dismantling (the target must still be valid). The
	// refund is credited to the player's carried inventory via AddStack, and
	// anything that DOESN'T fit is dropped as the vanilla dismantle crate at
	// the buildable's location - never silently destroyed. With a full
	// player inventory, AddStack's partial add silently discards every
	// refund item that needs a NEW inventory slot, logging the loss but
	// destroying the items.
	// Vanilla's own dismantle path (UFGBuildGunStateDismantle) spawns a
	// ground crate for exactly this overflow via FDismantleHelpers, so we do
	// the same here instead of relying on partial-add-and-hope).
	//
	// Capture the drop location NOW, while DismantleTarget is still valid -
	// after Execute_Dismantle it is pending-kill and GetActorLocation is no
	// longer trustworthy.
	TArray<FInventoryStack> RefundStacks;
	if (DismantleTarget->Implements<UFGDismantleInterface>())
	{
		IFGDismantleInterface::Execute_GetDismantleRefund(DismantleTarget, RefundStacks, /*noBuildCostEnabled=*/false);
	}
	const FVector RefundDropLocation = DismantleTarget->GetActorLocation();

	// Real, safe dismantle - see this function's header doc comment.
	// AFGBuildable::Dismantle_Implementation()/AFGVehicle's own
	// implementation handle connection cleanup, inventory locking/
	// emptying, subsystem deregistration, and network-replicated actor
	// destruction; this is not AActor::Destroy().
	IFGDismantleInterface::Execute_Dismantle(DismantleTarget);

	// Read-after-write consistency: the actual actor destruction can lag
	// this call by up to a frame - a merger deleted here still appears in an
	// immediately-following world.connections read, and worse, a
	// placeBuilding ground trace moments later can STACK a new attachment
	// on top of the not-yet-destroyed actor (its collision is still
	// live). Force the dying actor inert RIGHT NOW so nothing can trace
	// onto it during its final frame. The one-frame staleness in listing
	// RPCs (world.connections/world.buildables) can still occur - callers
	// should allow a tick before treating dependent reads as
	// authoritative (documented in RPC_REFERENCE.md).
	//
	// A plain null check (not IsValid()) is correct here: Execute_Dismantle
	// leaves the actor pending-kill, for which IsValid() returns FALSE -
	// exactly the state we need to act on. A pending-kill actor's memory
	// stays alive until GC, and SetActorEnableCollision/SetActorHiddenInGame
	// are safe to call on it - making it un-traceable is the whole point.
	//
	// This actor-level block is only belt-and-braces for plain-actor
	// buildables: splitter/merger-class buildables live in FactoryGame's
	// instanced-mesh system (AbstractInstanceManager), whose collision
	// actor-level calls can't touch and whose cleanup rides the deferred
	// destruction. The REAL fix for those is in AIModHttpServerSubsystem's
	// world.deleteBuilding handler: the HTTP response is deferred two
	// real ticks, so a caller's next request always runs after the
	// engine's own cleanup has finished. Do not rely on this block
	// alone for same-frame delete-then-place consistency.
	if (DismantleTarget)
	{
		DismantleTarget->SetActorEnableCollision(false);
		DismantleTarget->SetActorHiddenInGame(true);
	}

	int32 RefundedStackCount = 0;
	int32 CratedStackCount = 0;
	if (RefundStacks.Num() > 0)
	{
		AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
		UFGInventoryComponent* PlayerInventory = Character ? Character->GetInventory() : nullptr;

		// Credit what fits to the player's carried inventory; collect the
		// genuine overflow (the part that needs slots the inventory doesn't
		// have) and, only if there is any, drop it as the vanilla dismantle
		// crate. If it all fits, no crate is spawned - identical to the old
		// happy path, just without the silent loss when it doesn't.
		//
		// If there is no local player/inventory at all, the entire refund is
		// overflow and goes straight into a crate (previously it was logged
		// as "lost" and destroyed).
		TArray<FInventoryStack> OverflowStacks;
		for (const FInventoryStack& Stack : RefundStacks)
		{
			if (!Stack.HasItems())
			{
				continue;
			}

			int32 Added = 0;
			if (PlayerInventory)
			{
				Added = PlayerInventory->AddStack(Stack, /*allowPartialAdd=*/true);
				if (Added > 0)
				{
					++RefundedStackCount;
				}
			}

			if (Added < Stack.NumItems)
			{
				OverflowStacks.Add(FInventoryStack(Stack.NumItems - Added, Stack.Item.GetItemClass()));
			}
		}

		if (OverflowStacks.Num() > 0)
		{
			// NoActor variant: DismantleTarget is already pending-kill from
			// Execute_Dismantle above, so it is passed only as the actor to
			// ignore when tracing for a crate placement spot, never as a live
			// reference. RefundDropLocation was captured while it was valid.
			// This is the same FACTORYGAME_API entry point vanilla dismantle
			// uses for overflow, so if it ever fails to link the build will
			// say so and this must fall back to refusing the dismantle when
			// the refund won't fit (see docs/placement-lessons.md).
			FDismantleHelpers::DropRefundOnGroundNoActor(World, RefundDropLocation, DismantleTarget, OverflowStacks, Character);
			CratedStackCount = OverflowStacks.Num();
		}
	}

	UE_LOG(LogAIModAI, Display, TEXT("DismantleBuildable: %s (%d child actor(s) dismantled, %d refund stack(s) credited to inventory, %d overflow stack(s) dropped as a dismantle crate)"), *BuildableId, ChildDismantleActors.Num(), RefundedStackCount, CratedStackCount);

	return FAIModOperationResult::Success();
}


void UAIModFunctionLibrary::ConstructExtractorOnNode(UObject* WorldContextObject, const FString& NodeId, const FString& RecipeClassPath, TFunction<void(const FAIModOperationResult&)> OnComplete)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context")));
		return;
	}

	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Character)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_PLAYER"), TEXT("No local AFGCharacterPlayer (player index 0)")));
		return;
	}

	// Iterates AFGResourceNodeBase, not AFGResourceNode, for a wider search.
	// AFGResourceNode covers normal nodes and Fracking Satellites
	// (AFGResourceNodeFrackingSatellite : AFGResourceNode), but
	// AFGResourceNodeFrackingCore is NOT an AFGResourceNode - it derives
	// from AFGResourceNodeBase directly - so a Resource Well Pressurizer's
	// target node is only reachable via the base class. See this
	// function's header doc comment / docs/resource-well-research.md.
	AFGResourceNodeBase* TargetNode = nullptr;
	for (TActorIterator<AFGResourceNodeBase> It(World); It; ++It)
	{
		if (IsValid(*It) && It->GetPathName() == NodeId)
		{
			TargetNode = *It;
			break;
		}
	}
	if (!TargetNode)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NODE_NOT_FOUND"), FString::Printf(TEXT("No resource node found with id '%s'"), *NodeId)));
		return;
	}

	if (TargetNode->IsOccupied())
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NODE_OCCUPIED"), TEXT("Targeted node is already occupied")));
		return;
	}

	// LimitBuildDistance - see ConstructBuildingAtPosition's
	// identical check/comment for the full rationale (player-controlled
	// mod setting, off by default, synthetic restriction that doesn't
	// exist in the base game).
	if (UAIModFunctionLibrary::GetAIModConfigBool(World, TEXT("LimitBuildDistance"), false))
	{
		const float MaxBuildDistance = UAIModFunctionLibrary::GetAIModConfigFloat(World, TEXT("MaxBuildDistance"), 8000.0f);
		const float DistanceFromPlayer = FVector::Dist2D(Character->GetActorLocation(), TargetNode->GetActorLocation());
		if (DistanceFromPlayer > MaxBuildDistance)
		{
			OnComplete(FAIModOperationResult::Failure(TEXT("BUILD_DISTANCE_EXCEEDED"),
				FString::Printf(TEXT("Target node is %.0f units from the player, exceeding the configured Max Build Distance of %.0f units (Limit RPC Build Distance From Player is enabled in AIMod's mod settings)"),
					DistanceFromPlayer, MaxBuildDistance)));
			return;
		}
	}

	// This function does NOT manually enforce an RF_SOLID-only gate - see
	// this function's header doc comment for why. The engine-side gating
	// already does this correctly for every extractor type, including
	// rejecting a mismatched recipe/node pairing via
	// UFGCDNeedsFrackingCoreNode/UFGCDNeedsFrackingSatelliteNode - trust
	// CanConstruct() for it the same way this function does for every other
	// disqualifier.
	UClass* ResolvedRecipeClass = LoadObject<UClass>(nullptr, *RecipeClassPath);
	if (!ResolvedRecipeClass || !ResolvedRecipeClass->IsChildOf(UFGRecipe::StaticClass()))
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("INVALID_RECIPE"), FString::Printf(TEXT("'%s' did not resolve to a UFGRecipe subclass"), *RecipeClassPath)));
		return;
	}
	const TSubclassOf<UFGRecipe> RecipeClass = ResolvedRecipeClass;

	Character->HotKeyRecipe(RecipeClass);

	AFGBuildGun* BuildGun = Character->GetBuildGun();
	if (!BuildGun)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_BUILD_GUN"), TEXT("AFGCharacterPlayer::GetBuildGun() returned null")));
		return;
	}

	UFGBuildGunStateBuild* BuildState = Cast<UFGBuildGunStateBuild>(BuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD));
	if (!BuildState)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_BUILD_STATE"), TEXT("Could not resolve UFGBuildGunStateBuild from the build gun")));
		return;
	}

	AFGHologram* Hologram = BuildState->GetHologram();
	if (!Hologram)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("HOLOGRAM_SPAWN_FAILED"), TEXT("HotKeyRecipe did not result in a spawned hologram")));
		return;
	}

	const FVector RawLocation = TargetNode->GetActorLocation();
	const FVector PlacementLocation = TargetNode->GetPlacementLocation(RawLocation);
	const FRotator PlacementRotation = TargetNode->GetPlacementRotation(RawLocation);

	FHitResult SyntheticHit;
	SyntheticHit.Location = PlacementLocation;
	SyntheticHit.ImpactPoint = PlacementLocation;
	SyntheticHit.Normal = PlacementRotation.RotateVector(FVector::UpVector);
	SyntheticHit.ImpactNormal = SyntheticHit.Normal;
	SyntheticHit.HitObjectHandle = FActorInstanceHandle(TargetNode);
	SyntheticHit.bBlockingHit = true;
	if (UPrimitiveComponent* NodePrimitive = Cast<UPrimitiveComponent>(TargetNode->GetRootComponent()))
	{
		SyntheticHit.Component = NodePrimitive;
	}
	// Distance must be non-zero. If this synthetic hit's Distance field is
	// left at the FHitResult default (0.f) while every other field
	// (Location, Normal, Component, HitObjectHandle) is populated to look
	// like a real trace result, AFGResourceExtractorHologram's internal
	// placement validation sanity-checks Distance and rejects the
	// zero-distance "hit" with UFGCDNeedsResourceNode ("Must be placed on a
	// Resource Node!") even though the hit otherwise correctly identifies
	// the target node. A real build-gun trace always has a positive
	// camera-to-hit distance; set one.
	SyntheticHit.Distance = FVector::Dist(Character->GetActorLocation(), PlacementLocation);

	BuildGun->GetHitResult() = SyntheticHit;

	// Player-independence (the same fix used in
	// ConstructConveyorBelt/ConstructConveyorLift/ConstructPipe - see their
	// comments): without it, placement fails with "Invalid aim location!"
	// even for a fully valid, unoccupied Fracking Core node. Point the
	// controller at a deterministic target (the real placement location,
	// never the player's actual aim), reasserted every poll tick below.
	const FRotator ExtractorDeterministicLook = (PlacementLocation - Character->GetActorLocation()).Rotation();
	if (AController* ExtractorController = Character->GetController())
	{
		ExtractorController->SetControlRotation(ExtractorDeterministicLook);
	}

	// Explicit TrySnapToActor() call, matching every other click-driven
	// Construct* function in this file (belts/pipes/lifts).
	// AFGResourceExtractorHologram's own TrySnapToActor() override calls
	// TrySnapToExtractableResource() internally (per source) to populate
	// mSnappedExtractableResource, which the extractor needs correctly
	// populated to actually function, not just to pass the disqualifier
	// check.
	Hologram->UpdateHologramPlacement(SyntheticHit);
	Hologram->TrySnapToActor(SyntheticHit);

	struct FPollState
	{
		TWeakObjectPtr<AFGHologram> Hologram;
		TWeakObjectPtr<AFGCharacterPlayer> Character;
		TWeakObjectPtr<AFGResourceNodeBase> TargetNode;
		TWeakObjectPtr<UWorld> World;
		FString NodeId;
		FHitResult SyntheticHit;
		FRotator DeterministicLook;
		TFunction<void(const FAIModOperationResult&)> OnComplete;
		int32 AttemptsRemaining = 120; // safety cap - real ticks, not a fixed duration
		int32 AttemptsTaken = 0;
	};
	const TSharedRef<FPollState> PollState = MakeShared<FPollState>();
	PollState->Hologram = Hologram;
	PollState->Character = Character;
	PollState->TargetNode = TargetNode;
	PollState->World = World;
	PollState->NodeId = NodeId;
	PollState->SyntheticHit = SyntheticHit;
	PollState->DeterministicLook = ExtractorDeterministicLook;
	PollState->OnComplete = MoveTemp(OnComplete);

	const TSharedRef<TFunction<void()>> PollFn = MakeShared<TFunction<void()>>();
	*PollFn = [PollState, PollFn]()
	{
		++PollState->AttemptsTaken;

		AFGHologram* PollHologram = PollState->Hologram.Get();
		UWorld* PollWorld = PollState->World.Get();
		AFGCharacterPlayer* PollCharacter = PollState->Character.Get();
		AFGResourceNodeBase* PollTargetNode = PollState->TargetNode.Get();
		if (!IsValid(PollHologram) || !PollWorld)
		{
			UE_LOG(LogAIModAI, Warning, TEXT("ConstructExtractorOnNode (deferred): hologram or world became invalid while polling (after %d tick(s)) - nothing built"), PollState->AttemptsTaken);
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("HOLOGRAM_INVALIDATED"), TEXT("Hologram or world became invalid while polling")));
			return;
		}

		// Re-assert every tick - see ConstructConveyorBelt's identical block
		// for the full rationale (player camera movement between ticks can
		// still drag the resolved result off a one-time value).
		if (IsValid(PollCharacter))
		{
			if (AController* PollController = PollCharacter->GetController())
			{
				PollController->SetControlRotation(PollState->DeterministicLook);
			}
		}

		PollHologram->UpdateHologramPlacement(PollState->SyntheticHit);

		TArray<TSubclassOf<UFGConstructDisqualifier>> Disqualifiers;
		PollHologram->GetConstructDisqualifiers(Disqualifiers);
		const bool bStillInitializing = Disqualifiers.Contains(TSubclassOf<UFGConstructDisqualifier>(UFGCDInitializing::StaticClass()));

		--PollState->AttemptsRemaining;
		if (bStillInitializing && PollState->AttemptsRemaining > 0)
		{
			PollWorld->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
			return;
		}

		// Player-independence - same manual disqualifier-ignore
		// pattern as ConstructConveyorBelt, replacing the real (opaque)
		// CanConstruct() this function used to call directly.
		// UnlimitedResources - see ConstructBuildingAtPosition's
		// comment on this being a player-controlled mod setting, not a
		// per-call flag.
		const bool bUnlimitedResources = UAIModFunctionLibrary::GetAIModConfigBool(PollWorld, TEXT("UnlimitedResources"), false);

		bool bCanConstruct = true;
		TArray<FString> DisqualifierTexts;
		for (const TSubclassOf<UFGConstructDisqualifier>& DisqualifierClass : Disqualifiers)
		{
			// UFGCDEncroachingPlayer: "A player is in the
			// way!" is exactly as player-dependent as aim location for an
			// autonomous RPC build - the idle real character standing
			// somewhere near a remote build site blocked real placements
			// (worked around
			// with world.teleportPlayer). Same accepted risk
			// as ignoring aim location: the build may intersect the
			// player's capsule.
			const bool bIgnoredForPlayerIndependence = (DisqualifierClass == UFGCDInvalidAimLocation::StaticClass())
				|| (DisqualifierClass == UFGCDEncroachingPlayer::StaticClass())
				|| (bUnlimitedResources && DisqualifierClass == UFGCDUnaffordable::StaticClass());
			const bool bIsSoft = UFGConstructDisqualifier::GetIsSoftDisqualifier(DisqualifierClass);
			if (!bIgnoredForPlayerIndependence && !bIsSoft)
			{
				bCanConstruct = false;
			}
			DisqualifierTexts.Add(FString::Printf(TEXT("%s (%s%s)"),
				*UFGConstructDisqualifier::GetDisqualifyingText(DisqualifierClass).ToString(),
				bIsSoft ? TEXT("soft") : TEXT("hard"), bIgnoredForPlayerIndependence ? TEXT(", ignored") : TEXT("")));
		}
		const FString DisqualifierSummary = DisqualifierTexts.IsEmpty() ? TEXT("<none>") : FString::Join(DisqualifierTexts, TEXT("; "));

		if (!bCanConstruct)
		{
			UE_LOG(LogAIModAI, Display, TEXT("ConstructExtractorOnNode (deferred, resolved after %d real tick(s)): CanConstruct()=false, NOT constructing - node=%s disqualifiers=[%s]"),
				PollState->AttemptsTaken, *PollState->NodeId, *DisqualifierSummary);
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("CANNOT_CONSTRUCT"), DisqualifierSummary));
			return;
		}

		AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(PollWorld);
		const FNetConstructionID ConstructionID = BuildableSubsystem ? BuildableSubsystem->GetNewNetConstructionID() : FNetConstructionID();

		AFGBuildGun* PollBuildGun = IsValid(PollCharacter) ? PollCharacter->GetBuildGun() : nullptr;
		UFGBuildGunStateBuild* PollBuildState = PollBuildGun ? Cast<UFGBuildGunStateBuild>(PollBuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD)) : nullptr;
		if (!PollBuildState)
		{
			UE_LOG(LogAIModAI, Error, TEXT("ConstructExtractorOnNode (deferred): lost the build state before constructing - aborting, nothing built"));
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("Lost the build state before constructing")));
			return;
		}

		const FVector ConstructLocation = PollHologram->GetActorLocation();

		// GUARD: AFGResourceExtractorHologram::ConfigureActor() has
		// a hard check(mSnappedExtractableResource) that CRASHES the game (assert
		// at FGResourceExtractorHologram.cpp:235) if the extractor hologram never
		// snapped to an extractable resource - e.g. a Mk1 miner on an
		// iron resource node whose resource component didn't snap. The disqualifier
		// pass above does NOT catch this. Read that protected-but-UPROPERTY
		// TScriptInterface via reflection and abort with a structured error rather
		// than let the engine assert - per CLAUDE.md an expected failure (a node
		// the extractor can't actually attach to) must never crash the game.
		if (const FInterfaceProperty* SnapProp = CastField<FInterfaceProperty>(
				PollHologram->GetClass()->FindPropertyByName(TEXT("mSnappedExtractableResource"))))
		{
			const FScriptInterface* Snap = SnapProp->ContainerPtrToValuePtr<FScriptInterface>(PollHologram);
			if (!Snap || Snap->GetObject() == nullptr)
			{
				UE_LOG(LogAIModAI, Warning, TEXT("ConstructExtractorOnNode: extractor hologram did NOT snap an extractable resource for node=%s - aborting to avoid the ConfigureActor assert crash"), *PollState->NodeId);
				if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
				PollState->OnComplete(FAIModOperationResult::Failure(TEXT("NO_EXTRACTABLE_RESOURCE"), TEXT("The extractor hologram did not snap to an extractable resource on this node; miner not constructed (would otherwise assert/crash).")));
				return;
			}
		}

		PollBuildState->InternalConstructHologram(ConstructionID);

		// Confirmation, not just trust: if construction genuinely
		// succeeded, the node should now report occupied. Identify the
		// buildable by proximity, same pattern as ConstructBuildingAtPosition.
		const bool bNowOccupied = IsValid(PollTargetNode) && PollTargetNode->IsOccupied();

		FString ConstructedBuildableId;
		if (BuildableSubsystem)
		{
			float BestDistSq = TNumericLimits<float>::Max();
			AFGBuildable* BestMatch = nullptr;
			for (AFGBuildable* Candidate : BuildableSubsystem->GetAllBuildablesRef())
			{
				if (!IsValid(Candidate)) { continue; }
				// Class filter - same wrong-nearest-id guard as
				// ConstructBuildingAtPosition: only an
				// extractor can be what this function just constructed, so
				// never report a nearby belt/pole/attachment id instead.
				if (!Candidate->IsA(AFGBuildableResourceExtractorBase::StaticClass())) { continue; }
				const float DistSq = FVector::DistSquared(Candidate->GetActorLocation(), ConstructLocation);
				if (DistSq < BestDistSq)
				{
					BestDistSq = DistSq;
					BestMatch = Candidate;
				}
			}
			if (BestMatch && BestDistSq < FMath::Square(200.0f))
			{
				ConstructedBuildableId = BestMatch->GetPathName();
			}
		}

		UE_LOG(LogAIModAI, Display, TEXT("ConstructExtractorOnNode (deferred, resolved after %d real tick(s)): construction attempted via InternalConstructHologram - node=%s nodeNowOccupied=%s id=%s"),
			PollState->AttemptsTaken, *PollState->NodeId, bNowOccupied ? TEXT("true") : TEXT("false"), *ConstructedBuildableId);

		if (IsValid(PollCharacter))
		{
			PollCharacter->UnequipBuildGun();
		}

		if (!bNowOccupied)
		{
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("CONSTRUCTION_UNCONFIRMED"), TEXT("InternalConstructHologram was called but the node does not report occupied afterward")));
			return;
		}

		PollState->OnComplete(ConstructedBuildableId.IsEmpty()
			? FAIModOperationResult::Success()
			: FAIModOperationResult::SuccessWithBuildableId(ConstructedBuildableId));
	};

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
}


// See ConstructWaterPumpNearReference's doc comment in the header for
// the full "water pump was never reachable via ConstructExtractorOnNode"
// finding. Deliberately mirrors ConstructExtractorOnNode's own synthetic-
// hit/Distance-fix/TrySnapToActor/poll pattern almost exactly (same real
// AFGResourceExtractorHologram lineage, same IFGExtractableResourceInterface
// contract - AFGWaterVolume implements GetPlacementLocation/
// GetPlacementRotation the SAME way AFGResourceNodeBase does) rather than
// delegating to ConstructBuildingAtPosition's generic ground-trace flow -
// this uses the real, purpose-built placement-snapping mechanism the
// hologram itself relies on, not a guessed literal position.

// See ConstructWaterPumpNearReference's doc comment in the header for
// the full "water pump was never reachable via ConstructExtractorOnNode"
// finding - both this and ConstructWaterPumpAtPosition below just
// resolve their own candidate position, then delegate to the shared
// ConstructWaterPumpAtCandidatePosition helper above.
void UAIModFunctionLibrary::ConstructWaterPumpNearReference(UObject* WorldContextObject, const FString& ReferenceBuildableId, float OffsetX, float OffsetY, float OffsetZ, const FString& RecipeClassPath, TFunction<void(const FAIModOperationResult&)> OnComplete)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context")));
		return;
	}

	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Character)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_PLAYER"), TEXT("No local AFGCharacterPlayer (player index 0)")));
		return;
	}

	AFGBuildable* ReferenceBuildable = FindBuildableById(World, ReferenceBuildableId);
	AFGBuildableWaterPump* ReferencePump = Cast<AFGBuildableWaterPump>(ReferenceBuildable);
	if (!ReferencePump)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("WRONG_TYPE"),
			FString::Printf(TEXT("'%s' did not resolve to an AFGBuildableWaterPump (%s)"), *ReferenceBuildableId,
				ReferenceBuildable ? *ReferenceBuildable->GetClass()->GetName() : TEXT("not found"))));
		return;
	}

	const FVector CandidatePosition = ReferencePump->GetActorLocation() + FVector(OffsetX, OffsetY, OffsetZ);
	ConstructWaterPumpAtCandidatePosition(World, Character, CandidatePosition, RecipeClassPath, ReferenceBuildableId, MoveTemp(OnComplete));
}


// See ConstructWaterPumpAtPosition's doc comment in the header - the
// from-scratch counterpart to ConstructWaterPumpNearReference, for
// seeding the FIRST pump in a field with no existing reference.
void UAIModFunctionLibrary::ConstructWaterPumpAtPosition(UObject* WorldContextObject, float X, float Y, float Z, const FString& RecipeClassPath, TFunction<void(const FAIModOperationResult&)> OnComplete)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context")));
		return;
	}

	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Character)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_PLAYER"), TEXT("No local AFGCharacterPlayer (player index 0)")));
		return;
	}

	const FVector CandidatePosition(X, Y, Z);
	ConstructWaterPumpAtCandidatePosition(World, Character, CandidatePosition, RecipeClassPath,
		FString::Printf(TEXT("(%.0f,%.0f,%.0f)"), X, Y, Z), MoveTemp(OnComplete));
}




void UAIModFunctionLibrary::ConstructPortableMinerOnNode(UObject* WorldContextObject, const FString& NodeId, const FString& ItemClassPath, TFunction<void(const FAIModOperationResult&)> OnComplete)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context")));
		return;
	}

	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Character)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_PLAYER"), TEXT("No local AFGCharacterPlayer (player index 0)")));
		return;
	}

	AFGResourceNodeBase* TargetNodeBase = nullptr;
	for (TActorIterator<AFGResourceNodeBase> It(World); It; ++It)
	{
		if (IsValid(*It) && It->GetPathName() == NodeId)
		{
			TargetNodeBase = *It;
			break;
		}
	}
	if (!TargetNodeBase)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NODE_NOT_FOUND"), FString::Printf(TEXT("No resource node found with id '%s'"), *NodeId)));
		return;
	}
	if (TargetNodeBase->IsOccupied())
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NODE_OCCUPIED"), TEXT("Targeted node is already occupied")));
		return;
	}

	// Server_SpawnPortableMiner takes a real AFGResourceNode*, not the
	// wider AFGResourceNodeBase - Portable Miners only ever go on normal
	// solid ore nodes, never Fracking cores/satellites. Fail clearly here
	// rather than passing a null resourceNode into the reflection call
	// below.
	AFGResourceNode* TargetNode = Cast<AFGResourceNode>(TargetNodeBase);
	if (!TargetNode)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("INVALID_NODE_TYPE"),
			TEXT("Portable Miners only work on normal AFGResourceNode instances (not Fracking cores/satellites)")));
		return;
	}

	const FString EffectiveItemClassPath = ItemClassPath.IsEmpty()
		? TEXT("/Game/FactoryGame/Resource/Equipment/PortableMiner/BP_ItemDescriptorPortableMiner.BP_ItemDescriptorPortableMiner_C")
		: ItemClassPath;
	UClass* ResolvedItemClass = LoadObject<UClass>(nullptr, *EffectiveItemClassPath);
	const TSubclassOf<UFGItemDescriptor> ItemClass = (ResolvedItemClass && ResolvedItemClass->IsChildOf(UFGItemDescriptor::StaticClass())) ? ResolvedItemClass : nullptr;
	if (!ItemClass)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("INVALID_RECIPE"),
			FString::Printf(TEXT("'%s' did not resolve to a UFGItemDescriptor subclass"), *EffectiveItemClassPath)));
		return;
	}

	UFGInventoryComponent* PlayerInventory = Character->GetInventory();
	UFGInventoryComponentEquipment* ArmsSlot = Character->GetEquipmentSlot(EEquipmentSlot::ES_ARMS);
	if (!PlayerInventory || !ArmsSlot)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No player/ARMS equipment slot inventory found")));
		return;
	}

	// The ARMS equipment slot is a genuinely SEPARATE small inventory
	// component (UFGInventoryComponentEquipment), not a view/filter over
	// the player's general backpack inventory, shown two ways: (1) an item
	// sitting only in the general inventory never shows up scanning this
	// component's own stacks, and (2) once an item is moved into this slot
	// via the in-game UI, it stops showing up in the general inventory's
	// HasItems() check - the two are mutually exclusive locations, not a
	// view/mirror. So: check the ARMS slot FIRST (covers
	// "already equipped/slotted" including a manual move),
	// and only fall back to moving it from the general inventory if it's
	// not already there.
	int32 FoundIndex = INDEX_NONE;
	for (int32 i = 0; i < ArmsSlot->GetSizeLinear(); ++i)
	{
		FInventoryStack Stack;
		if (ArmsSlot->GetStackFromIndex(i, Stack) && Stack.HasItems() && Stack.Item.GetItemClass() == ItemClass)
		{
			FoundIndex = i;
			break;
		}
	}
	if (FoundIndex == INDEX_NONE)
	{
		if (!PlayerInventory->HasItems(ItemClass, 1))
		{
			OnComplete(FAIModOperationResult::Failure(TEXT("PORTABLE_MINER_NOT_IN_INVENTORY"),
				FString::Printf(TEXT("Player has no '%s' in the general inventory or the ARMS equipment slot - craft one first"), *EffectiveItemClassPath)));
			return;
		}

		PlayerInventory->Remove(ItemClass, 1);
		const int32 NumAdded = ArmsSlot->AddStack(FInventoryStack(1, ItemClass), /*allowPartialAdd=*/false);
		if (NumAdded <= 0)
		{
			// Put it back - never leave the player's inventory short an
			// item because of a failed internal move.
			PlayerInventory->AddStack(FInventoryStack(1, ItemClass), /*allowPartialAdd=*/true);
			OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"),
				FString::Printf(TEXT("'%s' could not be moved into the ARMS equipment slot inventory"), *EffectiveItemClassPath)));
			return;
		}

		for (int32 i = 0; i < ArmsSlot->GetSizeLinear(); ++i)
		{
			FInventoryStack Stack;
			if (ArmsSlot->GetStackFromIndex(i, Stack) && Stack.HasItems() && Stack.Item.GetItemClass() == ItemClass)
			{
				FoundIndex = i;
				break;
			}
		}
	}
	if (FoundIndex == INDEX_NONE)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"),
			FString::Printf(TEXT("'%s' was moved into the ARMS equipment slot but its resulting index could not be found"), *EffectiveItemClassPath)));
		return;
	}

	// Real, sanctioned equip path - the same one a player's own hotbar
	// key-press uses. See this function's header doc comment for why
	// AFGCharacterPlayer::SpawnEquipment/EquipEquipment aren't called
	// directly (SpawnEquipment is private, non-reflected C++).
	ArmsSlot->SetActiveEquipmentIndex(FoundIndex);

	struct FPollState
	{
		TWeakObjectPtr<AFGCharacterPlayer> Character;
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<AFGResourceNode> TargetNode;
		TFunction<void(const FAIModOperationResult&)> OnComplete;
		int32 AttemptsRemaining = 120; // real ticks, not a fixed duration
		int32 AttemptsTaken = 0;
	};
	const TSharedRef<FPollState> PollState = MakeShared<FPollState>();
	PollState->Character = Character;
	PollState->World = World;
	PollState->TargetNode = TargetNode;
	PollState->OnComplete = MoveTemp(OnComplete);

	const TSharedRef<TFunction<void()>> PollFn = MakeShared<TFunction<void()>>();
	*PollFn = [PollState, PollFn]()
	{
		++PollState->AttemptsTaken;

		UWorld* PollWorld = PollState->World.Get();
		AFGCharacterPlayer* PollCharacter = PollState->Character.Get();
		AFGResourceNode* PollTargetNode = PollState->TargetNode.Get();
		if (!PollWorld || !IsValid(PollCharacter) || !IsValid(PollTargetNode))
		{
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("HOLOGRAM_INVALIDATED"), TEXT("World/character/node became invalid while polling")));
			return;
		}

		AFGPortableMinerDispenser* Dispenser = Cast<AFGPortableMinerDispenser>(PollCharacter->GetEquipmentInSlot(EEquipmentSlot::ES_ARMS));
		--PollState->AttemptsRemaining;
		if (!Dispenser && PollState->AttemptsRemaining > 0)
		{
			PollWorld->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
			return;
		}
		if (!Dispenser)
		{
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("EQUIP_FAILED"),
				FString::Printf(TEXT("Portable Miner Dispenser did not equip after %d tick(s)"), PollState->AttemptsTaken)));
			return;
		}

		// Bypasses TraceForPortableMinerPlacementLocation's camera-dependent
		// aim entirely - real node location, not a trace, matching this
		// project's player-independence pattern for every other
		// Construct* function. See FPortableMinerDispenserAccessor's doc
		// comment above for why this calls the real UHT-generated thunk
		// directly (via a protected-access-bypass accessor) instead of
		// through FindFunction+ProcessEvent reflection, which twice
		// executed with no error but never produced a real actor.
		UE_LOG(LogAIModAI, Display, TEXT("ConstructPortableMinerOnNode: Dispenser HasAuthority=%s LocalRole=%d, Character HasAuthority=%s"),
			Dispenser->HasAuthority() ? TEXT("true") : TEXT("false"), static_cast<int32>(Dispenser->GetLocalRole()),
			PollCharacter->HasAuthority() ? TEXT("true") : TEXT("false"));

		static_cast<FPortableMinerDispenserAccessor*>(Dispenser)->Server_SpawnPortableMiner(PollTargetNode->GetActorLocation(), PollTargetNode);

		UE_LOG(LogAIModAI, Display, TEXT("ConstructPortableMinerOnNode: invoked Server_SpawnPortableMiner (direct C++ call) at %s for node %s"),
			*PollTargetNode->GetActorLocation().ToString(), *PollTargetNode->GetPathName());

		// Poll again for the real actor to appear before reporting success -
		// never trust an RPC call alone (this project's established
		// discipline).
		struct FVerifyState
		{
			TWeakObjectPtr<UWorld> World;
			TWeakObjectPtr<AFGCharacterPlayer> Character;
			TWeakObjectPtr<AFGResourceNode> TargetNode;
			TWeakObjectPtr<AFGPortableMinerDispenser> Dispenser;
			TFunction<void(const FAIModOperationResult&)> OnComplete;
			int32 AttemptsRemaining = 60;
			int32 AttemptsTaken = 0;
		};
		const TSharedRef<FVerifyState> VerifyState = MakeShared<FVerifyState>();
		VerifyState->World = PollWorld;
		VerifyState->Character = PollCharacter;
		VerifyState->TargetNode = PollTargetNode;
		VerifyState->Dispenser = Dispenser;
		VerifyState->OnComplete = PollState->OnComplete;

		const TSharedRef<TFunction<void()>> VerifyFn = MakeShared<TFunction<void()>>();
		*VerifyFn = [VerifyState, VerifyFn]()
		{
			++VerifyState->AttemptsTaken;

			UWorld* VerifyWorld = VerifyState->World.Get();
			AFGResourceNode* VerifyTargetNode = VerifyState->TargetNode.Get();
			if (!VerifyWorld || !IsValid(VerifyTargetNode))
			{
				VerifyState->OnComplete(FAIModOperationResult::Failure(TEXT("HOLOGRAM_INVALIDATED"), TEXT("World/node became invalid while verifying")));
				return;
			}

			AFGPortableMiner* NewMiner = nullptr;
			for (TActorIterator<AFGPortableMiner> It(VerifyWorld); It; ++It)
			{
				if (IsValid(*It) && It->mExtractResourceNode == VerifyTargetNode)
				{
					NewMiner = *It;
					break;
				}
			}

			--VerifyState->AttemptsRemaining;
			if (!NewMiner && VerifyState->AttemptsRemaining > 0)
			{
				VerifyWorld->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([VerifyFn]() { (*VerifyFn)(); }));
				return;
			}

			AFGCharacterPlayer* VerifyCharacter = VerifyState->Character.Get();
			AFGPortableMinerDispenser* VerifyDispenser = VerifyState->Dispenser.Get();
			if (IsValid(VerifyCharacter) && IsValid(VerifyDispenser))
			{
				VerifyCharacter->UnequipEquipment(VerifyDispenser);
			}

			if (!NewMiner)
			{
				VerifyState->OnComplete(FAIModOperationResult::Failure(TEXT("CONSTRUCTION_UNCONFIRMED"),
					FString::Printf(TEXT("Server_SpawnPortableMiner was invoked but no AFGPortableMiner targeting the node appeared after %d tick(s)"), VerifyState->AttemptsTaken)));
				return;
			}

			VerifyState->OnComplete(FAIModOperationResult::SuccessWithBuildableId(NewMiner->GetPathName()));
		};

		PollWorld->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([VerifyFn]() { (*VerifyFn)(); }));
	};

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
}


FString UAIModFunctionLibrary::LogPortableMinersAsJson(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		UE_LOG(LogAIModAI, Warning, TEXT("LogPortableMinersAsJson: no valid world context"));
		return TEXT("{}");
	}

	TArray<TSharedPtr<FJsonValue>> MinersArray;
	for (TActorIterator<AFGPortableMiner> It(World); It; ++It)
	{
		AFGPortableMiner* Miner = *It;
		if (!IsValid(Miner))
		{
			continue;
		}

		const FVector Position = Miner->GetActorLocation();
		const TSharedRef<FJsonObject> PositionObject = MakeShared<FJsonObject>();
		PositionObject->SetNumberField(TEXT("x"), Position.X);
		PositionObject->SetNumberField(TEXT("y"), Position.Y);
		PositionObject->SetNumberField(TEXT("z"), Position.Z);

		TArray<TSharedPtr<FJsonValue>> OutputArray;
		if (UFGInventoryComponent* OutputInventory = Miner->GetOutputInventory())
		{
			TArray<FInventoryStack> Stacks;
			OutputInventory->GetInventoryStacks(Stacks, false);
			for (const FInventoryStack& Stack : Stacks)
			{
				if (!Stack.HasItems())
				{
					continue;
				}
				const TSharedRef<FJsonObject> StackObject = MakeShared<FJsonObject>();
				StackObject->SetStringField(TEXT("itemClass"), Stack.Item.GetItemClass() ? Stack.Item.GetItemClass()->GetPathName() : TEXT(""));
				StackObject->SetNumberField(TEXT("numItems"), Stack.NumItems);
				OutputArray.Add(MakeShared<FJsonValueObject>(StackObject));
			}
		}

		const TSharedRef<FJsonObject> MinerObject = MakeShared<FJsonObject>();
		MinerObject->SetStringField(TEXT("id"), Miner->GetPathName());
		MinerObject->SetObjectField(TEXT("position"), PositionObject);
		MinerObject->SetStringField(TEXT("nodeId"), Miner->mExtractResourceNode ? Miner->mExtractResourceNode->GetPathName() : TEXT(""));
		MinerObject->SetBoolField(TEXT("isProducing"), Miner->IsProducing());
		MinerObject->SetNumberField(TEXT("extractionProgress"), Miner->GetExtractionProgress());
		MinerObject->SetArrayField(TEXT("outputInventory"), OutputArray);
		MinersArray.Add(MakeShared<FJsonValueObject>(MinerObject));
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetArrayField(TEXT("portableMiners"), MinersArray);

	const FString JsonString = WriteCondensedJson(RootObject);

	UE_LOG(LogAIModAI, Display, TEXT("LogPortableMinersAsJson: %d portable miner(s)"), MinersArray.Num());

	return JsonString;
}


void UAIModFunctionLibrary::RetrievePortableMinerInventory(UObject* WorldContextObject, const FString& PortableMinerId, TFunction<void(const FAIModOperationResult&)> OnComplete)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context")));
		return;
	}

	AFGPortableMiner* Miner = FindPortableMinerById(World, PortableMinerId);
	if (!Miner)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No Portable Miner found with id '%s'"), *PortableMinerId)));
		return;
	}

	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Character)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_PLAYER"), TEXT("No local AFGCharacterPlayer (player index 0)")));
		return;
	}

	UFGInventoryComponent* OutputInventory = Miner->GetOutputInventory();
	UFGInventoryComponent* PlayerInventory = Character->GetInventory();
	if (!OutputInventory || !PlayerInventory)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("Missing output or player inventory component")));
		return;
	}

	TArray<FInventoryStack> Stacks;
	OutputInventory->GetInventoryStacks(Stacks, false);

	bool bHadAnyItems = false;
	int32 TotalMoved = 0;
	for (const FInventoryStack& Stack : Stacks)
	{
		if (!Stack.HasItems())
		{
			continue;
		}
		bHadAnyItems = true;

		const TSubclassOf<UFGItemDescriptor> ItemClass = Stack.Item.GetItemClass();
		const int32 NumAdded = PlayerInventory->AddStack(FInventoryStack(Stack.NumItems, ItemClass), /*allowPartialAdd=*/true);
		if (NumAdded > 0)
		{
			OutputInventory->Remove(ItemClass, NumAdded);
			TotalMoved += NumAdded;
		}
	}

	if (!bHadAnyItems)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NOTHING_TO_RETRIEVE"), TEXT("Output inventory is empty")));
		return;
	}
	if (TotalMoved == 0)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("INVENTORY_FULL"), TEXT("Output inventory has items, but none of them fit in the player's inventory")));
		return;
	}

	UE_LOG(LogAIModAI, Display, TEXT("RetrievePortableMinerInventory: moved %d item(s) from %s to player inventory"), TotalMoved, *PortableMinerId);

	OnComplete(FAIModOperationResult::Success());
}


FAIModOperationResult UAIModFunctionLibrary::MovePortableMinerToInventory(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}

	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Character)
	{
		return FAIModOperationResult::Failure(TEXT("NO_PLAYER"), TEXT("No local AFGCharacterPlayer (player index 0)"));
	}

	UFGInventoryComponent* PlayerInventory = Character->GetInventory();
	UFGInventoryComponentEquipment* ArmsSlot = Character->GetEquipmentSlot(EEquipmentSlot::ES_ARMS);
	if (!PlayerInventory || !ArmsSlot)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No player/ARMS equipment slot inventory found"));
	}

	const TSubclassOf<UFGItemDescriptor> PortableMinerItemClass = LoadObject<UClass>(nullptr,
		TEXT("/Game/FactoryGame/Resource/Equipment/PortableMiner/BP_ItemDescriptorPortableMiner.BP_ItemDescriptorPortableMiner_C"));

	int32 FoundIndex = INDEX_NONE;
	for (int32 i = 0; i < ArmsSlot->GetSizeLinear(); ++i)
	{
		FInventoryStack Stack;
		if (ArmsSlot->GetStackFromIndex(i, Stack) && Stack.HasItems() && Stack.Item.GetItemClass() == PortableMinerItemClass)
		{
			FoundIndex = i;
			break;
		}
	}

	if (FoundIndex == INDEX_NONE)
	{
		// Not an error - the general inventory may already have one, or
		// the player may genuinely have none at all (a different failure
		// the caller will see when it actually tries to build).
		return FAIModOperationResult::Success();
	}

	ArmsSlot->RemoveFromIndex(FoundIndex, 1, PlayerInventory);

	UE_LOG(LogAIModAI, Display, TEXT("MovePortableMinerToInventory: moved 1 Portable Miner from ARMS slot to general inventory"));
	return FAIModOperationResult::Success();
}
