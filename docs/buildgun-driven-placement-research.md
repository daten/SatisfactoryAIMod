# Build-gun-driven placement research — after standalone hologram hit a wall

Recorded 2026-08-25. After five independently-falsified hypotheses for
why a standalone `AFGHologram` (spawned via `SpawnHologramFromRecipe`,
no real `AFGBuildGun` involved) always reports a hard `UFGCDInitializing`
disqualifier — see `extractor-placement-research.md`'s "will not clear"
section — this researches driving the *real* build gun flow instead.

An initial pass was delegated to a research agent; every claim below was
then **independently re-verified directly against the headers** before
being trusted (per this project's established practice of not trusting
delegated research for anything load-bearing) — one correction was found
in the process (`mBuildGun`'s access level), noted inline.

## 1. No need to spawn/equip a new build gun — every character already has one

`AFGCharacterPlayer::GetBuildGun() const` (`FGCharacterPlayer.h:560`,
`FORCEINLINE`, `BlueprintPure`, **public**) returns `mBuildGun`
(`FGCharacterPlayer.h:1724`, `TObjectPtr<AFGBuildGun>`, **`protected`**,
not `private` as the delegated pass initially reported — verified
directly). No public setter exists for `mBuildGun`, and the build gun
has no slot in the generic `EEquipmentSlot` inventory-equip system —
confirms every character is spawned with its own build gun already
present, just not necessarily *equipped* (active in-hand).

**Single entry point that does everything:**
`AFGCharacterPlayer::HotKeyRecipe(TSubclassOf<UFGRecipe> recipe)`
(`FGCharacterPlayer.h:601`, `UFUNCTION(BlueprintCallable)`, **public**),
doc comment: *"Instantly goes to build mode for selected recipe, If
already set we unequip the build gun."* This is a real, ordinary
player-facing hotkey feature (recipe hotkeys), not something invented
for this experiment — calling it should equip the build gun (if not
already) and drive it into `BGS_BUILD` state with the given recipe
active, in one call. **Caveat, verified from the doc comment itself**:
it's a *toggle* — calling it again while already in that exact build
mode unequips instead. Not yet verified whether calling it while a
*different* mode/recipe is active correctly switches over first.

## 2. Retrieving the active build state and its hologram

- `AFGBuildGun::GetCurrentState() const` (`FGBuildGun.h:317`,
  `FORCEINLINE`, **public**) → `UFGBuildGunState*`.
- `AFGBuildGun::GetBuildGunStateFor(EBuildGunState gunState) const`
  (`:291`, `FORCEINLINE`, **public**) → looks up a specific state
  (e.g. `EBuildGunState::BGS_BUILD`) without needing it to be the
  currently active one.
- The templated convenience `GetState<C>(EBuildGunState) const`
  (`:528-529`) is **`private`** (verified directly) — not usable
  externally; use `Cast<UFGBuildGunStateBuild>(GetBuildGunStateFor(...))`
  instead, same result via a public path.
- `UFGBuildGunStateBuild::GetHologram() const` (`FGBuildGunBuild.h:205`,
  `BlueprintPure`, **public**) → the actual spawned `AFGHologram*`, or
  null if none.

## 3. Feeding a synthetic hit result into the real flow

`AFGBuildGun::GetHitResult()` (`FGBuildGun.h:271`, `FORCEINLINE`,
**public**) returns `FHitResult&` — a **mutable reference**, not a
const getter, to the build gun's own cached `mHitResult`
(`:609`, `private`). This means external code can write directly into
it: `BuildGun->GetHitResult() = MySyntheticHit;` — bypassing
`TraceForBuilding()`'s real camera trace entirely, without touching any
private member.

**Not yet verified**: whether `UFGBuildGunStateBuild::TickState_Implementation`
actually reads this each tick and forwards it to
`Hologram->UpdateHologramPlacement()` — this is architecturally the only
sensible reason `GetHitResult()` would be a mutable public reference
rather than a const getter, but the call site itself lives in a stub
`.cpp` in this repo and can't be directly confirmed. This is the next
thing a live test will answer.

## 4. `SpawnHologram`/`InternalSpawnHologram` are genuinely private — confirmed

`FGBuildGunBuild.h:330` (`private:`) through `:340` covers `SpawnHologram()`,
`RemoveHologram()`, `CleanupHologramClearanceDetection()`, and
`InternalSpawnHologram()` — verified directly, no `friend` declarations
exist for these. A derived class cannot reach them; `HotKeyRecipe()` (§1)
is the only available way to trigger correct hologram spawning through
this class.

## 5. `Server_ConstructHologram` / `InternalConstructHologram` / `FConstructHologramMessage`

Both `Server_ConstructHologram(FNetConstructionID, FConstructHologramMessage)`
(`FGBuildGunBuild.h:209`, `UFUNCTION(Server, Reliable)`) and
`InternalConstructHologram(FNetConstructionID)` (`:210`) are **public** —
verified directly. `InternalConstructHologram` being public is notable:
it's the actual server-side execution function the RPC delegates to,
directly callable without going through RPC serialization, *provided*
the state object already has a valid hologram set up.

`FConstructHologramMessage` (`FGConstructionMessageInterface.h:14-52`
per the delegated pass; not yet independently re-verified line-by-line,
lower confidence than the items above) carries `ConstructionID`,
`Recipe`, `UpgradeActor`, `LightweightUpgradeClass`,
`LightweightUpgradeRuntimeIndex`, `SerializedHologramData` (`TArray<uint8>`),
and `NumBits` — the serialized-blob fields strongly suggest real
construction replays a full serialized hologram state
(`AFGHologram::SerializeConstructMessage`, `FGHologram.h:126`,
`IFGConstructionMessageInterface`), which a standalone hologram's
`Construct()` call was never going to populate correctly - a plausible
contributing reason `docs/extractor-placement-research.md`'s standalone
approach never got past `CanConstruct()`.

**Not planned for this pass.** Even if the build-gun-driven approach
gets `CanConstruct()` to return `true`, this experiment stops there
(mirroring the existing dry-run safety posture) rather than also calling
`Server_ConstructHologram`/`InternalConstructHologram` - that's a
separate, later step once `CanConstruct()==true` is actually observed
live.

## 6. Real, visible side effect this experiment has that the standalone one didn't

Unlike the standalone `SpawnHologramFromRecipe` approach (fully headless,
invisible to the player except the brief hologram appearing/disappearing
in the world), driving the real build gun **visibly changes what the
player has equipped** — `HotKeyRecipe()` puts the character's hands into
build mode with the build gun out, replacing whatever they were
previously holding, and shows the normal in-game build-mode HUD/hologram
UI. `Character->UnequipBuildGun()` (`FGCharacterPlayer.h:593`, public)
is called afterward to restore the player's prior state, but this is a
real UI interruption during the test, not a backend-only operation like
everything built so far in this repo.

## 7. `UFGCDInitializing` — re-searched repo-wide, still no additional reference

Confirmed again (delegated pass + independently spot-checked): the class
declaration/constructor at `FGConstructDisqualifier.h:49-57` is the only
reference anywhere in `Source/FactoryGame`, `.cpp` stubs included. No
`IsInitializing`/`bInitializing`-style flag exists anywhere in
`Source/FactoryGame/Public`. Circumstantial-only lead: `UFGBuildGunStateBuild`
maintains its own separate clearance-detector-overlap delegate system
(`OnClearanceDetectorAdded`, `BeginClearanceDetectorOverlap`/
`EndClearanceDetectorOverlap`, `UpdateClearanceData()` —
`FGBuildGunBuild.h:266,345,348,440`) with no equivalent on `AFGHologram`
itself — real, header-confirmed machinery that only exists when a
hologram is driven through a build gun state, but whether it's what
actually clears `UFGCDInitializing` remains unconfirmed until tested
live.

## Plan for the next code change

Add a new, separate debug function (leave the existing
`DebugCheckExtractorPlacementOnTargetedNode` untouched as a reference/
fallback): call `HotKeyRecipe(Recipe_MinerMk1)`, retrieve the resulting
`UFGBuildGunStateBuild`/hologram via the public accessors in §2, feed
the synthetic hit result via `GetHitResult()` (§3), poll real ticks
exactly as before, log `CanConstruct()`/disqualifiers, then always call
`UnequipBuildGun()` to restore the player's UI regardless of outcome.
Still stops before any real `Construct()`/`Server_ConstructHologram`
call - same safety posture as every prior experiment in this repo.

## Result: confirmed live, first try (2026-08-24)

`DebugCheckExtractorPlacementViaBuildGun` (commit `8286bd6b24`) worked
on the first real test, twice in a row: `canConstruct=true
disqualifiers=[<none>]`, resolved after **1 real tick** both times —
not the 120-tick safety cap the standalone experiment always hit. A
`LogBuildGun: Warning: ...GetHologramCost failed cause no hologram
spawned.` line appears in the log at the very start of each run — this
is a UI widget (`Widget_BuildMode`) querying the hologram cost in the
same frame `HotKeyRecipe` is called, before the hologram exists yet;
harmless, and resolved by the time our own poll checks a tick later.

**This confirms the core hypothesis**: something in the real
`AFGBuildGun`/`HotKeyRecipe`/`UFGBuildGunStateBuild` flow does what a
standalone `AFGHologram::SpawnHologramFromRecipe()`-spawned hologram
never got — `UFGCDInitializing` was never a timing issue at all (five
falsified hypotheses on the standalone path all made sense in
retrospect: no amount of waiting fixes a structural gap). The exact
mechanism is still unconfirmed (candidates from §7: the build-gun-only
clearance-detector-overlap delegate system, or something in
`BeginState_Implementation`/`TickState_Implementation`'s stub-hidden
logic), but it no longer matters *which* mechanism — driving the real
build gun is a working, confirmed path.

## First real building placement — confirmed live (2026-08-24)

`ConstructExtractorOnTargetedNode` (commit `ae7a6c0799`) — the same
validated flow as the dry-run, but calling
`UFGBuildGunStateBuild::InternalConstructHologram()` once `CanConstruct()`
genuinely resolves `true` — **worked on the first real test**:

```
ConstructExtractorOnTargetedNode (deferred, resolved after 1 real tick(s)):
construction attempted via InternalConstructHologram -
node=.../BP_ResourceNode571 nodeNowOccupied=true
```

The log immediately after shows the real `SK_MinerMk1` skeletal mesh
loading and the game's own build-effect system running
(`[BuildEffect]`/`LogSkeletalMesh` lines) — independent confirmation a
genuine `Build_MinerMk1` buildable now exists, not just a flag flip.
(The `[BuildEffect] Failed to find remap material`/missing
`bUsedWithSkeletalMesh` warnings that follow are a pre-existing
base-game material-compilation quirk — the engine's own log says it
recompiles on next editor launch — not something this mod caused; any
real player construction of a Miner Mk1 would trigger the same
warnings.)

**PLAN.md Phase 13's central question is answered**: an
`AFGResourceExtractorHologram` can be driven correctly, headlessly (no
real human pressing keys), to place a real, correctly-registered
building — via the real `AFGBuildGun`/`HotKeyRecipe` flow, not the
standalone `SpawnHologramFromRecipe` path. `InternalConstructHologram()`
being callable directly (§5) meant no RPC/`FConstructHologramMessage`
serialization was ever needed.

**Not yet verified**: whether the constructed Miner survives a save/
reload, whether it correctly appears in `world.buildables`/
`world.connections` telemetry, and whether it's genuinely producing (not
just occupying the node) — these are the natural next verification
steps, not further placement-mechanism risk.

## §3 correction: `GetHitResult()` alone does not control final placement

`ConstructBuildingNearPlayer`'s generalization to arbitrary near-player
positions (not resource-node-anchored) surfaced a real gap §3 above
missed: setting `AFGBuildGun::GetHitResult()` once, before spawning the
hologram, is **not sufficient** to control where construction actually
happens.

**Confirmed live (2026-08-24), a diagnostic added specifically to test
this**: on every one of three test runs, the build gun's `GetHitResult()`
had changed by the time the poll checked it one real tick later — in one
case by ~4000 units (40m), including a ~4400-unit altitude jump on the
run that then failed with "Surface is too uneven!" at a location nowhere
near the computed candidate. `UFGBuildGunStateBuild::TickState_Implementation`
almost certainly runs its own real `AFGBuildGun::TraceForBuilding()`
every tick (per its declaration, `FGBuildGun.h:267`, matching a normal
player's live crosshair-follows-camera behavior) and overwrites
`GetHitResult()` with the player's actual real-time aim, silently
discarding whatever was set beforehand. The user independently confirmed
the two earlier "successful" placements had genuinely matched where they
were aiming at the moment - meaning the computed candidate position was
never actually what got built; the real trace and the intended target
had simply coincided.

**Fix**: all three build-gun-driven functions
(`DebugCheckExtractorPlacementViaBuildGun`, `ConstructExtractorOnTargetedNode`,
`ConstructBuildingNearPlayer`) now store the synthetic `FHitResult` in
their poll state and call `AFGHologram::UpdateHologramPlacement()`
directly with it on **every poll tick**, immediately before checking
`CanConstruct()`/constructing — re-asserting the intended position after
whatever the build gun's own trace did that tick. This makes placement
deterministic regardless of where the player's camera happens to be
pointed, which is required for genuine API-driven placement (the
eventual goal is no human aiming at anything at all). **Not yet
re-verified live** — the fix compiled clean but hasn't been tested
against real gameplay yet.
