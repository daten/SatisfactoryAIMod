# Buildable coverage tracker

Started 2026-08-31 per explicit user request ("start a document that
lists all of the buildable items in the game that we currently support
and all of the buildable items that we do not yet support... to track
coverage as we implement new features").

**Scope**: construction (placing a new instance) and dedicated
control/telemetry of buildables specifically — not the general item/
recipe catalog (`world.recipeCatalog`/`world.itemCatalog` already
enumerate every recipe/item in the game regardless of construction
support; see `RPC_REFERENCE.md`).

**How to keep this current**: whenever a new `Construct*`/`Set*`/
`Log*AsJson` function is added to `AIModFunctionLibrary`, check whether
it changes a row below (new family covered, a 🟡/❓ becomes ✅, etc.)
and update this file in the same commit. The authoritative list of real
buildable families is `Content/FactoryGame/Buildable/{Building,Factory,
Vehicle}/` — re-derive from there if this file drifts, don't trust
memory of what "should" be in the game.

## Legend

- ✅ **Supported** — a real RPC constructs and/or controls this family.
- 🟡 **Partial** — construction/control exists but has known, real
  limitations (see the note).
- ❓ **Unconfirmed** — likely covered by an existing generic RPC based on
  the buildable's real hologram class, but not specifically verified —
  neither by source research nor live testing.
- ❌ **Not supported** — no RPC exists for this yet; a real, open gap.
- ⚠️ **Telemetry only** — read-only visibility exists; no construction
  or control RPC.

Everything in this file inherits the project's standing caveat: **NONE
of this session's newer additions (2026-08-31: teleportPlayer, map
markers, active events, beams, priority power switches) have been
live-tested against a running game yet** — "✅ Supported" means "a real
RPC exists and is believed correct from source research," not
"confirmed working in practice." Check `RPC_REFERENCE.md` entries for
individual "NOT YET LIVE-TESTED" flags.

## Generic single-hologram buildables — ✅ via `world.placeBuilding`

`world.placeBuilding` drives the simple, single-click
`AFGBuildableHologram`/`AFGFactoryHologram` flow — confirmed generic
across every recipe using that hologram lineage, not special-cased per
building. This covers the large majority of ordinary buildables:

| Family | Real content path | Status |
|---|---|---|
| Constructor, Assembler, Manufacturer, Refinery, Blender, Foundry, Smelter, Packager | `Factory/{ConstructorMk1,AssemblerMk1,ManufacturerMk1,OilRefinery,Blender,FoundryMk1,SmelterMk1,Packager}` | ✅ |
| Particle Accelerator, Quantum Encoder, Converter | `Factory/{HadronCollider,QuantumEncoder,Converter}` | ✅ |
| Generators: Biomass, Coal, Fuel, Geothermal, Nuclear | `Factory/Generator{Biomass,Coal,Fuel,GeoThermal,Nuclear}` | ✅ |
| Storage: containers Mk1/Mk2, Fluid Buffer, Industrial Fluid Buffer, Central Storage (Depot), personal Space Container | `Factory/{StorageContainerMk1,StorageContainerMk2,StorageTank,IndustrialFluidContainer,CentralStorage,StoragePlayer}` | ✅ |
| Power poles (Mk1-3, wall, wall double), Power Storage, Power Switch | `Factory/{PowerPoleMk1,PowerPoleMk2,PowerPoleMk3,PowerPoleWall,PowerPoleWallDouble,PowerStorage,PowerSwitch}` | ✅ (the pole/switch itself; the wire between two poles is a separate connector RPC — see below). Plain Power Switch on/off: `world.setPowerSwitchOn` |
| Power Tower (real `AFGBuildablePowerPole` with `powerPoleType=="PowerTower"` — the separate `Factory/PowerTower/` content folder's `AFGBuildablePowerTower` C++ class is unused/vestigial, confirmed 2026-08-31) | `Factory/PowerTower` | ✅ placement (generic). Telemetry: `world.powerPoles` (reports both real connectors — `PowerTower` long-range + `Default` short-range — and the per-instance `powerTowerWireMaxLength`). **Real correctness bug found and fixed 2026-08-31**: `world.connectPower`'s connector selection previously had no awareness that a Tower has TWO connector types and could pick the wrong one — fixed to jointly select a compatible pair across both endpoints. Not live-tested |
| Priority Power Switch (there is no separate "Smart Power Switch" recipe — confirmed 2026-08-31, only `Recipe_PowerSwitch`/`Recipe_PriorityPowerSwitch` exist; the `SmartPowerSwitch/` content folder is source art only) | `Factory/{PriorityPowerSwitch,SmartPowerSwitch}` | ✅ Full config+control: `world.priorityPowerSwitches` (telemetry — priority, on/off, circuit group ids), `world.setPowerSwitchOn`, `world.setPriorityPowerSwitchPriority` |
| Splitter/Merger and all Smart/Programmable/Priority/Lift variants | `Factory/CA_{Splitter,Merger,SplitterLift,SplitterLiftProgrammable,SplitterLiftSmart,SplitterProgrammable,SplitterSmart,MergerLift,MergerLiftPriority,MergerPriority}` | ✅ placement — confirmed generic (`AFGConveyorAttachmentHologram : AFGFactoryHologram`, same lineage), see `docs/conveyor-attachment-research.md`. Smart/Programmable Splitter sort-rule config: `world.splitterSortRules`/`world.setSplitterSortRules` — ✅, not live-tested |
| Structural: Foundation, Wall, Floor, Ramp, Stair, Roof, Pillars, Fence, Tarp Fence, Barrier, Catwalk, Walkway, Ladder, Doors, Vent, Corner Block, Decor, Stackable Shelf, Potty, conveyor/foundation holes+passthrough | `Building/{Foundation,Wall,Floor,Ramp,Stair,Roof,Pillars,Fence,TarpFence,Barrier,Catwalk,Walkway,Ladder,Doors,Vent,Potty}`, `Factory/{CornerBlock,ConveyorHole,ConveyorFloorHole,FoundationPassthrough}` | ✅ (many of these are "lightweight" instances at runtime — see `world.buildables`' `lightweight:` id scheme, `docs/lightweight-buildable-research.md`) |
| Stackable supports: Conveyor Pole Stackable, Pipe Support Stackable, Hyper Pole Stackable — one shared column, multiple vertically-stacked instances, MIXABLE across the three tiers in one column (real dense routing for pipes/belts/hypertube) | `Recipe_ConveyorPoleStackable`/`Recipe_PipeSupportStackable`/`Recipe_HyperPoleStackable` (all confirmed real recipes; class is `AFGBuildablePoleStackable`/`AFGStackablePoleHologram`, generic across all three) | ✅ — added 2026-08-31, explicit user request. **NOT via `world.placeBuilding`** — these use a real two-step Zoop build flow (`BHBS_PlacementAndRotation`→`BHBS_Zoop`), not the ordinary single-click hologram most of this table's other rows rely on, so the generic placement path alone would fail on these. Two RPCs, two real workflows: `world.constructStackableSupport` (`stackCount` + `SetZoopAmount(FIntVector)` — one Zoop call, multiple UNIFORM same-recipe instances; secondary use case) and `world.constructStackableSupportOnTop` (reference-relative, uses the reference's real `GetStackHeight()`, one call per level — the PRIMARY real-world mechanic per user correction 2026-08-31, since it's the only one that lets a column mix different recipe tiers). Neither is live-tested, and the FIntVector-to-vertical-axis mapping (`constructStackableSupport` only) is inferred, not confirmed |
| Lighting: Ceiling Light, Floodlight, Street Light, Lights Control Panel | `Factory/{CeilingLight,Floodlight,StreetLight,LightsControlPanel}` | ✅ placement; ❌ no dedicated RPC to control panel-driven light grouping/scheduling |
| Signs: Sign Pole, Digital Sign, Standalone Sign | `Factory/{SignPole,SignDigital,StandaloneSign}` | ✅ placement; ❌ no RPC to set sign text/icon content |
| Hub Terminal, Trading Post, MAM, Workbench, Automated Work Bench, Workshop, Space Elevator, Radar Tower, Resource Sink, Resource Sink Shop, Lookout Tower, Jump Pad, Landing Pad, Large Fan, Drone Station | `Factory/{HubTerminal,TradingPost,Mam,WorkBench,AutomatedWorkBench,Workshop,SpaceElevator,RadarTower,ResourceSink,ResourceSinkShop,LookoutTower,JumpPad,LandingPad,LargeFan,DroneStation}` | ✅ placement (these are singleton/rare-placement structures in practice, not repeatedly built) |
| Blueprint Designer, Blueprint-placed buildable wrapper | `Factory/{BlueprintDesigner,BlueprintBuildable}` | ❓ Unconfirmed — the Designer itself is likely generic placement, but placing a saved **Blueprint** (multi-building group) as a single paste operation is a materially different mechanism, not researched at all |

## Extractors — ✅ via `world.placeExtractor`

Single RPC, snaps to a real resource/well node id (`world.resourceNodes`),
confirmed to cover every extractor type in the game:

| Family | Status |
|---|---|
| Miner Mk1 / Mk2 / Mk3 | ✅ |
| Oil Pump (Impure Oil Node) | ✅ |
| Fracking Smasher (Pressurizer, on a `FrackingCore` node) | ✅ |
| Fracking Extractor (on an *activated* `FrackingSatellite` node) | ✅ |
| Portable Miner (handheld, on a node) | ✅ — separate RPC, `world.placePortableMiner`, different mechanism (equipment dispenser, not build-gun hologram) |
| Water Pump (on an `AFGWaterVolume`, NOT a resource node) | ✅ — **correction 2026-08-31**: this row previously said `world.placeExtractor` covered it; that was never true (Water Pump has no `AFGResourceNodeBase` to target). Real support is `world.constructWaterPumpAtPosition` (seed the first pump from scratch, literal x/y/z), `world.constructWaterPumpNearReference` (build additional pumps near an already-placed one), + `world.waterVolumes` (real, discoverable water bodies — directly resolves the "water is hard to locate" problem this project had previously flagged as too difficult). Large-scale field layout (rows of pumps grouped until a pipe tier saturates, multi-row trunk merging, shared stackable-support columns for pipe+power routing): `controller/satisfactory_ai/water.py`. **Live-tested 2026-08-31**: `constructWaterPumpNearReference` CONFIRMED WORKING (real min spacing found: fails at 2000 units, succeeds at 3000+); `constructWaterPumpAtPosition` CONFIRMED WORKING for genuine in-water positions, but its on-land negative path CRASHED THE GAME (real bug, root-caused and fixed same session — `AFGWaterVolume::EncompassesPoint()` is now a hard requirement instead of a distance-based fallback; see `docs/placement-lessons.md`). Fix compiled clean on both targets but NOT yet redeployed/re-verified live |

## Vehicles — ✅ via `world.constructVehicle`

| Family | Status |
|---|---|
| Truck, Tractor, Explorer, Cyber Wagon, Golf Cart | ✅ |
| Drone (requires a placed, unoccupied Drone Station) | ✅ |
| Locomotive, Freight Wagon (assembling into a **train consist** — coupling multiple vehicles together) | ❌ Not supported — `world.constructVehicle` places one vehicle; coupling logic is separate, unresearched |
| Vehicle Path segments (the road network drones/trucks follow) | ✅ `world.constructVehiclePathSegment` — **not yet live-tested** |
| Assigning a built vehicle's autopilot route over path segments | ❌ Not supported — real source API identified (`AFGWheeledVehicleIdentifier::SetVehicleRoute`/`AddWaypoint`/`SetAutopilotEnabled`) but needs its own path-node-GUID telemetry layer that doesn't exist yet |

## Spline / multi-step / connector-driven construction

Each of these needed (and got) its own dedicated `Construct*` function —
none can go through the generic single-click flow:

| Family | RPC | Status |
|---|---|---|
| Conveyor Belt Mk1–Mk6 | `world.connectConveyor` | ✅ |
| Conveyor Lift Mk1–Mk6 | `world.connectConveyorLift` | 🟡 Real, long-standing gap: cannot reliably reproduce arbitrary custom lift heights a player can achieve by hand (regression, 3 hypotheses tried and not yet confirmed working); free-end rotation (`freeEndRotationSteps`) added but not live-tested |
| Pipeline / Pipeline Mk2 | `world.connectPipe` | ✅ |
| Hypertube tube segment | `world.connectHypertube` | ✅ |
| Railroad track | `world.constructRailroadTrack` | ✅ — not yet live-tested |
| Power line (the wire between two poles) | `world.connectPower` | ✅ |
| Architecture Beam (all Beam/Beam_Support/Beam_Cross/Beam_H/etc variants) | `world.constructBeam` | ✅ — added 2026-08-31, not yet live-tested. Length control on an already-placed beam: `world.setBeamLength` |

## Explicitly NOT yet supported — real, open gaps

- **Pipeline Pump, Pipeline Junction (3-way/4-way), Valve, standalone
  Fluid Buffer/Industrial Tank as their own placement step.** These are
  ordinary `AFGBuildable`s (not spline-snapped), so `world.placeBuilding`
  likely covers them the same as any other generic buildable — but this
  was flagged in `docs/pipe-network-research.md` as genuinely unconfirmed
  rather than assumed, since pipe attachments were never specifically
  tried (unlike conveyor Splitters/Mergers, which WERE confirmed generic).
  Move to the ✅ table above once actually tried.
- **Valve flow-limit control** (`SetUserFlowLimit()`/`GetUserFlowLimit()`
  are real, public setters/getters on `AFGBuildablePipelinePump` —
  confirmed from source, just never wired to an RPC).
- **Train Signal (Type 1/2), Train Switch, Railway End Stop placement.**
  These likely snap onto an existing track segment similar to how a
  conveyor attachment snaps onto a belt — genuinely unconfirmed whether
  `world.placeBuilding`'s generic flow handles that snapping correctly,
  since it was never tried against these specific families.
- **Train Station platform / Truck Station construction.** Real
  telemetry exists (`world.trainCargoPlatforms`, `world.trainStations`,
  `world.truckStations`), but placing a NEW one was never specifically
  tested — same generic-placement-probably-works-but-unconfirmed
  situation as pipe attachments.
- **Sign content** (text/icon on Sign Pole / Digital Sign / Standalone
  Sign) — placement is presumably generic, but there's no RPC to set
  what a sign actually displays.
- **Portal** (`Factory/Portal`, `Factory/PortalPotty`) — very recent
  content, not researched at all yet.
- **Project Assembly** (`Factory/ProjectAssembly`) — likely a fixed,
  singleton story structure rather than something normally constructed;
  not researched.
- **Blueprint paste** (placing a saved multi-building Blueprint as one
  operation) — materially different from placing a single buildable;
  not researched at all.
- **Cheat-only buildables** (`Factory/CheatFluidPump`,
  `Factory/CheatPowerSource`) — deliberately out of scope, dev-only
  content not meant to be player-constructible.

## Control/telemetry RPCs, by family (beyond construction)

| Family | Control/telemetry | Status |
|---|---|---|
| Manufacturer/Extractor/Generator | `world.setRecipe`, `world.setClockSpeed`, `world.installPowerShard`, `world.targetedManufacturer` | ✅ |
| Any buildable (rotation, color, dismantle) | `world.setBuildableRotation` (🟡 confirmed broken on already-built Conveyor Lifts specifically — real free-end rotation only works during hologram placement), `world.setBuildableColor` (not live-tested), `world.deleteBuilding` | ✅/🟡 |
| Architecture Beam | `world.setBeamLength` | ✅ — not live-tested, real uncertainty flagged about whether it persists correctly for lightweight-instanced beams |
| Central Storage (Depot) | `world.centralStorage`, `world.withdrawFromCentralStorage` | ✅ |
| Drone Station | `world.droneStations`, `world.pairDroneStations` | ✅ |
| Train | `world.trains`, `world.setTrainSelfDriving`, `world.setTrainTimetable` | ✅ |
| Power line limits | `world.powerLineLimits` | ⚠️ Telemetry only |
| Priority Power Switch (priority, on/off, circuit topology) | `world.priorityPowerSwitches`, `world.setPowerSwitchOn`, `world.setPriorityPowerSwitchPriority` | ✅ — not live-tested |
| Smart/Programmable Splitter (per-output item sort rules) | `world.splitterSortRules`, `world.setSplitterSortRules` | ✅ — not live-tested. Closes the gap `world.conveyorAttachments`' `supportsSortRules` flag had been noting since 2026-08-25 |
| Conveyor/pipe connection topology | `world.connections`, `world.pipeConnections`, `world.pipeFluidBoxes` | ⚠️ Telemetry only |
| Map markers | `world.mapMarkers`/`world.placeMapMarker`/`world.removeMapMarker`/`world.mapMarkerIcons` | ✅ — not a buildable, but the closest thing to "player-assist annotation," included for completeness |

## Session log

- **2026-08-31**: Document created. Baseline snapshot reflects the state
  after `world.constructBeam`/`world.setBeamLength` were added (this
  session's most recent additions). Everything added 2026-08-31
  specifically (teleportPlayer, map markers, active events, beams) is
  real but **not yet live-tested** — see `project_satisfactory_ai_interface.md`
  memory for the full backlog.
- **2026-08-31 (later)**: Added Priority Power Switch config/control
  (`world.priorityPowerSwitches`/`world.setPowerSwitchOn`/
  `world.setPriorityPowerSwitchPriority`). Real finding while
  researching: no separate "Smart Power Switch" recipe exists — this
  file previously listed "Smart/Priority Power Switch" as if they might
  be two things; confirmed via a full Content-tree search that only
  `Recipe_PowerSwitch`/`Recipe_PriorityPowerSwitch` are real, corrected
  the row above.
- **2026-08-31 (later still)**: Added Smart/Programmable Splitter
  sort-rule config (`world.splitterSortRules`/
  `world.setSplitterSortRules`) — the exact gap this file's Splitter row
  and `world.conveyorAttachments`' `supportsSortRules` flag had both
  been flagging since 2026-08-25.
- **2026-08-31 (later still)**: User asked whether Power Tower support
  was correct given its dual short/long-range connectors — answering
  this honestly surfaced a REAL, previously-unnoticed correctness bug
  in `world.connectPower` (connector selection had no awareness a Tower
  has two connector types, could pick the wrong one). Fixed in the same
  pass, plus added `world.powerPoles` telemetry to make the previously-
  invisible per-connector-type state visible. Also corrected this file's
  Power Pole row, which previously listed "Power Tower" as if it were
  just another simple pole variant with no real distinction worth
  tracking.
- **2026-08-31 (later still)**: User asked whether automated Water Pump
  placement near a reference pump was a real code gap or just a
  placement-suggestion problem — turned out to be a real gap: this
  file's own Water Pump row was WRONG, `world.placeExtractor` never
  actually supported it (no resource-node equivalent exists for water).
  Fixed with a real, purpose-built mechanism
  (`world.constructWaterPumpNearReference`/`world.waterVolumes`) rather
  than just leaving it as a documented limitation.
- **2026-08-31 (later still)**: User asked whether this project could
  plan a full large-scale water pump field (locate water, lay out rows
  of pumps, group into pipe junctions until saturated, route pipes and
  power lines cleanly back to the main project). Answer was "partially"
  with two real gaps, both closed in this pass: added
  `world.constructWaterPumpAtPosition` (seed the first pump from
  scratch, no reference required) and
  `controller/satisfactory_ai/water.py` (row/field layout planner using
  the real `max_producers_per_pipe` flow-saturation math already built
  for pipes generally, plus a new generic
  `group_producers_by_flow_capacity` for merging rows into higher
  trunks). Also incorporated the user's note about real stackable
  support recipes (`Recipe_PipeSupportStackable`/
  `Recipe_ConveyorPoleStackable`) via `layout.plan_shared_support_columns`.
- **2026-08-31 (later still)**: Two follow-up corrections from the
  same user in one message: (1) `water.py`'s foundation placement was
  WRONG - it centered foundations directly under each pump, but a
  Water Pump sits in real water, a foundation can't share that spot;
  fixed to lay out a walkway strip ALONGSIDE the row instead
  (`foundation_offset` param added). (2) User asked to add real
  stackable-support construction rather than just modeling it as a
  layout idea - added `world.constructStackableSupport`, the first RPC
  in this project to drive the Zoop mechanic via `SetZoopAmount()`
  directly. See the new dedicated Stackable supports row above.
- **2026-08-31 (later still)**: User corrected the primary stacking
  workflow: "the stackable supports go vertical and can also be mixed
  so belts and pipes can be stacked interchangeably, usually as
  multiple separate attachments, not in one instantaneous placement."
  `stackCount`/Zoop only produces uniform same-recipe instances, so it
  can't mix tiers — added `world.constructStackableSupportOnTop`
  (reference-relative, one call per level, uses the reference's real
  `GetStackHeight()`) as the mechanism that actually supports mixing
  `Recipe_PipeSupportStackable`/`Recipe_ConveyorPoleStackable`/
  `Recipe_HyperPoleStackable` in one column. Both RPCs share an
  internal helper; `constructStackableSupport`'s doc comment now flags
  `stackCount` as the secondary, same-recipe-only case.
- **2026-08-31 (live test session)**: game running, player at a real
  ocean shoreline with a reference pump - first live test of the
  water-pump RPCs. `constructWaterPumpNearReference` CONFIRMED WORKING,
  plus a real minimum-spacing data point (fails at 2000 units, succeeds
  at 3000+, closing a previously-unconfirmed planner input).
  `constructWaterPumpAtPosition` CONFIRMED WORKING for genuine in-water
  positions - but its on-land negative-path test CRASHED THE GAME
  (`mSnappedExtractableResource` assert, same failure class as the
  already-documented extractor/spline-snapped crashes - a nearest-by-
  distance volume fallback let a dry-land point through as a fake
  target). Root-caused and fixed same session
  (`AFGWaterVolume::EncompassesPoint()` now a hard requirement), both
  targets compiled clean, but the crash killed the running game -
  **fix not yet redeployed/re-verified live**. Full writeup in
  `docs/placement-lessons.md`.
