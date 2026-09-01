# Test backlog: everything since the last live test session

Started 2026-08-31 per explicit user request ("track a list of new
features since the last active test session so we can test them during
the next session"). This is a checklist, not a reference — for what a
call actually does, see `RPC_REFERENCE.md`; for what's supported at
all, see `docs/buildable-coverage.md`.

**Last confirmed live test session: 2026-08-31** (this one — the water
pump / stackable support / Power Tower / map marker / power switch /
telemetry sweep documented below). Previous session: 2026-08-30 (the
Splitter-to-Splitter Conveyor Control Test, 48/48 live-verified, plus
the circular foundation/platform work — see
`project_satisfactory_ai_interface.md` memory).

**2026-08-31 session found two real bugs, both fixed, BOTH STILL
PENDING REDEPLOY** (Alpakit + relaunch — a manual Editor-UI step, not
something driven from this session): (1) `world.constructWaterPumpAtPosition`
CRASHED THE GAME on an on-land negative-path test — see
`docs/placement-lessons.md`'s dedicated writeup. (2)
`world.constructStackableSupportOnTop` failed every call
(`GetStackHeight()` read back as 0) — non-crashing, but always wrong.
Everything marked PASS below was confirmed against the build that was
actually running at the time (i.e. BEFORE either fix) — re-confirm the
two fixed items specifically after the next redeploy.

**How to use this**: work top to bottom within each priority tier —
they're ordered roughly easiest/highest-value first. Check a box, add
a one-line result note (pass/fail + what you saw), and if something
fails, leave it unchecked with the failure noted rather than deleting
the row — a failed test is a real finding, not noise. Once everything
in a tier is checked off, it can be deleted from this file entirely
(git history keeps the record) — this file is meant to shrink back to
empty, not accumulate forever like `docs/buildable-coverage.md` does.

---

## Tier 1 — quick, high-value, test these first

- [x] **`world.teleportPlayer`** — **DONE 2026-08-31.** Ground-trace
  mode (no `z`) twice returned `TELEPORT_BLOCKED`/"no clear
  destination" near the water-pump test site (possibly the water
  itself, or clutter from the newly-built pumps - not root-caused,
  low priority since `ignoreGroundTrace` is the reliable mode for this
  project's own use anyway). `ignoreGroundTrace: true` with a literal
  z above a real foundation's surface: PASS, landed at the exact
  requested position, verified via `world.player`.
- [x] **`world.powerPoles`** — **DONE 2026-08-31, PASS.** A real Power
  Tower reported exactly TWO `connections` entries (`Default` +
  `PowerTower`); an ordinary `PowerPoleMk1` reported exactly ONE
  (`Default`). Matches the bugfix's own claim exactly.
- [x] **Power Tower connection fix (`world.connectPower`)** — **DONE
  2026-08-31, PASS.** Found two real Power Towers 26894 units apart
  (beyond `world.powerLineLimits`' plain `maxLength` of 10000, within
  `maxPowerTowerLength` of 30000) with free `PowerTower`-type
  connectors. First attempt failed `CANNOT_CONSTRUCT`/"Invalid aim
  location!" (same known camera/aim flakiness this project already
  documents elsewhere) - retried with `ignoreAimLocation: true,
  ignoreWireSnap: true` and it succeeded. Verified via `world.buildables`:
  a real new `Build_PowerLine_C` actor appeared at Tower A's position
  pointing toward Tower B. **Not yet tried**: Tower→ordinary-pole
  (short range) to confirm the fix didn't break the common case.
- [x] **`world.mapMarkers` / `world.placeMapMarker`** — **DONE
  2026-08-31, PASS (full round trip).** Placed with a real `iconId`
  from `world.mapMarkerIcons`; the marker appeared in `world.mapMarkers`
  at the exact requested position with the correct `iconId`.
  `world.removeMapMarker` (using the `markerId` from the place
  response's `result.detail`) removed it - confirmed gone on
  re-query. Note: `placeMapMarker` has no `label`/`name` param (the
  marker's `name` field always comes back empty) - not a bug, just an
  unsupported param, don't pass one.
- [x] **`world.setPowerSwitchOn`** — **DONE 2026-08-31, PASS.** Toggled
  a real Priority Power Switch ("Copper Factory") off then back on via
  `world.priorityPowerSwitches` before/after checks - `isSwitchOn`
  flipped both ways correctly. Bonus finding: `circuitGroupID0` changed
  from `0` (single connected circuit) to a distinct nonzero value while
  OFF (the switch splitting the network into two groups), confirming
  the `circuitGroupID0`/`circuitGroupID1` topology semantics flagged as
  unconfirmed in Tier 2 below - closing that gap too.

## Tier 2 — moderate setup, real correctness questions

- [x] **`world.waterVolumes` / `world.constructWaterPumpAtPosition` /
  `world.constructWaterPumpNearReference`** — **PARTIALLY DONE
  2026-08-31, real crash found and fixed along the way, redeploy still
  needed.** `world.waterVolumes` near the player's real ocean reported
  sane bounds. `world.constructWaterPumpNearReference` from a real
  reference pump: PASS — 2000-unit offset correctly failed
  `CANNOT_CONSTRUCT`, 3000/4000/5000-unit offsets all succeeded and
  verified via `world.buildables` at the exact expected positions
  (**real minimum pump spacing is somewhere in (2000, 3000]**, closing
  the "unconfirmed value" gap noted below). `world.constructWaterPumpAtPosition`
  for a genuine in-water position: PASS, verified via
  `world.buildables`. **Then the negative-path test (a literal on-land
  position) CRASHED THE GAME** — real bug, root-caused and fixed same
  session (see `docs/placement-lessons.md`'s dedicated writeup), both
  targets compiled clean. **Still needed**: redeploy (Alpakit +
  relaunch) and re-test the on-land case now returns a clean
  `NO_WATER_VOLUME_FOUND`/`CANNOT_CONSTRUCT` instead of crashing, plus
  a too-shallow-water case (not yet tried). Once the crash fix is
  confirmed, also try feeding a real lake's bounds into
  `controller/satisfactory_ai/water.py`'s `plan_water_pump_field()` and
  constructing the resulting row of pumps - the first real test of the
  placement RPCs AND the layout planner together (still not done).
- [x] **`world.constructStackableSupport`** — **DONE 2026-08-31,
  PASS.** `stackCount: 0` built a single ordinary `PipeSupportStackable`,
  verified via `world.buildables`. Same-recipe multi-level stacking via
  REPEATED calls (not `stackCount`) also confirmed: 5 sequential calls
  near the same column all succeeded, each snapping to a new real slot
  above the last via the hologram's own `TrySnapToActor` logic - the
  literal Z passed didn't need to be precise (dz=50 through dz=400 all
  landed correctly), confirming the real engine snap is robust. Not
  independently tried: `stackCount: 2+` in a single Zoop call (the
  repeated-call path was what mattered for the user's actual use case,
  and got proven instead), nor `Recipe_HyperPoleStackable` specifically.
- [x] **`world.constructStackableSupportOnTop`** — **DONE 2026-08-31,
  FOUND A REAL BUG, FIXED (not yet redeployed).** Built a base
  `PipeSupportStackable`, then called this RPC with a DIFFERENT recipe
  (`ConveyorPoleStackable`) as reference - failed `CANNOT_CONSTRUCT`/
  "An identical buildable is already built there!" every time.
  Root-caused: `GetStackHeight()` read back as `0` for this class, so
  the computed candidate position was literally the reference's own
  location (self-overlap) - reproduced directly by constructing a
  second pole at the exact same Z as a placed reference via the
  literal-position RPC, same error. Fixed: floor the height at a sane
  minimum (100.0) instead of trusting `GetStackHeight()` outright - see
  `RPC_REFERENCE.md`. Compiled clean, **NOT yet redeployed/re-verified** -
  re-run this exact test after the next redeploy.
- [x] **`world.priorityPowerSwitches` / `world.setPowerSwitchOn`** —
  **DONE 2026-08-31, PASS** (via the Tier 1 `setPowerSwitchOn` test
  above) - `circuitGroupID0`/`circuitGroupID1` confirmed to reflect
  real circuit topology (changes when the switch splits the network).
  **Still not tried**: `world.setPriorityPowerSwitchPriority` itself
  (only the on/off toggle was exercised), and an actual power-shortage
  shedding scenario.
- [ ] **`world.splitterSortRules` / `world.setSplitterSortRules`** —
  only the READ side (`world.splitterSortRules`, 7 splitters found) was
  confirmed this session. Still need: place a Smart or Programmable
  Splitter, set a rule routing a specific item to a specific output,
  confirm it actually routes there. Then test the `"Wildcard"` sentinel
  on another output and confirm everything else falls through to it.
- [ ] **`world.constructBeam`** — construct a beam between two literal
  points with `ignoreGroundTrace: true`, both `freeformMode: false` and
  `true`, and a non-zero `rotationScrollSteps`. This is a genuinely new
  placement paradigm (its own hologram) — confirm it constructs at all
  before worrying about exact angle/rotation correctness.
- [ ] **`world.setBeamLength`** — after a beam exists, change its
  length and confirm the visual mesh actually updates. **Specifically
  check**: does the new length survive a save/reload or a
  `world.buildables` re-query? (Flagged as a real, unconfirmed
  lightweight-instance persistence question in the code's own doc
  comments.)
- [x] **`world.activeEvents`** — **DONE 2026-08-31, PASS.** Returned
  all 4 events (`Christmas`/`Anniversary`/`CSSBirthday`/`FirstOfApril`),
  all `isActive: false` (none currently running - correct for today's
  date). `world.recipeCatalog`'s `Recipe_IronPlate` (a normal, non-
  seasonal recipe) correctly reported `isAvailable: true`,
  `relevantEvents: []`.

## Tier 3 — the long-standing lift problem (harder, most complex)

- [ ] **`world.connectConveyorLift` height-matching** — the core,
  still-open problem: try to build a lift at a specific custom height
  (not the default) and see whether hypothesis #6/#7/#8 (the
  `GetHitResult()` injection, the extra `SetHologramLocationAndRotation()`
  call, or the camera-position fix) made any difference at all.
  Compare against a hand-built reference lift of the same intended
  height. This has failed 5+ times already — go in expecting another
  negative result, but log exactly what happens either way.
- [ ] **`freeEndRotationSteps`** — on a lift's still-open free end
  (per the user's own confirmed mechanic: only rotatable during
  hologram placement), test whether this parameter actually rotates
  it, and in which direction relative to a positive/negative value.
- [ ] **`world.setBuildableRotation` on other buildable types** — the
  "dead on already-built lifts" finding is lift-specific; confirm
  whether it still works on ordinary rotatable buildables (a wall, a
  machine) as a sanity check that the fix/finding is correctly scoped.

## Tier 4 — offline research day (pipe/rail/truck), straightforward reads

Lower risk — these are read-only telemetry additions, most likely to
"just work" if they compile, but genuinely never queried against a
real save:

- [x] `world.pipelinePumpTiers` (now includes Valve) — **DONE
  2026-08-31, PASS.** Real numbers: Pump `maxHeadLift=22`/
  `designHeadLift=20`, Mk2 `55`/`50`, Valve `0`/`0` (correct - a Valve
  doesn't pump), all `defaultFlowLimit=10`. All sane.
- [x] `world.pipeReservoirTiers` — **DONE 2026-08-31, PASS.** Storage
  Tank `maxContentM3=400`, Industrial Tank `maxContentM3=2400` - matches
  known real game values exactly.
- [ ] `world.pipeFluidBoxes` — call while a pipe network is actively
  filling (right after connecting a pump to an empty pipe run) and
  watch whether `fillPct`/`flowFill`/`flowDrain` behave the way
  `docs/pipe-network-research.md` predicts (sequential segment fill,
  possible sloshing). Not attempted this session.
- [x] `world.trainCargoPlatforms` — **PARTIALLY DONE 2026-08-31**: read
  succeeded (22 platforms found), but `outflowRate`/`inflowRate` on an
  actively-loading platform not specifically checked.
- [x] `world.truckStations` — **PARTIALLY DONE 2026-08-31**: read
  succeeded (4 stations found), all real ones nearby report
  `resourceForm: "Solid"` correctly. **Still unconfirmed**: the
  `"Liquid"` case specifically - no Fluid Truck Station was found
  nearby this session to test against.
- [ ] `world.setBuildableColor` — set a color on a placed buildable,
  confirm it visually changes (and check whether white-vs-black
  defaults matter the way the beam color code assumed). Not attempted
  this session.

---

## Once a tier is fully checked off

Delete that section from this file and note the outcome in
`project_satisfactory_ai_interface.md` memory (or wherever the running
session record lives) — this file should only ever contain what's
*currently* untested, not a permanent historical log.
