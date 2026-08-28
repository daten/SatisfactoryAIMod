# Write operations protocol v1 (PLAN.md Phase 12)

The first two controlled write operations. Both mutate an existing
`AFGBuildableManufacturer` — no construction, no actor spawning (that's
Phase 13, deliberately not started yet — see the note at the bottom of
this file). Per PLAN.md's "First Write Operations" guidance, these are
exactly the two operations it names as the safe on-ramp: `SetRecipe` and
`SetClockRate`.

**This is the first genuinely game-mutating capability in AIMod.** Test
it against a save you don't mind corrupting, not your main save, until
it's been exercised enough to trust — see
[manual-verification.md](manual-verification.md).

## `world.setClockSpeed`

Request:

```json
{
  "protocolVersion": 1,
  "requestId": "10",
  "method": "world.setClockSpeed",
  "params": {
    "buildableId": "/Game/Maps/.../Build_ConstructorMk1_C_1",
    "clockSpeedPercent": 150.0
  }
}
```

`buildableId` is the session-local id from `world.manufacturers` (see
[telemetry-protocol.md](telemetry-protocol.md) — **not** save-stable, so a
save/reload invalidates it; re-fetch `world.manufacturers` first).
`clockSpeedPercent` is a percentage (100 = normal speed, matching the
in-game overclock slider).

**Validation, in order** (`UAIModFunctionLibrary::SetManufacturerClockSpeed`,
`Mods/GameFeatures/AIMod/Source/AIMod/Private/AIModFunctionLibrary.cpp`):

1. `buildableId` resolves to an actual `AFGBuildableManufacturer` — else
   `TARGET_NOT_FOUND` (404).
2. The building allows changing potential at all
   (`GetCanChangePotential()`) — else `OPERATION_NOT_PERMITTED` (403).
3. `clockSpeedPercent / 100` is within
   `[GetCurrentMinPotential(), GetCurrentMaxPotential()]` (the max
   accounts for installed power shards) — else `INVALID_CLOCK_SPEED`
   (400), with the actual valid range in the error message.
4. Calls `SetPendingPotential(...)`.

**Important:** per `AFGBuildableFactory::SetPendingPotential`'s own doc
comment, this takes effect at the *next production cycle*, not
instantly. A `world.manufacturers` call immediately afterward may still
show the old `clockSpeedPercent` — that is expected, not a bug. Re-check
after the machine finishes its current cycle.

Success response: `{"protocolVersion":1,"requestId":"10","success":true,"result":{}}`.

## `world.setRecipe`

Request:

```json
{
  "protocolVersion": 1,
  "requestId": "11",
  "method": "world.setRecipe",
  "params": {
    "buildableId": "/Game/Maps/.../Build_ConstructorMk1_C_1",
    "recipeClass": "/Game/FactoryGame/Recipes/Recipe_IronPlate.Recipe_IronPlate_C"
  }
}
```

`recipeClass` is the full object path of a `UFGRecipe` subclass (the same
format `resourceClass`/`buildableClass` already use in read telemetry —
round-trippable).

**Validation, in order:**

1. `buildableId` resolves to an actual `AFGBuildableManufacturer` — else
   `TARGET_NOT_FOUND` (404).
2. `recipeClass` resolves (`LoadObject<UClass>`) to a real class that
   `IsChildOf(UFGRecipe::StaticClass())` — else `INVALID_RECIPE` (400).
   **This is deliberately narrow**, not a generic "load any class by
   path" capability: anything that isn't a real `UFGRecipe` subclass is
   rejected before it's used for anything, per CLAUDE.md's Safety and
   Stability Boundary (no `SpawnAnyUClass`-style operations).
3. `UFGRecipe::IsProducedIn(recipeClass, manufacturer->GetClass())` — the
   recipe must actually be producible in *this* building class — else
   `RECIPE_NOT_COMPATIBLE` (400).
4. Both input and output inventories must be empty
   (`UFGInventoryComponent::IsEmpty()`) — else `INVENTORY_NOT_EMPTY`
   (400). This isn't invented caution: `AFGBuildableManufacturer::SetRecipe`'s
   own doc comment says *"It is up to the caller to make sure input and
   output inventories are empty before changing recipe"* — so this is
   enforced rather than trusted to the caller or to undefined engine
   behavior.
5. Calls `SetRecipe(...)`.

Success response: `{"protocolVersion":1,"requestId":"11","success":true,"result":{}}`.

## Error response shape (both methods)

```json
{
  "protocolVersion": 1,
  "requestId": "10",
  "success": false,
  "error": { "code": "INVALID_CLOCK_SPEED", "message": "clockSpeedPercent 500.0 is outside the valid range [50.0, 250.0]" }
}
```

Error codes used: `TARGET_NOT_FOUND`, `OPERATION_NOT_PERMITTED`,
`INVALID_CLOCK_SPEED`, `INVALID_RECIPE`, `RECIPE_NOT_COMPATIBLE`,
`INVENTORY_NOT_EMPTY`, plus the generic ones both write and read methods
share (`INVALID_REQUEST`, `UNSUPPORTED_PROTOCOL_VERSION`,
`UNKNOWN_METHOD`, `PAYLOAD_TOO_LARGE`, `INTERNAL_ERROR` — see
[networking-research.md](networking-research.md)).

## What comes after this, and why it hasn't started

PLAN.md Phase 13 (Building Placement) is next in sequence, but is a much
larger jump in both scope and risk: spawning new actors into the live
game world, requiring construction metadata, ownership, replication, save
registration, connection components, hologram/build validation — PLAN.md
itself says to "research the existing construction code before
implementing placement," not to jump straight from telemetry to actor
spawning. Nothing built so far (Phases 2-12) has been runtime-verified in
an actual Satisfactory session yet either (see
[manual-verification.md](manual-verification.md)) — stacking building
placement on top of entirely unverified mutation code compounds risk in
exactly the way PLAN.md's "Read Before Write" section warns against. Research
for Phase 13 (read-only, no code) is a reasonable next step; implementing
actual placement is not, until at least some of this has been confirmed
working against a real game session.
