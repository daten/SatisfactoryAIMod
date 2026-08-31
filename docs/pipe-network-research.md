# Pipe network research (fluid mechanics) — 2026-08-31

Recorded during an offline research session (user away, no live game
running) at the user's explicit request to prepare for pipe-network
planning: headlift/pumps, gravity/elevation, flow-rate capacity across
branches, and multi-extractor site topology. Covers the **fluid
mechanics** side specifically — for the extraction-site sequencing side
(Resource Well Pressurizer/satellites, activation states), see
`docs/resource-well-research.md`, which already covers that thoroughly
and is not duplicated here.

**Same caveat as every other research doc in this project**: the
`Source/FactoryGame/Private/**/*.cpp` files in this workspace are
UHT-stub-generated (empty bodies) — declarations, class hierarchy, real
public getters, and doc comments are real and citable; internal
*numeric constants and logic* are not readable from source. Nothing
below claims a specific headlift-in-meters or flow-limit-in-m³/s number
for a specific tier — those are only knowable by querying the CDO live
(`world.pipelinePumpTiers`/`world.pipelineTiers`), which has not
happened yet this session (no game running).

## The core mechanic: liquids get a real gravity/pressure simulation, gas doesn't

`Source/FactoryGame/Public/FGPipeNetwork.h` (`AFGPipeNetwork`) runs two
**entirely separate** physics paths depending on `mFluidForm`
(`EResourceForm::RF_LIQUID` vs `RF_GAS`):

- **Liquid**: `TickPhysics` → `CreatePressureGroup`/
  `FindTopMostPressureGroupIndex`/`UpdatePressureGroups`/
  `UpdatePropagatedPressure`/`UpdatePressure`/`PostUpdatePressureGroups`/
  `PreUpdateFlow`/`UpdateFlow`/`UpdateContent`. A genuine pressure-group
  simulation: `FPressureGroup` (private struct) tracks
  `HighestPumpZ`/`HighestElevationZ` **in meters, not Unreal units** per
  connected group of pipes. This is the real mechanism behind "a liquid
  source at a higher elevation than where it's consumed doesn't need a
  pump" — confirmed from the struct's own field comments
  (`"World Z values for different pressure points in meters, NOT unreal
  units"`), matching the user's own description exactly.
- **Gas**: `TickPhysics_Gas`/`UpdatePressure_Gas`/`PreUpdateFlow_Gas`/
  `UpdateFlow_Gas`/`UpdateContent_Gas` — a fully separate, simpler set
  with **no elevation/pressure-group equivalent at all**. Confirmed from
  source, not inferred: there is no `CreatePressureGroup`-style function
  in the gas path, and `FPressureGroup` itself is only referenced from
  the liquid functions. **Gas pipe networks are not subject to the
  elevation/headlift concern liquids are** — matches the user's own
  statement exactly ("Gasses that flow through pipes do not have this
  concern from what I understand").

`PipeJunction` (also in `FGPipeNetwork.h`) is the per-connection-pair
unit both simulations tick every frame: `PreviousOutflowZ`/
`CurrentOutflowZ` (per-junction Z height), `Flow` (`[m^3/s]`, same unit
`world.pipelineTiers`' `flowLimit` already uses), `ShouldBreakPressureGroup`
(some junctions/attachments can split one physical network into two
independent pressure groups — not yet identified which buildable
classes actually set this), and `PreviousZeroPressure`/
`CurrentZeroPressure` ("used to fake 'suction' on pumps" per its own
comment — how a pump creates a low-pressure draw on its input side).

## Pumps add headlift, NOT flow rate

`Source/FactoryGame/Public/Buildables/FGBuildablePipelinePump.h`
(`AFGBuildablePipelinePump : AFGBuildablePipelineAttachment`) has an
explicit disclaimer comment worth quoting directly:

> "in our fluid model, pump pressure is measured in meters. That unit
> is called the pump head in the real world... we refer to it as
> pressure, as that is the real world phenomena we want to model in a
> simplified way."

Real public getters (all `BlueprintPure`, no reflection needed — now
exposed via the new `world.pipelinePumpTiers` RPC, see below):
- `GetMaxHeadLift()` — absolute ceiling in meters. Above `design`,
  "working outside of its specifications" per the header's own comment,
  but still functional up to `max`.
- `GetDesignHeadLift()` — the rated/recommended operating point in
  meters. Budget real elevation gain against this first.
- `GetDefaultFlowLimit()`/`GetFlowLimit()`/`GetFlowLimitPct()` — `[m^3/s]`.
  **Critically, `UpdateDefaultFlowLimit()`'s own comment says this is
  "Updates the maximum flow limit from the neighbouring pipes"** — a
  pump's own flow ceiling is DERIVED FROM whatever pipe tier it's
  actually connected to, not a fixed pump-specific value. **A pump does
  not raise a network's throughput ceiling** — only genuinely parallel
  pipes do that (see below). Pumps also support `SetUserFlowLimit()` (a
  manual valve, `-1` = fully open) for deliberately throttling a run
  below the pipe's natural max, if that's ever useful for balancing.

## Each pipe segment has its own real volume, and fills sequentially

Per the user's own description, added to this research before it
becomes directly relevant: pipe segments have real volume proportional
to length, fill sequentially from the source, and an imbalanced/
starved network can "slosh." All confirmed directly from source
(`Source/FactoryGame/Public/FGFluidIntegrantInterface.h`'s `FFluidBox`
struct) — this is not speculation:

- **"A fluid box is a simulation unit in the fluid system, it has a
  volume and keeps track of pressures, flow etc."** (the struct's own
  top comment). **Each individual `AFGBuildablePipeline` segment is its
  own independent `FFluidBox`** (`mFluidBox` member, `GetFluidBox()`
  override) — not the whole network as one unit. `Content`/`MaxContent`
  are both real `[m^3]` fields. Paired with `AFGBuildablePipeBase::
  GetLength()` ("Length of the pipe in centimeters," a real public
  getter), this confirms the length-to-volume relationship: a longer
  segment has more volume. Real per-tier constants (how many m³ per cm
  of a Pipeline Mk1 vs Mk2) are not decodable from source, only
  queryable live via the new `world.pipeFluidBoxes` RPC (see below).
- **`FlowFill`/`FlowDrain` are explicitly documented**: "Fill and drain
  is how fast the box is filling up or emptying in [m^3/s]." This is
  the real mechanism behind sequential fill: a box's `Content` climbs
  via `FlowFill` until some threshold, before the NEXT segment's own box
  starts receiving meaningful flow - the doc comment doesn't spell out
  the exact per-box handoff logic (only `.cpp`-body-level, not readable
  in this SDK), but the existence of distinct per-box fill/drain state,
  updated by the network's own `UpdateFlow`/`UpdateContent`
  junction-pair functions (`FGPipeNetwork.h`), structurally confirms
  segments are filled and drained as discrete units rather than the
  whole pipeline updating uniformly.
- **A real, documented overfill/pressure-buildup mechanic exists,
  plausibly the source of "sloshing"**: `MaxOverfillPct` (a fraction,
  e.g. the struct's own worked example uses 40%) lets a box hold MORE
  than `MaxContent`, and part of that overfill contributes to real
  pressure (`OVERFILL_USED_FOR_PRESSURE_PCT`) - "Overfilling is what
  creates pressure in the pipes" per the struct's own comment. Notably,
  the source also documents `PRESSURE_LOSS`, "A small damping factor...
  so we cannot use our own pressure to pump up ourselves" - i.e. the
  developers explicitly engineered against a feedback-instability risk
  in this exact mechanic. This is real, corroborating evidence for the
  user's "sloshing effects"/slow-or-never-stabilizing-network concern -
  not just community folklore, a documented simulation characteristic
  the developers themselves had to guard against.
- **Explicit caveat directly relevant to "telemetry may produce chaotic
  results"**: `FlowThrough`/`FlowFill`/`FlowDrain` are documented as
  "not used for any simulations only for feedback... For the
  simulation see the junction pairs in the network" - meaning these
  are real but DERIVED/reactive values, not the authoritative
  simulation state. Expect exactly what the user anticipated: noisy,
  transient readings during startup or under starved/imbalanced
  consumption, not clean steady-state numbers, especially before a
  network has been running long enough to settle (if it settles at
  all without deliberate pre-priming).

**New capability, NOT YET LIVE-TESTED**: `world.pipeFluidBoxes` RPC -
per-segment `contentM3`/`maxContentM3`/`fillPct`/`maxOverfillPct`/
`flowThrough`/`flowFill`/`flowDrain`/`flowLimit`/`pressureColumn`/
`elevationPressureColumn`/`addedPressure`/`pressureGroup`/`z`, plus the
segment's real `lengthCm`. Scoped to `AFGBuildablePipeline` only for
this first pass (pumps/tanks/other `IFGFluidIntegrantInterface`
implementers are not included - a real, separate future widening if
their fluid-box state turns out to matter for planning too). This is
the RPC to reach for once observing real fill/sloshing/manifold-balance
behavior becomes relevant, per the user's own framing ("before it
becomes relevant later").

**Manifold load-balancing, not yet independently confirmed from
source**: the user also described a linear manifold feeding multiple
consumers from one high-rate pipe potentially favoring the
first-in-line machines, with slow or uncertain convergence to a
balanced state unless the network is deliberately pre-filled before
consumers start drawing. This is consistent with everything confirmed
above (per-segment sequential fill, pressure/overfill-driven flow
rather than an even split at junctions - see "Flow-rate capacity"
below) but the SPECIFIC claim about linear-manifold ordering bias
wasn't independently verified against a dedicated source passage this
session - treat it as a real, plausible planning consideration (worth
designing around, e.g. preferring a tree/balanced junction topology
over a long daisy-chain of consumers) rather than a source-confirmed
fact the way the elevation/overfill mechanics above are.

## Flow-rate capacity: a pipe segment has a hard ceiling regardless of pumps

`world.pipelineTiers`' `flowLimit` (already exposed, live-queryable) is
the real per-tier ceiling in `[m^3/s]`. Exceeding it for a given total
required flow needs genuinely separate parallel pipe runs — this is
exactly the "a fully overclocked pressurized well may require 3 or 4
parallel pipelines" scenario the user described.

**Junctions have no special per-class flow-splitting logic.**
`AFGBuildablePipelineJunction` (`FGBuildablePipelineJunction.h`) is a
nearly-empty class — no override of anything flow-related. Both the
3-way (`Recipe_PipelineJunction_T`) and 4-way
(`Recipe_PipelineJunction_Cross`) junctions rely entirely on the shared
`AFGPipeNetwork` pressure/flow simulation described above, same as any
other multi-connector meeting point. **This means pipe flow does NOT
split evenly like a conveyor splitter's round-robin item distribution**
— it's governed by relative pressure and downstream demand at each
branch. Planning an even, predictable split across branches is
therefore NOT something the network mechanics do automatically the way
belts do; a real build needs to size each branch's own consumption
independently rather than assume equal division. (Storage Tanks and
both Junction types share the same `PCT_ANY` connector type — no
`PCT_PRODUCER`/`PCT_CONSUMER` distinction to lean on for planning,
confirmed via `world.pipeConnections` earlier this project.)

## Multi-extractor site topology: each satellite extractor is its own separate pipe source

See `docs/resource-well-research.md` for the full activation-sequencing
story. The piece relevant to pipe planning specifically:
`AFGBuildableFrackingActivator` (the Pressurizer) is **not** an
`AFGBuildableResourceExtractor` and has no output inventory/pipe of its
own — it only coordinates. Each `AFGBuildableFrackingExtractor`
(satellite) is a real, individually-clocked
`AFGBuildableResourceExtractor` with its own pipe output, built
separately at each satellite node (a real site can have several —
7 satellites were observed live in one cluster on 2026-08-27, though
only 3 needed extractors for that test).

`AFGBuildableFrackingActivator::GetPotentialExtractionPerMinute()`/
`GetDefaultPotentialExtractionPerMinute()` report the **combined**
telemetry rate across every connected extractor (a UI/summary
convenience), but the real fluid physically flows through each
extractor's own separate pipe connector — matching the user's own
framing exactly ("a few of the individual extractors will maximize one
pipe. So they can't all be joined together"). Planning a real build
means: query each satellite's real extractor output rate individually
(clock speed × node purity × recipe base rate, same math
`controller/satisfactory_ai/production.py` already uses for other
extractors), then use the new `max_producers_per_pipe`/
`required_parallel_pipes` calculators (see below) to decide which
extractors' pipes can safely join on one run vs. need to stay separate
or converge only partway.

## New toolkit, added this session (all NOT YET LIVE-TESTED)

**`world.pipelinePumpTiers`** (new RPC) — real `maxHeadLift`/
`designHeadLift` (meters) and `defaultFlowLimit` (`[m^3/s]`) per pump
tier, read from real public getters, no reflection. Full param/response
shape in `RPC_REFERENCE.md`. **Real open question, not yet confirmed**:
whether a never-placed CDO's getters return meaningful defaults, since
`GetDefaultFlowLimit()` is documented as deriving from "neighbouring
pipes" that a CDO obviously doesn't have — first live call should check
whether the reported values look sane (non-zero, roughly matching the
in-game build-menu tooltip) before trusting them for real planning
math.

**`controller/satisfactory_ai/pipes.py`**, four new pure calculator
functions (docstrings have the full rationale/caveats, summarized
here):
- `required_parallel_pipes(total_flow, tier)` — flow budget: how many
  parallel pipes of this tier to carry a total rate.
- `max_producers_per_pipe(tier, per_producer_flow)` — inverse: how many
  equal-rate producers can share one pipe before it needs to split.
- `pump_required_for_elevation(elevation_gain_meters)` — pure sign
  check for LIQUIDS ONLY; do not call for gas.
- `required_pumps_for_elevation(elevation_gain_meters, pump_tier, use_design_limit=True)`
  — height budget: how many pumps in series to overcome a net-uphill
  climb, LIQUIDS ONLY.

All four are unit-tested with sample values (not live game data) during
this offline session — see the commit that introduces them for the
exact sample-value check. Same "toolkit, not solver" posture as the
rest of `satisfactory_ai`: these answer one question each about
already-known numbers; they don't fetch telemetry or choose a network
topology.

## Genuinely open / not yet built

- **No RPC exists yet to construct a Pipeline Pump or Junction as a
  standalone attachment.** `world.connectPipe`/`ConstructPipe` connects
  two existing buildables' free pipe connectors with a spline pipe
  segment — it does not place an attachment (pump, junction, valve,
  storage tank) as its own step. Building that (likely mirroring
  `ConstructBuildingAtPosition`'s generic placement path, since pumps/
  junctions are ordinary `AFGBuildable`s, not spline-snapped) is
  unstarted work, needed before any of the headlift math above can be
  validated against a real build.
- **Real headlift-in-meters and flow-limit-in-m³/s values for each pump/
  pipe tier are unknown until queried live** — `world.pipelinePumpTiers`
  exists now but has never actually been called against a running game.
- **Which buildable classes set `PipeJunction::ShouldBreakPressureGroup`**
  is not identified — worth checking if a live multi-segment build
  behaves unexpectedly (e.g. a Valve or Storage Tank might isolate
  pressure groups on either side, changing how far a single pump's
  headlift budget actually reaches).
- **Real per-tier extractor output rates** (Water Extractor, Oil
  Extractor, fracking satellites at various purities/clocks) are
  data-driven (recipe + node purity + clock%), already computable via
  `world.recipeCatalog` + `world.resourceNodes`, same pattern
  `production.py` already uses elsewhere — not pipe-specific research,
  just noting it's the missing input the new flow calculators need to
  be useful for a real site.
- **`world.pipeFluidBoxes` exists now but has never been called against
  a running game** — same "unconfirmed live" caveat as
  `world.pipelinePumpTiers`. In particular, whether `fillPct`/
  `flowFill`/`flowDrain` actually show the noisy/transient behavior
  during startup that this research predicts (vs. converging cleanly)
  is a real, interesting thing to check the first time this gets
  polled during an actual fill.
- **Linear-manifold consumer-ordering bias** (the user's description:
  feeding several consumers in a daisy-chain off one high-rate pipe may
  favor the first-in-line machines, converging slowly or not at all
  without deliberate pre-priming) is plausible and consistent with
  everything else confirmed here, but was NOT independently verified
  against a specific source passage this session — flagged as a real
  planning consideration, not yet a source-confirmed fact the way the
  elevation/overfill mechanics are.
