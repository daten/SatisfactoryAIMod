# Telemetry protocol v1

Defined by PLAN.md Phases 5-6 / Task 8-9, expanded in Phase 10. These are
the concrete JSON shapes `UDocModFunctionLibrary`'s `Log*AsJson` functions
produce, and what PLAN.md Phase 9's `/rpc` endpoint
(`UDocModHttpServerSubsystem`) returns in the `result` field for each
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
  `FDocModResourceNodeTelemetry`'s header comment
  (`Mods/GameFeatures/DocMod/Source/DocMod/Public/DocModTelemetryTypes.h`).
  PLAN.md Phase 7 will need to replace this with a real stable identifier
  design before this telemetry is used for anything beyond debug logging.
- `resource` — human-readable name from `UFGItemDescriptor::GetItemName()`,
  e.g. `"Iron Ore"`.
- `resourceClass` — the resource descriptor's full object path
  (`UClass::GetPathName()`), for callers that want the actual asset
  reference rather than the display string.
- `purity` — one of `"Impure"`, `"Normal"`, `"Pure"` (FactoryGame's
  `EResourcePurity` display text via `GetResourcePurityText()`). Note this
  is the *display string*, not the raw enum name (`RP_Pure` etc.) — chosen
  because it's what `GetResourcePurityText()` actually returns and avoids
  inventing a second naming scheme; revisit if a controller would rather
  parse the raw enum token.
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

Generic fields for **any** placed `AFGBuildable` — foundations, conveyors,
power poles, machines, everything. `id` has the same session-local-only
caveat as resource nodes (see below). `buildableClass` is the buildable's
`UClass::GetPathName()`. Source: `docs/buildable-research.md` §1.

**Enumeration caveat:** tries `AFGBuildableSubsystem::GetAllBuildablesRef()`
first (a real public getter, unlike the resource node manager), falling
back to a `TActorIterator<AFGBuildable>` scan if the subsystem is
unavailable. Whether the subsystem is actually populated at runtime is
unverified from source (its `.cpp` is a stub) — see
[manual-verification.md](manual-verification.md).

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

## Common field notes

- `protocolVersion` — currently always `1`. Bump when a shape changes,
  per CLAUDE.md's protocol versioning requirement.
- `id` (resourceNodes/buildables/manufacturers) — **not save-stable.**
  Currently `AActor::GetPathName()`. Unique within a running session/map
  load, but not guaranteed to survive save/load, actor respawn, or map
  transition. See [resource-node-research.md](resource-node-research.md)
  §4 / [buildable-research.md](buildable-research.md) §1 and
  `DocModTelemetryTypes.h`'s struct comments. PLAN.md Phase 7 must design
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
- `purity` — one of `"Impure"`, `"Normal"`, `"Pure"` (FactoryGame's
  `EResourcePurity` display text via `GetResourcePurityText()`). Note this
  is the *display string*, not the raw enum name (`RP_Pure` etc.) — chosen
  because it's what `GetResourcePurityText()` actually returns and avoids
  inventing a second naming scheme; revisit if a controller would rather
  parse the raw enum token.
- `occupied` — whether an extractor currently occupies the node
  (`IFGExtractableResourceInterface::IsOccupied()`). Does **not** identify
  *which* extractor — see resource-node-research.md §5; that requires a
  separate reverse lookup this phase does not implement.

## Source of truth

The actual serialization code is
`UDocModFunctionLibrary::LogResourceNodesAsJson` /
`LogBuildablesAsJson` / `LogManufacturersAsJson` in
`Mods/GameFeatures/DocMod/Source/DocMod/Private/DocModFunctionLibrary.cpp`
— built manually with `FJsonObject`/`FJsonSerializer` (Unreal's own Json
module, already a `DocMod.Build.cs` dependency) rather than reflection-based
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
