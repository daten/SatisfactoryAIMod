# Splitter port control test (2026-08-30)

User-specified rigorous test proving deterministic control over splitter
port selection before any manifold/production/factory-layout code is
allowed to depend on it again. Full original spec preserved in session
history; this doc tracks real results against it.

Toolkit: `controller/satisfactory_ai/splitters.py` (pure functions over
already-fetched `world.connections` data - classifies each connector's
real cardinal direction from its real world normal, and exposes
`get_splitter_input`/`get_splitter_output_facing` with explicit,
typed errors, never guessing/nearest/first-match/index-based selection).

## Cardinal convention (this project's own, arbitrary but consistent)

`+Y = NORTH, -Y = SOUTH, +X = EAST, -X = WEST` - the game world has no
real compass; what matters is using the same convention everywhere.

## Phases 1-3: single-splitter rotation table — PASS, real data

Placed one splitter (`Recipe_ConveyorAttachmentSplitter`) on a foundation
at a known position, rotated it through yaw 0/90/180/270 (delete +
re-place, no in-place rotate RPC exists), inspected real
`world.connections` data at each rotation:

```
Yaw | Input | Output | Output | Output
----|-------|--------|--------|-------
0   | WEST  | EAST   | SOUTH  | NORTH
90  | SOUTH | NORTH  | EAST   | WEST
180 | EAST  | WEST   | NORTH  | SOUTH
270 | NORTH | SOUTH  | WEST   | EAST
```

Invariant confirmed at every rotation: exactly 1 Input + 3 Outputs, 4
unique cardinal directions. Each +90° yaw step shifts the input
direction by exactly one consistent step in the cycle
WEST→SOUTH→EAST→NORTH→WEST - fully predictable, not just "different
each time."

## Phase 4 + partial Phase 10: semantic selection + negative tests — PASS

Against the yaw=270 splitter (Input=NORTH, Outputs=SOUTH/WEST/EAST):
- `get_splitter_input()` → NORTH ✓
- `get_splitter_output_facing()` for SOUTH/WEST/EAST → each resolved to
  the correct real connector ✓
- Negative test: `get_splitter_output_facing("NORTH")` (the Input side)
  → `ERROR_NO_OUTPUT_IN_DIRECTION`, correctly refused rather than
  silently substituting a different output ✓
- Negative test: `get_splitter_output_facing("UP")` (invalid direction)
  → `INVALID_DIRECTION` ✓

## Real gap found and fixed before Phase 5 could mean anything

`world.connectConveyor` previously had NO way to target a specific
connector - it always used `FindFreeFactoryConnection`, which returns
"the first free connector of the requested Direction" in component-array
order, regardless of which cardinal direction it faces. This meant the
Python-side semantic selection (`get_splitter_output_facing`) could
correctly DETERMINE which port was wanted, but the actual C++ call had
no way to USE that specific one over the other two free outputs - a real
violation of the test spec's core requirement ("Do not use: nearest
connection, first connection, available connection, connection[0]").

**Fixed** (compiled 2026-08-30, not yet live-tested): `world.connectConveyor`
now accepts optional `sourceConnectorPosition`/`destConnectorPosition`
(`{x,y,z}`, real world coordinates read from a prior `world.connections`
call). When given, `FindFreeFactoryConnectionNear` (new C++ helper)
selects the free connector of the right direction within ~10cm of that
exact position - erroring `NO_FACTORY_CONNECTION` rather than falling
back to any other free connector if nothing matches closely enough.
Omitting both params keeps the old "first free" behavior for every
existing caller (manifold-building scripts etc.) unchanged.

## Phase 5 live-tested 2026-08-30 — connector targeting CONFIRMED CORRECT, but blocked by a separate, pre-existing reliability issue

Redeployed and ran Phase 5 live. Good news first: **the position-targeting
fix itself works exactly as designed** - `sourceConnectorPosition`/
`destConnectorPosition` reliably resolve to the exact real connector
requested every time (confirmed via `ConstructConveyorBelt diagnostic`
log lines matching the requested coordinates precisely on every
attempt), and the connector resolution step never once failed with
`NO_FACTORY_CONNECTION`. Phase 4's semantic selection correctly drives
Phase 5's connector choice end to end.

**But belt construction itself is unreliable in a way unrelated to
connector selection** - discovered while diagnosing this:

1. First attempt (2000cm apart) failed `CANNOT_CONSTRUCT "Conveyor Belt
   is too long!"` - reasonable at first glance, except `world.
   conveyorBeltTiers` confirms Mk1's real `maxSplineLength` is 5600cm,
   so a straight 2000cm run should never trigger this. Retried at
   800cm, then 600cm (identical spacing to several belts that connected
   fine earlier this session in the compact manifold work) - same
   "too long" failure every time.
2. Isolated whether this was caused by the NEW position-targeting code:
   ran the identical connection with `sourceConnectorPosition`/
   `destConnectorPosition` OMITTED (old "first free connector" behavior)
   - failed identically. **Rules out the new code as the cause.**
3. The player character was standing ~2500cm away from the test range
   (idle/AFK, not actively controlled) during all of the above. Built a
   fresh, identical 600cm-apart pair immediately next to the player's
   actual position instead - the very next attempt reported
   `success:true`, and `world.connections` confirmed the SOURCE end
   genuinely connected (not just a dangling stub this time).
4. But the DESTINATION end was still open (the known "reports
   success:true but one end dangles" pattern from earlier this session).
   8 further retry attempts (cleanup-and-retry loop, all 3 routeModes),
   still right next to the player, **all failed identically with "too
   long" again** - the one partial success looks like it may have been
   a fluke (e.g. a stale conveyor pole leftover from an earlier
   duplicate-S2 cleanup mistake was in the immediate area for that one
   attempt) rather than proof that proximity alone is the deciding
   factor.

**RETRACTED** (2026-08-30, later same day): the AFK/distance hypothesis
above was wrong, and was rejected by the user as insufficiently
rigorous before being investigated further - correctly, as it turned
out. Re-investigation (below) found the real, mundane cause, with zero
remaining mystery.

## Root cause conclusively found: test-script cleanup bug, not a game/mod bug

Rigorous re-investigation, in order:

1. Tested a brand-new never-touched location - failed identically to
   the "bad" location. Rules out location-specific residue.
2. Line-by-line `git diff` of `ConstructConveyorBelt_RealCharacterStrategy`
   against the last known-good commit - zero logic changes, only the
   documented connector-position additions. Rules out a code regression.
3. Re-tested the exact OLD compact-manifold location that had worked
   15/15 times earlier this session - it succeeded again, genuinely
   (`world.connections` confirmed both ends wired), even though the
   player was *farther away* from it than from the "bad" location.
   Directly contradicts distance/AFK.
4. A 5-point bisection between the known-good and known-bad locations
   produced a non-monotonic pattern - including the "known-bad" control
   point, which had failed 8/8 times earlier this same session,
   succeeding this time. Not explainable by geography at all.
5. A tightly controlled repeated-trials test (same exact spot, same
   inputs, 10 attempts) produced a **perfect alternating pattern**:
   succeed/fail/succeed/fail, 5/10, both with a 0.5s gap and again with
   a 6s gap between attempts (ruling out timing/GC races - a real race
   would be flaky, not perfectly parity-locked).
6. That parity-lock pointed at leftover state from the test loop itself
   rather than the game engine. Checked `world.buildables` near the
   test spots: **`world.deleteBuilding` was only ever called on the two
   splitters, never on the belt `connectConveyor` created between
   them** - so every trial (success or failure) left an orphaned belt
   actor stacked at the exact same coordinates. 10 overlapping belts had
   piled up at one test spot, 8 at another, matching the trial counts
   exactly. This is *correct*, faithful-to-the-real-game behavior for
   `world.deleteBuilding` (dismantling a machine doesn't cascade-delete
   belts touching it in vanilla Satisfactory either) - the bug was in my
   own Python cleanup scripts forgetting to also delete the belt.
7. Confirmation: re-ran the repeated-trials test deleting *every*
   buildable near the test spot (splitters + belt) between attempts -
   **10/10 genuinely connected**, zero failures.

The accumulated overlapping belt geometry from previous
attempts/session-long testing in the same small area is exactly what
was intermittently tripping "Overlapping another object's clearance...
Conveyor Belt is too long!" - not distance, not AFK, not a race
condition, not player-controller state, and not a mod bug at all. Every
finding in this doc's earlier sections that leaned on "player must be
nearby/active" is superseded by this - connector-targeted belt
construction is reliable regardless of player distance/AFK state, as
long as the destination site is actually clear.

## Second, real C++ bug found while attempting Phases 6-9 (2026-08-30, same day)

Built `controller/splitter_matrix_test.py` (a persistent, real physical
test-range generator - see its own module docstring) to run the 12-case
(Phase 6/7) and 48-case (Phase 8) matrices, with Phase 9 verification
built into every case. Early runs at freshly-chosen locations
(including a genuinely flat, `world.terrainHeightGrid`-confirmed area,
and later a location elevated well above all nearby terrain) failed
repeatedly with "Conveyor Belt is too long!" / "Surface is too uneven!"
- initially looking like another distance/geometry problem.

**Sanity check that broke the geometry hypothesis**: re-ran the exact
known-good compact-manifold coordinates (`-74073,109300`, proven 15/15+
earlier) and it now ALSO failed with "Surface is too uneven!". A
location that had never failed all session suddenly failing ruled out
geometry/distance/location entirely - something global had changed.

**Real root cause, found by re-reading this project's own earlier
research rather than re-deriving it**:
`docs/buildgun-driven-placement-research.md`'s "§3 correction"
(2026-08-24/25) had already documented and fixed this exact class of
bug for `ConstructBuildingNearPlayer`/`ConstructExtractorOnTargetedNode`:
`UFGBuildGunStateBuild::TickState_Implementation` runs its own real
`AFGBuildGun::TraceForBuilding()` every tick, from the REAL player's
live camera aim, silently overwriting whatever hit-result/placement the
mod set up beforehand (confirmed there via a ~4000-unit position drift
and a "Surface is too uneven!" failure nowhere near the intended
location). The documented fix: re-assert
`AFGHologram::UpdateHologramPlacement()` with the intended synthetic hit
on **every poll tick**, not just once.

**`ConstructConveyorBelt_RealCharacterStrategy`'s poll loop never got
this fix** - it already re-asserts `SetControlRotation` every tick (for
the unrelated camera-hijack workaround) but never re-asserted the
hologram's end-hit placement, so on any tick where the real player's
live camera trace lands somewhere else (rough terrain, empty space,
wherever they happen to be looking/AFK-idle-facing), the belt
hologram's internal placement can silently drift away from the actual
requested connector before `GetConstructDisqualifiers()`/
`InternalConstructHologram()` run - explaining every one of today's
"intermittent" failures without needing distance, AFK, or leftover-
geometry theories. (The orphaned-belt-clutter finding above is still
real and still fixed the specific alternating-pattern test that found
it - this is a second, independent contributing bug, not a
contradiction of that one.)

**Fix applied** (`AIModFunctionLibrary.cpp`, `ConstructConveyorBelt_RealCharacterStrategy`):
stores the computed `EndHit` in `FPollState` and calls
`PollHologram->UpdateHologramPlacement(PollState->EndHit)` every poll
tick, immediately before checking disqualifiers - the same pattern
already proven for point holograms, now applied to this spline
hologram. Rebuild triggered; **not yet live-verified** - next step is
re-running `controller/splitter_matrix_test.py` after redeploy to
confirm this actually resolves the matrix-test failures before treating
Phases 6-9 as unblocked.

## Phases 6-9 PASS, 48/48, live-verified (2026-08-30, after redeploy)

Redeployed with the `UpdateHologramPlacement` poll-tick fix. Ran
`controller/splitter_matrix_test.py --dest-yaws 0` (12 cases): **12/12
PASS** on the first clean attempt with a 5000cm placement offset
(bumped from an initial 3500cm - see the script's own module docstring
for the live-confirmed geometry reasoning). This alone confirms the fix
resolved the real bug: every one of today's earlier "Conveyor Belt is
too long!"/"Surface is too uneven!" mysteries, including a previously
bulletproof control location suddenly failing, never recurred once the
hologram's placement was re-asserted every poll tick.

Extending to the full 48-case matrix (`--dest-yaws 90,180,270`, the
remaining 36 cases) found two more real issues, both since resolved:

1. **3 cases hit `"Missing materials!"`** - genuine resource exhaustion
   from the volume of testing this session, not a bug. User enabled
   "Unlimited Resources" in AIMod's mod settings; resolved immediately
   (also flagged by the user as a separate concern - a possible refund
   bug elsewhere - to investigate later, not blocking this test).
2. **6 more cases (plus the 3 materials ones after enabling unlimited
   resources) showed the "dangling belt" pattern**: source output
   genuinely connects, destination input never does, `success:true`
   from `connectConveyor` notwithstanding. All 9 shared a real pattern -
   every one used a NORTH or SOUTH selected output direction. Verified
   the geometry itself wasn't at fault: reproducing one failing case's
   exact parameters in complete isolation, far from the test grid,
   succeeded cleanly with both ends genuinely wired. That isolated the
   cause to something specific to the grid layout itself.

   **Real root cause: a bug in the test script's own cleanup, not the
   mod.** `cleanup_cell()` (in `splitter_matrix_test.py`) checked X/Z
   within a `radius` but Y within a hardcoded `4000` - smaller than the
   5000cm `OUTPUT_OFFSET` used for NORTH/SOUTH-direction cases (whose
   destination splitter sits 5000cm away in Y). EAST/WEST cases offset
   along X instead, which the radius check covered correctly - so this
   only ever hit NORTH/SOUTH cases, matching the observed pattern
   exactly. Every retry after a NORTH/SOUTH failure left the old,
   already-half-connected destination splitter (and its belt) standing,
   and the next attempt placed a fresh duplicate splitter nearby -
   `world.buildables` confirmed up to 31 stacked buildables at one cell
   after repeated retries. Fixed the radius check to be symmetric
   (`abs(dy) < radius`, not `< 4000`), manually swept 175 leftover
   duplicate buildables across the 9 affected cells, and re-ran: **9/9
   PASS, all on the first attempt.**

**Final result: 48/48 cases PASS** - the full Phase 8 exhaustive
cardinal matrix (4 source rotations x 3 outputs x 4 destination
rotations), every one live-constructed, and every one Phase-9-verified
by re-fetching `world.connections` afterward and confirming the exact
requested source-output and destination-input connectors both point at
the same real belt buildable (not just "success:true", and not just
"S1 connects to S2 somehow" - the exact connector pair). The physical
test range is left standing in-game (not cleaned up), per the spec's
own requirement that every case remain separately visible.

## Status / next steps

- Phases 1-9: **PASS**, real live data throughout, no guessing, no
  index/nearest/first-match selection anywhere in the chain.
- Two real, independent C++/test-script bugs found and fixed along the
  way (the hologram-placement-drift fix in `ConstructConveyorBelt_RealCharacterStrategy`,
  and the cleanup-radius bug in the test harness) - both documented
  above with root cause, not just patched blind.
- **The hologram-placement-drift fix was audited and extended to every
  other affected function** (2026-08-30, same day, prompted by the user
  asking what's needed to stop a future session/agent from hitting the
  same class of bug): `ConstructConveyorLift`/`ConstructPipe`/
  `ConstructHypertube`/`ConstructRailroadTrack`/
  `ConstructVehiclePathSegment` all had the identical latent
  vulnerability (rotation re-asserted every poll tick, placement only
  set once) and have never been heavily live-tested, so the bug simply
  hadn't been triggered yet for them. Fixed identically, compiled clean.
  Full checklist (which functions are fixed, why, and the copy-paste
  pattern for any new one) now lives in
  `docs/buildgun-driven-placement-research.md`'s "§3 correction"
  checklist section - **not yet redeployed/live-tested for these 5**
  (belts were re-verified 48/48 after the same fix; pipes/lifts/
  hypertubes/tracks/vehicle-paths should get at least a spot-check next
  time any of them are used for real work).
- Phase 10 (negative tests): **all PASS**. `ERROR_NO_OUTPUT_IN_DIRECTION`
  and `INVALID_DIRECTION` proven in Phase 4. Output-to-output confirmed
  live: requesting one of a destination splitter's real OUTPUT connector
  positions as `destConnectorPosition` fails with `NO_FACTORY_CONNECTION`
  ("no free Input factory connection within tolerance...") rather than
  silently substituting the real input or any other connector -
  `FindFreeFactoryConnectionNear` only ever searches `FCD_INPUT` for the
  destination side, so this is refused by construction, not a runtime
  special case.
- Per the user's explicit original directive, manifold/production/
  factory-layout code may now depend on deterministic splitter port
  selection and connector-targeted belt construction.
