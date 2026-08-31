# Test backlog: everything since the last live test session

Started 2026-08-31 per explicit user request ("track a list of new
features since the last active test session so we can test them during
the next session"). This is a checklist, not a reference — for what a
call actually does, see `RPC_REFERENCE.md`; for what's supported at
all, see `docs/buildable-coverage.md`.

**Last confirmed live test session: 2026-08-30** (the Splitter-to-
Splitter Conveyor Control Test, 48/48 live-verified, plus the circular
foundation/platform work — see `project_satisfactory_ai_interface.md`
memory). Everything below was written, compiled, and committed during
the offline research day that followed (the user was away, no game
running) and this current session — **none of it has touched a real
running game yet.**

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

- [ ] **`world.teleportPlayer`** — teleport to a known coordinate,
  confirm the player actually moves and doesn't fall through terrain
  or get stuck. Try both ground-snapped and `ignoreGroundTrace` modes.
- [ ] **`world.powerPoles`** — call with at least one placed Power
  Tower nearby. Confirm a Tower reports TWO `connections` entries
  (`PowerTower` + `Default`) and an ordinary pole reports ONE. This is
  the telemetry half of the Power Tower bugfix below — start here.
- [ ] **Power Tower connection fix (`world.connectPower`)** — the
  actual bug verification. Place two Power Towers farther apart than
  `world.powerLineLimits`' plain `maxLength` but within a Tower's own
  `powerTowerWireMaxLength` (from `world.powerPoles`), then call
  `world.connectPower` between them. Should now succeed — confirm it
  picks each Tower's `PowerTower`-type connector, not the short-range
  one. Also try Tower→ordinary-pole (short range) to confirm the fix
  didn't break the common case.
- [ ] **`world.mapMarkers` / `world.placeMapMarker`** — place a marker
  with a real `iconId` from `world.mapMarkerIcons`, then open the
  in-game map and confirm it's actually visible with the right icon
  and color. Then `world.removeMapMarker` and confirm it's gone.
- [ ] **`world.setPowerSwitchOn`** — toggle an existing Power Switch or
  Priority Power Switch off/on, confirm the circuit actually
  loses/regains power.

## Tier 2 — moderate setup, real correctness questions

- [ ] **`world.waterVolumes` / `world.constructWaterPumpAtPosition` /
  `world.constructWaterPumpNearReference`** — call `world.waterVolumes`
  near a real lake and confirm it reports sane `bounds`. Then call
  `world.constructWaterPumpAtPosition` with a literal x/y/z read from
  those bounds and confirm a FIRST pump actually gets built with no
  reference needed. Then call `world.constructWaterPumpNearReference`
  with a real offset from that pump and confirm a second one builds
  nearby (not just "success" with nothing visible). Try an offset that
  lands on dry land or too-shallow water and confirm it correctly fails
  (`CANNOT_CONSTRUCT`) rather than silently building somewhere wrong.
  This whole feature was built from source reasoning (a real,
  previously-mis-documented gap — `world.placeExtractor` never actually
  supported Water Pump) and never tried against an actual lake. Once
  basic placement works, also try feeding a real lake's bounds into
  `controller/satisfactory_ai/water.py`'s `plan_water_pump_field()` and
  constructing the resulting row of pumps - this is the first real test
  of both the placement RPCs AND the layout planner together, and the
  first opportunity to measure a real minimum pump spacing (currently
  an unconfirmed, caller-supplied value in the planner).
- [ ] **`world.constructStackableSupport`** — construct one with
  `stackCount: 0` first (should behave like an ordinary single pole)
  and confirm it builds. Then try `stackCount: 2` or more and confirm
  it actually produces a vertical stack, not a sideways row or a
  single instance - this is the real, flagged unknown (whether the
  `FIntVector`'s Z component really maps to "vertical" for this
  hologram). Check `result.detail.foundCount` vs `requestedCount`
  matches what's actually visible in-game. Try all three real recipes
  (`Recipe_ConveyorPoleStackable`/`Recipe_PipeSupportStackable`/
  `Recipe_HyperPoleStackable`) since the "one shared class covers all
  three" claim was inferred from naming, not confirmed via binary-grep
  like most other "shared class" findings this session.
- [ ] **`world.priorityPowerSwitches` / `world.setPriorityPowerSwitchPriority`**
  — set a priority on a placed switch, cause a real power shortage
  (or check in-game), confirm it actually sheds before a lower-priority
  switch. Also confirm `circuitGroupID0`/`circuitGroupID1` match what
  `world.connections`-style topology would suggest.
- [ ] **`world.splitterSortRules` / `world.setSplitterSortRules`** —
  place a Smart or Programmable Splitter, set a rule routing a specific
  item to a specific output, confirm it actually routes there. Then
  test the `"Wildcard"` sentinel on another output and confirm
  everything else falls through to it.
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
- [ ] **`world.activeEvents`** — call it and confirm it returns all 4
  events with sane `isActive` values (should mostly be `false` unless
  an event happens to be running). Then check `world.recipeCatalog`'s
  `isAvailable`/`relevantEvents` fields on a normal (non-seasonal)
  recipe — should be `isAvailable: true`, `relevantEvents: []`.

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

- [ ] `world.pipelinePumpTiers` (now includes Valve) — check real
  `maxHeadLift`/`designHeadLift`/`defaultFlowLimit` numbers look
  sane for a placed Pump vs a placed Valve.
- [ ] `world.pipeReservoirTiers` — check real numbers for both Fluid
  Buffer and Industrial Fluid Buffer.
- [ ] `world.pipeFluidBoxes` — call while a pipe network is actively
  filling (right after connecting a pump to an empty pipe run) and
  watch whether `fillPct`/`flowFill`/`flowDrain` behave the way
  `docs/pipe-network-research.md` predicts (sequential segment fill,
  possible sloshing).
- [ ] `world.trainCargoPlatforms` — check `outflowRate`/`inflowRate`
  read as nonzero on a real Liquid Freight Platform actively
  loading/unloading.
- [ ] `world.truckStations` — check `resourceForm` correctly reports
  `"Liquid"` for a real placed Fluid Truck Station (this is the
  single least-confirmed inference in that whole feature — flagged in
  its own doc comment as the first thing to verify).
- [ ] `world.setBuildableColor` — set a color on a placed buildable,
  confirm it visually changes (and check whether white-vs-black
  defaults matter the way the beam color code assumed).

---

## Once a tier is fully checked off

Delete that section from this file and note the outcome in
`project_satisfactory_ai_interface.md` memory (or wherever the running
session record lives) — this file should only ever contain what's
*currently* untested, not a permanent historical log.
