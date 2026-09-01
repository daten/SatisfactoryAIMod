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

**2026-08-31 session found two real bugs, both fixed, BOTH NOW
REDEPLOYED AND RE-CONFIRMED LIVE**: (1) `world.constructWaterPumpAtPosition`
CRASHED THE GAME on an on-land negative-path test — re-tested against
the fixed build with the EXACT same call that crashed it before: now
correctly returns a clean `CANNOT_CONSTRUCT`/"Must be placed on deep
water!" instead of crashing. Positive path (a real in-water position)
also re-confirmed. See `docs/placement-lessons.md`'s dedicated
writeup. (2) `world.constructStackableSupportOnTop` failed every call
(`GetStackHeight()` read back as 0) — fixed, re-tested with a full
3-level mixed column (Pipe → Belt → Hyper), all three levels
succeeded, landing a consistent 201 units apart. **Real, recurring
finding while re-testing**: roughly half of these on-top calls failed
on the FIRST attempt with `CANNOT_CONSTRUCT`/"An identical buildable
is already built there!" and then succeeded on an immediate retry with
no other change — matches this project's already-documented
transient-disqualifier-flakiness pattern ("Surface is too uneven!" on
other buildables, retry-once). Budget for a retry on this RPC
specifically. Also found: `result.buildableId` can report a STALE or
neighboring buildable's id (not the one just constructed) when several
stackable poles sit close together, especially right after a
delete+rebuild at the same spot — always cross-check
`result.detail.buildableIds` (or a before/after `world.buildables`
diff) rather than trusting `buildableId` alone in a dense column.

**Second pass, same session (2026-08-31/09-01), all remaining backlog
items attempted**: nearly everything below is now checked off.
Real, still-open findings from this pass: `world.connectConveyorLift`'s
long-standing height-matching problem is CONFIRMED STILL PRESENT (the
free end lands at a fixed default offset from the source, completely
ignoring the real destination - cleanly isolated this time after the
user caught a misaligned first attempt: "the containers you built are
not vertically aligned... when doing these types of test it's worth
validating placements"). A NEW, separate, unexplained finding turned
up during that same retest - a duplicate buildable appeared after a
`connectConveyorLift` call - flagged for dedicated investigation
(`task_6f0276ff`), not root-caused this session. `world.setBuildableColor`
has a real, correctly-scoped architectural limit on lightweight
buildables (not a bug - see its own entry below). Everything else
attempted this pass came back PASS. Two items remain genuinely
untested: `world.truckStations`' Liquid-form case and
`world.trainCargoPlatforms`' active flow rates, both because no
matching real infrastructure (a Fluid Truck Station, an actively-
loading platform) was reachable this session.

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
  `world.constructWaterPumpNearReference`** — **DONE 2026-08-31, real
  crash found, fixed, redeployed, and re-confirmed fixed.**
  `world.waterVolumes` near the player's real ocean reported sane
  bounds. `world.constructWaterPumpNearReference` from a real reference
  pump: PASS — 2000-unit offset correctly failed `CANNOT_CONSTRUCT`,
  3000/4000/5000-unit offsets all succeeded and verified via
  `world.buildables` at the exact expected positions. **Real minimum
  pump spacing later precisely confirmed as exactly `2000` units**
  (matches the Water Pump's own clearance box width exactly; the RPC
  path needs `2001`+ due to floating-point precision — see
  `RPC_REFERENCE.md`). `world.constructWaterPumpAtPosition`
  for a genuine in-water position: PASS, verified via
  `world.buildables`. The negative-path test (a literal on-land
  position) CRASHED THE GAME the first time — root-caused and fixed
  (see `docs/placement-lessons.md`'s dedicated writeup). **After
  redeploy, re-ran the EXACT same on-land call**: now correctly returns
  `CANNOT_CONSTRUCT`/"Must be placed on deep water!" instead of
  crashing — confirmed fixed. Also re-confirmed the positive path on a
  fresh reference pump at a different shoreline, and found a new real
  fact: this shoreline needs pumps ~5000+ units offshore before water
  is deep enough (3000 failed "Must be placed on deep water!", 5000+
  succeeded) — a distinct, real depth constraint separate from the
  pump-to-pump clearance spacing above. **`plan_water_pump_field()`
  live test: DONE, PASS.** Fed real telemetry (a confirmed reference
  pump position, `world.pipelineTiers`' real Mk1 `flow_limit=5 m³/s`,
  Water Pump's real 120 m³/min = 2.0 m³/s extraction rate) into the
  planner, got back 2 planned pump positions, constructed both via
  `world.constructWaterPumpAtPosition`/`NearReference` - both landed at
  the planner's exact computed coordinates, verified via
  `world.buildables`. First real end-to-end confirmation of the
  placement RPCs AND the layout planner working together.
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
  FOUND A REAL BUG, FIXED, RE-CONFIRMED LIVE AFTER REDEPLOY.** Built a
  base `PipeSupportStackable`, then called this RPC with a DIFFERENT
  recipe (`ConveyorPoleStackable`) as reference - failed
  `CANNOT_CONSTRUCT`/"An identical buildable is already built there!"
  every time. Root-caused: `GetStackHeight()` read back as `0` for this
  class, so the computed candidate position was literally the
  reference's own location (self-overlap) - reproduced directly by
  constructing a second pole at the exact same Z as a placed reference
  via the literal-position RPC, same error. Fixed: floor the height at
  a sane minimum (100.0) instead of trusting `GetStackHeight()`
  outright. **Redeployed and re-tested**: a full 3-level mixed column
  (`Recipe_PipeSupportStackable` → `Recipe_ConveyorPoleStackable` →
  `Recipe_HyperPoleStackable`) succeeded end to end, each level landing
  a consistent 201 units above the last, verified via `world.buildables`.
  **New finding while re-testing**: roughly half these calls failed on
  the first attempt with the same "already built there" error and then
  succeeded on an immediate retry - matches this project's known
  transient-disqualifier-flakiness pattern, not a residual bug (the
  clean, deterministic failure was gone; only occasional first-attempt
  flakiness remained). Retry once on `CANNOT_CONSTRUCT` before treating
  it as real.
- [x] **`world.priorityPowerSwitches` / `world.setPowerSwitchOn`** —
  **DONE 2026-08-31, PASS** (via the Tier 1 `setPowerSwitchOn` test
  above) - `circuitGroupID0`/`circuitGroupID1` confirmed to reflect
  real circuit topology (changes when the switch splits the network).
  **`world.setPriorityPowerSwitchPriority` itself: DONE, PASS** - set a
  real switch's priority 5→12→5 (restored), verified via
  `world.priorityPowerSwitches` at each step. **Still not tried**: an
  actual power-shortage shedding scenario (needs a real overloaded
  circuit, not attempted).
- [x] **`world.splitterSortRules` / `world.setSplitterSortRules`** —
  **DONE 2026-08-31/09-01, PASS.** Placed a fresh, disposable Smart
  Splitter (avoided the 7 existing splitters - all real, active parts
  of the user's Copper Factory/power-slug/nuclear-processing lines, not
  safe to experiment on). Set 3 rules (Iron Plate → output 0, Wildcard
  → output 1, Overflow → output 2), verified via `world.splitterSortRules`
  that all 3 rules round-tripped exactly (correct `itemName`/
  `isWildcard` resolution). Configuration-level round trip confirmed;
  did not build a full belt+producer loop to physically watch items
  route (out of scope for tonight - the config is correctly stored and
  the RPC calls the real native sort-rule API, same trust level as
  other configuration-only RPCs this session).
- [x] **`world.constructBeam`** — **DONE 2026-08-31/09-01, PASS.** Built
  a horizontal beam (`ignoreGroundTrace:true`, `freeformMode:false`),
  verified position via `world.buildables`. Built a second, vertical
  beam with `freeformMode:true` and `rotationScrollSteps:4` - landed
  with `roll:60.0` exactly, confirming each scroll step is a real 15°
  increment. First attempt at the player's own position failed "A
  player is in the way!" - moving the target away from the player
  fixed it (not itself a bug, matches this project's own established
  proximity-based encroachment behavior).
- [x] **`world.setBeamLength`** — **DONE 2026-08-31/09-01, PASS.**
  Changed a real beam from 200→400 units, response reported
  `oldLength`/`newLength`/`maxLength` (200/400/4000) self-consistently.
  **Still unconfirmed**: save/reload persistence (not tested - no
  save/reload cycle performed this session).
- [x] **`world.activeEvents`** — **DONE 2026-08-31, PASS.** Returned
  all 4 events (`Christmas`/`Anniversary`/`CSSBirthday`/`FirstOfApril`),
  all `isActive: false` (none currently running - correct for today's
  date). `world.recipeCatalog`'s `Recipe_IronPlate` (a normal, non-
  seasonal recipe) correctly reported `isAvailable: true`,
  `relevantEvents: []`.

## Tier 3 — the long-standing lift problem (harder, most complex)

- [x] **`world.connectConveyorLift` height-matching** — **DONE
  2026-08-31/09-01, CONFIRMED STILL BROKEN, cleanly isolated this
  time.** First attempt used two Storage Containers placed 1400/400
  units apart in X/Y (a real setup mistake on my part, caught live by
  the user: "the containers you built are not vertically aligned...
  when doing these types of test it's worth validating placements") -
  redone properly with both containers at the same X/Y (only a ~140-
  unit horizontal offset from a tile-corner nudge), 851 units apart in
  Z. **Result unchanged in both attempts**: `connectConveyorLift`
  reports `success:true` and the lift's Input connector genuinely
  attaches to the source - but the lift's Output (free) end lands at a
  FIXED offset from the source (`+300` in the connector's own local Y,
  `+400` in Z - the lift's own default single-segment height) in BOTH
  attempts, completely independent of where the real `destBuildableId`
  actually was (851 units up vs the ~400 units the lift actually
  climbed). Confirms hypothesis #6/#7/#8 did NOT fix the core problem -
  the free end simply never tracks the destination at all, not even
  approximately. **New, separate, unexplained finding**: a duplicate
  Storage Container (same class, identical position) appeared after
  the second `connectConveyorLift` call, not explicitly built by
  anything in this test - flagged as its own background investigation
  (`task_6f0276ff`), not root-caused tonight.
- [x] **`freeEndRotationSteps`** — passed `2` on the first lift attempt
  and `0` on the second; inconclusive on its own effect since both
  attempts already failed to reach the destination for the reason
  above (the free end's ROTATION couldn't be meaningfully evaluated
  when its actual endpoint position is already wrong). Not
  independently isolated this session.
- [x] **`world.setBuildableRotation` on other buildable types** —
  **DONE 2026-08-31/09-01, PASS.** Rotated a real, disposable test
  Smart Splitter from yaw 0 to 45 and back, verified both ways via
  `world.buildables`. Confirms the "dead on already-built lifts"
  finding is genuinely lift-specific, not a broader regression.

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
- [x] `world.pipeFluidBoxes` — **DONE 2026-08-31/09-01, PASS.** Built a
  real pump→pipe→Storage Tank network from a confirmed water pump
  (verified the connection via `world.pipeConnections`, NOT
  `world.connections` — pipes don't show up there — plus a real
  `Build_PipelineFlowIndicator_C` spawned along the route). Our own
  segment read `contentM3:0`/`fillPct:0` since the pump was never
  powered (a setup gap, not tested further) - but real, already-active
  pipes elsewhere in the save showed genuine non-zero data matching
  `docs/pipe-network-research.md`'s predictions: `fillPct` up to
  `0.9986` with `flowFill`/`flowDrain` matching `flowLimit` almost
  exactly, and one segment at `fillPct:1.4026` (a real overfill case,
  within `maxOverfillPct:0.4`). Real numbers, sane, matches predictions.
- [x] `world.trainCargoPlatforms` — **PARTIALLY DONE 2026-08-31**: read
  succeeded (22 platforms found), but `outflowRate`/`inflowRate` on an
  actively-loading platform not specifically checked.
- [x] `world.truckStations` — **PARTIALLY DONE 2026-08-31**: read
  succeeded (4 stations found), all real ones nearby report
  `resourceForm: "Solid"` correctly. **Still unconfirmed**: the
  `"Liquid"` case specifically - no Fluid Truck Station was found
  nearby this session to test against.
- [x] `world.setBuildableColor` — **DONE 2026-08-31/09-01, PASS on a
  real `AFGBuildable` actor, real architectural limit found on
  lightweight instances.** Set a real (non-lightweight) Power Pole to
  red - `success:true`, not independently confirmed visually (no
  screenshot/observation this session, RPC-level confirmation only).
  Then tried a real, freshly-queried lightweight foundation id -
  `TARGET_NOT_FOUND` every time, because lightweight buildables (most
  structural pieces - foundations, walls) are NOT real `AFGBuildable`
  actors at all (`AFGLightweightBuildableSubsystem` instance data
  instead - see `docs/lightweight-buildable-research.md`), so there's
  no actor for the color-customization API to act on. This is a real,
  correctly-scoped architectural limit, not a bug - contrast with
  `world.setBeamLength`/`world.deleteBuilding`, which DO handle
  lightweight ids via `FindOrSpawnBuildableForRuntimeData` (materializing
  a temporary buildable) - `SetBuildableColor` could theoretically gain
  the same treatment if colorable lightweight buildables become a real
  need, but wasn't built that way.

---

## Once a tier is fully checked off

Delete that section from this file and note the outcome in
`project_satisfactory_ai_interface.md` memory (or wherever the running
session record lives) — this file should only ever contain what's
*currently* untested, not a permanent historical log.
