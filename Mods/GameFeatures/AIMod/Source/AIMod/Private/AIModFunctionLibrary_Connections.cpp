// Copyright Epic Games, Inc. All Rights Reserved.

#include "AIModFunctionLibraryInternal.h"
using namespace AIModInternal;


FString UAIModFunctionLibrary::LogConnectorLayoutAsJson(UObject* WorldContextObject, const FString& BuildableClassPath)
{
	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetStringField(TEXT("buildableClass"), BuildableClassPath);

	UClass* BuildableClass = LoadClass<AFGBuildable>(nullptr, *BuildableClassPath);
	if (!BuildableClass)
	{
		RootObject->SetStringField(TEXT("error"), TEXT("CLASS_NOT_FOUND"));
		return WriteCondensedJson(RootObject);
	}

	TArray<TSharedPtr<FJsonValue>> Rows;

	// Native components: created in C++ constructors, present on the CDO
	// with their default relative transforms. These attach to the actor
	// root in every FactoryGame case seen so far, so the relative
	// transform IS the actor-local transform - if a nested native
	// attachment ever appears, its rows would need parent composition
	// like the SCS walk below (the source tag lets a consumer notice).
	if (const AFGBuildable* CDO = BuildableClass->GetDefaultObject<AFGBuildable>())
	{
		TArray<UFGFactoryConnectionComponent*> NativeConnections;
		CDO->GetComponents<UFGFactoryConnectionComponent>(NativeConnections);
		for (const UFGFactoryConnectionComponent* Connection : NativeConnections)
		{
			if (IsValid(Connection))
			{
				AddConnectorLayoutRow(Rows, Connection, Connection->GetRelativeTransform(), TEXT("native"));
			}
		}
	}

	// Blueprint-added components: walk every SCS in the class hierarchy
	// (parent BP classes contribute their own nodes).
	for (UClass* CurrentClass = BuildableClass; CurrentClass; CurrentClass = CurrentClass->GetSuperClass())
	{
		const UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(CurrentClass);
		if (!BPGC || !BPGC->SimpleConstructionScript)
		{
			continue;
		}
		for (const USCS_Node* RootNode : BPGC->SimpleConstructionScript->GetRootNodes())
		{
			WalkScsNode(Rows, RootNode, FTransform::Identity);
		}
	}

	RootObject->SetArrayField(TEXT("connectors"), Rows);

	// Walkway/catwalk orientation data: catwalks (and walkways)
	// are AFGBuildableWalkway, which stores exactly the orientation facts a
	// caller needs - mSize (footprint side), mElevation
	// (ramp rise; per FGBuildableWalkway.h the ramp goes UP toward LOCAL +X),
	// and mDisableSnapOn (the sides with snapping disabled == the RAILED/closed
	// sides). Emitting the railed sides as LOCAL-frame normals lets a caller
	// rotate them by a placed piece's yaw to get the world rail direction with
	// zero eyeballing (a Catwalk_T reports one rail side, a Straight/Turn two,
	// a Cross none). Local side->normal: Front=+X, Back=-X, Right=+Y, Left=-Y.
	// Use the untyped CDO + Cast<> (returns null on mismatch). The templated
	// GetDefaultObject<AFGBuildableWalkway>() does a CastChecked internally and
	// hard-asserts (crash) for any non-walkway class - it takes down the game
	// when connectorLayout is queried for a non-walkway (e.g. a drone station).
	if (const AFGBuildableWalkway* WalkwayCDO = Cast<AFGBuildableWalkway>(BuildableClass->GetDefaultObject()))
	{
		const TSharedRef<FJsonObject> WalkwayObject = MakeShared<FJsonObject>();
		WalkwayObject->SetNumberField(TEXT("size"), WalkwayCDO->mSize);
		WalkwayObject->SetNumberField(TEXT("elevation"), WalkwayCDO->mElevation);
		// mElevation rises toward local +X (documented on FGBuildableWalkway).
		const TSharedRef<FJsonObject> RiseDir = MakeShared<FJsonObject>();
		RiseDir->SetNumberField(TEXT("x"), 1.0);
		RiseDir->SetNumberField(TEXT("y"), 0.0);
		RiseDir->SetNumberField(TEXT("z"), 0.0);
		WalkwayObject->SetObjectField(TEXT("rampHighLocalDir"), RiseDir);

		const FFoundationSideSelectionFlags& Snap = WalkwayCDO->mDisableSnapOn;
		TArray<TSharedPtr<FJsonValue>> RailNormals;
		auto AddRail = [&RailNormals](double NX, double NY, double NZ)
		{
			const TSharedRef<FJsonObject> N = MakeShared<FJsonObject>();
			N->SetNumberField(TEXT("x"), NX);
			N->SetNumberField(TEXT("y"), NY);
			N->SetNumberField(TEXT("z"), NZ);
			RailNormals.Add(MakeShared<FJsonValueObject>(N));
		};
		if (Snap.Front) { AddRail(1.0, 0.0, 0.0); }
		if (Snap.Back)  { AddRail(-1.0, 0.0, 0.0); }
		if (Snap.Right) { AddRail(0.0, 1.0, 0.0); }
		if (Snap.Left)  { AddRail(0.0, -1.0, 0.0); }
		WalkwayObject->SetArrayField(TEXT("railLocalNormals"), RailNormals);

		RootObject->SetObjectField(TEXT("walkway"), WalkwayObject);
	}

	UE_LOG(LogAIModAI, Display, TEXT("LogConnectorLayoutAsJson: %s -> %d connector(s)"), *BuildableClassPath, Rows.Num());
	return WriteCondensedJson(RootObject);
}


TArray<FAIModPipeConnectionTelemetry> UAIModFunctionLibrary::GetPipeConnectionTelemetry(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		UE_LOG(LogAIModAI, Warning, TEXT("GetPipeConnectionTelemetry: no valid world context"));
		return {};
	}

	return CollectPipeConnectionTelemetry(World);
}


void UAIModFunctionLibrary::LogPipeConnections(UObject* WorldContextObject)
{
	const TArray<FAIModPipeConnectionTelemetry> Connections = GetPipeConnectionTelemetry(WorldContextObject);

	for (const FAIModPipeConnectionTelemetry& Connection : Connections)
	{
		UE_LOG(LogAIModAI, Display, TEXT("PipeConnection: owner=%s type=%s isHypertube=%s connected=%s connectedTo=%s position=%s normal=%s"),
			*Connection.OwnerBuildableId, *Connection.ConnectionType, Connection.bIsHypertube ? TEXT("true") : TEXT("false"),
			Connection.bConnected ? TEXT("true") : TEXT("false"), *Connection.ConnectedBuildableId, *Connection.Position.ToString(), *Connection.Normal.ToString());
	}

	UE_LOG(LogAIModAI, Display, TEXT("LogPipeConnections: enumerated %d connection point(s)"), Connections.Num());
}


FString UAIModFunctionLibrary::LogPipeConnectionsAsJson(UObject* WorldContextObject)
{
	const TArray<FAIModPipeConnectionTelemetry> Connections = GetPipeConnectionTelemetry(WorldContextObject);

	TArray<TSharedPtr<FJsonValue>> ConnectionJsonArray;
	ConnectionJsonArray.Reserve(Connections.Num());

	for (const FAIModPipeConnectionTelemetry& Connection : Connections)
	{
		const TSharedRef<FJsonObject> ConnectionObject = MakeShared<FJsonObject>();
		ConnectionObject->SetStringField(TEXT("ownerBuildableId"), Connection.OwnerBuildableId);
		ConnectionObject->SetStringField(TEXT("connectionType"), Connection.ConnectionType);
		ConnectionObject->SetBoolField(TEXT("isHypertube"), Connection.bIsHypertube);
		ConnectionObject->SetBoolField(TEXT("connected"), Connection.bConnected);
		ConnectionObject->SetStringField(TEXT("connectedBuildableId"), Connection.ConnectedBuildableId);

		const TSharedRef<FJsonObject> PositionObject = MakeShared<FJsonObject>();
		PositionObject->SetNumberField(TEXT("x"), Connection.Position.X);
		PositionObject->SetNumberField(TEXT("y"), Connection.Position.Y);
		PositionObject->SetNumberField(TEXT("z"), Connection.Position.Z);
		ConnectionObject->SetObjectField(TEXT("position"), PositionObject);

		const TSharedRef<FJsonObject> NormalObject = MakeShared<FJsonObject>();
		NormalObject->SetNumberField(TEXT("x"), Connection.Normal.X);
		NormalObject->SetNumberField(TEXT("y"), Connection.Normal.Y);
		NormalObject->SetNumberField(TEXT("z"), Connection.Normal.Z);
		ConnectionObject->SetObjectField(TEXT("normal"), NormalObject);

		ConnectionJsonArray.Add(MakeShared<FJsonValueObject>(ConnectionObject));
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetArrayField(TEXT("connections"), ConnectionJsonArray);

	const FString JsonString = WriteCondensedJson(RootObject);

	UE_LOG(LogAIModAI, Display, TEXT("LogPipeConnectionsAsJson: %s"), *JsonString);

	return JsonString;
}


FAIModOperationResult UAIModFunctionLibrary::DebugCheckPowerConnection(UObject* WorldContextObject, const FString& BuildableIdA, const FString& BuildableIdB)
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

	AFGBuildable* BuildableA = FindBuildableById(World, BuildableIdA);
	if (!BuildableA)
	{
		return FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No buildable found with id '%s'"), *BuildableIdA));
	}
	AFGBuildable* BuildableB = FindBuildableById(World, BuildableIdB);
	if (!BuildableB)
	{
		return FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No buildable found with id '%s'"), *BuildableIdB));
	}

	// FindPowerConnectionPair - a joint, type-aware
	// selection over both buildables at once, see its own comment for
	// why a Power Tower's dual PCT_PowerTower/PCT_Default connectors
	// need this instead of two independent single-buildable lookups.
	UFGPowerConnectionComponent* ConnectionA = nullptr;
	UFGPowerConnectionComponent* ConnectionB = nullptr;
	if (!FindPowerConnectionPair(BuildableA, BuildableB, ConnectionA, ConnectionB))
	{
		return FAIModOperationResult::Failure(TEXT("NO_POWER_CONNECTION"),
			FString::Printf(TEXT("No compatible free power connection pair between '%s' and '%s' - connection types (Default/PowerTower/Any) must match, or one side must be Any"), *BuildableIdA, *BuildableIdB));
	}

	// A real asset in Content/FactoryGame/Recipes/Buildings/
	// (see docs/conveyor-power-connection-research.md) - the build-cost
	// recipe for a plain power line/wire.
	UClass* PowerLineRecipeClass = LoadObject<UClass>(nullptr, TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PowerLine.Recipe_PowerLine_C"));
	if (!PowerLineRecipeClass || !PowerLineRecipeClass->IsChildOf(UFGRecipe::StaticClass()))
	{
		return FAIModOperationResult::Failure(TEXT("RECIPE_LOAD_FAILED"), TEXT("Failed to load Recipe_PowerLine as a UFGRecipe"));
	}
	const TSubclassOf<UFGRecipe> RecipeClass = PowerLineRecipeClass;

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
	AFGWireHologram* WireHologram = Cast<AFGWireHologram>(Hologram);
	if (!WireHologram)
	{
		Character->UnequipBuildGun();
		return FAIModOperationResult::Failure(TEXT("HOLOGRAM_SPAWN_FAILED"),
			FString::Printf(TEXT("HotKeyRecipe(Recipe_PowerLine) did not result in an AFGWireHologram (got %s)"),
				Hologram ? *Hologram->GetClass()->GetName() : TEXT("null")));
	}

	// Untested assumption being probed here (docs/conveyor-power-connection-research.md):
	// whether SetConnection() alone is enough, or whether the real
	// multi-step TrySnapToActor/DoMultiStepPlacement flow needs to run
	// first. Feed a synthetic hit result at ConnectionA's location first,
	// same pattern as every other build-gun-driven function, in case
	// UpdateHologramPlacement is a precondition CanConstruct() depends on.
	FHitResult SyntheticHit;
	SyntheticHit.Location = ConnectionA->GetComponentLocation();
	SyntheticHit.ImpactPoint = SyntheticHit.Location;
	SyntheticHit.Normal = FVector::UpVector;
	SyntheticHit.ImpactNormal = FVector::UpVector;
	SyntheticHit.HitObjectHandle = FActorInstanceHandle(BuildableA);
	// UFGPowerConnectionComponent derives from USceneComponent (via
	// UFGCircuitConnectionComponent/UFGConnectionComponent), not
	// UPrimitiveComponent - FHitResult::Component can't reference it
	// directly, so it's left unset here.
	SyntheticHit.bBlockingHit = true;
	BuildGun->GetHitResult() = SyntheticHit;

	WireHologram->SetConnection(0, ConnectionA);
	WireHologram->SetConnection(1, ConnectionB);

	struct FPollState
	{
		TWeakObjectPtr<AFGWireHologram> Hologram;
		TWeakObjectPtr<AFGCharacterPlayer> Character;
		TWeakObjectPtr<UWorld> World;
		FHitResult SyntheticHit;
		FString BuildableIdA;
		FString BuildableIdB;
		int32 AttemptsRemaining = 120; // safety cap - real ticks, not a fixed duration
		int32 AttemptsTaken = 0;
	};
	const TSharedRef<FPollState> PollState = MakeShared<FPollState>();
	PollState->Hologram = WireHologram;
	PollState->Character = Character;
	PollState->World = World;
	PollState->SyntheticHit = SyntheticHit;
	PollState->BuildableIdA = BuildableIdA;
	PollState->BuildableIdB = BuildableIdB;

	const TSharedRef<TFunction<void()>> PollFn = MakeShared<TFunction<void()>>();
	*PollFn = [PollState, PollFn]()
	{
		++PollState->AttemptsTaken;

		AFGWireHologram* PollHologram = PollState->Hologram.Get();
		UWorld* PollWorld = PollState->World.Get();
		AFGCharacterPlayer* PollCharacter = PollState->Character.Get();
		if (!IsValid(PollHologram) || !PollWorld)
		{
			UE_LOG(LogAIModAI, Warning, TEXT("DebugCheckPowerConnection (deferred): hologram or world became invalid while polling (after %d tick(s))"), PollState->AttemptsTaken);
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

		UE_LOG(LogAIModAI, Display, TEXT("DebugCheckPowerConnection (deferred, resolved after %d real tick(s)): a=%s b=%s canConstruct=%s disqualifiers=[%s]"),
			PollState->AttemptsTaken, *PollState->BuildableIdA, *PollState->BuildableIdB, bCanConstruct ? TEXT("true") : TEXT("false"), *DisqualifierSummary);

		if (IsValid(PollCharacter))
		{
			PollCharacter->UnequipBuildGun();
		}
	};

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));

	return FAIModOperationResult::Failure(TEXT("PENDING"),
		TEXT("Scheduled via the real build gun - dry-run only, never constructs; see LogAIModAI for the real result"));
}


void UAIModFunctionLibrary::ConstructPowerConnection(UObject* WorldContextObject, const FString& BuildableIdA, const FString& BuildableIdB, bool bDryRun, bool bIgnoreAimLocation, bool bIgnoreWireSnap, bool bIgnoreWireLength, TFunction<void(const FAIModOperationResult&)> OnComplete, const TOptional<FVector>& ConnectorPositionA, const TOptional<FVector>& ConnectorPositionB)
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

	AFGBuildable* BuildableA = FindBuildableById(World, BuildableIdA);
	if (!BuildableA)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No buildable found with id '%s'"), *BuildableIdA)));
		return;
	}
	AFGBuildable* BuildableB = FindBuildableById(World, BuildableIdB);
	if (!BuildableB)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No buildable found with id '%s'"), *BuildableIdB)));
		return;
	}

	// See this function's header comment on the pole-vs-daisy-chain
	// gameplay constraint - a NO_POWER_CONNECTION result here may
	// correctly reflect that the save's progression hasn't unlocked
	// direct machine-to-machine wiring yet, not a bug.
	//
	// FindPowerConnectionPair - a joint, type-aware
	// selection over both buildables at once, see its own comment for
	// why a Power Tower's dual PCT_PowerTower/PCT_Default connectors
	// need this instead of two independent single-buildable lookups.
	UFGPowerConnectionComponent* ConnectionA = nullptr;
	UFGPowerConnectionComponent* ConnectionB = nullptr;
	if (!FindPowerConnectionPair(BuildableA, BuildableB, ConnectionA, ConnectionB, ConnectorPositionA, ConnectorPositionB))
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_POWER_CONNECTION"),
			FString::Printf(TEXT("No compatible free power connection pair between '%s' and '%s'%s - connection types (Default/PowerTower/Any) must match, or one side must be Any"),
				*BuildableIdA, *BuildableIdB,
				(ConnectorPositionA.IsSet() || ConnectorPositionB.IsSet()) ? TEXT(" (with the requested connector position pin(s))") : TEXT(""))));
		return;
	}

	UClass* PowerLineRecipeClass = LoadObject<UClass>(nullptr, TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PowerLine.Recipe_PowerLine_C"));
	if (!PowerLineRecipeClass || !PowerLineRecipeClass->IsChildOf(UFGRecipe::StaticClass()))
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("RECIPE_LOAD_FAILED"), TEXT("Failed to load Recipe_PowerLine as a UFGRecipe")));
		return;
	}
	const TSubclassOf<UFGRecipe> RecipeClass = PowerLineRecipeClass;

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
	AFGWireHologram* WireHologram = Cast<AFGWireHologram>(Hologram);
	if (!WireHologram)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("HOLOGRAM_SPAWN_FAILED"),
			FString::Printf(TEXT("HotKeyRecipe(Recipe_PowerLine) did not result in an AFGWireHologram (got %s)"),
				Hologram ? *Hologram->GetClass()->GetName() : TEXT("null"))));
		return;
	}

	// Stuck-state fix (docs/build-efficiency-plan.md 2d):
	// UnequipBuildGun() on a FAILED attempt does not
	// destroy the wire hologram - the next HotKeyRecipe hands back the
	// SAME hologram, still carrying the previous call's SetConnection()
	// targets, and from then on EVERY connectPower in the session fails
	// validation ("Must be hooked up to a connection!" - even between
	// two freshly placed empty poles; sometimes "Already connected with
	// another wire!") until something else swaps the hologram. A manual
	// workaround is placing and deleting a dummy building (which
	// equips a different recipe's hologram); this detects the stale
	// state directly - a FRESH wire hologram has both connection slots
	// null - and forces a clean respawn instead.
	if (WireHologram->GetConnection(0) != nullptr || WireHologram->GetConnection(1) != nullptr)
	{
		UE_LOG(LogAIModAI, Warning, TEXT("ConstructPowerConnection: stale wire hologram detected (connection slots still set from a previous failed call) - destroying it and re-equipping"));
		Character->UnequipBuildGun();
		if (IsValid(WireHologram))
		{
			WireHologram->Destroy();
		}
		Character->HotKeyRecipe(RecipeClass);
		BuildGun = Character->GetBuildGun();
		BuildState = BuildGun ? Cast<UFGBuildGunStateBuild>(BuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD)) : nullptr;
		WireHologram = BuildState ? Cast<AFGWireHologram>(BuildState->GetHologram()) : nullptr;
		if (!WireHologram || WireHologram->GetConnection(0) != nullptr || WireHologram->GetConnection(1) != nullptr)
		{
			if (IsValid(Character)) { Character->UnequipBuildGun(); }
			OnComplete(FAIModOperationResult::Failure(TEXT("HOLOGRAM_SPAWN_FAILED"),
				TEXT("Stale wire hologram persisted through a forced destroy-and-re-equip - a place+delete of any cheap building resets it (known workaround)")));
			return;
		}
	}

	// This exact SetConnection()-only mechanism (no click/snap step, a
	// single GetHitResult() assignment for ConnectionA only) is the only
	// mechanism that works at all for machine<->machine connections. Two
	// mechanism deviations both regress the machine<->machine case: (1) a
	// click-based UpdateHologramPlacement()+TrySnapToActor()+DoMultiStepPlacement()
	// rewrite - TrySnapToActor() never populates GetConnection(0)/(1)
	// for wires at all; (2) touching BuildGun->GetHitResult() for
	// ConnectionB too (not just A) before SetConnection(1,...) - breaks
	// the ConnectionA validation. Do not repeat either.
	//
	// Separately, repeated identical dry-run calls against the exact same
	// pair of buildables can return THREE DIFFERENT disqualifiers across
	// attempts - UFGCDWireSnap, UFGCDWireTooLong (despite real 3D
	// distance well under the real queried maxLength), and
	// UFGCDInvalidAimLocation - with success on other attempts, no code
	// change. This is the same class of live-camera-dependent flakiness
	// solved for building placement (ConstructBuildingAtPosition's
	// bIgnoreAimLocation etc.), not a genuine geometry problem with this
	// function's own logic. See the poll loop below for the
	// bIgnoreAimLocation/bIgnoreWireSnap disqualifier-bypass this
	// motivated.
	FHitResult SyntheticHit;
	SyntheticHit.Location = ConnectionA->GetComponentLocation();
	SyntheticHit.ImpactPoint = SyntheticHit.Location;
	SyntheticHit.Normal = FVector::UpVector;
	SyntheticHit.ImpactNormal = FVector::UpVector;
	SyntheticHit.HitObjectHandle = FActorInstanceHandle(BuildableA);
	// UFGPowerConnectionComponent derives from USceneComponent, not
	// UPrimitiveComponent - FHitResult::Component left unset, same as
	// the dry-run.
	SyntheticHit.bBlockingHit = true;
	BuildGun->GetHitResult() = SyntheticHit;

	WireHologram->SetConnection(0, ConnectionA);
	WireHologram->SetConnection(1, ConnectionB);

	struct FPollState
	{
		TWeakObjectPtr<AFGWireHologram> Hologram;
		TWeakObjectPtr<AFGCharacterPlayer> Character;
		TWeakObjectPtr<UWorld> World;
		FHitResult SyntheticHit;
		FString BuildableIdA;
		FString BuildableIdB;
		bool bDryRun = true;
		bool bIgnoreAimLocation = false;
		bool bIgnoreWireSnap = false;
		bool bIgnoreWireLength = false;
		int32 AttemptsRemaining = 120; // safety cap - real ticks, not a fixed duration
		int32 AttemptsTaken = 0;
		TFunction<void(const FAIModOperationResult&)> OnComplete;
	};
	const TSharedRef<FPollState> PollState = MakeShared<FPollState>();
	PollState->Hologram = WireHologram;
	PollState->Character = Character;
	PollState->World = World;
	PollState->SyntheticHit = SyntheticHit;
	PollState->BuildableIdA = BuildableIdA;
	PollState->BuildableIdB = BuildableIdB;
	PollState->bDryRun = bDryRun;
	PollState->bIgnoreAimLocation = bIgnoreAimLocation;
	PollState->bIgnoreWireSnap = bIgnoreWireSnap;
	PollState->bIgnoreWireLength = bIgnoreWireLength;
	PollState->OnComplete = MoveTemp(OnComplete);

	const TSharedRef<TFunction<void()>> PollFn = MakeShared<TFunction<void()>>();
	*PollFn = [PollState, PollFn]()
	{
		++PollState->AttemptsTaken;

		AFGWireHologram* PollHologram = PollState->Hologram.Get();
		UWorld* PollWorld = PollState->World.Get();
		AFGCharacterPlayer* PollCharacter = PollState->Character.Get();
		if (!IsValid(PollHologram) || !PollWorld)
		{
			UE_LOG(LogAIModAI, Warning, TEXT("ConstructPowerConnection (deferred): hologram or world became invalid while polling (after %d tick(s))"), PollState->AttemptsTaken);
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("HOLOGRAM_INVALIDATED"), TEXT("Hologram or world became invalid while polling")));
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

		// Repeated identical dry-run calls
		// against the exact same pair of buildables (no code or geometry
		// change between calls) can return THREE DIFFERENT disqualifiers
		// across attempts - UFGCDWireSnap ("Must be hooked up to a
		// connection!"), UFGCDWireTooLong ("Wire is too long!" - despite
		// the real 3D distance being well under the real queried
		// maxLength), and UFGCDInvalidAimLocation ("Invalid aim
		// location!") - with success on other attempts with STILL no
		// change. This matches the exact same class of live-camera-
		// dependent flakiness already solved for building placement
		// (ConstructBuildingAtPosition's bIgnoreAimLocation etc.) - the
		// wire hologram's validation appears to depend on real,
		// currently-changing player camera/aim state our synthetic hit
		// doesn't fully override, not on anything this function's own
		// geometry gets wrong. Bypassing PollHologram->CanConstruct()'s
		// opaque bool (stub source, unreadable) in favor of manually
		// walking GetConstructDisqualifiers() ourselves - same pattern
		// as ConstructBuildingAtPosition - so bIgnoreAimLocation/
		// bIgnoreWireSnap can skip specific disqualifier classes the
		// caller explicitly opts into ignoring, without bypassing
		// FactoryGame's own real validation inside
		// InternalConstructHologram() itself.
		//
		// UFGCDWireTooLong (bIgnoreWireLength): the wire's
		// mMaxLength cap (10000cm pole / 30000cm tower - see
		// world.powerLineLimits). Unlike aim/snap flakiness this IS a real,
		// deterministic geometry check, so it is non-ignorable by
		// default. But it is a pure BUILD-TIME gate: FGPowerConnectionComponent
		// merges the two power circuits logically, with no runtime dependency
		// on wire length, so a wire built past the cap still delivers power.
		// The caller opts in explicitly to
		// run a single span across any distance - e.g. powering a remote
		// mining/smelting outpost kilometres from the main grid without
		// hand-placing a chain of dozens of poles. The visual spline simply
		// stretches. Off by default; on only when the caller passes the flag.
		// UnlimitedResources - see ConstructBuildingAtPosition's
		// identical comment on this being a player-controlled mod setting.
		const bool bUnlimitedResources = UAIModFunctionLibrary::GetAIModConfigBool(PollWorld, TEXT("UnlimitedResources"), false);

		bool bCanConstruct = true;
		TArray<FString> DisqualifierTexts;
		for (const TSubclassOf<UFGConstructDisqualifier>& DisqualifierClass : Disqualifiers)
		{
			const bool bIgnoredByFlag =
				(PollState->bIgnoreAimLocation && DisqualifierClass == UFGCDInvalidAimLocation::StaticClass()) ||
				(PollState->bIgnoreWireSnap && DisqualifierClass == UFGCDWireSnap::StaticClass()) ||
				(PollState->bIgnoreWireLength && DisqualifierClass == UFGCDWireTooLong::StaticClass()) ||
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

		UE_LOG(LogAIModAI, Display, TEXT("ConstructPowerConnection (deferred, resolved after %d real tick(s)): a=%s b=%s dryRun=%s canConstruct=%s disqualifiers=[%s]"),
			PollState->AttemptsTaken, *PollState->BuildableIdA, *PollState->BuildableIdB, PollState->bDryRun ? TEXT("true") : TEXT("false"),
			bCanConstruct ? TEXT("true") : TEXT("false"), *DisqualifierSummary);

		if (!bCanConstruct)
		{
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("CANNOT_CONSTRUCT"), DisqualifierSummary));
			return;
		}

		if (PollState->bDryRun)
		{
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Success());
			return;
		}

		AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(PollWorld);
		const FNetConstructionID ConstructionID = BuildableSubsystem ? BuildableSubsystem->GetNewNetConstructionID() : FNetConstructionID();

		AFGBuildGun* PollBuildGun = IsValid(PollCharacter) ? PollCharacter->GetBuildGun() : nullptr;
		UFGBuildGunStateBuild* PollBuildState = PollBuildGun ? Cast<UFGBuildGunStateBuild>(PollBuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD)) : nullptr;
		if (!PollBuildState)
		{
			UE_LOG(LogAIModAI, Error, TEXT("ConstructPowerConnection (deferred): lost the build state before constructing - aborting, nothing built"));
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("Lost the build state before constructing")));
			return;
		}

		PollBuildState->InternalConstructHologram(ConstructionID);

		UE_LOG(LogAIModAI, Display, TEXT("ConstructPowerConnection (deferred, resolved after %d real tick(s)): construction attempted via InternalConstructHologram - a=%s b=%s"),
			PollState->AttemptsTaken, *PollState->BuildableIdA, *PollState->BuildableIdB);

		if (IsValid(PollCharacter))
		{
			PollCharacter->UnequipBuildGun();
		}

		PollState->OnComplete(FAIModOperationResult::Success());
	};

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
}


FAIModOperationResult UAIModFunctionLibrary::DebugCheckConveyorSnap(UObject* WorldContextObject, const FString& SourceBuildableId)
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

	AFGBuildable* SourceBuildable = FindBuildableById(World, SourceBuildableId);
	if (!SourceBuildable)
	{
		return FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No buildable found with id '%s'"), *SourceBuildableId));
	}

	UFGFactoryConnectionComponent* SourceConnection = FindFreeFactoryConnection(SourceBuildable, EFactoryConnectionDirection::FCD_OUTPUT);
	if (!SourceConnection)
	{
		return FAIModOperationResult::Failure(TEXT("NO_FACTORY_CONNECTION"), FString::Printf(TEXT("'%s' has no free Output factory connection component"), *SourceBuildableId));
	}

	UClass* BeltRecipeClass = LoadObject<UClass>(nullptr, TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorBeltMk1.Recipe_ConveyorBeltMk1_C"));
	if (!BeltRecipeClass || !BeltRecipeClass->IsChildOf(UFGRecipe::StaticClass()))
	{
		return FAIModOperationResult::Failure(TEXT("RECIPE_LOAD_FAILED"), TEXT("Failed to load Recipe_ConveyorBeltMk1 as a UFGRecipe"));
	}
	const TSubclassOf<UFGRecipe> RecipeClass = BeltRecipeClass;

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
	AFGConveyorBeltHologram* BeltHologram = Cast<AFGConveyorBeltHologram>(Hologram);
	if (!BeltHologram)
	{
		Character->UnequipBuildGun();
		return FAIModOperationResult::Failure(TEXT("HOLOGRAM_SPAWN_FAILED"),
			FString::Printf(TEXT("HotKeyRecipe(Recipe_ConveyorBeltMk1) did not result in an AFGConveyorBeltHologram (got %s)"),
				Hologram ? *Hologram->GetClass()->GetName() : TEXT("null")));
	}

	const ESplineHologramBuildStep StepInitial = BeltHologram->GetCurrentBuildStep();

	FHitResult SyntheticHit;
	SyntheticHit.Location = SourceConnection->GetConnectorLocation();
	SyntheticHit.ImpactPoint = SyntheticHit.Location;
	SyntheticHit.Normal = FVector::UpVector;
	SyntheticHit.ImpactNormal = FVector::UpVector;
	SyntheticHit.HitObjectHandle = FActorInstanceHandle(SourceBuildable);
	SyntheticHit.bBlockingHit = true;

	// Calling TrySnapToActor() directly can return true but leave every
	// state indicator unchanged (step/IsConnectionSnapped/connected count
	// all show no real snap) - contradictory evidence. Every other hologram
	// in this
	// project (buildings, wires) is driven through
	// UpdateHologramPlacement(), not by calling the override method
	// directly - try that first, matching the proven pattern, before
	// (and after) TrySnapToActor() and a single DoMultiStepPlacement()
	// "click" to gather maximum evidence in one pass.
	BeltHologram->UpdateHologramPlacement(SyntheticHit);
	const ESplineHologramBuildStep StepAfterUpdate = BeltHologram->GetCurrentBuildStep();
	const bool bConnectionSnappedAfterUpdate = BeltHologram->IsConnectionSnapped(false);

	const bool bSnapped = BeltHologram->TrySnapToActor(SyntheticHit);
	const ESplineHologramBuildStep StepAfterSnap = BeltHologram->GetCurrentBuildStep();
	const bool bConnectionSnappedAfterSnap = BeltHologram->IsConnectionSnapped(false);
	const TArray<AFGBuildable*> ConnectedBuildablesAfterSnap = BeltHologram->GetAnyConnectedBuildables();

	TArray<TSubclassOf<UFGConstructDisqualifier>> Disqualifiers;
	BeltHologram->GetConstructDisqualifiers(Disqualifiers);
	TArray<FString> DisqualifierTexts;
	for (const TSubclassOf<UFGConstructDisqualifier>& DisqualifierClass : Disqualifiers)
	{
		DisqualifierTexts.Add(UFGConstructDisqualifier::GetDisqualifyingText(DisqualifierClass).ToString());
	}
	const FString DisqualifierSummary = DisqualifierTexts.IsEmpty() ? TEXT("<none>") : FString::Join(DisqualifierTexts, TEXT("; "));

	// A single simulated "click" - per FGHologram.h's doc comment on
	// DoMultiStepPlacement, true signals a release; a real player's
	// first click on a belt should fix the start point without
	// finishing the belt (only returns true once the whole sequence is
	// done), so false is expected here, not a failure indicator by
	// itself.
	const bool bStepComplete = BeltHologram->DoMultiStepPlacement(true);
	const ESplineHologramBuildStep StepAfterClick = BeltHologram->GetCurrentBuildStep();
	const bool bConnectionSnappedAfterClick = BeltHologram->IsConnectionSnapped(false);
	const TArray<AFGBuildable*> ConnectedBuildablesAfterClick = BeltHologram->GetAnyConnectedBuildables();

	UE_LOG(LogAIModAI, Display, TEXT("DebugCheckConveyorSnap: source=%s connectorLocation=%s stepInitial=%d | afterUpdateHologramPlacement: step=%d snapped=%s | afterTrySnapToActor: result=%s step=%d snapped=%s connectedCount=%d disqualifiers=[%s] | afterDoMultiStepPlacement(true): stepComplete=%s step=%d snapped=%s connectedCount=%d"),
		*SourceBuildableId, *SyntheticHit.Location.ToString(), static_cast<int32>(StepInitial),
		static_cast<int32>(StepAfterUpdate), bConnectionSnappedAfterUpdate ? TEXT("true") : TEXT("false"),
		bSnapped ? TEXT("true") : TEXT("false"), static_cast<int32>(StepAfterSnap), bConnectionSnappedAfterSnap ? TEXT("true") : TEXT("false"), ConnectedBuildablesAfterSnap.Num(), *DisqualifierSummary,
		bStepComplete ? TEXT("true") : TEXT("false"), static_cast<int32>(StepAfterClick), bConnectionSnappedAfterClick ? TEXT("true") : TEXT("false"), ConnectedBuildablesAfterClick.Num());

	// Never calls CanConstruct()/Construct() - this hologram is just a
	// scratch actor to be cleaned up now, regardless of outcome.
	Character->UnequipBuildGun();

	const bool bAnySnapIndicator = bConnectionSnappedAfterUpdate || bSnapped || bConnectionSnappedAfterSnap || bConnectionSnappedAfterClick || ConnectedBuildablesAfterClick.Num() > 0;
	if (!bAnySnapIndicator)
	{
		return FAIModOperationResult::Failure(TEXT("SNAP_FAILED"), TEXT("No snap indicator was ever true - see LogAIModAI for the full step-by-step trace"));
	}

	return FAIModOperationResult::Success();
}


FString UAIModFunctionLibrary::LogConveyorBeltTiersAsJson(UObject* WorldContextObject)
{
	// Read-only telemetry, no World/player needed - just resolves each
	// recipe -> buildable class -> CDO and reads GetSpeed() off it.
	// Reuses ResolveBuildableClassForRecipe (anonymous namespace above),
	// the same recipe->buildable-class resolution
	// ConstructBuildingAtPosition's lightweight-instance fallback uses.
	static const TCHAR* TierRecipePaths[] = {
		TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorBeltMk1.Recipe_ConveyorBeltMk1_C"),
		TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorBeltMk2.Recipe_ConveyorBeltMk2_C"),
		TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorBeltMk3.Recipe_ConveyorBeltMk3_C"),
		TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorBeltMk4.Recipe_ConveyorBeltMk4_C"),
		TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorBeltMk5.Recipe_ConveyorBeltMk5_C"),
		TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorBeltMk6.Recipe_ConveyorBeltMk6_C"),
	};

	TArray<TSharedPtr<FJsonValue>> TierJsonArray;
	for (const TCHAR* RecipePath : TierRecipePaths)
	{
		const TSubclassOf<AFGBuildable> BuildableClass = ResolveBuildableClassForRecipe(RecipePath);
		const AFGBuildableConveyorBase* BeltCDO = BuildableClass ? Cast<AFGBuildableConveyorBase>(BuildableClass->GetDefaultObject()) : nullptr;
		if (!BeltCDO)
		{
			UE_LOG(LogAIModAI, Warning, TEXT("LogConveyorBeltTiersAsJson: could not resolve a AFGBuildableConveyorBase CDO for '%s' - omitting"), RecipePath);
			continue;
		}

		const TSharedRef<FJsonObject> TierObject = MakeShared<FJsonObject>();
		TierObject->SetStringField(TEXT("recipeClass"), RecipePath);
		TierObject->SetStringField(TEXT("buildableClass"), BuildableClass->GetPathName());
		TierObject->SetNumberField(TEXT("speed"), BeltCDO->GetSpeed());

		// bendRadius/maxSplineLength: read off the HOLOGRAM
		// class's CDO (a different descriptor accessor,
		// UFGBuildDescriptor::GetHologramClass, than the buildable class
		// above) - both are EditDefaultsOnly Blueprint-configured class
		// defaults (AFGConveyorBeltHologram.h), so this works without
		// spawning anything. Lets an agent tell BEFORE attempting
		// a connection whether two points are too far apart for a single
		// belt segment, rather than discovering it via a failed
		// world.testConveyorBelt.
		if (const TSubclassOf<AFGConveyorBeltHologram> HologramClass = ResolveConveyorBeltHologramClassForRecipe(RecipePath))
		{
			if (const AFGConveyorBeltHologram* HologramCDO = Cast<AFGConveyorBeltHologram>(HologramClass->GetDefaultObject()))
			{
				TierObject->SetNumberField(TEXT("maxSplineLength"), HologramCDO->GetMaxSplineLength());
				TierObject->SetNumberField(TEXT("bendRadius"), HologramCDO->GetBendRadius());

				// mMaxIncline (degrees) has no public C++ getter, but is a
				// real UPROPERTY (EditDefaultsOnly) with a documented
				// meaning ("What is the maximum incline of the conveyor
				// belt (degrees)") - read via reflection, a single
				// hardcoded read-only field lookup, not a generic
				// property-access capability (CLAUDE.md's Safety and
				// Stability Boundary is about not exposing arbitrary
				// property get/set to external commands; this is neither
				// arbitrary - the field name is fixed in this file - nor
				// a write).
				if (const FFloatProperty* MaxInclineProperty = FindFProperty<FFloatProperty>(HologramClass, TEXT("mMaxIncline")))
				{
					TierObject->SetNumberField(TEXT("maxInclineDegrees"), MaxInclineProperty->GetPropertyValue_InContainer(HologramCDO));
				}
			}
		}

		TierJsonArray.Add(MakeShared<FJsonValueObject>(TierObject));
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetArrayField(TEXT("tiers"), TierJsonArray);

	FString JsonString;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonString);
	FJsonSerializer::Serialize(RootObject, Writer);

	UE_LOG(LogAIModAI, Display, TEXT("LogConveyorBeltTiersAsJson: %s"), *JsonString);

	return JsonString;
}


FString UAIModFunctionLibrary::LogPipelineTiersAsJson(UObject* WorldContextObject)
{
	// Read-only telemetry, no World/player needed - mirrors
	// LogConveyorBeltTiersAsJson's structure for the pipe equivalent.
	// Recipe_Pipeline (Mk1) and Recipe_PipelineMK2 (note the capital
	// "MK2", unlike belts' "Mk2" - confirmed from the actual filename on
	// disk) are the two real tiers.
	static const TCHAR* TierRecipePaths[] = {
		TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_Pipeline.Recipe_Pipeline_C"),
		TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PipelineMK2.Recipe_PipelineMK2_C"),
	};

	TArray<TSharedPtr<FJsonValue>> TierJsonArray;
	for (const TCHAR* RecipePath : TierRecipePaths)
	{
		const TSubclassOf<AFGBuildable> BuildableClass = ResolveBuildableClassForRecipe(RecipePath);
		const AFGBuildablePipeline* PipelineCDO = BuildableClass ? Cast<AFGBuildablePipeline>(BuildableClass->GetDefaultObject()) : nullptr;
		if (!PipelineCDO)
		{
			UE_LOG(LogAIModAI, Warning, TEXT("LogPipelineTiersAsJson: could not resolve a AFGBuildablePipeline CDO for '%s' - omitting"), RecipePath);
			continue;
		}

		const TSharedRef<FJsonObject> TierObject = MakeShared<FJsonObject>();
		TierObject->SetStringField(TEXT("recipeClass"), RecipePath);
		TierObject->SetStringField(TEXT("buildableClass"), BuildableClass->GetPathName());
		// flowLimit: "Maximum flow through this pipe in cubic meters.
		// [m^3/s]" per FGBuildablePipeline.h's own doc comment - a real
		// documented unit, unlike belts' ambiguous GetSpeed().
		TierObject->SetNumberField(TEXT("flowLimit"), PipelineCDO->GetFlowLimit());

		// maxSplineLength/bendRadius/minBendRadius: ALL
		// THREE are private on AFGPipelineHologram with no public
		// getters (unlike belts, where two of three have public getters)
		// - per source, all real UPROPERTY(EditDefaultsOnly)
		// fields, read via reflection same as belts' mMaxIncline.
		if (const TSubclassOf<AFGPipelineHologram> HologramClass = ResolvePipelineHologramClassForRecipe(RecipePath))
		{
			if (const AFGPipelineHologram* HologramCDO = Cast<AFGPipelineHologram>(HologramClass->GetDefaultObject()))
			{
				if (const FFloatProperty* MaxSplineLengthProperty = FindFProperty<FFloatProperty>(HologramClass, TEXT("mMaxSplineLength")))
				{
					TierObject->SetNumberField(TEXT("maxSplineLength"), MaxSplineLengthProperty->GetPropertyValue_InContainer(HologramCDO));
				}
				if (const FFloatProperty* BendRadiusProperty = FindFProperty<FFloatProperty>(HologramClass, TEXT("mBendRadius")))
				{
					TierObject->SetNumberField(TEXT("bendRadius"), BendRadiusProperty->GetPropertyValue_InContainer(HologramCDO));
				}
				if (const FFloatProperty* MinBendRadiusProperty = FindFProperty<FFloatProperty>(HologramClass, TEXT("mMinBendRadius")))
				{
					TierObject->SetNumberField(TEXT("minBendRadius"), MinBendRadiusProperty->GetPropertyValue_InContainer(HologramCDO));
				}
			}
		}

		TierJsonArray.Add(MakeShared<FJsonValueObject>(TierObject));
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetArrayField(TEXT("tiers"), TierJsonArray);

	FString JsonString;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonString);
	FJsonSerializer::Serialize(RootObject, Writer);

	UE_LOG(LogAIModAI, Display, TEXT("LogPipelineTiersAsJson: %s"), *JsonString);

	return JsonString;
}


FString UAIModFunctionLibrary::LogPipelinePumpTiersAsJson(UObject* WorldContextObject)
{
	// Read-only telemetry, no World/player needed - mirrors
	// LogPipelineTiersAsJson's structure. Recipe_PipelinePump (Mk1) and
	// Recipe_PipelinePumpMK2 (capital "MK2", confirmed from the real
	// asset filenames on disk, matching the pipe tiers' own naming) are
	// the two real pump tiers.
	//
	// Recipe_Valve is included here too, NOT a separate tier list - the
	// binary asset (Build_Valve.uasset references the literal class name
	// "FGBuildablePipelinePump", grepped from the .uasset itself) shows the
	// in-game Valve IS a Blueprint variant of AFGBuildablePipelinePump, not
	// a separate C++ class. This also explains why
	// AFGBuildablePipelinePump.h's own SetUserFlowLimit() doc comment uses
	// valve terminology ("Set this to -1 to use the max limit, i.e. valve
	// is fully opened") - the class is dual-purpose. A "kind" field
	// distinguishes Pump vs Valve entries for the caller since the RPC
	// name itself still says "pump" tiers.
	static const TCHAR* PumpTierRecipePaths[] = {
		TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PipelinePump.Recipe_PipelinePump_C"),
		TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PipelinePumpMK2.Recipe_PipelinePumpMK2_C"),
	};
	static const TCHAR* ValveRecipePath = TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_Valve.Recipe_Valve_C");

	TArray<TSharedPtr<FJsonValue>> TierJsonArray;
	for (const TCHAR* RecipePath : PumpTierRecipePaths)
	{
		const TSubclassOf<AFGBuildable> BuildableClass = ResolveBuildableClassForRecipe(RecipePath);
		const AFGBuildablePipelinePump* PumpCDO = BuildableClass ? Cast<AFGBuildablePipelinePump>(BuildableClass->GetDefaultObject()) : nullptr;
		if (!PumpCDO)
		{
			UE_LOG(LogAIModAI, Warning, TEXT("LogPipelinePumpTiersAsJson: could not resolve a AFGBuildablePipelinePump CDO for '%s' - omitting"), RecipePath);
			continue;
		}

		const TSharedRef<FJsonObject> TierObject = MakeShared<FJsonObject>();
		TierObject->SetStringField(TEXT("kind"), TEXT("Pump"));
		TierObject->SetStringField(TEXT("recipeClass"), RecipePath);
		TierObject->SetStringField(TEXT("buildableClass"), BuildableClass->GetPathName());
		// All three real public BlueprintPure getters - no reflection
		// needed, unlike the pipe tier's maxSplineLength/bendRadius/
		// minBendRadius. Units per AFGBuildablePipelinePump.h's own doc
		// comments: headlift in meters, flow in [m^3/s].
		TierObject->SetNumberField(TEXT("maxHeadLift"), PumpCDO->GetMaxHeadLift());
		TierObject->SetNumberField(TEXT("designHeadLift"), PumpCDO->GetDesignHeadLift());
		TierObject->SetNumberField(TEXT("defaultFlowLimit"), PumpCDO->GetDefaultFlowLimit());

		TierJsonArray.Add(MakeShared<FJsonValueObject>(TierObject));
	}

	{
		const TSubclassOf<AFGBuildable> ValveBuildableClass = ResolveBuildableClassForRecipe(ValveRecipePath);
		const AFGBuildablePipelinePump* ValveCDO = ValveBuildableClass ? Cast<AFGBuildablePipelinePump>(ValveBuildableClass->GetDefaultObject()) : nullptr;
		if (ValveCDO)
		{
			const TSharedRef<FJsonObject> ValveObject = MakeShared<FJsonObject>();
			ValveObject->SetStringField(TEXT("kind"), TEXT("Valve"));
			ValveObject->SetStringField(TEXT("recipeClass"), ValveRecipePath);
			ValveObject->SetStringField(TEXT("buildableClass"), ValveBuildableClass->GetPathName());
			ValveObject->SetNumberField(TEXT("maxHeadLift"), ValveCDO->GetMaxHeadLift());
			ValveObject->SetNumberField(TEXT("designHeadLift"), ValveCDO->GetDesignHeadLift());
			ValveObject->SetNumberField(TEXT("defaultFlowLimit"), ValveCDO->GetDefaultFlowLimit());
			TierJsonArray.Add(MakeShared<FJsonValueObject>(ValveObject));
		}
		else
		{
			UE_LOG(LogAIModAI, Warning, TEXT("LogPipelinePumpTiersAsJson: could not resolve a AFGBuildablePipelinePump CDO for the Valve recipe '%s' - omitting"), ValveRecipePath);
		}
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetArrayField(TEXT("tiers"), TierJsonArray);

	FString JsonString;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonString);
	FJsonSerializer::Serialize(RootObject, Writer);

	UE_LOG(LogAIModAI, Display, TEXT("LogPipelinePumpTiersAsJson: %s"), *JsonString);

	return JsonString;
}


FString UAIModFunctionLibrary::LogPipeFluidBoxesAsJson(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		UE_LOG(LogAIModAI, Warning, TEXT("LogPipeFluidBoxesAsJson: no valid world context"));
		return TEXT("{}");
	}

	// Scoped to AFGBuildablePipeline (real pipe SEGMENTS) specifically -
	// IFGFluidIntegrantInterface is also implemented by pumps/storage
	// tanks/etc, but segments are what the volume/fill query is about.
	// Widening to other fluid integrants is a separate future addition if
	// needed, not done here.
	TArray<TSharedPtr<FJsonValue>> BoxJsonArray;
	for (TActorIterator<AFGBuildablePipeline> It(World); It; ++It)
	{
		AFGBuildablePipeline* Pipe = *It;
		if (!IsValid(Pipe))
		{
			continue;
		}
		FFluidBox* Box = Pipe->GetFluidBox();
		if (!Box)
		{
			continue;
		}

		const TSharedRef<FJsonObject> BoxObject = MakeShared<FJsonObject>();
		BoxObject->SetStringField(TEXT("id"), Pipe->GetPathName());
		// GetLength() is documented "Length of the pipe in centimeters"
		// (FGBuildablePipeBase.h) - real unit, paired here with the
		// fluid box's own real volume for the length-to-volume
		// relationship.
		BoxObject->SetNumberField(TEXT("lengthCm"), Pipe->GetLength());
		BoxObject->SetNumberField(TEXT("contentM3"), Box->Content);
		BoxObject->SetNumberField(TEXT("maxContentM3"), Box->MaxContent);
		BoxObject->SetNumberField(TEXT("fillPct"), Box->MaxContent > 0.f ? (Box->Content / Box->MaxContent) : 0.f);
		BoxObject->SetNumberField(TEXT("maxOverfillPct"), Box->MaxOverfillPct);
		BoxObject->SetNumberField(TEXT("flowThrough"), Box->FlowThrough);
		BoxObject->SetNumberField(TEXT("flowFill"), Box->FlowFill);
		BoxObject->SetNumberField(TEXT("flowDrain"), Box->FlowDrain);
		BoxObject->SetNumberField(TEXT("flowLimit"), Box->FlowLimit);
		BoxObject->SetNumberField(TEXT("pressureColumn"), Box->PressureColumn);
		BoxObject->SetNumberField(TEXT("elevationPressureColumn"), Box->ElevationPressureColumn);
		BoxObject->SetNumberField(TEXT("addedPressure"), Box->GetCurrentAddedPressure());
		BoxObject->SetNumberField(TEXT("pressureGroup"), Box->PressureGroup);
		BoxObject->SetNumberField(TEXT("z"), Box->Z);

		BoxJsonArray.Add(MakeShared<FJsonValueObject>(BoxObject));
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetArrayField(TEXT("pipes"), BoxJsonArray);

	const FString JsonString = WriteCondensedJson(RootObject);

	UE_LOG(LogAIModAI, Display, TEXT("LogPipeFluidBoxesAsJson: %d pipe segment(s)"), BoxJsonArray.Num());

	return JsonString;
}


FString UAIModFunctionLibrary::LogPipeReservoirTiersAsJson(UObject* WorldContextObject)
{
	// Read-only telemetry, no World/player needed - mirrors
	// LogPipelinePumpTiersAsJson's structure. Both recipes confirmed to
	// resolve to AFGBuildablePipeReservoir via a direct grep of each
	// .uasset's binary for the embedded class name string.
	static const TCHAR* TierRecipePaths[] = {
		TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PipeStorageTank.Recipe_PipeStorageTank_C"),
		TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_IndustrialTank.Recipe_IndustrialTank_C"),
	};

	TArray<TSharedPtr<FJsonValue>> TierJsonArray;
	for (const TCHAR* RecipePath : TierRecipePaths)
	{
		const TSubclassOf<AFGBuildable> BuildableClass = ResolveBuildableClassForRecipe(RecipePath);
		const AFGBuildablePipeReservoir* ReservoirCDO = BuildableClass ? Cast<AFGBuildablePipeReservoir>(BuildableClass->GetDefaultObject()) : nullptr;
		if (!ReservoirCDO)
		{
			UE_LOG(LogAIModAI, Warning, TEXT("LogPipeReservoirTiersAsJson: could not resolve a AFGBuildablePipeReservoir CDO for '%s' - omitting"), RecipePath);
			continue;
		}

		const TSharedRef<FJsonObject> TierObject = MakeShared<FJsonObject>();
		TierObject->SetStringField(TEXT("recipeClass"), RecipePath);
		TierObject->SetStringField(TEXT("buildableClass"), BuildableClass->GetPathName());
		// Real public BlueprintPure getters - no reflection needed.
		// Units per AFGBuildablePipeReservoir.h's own doc comments:
		// capacity in [m^3], flow limit in [m^3/s].
		TierObject->SetNumberField(TEXT("maxContentM3"), ReservoirCDO->GetFluidContentMax());
		TierObject->SetNumberField(TEXT("flowLimit"), ReservoirCDO->GetFlowLimit());

		TierJsonArray.Add(MakeShared<FJsonValueObject>(TierObject));
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetArrayField(TEXT("tiers"), TierJsonArray);

	const FString JsonString = WriteCondensedJson(RootObject);

	UE_LOG(LogAIModAI, Display, TEXT("LogPipeReservoirTiersAsJson: %s"), *JsonString);

	return JsonString;
}


// LogConveyorAttachmentCatalogAsJson - per
// docs/conveyor-attachment-research.md,
// splitters/mergers use AFGConveyorAttachmentHologram : AFGFactoryHologram
// : AFGBuildableHologram - the SAME simple, single-step hologram lineage
// already proven for Miners/Smelters/Constructors, NOT the AFGSplineHologram
// branch belts/pipes needed special multi-click driving for. That means
// ConstructBuildingAtPosition/world.placeBuilding (already generic) and
// ConstructConveyorBelt/world.connectConveyor (source/dest already not
// restricted to machines) place and connect splitters/mergers with ZERO
// new construction code - deliberately NOT duplicating a
// ConstructSplitter-style function that would just be a thin, unnecessary
// wrapper. This function exists purely to report the real recipe catalog
// and real per-class input/output UFGFactoryConnectionComponent counts -
// read generically via GetDirection() on each buildable class CDO's
// components (the same technique world.connections itself uses), rather
// than hardcoding the commonly-known 1-in/3-out (splitter) / 3-in/1-out
// (merger) figures, since AFGBuildableConveyorAttachment's header doesn't
// declare them as a literal constant anywhere.
FString UAIModFunctionLibrary::LogConveyorAttachmentCatalogAsJson(UObject* WorldContextObject)
{
	static const TCHAR* RecipePaths[] = {
		TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorAttachmentSplitter.Recipe_ConveyorAttachmentSplitter_C"),
		TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorAttachmentSplitterSmart.Recipe_ConveyorAttachmentSplitterSmart_C"),
		TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorAttachmentSplitterProgrammable.Recipe_ConveyorAttachmentSplitterProgrammable_C"),
		TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorAttachmentMerger.Recipe_ConveyorAttachmentMerger_C"),
		TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorAttachmentMergerPriority.Recipe_ConveyorAttachmentMergerPriority_C"),
	};

	TArray<TSharedPtr<FJsonValue>> EntryJsonArray;
	for (const TCHAR* RecipePath : RecipePaths)
	{
		const TSubclassOf<AFGBuildable> BuildableClass = ResolveBuildableClassForRecipe(RecipePath);
		const AFGBuildableConveyorAttachment* AttachmentCDO = BuildableClass ? Cast<AFGBuildableConveyorAttachment>(BuildableClass->GetDefaultObject()) : nullptr;
		if (!AttachmentCDO)
		{
			UE_LOG(LogAIModAI, Warning, TEXT("LogConveyorAttachmentCatalogAsJson: could not resolve a AFGBuildableConveyorAttachment CDO for '%s' - omitting"), RecipePath);
			continue;
		}

		// GetComponents<T>() on a CDO only finds NATIVE
		// (CreateDefaultSubobject) components - these
		// buildables add their connectors via the Blueprint's Simple
		// Construction Script, which does NOT populate onto the CDO
		// (SCS-added components only exist on a real spawned instance), so
		// GetComponents<T>() reports inputCount=0/outputCount=0 for every
		// entry. AFGBuildable::GetDefaultComponents<T>() (FGBuildable.h) is
		// FactoryGame's own purpose-built helper for exactly this - walks
		// the Blueprint inheritance chain's SimpleConstructionScript
		// nodes (resolving InheritableComponentHandler overrides) in
		// addition to the native GetComponents() scan, giving the
		// correct full list without spawning anything.
		TArray<UFGFactoryConnectionComponent*> Connections;
		AttachmentCDO->GetDefaultComponents<UFGFactoryConnectionComponent>(Connections);
		int32 InputCount = 0;
		int32 OutputCount = 0;
		for (const UFGFactoryConnectionComponent* Connection : Connections)
		{
			if (!IsValid(Connection)) { continue; }
			if (Connection->GetDirection() == EFactoryConnectionDirection::FCD_INPUT) { ++InputCount; }
			else if (Connection->GetDirection() == EFactoryConnectionDirection::FCD_OUTPUT) { ++OutputCount; }
		}

		const TSharedRef<FJsonObject> EntryObject = MakeShared<FJsonObject>();
		EntryObject->SetStringField(TEXT("recipeClass"), RecipePath);
		EntryObject->SetStringField(TEXT("buildableClass"), BuildableClass->GetPathName());
		EntryObject->SetNumberField(TEXT("inputCount"), InputCount);
		EntryObject->SetNumberField(TEXT("outputCount"), OutputCount);
		// supportsSortRules: Smart and Programmable
		// splitters share the AFGBuildableSplitterSmart native class -
		// per-output item-type routing (mSortRules, AddSortRule/
		// RemoveSortRuleAt/SetSortRuleAt/GetSortRules, confirmed public
		// on FGBuildableSplitterSmart.h) needs RPC support this project
		// does NOT yet have. Placement/connection works today via the
		// generic mechanism above; sort-rule configuration is a
		// genuinely separate, not-yet-built capability - this flag lets
		// an agent know not to assume a placed Smart/Programmable
		// splitter can already be configured to route by item type.
		EntryObject->SetBoolField(TEXT("supportsSortRules"), Cast<AFGBuildableSplitterSmart>(AttachmentCDO) != nullptr);

		EntryJsonArray.Add(MakeShared<FJsonValueObject>(EntryObject));
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetArrayField(TEXT("attachments"), EntryJsonArray);

	FString JsonString;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonString);
	FJsonSerializer::Serialize(RootObject, Writer);

	UE_LOG(LogAIModAI, Display, TEXT("LogConveyorAttachmentCatalogAsJson: %s"), *JsonString);

	return JsonString;
}



// Decoy-instigator strategy: the RealCharacter path forces the REAL
// player's controller rotation every poll tick, which visibly hijacks the
// player's camera for the full duration of every call (minutes at a time
// in intermittent bursts on a multi-belt build). Root cause:
// AutoRouteSpline()'s routing (stub source, unverifiable directly)
// empirically depends on the CONSTRUCTION INSTIGATOR's controller
// rotation, not just the connector geometry. This strategy spawns the
// hologram via the real, public,
// non-stub AFGHologram::SpawnHologramFromRecipe() (FGHologram.h) with an
// explicit throwaway APawn+AController as the CONSTRUCTION INSTIGATOR,
// instead of implicitly using Character->GetBuildGun()'s real equipped
// hologram - the real player's BuildGun/camera is never touched at all
// (no HotKeyRecipe/GetBuildGun/UnequipBuildGun anywhere in this
// function). The deterministic-look rotation still gets set, but on the
// DECOY controller only.
//
// Trade-off this introduces: swapping the instigator away from Character
// means CanConstruct()'s real UFGCDUnaffordable check would resolve
// against the decoy's (nonexistent) inventory, not the player's - so
// this function now manually verifies and charges the recipe's real
// ingredient cost from the player's OWN inventory (same "verify every
// ingredient before touching any of them" pattern as SimulatedCraft
// above), and always ignores UFGCDUnaffordable in the poll loop's
// disqualifier check (not just when the UnlimitedResources setting is
// on) since affordability is now handled explicitly, before Construct()
// is ever called. Not yet verified at runtime - AutoRouteSpline/
// GenerateAndUpdateSpline/ConfigureActor/Construct are all stub source
// in this SDK, so whether a decoy instigator produces correct routing
// and a correctly-owned/replicated real belt actor is a runtime
// question, not something readable from source.
// Serialize a belt/spline HOLOGRAM's computed spline to a JSON object string
// (the predicted mid-span path before construction). The hologram builds a real
// USplineComponent (AFGSplineHologram::mSplineComponent) during placement; it's
// a component subobject, so FindComponentByClass reaches it even though
// GetSplineData()/mSplineComponent are protected. Same point/tangent shape as
// LogSplineGeometryAsJson (built belts), so callers parse one format. Empty
// points[] if the component isn't populated yet. Used by world.testConveyorBelt
// (dry run) to answer "what path will this belt actually take?" without building
// it - the missing piece for verifying long-span routing.

void UAIModFunctionLibrary::ConstructConveyorBelt(UObject* WorldContextObject, const FString& SourceBuildableId, const FString& DestBuildableId, const FString& RecipeClassPath, const FString& RouteMode, const FString& InstigatorStrategy, const TOptional<FVector>& SourceConnectorPosition, const TOptional<FVector>& DestConnectorPosition, bool bDryRun, TFunction<void(const FAIModOperationResult&)> OnComplete)
{
	// Strategy dispatch: multiple competing strategies per-compile, so if
	// one test fails, other theories can be attempted before requiring a
	// fresh build. "RealCharacter" delegates to the original implementation
	// (see its own comment) - everything below this point is the
	// decoy-instigator path, parameterized only by which controller class
	// possesses the decoy pawn.
	//
	// SourceConnectorPosition/DestConnectorPosition (see
	// FindFreeFactoryConnectionNear's comment): when provided, target one
	// SPECIFIC connector by its real world position instead of "the first
	// free one of the right direction" - required for deterministic per-port
	// selection on a multi-output buildable like a splitter. Optional and
	// backward-compatible - omitting them keeps existing callers unchanged.
	// Default strategy: "RealCharacter", the ONLY strategy that
	// clears UFGCDInitializing. The decoy-instigator strategies below
	// (PlayerController/AIController/LocalPlayer) are an experiment that
	// conclusively, permanently fails on UFGCDInitializing at every location -
	// a raw caller hitting a "PlayerController" default gets belts that
	// never build (KL-1 in docs/known-limitations.md). Default
	// to the working path so the RPC is correct out of the box; the decoy
	// strategies remain reachable by explicit opt-in only.
	const FString Strategy = InstigatorStrategy.IsEmpty() ? TEXT("RealCharacter") : InstigatorStrategy;
	if (Strategy.Equals(TEXT("RealCharacter"), ESearchCase::IgnoreCase))
	{
		ConstructConveyorBelt_RealCharacterStrategy(WorldContextObject, SourceBuildableId, DestBuildableId, RecipeClassPath, RouteMode, SourceConnectorPosition, DestConnectorPosition, bDryRun, MoveTemp(OnComplete));
		return;
	}
	const bool bUseAIController = Strategy.Equals(TEXT("AIController"), ESearchCase::IgnoreCase);
	const bool bUsePlayerController = Strategy.Equals(TEXT("PlayerController"), ESearchCase::IgnoreCase);
	// "LocalPlayer" - not yet verified at runtime (see this
	// strategy's own comment below, and docs/camera-hijack-and-second-
	// player-research.md, for the research this is based on). Spawns
	// a GENUINE second ULocalPlayer via UGameInstance::CreateLocalPlayer()
	// - confirmed-real, non-stub engine mechanism that routes through the
	// same Login/PostLogin path a real multiplayer join uses - instead of
	// a bare decoy actor, on the hypothesis that whatever gates
	// UFGCDInitializing cares about genuine local-player identity, not
	// just controller class (both AIController and PlayerController
	// decoys already conclusively failed identically).
	const bool bUseLocalPlayer = Strategy.Equals(TEXT("LocalPlayer"), ESearchCase::IgnoreCase);
	if (!bUseAIController && !bUsePlayerController && !bUseLocalPlayer)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("INVALID_INSTIGATOR_STRATEGY"),
			FString::Printf(TEXT("'%s' is not one of \"RealCharacter\", \"AIController\", \"PlayerController\", \"LocalPlayer\""), *Strategy)));
		return;
	}

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
	UFGInventoryComponent* PlayerInventory = Character->GetInventory();
	if (!PlayerInventory)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No player inventory found")));
		return;
	}

	AFGBuildable* SourceBuildable = FindBuildableById(World, SourceBuildableId);
	if (!SourceBuildable)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No buildable found with id '%s'"), *SourceBuildableId)));
		return;
	}
	AFGBuildable* DestBuildable = FindBuildableById(World, DestBuildableId);
	if (!DestBuildable)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No buildable found with id '%s'"), *DestBuildableId)));
		return;
	}

	UFGFactoryConnectionComponent* SourceConnection = SourceConnectorPosition.IsSet()
		? FindFreeFactoryConnectionNear(SourceBuildable, EFactoryConnectionDirection::FCD_OUTPUT, SourceConnectorPosition.GetValue())
		: FindFreeFactoryConnection(SourceBuildable, EFactoryConnectionDirection::FCD_OUTPUT);
	if (!SourceConnection)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_FACTORY_CONNECTION"), FString::Printf(TEXT("'%s' has no free Output factory connection component"), *SourceBuildableId)));
		return;
	}
	UFGFactoryConnectionComponent* DestConnection = DestConnectorPosition.IsSet()
		? FindFreeFactoryConnectionNear(DestBuildable, EFactoryConnectionDirection::FCD_INPUT, DestConnectorPosition.GetValue())
		: FindFreeFactoryConnection(DestBuildable, EFactoryConnectionDirection::FCD_INPUT);
	if (!DestConnection)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_FACTORY_CONNECTION"), FString::Printf(TEXT("'%s' has no free Input factory connection component"), *DestBuildableId)));
		return;
	}

	// Caller-chosen belt tier - any of Recipe_ConveyorBeltMk1..Mk6
	// resolve the same way. Same validation posture as
	// ConstructBuildingAtPosition's RecipeClassPath - not a generic
	// "load any class" capability, just requires a real UFGRecipe.
	UClass* BeltRecipeClass = LoadObject<UClass>(nullptr, *RecipeClassPath);
	if (!BeltRecipeClass || !BeltRecipeClass->IsChildOf(UFGRecipe::StaticClass()))
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("INVALID_RECIPE"), FString::Printf(TEXT("'%s' did not resolve to a UFGRecipe subclass"), *RecipeClassPath)));
		return;
	}
	const TSubclassOf<UFGRecipe> RecipeClass = BeltRecipeClass;

	// Verify affordability up front (never partially consume ingredients
	// for a belt that can't complete) - same pattern as SimulatedCraft.
	// Actual deduction happens later, immediately before Construct(), once
	// every other disqualifier has been confirmed clear.
	const TArray<FItemAmount> BeltIngredients = UFGRecipe::GetIngredients(World, RecipeClass);
	{
		TArray<FString> ShortfallDescriptions;
		for (const FItemAmount& Ingredient : BeltIngredients)
		{
			if (!Ingredient.ItemClass || !PlayerInventory->HasItems(Ingredient.ItemClass, Ingredient.Amount))
			{
				const int32 Have = Ingredient.ItemClass ? PlayerInventory->GetNumItems(Ingredient.ItemClass) : 0;
				ShortfallDescriptions.Add(FString::Printf(TEXT("%s (need %d, have %d)"),
					Ingredient.ItemClass ? *Ingredient.ItemClass->GetName() : TEXT("<null>"), Ingredient.Amount, Have));
			}
		}
		if (!ShortfallDescriptions.IsEmpty())
		{
			OnComplete(FAIModOperationResult::Failure(TEXT("INSUFFICIENT_INGREDIENTS"),
				FString::Printf(TEXT("Missing: %s"), *FString::Join(ShortfallDescriptions, TEXT("; ")))));
			return;
		}
	}

	// Decoy pawn+controller - stands in for the real player as the
	// hologram's construction instigator so nothing here ever touches
	// Character's actual camera/equipped item. Cleaned up on every exit
	// path below via CleanupScratch(). Three concrete candidates,
	// selectable via params.instigatorStrategy without a recompile:
	// AIController and PlayerController (both leave the hologram permanently
	// stuck on UFGCDInitializing - present immediately after the first
	// click, never clears across the full 120-tick poll, even though
	// stepComplete/connectedCount both look correct - so controller CLASS
	// is not the variable) and LocalPlayer (below).
	APawn* DecoyPawn = nullptr;
	AController* DecoyController = nullptr;
	ULocalPlayer* NewLocalPlayer = nullptr; // only set for the LocalPlayer strategy - drives cleanup below

	if (bUseLocalPlayer)
	{
		// GENUINELY LOCAL second player - not yet verified at runtime (see
		// docs/camera-hijack-and-second-player-research.md for the citations
		// behind every claim in this comment).
		// UGameInstance::CreateLocalPlayer() is confirmed-real, non-stub
		// engine source (Engine\Private\GameInstance.cpp) - with
		// bSpawnPlayerController=true and NM_Standalone (true for this
		// project's single-player target), it drives the SAME
		// AGameModeBase::Login()/PostLogin() path a real multiplayer
		// client join uses, which - per AFGGameMode's real confirmed
		// default (FGGameMode.cpp) - spawns and possesses a genuine
		// AFGCharacterPlayer via DefaultPawnClass. That's the whole
		// reason to try this: AIController/PlayerController decoys are
		// bare, never-joined actors; this one goes through the actual
		// join flow FactoryGame itself uses.
		UGameInstance* GameInstance = World->GetGameInstance();
		if (!GameInstance)
		{
			OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No UGameInstance for this world")));
			return;
		}
		FString CreateLocalPlayerError;
		NewLocalPlayer = GameInstance->CreateLocalPlayer(FPlatformUserId::CreateFromInternalId(1), CreateLocalPlayerError, /*bSpawnPlayerController=*/true);
		if (!NewLocalPlayer)
		{
			OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), FString::Printf(TEXT("CreateLocalPlayer failed: %s"), *CreateLocalPlayerError)));
			return;
		}
		// Headless - confirmed-real engine mechanism
		// (GameViewportClient.cpp): forces every non-primary local
		// player's viewport rect to zero size, so nothing is ever
		// rendered for this second player and no split-screen ever
		// appears. Applied immediately, before anything else can render
		// a frame with the new player in it.
		if (UGameViewportClient* ViewportClient = World->GetGameViewport())
		{
			ViewportClient->SetForceDisableSplitscreen(true);
		}

		APlayerController* NewPC = NewLocalPlayer->GetPlayerController(World);
		APawn* NewPawn = NewPC ? NewPC->GetPawn() : nullptr;
		if (!NewPC || !NewPawn)
		{
			GameInstance->RemoveLocalPlayer(NewLocalPlayer);
			OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"),
				FString::Printf(TEXT("CreateLocalPlayer succeeded but no PlayerController/Pawn resulted (PC=%s pawn=%s) - login/possession may not have completed synchronously"),
					NewPC ? TEXT("valid") : TEXT("null"), NewPawn ? TEXT("valid") : TEXT("null"))));
			return;
		}
		DecoyController = NewPC;
		DecoyPawn = NewPawn;
		UE_LOG(LogAIModAI, Display, TEXT("ConstructConveyorBelt: LocalPlayer strategy - spawned real second local player, pawn class=%s"), *NewPawn->GetClass()->GetName());
	}
	else
	{
		DecoyPawn = World->SpawnActor<APawn>(APawn::StaticClass(), SourceConnection->GetConnectorLocation(), FRotator::ZeroRotator);
		// Plain AController is abstract in this engine build (SpawnActor
		// fails: "class Controller is abstract").
		DecoyController = bUseAIController
			? Cast<AController>(World->SpawnActor<AAIController>(AAIController::StaticClass()))
			: Cast<AController>(World->SpawnActor<APlayerController>(APlayerController::StaticClass()));
		if (!DecoyPawn || !DecoyController)
		{
			if (DecoyPawn) { DecoyPawn->Destroy(); }
			if (DecoyController) { DecoyController->Destroy(); }
			OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("Failed to spawn the decoy instigator pawn/controller")));
			return;
		}
		DecoyController->Possess(DecoyPawn);
	}

	// Replaces every former Character->UnequipBuildGun() call. For the two
	// bare-decoy strategies, cleanup is just destroying our own scratch
	// actors. For LocalPlayer, destroying the PlayerController directly
	// would leave the ULocalPlayer wrapper itself dangling/leaked -
	// GameInstance->RemoveLocalPlayer() is the confirmed-real, proper
	// engine teardown (destroys the PlayerController AND unregisters the
	// ULocalPlayer) - see docs/camera-hijack-and-second-player-research.md.
	// Defined before the SpawnHologramFromRecipe call so it covers that
	// call's own failure path too, not just later ones.
	UGameInstance* CleanupGameInstance = World->GetGameInstance();
	auto CleanupScratch = [CleanupGameInstance, bUseLocalPlayer](AFGHologram* H, APawn* DPawn, AController* DController, ULocalPlayer* LP)
	{
		if (IsValid(H)) { H->Destroy(); }
		if (bUseLocalPlayer)
		{
			if (LP && CleanupGameInstance) { CleanupGameInstance->RemoveLocalPlayer(LP); }
		}
		else
		{
			if (IsValid(DController)) { DController->Destroy(); }
			if (IsValid(DPawn)) { DPawn->Destroy(); }
		}
	};

	// hologramOwner is ALSO the decoy: passing Character here - even with
	// DecoyPawn already used as the instigator - still visibly swings the
	// REAL player's camera (the player turns to face the connector).
	// Something in
	// the hologram's construction/camera-preview logic evidently reads
	// the OWNER, not just the instigator, for whatever drives that. Fully
	// decoupling Character from both parameters is the only way to be
	// sure nothing in this call can reach the real player's camera - the
	// belt's material cost is already charged from Character's inventory
	// manually (see BeltIngredients above), so nothing here still needs
	// Character to be the owner for cost/affordability purposes either.
	AFGHologram* Hologram = AFGHologram::SpawnHologramFromRecipe(RecipeClass, DecoyPawn, SourceConnection->GetConnectorLocation(), DecoyPawn);
	AFGConveyorBeltHologram* BeltHologram = Cast<AFGConveyorBeltHologram>(Hologram);
	if (!BeltHologram)
	{
		CleanupScratch(Hologram, DecoyPawn, DecoyController, NewLocalPlayer);
		OnComplete(FAIModOperationResult::Failure(TEXT("HOLOGRAM_SPAWN_FAILED"),
			FString::Printf(TEXT("SpawnHologramFromRecipe(%s) did not result in an AFGConveyorBeltHologram (got %s)"),
				*RecipeClassPath, Hologram ? *Hologram->GetClass()->GetName() : TEXT("null"))));
		return;
	}

	// A hologram spawned via SpawnHologramFromRecipe (bypassing the real
	// BuildGun's equip flow) stays stuck on UFGCDInitializing indefinitely -
	// CANNOT_CONSTRUCT "Initializing (hard)" after the full poll window
	// elapses, unlike the BuildGun-driven path this replaces, which clears
	// it within a tick or two, for BOTH bare-decoy strategies. Explicitly
	// enabling tick here is cheap and safe even though it doesn't fix that
	// on its own.
	BeltHologram->SetActorTickEnabled(true);

	// RouteMode: the 2-click TrySnapToActor flow fails ("Conveyor Belt is
	// too long!"/"Invalid placement!") for ANY meaningful direction mismatch
	// between source and destination connectors. Real, on-disk asset paths
	// (grepped from Holo_ConveyorBelt.uasset's own string table):
	// AFGHologram::SetBuildModeOverride() (public,
	// FGHologram.h) accepts one of
	// "/Game/FactoryGame/Buildable/Factory/-Shared/BuildGunModes/BuildMode_Default"
	// (the implicit default when nothing is overridden - what the player
	// UX calls "Auto"), "...BuildMode_Straight", or "...BuildMode_Curve"
	// - AFGConveyorBeltHologram exposes exactly two of these via its own
	// mBuildModeStraight/mBuildModeCurve fields (GetSupportedBuildModes_Implementation).
	// Empty RouteMode (default) leaves the hologram's own default mode
	// untouched. Not yet verified at runtime that forcing Curve actually
	// resolves the bend failures above - a well-evidenced hypothesis
	// (AutoRouteSpline's own doc comment: "routes the spline to the new
	// location, inserting bends and straights"), not a proven fix, since
	// the private engine logic
	// behind SetBuildModeOverride()/AutoRouteSpline() is stub-source in
	// this SDK like everything else - only the public entry point and
	// real asset paths are confirmed from source/binary inspection.
	if (!RouteMode.IsEmpty())
	{
		FString RouteModeAssetPath;
		if (RouteMode.Equals(TEXT("Straight"), ESearchCase::IgnoreCase))
		{
			RouteModeAssetPath = TEXT("/Game/FactoryGame/Buildable/Factory/-Shared/BuildGunModes/BuildMode_Straight.BuildMode_Straight_C");
		}
		else if (RouteMode.Equals(TEXT("Curve"), ESearchCase::IgnoreCase))
		{
			RouteModeAssetPath = TEXT("/Game/FactoryGame/Buildable/Factory/-Shared/BuildGunModes/BuildMode_Curve.BuildMode_Curve_C");
		}
		else if (RouteMode.Equals(TEXT("Auto"), ESearchCase::IgnoreCase) || RouteMode.Equals(TEXT("Default"), ESearchCase::IgnoreCase))
		{
			RouteModeAssetPath = TEXT("/Game/FactoryGame/Buildable/Factory/-Shared/BuildGunModes/BuildMode_Default.BuildMode_Default_C");
		}
		else
		{
			CleanupScratch(BeltHologram, DecoyPawn, DecoyController, NewLocalPlayer);
			OnComplete(FAIModOperationResult::Failure(TEXT("INVALID_ROUTE_MODE"), FString::Printf(TEXT("'%s' is not one of \"Straight\", \"Curve\", \"Auto\""), *RouteMode)));
			return;
		}

		UClass* RouteModeClass = LoadObject<UClass>(nullptr, *RouteModeAssetPath);
		if (!RouteModeClass || !RouteModeClass->IsChildOf(UFGHologramBuildModeDescriptor::StaticClass()))
		{
			CleanupScratch(BeltHologram, DecoyPawn, DecoyController, NewLocalPlayer);
			OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), FString::Printf(TEXT("Failed to resolve '%s' as a UFGHologramBuildModeDescriptor"), *RouteModeAssetPath)));
			return;
		}

		BeltHologram->SetBuildModeOverride(TSubclassOf<UFGHologramBuildModeDescriptor>(RouteModeClass));
		UE_LOG(LogAIModAI, Display, TEXT("ConstructConveyorBelt: applied RouteMode='%s' (%s)"), *RouteMode, *RouteModeAssetPath);
	}

	// Using FVector::UpVector for Normal/ImpactNormal here (as an
	// arbitrary placeholder) produces an
	// "Invalid Conveyor Belt shape! (hard)" CanConstruct() failure even
	// though both endpoints snap cleanly (stepComplete/connectedCount
	// look correct) - the spline's arrive/leave tangent is evidently
	// derived from the hit normal, so an UpVector normal on a
	// horizontally-facing connector produces a degenerate tangent.
	// UFGFactoryConnectionComponent::GetConnectorNormal() (GetComponentRotation().Vector())
	// is the connector's real outward-facing direction - use that instead.
	auto MakeHitAt = [](AFGBuildable* Buildable, UFGFactoryConnectionComponent* Connection) -> FHitResult
	{
		FHitResult Hit;
		Hit.Location = Connection->GetConnectorLocation();
		Hit.ImpactPoint = Hit.Location;
		Hit.Normal = Connection->GetConnectorNormal();
		Hit.ImpactNormal = Hit.Normal;
		Hit.HitObjectHandle = FActorInstanceHandle(Buildable);
		Hit.bBlockingHit = true;
		// See PopulateSyntheticTraceRay's
		// doc comment (load-bearing for the lift's height; suspected fix for
		// the belt player-distance "too long" failures).
		PopulateSyntheticTraceRay(Hit);
		return Hit;
	};

	// Diagnostic evidence-gathering: the "Invalid aim
	// location! (hard)"/"Invalid Conveyor Belt shape! (hard)"
	// disqualifiers can flip depending solely on
	// Hit.Normal, at fixed player/buildable positions - log everything
	// relevant to correlate. This block never changes behavior, only logs.
	auto SummarizeDisqualifiers = [](AFGConveyorBeltHologram* H) -> FString
	{
		TArray<TSubclassOf<UFGConstructDisqualifier>> Disqualifiers;
		H->GetConstructDisqualifiers(Disqualifiers);
		TArray<FString> Texts;
		for (const TSubclassOf<UFGConstructDisqualifier>& D : Disqualifiers)
		{
			Texts.Add(FString::Printf(TEXT("%s (%s)"), *UFGConstructDisqualifier::GetDisqualifyingText(D).ToString(),
				UFGConstructDisqualifier::GetIsSoftDisqualifier(D) ? TEXT("soft") : TEXT("hard")));
		}
		return Texts.IsEmpty() ? TEXT("<none>") : FString::Join(Texts, TEXT("; "));
	};
	UE_LOG(LogAIModAI, Display, TEXT("ConstructConveyorBelt diagnostic: playerLoc=%s playerRot=%s sourceConnectorLoc=%s sourceConnectorNormal=%s sourceConnectorClearanceLoc=%s destConnectorLoc=%s destConnectorNormal=%s destConnectorClearanceLoc=%s"),
		*Character->GetActorLocation().ToString(), *Character->GetActorRotation().ToString(),
		*SourceConnection->GetConnectorLocation().ToString(), *SourceConnection->GetConnectorNormal().ToString(), *SourceConnection->GetConnectorLocation(true).ToString(),
		*DestConnection->GetConnectorLocation().ToString(), *DestConnection->GetConnectorNormal().ToString(), *DestConnection->GetConnectorLocation(true).ToString());

	// Player-independence: the underlying dependency
	// (see the function's top comment) is real and still needs a
	// deterministic rotation hint - but it now targets the DECOY
	// controller instead of the real Character's, so the real player's
	// camera never moves. Reasserted every poll tick below for the same
	// "state re-derived each tick" reason as before, just on DecoyController.
	const FRotator BeltDeterministicLook = (DestConnection->GetConnectorLocation() - SourceConnection->GetConnectorLocation()).Rotation();
	DecoyController->SetControlRotation(BeltDeterministicLook);

	// Step 1 of the flow (see DebugCheckConveyorSnap) - fix the
	// start point on the source's Output connection. UpdateHologramPlacement()
	// before TrySnapToActor() is not optional: DebugCheckConveyorSnap's
	// successful trace calls both, and omitting it here reproduces a
	// "Invalid aim location! (hard)" CanConstruct() failure even
	// though the snap/step/connectedCount indicators all look correct -
	// evidently CanConstruct()'s aim-location disqualifier reads state
	// that only UpdateHologramPlacement() sets, not TrySnapToActor() alone.
	const FHitResult StartHit = MakeHitAt(SourceBuildable, SourceConnection);
	BeltHologram->UpdateHologramPlacement(StartHit);
	BeltHologram->TrySnapToActor(StartHit);
	const bool bStartStepComplete = BeltHologram->DoMultiStepPlacement(true);
	const ESplineHologramBuildStep StepAfterStart = BeltHologram->GetCurrentBuildStep();

	UE_LOG(LogAIModAI, Display, TEXT("ConstructConveyorBelt: source=%s dest=%s after start click: stepComplete=%s step=%d disqualifiers=[%s]"),
		*SourceBuildableId, *DestBuildableId, bStartStepComplete ? TEXT("true") : TEXT("false"), static_cast<int32>(StepAfterStart), *SummarizeDisqualifiers(BeltHologram));

	if (bStartStepComplete)
	{
		// Unexpected - a two-endpoint belt shouldn't complete on the
		// first click. Report exactly what happened rather than
		// guessing further; do not proceed to a second click on an
		// already-"complete" hologram.
		CleanupScratch(BeltHologram, DecoyPawn, DecoyController, NewLocalPlayer);
		OnComplete(FAIModOperationResult::Failure(TEXT("UNEXPECTED_STEP_COMPLETE"), TEXT("DoMultiStepPlacement() reported complete after only the start click")));
		return;
	}

	// Step 2 - the destination's free Input connection. Same
	// UpdateHologramPlacement()-before-TrySnapToActor() requirement as
	// step 1 above.
	const FHitResult EndHit = MakeHitAt(DestBuildable, DestConnection);
	BeltHologram->UpdateHologramPlacement(EndHit);
	BeltHologram->TrySnapToActor(EndHit);
	const bool bEndStepComplete = BeltHologram->DoMultiStepPlacement(true);
	const ESplineHologramBuildStep StepAfterEnd = BeltHologram->GetCurrentBuildStep();
	const TArray<AFGBuildable*> ConnectedBuildables = BeltHologram->GetAnyConnectedBuildables();

	UE_LOG(LogAIModAI, Display, TEXT("ConstructConveyorBelt: source=%s dest=%s after end click: stepComplete=%s step=%d connectedCount=%d disqualifiers=[%s]"),
		*SourceBuildableId, *DestBuildableId, bEndStepComplete ? TEXT("true") : TEXT("false"), static_cast<int32>(StepAfterEnd), ConnectedBuildables.Num(), *SummarizeDisqualifiers(BeltHologram));

	if (!bEndStepComplete)
	{
		CleanupScratch(BeltHologram, DecoyPawn, DecoyController, NewLocalPlayer);
		OnComplete(FAIModOperationResult::Failure(TEXT("PLACEMENT_INCOMPLETE"),
			FString::Printf(TEXT("DoMultiStepPlacement() did not report complete after the end click - step=%d connectedCount=%d, may need a third step"), static_cast<int32>(StepAfterEnd), ConnectedBuildables.Num())));
		return;
	}

	struct FPollState
	{
		TWeakObjectPtr<AFGConveyorBeltHologram> Hologram;
		TWeakObjectPtr<APawn> DecoyPawn;
		TWeakObjectPtr<AController> DecoyController;
		TWeakObjectPtr<ULocalPlayer> NewLocalPlayer;
		TWeakObjectPtr<UGameInstance> GameInstance;
		bool bUseLocalPlayer = false;
		TWeakObjectPtr<AFGCharacterPlayer> Character;
		TWeakObjectPtr<UFGInventoryComponent> PlayerInventory;
		TArray<FItemAmount> Ingredients;
		TWeakObjectPtr<UWorld> World;
		FString SourceBuildableId;
		FString DestBuildableId;
		bool bDryRun = true;
		FRotator DeterministicLook;
		FHitResult EndHit;
		int32 AttemptsRemaining = 120; // safety cap - real ticks, not a fixed duration
		int32 AttemptsTaken = 0;
		TFunction<void(const FAIModOperationResult&)> OnComplete;
	};
	const TSharedRef<FPollState> PollState = MakeShared<FPollState>();
	PollState->Hologram = BeltHologram;
	PollState->DecoyPawn = DecoyPawn;
	PollState->DecoyController = DecoyController;
	PollState->NewLocalPlayer = NewLocalPlayer;
	PollState->GameInstance = CleanupGameInstance;
	PollState->bUseLocalPlayer = bUseLocalPlayer;
	PollState->Character = Character;
	PollState->PlayerInventory = PlayerInventory;
	PollState->Ingredients = BeltIngredients;
	PollState->World = World;
	PollState->SourceBuildableId = SourceBuildableId;
	PollState->DestBuildableId = DestBuildableId;
	PollState->bDryRun = bDryRun;
	PollState->DeterministicLook = BeltDeterministicLook;
	PollState->EndHit = EndHit;
	PollState->OnComplete = MoveTemp(OnComplete);

	auto PollCleanup = [](const TSharedRef<FPollState>& S)
	{
		if (AFGConveyorBeltHologram* H = S->Hologram.Get()) { H->Destroy(); }
		if (S->bUseLocalPlayer)
		{
			if (ULocalPlayer* LP = S->NewLocalPlayer.Get())
			{
				if (UGameInstance* GI = S->GameInstance.Get()) { GI->RemoveLocalPlayer(LP); }
			}
		}
		else
		{
			if (AController* C = S->DecoyController.Get()) { C->Destroy(); }
			if (APawn* P = S->DecoyPawn.Get()) { P->Destroy(); }
		}
	};

	const TSharedRef<TFunction<void()>> PollFn = MakeShared<TFunction<void()>>();
	*PollFn = [PollState, PollFn, PollCleanup]()
	{
		++PollState->AttemptsTaken;

		AFGConveyorBeltHologram* PollHologram = PollState->Hologram.Get();
		UWorld* PollWorld = PollState->World.Get();
		AController* PollDecoyController = PollState->DecoyController.Get();
		if (!IsValid(PollHologram) || !PollWorld)
		{
			UE_LOG(LogAIModAI, Warning, TEXT("ConstructConveyorBelt (deferred): hologram or world became invalid while polling (after %d tick(s))"), PollState->AttemptsTaken);
			PollCleanup(PollState);
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("HOLOGRAM_INVALIDATED"), TEXT("Hologram or world became invalid while polling")));
			return;
		}

		// Re-assert every tick, not just once before the poll started -
		// AutoRouteSpline()/UpdateHologramPlacement() (stub source) re-reads
		// the controller's CURRENT rotation each tick. Targets the DECOY
		// controller only - see this function's top comment.
		if (IsValid(PollDecoyController))
		{
			PollDecoyController->SetControlRotation(PollState->DeterministicLook);
		}

		// UFGCDInitializing never clears on its own even with
		// SetActorTickEnabled(true) - it stays present for the full 120-tick
		// poll window and the call fails
		// CANNOT_CONSTRUCT "Initializing (hard)". The real BuildGun-driven
		// flow calls UpdateHologramPlacement() continuously every frame
		// while the player aims, not just once per click - reassert it
		// here too, same "state re-derived each tick" shape as the
		// rotation reassert above, in case whatever UFGCDInitializing
		// gates only advances in response to a fresh placement update.
		PollHologram->UpdateHologramPlacement(PollState->EndHit);

		TArray<TSubclassOf<UFGConstructDisqualifier>> Disqualifiers;
		PollHologram->GetConstructDisqualifiers(Disqualifiers);
		const bool bStillInitializing = Disqualifiers.Contains(TSubclassOf<UFGConstructDisqualifier>(UFGCDInitializing::StaticClass()));

		--PollState->AttemptsRemaining;
		if (bStillInitializing && PollState->AttemptsRemaining > 0)
		{
			PollWorld->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
			return;
		}

		// Player-independence: don't
		// use the real (stub-source, opaque) CanConstruct() here - it has
		// no way to selectively ignore a disqualifier. Belts/lifts are
		// always built between two EXPLICIT existing buildables (never
		// player-relative), so there is no legitimate reason a fixed
		// source->dest connection should ever depend on where the player
		// happens to be standing or looking. Always ignore
		// UFGCDInvalidAimLocation here, same "any other hard disqualifier
		// blocks" rule as ConstructBuildingAtPosition's own manual
		// disqualifier loop.
		//
		// UFGCDUnaffordable: ALWAYS ignored, not
		// just under the UnlimitedResources setting - the construction
		// instigator is the decoy pawn (see top comment), which has no
		// inventory of its own, so this disqualifier would otherwise fire
		// unconditionally regardless of the real player's actual
		// inventory. Affordability against the REAL player's inventory is
		// verified up front (see BeltIngredients above) and charged
		// explicitly right before Construct() below - this disqualifier
		// genuinely has nothing meaningful left to check here.
		bool bCanConstruct = true;
		TArray<FString> DisqualifierTexts;
		for (const TSubclassOf<UFGConstructDisqualifier>& DisqualifierClass : Disqualifiers)
		{
			const bool bIgnoredForPlayerIndependence = (DisqualifierClass == UFGCDInvalidAimLocation::StaticClass())
				|| (DisqualifierClass == UFGCDEncroachingPlayer::StaticClass()) // see the identical addition in the RealCharacter strategy
				|| (DisqualifierClass == UFGCDUnaffordable::StaticClass());
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

		UE_LOG(LogAIModAI, Display, TEXT("ConstructConveyorBelt (deferred, resolved after %d real tick(s)): source=%s dest=%s dryRun=%s canConstruct=%s disqualifiers=[%s]"),
			PollState->AttemptsTaken, *PollState->SourceBuildableId, *PollState->DestBuildableId, PollState->bDryRun ? TEXT("true") : TEXT("false"),
			bCanConstruct ? TEXT("true") : TEXT("false"), *DisqualifierSummary);

		if (!bCanConstruct)
		{
			PollCleanup(PollState);
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("CANNOT_CONSTRUCT"), DisqualifierSummary));
			return;
		}

		if (PollState->bDryRun)
		{
			// Capture the hologram's predicted spline BEFORE cleanup destroys it,
			// so world.testConveyorBelt returns the path the belt would take
			// (result.detail.points) - lets a caller verify long-span routing
			// against obstacles without building. See SerializeHologramSplineJson.
			const FString SplineJson = SerializeHologramSplineJson(PollHologram);
			PollCleanup(PollState);
			FAIModOperationResult DryResult = FAIModOperationResult::Success();
			DryResult.ResultDetailJson = SplineJson;
			PollState->OnComplete(DryResult);
			return;
		}

		// Re-verify affordability right before charging - real time has
		// passed since the up-front check (other calls may have spent the
		// same materials in the meantime). Still never partially consumes.
		UFGInventoryComponent* PollInventory = PollState->PlayerInventory.Get();
		if (!PollInventory)
		{
			PollCleanup(PollState);
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("Player inventory became invalid before constructing")));
			return;
		}
		TArray<FString> ShortfallDescriptions;
		for (const FItemAmount& Ingredient : PollState->Ingredients)
		{
			if (!Ingredient.ItemClass || !PollInventory->HasItems(Ingredient.ItemClass, Ingredient.Amount))
			{
				const int32 Have = Ingredient.ItemClass ? PollInventory->GetNumItems(Ingredient.ItemClass) : 0;
				ShortfallDescriptions.Add(FString::Printf(TEXT("%s (need %d, have %d)"),
					Ingredient.ItemClass ? *Ingredient.ItemClass->GetName() : TEXT("<null>"), Ingredient.Amount, Have));
			}
		}
		if (!ShortfallDescriptions.IsEmpty())
		{
			PollCleanup(PollState);
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("INSUFFICIENT_INGREDIENTS"),
				FString::Printf(TEXT("Missing: %s"), *FString::Join(ShortfallDescriptions, TEXT("; ")))));
			return;
		}

		AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(PollWorld);
		const FNetConstructionID ConstructionID = BuildableSubsystem ? BuildableSubsystem->GetNewNetConstructionID() : FNetConstructionID();

		TArray<AActor*> OutChildren;
		PollHologram->Construct(OutChildren, ConstructionID);

		UE_LOG(LogAIModAI, Display, TEXT("ConstructConveyorBelt (deferred, resolved after %d real tick(s)): construction attempted via Hologram->Construct() - source=%s dest=%s children=%d"),
			PollState->AttemptsTaken, *PollState->SourceBuildableId, *PollState->DestBuildableId, OutChildren.Num());

		for (const FItemAmount& Ingredient : PollState->Ingredients)
		{
			PollInventory->Remove(Ingredient.ItemClass, Ingredient.Amount);
		}

		PollCleanup(PollState);
		PollState->OnComplete(FAIModOperationResult::Success());
	};

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
}


// LogSplineGeometryAsJson - see header doc comment for the
// full rationale (diagnoses world.connectConveyor's unpredictable
// curving by comparing its output against a normally-placed belt's real
// geometry). "found"/"isSplineBuildable" embedded in the payload, not a
// thrown RPC error - same convention as LogGroundHeightAsJson's "found".
FString UAIModFunctionLibrary::LogSplineGeometryAsJson(UObject* WorldContextObject, const FString& BuildableId)
{
	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetStringField(TEXT("buildableId"), BuildableId);
	RootObject->SetBoolField(TEXT("found"), false);
	RootObject->SetBoolField(TEXT("isSplineBuildable"), false);

	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	AFGBuildable* Buildable = World ? FindBuildableById(World, BuildableId) : nullptr;

	if (Buildable)
	{
		RootObject->SetBoolField(TEXT("found"), true);
		RootObject->SetStringField(TEXT("buildableClass"), Buildable->GetClass()->GetPathName());

		if (IFGSplineBuildableInterface* SplineBuildable = Cast<IFGSplineBuildableInterface>(Buildable))
		{
			RootObject->SetBoolField(TEXT("isSplineBuildable"), true);
			RootObject->SetNumberField(TEXT("meshLength"), SplineBuildable->GetMeshLength());

			TArray<TSharedPtr<FJsonValue>> PointsJsonArray;
			if (USplineComponent* Spline = SplineBuildable->GetSplineComponent())
			{
				RootObject->SetNumberField(TEXT("splineLength"), Spline->GetSplineLength());
				const int32 NumPoints = Spline->GetNumberOfSplinePoints();
				PointsJsonArray.Reserve(NumPoints);
				for (int32 i = 0; i < NumPoints; ++i)
				{
					const FVector Location = Spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World);
					const FVector Tangent = Spline->GetTangentAtSplinePoint(i, ESplineCoordinateSpace::World);

					const TSharedRef<FJsonObject> PointObject = MakeShared<FJsonObject>();
					const TSharedRef<FJsonObject> LocationObject = MakeShared<FJsonObject>();
					LocationObject->SetNumberField(TEXT("x"), Location.X);
					LocationObject->SetNumberField(TEXT("y"), Location.Y);
					LocationObject->SetNumberField(TEXT("z"), Location.Z);
					PointObject->SetObjectField(TEXT("location"), LocationObject);

					const TSharedRef<FJsonObject> TangentObject = MakeShared<FJsonObject>();
					TangentObject->SetNumberField(TEXT("x"), Tangent.X);
					TangentObject->SetNumberField(TEXT("y"), Tangent.Y);
					TangentObject->SetNumberField(TEXT("z"), Tangent.Z);
					PointObject->SetObjectField(TEXT("tangent"), TangentObject);

					PointsJsonArray.Add(MakeShared<FJsonValueObject>(PointObject));
				}
			}
			else
			{
				UE_LOG(LogAIModAI, Warning, TEXT("LogSplineGeometryAsJson: '%s' implements IFGSplineBuildableInterface but GetSplineComponent() returned null"), *BuildableId);
			}
			RootObject->SetArrayField(TEXT("points"), PointsJsonArray);
		}
	}

	FString JsonString;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonString);
	FJsonSerializer::Serialize(RootObject, Writer);

	UE_LOG(LogAIModAI, Display, TEXT("LogSplineGeometryAsJson: %s"), *JsonString);

	return JsonString;
}


// LogConveyorLiftTiersAsJson - mirrors LogConveyorBeltTiersAsJson's
// structure. Recipe_ConveyorLiftMk1..Mk6 (all six present on
// disk, same "Mk1..Mk6" naming as belts) resolve to AFGBuildableConveyorLift
// (AFGBuildableConveyorBase's OTHER direct subclass alongside regular
// belts - confirmed from source, shares GetSpeed()/GetConnection0()/
// GetConnection1()). Deliberately does NOT report min/max height limits:
// AFGConveyorLiftHologram's mStepHeight/mMinimumHeight/mMaximumHeight/
// mMinimumHeightWithVerticalConnection are plain private float members
// with NO UPROPERTY macro (confirmed from header) - unlike every other
// reflection-based CDO read in this file (belts' mMaxIncline, pipes'
// mMaxSplineLength/etc.), FindFProperty<FFloatProperty> cannot find a
// non-UPROPERTY field at all, since UHT never generates reflection data
// for it. This is a genuine, real gap (not yet solved) rather than an
// omission - real height limits remain unknown until discovered another
// way (e.g. live binary-search construction attempts).
FString UAIModFunctionLibrary::LogConveyorLiftTiersAsJson(UObject* WorldContextObject)
{
	static const TCHAR* RecipePaths[] = {
		TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorLiftMk1.Recipe_ConveyorLiftMk1_C"),
		TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorLiftMk2.Recipe_ConveyorLiftMk2_C"),
		TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorLiftMk3.Recipe_ConveyorLiftMk3_C"),
		TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorLiftMk4.Recipe_ConveyorLiftMk4_C"),
		TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorLiftMk5.Recipe_ConveyorLiftMk5_C"),
		TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorLiftMk6.Recipe_ConveyorLiftMk6_C"),
	};

	TArray<TSharedPtr<FJsonValue>> TierJsonArray;
	for (const TCHAR* RecipePath : RecipePaths)
	{
		const TSubclassOf<AFGBuildable> BuildableClass = ResolveBuildableClassForRecipe(RecipePath);
		const AFGBuildableConveyorLift* LiftCDO = BuildableClass ? Cast<AFGBuildableConveyorLift>(BuildableClass->GetDefaultObject()) : nullptr;
		if (!LiftCDO)
		{
			UE_LOG(LogAIModAI, Warning, TEXT("LogConveyorLiftTiersAsJson: could not resolve a AFGBuildableConveyorLift CDO for '%s' - omitting"), RecipePath);
			continue;
		}

		const TSharedRef<FJsonObject> TierObject = MakeShared<FJsonObject>();
		TierObject->SetStringField(TEXT("recipeClass"), RecipePath);
		TierObject->SetStringField(TEXT("buildableClass"), BuildableClass->GetPathName());
		// speed: AFGBuildableConveyorBase::GetSpeed(), same unit-ambiguity
		// caveat as belts (see LogConveyorBeltTiersAsJson) - "Speed of
		// this conveyor", no documented unit.
		TierObject->SetNumberField(TEXT("speed"), LiftCDO->GetSpeed());

		TierJsonArray.Add(MakeShared<FJsonValueObject>(TierObject));
	}

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("protocolVersion"), 1);
	RootObject->SetArrayField(TEXT("tiers"), TierJsonArray);

	FString JsonString;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonString);
	FJsonSerializer::Serialize(RootObject, Writer);

	UE_LOG(LogAIModAI, Display, TEXT("LogConveyorLiftTiersAsJson: %s"), *JsonString);

	return JsonString;
}


// ConstructConveyorLift - vertical conveyors, used to transition from
// terrain-locked miners up to raised foundations. Deliberate
// near-mirror of ConstructConveyorBelt's two-click TrySnapToActor flow -
// AFGConveyorLiftHologram is NOT a spline hologram (confirmed from
// source: AFGConveyorLiftHologram : AFGBuildableHologram directly, NOT
// AFGSplineHologram like belts/pipes - a vertical lift is a straight
// column, no bending), but it DOES override TrySnapToActor/
// DoMultiStepPlacement itself, so the same click-driven pattern applies:
// UpdateHologramPlacement() before each TrySnapToActor(), the connector's
// real GetConnectorNormal() (not a placeholder) in the synthetic hit.
// Reuses FindFreeFactoryConnection/UFGFactoryConnectionComponent -
// AFGBuildableConveyorLift shares the exact same connection component
// type as regular belts (both derive from AFGBuildableConveyorBase).
// No RouteMode param - lifts are a fixed vertical column, no bend/curve
// concept applies.
//
// CONNECTOR-FINDING (see FindFreeFactoryConnection/
// FindFreeFactoryConnectionNear's doc comments above): a wall/pole's
// FCD_SNAP_ONLY connector is a real, valid attachment point ("special
// case for conveyor poles" per FGFactoryConnectionComponent.h) but is not
// an exact Input/Output direction match, so both finders fall back to
// a free SnapOnly connector when no exact-direction match exists.
// IsConnected() is documented to always read false for SnapOnly
// regardless of real attachment state - not a bug, don't use it to judge
// whether a wall/pole slot is free.
//
// HEIGHT: the top step of a lift ignores Hit.Location and derives its
// height from the trace RAY, not the hit point - which matches real
// gameplay: you place the bottom by pointing AT a thing, but you set the
// height by aiming INTO THE AIR, where there is often no blocking hit at
// all. A synthetic hit that leaves FHitResult::TraceStart/TraceEnd at
// zero-vectors is a degenerate ray, which clamps the lift to its 400-unit
// minimum height every time. So the synthetic hits here MUST populate a
// real trace ray (PopulateSyntheticTraceRay): the end click gets a
// HORIZONTAL ray at exactly the dest connector's Z through the connector
// toward the lift column, chosen so every plausible ray-based height
// computation (ray-vs-axis closest point, ray-vs-plane intersection,
// blocking-hit location) agrees on the dest height. The end click is also
// deferred ONE REAL TICK with the deterministic look re-asserted first, so
// the live camera POV (PlayerCameraManager lags SetControlRotation by a
// frame) is current by click time.
//
// Validate test placements for connector FACING, not just position: a
// lift arm docks 300 units along the dest connector's facing normal
// (column at destConnectorLoc + 300 * destNormal, opposite-facing
// normals, exactly 300 apart on the working reference lift), so a dest
// connector whose normal faces AWAY from the lift column can never fully
// dock regardless of height logic.
//
// A lift travels straight up/down only, X/Y locked to SourceConnection's
// real position - if the real destination isn't directly above/below the
// source, a separate ConstructConveyorBelt call is still needed to bridge
// the horizontal gap.
//
// UNEXPLAINED (not root-caused): a duplicate Storage Container (identical
// class, identical position to the real source) has been seen in
// world.buildables after a connectConveyorLift call, stable across
// repeated queries, never explicitly constructed. Might be this function's
// own snap/connector logic, might be unrelated - genuinely unknown.
void UAIModFunctionLibrary::ConstructConveyorLift(UObject* WorldContextObject, const FString& SourceBuildableId, const FString& DestBuildableId, const FString& RecipeClassPath, int32 FreeEndRotationSteps, const TOptional<FVector>& SourceConnectorPosition, const TOptional<FVector>& DestConnectorPosition, bool bDryRun, TFunction<void(const FAIModOperationResult&)> OnComplete)
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

	AFGBuildable* SourceBuildable = FindBuildableById(World, SourceBuildableId);
	if (!SourceBuildable)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No buildable found with id '%s'"), *SourceBuildableId)));
		return;
	}
	AFGBuildable* DestBuildable = FindBuildableById(World, DestBuildableId);
	if (!DestBuildable)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No buildable found with id '%s'"), *DestBuildableId)));
		return;
	}

	// Connector pinning: picking "first free output/input" on a stacked
	// splitter/merger riser can choose non-coaxial SIDE connectors and build
	// a lift whose top lands nowhere near the dest. Belts accept
	// sourceConnectorPosition/destConnectorPosition for exactly this; lifts
	// do too, so a caller can force the specific connectors whose X/Y line
	// up for a clean vertical column. Falls back to first-free behavior when
	// no position is supplied.
	UFGFactoryConnectionComponent* SourceConnection = SourceConnectorPosition.IsSet()
		? FindFreeFactoryConnectionNear(SourceBuildable, EFactoryConnectionDirection::FCD_OUTPUT, SourceConnectorPosition.GetValue())
		: FindFreeFactoryConnection(SourceBuildable, EFactoryConnectionDirection::FCD_OUTPUT);
	if (!SourceConnection)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_FACTORY_CONNECTION"),
			SourceConnectorPosition.IsSet()
				? FString::Printf(TEXT("'%s' has no free Output factory connection within tolerance of the requested position %s"), *SourceBuildableId, *SourceConnectorPosition.GetValue().ToString())
				: FString::Printf(TEXT("'%s' has no free Output factory connection component"), *SourceBuildableId)));
		return;
	}
	UFGFactoryConnectionComponent* DestConnection = DestConnectorPosition.IsSet()
		? FindFreeFactoryConnectionNear(DestBuildable, EFactoryConnectionDirection::FCD_INPUT, DestConnectorPosition.GetValue())
		: FindFreeFactoryConnection(DestBuildable, EFactoryConnectionDirection::FCD_INPUT);
	if (!DestConnection)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_FACTORY_CONNECTION"),
			DestConnectorPosition.IsSet()
				? FString::Printf(TEXT("'%s' has no free Input factory connection within tolerance of the requested position %s"), *DestBuildableId, *DestConnectorPosition.GetValue().ToString())
				: FString::Printf(TEXT("'%s' has no free Input factory connection component"), *DestBuildableId)));
		return;
	}

	UClass* LiftRecipeClass = LoadObject<UClass>(nullptr, *RecipeClassPath);
	if (!LiftRecipeClass || !LiftRecipeClass->IsChildOf(UFGRecipe::StaticClass()))
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("INVALID_RECIPE"), FString::Printf(TEXT("'%s' did not resolve to a UFGRecipe subclass"), *RecipeClassPath)));
		return;
	}
	const TSubclassOf<UFGRecipe> RecipeClass = LiftRecipeClass;

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
	AFGConveyorLiftHologram* LiftHologram = Cast<AFGConveyorLiftHologram>(Hologram);
	if (!LiftHologram)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("HOLOGRAM_SPAWN_FAILED"),
			FString::Printf(TEXT("HotKeyRecipe(%s) did not result in an AFGConveyorLiftHologram (got %s)"),
				*RecipeClassPath, Hologram ? *Hologram->GetClass()->GetName() : TEXT("null"))));
		return;
	}

	// Sets Hit.Component to a real UPrimitiveComponent (matches what a real
	// trace would populate).
	//
	// Also populates TraceStart/TraceEnd/Distance/Time - the camera ray
	// fields a real build-gun trace ALWAYS carries and that a bare synthetic
	// hit leaves at zero-vectors. See this function's doc comment (HEIGHT)
	// for why the free end's height is computed from this ray (a real player
	// raises the top by aiming INTO THE AIR, where there is no blocking hit
	// at all - the view ray is the only geometric data that can drive height
	// in that case); a degenerate ray clamps it to the 400-unit minimum.
	auto MakeHitAt = [](AFGBuildable* Buildable, UFGFactoryConnectionComponent* Connection, const FVector& TraceStart, const FVector& TraceEnd) -> FHitResult
	{
		FHitResult Hit;
		Hit.Location = Connection->GetConnectorLocation();
		Hit.ImpactPoint = Hit.Location;
		Hit.Normal = Connection->GetConnectorNormal();
		Hit.ImpactNormal = Hit.Normal;
		Hit.HitObjectHandle = FActorInstanceHandle(Buildable);
		Hit.bBlockingHit = true;
		if (UPrimitiveComponent* RealPrimitive = Buildable->FindComponentByClass<UPrimitiveComponent>())
		{
			Hit.Component = RealPrimitive;
		}
		Hit.TraceStart = TraceStart;
		Hit.TraceEnd = TraceEnd;
		Hit.Distance = static_cast<float>(FVector::Dist(TraceStart, Hit.Location));
		const float TraceLength = static_cast<float>(FVector::Dist(TraceStart, TraceEnd));
		Hit.Time = TraceLength > KINDA_SMALL_NUMBER ? Hit.Distance / TraceLength : 0.0f;
		return Hit;
	};

	auto SummarizeDisqualifiers = [](AFGConveyorLiftHologram* H) -> FString
	{
		TArray<TSubclassOf<UFGConstructDisqualifier>> Disqualifiers;
		H->GetConstructDisqualifiers(Disqualifiers);
		TArray<FString> Texts;
		for (const TSubclassOf<UFGConstructDisqualifier>& D : Disqualifiers)
		{
			Texts.Add(FString::Printf(TEXT("%s (%s)"), *UFGConstructDisqualifier::GetDisqualifyingText(D).ToString(),
				UFGConstructDisqualifier::GetIsSoftDisqualifier(D) ? TEXT("soft") : TEXT("hard")));
		}
		return Texts.IsEmpty() ? TEXT("<none>") : FString::Join(Texts, TEXT("; "));
	};

	UE_LOG(LogAIModAI, Display, TEXT("ConstructConveyorLift diagnostic: sourceConnectorLoc=%s sourceConnectorNormal=%s destConnectorLoc=%s destConnectorNormal=%s"),
		*SourceConnection->GetConnectorLocation().ToString(), *SourceConnection->GetConnectorNormal().ToString(),
		*DestConnection->GetConnectorLocation().ToString(), *DestConnection->GetConnectorNormal().ToString());

	// Player-independence rotation: a deterministic look direction is needed
	// for player-independence, matching ConstructConveyorBelt's identical
	// pattern. Computed from the real CAMERA location
	// (AFGCharacterPlayer::GetCameraComponentWorldLocation(), the actual
	// first-person camera position, offset upward from the capsule root by
	// roughly eye height), NOT Character->GetActorLocation() (the capsule
	// root): any camera-position-based placement logic reading the capsule
	// root instead of the real eye position bakes in a systematic vertical
	// error.
	const FRotator LiftDeterministicLook = (DestConnection->GetConnectorLocation() - Character->GetCameraComponentWorldLocation()).Rotation();
	if (AController* LiftController = Character->GetController())
	{
		LiftController->SetControlRotation(LiftDeterministicLook);
	}

	// The start click gets a realistic camera->connector ray (the bottom
	// placement works from Hit.Location alone, so this is completeness, not
	// the fix itself - the height fix targets the END click below).
	const FVector StartConnectorLoc = SourceConnection->GetConnectorLocation();
	const FVector StartTraceStart = Character->GetCameraComponentWorldLocation();
	const FVector StartTraceEnd = StartConnectorLoc + (StartConnectorLoc - StartTraceStart).GetSafeNormal() * 1000.0f;
	const FHitResult StartHit = MakeHitAt(SourceBuildable, SourceConnection, StartTraceStart, StartTraceEnd);
	const bool bStartHitValid = LiftHologram->IsValidHitResult(StartHit);
	LiftHologram->UpdateHologramPlacement(StartHit);
	// UpdateHologramPlacement() is documented to already call
	// TrySnapToActor()/SetHologramLocationAndRotation() internally with
	// whatever hit it's given - the explicit, SEPARATE TrySnapToActor()
	// call below (kept for its own return value, used in the diagnostic
	// log) may be redundant, and if a failed TrySnapToActor() resets
	// height/transform state, this redundant second call could undo a
	// correct height UpdateHologramPlacement() just computed internally.
	// Log height BEFORE the explicit call to check.
	UE_LOG(LogAIModAI, Display, TEXT("ConstructConveyorLift: height immediately after UpdateHologramPlacement(StartHit), before explicit TrySnapToActor=%.1f"), LiftHologram->GetHeight());
	// Also inject the same hit into the build gun's own cached trace, in
	// case height is read from there rather than from whatever's passed into
	// UpdateHologramPlacement() (belt-and-braces).
	BuildGun->GetHitResult() = StartHit;
	const bool bStartSnapped = LiftHologram->TrySnapToActor(StartHit);
	// TrySnapToActor()'s doc comment names "the build gun" (not the hologram
	// itself) as whatever calls SetHologramLocationAndRotation() when
	// snapping fails - and since this function bypasses the real build gun's
	// TickState_Implementation entirely, nothing may otherwise call
	// SetHologramLocationAndRotation() in this code path at all. Call it
	// explicitly here, matching the documented precondition exactly ("only
	// be called if we have a valid hit result and did not snap").
	if (bStartHitValid && !bStartSnapped)
	{
		LiftHologram->SetHologramLocationAndRotation(StartHit);
		UE_LOG(LogAIModAI, Display, TEXT("ConstructConveyorLift: height after explicit SetHologramLocationAndRotation(StartHit)=%.1f"), LiftHologram->GetHeight());
	}
	const bool bStartStepComplete = LiftHologram->DoMultiStepPlacement(true);

	UE_LOG(LogAIModAI, Display, TEXT("ConstructConveyorLift: source=%s dest=%s after start click: hitValid=%s snapped=%s stepComplete=%s height=%.1f disqualifiers=[%s]"),
		*SourceBuildableId, *DestBuildableId, bStartHitValid ? TEXT("true") : TEXT("false"), bStartSnapped ? TEXT("true") : TEXT("false"), bStartStepComplete ? TEXT("true") : TEXT("false"), LiftHologram->GetHeight(), *SummarizeDisqualifiers(LiftHologram));

	if (bStartStepComplete)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("UNEXPECTED_STEP_COMPLETE"), TEXT("DoMultiStepPlacement() reported complete after only the start click")));
		return;
	}

	// The end click is deferred ONE REAL TICK instead of firing
	// synchronously in the same frame as the start click. Two reasons: (1)
	// if the lift's top-step height reads the LIVE camera POV
	// (PlayerCameraManager) rather than the hit, the POV only consumes
	// SetControlRotation() during its own per-frame update - a same-frame
	// synchronous click would read the STALE aim (wherever the player
	// actually looks - typically level -> minimum height); (2) it matches
	// the real click cadence (a player's two clicks are always frames
	// apart), removing a class of same-frame-state doubts. The deterministic
	// look set in phase 1 above is re-asserted inside the deferred phase
	// before the click.
	struct FLiftEndClickState
	{
		TWeakObjectPtr<AFGConveyorLiftHologram> Hologram;
		TWeakObjectPtr<AFGCharacterPlayer> Character;
		TWeakObjectPtr<AFGBuildGun> BuildGun;
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<AFGBuildable> DestBuildable;
		TWeakObjectPtr<UFGFactoryConnectionComponent> SourceConnection;
		TWeakObjectPtr<UFGFactoryConnectionComponent> DestConnection;
		FString SourceBuildableId;
		FString DestBuildableId;
		int32 FreeEndRotationSteps = 0;
		bool bDryRun = true;
		FRotator DeterministicLook;
		TFunction<void(const FAIModOperationResult&)> OnComplete;
	};
	const TSharedRef<FLiftEndClickState> EndClickState = MakeShared<FLiftEndClickState>();
	EndClickState->Hologram = LiftHologram;
	EndClickState->Character = Character;
	EndClickState->BuildGun = BuildGun;
	EndClickState->World = World;
	EndClickState->DestBuildable = DestBuildable;
	EndClickState->SourceConnection = SourceConnection;
	EndClickState->DestConnection = DestConnection;
	EndClickState->SourceBuildableId = SourceBuildableId;
	EndClickState->DestBuildableId = DestBuildableId;
	EndClickState->FreeEndRotationSteps = FreeEndRotationSteps;
	EndClickState->bDryRun = bDryRun;
	EndClickState->DeterministicLook = LiftDeterministicLook;
	EndClickState->OnComplete = MoveTemp(OnComplete);

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([EndClickState, MakeHitAt, SummarizeDisqualifiers]()
	{
		AFGConveyorLiftHologram* LiftHologram = EndClickState->Hologram.Get();
		UWorld* World = EndClickState->World.Get();
		AFGCharacterPlayer* Character = EndClickState->Character.Get();
		AFGBuildGun* BuildGun = EndClickState->BuildGun.Get();
		AFGBuildable* DestBuildable = EndClickState->DestBuildable.Get();
		UFGFactoryConnectionComponent* SourceConnection = EndClickState->SourceConnection.Get();
		UFGFactoryConnectionComponent* DestConnection = EndClickState->DestConnection.Get();
		const FString& SourceBuildableId = EndClickState->SourceBuildableId;
		const FString& DestBuildableId = EndClickState->DestBuildableId;
		const int32 FreeEndRotationSteps = EndClickState->FreeEndRotationSteps;
		const bool bDryRun = EndClickState->bDryRun;
		const FRotator LiftDeterministicLook = EndClickState->DeterministicLook;
		TFunction<void(const FAIModOperationResult&)> OnComplete = EndClickState->OnComplete;

		if (!IsValid(LiftHologram) || !World || !IsValid(Character) || !IsValid(BuildGun) || !IsValid(DestBuildable) || !IsValid(SourceConnection) || !IsValid(DestConnection))
		{
			UE_LOG(LogAIModAI, Warning, TEXT("ConstructConveyorLift (end click, deferred one tick): hologram/world/target became invalid between clicks - aborting, nothing built"));
			if (IsValid(Character)) { Character->UnequipBuildGun(); }
			OnComplete(FAIModOperationResult::Failure(TEXT("HOLOGRAM_INVALIDATED"), TEXT("Hologram or targets became invalid between the start and end clicks")));
			return;
		}

		// Re-assert the deterministic look now that a real tick has
		// elapsed - the camera manager has had its per-frame update since
		// phase 1 set it, so by this point the LIVE camera POV genuinely
		// points at the destination too (the whole purpose of the one-tick defer).
		if (AController* LiftController = Character->GetController())
		{
			LiftController->SetControlRotation(LiftDeterministicLook);
		}

		// End-click ray:
		// a HORIZONTAL synthetic camera ray at EXACTLY the destination
		// connector's Z, passing through the connector and continuing
		// toward/past the lift column. Direction: the dest connector's
		// horizontal normal - a lift's arm connector always docks 300
		// units along its facing normal (verified from real connector
		// data: lift column sits at destConnectorLoc + 300 * destNormal),
		// so +normal points from the connector toward where the column
		// will be. Chosen so that EVERY plausible ray-based height
		// computation agrees on the answer: ray-vs-vertical-axis closest
		// point Z = destZ, ray-vs-vertical-plane intersection Z = destZ,
		// and the blocking-hit Location itself is the dest connector.
		// Falls back to the dest->source horizontal direction (then a
		// fixed axis) if the dest normal is vertical.
		const FVector DestConnectorLoc = DestConnection->GetConnectorLocation();
		FVector EndRayDir = DestConnection->GetConnectorNormal();
		EndRayDir.Z = 0.0;
		if (!EndRayDir.Normalize())
		{
			EndRayDir = SourceConnection->GetConnectorLocation() - DestConnectorLoc;
			EndRayDir.Z = 0.0;
			if (!EndRayDir.Normalize())
			{
				EndRayDir = FVector::ForwardVector;
			}
		}
		const FVector EndTraceStart = DestConnectorLoc - EndRayDir * 1500.0;
		const FVector EndTraceEnd = DestConnectorLoc + EndRayDir * 1500.0;

		const FHitResult EndHit = MakeHitAt(DestBuildable, DestConnection, EndTraceStart, EndTraceEnd);
		const bool bEndHitValid = LiftHologram->IsValidHitResult(EndHit);
		LiftHologram->UpdateHologramPlacement(EndHit);
		// Log height before the explicit TrySnapToActor - same check as on StartHit above.
		UE_LOG(LogAIModAI, Display, TEXT("ConstructConveyorLift: height immediately after UpdateHologramPlacement(EndHit), before explicit TrySnapToActor=%.1f"), LiftHologram->GetHeight());
		// Also inject the hit into the build gun's cached trace - identical
		// injection to StartHit above.
		BuildGun->GetHitResult() = EndHit;
		const bool bEndSnapped = LiftHologram->TrySnapToActor(EndHit);
		// Explicit SetHologramLocationAndRotation after a failed snap - same
		// as on StartHit above.
		if (bEndHitValid && !bEndSnapped)
		{
			LiftHologram->SetHologramLocationAndRotation(EndHit);
			UE_LOG(LogAIModAI, Display, TEXT("ConstructConveyorLift: height after explicit SetHologramLocationAndRotation(EndHit)=%.1f"), LiftHologram->GetHeight());
		}

		// FreeEndRotationSteps: rotates the still-unconnected end in
		// 90-degree increments via ScrollRotate() BEFORE the final click,
		// mirroring ConstructBuildingAtPosition's Scroll()-called-N-times-per-notch
		// pattern (see its comment) - this can only be done while the
		// hologram is still being placed, which is why world.setBuildableRotation
		// fails silently on an already-built lift.
		//
		// Explicit zero-reset FIRST, unconditionally: a fresh lift's free-end
		// orientation may default to the orientation of the last-placed lift -
		// if mScrollRotation persists/carries forward across hologram
		// instances rather than resetting per-spawn, ScrollRotate() calls here
		// would land on top of an unpredictable inherited baseline, not a
		// clean zero. SetScrollRotateValue(0) forces a known starting point
		// every call, whether or not rotation was requested, so the
		// no-rotation default is deterministic too. Not yet verified at
		// runtime whether 0 is really the hologram's own unrotated baseline.
		LiftHologram->SetScrollRotateValue(0);
		if (FreeEndRotationSteps != 0)
		{
			const int32 ScrollRotateStep = FreeEndRotationSteps > 0 ? 1 : -1;
			for (int32 i = 0; i < FMath::Abs(FreeEndRotationSteps); ++i)
			{
				LiftHologram->ScrollRotate(ScrollRotateStep, 90);
			}
		}
		UE_LOG(LogAIModAI, Display, TEXT("ConstructConveyorLift: freeEndRotationSteps=%d scrollRotateValue after reset+rotate=%d"),
			FreeEndRotationSteps, LiftHologram->GetScrollRotateValue());

		const bool bEndStepComplete = LiftHologram->DoMultiStepPlacement(true);

		UE_LOG(LogAIModAI, Display, TEXT("ConstructConveyorLift: source=%s dest=%s after end click: hitValid=%s snapped=%s stepComplete=%s height=%.1f expectedHeight=%.1f disqualifiers=[%s]"),
			*SourceBuildableId, *DestBuildableId, bEndHitValid ? TEXT("true") : TEXT("false"), bEndSnapped ? TEXT("true") : TEXT("false"), bEndStepComplete ? TEXT("true") : TEXT("false"), LiftHologram->GetHeight(),
			DestConnection->GetConnectorLocation().Z - SourceConnection->GetConnectorLocation().Z, *SummarizeDisqualifiers(LiftHologram));

		if (!bEndStepComplete)
		{
			Character->UnequipBuildGun();
			OnComplete(FAIModOperationResult::Failure(TEXT("PLACEMENT_INCOMPLETE"), TEXT("DoMultiStepPlacement() did not report complete after the end click")));
			return;
		}

		struct FPollState
		{
			TWeakObjectPtr<AFGConveyorLiftHologram> Hologram;
			TWeakObjectPtr<AFGCharacterPlayer> Character;
			TWeakObjectPtr<AFGBuildGun> BuildGun; // build gun cached-trace injection, see below
			TWeakObjectPtr<UWorld> World;
			FString SourceBuildableId;
			FString DestBuildableId;
			bool bDryRun = true;
			FRotator DeterministicLook;
			FHitResult EndHit; // re-asserted every poll tick, see below
			int32 AttemptsRemaining = 120; // safety cap - real ticks, not a fixed duration
			int32 AttemptsTaken = 0;
			TFunction<void(const FAIModOperationResult&)> OnComplete;
		};
		const TSharedRef<FPollState> PollState = MakeShared<FPollState>();
		PollState->Hologram = LiftHologram;
		PollState->Character = Character;
		PollState->BuildGun = BuildGun;
		PollState->World = World;
		PollState->SourceBuildableId = SourceBuildableId;
		PollState->DestBuildableId = DestBuildableId;
		PollState->bDryRun = bDryRun;
		PollState->DeterministicLook = LiftDeterministicLook;
		PollState->EndHit = EndHit;
		PollState->OnComplete = MoveTemp(OnComplete);

		const TSharedRef<TFunction<void()>> PollFn = MakeShared<TFunction<void()>>();
		*PollFn = [PollState, PollFn]()
		{
			++PollState->AttemptsTaken;

			AFGConveyorLiftHologram* PollHologram = PollState->Hologram.Get();
			UWorld* PollWorld = PollState->World.Get();
			AFGCharacterPlayer* PollCharacter = PollState->Character.Get();
			if (!IsValid(PollHologram) || !PollWorld)
			{
				UE_LOG(LogAIModAI, Warning, TEXT("ConstructConveyorLift (deferred): hologram or world became invalid while polling (after %d tick(s))"), PollState->AttemptsTaken);
				if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
				PollState->OnComplete(FAIModOperationResult::Failure(TEXT("HOLOGRAM_INVALIDATED"), TEXT("Hologram or world became invalid while polling")));
				return;
			}

			// Re-assert every tick - see ConstructConveyorBelt's identical block
			// for the full rationale (player camera movement between ticks can
			// still drag the resolved path off a one-time value).
			if (IsValid(PollCharacter))
			{
				if (AController* PollController = PollCharacter->GetController())
				{
					PollController->SetControlRotation(PollState->DeterministicLook);
				}
			}

			// Re-assert the end hit every poll tick - same fix
			// as ConstructConveyorBelt_RealCharacterStrategy, applied here
			// for the same reason (see that function's comment for the full
			// TickState_Implementation live-camera-trace rationale).
			PollHologram->UpdateHologramPlacement(PollState->EndHit);

			// Re-assert the build gun's cached trace every tick alongside the
			// above, same rationale (belt-and-braces).
			if (AFGBuildGun* PollBuildGun = PollState->BuildGun.Get())
			{
				PollBuildGun->GetHitResult() = PollState->EndHit;
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

			UE_LOG(LogAIModAI, Display, TEXT("ConstructConveyorLift (initializing cleared): height=%.1f"), PollHologram->GetHeight());

			// Player-independence fix - same rationale as ConstructConveyorBelt's
			// identical block above.
			// UnlimitedResources - see ConstructBuildingAtPosition's
			// comment on this being a player-controlled mod setting, not a
			// per-call flag.
			const bool bUnlimitedResources = UAIModFunctionLibrary::GetAIModConfigBool(PollWorld, TEXT("UnlimitedResources"), false);

			bool bCanConstruct = true;
			TArray<FString> DisqualifierTexts;
			for (const TSubclassOf<UFGConstructDisqualifier>& DisqualifierClass : Disqualifiers)
			{
				const bool bIgnoredForPlayerIndependence = (DisqualifierClass == UFGCDInvalidAimLocation::StaticClass())
					|| (DisqualifierClass == UFGCDEncroachingPlayer::StaticClass()) // see the identical addition in ConstructConveyorBelt's RealCharacter strategy
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

			UE_LOG(LogAIModAI, Display, TEXT("ConstructConveyorLift (deferred, resolved after %d real tick(s)): source=%s dest=%s dryRun=%s canConstruct=%s disqualifiers=[%s]"),
				PollState->AttemptsTaken, *PollState->SourceBuildableId, *PollState->DestBuildableId, PollState->bDryRun ? TEXT("true") : TEXT("false"),
				bCanConstruct ? TEXT("true") : TEXT("false"), *DisqualifierSummary);

			if (!bCanConstruct)
			{
				if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
				PollState->OnComplete(FAIModOperationResult::Failure(TEXT("CANNOT_CONSTRUCT"), DisqualifierSummary));
				return;
			}

			if (PollState->bDryRun)
			{
				if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
				PollState->OnComplete(FAIModOperationResult::Success());
				return;
			}

			AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(PollWorld);
			const FNetConstructionID ConstructionID = BuildableSubsystem ? BuildableSubsystem->GetNewNetConstructionID() : FNetConstructionID();

			AFGBuildGun* PollBuildGun = IsValid(PollCharacter) ? PollCharacter->GetBuildGun() : nullptr;
			UFGBuildGunStateBuild* PollBuildState = PollBuildGun ? Cast<UFGBuildGunStateBuild>(PollBuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD)) : nullptr;
			if (!PollBuildState)
			{
				UE_LOG(LogAIModAI, Error, TEXT("ConstructConveyorLift (deferred): lost the build state before constructing - aborting, nothing built"));
				if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
				PollState->OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("Lost the build state before constructing")));
				return;
			}

			UE_LOG(LogAIModAI, Display, TEXT("ConstructConveyorLift (deferred): height right before InternalConstructHologram=%.1f"), PollHologram->GetHeight());

			PollBuildState->InternalConstructHologram(ConstructionID);

			UE_LOG(LogAIModAI, Display, TEXT("ConstructConveyorLift (deferred, resolved after %d real tick(s)): construction attempted via InternalConstructHologram - source=%s dest=%s"),
				PollState->AttemptsTaken, *PollState->SourceBuildableId, *PollState->DestBuildableId);

			if (IsValid(PollCharacter))
			{
				PollCharacter->UnequipBuildGun();
			}

			PollState->OnComplete(FAIModOperationResult::Success());
		};

		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
	}));
}


// ConstructPipe - deliberate near-exact mirror of ConstructConveyorBelt
// above, same mechanism, different types: AFGPipelineHologram is a sibling
// of AFGConveyorBeltHologram (both derive directly from AFGSplineHologram -
// per source), and UFGPipeConnectionComponentBase/EPipeConnectionType is
// pipes' own parallel connection-type hierarchy, NOT
// UFGFactoryConnectionComponent/EFactoryConnectionDirection. Applies
// every belt fix up front rather than rediscovering them -
// UpdateHologramPlacement() before TrySnapToActor() at each click, and the
// connector's REAL GetConnectorNormal() (not a placeholder UpVector) in the
// synthetic hit. Live-verified since on real pump/tank/machine pipe runs.
// Two pipe-specific caveats that remain true: (1) AFGSplineHologram (the shared
// base) has no GetAnyConnectedBuildables() - only
// AFGConveyorBeltHologram declares that method - so this uses
// IsConnectionSnapped(false) instead for the post-end-click diagnostic,
// an indicator already noted (see DebugCheckConveyorSnap's findings)
// as not fully reliable even for belts; (2) fluid type compatibility
// (UFGCDPipeFluidTypeMismatch, confirmed to exist in
// FGConstructDisqualifier.h) is NOT pre-validated here - the real
// CanConstruct() disqualifier check is trusted to catch it, same as
// every other disqualifier this function doesn't special-case.
void UAIModFunctionLibrary::ConstructPipe(UObject* WorldContextObject, const FString& SourceBuildableId, const FString& DestBuildableId, const FString& RecipeClassPath, bool bDryRun, TFunction<void(const FAIModOperationResult&)> OnComplete)
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

	AFGBuildable* SourceBuildable = FindBuildableById(World, SourceBuildableId);
	if (!SourceBuildable)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No buildable found with id '%s'"), *SourceBuildableId)));
		return;
	}
	AFGBuildable* DestBuildable = FindBuildableById(World, DestBuildableId);
	if (!DestBuildable)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No buildable found with id '%s'"), *DestBuildableId)));
		return;
	}

	UFGPipeConnectionComponentBase* SourceConnection = FindFreeFluidPipeConnection(SourceBuildable, EPipeConnectionType::PCT_PRODUCER);
	if (!SourceConnection)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_PIPE_CONNECTION"), FString::Printf(TEXT("'%s' has no free Producer or Any pipe connection component"), *SourceBuildableId)));
		return;
	}
	UFGPipeConnectionComponentBase* DestConnection = FindFreeFluidPipeConnection(DestBuildable, EPipeConnectionType::PCT_CONSUMER);
	if (!DestConnection)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_PIPE_CONNECTION"), FString::Printf(TEXT("'%s' has no free Consumer or Any pipe connection component"), *DestBuildableId)));
		return;
	}

	// Caller-chosen pipe tier - see LogPipelineTiersAsJson for the two
	// real recipes (Recipe_Pipeline, Recipe_PipelineMK2). Same
	// validation posture as every other recipe param in this file.
	UClass* PipeRecipeClass = LoadObject<UClass>(nullptr, *RecipeClassPath);
	if (!PipeRecipeClass || !PipeRecipeClass->IsChildOf(UFGRecipe::StaticClass()))
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("INVALID_RECIPE"), FString::Printf(TEXT("'%s' did not resolve to a UFGRecipe subclass"), *RecipeClassPath)));
		return;
	}
	const TSubclassOf<UFGRecipe> RecipeClass = PipeRecipeClass;

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
	AFGPipelineHologram* PipeHologram = Cast<AFGPipelineHologram>(Hologram);
	if (!PipeHologram)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("HOLOGRAM_SPAWN_FAILED"),
			FString::Printf(TEXT("HotKeyRecipe(%s) did not result in an AFGPipelineHologram (got %s)"),
				*RecipeClassPath, Hologram ? *Hologram->GetClass()->GetName() : TEXT("null"))));
		return;
	}

	auto MakeHitAt = [](AFGBuildable* Buildable, UFGPipeConnectionComponentBase* Connection) -> FHitResult
	{
		FHitResult Hit;
		Hit.Location = Connection->GetConnectorLocation();
		Hit.ImpactPoint = Hit.Location;
		Hit.Normal = Connection->GetConnectorNormal();
		Hit.ImpactNormal = Hit.Normal;
		Hit.HitObjectHandle = FActorInstanceHandle(Buildable);
		Hit.bBlockingHit = true;
		// See PopulateSyntheticTraceRay's
		// doc comment (load-bearing for the lift's height; suspected fix for
		// the belt player-distance "too long" failures).
		PopulateSyntheticTraceRay(Hit);
		return Hit;
	};

	auto SummarizeDisqualifiers = [](AFGPipelineHologram* H) -> FString
	{
		TArray<TSubclassOf<UFGConstructDisqualifier>> Disqualifiers;
		H->GetConstructDisqualifiers(Disqualifiers);
		TArray<FString> Texts;
		for (const TSubclassOf<UFGConstructDisqualifier>& D : Disqualifiers)
		{
			Texts.Add(FString::Printf(TEXT("%s (%s)"), *UFGConstructDisqualifier::GetDisqualifyingText(D).ToString(),
				UFGConstructDisqualifier::GetIsSoftDisqualifier(D) ? TEXT("soft") : TEXT("hard")));
		}
		return Texts.IsEmpty() ? TEXT("<none>") : FString::Join(Texts, TEXT("; "));
	};
	UE_LOG(LogAIModAI, Display, TEXT("ConstructPipe diagnostic: playerLoc=%s playerRot=%s sourceConnectorLoc=%s sourceConnectorNormal=%s sourceConnectorClearanceLoc=%s destConnectorLoc=%s destConnectorNormal=%s destConnectorClearanceLoc=%s"),
		*Character->GetActorLocation().ToString(), *Character->GetActorRotation().ToString(),
		*SourceConnection->GetConnectorLocation().ToString(), *SourceConnection->GetConnectorNormal().ToString(), *SourceConnection->GetConnectorLocation(true).ToString(),
		*DestConnection->GetConnectorLocation().ToString(), *DestConnection->GetConnectorNormal().ToString(), *DestConnection->GetConnectorLocation(true).ToString());

	// Player-independence (the same fix used in
	// ConstructConveyorBelt/ConstructConveyorLift - see their comments):
	// without it, placement fails with "Invalid aim location!" the same way
	// belts do, even for a completely valid connector pair with a real
	// Distance apart. Point
	// the controller at a deterministic target computed from the two
	// connectors themselves (never the player's real aim), reasserted
	// every poll tick below.
	const FRotator PipeDeterministicLook = (DestConnection->GetConnectorLocation() - SourceConnection->GetConnectorLocation()).Rotation();
	if (AController* PipeController = Character->GetController())
	{
		PipeController->SetControlRotation(PipeDeterministicLook);
	}

	const FHitResult StartHit = MakeHitAt(SourceBuildable, SourceConnection);
	PipeHologram->UpdateHologramPlacement(StartHit);
	PipeHologram->TrySnapToActor(StartHit);
	const bool bStartStepComplete = PipeHologram->DoMultiStepPlacement(true);
	const ESplineHologramBuildStep StepAfterStart = PipeHologram->GetCurrentBuildStep();

	UE_LOG(LogAIModAI, Display, TEXT("ConstructPipe: source=%s dest=%s after start click: stepComplete=%s step=%d disqualifiers=[%s]"),
		*SourceBuildableId, *DestBuildableId, bStartStepComplete ? TEXT("true") : TEXT("false"), static_cast<int32>(StepAfterStart), *SummarizeDisqualifiers(PipeHologram));

	if (bStartStepComplete)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("UNEXPECTED_STEP_COMPLETE"), TEXT("DoMultiStepPlacement() reported complete after only the start click")));
		return;
	}

	const FHitResult EndHit = MakeHitAt(DestBuildable, DestConnection);
	PipeHologram->UpdateHologramPlacement(EndHit);
	PipeHologram->TrySnapToActor(EndHit);
	const bool bEndStepComplete = PipeHologram->DoMultiStepPlacement(true);
	const ESplineHologramBuildStep StepAfterEnd = PipeHologram->GetCurrentBuildStep();
	const bool bEndConnectionSnapped = PipeHologram->IsConnectionSnapped(false);

	UE_LOG(LogAIModAI, Display, TEXT("ConstructPipe: source=%s dest=%s after end click: stepComplete=%s step=%d connectionSnapped=%s disqualifiers=[%s]"),
		*SourceBuildableId, *DestBuildableId, bEndStepComplete ? TEXT("true") : TEXT("false"), static_cast<int32>(StepAfterEnd), bEndConnectionSnapped ? TEXT("true") : TEXT("false"), *SummarizeDisqualifiers(PipeHologram));

	if (!bEndStepComplete)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("PLACEMENT_INCOMPLETE"),
			FString::Printf(TEXT("DoMultiStepPlacement() did not report complete after the end click - step=%d connectionSnapped=%s, may need a third step"), static_cast<int32>(StepAfterEnd), bEndConnectionSnapped ? TEXT("true") : TEXT("false"))));
		return;
	}

	struct FPollState
	{
		TWeakObjectPtr<AFGPipelineHologram> Hologram;
		TWeakObjectPtr<AFGCharacterPlayer> Character;
		TWeakObjectPtr<UWorld> World;
		FString SourceBuildableId;
		FString DestBuildableId;
		bool bDryRun = true;
		FRotator DeterministicLook;
		FHitResult EndHit; // re-asserted every poll tick, see below
		int32 AttemptsRemaining = 120; // safety cap - real ticks, not a fixed duration
		int32 AttemptsTaken = 0;
		TFunction<void(const FAIModOperationResult&)> OnComplete;
	};
	const TSharedRef<FPollState> PollState = MakeShared<FPollState>();
	PollState->Hologram = PipeHologram;
	PollState->Character = Character;
	PollState->World = World;
	PollState->SourceBuildableId = SourceBuildableId;
	PollState->DestBuildableId = DestBuildableId;
	PollState->bDryRun = bDryRun;
	PollState->DeterministicLook = PipeDeterministicLook;
	PollState->EndHit = EndHit;
	PollState->OnComplete = MoveTemp(OnComplete);

	const TSharedRef<TFunction<void()>> PollFn = MakeShared<TFunction<void()>>();
	*PollFn = [PollState, PollFn]()
	{
		++PollState->AttemptsTaken;

		AFGPipelineHologram* PollHologram = PollState->Hologram.Get();
		UWorld* PollWorld = PollState->World.Get();
		AFGCharacterPlayer* PollCharacter = PollState->Character.Get();
		if (!IsValid(PollHologram) || !PollWorld)
		{
			UE_LOG(LogAIModAI, Warning, TEXT("ConstructPipe (deferred): hologram or world became invalid while polling (after %d tick(s))"), PollState->AttemptsTaken);
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("HOLOGRAM_INVALIDATED"), TEXT("Hologram or world became invalid while polling")));
			return;
		}

		// Re-assert every tick - see ConstructConveyorBelt's identical block
		// for the full rationale.
		if (IsValid(PollCharacter))
		{
			if (AController* PollController = PollCharacter->GetController())
			{
				PollController->SetControlRotation(PollState->DeterministicLook);
			}
		}

		// Re-assert the end hit every poll tick - same fix
		// as ConstructConveyorBelt_RealCharacterStrategy, applied here
		// for the same reason.
		PollHologram->UpdateHologramPlacement(PollState->EndHit);

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

		UE_LOG(LogAIModAI, Display, TEXT("ConstructPipe (deferred, resolved after %d real tick(s)): source=%s dest=%s dryRun=%s canConstruct=%s disqualifiers=[%s]"),
			PollState->AttemptsTaken, *PollState->SourceBuildableId, *PollState->DestBuildableId, PollState->bDryRun ? TEXT("true") : TEXT("false"),
			bCanConstruct ? TEXT("true") : TEXT("false"), *DisqualifierSummary);

		if (!bCanConstruct)
		{
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("CANNOT_CONSTRUCT"), DisqualifierSummary));
			return;
		}

		if (PollState->bDryRun)
		{
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Success());
			return;
		}

		AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(PollWorld);
		const FNetConstructionID ConstructionID = BuildableSubsystem ? BuildableSubsystem->GetNewNetConstructionID() : FNetConstructionID();

		AFGBuildGun* PollBuildGun = IsValid(PollCharacter) ? PollCharacter->GetBuildGun() : nullptr;
		UFGBuildGunStateBuild* PollBuildState = PollBuildGun ? Cast<UFGBuildGunStateBuild>(PollBuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD)) : nullptr;
		if (!PollBuildState)
		{
			UE_LOG(LogAIModAI, Error, TEXT("ConstructPipe (deferred): lost the build state before constructing - aborting, nothing built"));
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("Lost the build state before constructing")));
			return;
		}

		PollBuildState->InternalConstructHologram(ConstructionID);

		UE_LOG(LogAIModAI, Display, TEXT("ConstructPipe (deferred, resolved after %d real tick(s)): construction attempted via InternalConstructHologram - source=%s dest=%s"),
			PollState->AttemptsTaken, *PollState->SourceBuildableId, *PollState->DestBuildableId);

		if (IsValid(PollCharacter))
		{
			PollCharacter->UnequipBuildGun();
		}

		PollState->OnComplete(FAIModOperationResult::Success());
	};

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
}


// ConstructHypertube - hypertube support alongside longer pipe runs.
// Research finding (full detail in docs/hypertube-research.md): despite
// the separate-looking
// "Recipe_HyperTube*" family in the catalog (Junction/TJunction/
// WallSupport/WallHole - those are ATTACHMENTS, not the tube), the actual
// connecting tube is `Recipe_PipeHyper` -> AFGBuildablePipeHyper, and its
// hologram (`Holo_PipeHyper_C`) is a Blueprint child of the SAME
// AFGPipelineHologram class ConstructPipe already drives - confirmed from
// the hologram BP's own uasset name table. This is deliberately a
// near-mirror of ConstructPipe (same two-click TrySnapToActor +
// DoMultiStepPlacement flow, same deferred poll/disqualifier-ignore/
// deterministic-look pattern), differing only
// in: (1) hardcoded to Recipe_PipeHyper - no tiers exist, unlike
// Recipe_Pipeline/PipelineMK2, so no recipeClass param; (2) connector
// lookup via FindFreeHyperPipeConnection instead of FindFreePipeConnection,
// since hypertube connectors are UFGPipeConnectionComponentHyper at
// PCT_ANY, not PCT_PRODUCER/PCT_CONSUMER - the exact-type match that
// works for fluid pipe machines finds nothing on any hypertube part; (3)
// no real producer/consumer distinction - hypertubes are bidirectional,
// so "source"/"dest" here are just which buildable's free connector each
// end can find, not a meaningful flow direction.
void UAIModFunctionLibrary::ConstructHypertube(UObject* WorldContextObject, const FString& SourceBuildableId, const FString& DestBuildableId, bool bDryRun, TFunction<void(const FAIModOperationResult&)> OnComplete)
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

	AFGBuildable* SourceBuildable = FindBuildableById(World, SourceBuildableId);
	if (!SourceBuildable)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No buildable found with id '%s'"), *SourceBuildableId)));
		return;
	}
	AFGBuildable* DestBuildable = FindBuildableById(World, DestBuildableId);
	if (!DestBuildable)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No buildable found with id '%s'"), *DestBuildableId)));
		return;
	}

	UFGPipeConnectionComponentBase* SourceConnection = FindFreeHyperPipeConnection(SourceBuildable);
	if (!SourceConnection)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_PIPE_CONNECTION"), FString::Printf(TEXT("'%s' has no free hypertube connection component"), *SourceBuildableId)));
		return;
	}
	UFGPipeConnectionComponentBase* DestConnection = FindFreeHyperPipeConnection(DestBuildable);
	if (!DestConnection)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_PIPE_CONNECTION"), FString::Printf(TEXT("'%s' has no free hypertube connection component"), *DestBuildableId)));
		return;
	}

	UClass* HyperTubeRecipeClass = LoadObject<UClass>(nullptr, TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PipeHyper.Recipe_PipeHyper_C"));
	if (!HyperTubeRecipeClass || !HyperTubeRecipeClass->IsChildOf(UFGRecipe::StaticClass()))
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("RECIPE_LOAD_FAILED"), TEXT("Failed to load Recipe_PipeHyper as a UFGRecipe")));
		return;
	}
	const TSubclassOf<UFGRecipe> RecipeClass = HyperTubeRecipeClass;

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
	AFGPipelineHologram* HyperHologram = Cast<AFGPipelineHologram>(Hologram);
	if (!HyperHologram)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("HOLOGRAM_SPAWN_FAILED"),
			FString::Printf(TEXT("HotKeyRecipe(Recipe_PipeHyper) did not result in an AFGPipelineHologram (got %s)"),
				Hologram ? *Hologram->GetClass()->GetName() : TEXT("null"))));
		return;
	}

	auto MakeHitAt = [](AFGBuildable* Buildable, UFGPipeConnectionComponentBase* Connection) -> FHitResult
	{
		FHitResult Hit;
		Hit.Location = Connection->GetConnectorLocation();
		Hit.ImpactPoint = Hit.Location;
		Hit.Normal = Connection->GetConnectorNormal();
		Hit.ImpactNormal = Hit.Normal;
		Hit.HitObjectHandle = FActorInstanceHandle(Buildable);
		Hit.bBlockingHit = true;
		// See PopulateSyntheticTraceRay's
		// doc comment (load-bearing for the lift's height; suspected fix for
		// the belt player-distance "too long" failures).
		PopulateSyntheticTraceRay(Hit);
		return Hit;
	};

	auto SummarizeDisqualifiers = [](AFGPipelineHologram* H) -> FString
	{
		TArray<TSubclassOf<UFGConstructDisqualifier>> Disqualifiers;
		H->GetConstructDisqualifiers(Disqualifiers);
		TArray<FString> Texts;
		for (const TSubclassOf<UFGConstructDisqualifier>& D : Disqualifiers)
		{
			Texts.Add(FString::Printf(TEXT("%s (%s)"), *UFGConstructDisqualifier::GetDisqualifyingText(D).ToString(),
				UFGConstructDisqualifier::GetIsSoftDisqualifier(D) ? TEXT("soft") : TEXT("hard")));
		}
		return Texts.IsEmpty() ? TEXT("<none>") : FString::Join(Texts, TEXT("; "));
	};
	UE_LOG(LogAIModAI, Display, TEXT("ConstructHypertube diagnostic: sourceConnectorLoc=%s sourceConnectorNormal=%s destConnectorLoc=%s destConnectorNormal=%s"),
		*SourceConnection->GetConnectorLocation().ToString(), *SourceConnection->GetConnectorNormal().ToString(),
		*DestConnection->GetConnectorLocation().ToString(), *DestConnection->GetConnectorNormal().ToString());

	// Player-independence, applied from day one here (not retrofitted like
	// ConstructPipe/ConstructConveyorBelt above) - see those functions'
	// comments for the full incident this pattern fixes.
	const FRotator HyperDeterministicLook = (DestConnection->GetConnectorLocation() - SourceConnection->GetConnectorLocation()).Rotation();
	if (AController* HyperController = Character->GetController())
	{
		HyperController->SetControlRotation(HyperDeterministicLook);
	}

	const FHitResult StartHit = MakeHitAt(SourceBuildable, SourceConnection);
	HyperHologram->UpdateHologramPlacement(StartHit);
	HyperHologram->TrySnapToActor(StartHit);
	const bool bStartStepComplete = HyperHologram->DoMultiStepPlacement(true);
	const ESplineHologramBuildStep StepAfterStart = HyperHologram->GetCurrentBuildStep();

	UE_LOG(LogAIModAI, Display, TEXT("ConstructHypertube: source=%s dest=%s after start click: stepComplete=%s step=%d disqualifiers=[%s]"),
		*SourceBuildableId, *DestBuildableId, bStartStepComplete ? TEXT("true") : TEXT("false"), static_cast<int32>(StepAfterStart), *SummarizeDisqualifiers(HyperHologram));

	if (bStartStepComplete)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("UNEXPECTED_STEP_COMPLETE"), TEXT("DoMultiStepPlacement() reported complete after only the start click")));
		return;
	}

	const FHitResult EndHit = MakeHitAt(DestBuildable, DestConnection);
	HyperHologram->UpdateHologramPlacement(EndHit);
	HyperHologram->TrySnapToActor(EndHit);
	const bool bEndStepComplete = HyperHologram->DoMultiStepPlacement(true);
	const ESplineHologramBuildStep StepAfterEnd = HyperHologram->GetCurrentBuildStep();
	const bool bEndConnectionSnapped = HyperHologram->IsConnectionSnapped(false);

	UE_LOG(LogAIModAI, Display, TEXT("ConstructHypertube: source=%s dest=%s after end click: stepComplete=%s step=%d connectionSnapped=%s disqualifiers=[%s]"),
		*SourceBuildableId, *DestBuildableId, bEndStepComplete ? TEXT("true") : TEXT("false"), static_cast<int32>(StepAfterEnd), bEndConnectionSnapped ? TEXT("true") : TEXT("false"), *SummarizeDisqualifiers(HyperHologram));

	if (!bEndStepComplete)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("PLACEMENT_INCOMPLETE"),
			FString::Printf(TEXT("DoMultiStepPlacement() did not report complete after the end click - step=%d connectionSnapped=%s, may need a third step"), static_cast<int32>(StepAfterEnd), bEndConnectionSnapped ? TEXT("true") : TEXT("false"))));
		return;
	}

	struct FPollState
	{
		TWeakObjectPtr<AFGPipelineHologram> Hologram;
		TWeakObjectPtr<AFGCharacterPlayer> Character;
		TWeakObjectPtr<UWorld> World;
		FString SourceBuildableId;
		FString DestBuildableId;
		bool bDryRun = true;
		FRotator DeterministicLook;
		FHitResult EndHit; // re-asserted every poll tick, see below
		int32 AttemptsRemaining = 120; // safety cap - real ticks, not a fixed duration
		int32 AttemptsTaken = 0;
		TFunction<void(const FAIModOperationResult&)> OnComplete;
	};
	const TSharedRef<FPollState> PollState = MakeShared<FPollState>();
	PollState->Hologram = HyperHologram;
	PollState->Character = Character;
	PollState->World = World;
	PollState->SourceBuildableId = SourceBuildableId;
	PollState->DestBuildableId = DestBuildableId;
	PollState->bDryRun = bDryRun;
	PollState->DeterministicLook = HyperDeterministicLook;
	PollState->EndHit = EndHit;
	PollState->OnComplete = MoveTemp(OnComplete);

	const TSharedRef<TFunction<void()>> PollFn = MakeShared<TFunction<void()>>();
	*PollFn = [PollState, PollFn]()
	{
		++PollState->AttemptsTaken;

		AFGPipelineHologram* PollHologram = PollState->Hologram.Get();
		UWorld* PollWorld = PollState->World.Get();
		AFGCharacterPlayer* PollCharacter = PollState->Character.Get();
		if (!IsValid(PollHologram) || !PollWorld)
		{
			UE_LOG(LogAIModAI, Warning, TEXT("ConstructHypertube (deferred): hologram or world became invalid while polling (after %d tick(s))"), PollState->AttemptsTaken);
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("HOLOGRAM_INVALIDATED"), TEXT("Hologram or world became invalid while polling")));
			return;
		}

		if (IsValid(PollCharacter))
		{
			if (AController* PollController = PollCharacter->GetController())
			{
				PollController->SetControlRotation(PollState->DeterministicLook);
			}
		}

		// Re-assert the end hit every poll tick - same fix
		// as ConstructConveyorBelt_RealCharacterStrategy, applied here
		// for the same reason.
		PollHologram->UpdateHologramPlacement(PollState->EndHit);

		TArray<TSubclassOf<UFGConstructDisqualifier>> Disqualifiers;
		PollHologram->GetConstructDisqualifiers(Disqualifiers);
		const bool bStillInitializing = Disqualifiers.Contains(TSubclassOf<UFGConstructDisqualifier>(UFGCDInitializing::StaticClass()));

		--PollState->AttemptsRemaining;
		if (bStillInitializing && PollState->AttemptsRemaining > 0)
		{
			PollWorld->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
			return;
		}

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

		UE_LOG(LogAIModAI, Display, TEXT("ConstructHypertube (deferred, resolved after %d real tick(s)): source=%s dest=%s dryRun=%s canConstruct=%s disqualifiers=[%s]"),
			PollState->AttemptsTaken, *PollState->SourceBuildableId, *PollState->DestBuildableId, PollState->bDryRun ? TEXT("true") : TEXT("false"),
			bCanConstruct ? TEXT("true") : TEXT("false"), *DisqualifierSummary);

		if (!bCanConstruct)
		{
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("CANNOT_CONSTRUCT"), DisqualifierSummary));
			return;
		}

		if (PollState->bDryRun)
		{
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Success());
			return;
		}

		AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(PollWorld);
		const FNetConstructionID ConstructionID = BuildableSubsystem ? BuildableSubsystem->GetNewNetConstructionID() : FNetConstructionID();

		AFGBuildGun* PollBuildGun = IsValid(PollCharacter) ? PollCharacter->GetBuildGun() : nullptr;
		UFGBuildGunStateBuild* PollBuildState = PollBuildGun ? Cast<UFGBuildGunStateBuild>(PollBuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD)) : nullptr;
		if (!PollBuildState)
		{
			UE_LOG(LogAIModAI, Error, TEXT("ConstructHypertube (deferred): lost the build state before constructing - aborting, nothing built"));
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("Lost the build state before constructing")));
			return;
		}

		PollBuildState->InternalConstructHologram(ConstructionID);

		UE_LOG(LogAIModAI, Display, TEXT("ConstructHypertube (deferred, resolved after %d real tick(s)): construction attempted via InternalConstructHologram - source=%s dest=%s"),
			PollState->AttemptsTaken, *PollState->SourceBuildableId, *PollState->DestBuildableId);

		if (IsValid(PollCharacter))
		{
			PollCharacter->UnequipBuildGun();
		}

		PollState->OnComplete(FAIModOperationResult::Success());
	};

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
}


// Railroad tracks - researched from source before
// implementing: AFGRailroadTrackHologram : AFGSplineHologram, the exact
// same base ConstructPipe/ConstructConveyorBelt already drive
// (GetConstructDisqualifiers/CanConstruct/TrySnapToActor/
// DoMultiStepPlacement/GetCurrentBuildStep are all AFGSplineHologram
// members) - this is a near-mirror of ConstructPipe, same two-click
// snap-to-connector-component flow, differing only in the connector type
// (UFGRailroadTrackConnectionComponent via FindFreeRailroadConnection,
// bidirectional - no producer/consumer split). Switches and signals are
// deliberately out of scope (see FindFreeRailroadConnection's comment) -
// this only builds a single point-to-point segment between two existing
// connector-bearing buildables (e.g. two Train Station platforms, or an
// existing track's open end).
// Reflection setters for the rail hologram's PRIVATE UPROPERTYs
// (mUseCustomEndRotation, mHitTangent, mStraightMode). They are private in
// FGRailroadTrackHologram but UPROPERTY(CustomSerialization), so reflectable.
// This is how we drive the interactive player's far-end route controls (the
// far-end tangent = pitch + yaw) that our headless build otherwise never sets.
static void AIModSetHologramBoolProp(UObject* Obj, const TCHAR* Name, bool Value)
{
	if (!Obj) { return; }
	if (FBoolProperty* Prop = FindFProperty<FBoolProperty>(Obj->GetClass(), Name))
	{
		Prop->SetPropertyValue_InContainer(Obj, Value);
	}
}
static bool AIModSetHologramVectorProp(UObject* Obj, const TCHAR* Name, const FVector& Value)
{
	if (!Obj) { return false; }
	if (FProperty* Prop = FindFProperty<FProperty>(Obj->GetClass(), Name))
	{
		if (FVector* Ptr = Prop->ContainerPtrToValuePtr<FVector>(Obj))
		{
			*Ptr = Value;
			return true;
		}
	}
	return false;
}

void UAIModFunctionLibrary::ConstructRailroadTrack(UObject* WorldContextObject, const FString& SourceBuildableId, const FString& DestBuildableId, const FString& RecipeClassPath, bool bDryRun, const FVector& SourceConnectorPos, bool bHasSourceConnectorPos, const FVector& DestConnectorPos, bool bHasDestConnectorPos, bool bUsePrimaryFire, bool bStraightMode, int32 EndRotationSteps, const FVector& EndTangent, bool bHasEndTangent, TFunction<void(const FAIModOperationResult&)> OnComplete)
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

	AFGBuildable* SourceBuildable = FindBuildableById(World, SourceBuildableId);
	if (!SourceBuildable)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No buildable found with id '%s'"), *SourceBuildableId)));
		return;
	}
	// FREE-END mode: an empty destBuildableId means "build this
	// segment to a free landing point" (destConnectorPosition) rather than onto a
	// dest buildable's connector - the enabler for laying long multi-segment runs
	// track-to-track. The free END must land on a solid surface (a foundation),
	// same as the interactive build gun; only the free end needs a surface, the
	// span may float. The SOURCE is still a real connector (a station's, or a
	// prior track's free end - which IS the track-to-track snap), so segments
	// chain end to end.
	const bool bFreeEndDest = DestBuildableId.IsEmpty();
	AFGBuildable* DestBuildable = nullptr;
	if (!bFreeEndDest)
	{
		DestBuildable = FindBuildableById(World, DestBuildableId);
		if (!DestBuildable)
		{
			OnComplete(FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No buildable found with id '%s'"), *DestBuildableId)));
			return;
		}
	}
	else if (!bHasDestConnectorPos)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("MISSING_DEST_POSITION"), TEXT("free-end build (empty destBuildableId) requires destConnectorPosition - the landing point, which must be over a foundation")));
		return;
	}

	UFGRailroadTrackConnectionComponent* SourceConnection = bHasSourceConnectorPos
		? FindFreeRailroadConnectionNearest(SourceBuildable, SourceConnectorPos)
		: FindFreeRailroadConnection(SourceBuildable);
	if (!SourceConnection)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("NO_RAILROAD_CONNECTION"), FString::Printf(TEXT("'%s' has no free railroad track connection component"), *SourceBuildableId)));
		return;
	}
	UFGRailroadTrackConnectionComponent* DestConnection = nullptr;
	if (!bFreeEndDest)
	{
		DestConnection = bHasDestConnectorPos
			? FindFreeRailroadConnectionNearest(DestBuildable, DestConnectorPos)
			: FindFreeRailroadConnection(DestBuildable);
		if (!DestConnection)
		{
			OnComplete(FAIModOperationResult::Failure(TEXT("NO_RAILROAD_CONNECTION"), FString::Printf(TEXT("'%s' has no free railroad track connection component"), *DestBuildableId)));
			return;
		}
	}

	UClass* TrackRecipeClass = LoadObject<UClass>(nullptr, *RecipeClassPath);
	if (!TrackRecipeClass || !TrackRecipeClass->IsChildOf(UFGRecipe::StaticClass()))
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("INVALID_RECIPE"), FString::Printf(TEXT("'%s' did not resolve to a UFGRecipe subclass"), *RecipeClassPath)));
		return;
	}
	const TSubclassOf<UFGRecipe> RecipeClass = TrackRecipeClass;

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
	AFGRailroadTrackHologram* TrackHologram = Cast<AFGRailroadTrackHologram>(Hologram);
	if (!TrackHologram)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("HOLOGRAM_SPAWN_FAILED"),
			FString::Printf(TEXT("HotKeyRecipe(%s) did not result in an AFGRailroadTrackHologram (got %s)"),
				*RecipeClassPath, Hologram ? *Hologram->GetClass()->GetName() : TEXT("null"))));
		return;
	}

	// EXPERIMENTAL far-end/route controls (the interactive player has these; our
	// headless build never set them, which is why AutoRouteSpline balloons a
	// straight/gentle span into a "too long"/"too steep" spline). mStraightMode
	// and mUseCustomEndRotation are private UPROPERTYs on the hologram, set here
	// via reflection; ScrollRotate (the player's "rotate the far end" scroll) is
	// a public override applied between the start and end clicks below.
	// NOTE: "straight mode" MAY mean "auto 90-degree bends" rather than "straight
	// line" (as it does for conveyors/pipes) - treat as experimental, characterize
	// with the rail test course before relying on it.
	if (bStraightMode)
	{
		if (FBoolProperty* StraightProp = FindFProperty<FBoolProperty>(TrackHologram->GetClass(), TEXT("mStraightMode")))
		{
			StraightProp->SetPropertyValue_InContainer(TrackHologram, true);
			UE_LOG(LogAIModAI, Display, TEXT("ConstructRailroadTrack: set mStraightMode=true (experimental)"));
		}
		else
		{
			UE_LOG(LogAIModAI, Warning, TEXT("ConstructRailroadTrack: mStraightMode property not found - straightMode ignored"));
		}
	}

	auto MakeHitAt = [](AFGBuildable* Buildable, UFGRailroadTrackConnectionComponent* Connection) -> FHitResult
	{
		FHitResult Hit;
		Hit.Location = Connection->GetConnectorLocation();
		Hit.ImpactPoint = Hit.Location;
		Hit.Normal = Connection->GetConnectorNormal();
		Hit.ImpactNormal = Hit.Normal;
		Hit.HitObjectHandle = FActorInstanceHandle(Buildable);
		Hit.bBlockingHit = true;
		// Rail TrySnapToActor may key off the hit's Component (belts/pipes
		// tolerate a null Component; the rail hologram's start step never
		// advances without one). The connection is a
		// USceneComponent, not a primitive, so point the hit at the
		// buildable's root primitive instead.
		if (Buildable)
		{
			if (UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(Buildable->GetRootComponent()))
			{
				Hit.Component = RootPrim;
			}
		}
		// See PopulateSyntheticTraceRay's
		// doc comment (load-bearing for the lift's height; suspected fix for
		// the belt player-distance "too long" failures).
		PopulateSyntheticTraceRay(Hit);
		return Hit;
	};

	auto SummarizeDisqualifiers = [](AFGRailroadTrackHologram* H) -> FString
	{
		TArray<TSubclassOf<UFGConstructDisqualifier>> Disqualifiers;
		H->GetConstructDisqualifiers(Disqualifiers);
		TArray<FString> Texts;
		for (const TSubclassOf<UFGConstructDisqualifier>& D : Disqualifiers)
		{
			Texts.Add(FString::Printf(TEXT("%s (%s)"), *UFGConstructDisqualifier::GetDisqualifyingText(D).ToString(),
				UFGConstructDisqualifier::GetIsSoftDisqualifier(D) ? TEXT("soft") : TEXT("hard")));
		}
		return Texts.IsEmpty() ? TEXT("<none>") : FString::Join(Texts, TEXT("; "));
	};

	// Player-independence from day one - see ConstructPipe's comment for
	// the full incident this pattern fixes.
	const FVector TrackDestAimLoc = bFreeEndDest ? DestConnectorPos : DestConnection->GetConnectorLocation();
	const FRotator TrackDeterministicLook = (TrackDestAimLoc - SourceConnection->GetConnectorLocation()).Rotation();
	if (AController* TrackController = Character->GetController())
	{
		TrackController->SetControlRotation(TrackDeterministicLook);
	}

	auto StepName = [](ESplineHologramBuildStep s) -> const TCHAR*
	{
		switch (s)
		{
			case ESplineHologramBuildStep::SHBS_FindStart: return TEXT("FindStart");
			case ESplineHologramBuildStep::SHBS_AdjustStartingPole: return TEXT("AdjustStartingPole");
			case ESplineHologramBuildStep::SHBS_PlacePoleOrSnapEnding: return TEXT("PlacePoleOrSnapEnding");
			case ESplineHologramBuildStep::SHBS_AdjustPole: return TEXT("AdjustPole");
			default: return TEXT("?");
		}
	};

	// Snap verification helper: the drivable-joint fix hinges on
	// the hologram actually snapping its endpoint onto a station connector.
	// GetSnappedConnectionComponents() (public) returns the connectors the
	// hologram snapped to; if it contains our source/dest connection, the
	// engine will wire a real drivable joint in ConfigureComponents at construct
	// (unlike a post-construct AddConnection force-link, which only graph-merges).
	auto DidSnapTo = [](AFGRailroadTrackHologram* H, UFGRailroadTrackConnectionComponent* Want) -> bool
	{
		if (!Want) { return false; }
		for (UFGRailroadTrackConnectionComponent* S : H->GetSnappedConnectionComponents())
		{
			if (S == Want) { return true; }
		}
		return false;
	};

	// ==== Alternate PrimaryFire path (see docs/train-drivable-joint-research.md):
	// drive the engine's REAL build-gun PrimaryFire path instead of the manual
	// DoMultiStepPlacement + InternalConstructHologram below. The manual path
	// graph-merges the track but can leave the JOINT non-traversable (loco reports
	// StationUnreachable and never moves - seen for straight AND curved
	// track, even both-ends-snapped and powered). Theory: the binary's
	// ConfigureComponents (opaque stub in the workspace) wires a drivable joint
	// only when the hologram is in the exact state the interactive player build
	// leaves it, which PrimaryFire_Implementation() produces but our manual
	// DoMultiStepPlacement does not. So: fire the source connector as the first
	// "click", then the dest connector as the second "click" (which constructs),
	// letting the engine own placement + connection setup end to end. Behind a
	// param (default off) so the proven straight-build path is untouched.
	if (bUsePrimaryFire && !bDryRun && !bFreeEndDest)
	{
		const FHitResult FireStartHit = MakeHitAt(SourceBuildable, SourceConnection);
		const FHitResult FireEndHit = MakeHitAt(DestBuildable, DestConnection);

		struct FFireState
		{
			TWeakObjectPtr<AFGRailroadTrackHologram> Hologram;
			TWeakObjectPtr<AFGCharacterPlayer> Character;
			TWeakObjectPtr<AFGBuildGun> BuildGun;
			TWeakObjectPtr<UFGBuildGunStateBuild> BuildState;
			TWeakObjectPtr<UWorld> World;
			TWeakObjectPtr<UFGRailroadTrackConnectionComponent> SourceConn;
			TWeakObjectPtr<UFGRailroadTrackConnectionComponent> DestConn;
			FHitResult StartHit;
			FHitResult EndHit;
			FRotator DeterministicLook;
			FString SourceBuildableId;
			FString DestBuildableId;
			int32 Phase = 0; // 0 = wait-out-Initializing then fire START; 1 = fire END (constructs); 2 = report
			int32 AttemptsRemaining = 120;
			int32 AttemptsTaken = 0;
			TFunction<void(const FAIModOperationResult&)> OnComplete;
		};
		const TSharedRef<FFireState> Fire = MakeShared<FFireState>();
		Fire->Hologram = TrackHologram;
		Fire->Character = Character;
		Fire->BuildGun = BuildGun;
		Fire->BuildState = BuildState;
		Fire->World = World;
		Fire->SourceConn = SourceConnection;
		Fire->DestConn = DestConnection;
		Fire->StartHit = FireStartHit;
		Fire->EndHit = FireEndHit;
		Fire->DeterministicLook = TrackDeterministicLook;
		Fire->SourceBuildableId = SourceBuildableId;
		Fire->DestBuildableId = DestBuildableId;
		Fire->OnComplete = MoveTemp(OnComplete);

		const TSharedRef<TFunction<void()>> FireFn = MakeShared<TFunction<void()>>();
		*FireFn = [Fire, FireFn]()
		{
			++Fire->AttemptsTaken;
			AFGRailroadTrackHologram* H = Fire->Hologram.Get();
			UWorld* W = Fire->World.Get();
			AFGCharacterPlayer* Ch = Fire->Character.Get();
			AFGBuildGun* Gun = Fire->BuildGun.Get();
			UFGBuildGunStateBuild* State = Fire->BuildState.Get();
			if (!IsValid(H) || !W || !IsValid(Gun) || !IsValid(State))
			{
				if (IsValid(Ch)) { Ch->UnequipBuildGun(); }
				Fire->OnComplete(FAIModOperationResult::Failure(TEXT("HOLOGRAM_INVALIDATED"), TEXT("hologram/build gun invalid during PrimaryFire drive")));
				return;
			}

			// Keep the player's control rotation deterministic (as the manual
			// path does) so any aim-derived logic in PrimaryFire is stable.
			if (IsValid(Ch))
			{
				if (AController* Ct = Ch->GetController()) { Ct->SetControlRotation(Fire->DeterministicLook); }
			}

			auto DriveHit = [H, Gun](const FHitResult& Hit)
			{
				Gun->GetHitResult() = Hit;
				H->SetHologramLocationAndRotation(Hit);
				H->UpdateHologramPlacement(Hit);
			};

			--Fire->AttemptsRemaining;

			if (Fire->Phase == 0)
			{
				// Wait out the one-shot "Initializing" disqualifier just like the
				// manual poll, so the first fire lands on a ready hologram.
				TArray<TSubclassOf<UFGConstructDisqualifier>> Disq;
				H->GetConstructDisqualifiers(Disq);
				const bool bInit = Disq.Contains(TSubclassOf<UFGConstructDisqualifier>(UFGCDInitializing::StaticClass()));
				DriveHit(Fire->StartHit);
				if (bInit && Fire->AttemptsRemaining > 0)
				{
					W->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([FireFn]() { (*FireFn)(); }));
					return;
				}
				const ESplineHologramBuildStep StepBefore = H->GetCurrentBuildStep();
				// Drive the GUN's press+release cycle (not the state's fire impl
				// directly) - the gun manages mWaitingForPrimaryFireRelease and
				// redirects to the active state; a bare State->PrimaryFire_Implementation()
				// no-ops without that gun-level state (verified: step stayed 0).
				Gun->OnPrimaryFirePressed();
				Gun->OnPrimaryFireReleased();
				const ESplineHologramBuildStep StepAfter = H->GetCurrentBuildStep();
				UE_LOG(LogAIModAI, Display, TEXT("ConstructRailroadTrack[PrimaryFire]: START fire stepBefore=%d stepAfter=%d"),
					static_cast<int32>(StepBefore), static_cast<int32>(StepAfter));
				Fire->Phase = 1;
				W->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([FireFn]() { (*FireFn)(); }));
				return;
			}

			if (Fire->Phase == 1)
			{
				DriveHit(Fire->EndHit);
				const ESplineHologramBuildStep StepBefore = H->GetCurrentBuildStep();
				Gun->OnPrimaryFirePressed();
				Gun->OnPrimaryFireReleased();
				UE_LOG(LogAIModAI, Display, TEXT("ConstructRailroadTrack[PrimaryFire]: END fire stepBefore=%d (construct expected)"),
					static_cast<int32>(StepBefore));
				Fire->Phase = 2;
				// give the construct a tick to resolve before we look for the track
				W->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([FireFn]() { (*FireFn)(); }));
				return;
			}

			// Phase 2: report. The engine (if the fire worked) built + wired the
			// track itself - do NOT run the force-link/subsystem surgery. Resolve
			// the new track via a now-connected source/dest connector.
			if (IsValid(Ch)) { Ch->UnequipBuildGun(); }
			AFGBuildableRailroadTrack* NewTrack = nullptr;
			for (UFGRailroadTrackConnectionComponent* An : { Fire->SourceConn.Get(), Fire->DestConn.Get() })
			{
				if (IsValid(An) && An->IsConnected())
				{
					if (UFGRailroadTrackConnectionComponent* Peer = An->GetConnection())
					{
						NewTrack = Peer->GetTrack();
						if (NewTrack) { break; }
					}
				}
			}
			const bool bSrcConn = Fire->SourceConn.IsValid() && Fire->SourceConn->IsConnected();
			const bool bDstConn = Fire->DestConn.IsValid() && Fire->DestConn->IsConnected();
			UE_LOG(LogAIModAI, Display, TEXT("ConstructRailroadTrack[PrimaryFire]: done src=%s dst=%s newTrack=%s srcConnected=%d dstConnected=%d"),
				*Fire->SourceBuildableId, *Fire->DestBuildableId, NewTrack ? *NewTrack->GetName() : TEXT("<none>"), bSrcConn ? 1 : 0, bDstConn ? 1 : 0);
			if (!NewTrack)
			{
				Fire->OnComplete(FAIModOperationResult::Failure(TEXT("PRIMARYFIRE_NO_TRACK"),
					FString::Printf(TEXT("PrimaryFire drive produced no connected track (srcConnected=%d dstConnected=%d) - the fire path may need a different input sequence"),
						bSrcConn ? 1 : 0, bDstConn ? 1 : 0)));
				return;
			}
			Fire->OnComplete(FAIModOperationResult::Success());
		};
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([FireFn]() { (*FireFn)(); }));
		return;
	}

	// ---- START click. The rail hologram's start step never advances past
	// FindStart with the belt/pipe pattern. Capture every state signal
	// (returned verbatim in the error) and, if the "release/tap" input
	// didn't advance the step, retry as a "press". Also drive the
	// rail-specific SetHologramLocationAndRotation (the override that runs
	// TryFindAndSnapToOverlappingConnection) so the endpoint actually snaps
	// to the station connector - UpdateHologramPlacement alone never sets
	// IsConnectionSnapped.
	const FHitResult StartHit = MakeHitAt(SourceBuildable, SourceConnection);
	TrackHologram->SetHologramLocationAndRotation(StartHit);
	TrackHologram->UpdateHologramPlacement(StartHit);
	const bool bSnapStart = TrackHologram->TrySnapToActor(StartHit);
	const bool bCanStepStart = TrackHologram->CanTakeNextBuildStep();
	const bool bConnSnapStart = TrackHologram->IsConnectionSnapped(false);
	bool bStartStepComplete = TrackHologram->DoMultiStepPlacement(true);
	ESplineHologramBuildStep StepAfterStart = TrackHologram->GetCurrentBuildStep();
	bool bStartRetriedAsPress = false;
	if (!bStartStepComplete && StepAfterStart == ESplineHologramBuildStep::SHBS_FindStart)
	{
		bStartRetriedAsPress = true;
		TrackHologram->UpdateHologramPlacement(StartHit);
		TrackHologram->TrySnapToActor(StartHit);
		bStartStepComplete = TrackHologram->DoMultiStepPlacement(false);
		StepAfterStart = TrackHologram->GetCurrentBuildStep();
	}

	const bool bStartSnappedToConnector = DidSnapTo(TrackHologram, SourceConnection);
	FString Diag = FString::Printf(TEXT("start[snap=%d canStep=%d connSnap=%d snappedToSrc=%d done=%d step=%s pressRetry=%d disq=%s]"),
		bSnapStart ? 1 : 0, bCanStepStart ? 1 : 0, bConnSnapStart ? 1 : 0, bStartSnappedToConnector ? 1 : 0, bStartStepComplete ? 1 : 0,
		StepName(StepAfterStart), bStartRetriedAsPress ? 1 : 0, *SummarizeDisqualifiers(TrackHologram));
	UE_LOG(LogAIModAI, Display, TEXT("ConstructRailroadTrack: src=%s dst=%s %s"), *SourceBuildableId, *DestBuildableId, *Diag);

	if (bStartStepComplete)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("UNEXPECTED_STEP_COMPLETE"),
			FString::Printf(TEXT("placement completed after only the start click - %s"), *Diag)));
		return;
	}

	// ---- END click ----
	// Free-end: the second click lands on a foundation surface at the given XY
	// (trace the ground like ConstructBuildingAtPosition), so the hologram's end
	// sits on the foundation. Only the free end needs a surface; if none is found
	// the surface disqualifier will (correctly) block the build. Otherwise the end
	// snaps to the dest connector as before.
	FHitResult EndHit;
	if (bFreeEndDest)
	{
		const FGroundTraceResult FreeEndGround = FindGroundAtXY(World, DestConnectorPos.X, DestConnectorPos.Y, DestConnectorPos.Z, Character);
		EndHit = FreeEndGround.Hit;
		if (!FreeEndGround.bFound)
		{
			UE_LOG(LogAIModAI, Warning, TEXT("ConstructRailroadTrack: free-end landing at (%.0f,%.0f,%.0f) found no surface - place a foundation there first, or the surface check will block it"), DestConnectorPos.X, DestConnectorPos.Y, DestConnectorPos.Z);
		}
	}
	else
	{
		EndHit = MakeHitAt(DestBuildable, DestConnection);
	}
	TrackHologram->SetHologramLocationAndRotation(EndHit);
	TrackHologram->UpdateHologramPlacement(EndHit);
	// (Far-end route controls - ScrollRotate / mHitTangent - are applied in the
	// poll loop AFTER the hologram finishes "Initializing"; applying them here
	// during init is a no-op, which is why the earlier attempt had no effect.)
	const bool bSnapEnd = TrackHologram->TrySnapToActor(EndHit);
	const bool bCanStepEnd = TrackHologram->CanTakeNextBuildStep();
	bool bEndStepComplete = TrackHologram->DoMultiStepPlacement(true);
	ESplineHologramBuildStep StepAfterEnd = TrackHologram->GetCurrentBuildStep();
	bool bEndConnectionSnapped = TrackHologram->IsConnectionSnapped(true);
	bool bEndRetriedAsPress = false;
	if (!bEndStepComplete && StepAfterEnd == StepAfterStart)
	{
		bEndRetriedAsPress = true;
		TrackHologram->UpdateHologramPlacement(EndHit);
		TrackHologram->TrySnapToActor(EndHit);
		bEndStepComplete = TrackHologram->DoMultiStepPlacement(false);
		StepAfterEnd = TrackHologram->GetCurrentBuildStep();
		bEndConnectionSnapped = TrackHologram->IsConnectionSnapped(true);
	}

	const bool bEndSnappedToConnector = DidSnapTo(TrackHologram, DestConnection);
	const bool bBothConnectorsSnapped = bStartSnappedToConnector && bEndSnappedToConnector;
	Diag += FString::Printf(TEXT(" end[snap=%d canStep=%d done=%d step=%s connSnapLast=%d snappedToDst=%d pressRetry=%d disq=%s] bothSnapped=%d"),
		bSnapEnd ? 1 : 0, bCanStepEnd ? 1 : 0, bEndStepComplete ? 1 : 0, StepName(StepAfterEnd),
		bEndConnectionSnapped ? 1 : 0, bEndSnappedToConnector ? 1 : 0, bEndRetriedAsPress ? 1 : 0, *SummarizeDisqualifiers(TrackHologram), bBothConnectorsSnapped ? 1 : 0);
	UE_LOG(LogAIModAI, Display, TEXT("ConstructRailroadTrack: src=%s dst=%s %s"), *SourceBuildableId, *DestBuildableId, *Diag);

	if (!bEndStepComplete)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("PLACEMENT_INCOMPLETE"),
			FString::Printf(TEXT("track placement did not complete - %s"), *Diag)));
		return;
	}

	struct FPollState
	{
		TWeakObjectPtr<AFGRailroadTrackHologram> Hologram;
		TWeakObjectPtr<AFGCharacterPlayer> Character;
		TWeakObjectPtr<UWorld> World;
		FString SourceBuildableId;
		FString DestBuildableId;
		bool bDryRun = true;
		FRotator DeterministicLook;
		FHitResult EndHit; // re-asserted every poll tick, see below
		// The source/dest rail connections to force-link the new track to
		// (the hologram builds the spline but IsConnectionSnapped stays
		// false -> isolated track -> trains can't path).
		TWeakObjectPtr<UFGRailroadTrackConnectionComponent> SourceConn;
		TWeakObjectPtr<UFGRailroadTrackConnectionComponent> DestConn;
		// When the hologram genuinely snapped BOTH endpoints onto the station
		// connectors, ConfigureComponents (run inside InternalConstructHologram)
		// wires a real drivable joint - so the post-construct force-link/
		// RemoveTrack/AddTrack graph surgery must be SKIPPED (it only graph-
		// merges and would fight the engine's own setup).
		bool bBothSnapped = false;
		// EXPERIMENTAL far-end route controls, applied ONCE after the hologram
		// finishes "Initializing" (the correct moment; applying during init is a
		// no-op). EndRotationSteps -> ScrollRotate (yaw); EndTangent (if set) ->
		// mUseCustomEndRotation + mHitTangent (full 3D far-end tangent = pitch+yaw).
		int32 EndRotationSteps = 0;
		FVector EndTangent = FVector::ZeroVector;
		bool bHasEndTangent = false;
		bool bRouteControlsApplied = false;
		int32 AttemptsRemaining = 120;
		int32 AttemptsTaken = 0;
		TFunction<void(const FAIModOperationResult&)> OnComplete;
	};
	const TSharedRef<FPollState> PollState = MakeShared<FPollState>();
	PollState->Hologram = TrackHologram;
	PollState->Character = Character;
	PollState->World = World;
	PollState->SourceBuildableId = SourceBuildableId;
	PollState->DestBuildableId = DestBuildableId;
	PollState->bDryRun = bDryRun;
	PollState->DeterministicLook = TrackDeterministicLook;
	PollState->EndHit = EndHit;
	PollState->SourceConn = SourceConnection;
	PollState->DestConn = DestConnection;
	PollState->bBothSnapped = bBothConnectorsSnapped;
	PollState->EndRotationSteps = EndRotationSteps;
	PollState->EndTangent = EndTangent;
	PollState->bHasEndTangent = bHasEndTangent;
	PollState->OnComplete = MoveTemp(OnComplete);

	const TSharedRef<TFunction<void()>> PollFn = MakeShared<TFunction<void()>>();
	*PollFn = [PollState, PollFn]()
	{
		++PollState->AttemptsTaken;

		AFGRailroadTrackHologram* PollHologram = PollState->Hologram.Get();
		UWorld* PollWorld = PollState->World.Get();
		AFGCharacterPlayer* PollCharacter = PollState->Character.Get();
		if (!IsValid(PollHologram) || !PollWorld)
		{
			UE_LOG(LogAIModAI, Warning, TEXT("ConstructRailroadTrack (deferred): hologram or world became invalid while polling (after %d tick(s))"), PollState->AttemptsTaken);
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("HOLOGRAM_INVALIDATED"), TEXT("Hologram or world became invalid while polling")));
			return;
		}

		if (IsValid(PollCharacter))
		{
			if (AController* PollController = PollCharacter->GetController())
			{
				PollController->SetControlRotation(PollState->DeterministicLook);
			}
		}

		// Re-assert the end hit every poll tick - same fix
		// as ConstructConveyorBelt_RealCharacterStrategy, applied here
		// for the same reason.
		PollHologram->UpdateHologramPlacement(PollState->EndHit);

		TArray<TSubclassOf<UFGConstructDisqualifier>> Disqualifiers;
		PollHologram->GetConstructDisqualifiers(Disqualifiers);
		const bool bStillInitializing = Disqualifiers.Contains(TSubclassOf<UFGConstructDisqualifier>(UFGCDInitializing::StaticClass()));

		--PollState->AttemptsRemaining;
		if (bStillInitializing && PollState->AttemptsRemaining > 0)
		{
			PollWorld->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
			return;
		}

		// EXPERIMENTAL far-end route controls, applied ONCE now that the hologram
		// has finished initializing (this is the correct moment - the interactive
		// player scrolls/aims the far end here, after the end click, before
		// construct). ScrollRotate = the yaw scroll; mUseCustomEndRotation +
		// mHitTangent = the full 3D far-end tangent (pitch + yaw). Then re-route
		// (UpdateHologramPlacement re-runs AutoRouteSpline) and re-fetch
		// disqualifiers so the check below reflects the re-routed spline.
		if (!PollState->bRouteControlsApplied && (PollState->EndRotationSteps != 0 || PollState->bHasEndTangent))
		{
			PollState->bRouteControlsApplied = true;
			if (PollState->EndRotationSteps != 0)
			{
				const int32 RotStep = PollHologram->GetRotationStep();
				const int32 Dir = PollState->EndRotationSteps > 0 ? 1 : -1;
				for (int32 s = 0; s < FMath::Abs(PollState->EndRotationSteps); ++s)
				{
					PollHologram->ScrollRotate(Dir, RotStep);
				}
			}
			if (PollState->bHasEndTangent)
			{
				AIModSetHologramBoolProp(PollHologram, TEXT("mUseCustomEndRotation"), true);
				AIModSetHologramVectorProp(PollHologram, TEXT("mHitTangent"), PollState->EndTangent);
			}
			// Re-route with the new controls; re-set the tangent after in case
			// UpdateHologramPlacement recomputed mHitTangent from the hit.
			PollHologram->UpdateHologramPlacement(PollState->EndHit);
			if (PollState->bHasEndTangent)
			{
				AIModSetHologramBoolProp(PollHologram, TEXT("mUseCustomEndRotation"), true);
				AIModSetHologramVectorProp(PollHologram, TEXT("mHitTangent"), PollState->EndTangent);
				PollHologram->UpdateHologramPlacement(PollState->EndHit);
			}
			Disqualifiers.Reset();
			PollHologram->GetConstructDisqualifiers(Disqualifiers);
			TArray<FString> AppliedDisq;
			for (const TSubclassOf<UFGConstructDisqualifier>& D : Disqualifiers)
			{
				AppliedDisq.Add(UFGConstructDisqualifier::GetDisqualifyingText(D).ToString());
			}
			UE_LOG(LogAIModAI, Display, TEXT("ConstructRailroadTrack: post-init route controls applied (rotSteps=%d, hasTangent=%d tangent=%s); disq now=[%s]"),
				PollState->EndRotationSteps, PollState->bHasEndTangent ? 1 : 0, *PollState->EndTangent.ToString(),
				AppliedDisq.IsEmpty() ? TEXT("<none>") : *FString::Join(AppliedDisq, TEXT("; ")));
		}

		// No bIgnore* bypass flags here, deliberately - UFGCDTrackTooLong/
		// TooShort/TooSteep/TrunToSharp (sic - real name typo in source)
		// must always block construction, matching UFGCDWireTooLong
		// elsewhere in this file. Only UnlimitedResources (a player-
		// controlled mod setting) and the always-ignored aim-location
		// disqualifier get any leniency.
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

		UE_LOG(LogAIModAI, Display, TEXT("ConstructRailroadTrack (deferred, resolved after %d real tick(s)): source=%s dest=%s dryRun=%s canConstruct=%s disqualifiers=[%s]"),
			PollState->AttemptsTaken, *PollState->SourceBuildableId, *PollState->DestBuildableId, PollState->bDryRun ? TEXT("true") : TEXT("false"),
			bCanConstruct ? TEXT("true") : TEXT("false"), *DisqualifierSummary);

		if (!bCanConstruct)
		{
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("CANNOT_CONSTRUCT"), DisqualifierSummary));
			return;
		}

		if (PollState->bDryRun)
		{
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Success());
			return;
		}

		AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(PollWorld);
		const FNetConstructionID ConstructionID = BuildableSubsystem ? BuildableSubsystem->GetNewNetConstructionID() : FNetConstructionID();

		AFGBuildGun* PollBuildGun = IsValid(PollCharacter) ? PollCharacter->GetBuildGun() : nullptr;
		UFGBuildGunStateBuild* PollBuildState = PollBuildGun ? Cast<UFGBuildGunStateBuild>(PollBuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD)) : nullptr;
		if (!PollBuildState)
		{
			UE_LOG(LogAIModAI, Error, TEXT("ConstructRailroadTrack (deferred): lost the build state before constructing - aborting, nothing built"));
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("Lost the build state before constructing")));
			return;
		}

		PollBuildState->InternalConstructHologram(ConstructionID);

		UE_LOG(LogAIModAI, Display, TEXT("ConstructRailroadTrack (deferred, resolved after %d real tick(s)): construction attempted via InternalConstructHologram - source=%s dest=%s"),
			PollState->AttemptsTaken, *PollState->SourceBuildableId, *PollState->DestBuildableId);

		// GRAPH-LINK FIX: the hologram builds the track spline but
		// never snaps to the rail connections (IsConnectionSnapped stays false),
		// so the new track is isolated and trains can't
		// path over it (selfDrivingError StationUnreachable). The new track's
		// end connections are co-located with the source/dest connections we
		// aimed at but are NOT graph-linked. Explicitly link them via the public
		// UFGRailroadTrackConnectionComponent::AddConnection (bidirectional).
		auto ForceLink = [PollWorld](UFGRailroadTrackConnectionComponent* Anchor, const TCHAR* Label) -> AFGBuildableRailroadTrack*
		{
			if (!IsValid(Anchor))
			{
				return nullptr;
			}
			if (Anchor->IsConnected())
			{
				UE_LOG(LogAIModAI, Display, TEXT("ConstructRailroadTrack: %s connection already graph-linked (hologram snapped)"), Label);
				UFGRailroadTrackConnectionComponent* Peer = Anchor->GetConnection();
				return Peer ? Peer->GetTrack() : nullptr;
			}
			const FVector Loc = Anchor->GetConnectorLocation();
			UFGRailroadTrackConnectionComponent* Best = nullptr;
			float BestDistSq = TNumericLimits<float>::Max();
			for (TActorIterator<AFGBuildableRailroadTrack> It(PollWorld); It; ++It)
			{
				AFGBuildableRailroadTrack* Track = *It;
				if (!IsValid(Track))
				{
					continue;
				}
				TArray<UFGRailroadTrackConnectionComponent*> Conns;
				Track->GetComponents<UFGRailroadTrackConnectionComponent>(Conns);
				for (UFGRailroadTrackConnectionComponent* Cn : Conns)
				{
					if (!IsValid(Cn) || Cn == Anchor || Cn->IsConnected())
					{
						continue;
					}
					const float DistSq = FVector::DistSquared(Cn->GetConnectorLocation(), Loc);
					if (DistSq < BestDistSq)
					{
						BestDistSq = DistSq;
						Best = Cn;
					}
				}
			}
			// Co-located end of the freshly built track (tolerance 150cm).
			if (Best && BestDistSq <= 150.0f * 150.0f)
			{
				Anchor->AddConnection(Best);
				UE_LOG(LogAIModAI, Display, TEXT("ConstructRailroadTrack: force-linked %s connection to new track end at dist %.1f (nowConnected=%s)"),
					Label, FMath::Sqrt(BestDistSq), Anchor->IsConnected() ? TEXT("true") : TEXT("false"));
				return Best->GetTrack();
			}
			UE_LOG(LogAIModAI, Warning, TEXT("ConstructRailroadTrack: could not force-link %s connection - no free co-located track end (nearest %.1f)"),
				Label, Best ? FMath::Sqrt(BestDistSq) : -1.0f);
			return nullptr;
		};
		if (PollState->bBothSnapped)
		{
			// Engine snapped both endpoints onto the station connectors during
			// placement, so ConfigureComponents already LINKED the connections
			// (An->IsConnected() is true both ends). That is NOT enough on its
			// own: a snapped track still reads StationUnreachable because the
			// railroad SUBSYSTEM's pathfinding graph never merged it in:
			// a loco between two stations on a fully-snapped joint still can't
			// path to either, so skipping the graph surgery when snapped is
			// NOT safe. So fall
			// through into the same RemoveTrack/AddTrack re-registration below,
			// which IS what merges the graphs. ForceLink there is a safe no-op
			// when a connection is already snapped (it early-returns the peer's
			// track without calling AddConnection), so this single path serves
			// both the snapped and the never-snapped cases.
			UE_LOG(LogAIModAI, Display, TEXT("ConstructRailroadTrack: both ends snapped to station connectors - still re-registering with railroad subsystem for the pathfinding-graph merge"));
		}
		if (!PollState->bDryRun)
		{
			// The hologram builds the spline but never connection-snaps
			// (IsConnectionSnapped stayed false), and AddConnection alone
			// doesn't reach the railroad SUBSYSTEM's pathfinding graph - trains
			// read StationUnreachable even across a straight, fully component-
			// connected join. Correct sequence per
			// AFGRailroadSubsystem::AddTrack ("Track must have its connections
			// set up"): find the new track, RemoveTrack it (drops it from its
			// isolated graph), (re)link BOTH ends to the source/dest station
			// connections, THEN AddTrack so the subsystem merges the graphs
			// with the complete connection set. Order matters: linking must
			// happen while the track is out of the graph and BEFORE AddTrack.
			AFGRailroadSubsystem* RailSub = AFGRailroadSubsystem::Get(PollWorld);
			AFGBuildableRailroadTrack* NewTrack = nullptr;
			for (UFGRailroadTrackConnectionComponent* An : { PollState->SourceConn.Get(), PollState->DestConn.Get() })
			{
				if (IsValid(An) && An->IsConnected())
				{
					if (UFGRailroadTrackConnectionComponent* Peer = An->GetConnection())
					{
						NewTrack = Peer->GetTrack();
						if (NewTrack) { break; }
					}
				}
			}
			if (NewTrack && RailSub)
			{
				RailSub->RemoveTrack(NewTrack);
			}
			ForceLink(PollState->SourceConn.Get(), TEXT("source"));
			ForceLink(PollState->DestConn.Get(), TEXT("dest"));
			if (!NewTrack)
			{
				// Fallback: resolve the curve now that links exist.
				for (UFGRailroadTrackConnectionComponent* An : { PollState->SourceConn.Get(), PollState->DestConn.Get() })
				{
					if (IsValid(An) && An->IsConnected())
					{
						if (UFGRailroadTrackConnectionComponent* Peer = An->GetConnection())
						{
							NewTrack = Peer->GetTrack();
							if (NewTrack) { break; }
						}
					}
				}
			}
			if (NewTrack && RailSub)
			{
				RailSub->AddTrack(NewTrack);
				UE_LOG(LogAIModAI, Display, TEXT("ConstructRailroadTrack: re-registered new track %s with railroad subsystem for graph merge"), *NewTrack->GetName());
			}
			else
			{
				UE_LOG(LogAIModAI, Warning, TEXT("ConstructRailroadTrack: no new track resolved for subsystem re-registration (RailSub=%s)"), RailSub ? TEXT("ok") : TEXT("null"));
			}

			// DRIVABLE-JOINT REPAIR (docs/train-drivable-joint-research.md).
			// RemoveTrack/AddTrack above graph-MERGES the track (same trackGraphID)
			// but the joint is still not a drivable track-POSITION edge - a loco
			// reads StationUnreachable and never moves. A single HUMAN in-game
			// connection to RPC-built track repairs it, so the engine has a fixup
			// path to invoke. Two PUBLIC
			// AFGRailroadSubsystem entry points do exactly that global repair:
			//   - ValidateAndFixupAllRailroadConnections(): "goes through every
			//     railroad track buildable and fixes up their connections. Removing
			//     invalid ones, adding missing switches for junctions, etc."
			//   - Debug_MarkAllGraphsForFullRebuild(): forces TickTrackGraphs to
			//     recompute the navigation graph (track positions) next tick.
			// Call fixup first (repairs/validates the connection topology), then
			// mark for full rebuild so the pathfinding graph is recomputed with the
			// repaired joint. This keeps the proven manual build and just adds the
			// engine's own repair the interactive path triggers implicitly.
			if (RailSub)
			{
				RailSub->ValidateAndFixupAllRailroadConnections();
				RailSub->Debug_MarkAllGraphsForFullRebuild();
				UE_LOG(LogAIModAI, Display, TEXT("ConstructRailroadTrack: ran ValidateAndFixupAllRailroadConnections + Debug_MarkAllGraphsForFullRebuild to repair the joint into a drivable edge"));
			}
		}

		if (IsValid(PollCharacter))
		{
			PollCharacter->UnequipBuildGun();
		}

		PollState->OnComplete(FAIModOperationResult::Success());
	};

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
}



// See ConstructBeam's doc comment in the header for the real
// AFGBeamHologram sourcing, the build-mode reflection reasoning, and the
// (unconfirmed) RotationScrollSteps timing/meaning.
void UAIModFunctionLibrary::ConstructBeam(UObject* WorldContextObject, const FString& RecipeClassPath, float StartX, float StartY, float StartZ, float EndX, float EndY, float EndZ, bool bIgnoreGroundTrace, bool bFreeformMode, int32 RotationScrollSteps, TFunction<void(const FAIModOperationResult&)> OnComplete)
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

	UClass* ResolvedClass = LoadObject<UClass>(nullptr, *RecipeClassPath);
	if (!ResolvedClass || !ResolvedClass->IsChildOf(UFGRecipe::StaticClass()))
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("INVALID_RECIPE"), FString::Printf(TEXT("'%s' did not resolve to a UFGRecipe subclass"), *RecipeClassPath)));
		return;
	}
	const TSubclassOf<UFGRecipe> RecipeClass = ResolvedClass;

	if (bIgnoreGroundTrace && (StartZ <= -1000000.0f || EndZ <= -1000000.0f))
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("MISSING_REFERENCE_Z"),
			TEXT("bIgnoreGroundTrace requires explicit startZ and endZ - there is no ground trace to fall back to")));
		return;
	}

	auto MakeHit = [World, Character, bIgnoreGroundTrace](float X, float Y, float Z) -> FHitResult
	{
		FHitResult Hit;
		if (bIgnoreGroundTrace)
		{
			Hit.Location = FVector(X, Y, Z);
			Hit.ImpactPoint = Hit.Location;
			Hit.Normal = FVector::UpVector;
			Hit.ImpactNormal = FVector::UpVector;
			Hit.bBlockingHit = true;
		}
		else
		{
			const float ZSearchCenter = (Z > -1000000.0f) ? Z : Character->GetActorLocation().Z;
			const FGroundTraceResult GroundTrace = FindGroundAtXY(World, X, Y, ZSearchCenter, Character);
			Hit = GroundTrace.Hit;
		}
		return Hit;
	};

	const FHitResult StartHitPreview = MakeHit(StartX, StartY, StartZ);
	const FHitResult EndHitPreview = MakeHit(EndX, EndY, EndZ);

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

	AFGBeamHologram* BeamHologram = Cast<AFGBeamHologram>(BuildState->GetHologram());
	if (!BeamHologram)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("HOLOGRAM_SPAWN_FAILED"),
			FString::Printf(TEXT("HotKeyRecipe(%s) did not result in an AFGBeamHologram (got %s)"),
				*RecipeClassPath, BuildState->GetHologram() ? *BuildState->GetHologram()->GetClass()->GetName() : TEXT("null"))));
		return;
	}

	// Build-mode selection - see header doc comment. Missing property
	// (nullptr) just means the override is skipped, not a hard failure -
	// the hologram's own mDefaultBuildMode still applies.
	const TSubclassOf<UFGHologramBuildModeDescriptor> RequestedBuildMode = ReadBeamBuildModeProperty(
		BeamHologram, bFreeformMode ? TEXT("mBuildModeFreeForm") : TEXT("mBuildModeDiagonal"));
	if (RequestedBuildMode)
	{
		BeamHologram->SetBuildModeOverride(RequestedBuildMode);
	}

	auto SummarizeDisqualifiers = [](AFGBeamHologram* H) -> FString
	{
		TArray<TSubclassOf<UFGConstructDisqualifier>> Disqualifiers;
		H->GetConstructDisqualifiers(Disqualifiers);
		TArray<FString> Texts;
		for (const TSubclassOf<UFGConstructDisqualifier>& D : Disqualifiers)
		{
			Texts.Add(FString::Printf(TEXT("%s (%s)"), *UFGConstructDisqualifier::GetDisqualifyingText(D).ToString(),
				UFGConstructDisqualifier::GetIsSoftDisqualifier(D) ? TEXT("soft") : TEXT("hard")));
		}
		return Texts.IsEmpty() ? TEXT("<none>") : FString::Join(Texts, TEXT("; "));
	};

	const FRotator BeamDeterministicLook = (EndHitPreview.Location - StartHitPreview.Location).Rotation();
	if (AController* BeamController = Character->GetController())
	{
		BeamController->SetControlRotation(BeamDeterministicLook);
	}

	BeamHologram->UpdateHologramPlacement(StartHitPreview);
	BeamHologram->TrySnapToActor(StartHitPreview);
	const bool bStartStepComplete = BeamHologram->DoMultiStepPlacement(true);

	UE_LOG(LogAIModAI, Display, TEXT("ConstructBeam: start=(%.0f,%.0f,%.0f) end=(%.0f,%.0f,%.0f) freeform=%s after start click: stepComplete=%s disqualifiers=[%s]"),
		StartX, StartY, StartZ, EndX, EndY, EndZ, bFreeformMode ? TEXT("true") : TEXT("false"), bStartStepComplete ? TEXT("true") : TEXT("false"), *SummarizeDisqualifiers(BeamHologram));

	if (bStartStepComplete)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("UNEXPECTED_STEP_COMPLETE"), TEXT("DoMultiStepPlacement() reported complete after only the start click")));
		return;
	}

	BeamHologram->UpdateHologramPlacement(EndHitPreview);
	BeamHologram->TrySnapToActor(EndHitPreview);

	// RotationScrollSteps - see header doc comment for why this timing
	// (after the end hit, before finalizing) and the real
	// GetRotationStep() query (0/negative means "no override", per its
	// own doc comment) instead of a hardcoded 90.
	const int32 RawRotationStep = BeamHologram->GetRotationStep();
	const int32 EffectiveRotationStep = RawRotationStep > 0 ? RawRotationStep : 90;
	BeamHologram->SetScrollRotateValue(0);
	if (RotationScrollSteps != 0)
	{
		const int32 ScrollDirection = RotationScrollSteps > 0 ? 1 : -1;
		for (int32 i = 0; i < FMath::Abs(RotationScrollSteps); ++i)
		{
			BeamHologram->ScrollRotate(ScrollDirection, EffectiveRotationStep);
		}
	}

	const bool bEndStepComplete = BeamHologram->DoMultiStepPlacement(true);

	UE_LOG(LogAIModAI, Display, TEXT("ConstructBeam: start=(%.0f,%.0f,%.0f) end=(%.0f,%.0f,%.0f) rotationStep=%d rotationScrollSteps=%d after end click: stepComplete=%s disqualifiers=[%s]"),
		StartX, StartY, StartZ, EndX, EndY, EndZ, RawRotationStep, RotationScrollSteps, bEndStepComplete ? TEXT("true") : TEXT("false"), *SummarizeDisqualifiers(BeamHologram));

	if (!bEndStepComplete)
	{
		Character->UnequipBuildGun();
		OnComplete(FAIModOperationResult::Failure(TEXT("PLACEMENT_INCOMPLETE"), TEXT("DoMultiStepPlacement() did not report complete after the end click")));
		return;
	}

	struct FPollState
	{
		TWeakObjectPtr<AFGBeamHologram> Hologram;
		TWeakObjectPtr<AFGCharacterPlayer> Character;
		TWeakObjectPtr<UWorld> World;
		FRotator DeterministicLook;
		FHitResult EndHit; // re-asserted every poll tick, see below
		int32 AttemptsRemaining = 120;
		int32 AttemptsTaken = 0;
		TFunction<void(const FAIModOperationResult&)> OnComplete;
	};
	const TSharedRef<FPollState> PollState = MakeShared<FPollState>();
	PollState->Hologram = BeamHologram;
	PollState->Character = Character;
	PollState->World = World;
	PollState->DeterministicLook = BeamDeterministicLook;
	PollState->EndHit = EndHitPreview;
	PollState->OnComplete = MoveTemp(OnComplete);

	const TSharedRef<TFunction<void()>> PollFn = MakeShared<TFunction<void()>>();
	*PollFn = [PollState, PollFn]()
	{
		++PollState->AttemptsTaken;

		AFGBeamHologram* PollHologram = PollState->Hologram.Get();
		UWorld* PollWorld = PollState->World.Get();
		AFGCharacterPlayer* PollCharacter = PollState->Character.Get();
		if (!IsValid(PollHologram) || !PollWorld)
		{
			UE_LOG(LogAIModAI, Warning, TEXT("ConstructBeam (deferred): hologram or world became invalid while polling (after %d tick(s))"), PollState->AttemptsTaken);
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("HOLOGRAM_INVALIDATED"), TEXT("Hologram or world became invalid while polling")));
			return;
		}

		if (IsValid(PollCharacter))
		{
			if (AController* PollController = PollCharacter->GetController())
			{
				PollController->SetControlRotation(PollState->DeterministicLook);
			}
		}

		// Re-assert the end hit every poll tick - same fix as
		// ConstructConveyorBelt_RealCharacterStrategy/
		// ConstructVehiclePathSegment, applied here for the same reason.
		PollHologram->UpdateHologramPlacement(PollState->EndHit);

		TArray<TSubclassOf<UFGConstructDisqualifier>> Disqualifiers;
		PollHologram->GetConstructDisqualifiers(Disqualifiers);
		const bool bStillInitializing = Disqualifiers.Contains(TSubclassOf<UFGConstructDisqualifier>(UFGCDInitializing::StaticClass()));

		--PollState->AttemptsRemaining;
		if (bStillInitializing && PollState->AttemptsRemaining > 0)
		{
			PollWorld->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
			return;
		}

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

		UE_LOG(LogAIModAI, Display, TEXT("ConstructBeam (deferred, resolved after %d real tick(s)): canConstruct=%s disqualifiers=[%s]"),
			PollState->AttemptsTaken, bCanConstruct ? TEXT("true") : TEXT("false"), *DisqualifierSummary);

		if (!bCanConstruct)
		{
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
			UE_LOG(LogAIModAI, Error, TEXT("ConstructBeam (deferred): lost the build state before constructing - aborting, nothing built"));
			if (IsValid(PollCharacter)) { PollCharacter->UnequipBuildGun(); }
			PollState->OnComplete(FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("Lost the build state before constructing")));
			return;
		}

		PollBuildState->InternalConstructHologram(ConstructionID);

		UE_LOG(LogAIModAI, Display, TEXT("ConstructBeam (deferred, resolved after %d real tick(s)): construction attempted via InternalConstructHologram"),
			PollState->AttemptsTaken);

		if (IsValid(PollCharacter))
		{
			PollCharacter->UnequipBuildGun();
		}

		PollState->OnComplete(FAIModOperationResult::Success());
	};

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([PollFn]() { (*PollFn)(); }));
}


// See ConstructStackableSupport's doc comment in the header for the
// real AFGBuildablePoleStackable/AFGStackablePoleHologram/SetZoopAmount
// sourcing and the SetZoopFromHitresult-may-overwrite-it risk.

// See ConstructStackableSupport's doc comment in the header for the
// real Zoop/SetZoopAmount sourcing - this and
// ConstructStackableSupportOnTop below just resolve their own candidate
// position, then delegate to the shared
// ConstructStackableSupportAtCandidatePosition helper above.
void UAIModFunctionLibrary::ConstructStackableSupport(UObject* WorldContextObject, const FString& RecipeClassPath, float X, float Y, float Z, int32 StackCount, bool bIgnoreGroundTrace, TFunction<void(const FAIModOperationResult&)> OnComplete)
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

	if (bIgnoreGroundTrace && Z <= -1000000.0f)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("MISSING_REFERENCE_Z"),
			TEXT("bIgnoreGroundTrace requires an explicit z - there is no ground trace to fall back to")));
		return;
	}

	FVector CandidatePosition;
	if (bIgnoreGroundTrace)
	{
		CandidatePosition = FVector(X, Y, Z);
	}
	else
	{
		const float ZSearchCenter = (Z > -1000000.0f) ? Z : Character->GetActorLocation().Z;
		const FGroundTraceResult GroundTrace = FindGroundAtXY(World, X, Y, ZSearchCenter, Character);
		CandidatePosition = GroundTrace.Hit.Location;
	}

	ConstructStackableSupportAtCandidatePosition(World, Character, CandidatePosition, RecipeClassPath, StackCount,
		FString::Printf(TEXT("(%.0f,%.0f,%.0f)"), X, Y, Z), MoveTemp(OnComplete));
}


// See ConstructStackableSupportOnTop's doc comment in the header - the
// "snap a (possibly different) support onto an existing one's real top"
// counterpart to ConstructStackableSupport's literal-position/StackCount
// mode - mixed pipe+belt dense routing is normally built as separate
// stacked attachments, not one uniform Zoop placement.
void UAIModFunctionLibrary::ConstructStackableSupportOnTop(UObject* WorldContextObject, const FString& ReferenceBuildableId, const FString& RecipeClassPath, TFunction<void(const FAIModOperationResult&)> OnComplete)
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
	AFGBuildablePoleStackable* ReferencePole = Cast<AFGBuildablePoleStackable>(ReferenceBuildable);
	if (!ReferencePole)
	{
		OnComplete(FAIModOperationResult::Failure(TEXT("WRONG_TYPE"),
			FString::Printf(TEXT("'%s' did not resolve to an AFGBuildablePoleStackable (%s)"), *ReferenceBuildableId,
				ReferenceBuildable ? *ReferenceBuildable->GetClass()->GetName() : TEXT("not found"))));
		return;
	}

	// GetStackHeight() is real, public (FGBuildablePoleStackable.h), but
	// reads back as 0 for at least
	// Recipe_PipeSupportStackable's buildable class - constructing a second
	// pole at literally the same Z as a real placed reference (dz=0) fails
	// with "An identical buildable is already built there!". A modest
	// literal offset (dz=50 through dz=400 all land correctly in the same
	// real column) is all TrySnapToActor (inside the shared helper below)
	// needs to find the true next slot - the real engine's own snap logic
	// resolves the exact final position regardless of small input error,
	// same "let the real engine decide" posture as every other Construct*
	// function in this file. Floor GetStackHeight() at a sane minimum
	// rather than trusting it outright, so a same-shaped 0 (or
	// near-zero) reading for any other stackable tier can't reproduce
	// this same self-collision.
	const FVector CandidatePosition = ReferencePole->GetActorLocation() + FVector(0.0f, 0.0f, FMath::Max(ReferencePole->GetStackHeight(), 100.0f));

	ConstructStackableSupportAtCandidatePosition(World, Character, CandidatePosition, RecipeClassPath, 0, ReferenceBuildableId, MoveTemp(OnComplete));
}


// See SetBeamLength's doc comment in the header for the real
// AFGBuildableBeam::SetLength() sourcing and the lightweight-instance
// persistence caveat.
FAIModOperationResult UAIModFunctionLibrary::SetBeamLength(UObject* WorldContextObject, const FString& BuildableId, float NewLength)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}

	AFGBuildable* Buildable = nullptr;

	if (IsLightweightBuildableId(BuildableId))
	{
		// Same resolution as DismantleBuildable's lightweight branch -
		// see that function's comment for the full rationale.
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

	if (!Buildable)
	{
		return FAIModOperationResult::Failure(TEXT("TARGET_NOT_FOUND"), FString::Printf(TEXT("No buildable found with id '%s'"), *BuildableId));
	}

	AFGBuildableBeam* Beam = Cast<AFGBuildableBeam>(Buildable);
	if (!Beam)
	{
		return FAIModOperationResult::Failure(TEXT("WRONG_TYPE"), FString::Printf(TEXT("'%s' is a %s, not an AFGBuildableBeam"), *BuildableId, *Buildable->GetClass()->GetName()));
	}

	if (NewLength <= 0.0f || NewLength > Beam->GetMaxLength())
	{
		return FAIModOperationResult::Failure(TEXT("INVALID_LENGTH"),
			FString::Printf(TEXT("newLength %.1f must be > 0 and <= this beam's real maxLength %.1f"), NewLength, Beam->GetMaxLength()));
	}

	const float OldLength = Beam->GetLength();
	Beam->SetLength(NewLength);

	const TSharedRef<FJsonObject> DetailObject = MakeShared<FJsonObject>();
	DetailObject->SetNumberField(TEXT("oldLength"), OldLength);
	DetailObject->SetNumberField(TEXT("newLength"), Beam->GetLength());
	DetailObject->SetNumberField(TEXT("maxLength"), Beam->GetMaxLength());

	UE_LOG(LogAIModAI, Display, TEXT("SetBeamLength: '%s' %.1f -> %.1f (maxLength=%.1f)"), *BuildableId, OldLength, Beam->GetLength(), Beam->GetMaxLength());

	FAIModOperationResult Result = FAIModOperationResult::Success();
	Result.ResultDetailJson = WriteCondensedJson(DetailObject);
	return Result;
}
