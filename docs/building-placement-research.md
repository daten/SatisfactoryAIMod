# Building placement research (Phase 13) — read-only, no implementation

Recorded 2026-08-24. Same evidence-based approach and stub-`.cpp` caveat
as [resource-node-research.md](resource-node-research.md) and
[buildable-research.md](buildable-research.md) (re-confirmed directly here:
`AFGHologram::CanConstruct() const{ return bool(); }` at
`Source/FactoryGame/Private/Hologram/FGHologram.cpp:99`,
`AFGBuildableSubsystem::BeginSpawnBuildable(...){ return nullptr; }` at
`Source/FactoryGame/Private/FGBuildableSubsystem.cpp:92`). This document
is research only — **no building-placement code exists in AIMod, and
none is being written as part of this pass.** See "Why this stops here"
below.

## 1. The hologram system

`AFGHologram` — `Source/FactoryGame/Public/Hologram/FGHologram.h:84`,
base `AActor` + `IFGConstructionMessageInterface`. A preview/ghost actor
distinct from the eventual `AFGBuildable` — doc comment: *"The base class
for all holograms... defines the interface all 'buildable things' must
follow"* (`:76-82`).

Class linkage: descriptor-level
(`UFGBuildDescriptor::GetHologramClass`/`GetHologramClassInternal`,
`Source/FactoryGame/Public/Resources/FGBuildDescriptor.h:29,36`) and
buildable-instance-level (`AFGBuildable::mHologramClass`,
`FGBuildable.h:820`).

Conversion entry point: `virtual AActor* AFGHologram::Construct(TArray<AActor*>& out_children, FNetConstructionID constructionID)`
(`FGHologram.h:331`, doc: *"Construct the real deal... @return the
constructed actor; nullptr on failure"*), overridden by
`AFGBuildableHologram::Construct(...)`
(`Source/FactoryGame/Public/Hologram/FGBuildableHologram.h:159`).

## 2. The build gun

`AFGBuildGun` — `Source/FactoryGame/Public/Equipment/FGBuildGun.h:244`,
base `AFGEquipment` (a **player-held item**). Hit-testing is pawn-driven:
`void TraceForBuilding(APawn* owningPawn, FHitResult& hitresult) const`
(`:267`) — not a free function usable without a pawn.

Construct flow: client's
`UFGBuildGunStateBuild::Server_ConstructHologram(FNetConstructionID, FConstructHologramMessage)`
RPC (`Source/FactoryGame/Public/Equipment/FGBuildGunBuild.h:208-209`,
`UFUNCTION(Server, Reliable)`) → `InternalConstructHologram(...)` (`:210`)
on the server. **No lower-level "class + transform + validate → actor"
API exists on the build gun or hologram path itself.**

## 3. Validation

`bool AFGHologram::CanConstruct() const` (`FGHologram.h:320`) — a plain
bool, but backed by a **class-based disqualifier list, not an enum**:
`void GetConstructDisqualifiers(TArray<TSubclassOf<UFGConstructDisqualifier>>&) const`
(`:399`). Roughly 70 concrete disqualifier subclasses exist in
`Source/FactoryGame/Public/FGConstructDisqualifier.h` (e.g.
`UFGCDEncroachingClearance`, `UFGCDInvalidFloor`,
`UFGCDNeedsResourceNode`, `UFGCDMustSnap`, `UFGCDWireTooLong`), each with
`mDisqfualifyingText` and an `mIsSoftDisqualifier` flag (soft = still
buildable). `IsValidHitResult(const FHitResult&) const` (`:152`) gates
whether the raw hit-test point is even usable. `CheckClearance()`,
`CheckValidPlacement()`, `CheckCanAfford()` are `protected virtual`s that
populate the disqualifier list (`:543,567,572`).

## 4. Construction registration/side effects

`AFGBuildableSubsystem::BeginSpawnBuildable(TSubclassOf<AFGBuildable> inClass, const FTransform&)`
(`FGBuildableSubsystem.h:284`, doc: *"you need to call FinishSpawning on
the buildable after this to finalize the spawning"*) — a **raw,
unvalidated spawn primitive**. `void AddBuildable(AFGBuildable*)`
(`:287`) registers into the same array read-only telemetry already relies
on (`GetAllBuildablesRef()`, see buildable-research.md §1); construction
broadcasts `mBuildableAddedDelegate`/`BuildableConstructedGlobalDelegate`
(`:555-560`).

`AFGBuildableHologram` documents a strict configure order
(`FGBuildableHologram.h:320-331`): `ConfigureActor` → `ConfigureBuildEffect`
→ spawn → `ConfigureComponents` (explicitly *"a good place to initialize
snapped connections"*) → `BeginPlay`.

Recipe state: `AFGBuildable::SetBuiltWithRecipe`/`GetBuiltWithRecipe`
(`FGBuildable.h:325,328,1086`). Save registration: `AFGBuildable`
implements `IFGSaveInterface`, and `AFGBuildableSubsystem` itself is
`IFGSaveInterface` (`FGBuildableSubsystem.h:253-256`) — save persistence
is subsystem-array-driven, so **a buildable never passed to `AddBuildable`
plausibly won't save.**

**A naive `BeginSpawnBuildable` + `FinishSpawning` shortcut would skip
all of:** clearance/disqualifier checks, connection-component
configuration (so it wouldn't connect to belts/pipes/power even if
physically adjacent), recipe assignment, and — unless called manually —
`AddBuildable` registration (so it might not even survive a save).

## 5. Server/client and authority

Client-initiated, server-authoritative (standard Unreal RPC pattern, not
purely server-only). `UFGBuildGunStateBuild::Server_ConstructHologram`
(`UFUNCTION(Server, Reliable)`) plus matching
`Client_OnBuildableConstructed`/`Client_OnRecipeBuilt`/`Client_OnBuildableFailedConstruction`
(`FGBuildGunBuild.h:317,321,328`) confirm the server executes and can
reject, with the client notified either way. `mConstructionInstigator`
(`FGHologram.h:815`, replicated `APawn*`) ties a hologram to a requesting
pawn — construction is conceptually attributed to a player.

## 6. Existing SML code

Zero relevant matches in `Mods/SML/Source/SML` for "Hologram",
"BuildGun", or "Construct" (18 hits on "Construct" were all unrelated —
C++ object construction, Blueprint hook codegen, config). Same pattern as
every prior research pass: nothing to build on.

## 7. Verdict

**There is no narrow, safe API for this**, unlike `SetRecipe`/
`SetClockSpeed` (Phase 12), which mutate state on an already-valid,
already-registered building. `BeginSpawnBuildable` + manual
`FinishSpawning` is exactly the "SpawnActor and assume it's valid"
pattern PLAN.md explicitly warns against — it has none of the safety
machinery (no clearance checks, no connection setup, no recipe
assignment, no save registration unless done by hand).

The real, validated path runs through `AFGBuildGun`/
`UFGBuildGunStateBuild`, built around a pawn-held equipment item doing
pawn-relative hit-tracing and a client→server RPC. Reaching a
correctly-configured `AFGBuildable` without a physical build gun means
either:

- **(a)** driving a real `AFGHologram` instance server-side through its
  full lifecycle (`IsValidHitResult` → placement → `CanConstruct` →
  `Construct`) — feasible without literal player-input simulation, since
  these are plain virtual calls on a spawned hologram, but still requires
  correctly reproducing clearance, snapping, connection, and cost logic
  most of which lives in `protected` methods not designed for external
  drivers; or
- **(b)** manually replicating `ConfigureActor`/`ConfigureComponents`/
  `AddBuildable`/save-registration by hand, with high risk of silently
  missing an undocumented step (there is no exhaustive list of what
  "correctly constructed" requires — only what was found by reading
  headers, which per the recurring caveat in this repo cannot be checked
  against real `.cpp` logic).

## Why this stops here

This confirms the concern raised when Phase 12 finished
(`docs/operations-protocol.md`'s closing section): building placement is
a substantially larger jump in both scope and risk than anything
implemented so far, and — unlike resource-node/buildable/manufacturer
enumeration or the two Phase 12 write operations — there is no
"read the header, call the documented function, validate inputs" path
available. Getting this wrong risks actually corrupting a save (an
improperly-registered or improperly-connected buildable), not just
returning a wrong telemetry value.

Combined with the fact that **nothing built in Phases 2-12 has been
runtime-verified against a real Satisfactory session yet** (see
[manual-verification.md](manual-verification.md) — every item is still
"Pending"), implementing Phase 13 now would mean building the riskiest
capability yet on a completely unverified foundation. Per PLAN.md's own
"Read Before Write" principle and CLAUDE.md's building-placement
guidance, this is the point to stop and get human input before writing
any placement code — not because a rule was blindly followed, but
because the actual evidence gathered here (§7) shows there's a real,
substantive design decision to make (approach (a) vs (b) above, or
deferring further) rather than a mechanical next step.
