# Telemetry protocol v1 (resource nodes)

Defined by PLAN.md Phases 5-6 / Task 8-9. This is the concrete JSON shape
`UDocModFunctionLibrary::LogResourceNodesAsJson` produces. No network
transport sends this anywhere yet (Phase 9) — it is currently only logged
locally via `LogDocModAI` for debugging and for building test fixtures.

## Shape

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

## Source of truth

The actual serialization code is
`UDocModFunctionLibrary::LogResourceNodesAsJson` in
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
