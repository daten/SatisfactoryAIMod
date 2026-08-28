# Lightweight buildables — discovered 2026-08-25

## The finding

Placing a foundation via `ConstructBuildingAtPosition`/`world.placeBuilding`
(recipe `Recipe_Foundation_8x4_01`) returned `success:true` with an empty
`result.buildableId`. `world.buildables` showed exactly 10121 real
`AFGBuildable` actors before and after the call — no new entry. The user
confirmed the foundation was genuinely visible in-game at the expected
spot. So construction really worked; the mod's existing telemetry and
mutation APIs simply couldn't see the result.

Root cause: Satisfactory stores foundations — and, going by the class's
name and its "mass-placed pieces" framing, likely walls and other
high-count buildables too, though only foundations have been confirmed
live so far — via a separate performance system,
`AFGLightweightBuildableSubsystem`, instead of as full `AFGBuildable`
actors. A base can have thousands of foundation/wall pieces; keeping
each as a complete actor+component tree would be expensive, so they're
kept as plain instance data (`FRuntimeBuildableInstanceData`: a
`FTransform`, the recipe that built it, customization data, etc.) and
only "materialized" into a real, temporary `AFGBuildable*` on demand
(e.g. when a player aims at one to dismantle it).

Every AIMod telemetry/mutation function up to this point
(`world.buildables`, `world.connections`, `DismantleBuildable`, the
proximity-based buildable-id lookup after construction) was built
around `AFGBuildableSubsystem::GetAllBuildablesRef()`/
`AFGBuildable::GetPathName()` — none of which lightweight buildables
participate in.

## The fix

### Identity

Lightweight buildables have no `GetPathName()` (there's no actor).
Identity is instead `(BuildableClass, index into
GetAllLightweightBuildableInstances()[BuildableClass])`. AIMod encodes
this as a string id:

```text
lightweight:<BuildableClassPath>|<Index>
```

e.g. `lightweight:/Game/FactoryGame/Buildable/Factory/Foundation_8x4_01/Build_Foundation_8x4_01.Build_Foundation_8x4_01_C|42`

`|` rather than a second `:` as the separator — Unreal object paths can
themselves contain `:` (e.g. a level's `Persistent_Level:PersistentLevel`
nesting), but never `|`. Regular buildable ids (`GetPathName()`) always
start with `/Game/...`, never `lightweight:`, so the two id shapes never
collide. Helpers: `MakeLightweightBuildableId`,
`IsLightweightBuildableId`, `ParseLightweightBuildableId` (all in the
anonymous namespace at the top of `AIModFunctionLibrary.cpp`).

### Index stability — CORRECTED 2026-08-25, indices are NOT stable

Originally believed (from reading `FRuntimeBuildableInstanceData::IsValid()`/
`Clear()`'s real bodies) that removal tombstones a slot rather than
compacting the array, keeping `(class, index)` identity stable for
every other instance of that class. **Live evidence contradicts this**:
five foundations placed in one session (indices 771-775) were later
found, after several unrelated `world.deleteBuilding` calls against
*different* lightweight instances of the same class, to have been
renumbered to 754-758 — every single one shifted by exactly -17, with
their positions unchanged. This means something (a periodic compaction
pass, possibly `RemoveStaleTemporaryBuildables()` - "Called end of
tick. Destroys and buildables and deletes the instance to temp data
for them" - or an equivalent) does eventually reindex the array,
contradicting the tombstone theory. `IsValid()`/`Clear()`'s real bodies
still explain the *low-level* mechanics of one removal correctly; what
was wrong was assuming that's the *only* thing that happens to the
array over time.

**Practical consequence:** a lightweight buildable id captured at one
point in time is not safe to reuse indefinitely, especially after any
`world.deleteBuilding` call (on that class or possibly others) or
after a game session reload - re-resolve by position via
`world.buildables` before trusting an old id again, don't just retry
the same id. This is a stronger version of the pre-existing
"session-local only, not save-stable" caveat that already applies to
every id in this protocol.

### Telemetry

`world.buildables` (`CollectBuildableTelemetry`) now appends rows from
`AFGLightweightBuildableSubsystem::GetAllLightweightBuildableInstances()`
(a public getter — no need to reach into private state) alongside the
regular actor scan. Position/rotation come straight from each instance's
`FTransform`.

### Construction result id

`ConstructBuildingAtPosition`'s post-construction proximity search
(originally: find the nearest real `AFGBuildable` within 200 units of
where the hologram was) now falls back to searching lightweight
instances of the recipe's buildable class by the same 200-unit
tolerance when the regular-actor search finds nothing.
`ResolveBuildableClassForRecipe` gets there via the recipe's first
product (`UFGRecipe::GetProducts`), which for a building recipe is
expected to be a `UFGBuildingDescriptor`, then
`UFGBuildingDescriptor::GetBuildableClass()`.

### Dismantle

`DismantleBuildable`/`world.deleteBuilding` branches on the id prefix.
For a lightweight id, it resolves the buildable class, looks up the
`FRuntimeBuildableInstanceData*` via
`AFGLightweightBuildableSubsystem::GetRuntimeDataForBuildableClassAndIndex()`,
then calls `FindOrSpawnBuildableForRuntimeData()` to materialize a real,
temporary `AFGBuildable*` — and reuses the *exact same*
`IFGDismantleInterface::Execute_Dismantle()` call used for regular
buildables. This works because `AFGBuildable::Dismantle_Implementation()`
(real source, `FGBuildable.cpp`) already has a dedicated
`mIsLightweightTemporary` branch that calls
`AFGLightweightBuildableSubsystem::RemoveByInstanceIndex()` for us — no
need to duplicate that logic in the mod.

## Still unconfirmed

- Whether buildables *other* than foundations (walls, pipes, etc.) also
  use this system — plausible from the class's framing and naming, not
  yet tested live.
- `RemoveByInstanceIndex()`/`InvalidateRuntimeInstanceDataForIndex()`
  are stub bodies in this installed SDK (no source) — their exact
  replication/timing behavior is inferred from `IsValid()`/`Clear()`'s
  real bodies and from `Dismantle_Implementation()`'s real call site,
  not read directly.
- Whether `FindOrSpawnBuildableForRuntimeData()`'s materialized
  "temporary" actor has any other behavioral differences from a normal
  actor beyond what `Dismantle_Implementation()` already accounts for.
