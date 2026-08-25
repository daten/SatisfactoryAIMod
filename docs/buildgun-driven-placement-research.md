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
eventual goal is no human aiming at anything at all).

**Confirmed live (2026-08-25).** `ConstructBuildingNearPlayer` with
Constructor Mk1: ground-trace candidate `X=-54853.050 Y=160796.653
Z=3337.826`, actual constructed location `X=-54853.103 Y=160796.700
Z=3341.401` - matching to within a unit. Every pre-fix test diverged by
hundreds to thousands of units (up to ~4000 units/40m), so a near-exact
match on the first post-fix test is strong confirmation the
`UpdateHologramPlacement()` override is winning against the build gun's
own per-tick trace as intended. Placement is now genuinely
API-controlled, not dependent on where the player happens to be looking.

## §4 Rotation control (`rotationScrollDelta`) — investigation notes, 2026-08-25

Added to unblock the demo's Constructor-orientation problem (see
`docs/demo-production-chain.md`), then needed real live calibration
since `AFGHologram::Scroll()`/`ScrollRotate()`/`SetScrollRotateValue()`/
`GetRotationStep()` are all stub bodies in this installed SDK - no
source to read the real degrees-per-call relationship from.

**Not camera-direction-dependent** - ruled out directly. An early
calibration batch got stuck with `"Invalid aim location!"` even at
`rotationScrollDelta=0` (the prior, previously-working no-rotation
behavior), which looked like it might depend on the player's actual
camera look direction (unqueryable - `world.player` reads
`AFGCharacterPlayer::GetActorRotation()`, the capsule/movement yaw, not
the camera). Root cause turned out to be mundane: the test coordinates
were near/on the player's own elevated position on ground distinct from
the actual flat construction platform (~97 units of Z difference) -
retesting at genuine flat-platform coordinates got past aim validation
into ordinary clearance failures instead. No camera dependency exists
for aim-location validity as far as this investigation went.

**A single `Scroll(N)` call with `|N|>1` does not behave like N
individual scroll events.** Live evidence: `rotationScrollDelta=1,2,3`
in one call each all produced the identical resolved yaw, and
`rotationScrollDelta=-1,-2` produced the *same positive* result as
`+1,+2` rather than a negative rotation - the signature of a per-call
clamped/smoothed input handler built for one mouse-wheel notch per
call. **Fixed** in `ConstructBuildingAtPosition` by calling
`Hologram->Scroll(sign)` in a loop `|RotationScrollDelta|` times
instead of `Scroll(RotationScrollDelta)` once - matches how real wheel
input actually arrives.

**Base orientation is context/terrain-dependent, not a fixed default -
and NOT simply "= player's current facing" either**, despite one early
sample (`rotationScrollDelta=0` -> resolved yaw exactly matching the
player's yaw at the time) suggesting that. A later test on open terrain
placed a Constructor at yaw=-80° while the player was facing yaw=-177°
(~97° apart) - something beyond raw player facing (very likely local
ground contour/slope) also feeds into the hologram's default
orientation on unconstrained terrain.

**Foundation-adjacent vs. raw-terrain rotation behavior differs
sharply - confirmed by the user's own interactive placement
experience, not just RPC testing:** on a foundation, rotation appears
to hard-snap to a 90°-multiple grid; on raw terrain, finer rotation is
possible but the valid range is narrow and terrain-dependent (ground
slope constrains which orientations keep the footing valid at all).
Live data matches: a 12-point ring of test placements 1500 units from
the player, one `rotationScrollDelta` each (0 through 12, and -1
through -4), only `0` and `1` succeeded - all others failed
`"Invalid aim location!"` even though each point was a distinct,
well-separated location (not a repeat-clearance artifact). The two
successes gave a **clean, real calibration point: `Scroll(1)` ≈ +10°**
(`delta=0` -> yaw=180°, `delta=1` -> yaw=-170°, i.e. `180+10=190≡-170
(mod 360)`), but this was only reproducible within a narrow window at
that specific spot - not confirmed as a universal constant, since a
foundation-adjacent test earlier the same session showed `delta=1,2`
both snapping back to the SAME 0° (implying a coarser/different
snapping rule applies near foundations, consistent with the user's
"snaps to 90° intervals on foundations" observation).

**Practical takeaway for future placement work:** prefer building on
foundations when precise/intentional orientation matters - raw terrain
introduces slope-dependent unpredictability in both placement validity
*and* rotation tolerance, matching why CLAUDE.md's Building Placement
section emphasizes validated construction over raw actor spawning in
the first place. A full closed-form `rotationScrollDelta`-to-degrees
formula was not reached and is not assumed reliable - treat any
specific delta as a candidate to verify via telemetry
(`world.buildables`' `rotation.yaw`) after placement, not a
precomputed guarantee.

**Caution when testing:** an early test in this investigation placed a
building directly at the player's own X/Y, trapping them momentarily -
subsequent candidate positions for any multi-point sweep should be
offset well clear of the player's current position (a ring at a fixed
radius, one point per candidate, is what worked here) rather than
reusing the player's exact coordinates.

## §5 "Invalid aim location!" can persist at ~100 units, independent of distance/yaw - camera pitch/yaw, not capsule facing, 2026-08-25

A second, real Smelter/Constructor/foundation build near an existing
Miner (`docs/lightweight-buildable-research.md` has the foundation
side of this session) repeatedly hit `"Invalid aim location!"` even
after every previously-understood cause was ruled out one at a time,
live:

- Not distance: a 7-point radial sweep (400 to 2200 units, along the
  bearing toward the actual target) failed at **every** distance,
  including the closest.
- Not ground validity: `groundTraceHit=true` with a sane Z at every
  failing attempt (confirmed via the per-attempt `ConstructBuildingAtPosition:
  recipe=... groundTraceHit=...` log line).
- Not horizontal capsule yaw: a further sweep placed candidates
  directly along `Character->GetActorRotation()`'s own yaw (via
  `world.player`) at 100/200/300 units - all three still failed, with
  the player stationary the whole time (user had stepped away from the
  keyboard/mouse, so this ruled out both distance AND yaw-alignment
  changing between attempts).
- Not leftover debris/duplicate objects at the target (checked via
  `world.buildables` proximity query - clean).
- Not a stuck build-gun/cooldown state: an 8-second real-time wait
  before retrying made no difference.

**Working theory (implemented, not yet live-verified - needs an
Alpakit redeploy):** `world.player`/`AFGCharacterPlayer::GetActorRotation()`
only reflects the character **capsule's** yaw, never pitch, and this
game decouples camera look direction from capsule facing (as most
third-person-capable characters do) - if the player's actual camera
happens to be pointed somewhere unrelated to the capsule's forward
vector (e.g. looking down at inventory, or simply however it was left
when the user stepped away), some disqualifier apparently still
consults the real controller `ControlRotation` independent of
`UpdateHologramPlacement()`'s per-tick position override - the same
class of bug as this doc's §3 finding (`GetHitResult()` alone doesn't
control final placement), just one layer deeper: overriding the
hologram's *position* isn't enough if a disqualifier separately checks
the *camera's* direction.

**Fix, applied in `ConstructBuildingAtPosition`:** before
`HotKeyRecipe()`, set `Character->GetController()->SetControlRotation()`
to face the synthetic hit location - `(SyntheticHit.Location -
Character->GetActorLocation()).Rotation()`. This is a real, visible
side effect (snaps the player's camera to face the target, same
category as `HotKeyRecipe()`'s existing visible side effect) but makes
placement work regardless of where the camera actually happens to be
aimed - the whole point of RPC-driven placement being independent of
player attention. **Not yet confirmed live** - written and compiled
while the user had stepped away and redeploying needs them at Alpakit;
next session should verify this actually resolves the failures
reproduced above before trusting it.

## §6 Named, scoped bypasses for UX-only disqualifiers - 2026-08-25

Explicit user direction after §5: player-proximity/camera-direction
gates fundamentally don't scale for the long-term goal (large,
autonomous, multi-building layouts with no human in the loop) - other
mods are known to disable equivalent barriers, and the user explicitly
accepts the risk of invalid terrain collisions in exchange for not
being gated on player position/aim at all.

`ConstructBuildingAtPosition`/`world.placeBuilding` gained four
independent opt-in flags, all defaulting to `false` (today's strict
behavior preserved): `ignoreAimLocation`, `ignorePlayerEncroachment`,
`ignoreClearance`, `ignoreInvalidFloor` - mapping to the real
FactoryGame disqualifier classes `UFGCDInvalidAimLocation`,
`UFGCDEncroachingPlayer`, `UFGCDEncroachingClearance`,
`UFGCDInvalidFloor` (found via `FGConstructDisqualifier.h`, which lists
~70 disqualifier classes total - the full catalog for future scoped
bypasses if needed). Deliberately narrow, named flags per class - not
a generic "ignore everything" switch - every other disqualifier
(structural validity, resource requirements, snap-to requirements,
etc.) still applies.

**Implementation note:** `AFGHologram::CanConstruct()`'s real logic is
unreadable (stub source, like most of this SDK) and can't be
selectively overridden from outside the class - so DocMod no longer
calls it at all for this function. Instead it replicates the
documented "any non-soft (hard) disqualifier blocks construction" rule
directly, using the same `UFGConstructDisqualifier::GetIsSoftDisqualifier()`
static query already used for logging everywhere else in this file,
skipping whichever classes the caller opted to ignore. This still
calls the real `InternalConstructHologram()` to actually build -
whatever validation FactoryGame performs server-side inside that
function, if any, is unverified from source and NOT bypassed by these
flags; they only change DocMod's own decision about whether to attempt
construction. **Not yet live-tested** - same redeploy dependency as §5.
