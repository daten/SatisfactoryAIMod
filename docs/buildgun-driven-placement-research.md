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
