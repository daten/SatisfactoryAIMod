# Telemetry protocol v1

Defined by PLAN.md Phases 5-6 / Task 8-9, expanded in Phase 10. These are
the concrete JSON shapes `UAIModFunctionLibrary`'s `Log*AsJson` functions
produce, and what PLAN.md Phase 9's `/rpc` endpoint
(`UAIModHttpServerSubsystem`) returns in the `result` field for each
`method`. See [networking-research.md](networking-research.md) for the RPC
envelope itself (`{"protocolVersion","requestId","method"}` request →
`{"protocolVersion","requestId","success","result"|"error"}` response).

## resourceNodes (`method: "world.resourceNodes"`)

```json
{
  "protocolVersion": 1,
  "resourceNodes": [
    {
      "id": "/Game/Maps/Game/Grasslands/Grass_Terrain.Grass_Terrain:PersistentLevel.BP_ResourceNodeOreIron_C_1",
      "resource": "Iron Ore",
      "resourceClass": "/Game/FactoryGame/Resource/RawResources/OreIron/Desc_OreIron.Desc_OreIron_C",
      "purity": "Pure",
      "position": { "x": 12345.0, "y": -6789.0, "z": 890.0 },
      "occupied": false
    }
  ]
}
```

## Field notes

- `protocolVersion` — currently always `1`. Bump when the shape changes,
  per CLAUDE.md's protocol versioning requirement.
- `id` — **not save-stable.** Currently `AActor::GetPathName()` of the
  resource node actor. Unique within a running session/map load, but not
  guaranteed to survive save/load, actor respawn, or map transition. See
  [resource-node-research.md](resource-node-research.md) §4 and
  `FAIModResourceNodeTelemetry`'s header comment
  (`Mods/GameFeatures/AIMod/Source/AIMod/Public/AIModTelemetryTypes.h`).
  PLAN.md Phase 7 will need to replace this with a real stable identifier
  design before this telemetry is used for anything beyond debug logging.
- `resource` — human-readable name from `UFGItemDescriptor::GetItemName()`,
  e.g. `"Iron Ore"`.
- `resourceClass` — the resource descriptor's full object path
  (`UClass::GetPathName()`), for callers that want the actual asset
  reference rather than the display string.
- `purity` — one of `"Impure"`, `"Normal"`, `"Pure"`, mapped manually from
  the raw `EResourcePurity` enum (`GetResourcePurity()`). **Not**
  `GetResourcePurityText()`, despite that looking like the obvious choice
  — confirmed against a real save (2026-08-24) that it returns Slate rich
  -text UI markup, not plain text (`"<Bold>(Normal)</>"`), caught by
  `docs/self-test.md`'s automatic self-test on its first real run. See
  the fix commit and `ResourcePurityToString()` in
  `AIModFunctionLibrary.cpp`.
- `position` — world-space `{x, y, z}` in Unreal units (centimeters), from
  `AActor::GetActorLocation()`.
- `occupied` — whether an extractor currently occupies the node
  (`IFGExtractableResourceInterface::IsOccupied()`). Does **not** identify
  *which* extractor — see resource-node-research.md §5; that requires a
  separate reverse lookup this phase does not implement.

## buildables (`method: "world.buildables"`)

```json
{
  "protocolVersion": 1,
  "buildables": [
    {
      "id": "/Game/Maps/.../Persistent_Level.Persistent_Level:PersistentLevel.Build_ConstructorMk1_C_1",
      "buildableClass": "/Game/FactoryGame/Buildable/Factory/ConstructorMk1/Build_ConstructorMk1.Build_ConstructorMk1_C",
      "position": { "x": 100.0, "y": 200.0, "z": 300.0 },
      "rotation": { "pitch": 0.0, "yaw": 90.0, "roll": 0.0 }
    }
  ]
}
```

Generic fields for placed buildables — conveyors, power poles, machines,
foundations, everything. `id` has the same session-local-only caveat as
resource nodes (see below). `buildableClass` is the buildable's
`UClass::GetPathName()`. Source: `docs/buildable-research.md` §1.

**Enumeration caveat:** tries `AFGBuildableSubsystem::GetAllBuildablesRef()`
first (a real public getter, unlike the resource node manager), falling
back to a `TActorIterator<AFGBuildable>` scan if the subsystem is
unavailable. Whether the subsystem is actually populated at runtime is
unverified from source (its `.cpp` is a stub) — see
[manual-verification.md](manual-verification.md).

**Two different id shapes, 2026-08-25:** most buildables are real
`AFGBuildable` actors and get a `GetPathName()`-based id
(`/Game/Maps/.../Build_X_C_1`). Foundations — and likely other
mass-placed buildables — are NOT actors at all; they're stored via
`AFGLightweightBuildableSubsystem` for performance at scale, and get an
id shaped `"lightweight:<BuildableClassPath>|<Index>"` instead (identity
is `(class, array index)`, not a path). See
`docs/lightweight-buildable-research.md` for the full discovery and why
both id shapes are handled by `DismantleBuildable`/`world.deleteBuilding`.
Callers should treat `id` as an opaque string either way — never parse
it to infer whether something is "real" vs. lightweight, since that's
an implementation detail that could change.

## manufacturers (`method: "world.manufacturers"`)

```json
{
  "protocolVersion": 1,
  "manufacturers": [
    {
      "id": "/Game/Maps/.../Persistent_Level.Persistent_Level:PersistentLevel.Build_ConstructorMk1_C_1",
      "buildableClass": "/Game/FactoryGame/Buildable/Factory/ConstructorMk1/Build_ConstructorMk1.Build_ConstructorMk1_C",
      "position": { "x": 100.0, "y": 200.0, "z": 300.0 },
      "recipe": "Iron Plate",
      "clockSpeedPercent": 100.0,
      "productionStatus": "Producing",
      "productionProgress": 0.42,
      "productivity": 1.0,
      "inputInventory": [
        { "itemClass": "/Game/.../Desc_OreIron.Desc_OreIron_C", "itemName": "Iron Ore", "count": 45 }
      ],
      "outputInventory": [
        { "itemClass": "/Game/.../Desc_IronPlate.Desc_IronPlate_C", "itemName": "Iron Plate", "count": 12 }
      ]
    }
  ]
}
```

Only `AFGBuildableManufacturer` actors (Constructor/Assembler/
Manufacturer/Smelter/Refinery/...) — enumerated via
`TActorIterator<AFGBuildableManufacturer>`, not the buildable subsystem
(simpler, type-safe, and consistent with the resource-node approach).
Field notes:

- `recipe` — display name of the currently-set recipe
  (`UFGRecipe::GetRecipeName()`), or `""` if no recipe is set.
- `clockSpeedPercent` — `GetCurrentPotential() * 100` (100 = normal speed).
- `productionStatus` — one of `"None"`, `"Producing"`,
  `"ProducingWithCrystal"`, `"Standby"`, `"Error"` — a manual string
  mapping of `EProductionStatus`, which is a plain C++ enum, not a
  `UENUM` (no reflection-based name available).
- `productionProgress` — `[0,1]`, current production cycle progress.
- `productivity` — FactoryGame's own "how productive" measure, distinct
  from clock speed.
- `inputInventory`/`outputInventory` — item stacks currently held, via
  `GetInventoryStacks()`. Empty stacks are omitted.

Source: `docs/buildable-research.md` §2-4.

## connections (`method: "world.connections"`)

```json
{
  "protocolVersion": 1,
  "connections": [
    {
      "ownerBuildableId": "/Game/Maps/.../Build_ConstructorMk1_C_1",
      "direction": "Output",
      "connected": true,
      "connectedBuildableId": "/Game/Maps/.../Build_ConveyorBeltMk1_C_1",
      "position": { "x": 0.0, "y": 0.0, "z": 0.0 },
      "normal": { "x": 0.0, "y": 0.0, "z": 0.0 }
    }
  ]
}
```

One row per connection point on every `AFGBuildable` actor that has any
`UFGFactoryConnectionComponent` attached (machines, belts/lifts,
splitters/mergers — discovered generically via `AActor::GetComponents<>()`
rather than a per-class-hierarchy enumeration list, after two separate
live-found gaps in the class-specific approach: belts/lifts and then
splitters/mergers each turned out to be their own sibling hierarchy —
see `docs/buildable-research.md`'s correction notes), **not**
a constructed graph. A single physical belt/pipe link between two
buildings produces two rows: an `"Output"` row on the source and an
`"Input"` row on the destination, each naming the other via
`connectedBuildableId`. `direction` is one of `"Input"`, `"Output"`,
`"Any"`, `"SnapOnly"` (`EFactoryConnectionDirection`).
`connectedBuildableId` is `""` when `connected` is `false`. Source:
`docs/buildable-research.md` §6.

`position` (`UFGFactoryConnectionComponent::GetConnectorLocation()`, no
clearance offset) and `normal`
(`UFGFactoryConnectionComponent::GetConnectorNormal()`, i.e.
`GetComponentRotation().Vector()`) were added 2026-08-25 after a live
belt-routing investigation (see `docs/demo-production-chain.md`) needed
this exact data ad hoc, via one-off diagnostic log statements, to
explain a "belt geometrically impossible" `CanConstruct()` failure — a
straight Smelter(output)→Constructor(input) belt failed because the two
connectors' normals weren't compatible for the buildings' relative
positions. For an `"Output"` connection, items leave the building moving
in `+normal`; for an `"Input"` connection, items must arrive moving in
`-normal` (approaching from outside along `+normal`, then entering along
`-normal`). A straight, no-bend belt between two fixed (unrotated)
buildings requires the destination's `normal` to equal the negation of
the source's `normal`, with the destination positioned further along the
source's `+normal` direction than its clearance extends. See
`controller/satisfactory_ai/layout.py` for a small toolkit of
composable geometry primitives (connector-compatibility checks,
local/world coordinate transforms, single-candidate placement
computation) built on this data — deliberately NOT a "solve the whole
layout" function per CLAUDE.md's LLM/deterministic-code split: the tools
answer geometry questions (is this pair compatible? where would this
connector end up at position P, yaw Y? what position aligns connector A
with connector B?) for an agent to compose and iterate over, rather than
pre-determining a layout the agent can't reconsider or optimize.

**Building the actual graph from these rows is PLAN.md Phase 11's job,
and belongs on the external controller, not the mod** — see
`controller/satisfactory_ai/graph.py`'s `build_world_graph()`, which
takes the `buildables` and `connections` payloads and produces a directed
graph (one edge per connected `"Output"` row, since `"Input"` rows are
the same physical link seen from the other end).

## pipeConnections (`method: "world.pipeConnections"`)

```json
{
  "protocolVersion": 1,
  "connections": [
    {
      "ownerBuildableId": "/Game/Maps/.../Build_PipeHyperStart_C_1",
      "connectionType": "Any",
      "isHypertube": true,
      "connected": false,
      "connectedBuildableId": "",
      "position": { "x": 0.0, "y": 0.0, "z": 0.0 },
      "normal": { "x": 1.0, "y": 0.0, "z": 0.0 }
    }
  ]
}
```

Same shape and purpose as `connections` above, for `UFGPipeConnectionComponentBase`
— **fluid pipes AND Hypertube tubes**, discovered the same generic
`AActor::GetComponents<>()` way (added 2026-08-27 after discovering live
that `world.connections` only ever covered `UFGFactoryConnectionComponent`,
leaving no way to read a real pipe/hypertube connector's position/normal
before placing one). `connectionType` is one of `"Any"`, `"Producer"`,
`"Consumer"`, `"SnapOnly"` (`EPipeConnectionType`) — fluid pipe machines
generally use `"Producer"`/`"Consumer"`, Hypertube connectors stay at the
CDO default `"Any"` (confirmed from source — see
`docs/hypertube-research.md`), and `"SnapOnly"` connectors (wall
supports/poles) are structural snap points, not real endpoints.
`isHypertube` is `true` for `UFGPipeConnectionComponentHyper` (a
type-tag-only subclass with no added members) and `false` for a regular
fluid `UFGPipeConnectionComponent`. Same `normal`/docking convention as
`connections`: a straight run needs the destination's `normal` to equal
the negation of the source's `normal` — confirmed live this matters for
Hypertube entrances specifically, which have exactly one connector each,
so two entrances placed at default rotation both face the same world
direction and need one rotated 180° for a straight (non-curving) tube.

## conveyorBeltTiers (`method: "world.conveyorBeltTiers"`)

```json
{
  "protocolVersion": 1,
  "tiers": [
    {
      "recipeClass": "/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorBeltMk1.Recipe_ConveyorBeltMk1_C",
      "buildableClass": "/Game/FactoryGame/Buildable/Factory/ConveyorBeltMk1/Build_ConveyorBeltMk1.Build_ConveyorBeltMk1_C",
      "speed": 0.0,
      "maxSplineLength": 5600.1,
      "bendRadius": 0.0,
      "maxInclineDegrees": 0.0
    }
  ]
}
```

Added 2026-08-25 alongside making `world.connectConveyor`/
`world.testConveyorBelt`'s `recipeClass` a real parameter (previously
hardcoded to `Recipe_ConveyorBeltMk1`) — one row per
`Recipe_ConveyorBeltMk1`..`Mk6` (all six exist on disk), `speed` being
`AFGBuildableConveyorBase::GetSpeed()` read live off each tier's
buildable class CDO (`LoadObject` + `GetDefaultObject`), not a
hardcoded or assumed items-per-minute table. **`speed`'s exact unit is
unconfirmed** — `FGBuildableConveyorBase.h` only comments the backing
field as `"Speed of this conveyor"`, no unit given, so it is NOT
guaranteed to be items-per-minute directly. Treat it as
relative/comparable across tiers (a bigger number is a faster belt)
until a live comparison against the game's own displayed
items-per-minute figures confirms the exact conversion. A tier whose
class fails to resolve is simply omitted, not a hard error.

`maxSplineLength`/`bendRadius` (`AFGConveyorBeltHologram`'s public
`GetMaxSplineLength()`/`GetBendRadius()`) and `maxInclineDegrees`
(`mMaxIncline`, degrees — no public getter, read via a single
hardcoded reflection lookup) come from the belt's **hologram** class
CDO, a different descriptor accessor than the buildable class `speed`
is read from. A field is simply omitted (not a hard error) for any
tier whose hologram class doesn't resolve.

`controller/satisfactory_ai/conveyors.py` has three small toolkit
functions (not a solver) built on this data: `select_cheapest_sufficient_tier()`
picks the cheapest tier meeting a minimum speed;
`is_straight_segment_feasible()` checks a candidate straight segment's
distance/incline against a tier's limits before attempting it;
`satisfactory_ai.layout.compute_waypoint_positions()` (re-exported
here, generic geometry) computes evenly-spaced candidate anchor points
for chaining multiple belt segments (via intermediate
`Recipe_ConveyorPole` placements — confirmed present on disk, not yet
live-tested) when a route is too long or steep for one segment. The
caller decides all thresholds and validates the result live — same
posture as `satisfactory_ai.layout`.

## powerLineLimits (`method: "world.powerLineLimits"`)

```json
{
  "protocolVersion": 1,
  "recipeClass": "/Game/FactoryGame/Recipes/Buildings/Recipe_PowerLine.Recipe_PowerLine_C",
  "buildableClass": "/Game/FactoryGame/Buildable/Factory/PowerLine/Build_PowerLine.Build_PowerLine_C",
  "maxLength": 0.0,
  "maxPowerTowerLength": 0.0,
  "lengthPerCost": 0.0
}
```

Added 2026-08-25, directly motivated by the user asking whether power
cables (like conveyors) need distance-limit/intermediate-pole
handling — they do. Unlike `conveyorBeltTiers`' `speed`, these ARE
documented-unit values: `AFGBuildableWire::mMaxLength`/
`mMaxPowerTowerLength`/`mLengthPerCost` are plain **public**
`EditDefaultsOnly` `UPROPERTY`s commented `"[cm]"` in
`FGBuildableWire.h` — a straight member read off `Recipe_PowerLine`'s
buildable CDO, no reflection needed, and `maxLength` is directly
comparable to a computed 3D distance with no unknown-conversion
caveat. Only one power line tier exists (no Mk1..N like belts), so
this is a flat object, not an array — all fields are omitted (just
`protocolVersion` remains) if the mod couldn't resolve the CDO.

`ConstructPowerConnection`/`world.connectPower`'s source/dest were
already generic before this addition — `FindFreePowerConnection`
searches any `AFGBuildable` for a free `UFGPowerConnectionComponent`,
not hardcoded to machines — so a real power pole
(`Recipe_PowerPoleMk1`/`Mk2`/`Mk3`, confirmed present on disk) placed
at each waypoint should already work as an intermediate relay for a
connection exceeding `maxLength`, chaining multiple
`world.connectPower` calls, with **no C++ changes needed for that
part** — only untested live, same as belt chaining.
`controller/satisfactory_ai/power.py`'s `is_direct_connection_feasible()`
checks a candidate connection's distance against `maxLength`; it
re-exports `compute_waypoint_positions()` for the same chaining
pattern. Separately, per the user's own earlier note this session
(`docs/conveyor-power-connection-research.md`): a machine's default
single power connection slot may require routing through a pole even
for a *short* connection if the later-game daisy-chain unlock isn't
active in the current save — a real constraint distinct from the
`maxLength` distance question this section addresses.

## pipelineTiers (`method: "world.pipelineTiers"`)

```json
{
  "protocolVersion": 1,
  "tiers": [
    {
      "recipeClass": "/Game/FactoryGame/Recipes/Buildings/Recipe_Pipeline.Recipe_Pipeline_C",
      "buildableClass": "/Game/FactoryGame/Buildable/Factory/Pipeline/Build_Pipeline.Build_Pipeline_C",
      "flowLimit": 0.0,
      "maxSplineLength": 0.0,
      "bendRadius": 0.0,
      "minBendRadius": 0.0
    }
  ]
}
```

Added 2026-08-25, pipe groundwork directly motivated by the user asking
to prepare pipe handling ahead of the next live testing session — same
treatment already given to belts/power above. **NOT YET
LIVE-TESTED.** One row per `Recipe_Pipeline`/`Recipe_PipelineMK2` (note
the capital `MK2`, unlike belts' `Mk2` — confirmed from the actual
filename on disk). `flowLimit` is `AFGBuildablePipeline::GetFlowLimit()`,
a **public**, documented-unit value (`"Maximum flow through this pipe
in cubic meters. [m^3/s]"` per `FGBuildablePipeline.h`'s own doc
comment) — unlike belts' `speed`, no unit-conversion caveat.
`maxSplineLength`/`bendRadius`/`minBendRadius` come from
`AFGPipelineHologram`'s CDO (`ResolvePipelineHologramClassForRecipe`,
the same `UFGBuildDescriptor::GetHologramClass` pattern used for belt
holograms) — **all three** are private `UPROPERTY(EditDefaultsOnly)`
fields with no public getter (unlike belts, where two of three had
public getters), read via the same single-hardcoded-field
`FindFProperty<FFloatProperty>` reflection technique. Any tier/field
that fails to resolve is simply omitted, not a hard error.

`AFGPipelineHologram` is a sibling of `AFGConveyorBeltHologram` — both
derive directly from `AFGSplineHologram`, confirmed from source — and
`ConstructPipe`/`world.connectPipe` mirrors `ConstructConveyorBelt`'s
two-click `TrySnapToActor`+`DoMultiStepPlacement` mechanism using
`UFGPipeConnectionComponentBase`/`EPipeConnectionType` (pipes' own
parallel connection-type hierarchy — `PCT_PRODUCER`/`PCT_CONSUMER`,
**not** `UFGFactoryConnectionComponent`/`EFactoryConnectionDirection`).
Two open questions flagged for the first live test: (1) the shared
`AFGSplineHologram` base has no `GetAnyConnectedBuildables()` (only
`AFGConveyorBeltHologram` declares that), so the post-end-click
diagnostic uses `IsConnectionSnapped(false)` instead — an indicator
already noted as not fully reliable even for belts; (2) **no
standalone `Recipe_PipelineSupport`/pole recipe was found on disk**
(unlike `Recipe_ConveyorPole` for belts or
`Recipe_PowerPoleMk1`/`Mk2`/`Mk3` for power), even though
`Build_PipelineSupport.uasset` exists as a buildable — pipe poles may
only be auto-spawned internally by the hologram's own
`mDefaultPipelineSupportRecipe` mechanism during a real multi-click
placement, meaning the belt/power "place a pole via
`world.placeBuilding`, then chain connect calls" pattern may **not**
transfer directly to pipes. This is an open question for the first
live pipe test, not assumed either way.

`controller/satisfactory_ai/pipes.py` mirrors `conveyors.py`'s toolkit
shape: `select_cheapest_sufficient_tier()` picks the cheapest tier
meeting a minimum flow; `is_straight_segment_feasible()` checks a
candidate segment's distance against `maxSplineLength` only (no
incline check — unlike belts, no incline-vs-limit relationship for
pipes has been confirmed from source); `compute_waypoint_positions()`
is re-exported for the same waypoint-chaining pattern, though whether
pipe chaining actually works the way belt/power chaining does is
exactly the open pole-recipe question above.

## conveyorAttachments (`method: "world.conveyorAttachments"`)

```json
{
  "protocolVersion": 1,
  "attachments": [
    {
      "recipeClass": "/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorAttachmentSplitter.Recipe_ConveyorAttachmentSplitter_C",
      "buildableClass": "/Game/FactoryGame/Buildable/Factory/CA_Splitter/Build_ConveyorAttachmentSplitter.Build_ConveyorAttachmentSplitter_C",
      "inputCount": 1,
      "outputCount": 3,
      "supportsSortRules": false
    }
  ]
}
```

Added 2026-08-25 in response to the user asking to research/verify/build
splitter+merger support ahead of a future session. **Live-tested
2026-08-27**: the example JSON above (`inputCount`/`outputCount`) is now
confirmed correct against a real session — but the *first* live test
found this method had been silently returning `inputCount`/
`outputCount` of `0`/`0` for every entry since it was written, because
`GetComponents<UFGFactoryConnectionComponent>()` on a class's CDO only
finds natively-added (`CreateDefaultSubobject`) components — these
buildables add their connectors via the Blueprint's Simple Construction
Script instead, which never populates onto the CDO. Fixed by switching to
`AFGBuildable::GetDefaultComponents<>()`, FactoryGame's own purpose-built
helper that additionally walks the Blueprint inheritance chain's SCS
nodes (resolving `InheritableComponentHandler` overrides). The same fix
was needed in `buildableCatalog` below (built with the same,
now-corrected pattern from the start of its live testing, so it never
shipped the bug) — see that section's note. See
`docs/conveyor-attachment-research.md` for the full
research trail; the key finding: splitters and mergers use
`AFGConveyorAttachmentHologram : AFGFactoryHologram :
AFGBuildableHologram` — the **same simple, single-step hologram lineage**
already proven for Miners/Smelters/Constructors, **not** the
`AFGSplineHologram` branch belts/pipes needed special multi-click
driving for. That means **placement and connection needed zero new
construction code**: `world.placeBuilding` already places any of the
five real recipes below, and `world.connectConveyor`'s
`FindFreeFactoryConnection` helper is already generic (a plain
`GetComponents<UFGFactoryConnectionComponent>()` scan by direction, not
hardcoded to any building class), so it correctly hands out whichever
of a splitter's 3 outputs (or a merger's 3 inputs) is still free on each
successive call — no `ConstructSplitter`-style wrapper was added,
deliberately, since it would just duplicate `ConstructBuildingAtPosition`.

One row per real recipe confirmed on disk: `Recipe_ConveyorAttachmentSplitter`,
`Recipe_ConveyorAttachmentSplitterSmart`, `Recipe_ConveyorAttachmentSplitterProgrammable`,
`Recipe_ConveyorAttachmentMerger`, `Recipe_ConveyorAttachmentMergerPriority`
(the Programmable Splitter shares Smart Splitter's native class,
`AFGBuildableSplitterSmart`; no "Smart Merger" exists). `inputCount`/
`outputCount` are read live off each variant's real
`UFGFactoryConnectionComponent`s via `GetDirection()` — not hardcoded to
the commonly-known 1-in/3-out / 3-in/1-out figures, since
`AFGBuildableConveyorAttachment`'s header doesn't declare them as a
literal constant. `supportsSortRules` is `true` only for the Smart/
Programmable variants — flags a **real, separate, not-yet-built**
capability: per-output item-type routing
(`AFGBuildableSplitterSmart::AddSortRule()`/etc., confirmed public) has
no RPC method yet, so a placed Smart/Programmable splitter connects
exactly like a plain one today but cannot yet be configured to route by
item type.

`satisfactory_ai.models.ConveyorAttachmentInfo`/
`satisfactory_ai.protocol.parse_conveyor_attachment_catalog_telemetry`
mirror this shape. No dedicated toolkit module was added (unlike
`conveyors.py`/`power.py`/`pipes.py`) — there's no distance/speed/flow
limit or chaining pattern to reason about here, just a placement +
generic-connection lookup already covered by existing tools.

## recipeCatalog (`method: "world.recipeCatalog"`)

```json
{
  "protocolVersion": 1,
  "recipes": [
    {
      "recipeClass": "/Game/FactoryGame/Recipes/Constructor/Recipe_IronPlate.Recipe_IronPlate_C",
      "displayName": "Iron Plate",
      "isBuildingRecipe": false,
      "manufacturingDuration": 6.0,
      "ingredients": [
        { "itemClass": "/Game/FactoryGame/Resource/Parts/IronIngot/Desc_IronIngot.Desc_IronIngot_C", "itemName": "Iron Ingot", "amount": 3 }
      ],
      "products": [
        { "itemClass": "/Game/FactoryGame/Resource/Parts/IronPlate/Desc_IronPlate.Desc_IronPlate_C", "itemName": "Iron Plate", "amount": 2 }
      ],
      "producedIn": ["/Game/FactoryGame/Buildable/Factory/ConstructorMk1/Build_ConstructorMk1.Build_ConstructorMk1_C"],
      "variablePowerConsumptionConstant": 0.0,
      "variablePowerConsumptionFactor": 0.0
    }
  ]
}
```

Added 2026-08-27 per explicit user request to support pre-planning
complex builds ("what recipes/alternates build each item, what machines
are needed, resource/power requirements, rates"). Backed by
`AFGRecipeManager::GetAllRecipes()` — **every recipe in the game,
including ones not yet unlocked** in the current save, unlike the
progression-gated `GetAllAvailableRecipes()`. Building recipes (whose
product is itself a `UFGBuildingDescriptor`) are included too, flagged
via `isBuildingRecipe` — that entry's `ingredients` is the building's real
construction cost; see `buildableCatalog` below for the resolved
buildable class and its power/connection data.

`amount` is `FItemAmount`'s raw internal unit — for liquid/gas items this
is thousandths of a cubic meter (same convention as
`GetStackSizeConverted()`'s own `/1000` behavior for fluid stack sizes),
**not pre-converted here**. Check the item's `"form"` via `itemCatalog`
before interpreting the number. Deliberately does not compute effective
production rates (items/min accounting for clock speed or Somersloop
boost) — combine `manufacturingDuration`/`ingredients`/`products` here
with `buildableCatalog`'s `minPotential`/`maxPotential` on the caller
side, per this project's toolkit-not-solver preference.

**IMPORTANT**: `AFGRecipeManager::Get()` and the private
`PopulateAllRecipesList()` behind these arrays are stub-source
(`Source/FactoryGame/Private/FGRecipeManager.cpp`) — `Get()` returns
`null` in Editor/PIE and only resolves to real data in the packaged/
Alpakit-deployed game. Test against the real Steam session.

## itemCatalog (`method: "world.itemCatalog"`)

```json
{
  "protocolVersion": 1,
  "items": [
    {
      "itemClass": "/Game/FactoryGame/Resource/Parts/IronPlate/Desc_IronPlate.Desc_IronPlate_C",
      "name": "Iron Plate",
      "form": "Solid",
      "isBuildingDescriptor": false,
      "stackSize": 100,
      "energyValue": 0.0,
      "radioactiveDecay": 0.0
    }
  ]
}
```

Companion to `recipeCatalog` — same `AFGRecipeManager`/stub-source
caveats apply. Backed by `GetAllItemDescriptors()`. `form` is one of
`"Solid"`/`"Liquid"`/`"Gas"`/`"Invalid"`. `gasType` (`"Normal"`/
`"Energy"`) is only present when `form` is `"Gas"`.
`isBuildingDescriptor` is `true` for the "item" a building recipe
produces (e.g. `Recipe_ConstructorMk1`'s product) — cross-check against
`recipeCatalog`'s `isBuildingRecipe`.

## buildableCatalog (`method: "world.buildableCatalog"`)

```json
{
  "protocolVersion": 1,
  "buildables": [
    {
      "recipeClass": "/Game/FactoryGame/Recipes/Buildings/Recipe_ConstructorMk1.Recipe_ConstructorMk1_C",
      "buildableClass": "/Game/FactoryGame/Buildable/Factory/ConstructorMk1/Build_ConstructorMk1.Build_ConstructorMk1_C",
      "category": "Manufacturer",
      "constructionCost": [
        { "itemClass": "...", "itemName": "Reinforced Iron Plate", "amount": 2 }
      ],
      "clearance": [
        {
          "min": { "x": -400.0, "y": -600.0, "z": 0.0 },
          "max": { "x": 400.0, "y": 600.0, "z": 800.0 },
          "size": { "x": 800.0, "y": 1200.0, "z": 800.0 },
          "type": "Default"
        }
      ],
      "runsOnPower": true,
      "idlePowerConsumption": 0.5,
      "producingPowerConsumptionBase": 4.0,
      "defaultProducingPowerConsumption": 4.0,
      "minPotential": 0.01,
      "maxPotential": 1.0,
      "canChangePotential": true,
      "overridesShardSlotCount": false,
      "factoryInputCount": 1,
      "factoryOutputCount": 1,
      "pipeInputCount": 0,
      "pipeOutputCount": 0,
      "powerConnectionCount": 1
    }
  ]
}
```

Derived from `recipeCatalog` (filters to `isBuildingRecipe == true`,
resolves each to its real `AFGBuildable` class via
`UFGBuildingDescriptor::GetBuildableClass()`) rather than a separate
enumeration source, so `constructionCost` always matches the recipe's own
ingredients. `category` is `"Generator"`/`"Extractor"`/`"Manufacturer"`/
`"Other"`, determined by C++ class hierarchy
(`AFGBuildableGenerator`/`AFGBuildableResourceExtractorBase`/
`AFGBuildableManufacturer`), not a FactoryGame-declared enum — covers the
buildings most relevant to production planning. Non-factory buildables
(foundations, walls, belts, poles...) still appear with category
`"Other"`, but the power/potential fields are **omitted entirely** rather
than reported as misleading zeros.

`clearance` (2026-08-27, per explicit user request to pre-plan layouts —
estimate foundation counts, and reserve space for belt/pipe routing gaps
— before placing anything) is present on **every** entry, not just
factory buildables. It's an array of the buildable's real
`mClearanceData` boxes — the exact same data FactoryGame's own
construction-overlap checks use, not an approximation — each with
`min`/`max`/`size` (in the buildable's local space, `RelativeTransform`
already applied) and `type` (`"Default"`/`"Soft"`/`"BlockEverything"`,
see `FGClearanceData.h`'s `EClearanceType`). Most buildables declare
exactly one box, but some declare more than one (e.g. a base volume plus
a separate one for an attached arm or platform) — an empty array means no
clearance data was found, not zero size. Retrieved via the
`IFGClearanceInterface` `BlueprintNativeEvent` on each buildable class's
CDO — safe, since (unlike connector components above) clearance data is
a plain class-default property, not something added via a Blueprint's
Simple Construction Script.

Power/potential fields (`runsOnPower` through `canChangePotential`) are
present for anything deriving from `AFGBuildableFactory` — Manufacturer,
Extractor, and Generator all do. `maxPotential` is explicitly the
un-overclocked baseline (documented on the class as "not accounting for
the installed power shards"), **not** the real ceiling with shards
inserted — live-confirmed both Constructor and Miner report `1.0` here.
`potentialShardSlots` is only present/meaningful when
`overridesShardSlotCount` is `true` — live-confirmed most buildings
(Constructor, Miner) report the override off, meaning the real slot count
falls back to a global default this per-building reflection read cannot
see (a real, documented gap, not a wrong number — do not treat a missing
`potentialShardSlots` as "0 slots"). `powerProductionCapacity`/
`defaultPowerProductionCapacity` (real MW output) are additionally
present only for `AFGBuildableGenerator`. All fields are read off each
buildable class's **CDO** (never spawned in the world) via
`AFGBuildable::GetDefaultComponents<>()` — **not** a plain
`GetComponents<>()` scan, which misses every connector added via a
Blueprint's Simple Construction Script (confirmed live: this bug affected
both this method and the pre-existing `conveyorAttachments`, silently
returning `factoryInputCount`/`factoryOutputCount`/`powerConnectionCount`
all `0` for every entry, until fixed 2026-08-27 — see that section above
for the same root cause). Class-level defaults only, same technique
`conveyorAttachments`/`conveyorBeltTiers`
already use. Same `AFGRecipeManager`/stub-source caveat as `recipeCatalog`
applies (Editor/PIE returns an empty catalog).

## targetedManufacturer (`method: "world.targetedManufacturer"`)

```json
{
  "protocolVersion": 1,
  "manufacturer": null
}
```

or, if the local player is currently looking at a manufacturer (via
`AFGCharacterPlayer::GetBestUsableActor()` — the game's own "what can I
press E on" state, the same one driving the interact prompt, not a
reimplemented line trace):

```json
{
  "protocolVersion": 1,
  "manufacturer": {
    "id": "...", "buildableClass": "...", "position": {...}, "recipe": "...",
    "clockSpeedPercent": 100.0, "productionStatus": "Producing",
    "productionProgress": 0.42, "productivity": 1.0,
    "inputInventory": [...], "outputInventory": [...]
  }
}
```

Same object shape as one entry in `manufacturers` (above).
`"manufacturer": null` means nothing is targeted or the targeted actor
isn't a manufacturer — not an error. Player index 0 only (single-player/
local session scope, per PLAN.md/CLAUDE.md). Added specifically so a
caller can target `world.setClockSpeed`/`world.setRecipe` at whatever the
player is currently looking at instead of picking a `buildableId` out of
the full `world.manufacturers` list — see
[chat-and-console-commands.md](chat-and-console-commands.md) for the
matching `AIMod.Target` console command / `/aimod target` chat
subcommand. Source:
`UAIModFunctionLibrary::GetTargetedManufacturer`/`LogTargetedManufacturerAsJson`
in `AIModFunctionLibrary.cpp`.

## Common field notes

- `protocolVersion` — currently always `1`. Bump when a shape changes,
  per CLAUDE.md's protocol versioning requirement.
- `id` (resourceNodes/buildables/manufacturers) — **not save-stable.**
  Currently `AActor::GetPathName()`. Unique within a running session/map
  load, but not guaranteed to survive save/load, actor respawn, or map
  transition. See [resource-node-research.md](resource-node-research.md)
  §4 / [buildable-research.md](buildable-research.md) §1 and
  `AIModTelemetryTypes.h`'s struct comments. PLAN.md Phase 7 must design
  a real stable identifier before this telemetry is used for anything
  beyond debug logging/local testing.
- `position` — world-space `{x, y, z}` in Unreal units (centimeters), from
  `AActor::GetActorLocation()`.

resourceNodes-specific:

- `resource` — human-readable name from `UFGItemDescriptor::GetItemName()`,
  e.g. `"Iron Ore"`.
- `resourceClass` — the resource descriptor's full object path
  (`UClass::GetPathName()`), for callers that want the actual asset
  reference rather than the display string.
- `purity` — one of `"Impure"`, `"Normal"`, `"Pure"`, mapped manually from
  the raw `EResourcePurity` enum (`GetResourcePurity()`). **Not**
  `GetResourcePurityText()`, despite that looking like the obvious choice
  — confirmed against a real save (2026-08-24) that it returns Slate rich
  -text UI markup, not plain text (`"<Bold>(Normal)</>"`), caught by
  `docs/self-test.md`'s automatic self-test on its first real run. See
  the fix commit and `ResourcePurityToString()` in
  `AIModFunctionLibrary.cpp`.
- `occupied` — whether an extractor currently occupies the node
  (`IFGExtractableResourceInterface::IsOccupied()`). Does **not** identify
  *which* extractor — see resource-node-research.md §5; that requires a
  separate reverse lookup this phase does not implement.

## Source of truth

The actual serialization code is
`UAIModFunctionLibrary::LogResourceNodesAsJson` /
`LogBuildablesAsJson` / `LogManufacturersAsJson` in
`Mods/GameFeatures/AIMod/Source/AIMod/Private/AIModFunctionLibrary.cpp`
— built manually with `FJsonObject`/`FJsonSerializer` (Unreal's own Json
module, already a `AIMod.Build.cs` dependency) rather than reflection-based
struct-to-json conversion, so the wire field names/casing here are exactly
what's documented and don't silently change if the C++ struct's property
names change.

## Test fixture status

`tests/fixtures/resource_nodes.json` matches this shape but is
**hand-written synthetic data**, not captured from a real game session —
no runtime verification has happened yet (see
[manual-verification.md](manual-verification.md)). Replace it with real
captured output from `LogResourceNodesAsJson` once that's been run in a
loaded Satisfactory save, then re-run the Python schema test.
