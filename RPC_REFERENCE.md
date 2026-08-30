# AIMod RPC Reference

For an agent that only has access to a **running, deployed AIMod instance** —
no repo checkout, no C++ source. Everything here is derived from the actual
dispatch code (`AIModHttpServerSubsystem.cpp`), not from memory, and should
be complete as of this file's last update. If a call behaves differently than
documented here, trust the live response over this file — the mod may have
moved on since this was written.

If you *do* have the repo, `PLAYBOOK.md` covers how to phrase a request in
plain language; this file covers how the interface actually works once a
request has been decided.

---

## Connecting

- **Endpoint**: `POST http://127.0.0.1:51902/rpc` — loopback only by default,
  fixed port (not configurable, no discovery endpoint).
- **Remote access**: rejected by default. The player can enable it from
  AIMod's entry in the game's mod settings menu ("Allow Remote Connections")
  — if you're not on the same machine and get a `FORBIDDEN` response, that's
  why.
- **Content-Type**: `application/json`. Body is a single JSON object, one
  request per HTTP call — no batching.
- **Size limit**: requests over a fixed byte limit are rejected with
  `PAYLOAD_TOO_LARGE` before being parsed.

## Request shape

```json
{
  "protocolVersion": 1,
  "requestId": "any string you choose",
  "method": "world.someMethod",
  "params": { }
}
```

- `protocolVersion` must be exactly `1` (an integer, not a string) or the
  request is rejected with `UNSUPPORTED_PROTOCOL_VERSION`.
- `requestId` is yours to set and is echoed back verbatim — use it to match
  responses to requests if you're firing multiple calls.
- `method` is required. An unrecognized method returns `UNKNOWN_METHOD`.
- `params` is required for any method that takes arguments (all of the write
  methods, and a few of the read ones), omitted or empty for the rest — see
  each method below.

## Response shape

Success:
```json
{ "protocolVersion": 1, "requestId": "...", "success": true, "result": { } }
```

Failure:
```json
{ "protocolVersion": 1, "requestId": "...", "success": false, "error": { "code": "SOME_CODE", "message": "human-readable detail" } }
```

**`success: true` is not proof the thing you asked for actually happened
the way you expect** — several methods below are asynchronous (they poll
real game ticks before resolving) and even a synchronous success can reflect
state that changed again a moment later. For anything you're about to build
on top of, re-read the relevant world state afterward rather than trusting
the response alone. This isn't paranoia for its own sake — it's caught real
bugs during this mod's own development (a pipe construction call once
reported success while silently connecting to the wrong target entirely).

### Common error codes

`INVALID_REQUEST` (malformed/missing params), `UNSUPPORTED_PROTOCOL_VERSION`,
`UNKNOWN_METHOD`, `PAYLOAD_TOO_LARGE`, `INTERNAL_ERROR`, `FORBIDDEN`
(non-loopback request, remote access not enabled), `TARGET_NOT_FOUND`
(an id in `params` didn't resolve to anything real), `NO_PLAYER` (no local
player character found — shouldn't happen in a normal session).
Method-specific codes are listed per method below.

### IDs

Every id this protocol hands you (`buildableId`, `nodeId`, `portableMinerId`,
etc.) is **session-local**, generally an `AActor::GetPathName()` string. It
is *not* guaranteed to survive a save/reload, and is otherwise opaque — never
try to parse or construct one yourself. Re-fetch the relevant list (e.g.
`world.buildables`) after any save/reload before reusing an id from before it.

---

## Read-only methods

All take `GET`-style semantics over `POST /rpc` (no side effects), most take
no `params` at all — noted where one does.

### `world.player` — no params
```json
{ "protocolVersion": 1, "position": {"x":0,"y":0,"z":0}, "rotation": {"pitch":0,"yaw":0,"roll":0} }
```
Local player character's position/rotation.

### `world.timeOfDay` — no params
```json
{ "protocolVersion": 1, "hour": 10, "minute": 30, "daySeconds": 37800.0, "isDay": true }
```

### `world.resourceNodes` — no params
```json
{
  "protocolVersion": 1,
  "resourceNodes": [
    {
      "id": "...", "resource": "Iron Ore",
      "resourceClass": "/Game/FactoryGame/Resource/RawResources/OreIron/Desc_OreIron.Desc_OreIron_C",
      "purity": "Pure", "position": {"x":0,"y":0,"z":0}, "occupied": false,
      "nodeType": "Node", "coreId": "", "satelliteState": ""
    }
  ]
}
```
- `purity` is `"Impure"`/`"Normal"`/`"Pure"`.
- `nodeType` is `"Node"` (a normal resource node), `"FrackingCore"` (a
  Resource Well Pressurizer's target — `purity` reads `"N/A"` on these),
  `"FrackingSatellite"` (an individual well head), `"Geyser"`, or
  `"Deposit"`.
- `coreId` — for a `FrackingSatellite`, the id of its owning `FrackingCore`.
  Empty otherwise.
- `satelliteState` — for a `FrackingSatellite`, one of `"Untouched"`/
  `"Active"`/etc. A satellite must reach `"Active"` (by powering its core's
  Pressurizer) before an extractor can be built on it — see
  `world.placeExtractor` below.

### `world.buildables` — no params
```json
{
  "protocolVersion": 1,
  "buildables": [
    { "id": "...", "buildableClass": "...", "position": {"x":0,"y":0,"z":0}, "rotation": {"pitch":0,"yaw":0,"roll":0} }
  ]
}
```
Every placed buildable — machines, belts, foundations, poles, everything.
Some ids (foundations and other mass-placed pieces) look like
`"lightweight:<ClassPath>|<Index>"` instead of a path — treat as opaque
either way, both work with `world.deleteBuilding`.

**Does not include vehicles** — `AFGVehicle` (drones, tractors, trucks,
explorers, trains, etc.) is not an `AFGBuildable`. Use `world.vehicles`
for those.

### `world.vehicles` — no params
```json
{
  "protocolVersion": 1,
  "vehicles": [
    { "id": "...", "buildableClass": "...", "position": {"x":0,"y":0,"z":0}, "rotation": {"pitch":0,"yaw":0,"roll":0} }
  ]
}
```
Same shape as `world.buildables` but iterates `AFGVehicle` actors instead
(drones, wheeled vehicles, trains). `id` is a stable path name usable with
`world.deleteBuilding`. Added alongside the `world.constructVehicle`
dismantle fix — before this, vehicles built via `world.constructVehicle`
were invisible to telemetry and undeletable.

### `world.manufacturers` — no params
```json
{
  "protocolVersion": 1,
  "manufacturers": [
    {
      "id": "...", "buildableClass": "...", "position": {"x":0,"y":0,"z":0},
      "recipe": "Iron Plate", "clockSpeedPercent": 100.0,
      "productionStatus": "Producing", "productionProgress": 0.42, "productivity": 1.0,
      "inputInventory": [ {"itemClass":"...","itemName":"Iron Ore","count":45} ],
      "outputInventory": [ {"itemClass":"...","itemName":"Iron Plate","count":12} ]
    }
  ]
}
```
`productionStatus` is `"None"`/`"Producing"`/`"ProducingWithCrystal"`/
`"Standby"`/`"Error"`. Empty inventory stacks are omitted.

### `world.targetedManufacturer` — no params
```json
{ "protocolVersion": 1, "manufacturer": null }
```
Same object shape as one `world.manufacturers` entry when the local player
is looking at one (via the game's own "what can I press E on" state);
`null` otherwise.

### `world.connections` — no params
```json
{
  "protocolVersion": 1,
  "connections": [
    { "ownerBuildableId": "...", "direction": "Output", "connected": true, "connectedBuildableId": "...", "position": {"x":0,"y":0,"z":0}, "normal": {"x":0,"y":0,"z":0} }
  ]
}
```
One row per belt/lift/splitter/merger/machine connector
(`UFGFactoryConnectionComponent`). `direction` is `"Input"`/`"Output"`/
`"Any"`/`"SnapOnly"`. A real link between two buildings produces two rows
(an `"Output"` row on the source, an `"Input"` row on the destination).
For a straight, no-bend connection, the destination connector's `normal`
must be the exact negation of the source's.

### `world.pipeConnections` — no params
Same shape as `world.connections`, for fluid pipes AND Hypertube tubes
(`UFGPipeConnectionComponentBase`):
```json
{
  "protocolVersion": 1,
  "connections": [
    { "ownerBuildableId": "...", "connectionType": "Any", "isHypertube": false, "connected": false, "connectedBuildableId": "", "position": {"x":0,"y":0,"z":0}, "normal": {"x":1,"y":0,"z":0} }
  ]
}
```
`connectionType` is `"Any"`/`"Producer"`/`"Consumer"`/`"SnapOnly"`. Storage
Tanks and Pipeline Junctions only ever report `"Any"` on every connector —
`world.connectPipe` (below) already handles this. Hypertube connectors also
stay at `"Any"`. Same opposite-normal docking rule as `world.connections`.

### `world.conveyorBeltTiers` — no params
```json
{ "protocolVersion": 1, "tiers": [ { "recipeClass": "...Recipe_ConveyorBeltMk1_C", "buildableClass": "...", "speed": 0.0, "maxSplineLength": 5600.1, "bendRadius": 0.0, "maxInclineDegrees": 0.0 } ] }
```
One row per `Recipe_ConveyorBeltMk1`..`Mk6`. `speed`'s exact unit is
unconfirmed — treat as relative/comparable across tiers only.

### `world.conveyorLiftTiers` — no params
Same shape as `conveyorBeltTiers`, for `Recipe_ConveyorLiftMk1`..`Mk6`.

### `world.powerLineLimits` — no params
```json
{ "protocolVersion": 1, "recipeClass": "...Recipe_PowerLine_C", "buildableClass": "...", "maxLength": 10000.0, "maxPowerTowerLength": 0.0, "lengthPerCost": 0.0 }
```
Flat object (only one power line tier exists), all lengths in cm.

### `world.pipelineTiers` — no params
```json
{ "protocolVersion": 1, "tiers": [ { "recipeClass": "...Recipe_Pipeline_C", "buildableClass": "...", "flowLimit": 0.0, "maxSplineLength": 5600.1, "bendRadius": 0.0, "minBendRadius": 0.0 } ] }
```
One row per `Recipe_Pipeline`/`Recipe_PipelineMK2` (capital `MK2`).
`flowLimit` is in m³/s.

### `world.conveyorAttachments` — no params
```json
{ "protocolVersion": 1, "attachments": [ { "recipeClass": "...Recipe_ConveyorAttachmentSplitter_C", "buildableClass": "...", "inputCount": 1, "outputCount": 3, "supportsSortRules": false } ] }
```
One row per real splitter/merger recipe. `supportsSortRules` is `true` only
for Smart/Programmable Splitter — there is no RPC method yet to actually set
sort rules on one.

### `world.recipeCatalog` — no params
```json
{
  "protocolVersion": 1,
  "recipes": [
    {
      "recipeClass": "...", "displayName": "Iron Plate", "isBuildingRecipe": false,
      "manufacturingDuration": 6.0,
      "ingredients": [ {"itemClass":"...","itemName":"Iron Ingot","amount":3} ],
      "products": [ {"itemClass":"...","itemName":"Iron Plate","amount":2} ],
      "producedIn": ["...Build_ConstructorMk1_C"],
      "variablePowerConsumptionConstant": 0.0, "variablePowerConsumptionFactor": 0.0
    }
  ]
}
```
**Every recipe in the game, including ones not yet unlocked in the current
save** — check the player's actual unlocks separately before assuming a
recipe/alternate is usable. Building recipes are included too
(`isBuildingRecipe: true`); their `ingredients` is the building's OWN base
construction cost — **not necessarily the real total**. `amount` for
liquid/gas ingredients is in thousandths of a cubic meter — check the
item's `form` via `world.itemCatalog` first. `Recipe_*` paths under
`/Recipes/Converter/` are the Resource Converter building's raw-resource-
conversion recipes (e.g. Iron Ore from Limestone) — exclude these when
computing a normal extraction-based chain unless the request specifically
asked for converters. `Alternate` in the path marks an alternate recipe.

**For any `isBuildingRecipe: true` entry, `ingredients` is only part of the
real cost** — a building's currently-active customization (swatch/pattern/
material, e.g. a "Concrete" or "FICSIT" wall pattern) charges its own
separate ingredients on top, confirmed live (a wall's base recipe costs 2
Concrete; the same wall, built normally, also needed 2 Iron Plate for
whatever pattern was active). Use `world.constructionCost` (below) for the
real total before relying on this field for an affordability check.

### `world.itemCatalog` — no params
```json
{ "protocolVersion": 1, "items": [ { "itemClass": "...", "name": "Iron Plate", "form": "Solid", "isBuildingDescriptor": false, "stackSize": 100, "energyValue": 0.0, "radioactiveDecay": 0.0 } ] }
```
`form` is `"Solid"`/`"Liquid"`/`"Gas"`/`"Invalid"`.

### `world.buildableCatalog` — no params
```json
{
  "protocolVersion": 1,
  "buildables": [
    {
      "recipeClass": "...", "buildableClass": "...", "category": "Manufacturer",
      "constructionCost": [ {"itemClass":"...","itemName":"Reinforced Iron Plate","amount":2} ],
      "clearance": [ { "min": {"x":0,"y":0,"z":0}, "max": {"x":0,"y":0,"z":0}, "size": {"x":0,"y":0,"z":0}, "type": "Default" } ],
      "runsOnPower": true, "idlePowerConsumption": 0.5,
      "producingPowerConsumptionBase": 4.0, "defaultProducingPowerConsumption": 4.0,
      "minPotential": 0.01, "maxPotential": 1.0, "canChangePotential": true,
      "overridesShardSlotCount": false,
      "factoryInputCount": 1, "factoryOutputCount": 1,
      "pipeInputCount": 0, "pipeOutputCount": 0, "powerConnectionCount": 1
    }
  ]
}
```
`category` is `"Generator"`/`"Extractor"`/`"Manufacturer"`/`"Other"`.
Power/potential fields are present only for Manufacturer/Extractor/
Generator; `powerProductionCapacity`/`defaultPowerProductionCapacity` only
for Generator. `maxPotential` is the un-overclocked baseline (does not
account for installed Power Shards). `clearance` is present on every entry.
Like `world.recipeCatalog`'s `ingredients`, `constructionCost` is the base
recipe only — see `world.constructionCost` below for the real total
including any active customization cost.

### `world.constructionCost` — params: `{"recipeClass"}`
```json
{
  "protocolVersion": 1,
  "recipeClass": "...",
  "baseIngredients": [ {"itemClass":"...","itemName":"Concrete","amount":2} ],
  "appliedCustomizationRecipes": ["...Recipe_Material_Wall_Concrete_C"],
  "totalIngredients": [ {"itemClass":"...","itemName":"Concrete","amount":4} ]
}
```
The real total construction cost for a recipe RIGHT NOW, including whatever
customization (swatch/pattern/material) is currently active for that
building's category — confirmed this is a real, separate cost layered on
top of the base recipe, not a `world.recipeCatalog` reporting bug: a wall's
base recipe lists 2 Concrete only, but a real build (via hotbar or the
build menu) also charged Iron Plate or extra Concrete depending on which
pattern was active. `baseIngredients` matches `world.recipeCatalog`'s
`ingredients` for this recipe; `totalIngredients` is `baseIngredients` plus
every ingredient from every currently-applied customization recipe
(`appliedCustomizationRecipes`, empty if none), merged by item class.
Applies generically to any building category with a customization system —
walls, foundations, roofs, pipes, etc. — since it works by spawning a real
hologram for the recipe (the same way construction does) and reading
whatever customization the game itself automatically applies, rather than
a hardcoded per-category rule. Never places anything or touches inventory.
Errors: `INVALID_RECIPE` (`recipeClass` did not resolve to a real
`UFGRecipe`). For an extractor recipe, `appliedCustomizationRecipes` is
always empty and `totalIngredients` equals `baseIngredients` — extractor
holograms are never spawned here, since a real, confirmed engine crash
lives inside their construction path (see `world.placeBuilding`'s
extractor-recipe refusal above) and it isn't worth the risk for a read-only
query on a category that realistically has no meaningful swatch cost
anyway.

### `world.chatHistory` — no params
```json
{ "protocolVersion": 1, "messages": [ { "sender": "PlayerName", "text": "hello", "type": "PlayerMessage", "timestamp": 0, "isLocalPlayerMessage": true } ] }
```
`type` is `"PlayerMessage"`/`"SystemMessage"`/`"AdaMessage"`/
`"CustomMessage"` (the last is what `world.sendChatMessage` posts as).
Messages the player types starting with `/` never appear here — those are
diverted to chat-command dispatch. No "since last call" filtering — track
your own high-water mark between polls.

**A real player-typed message gets an instant "received, thinking..." reply
from the mod itself**, independent of any external polling — the mod binds
directly to the game's own chat-added event, so this fires the same tick
the message is sent, not after your next poll interval. This is a fixed,
canned acknowledgment only (no reasoning happens in the mod) — you still
need to poll `world.chatHistory` yourself to actually see and respond to
what was said. Controlled by the player's "Auto-Acknowledge Chat Messages"
mod setting (default on) — if you're relying on it, don't assume it's
there; check for the ack, but don't treat its absence as a sign nothing was
received.

### `world.portableMiners` — no params
```json
{
  "protocolVersion": 1,
  "portableMiners": [
    {
      "id": "...", "position": {"x":0,"y":0,"z":0}, "nodeId": "...",
      "isProducing": true, "extractionProgress": 0.5,
      "outputInventory": [ {"itemClass":"...","numItems":10} ]
    }
  ]
}
```
Portable Miners must be emptied via `world.retrievePortableMinerInventory`
below — they have no belt output.

### `world.groundHeight` — params: `{"x", "y"}`, optional `"z"`
```json
{ "protocolVersion": 1, "found": true, "x": 0.0, "y": 0.0, "z": 123.4, "normal": {"x":0,"y":0,"z":1} }
```
Runs the exact same ground trace `world.placeBuilding` uses internally —
query this first, then pass the returned `z` straight back in as
`world.placeBuilding`'s `z` param for a guaranteed match. `z` (request
param, optional) anchors the ±1000-unit search center; omit to default to
the player's current Z. `found: false` means nothing was hit in that range
— `z` in the response is then just the literal search center, matching what
a real placement there would fall back to.

### `world.terrainHeightGrid` — params: `{"minX", "minY", "maxX", "maxY", "stepSize"}`, optional `"z"`
```json
{
  "protocolVersion": 1, "minX": 0.0, "minY": 0.0, "stepSize": 100.0,
  "countX": 5, "countY": 5,
  "heights": [123.4, 125.0, ...],
  "found": [true, true, ...]
}
```
Batched `world.groundHeight`: the exact same real trace, run for an
entire rectangular grid in **one call** instead of one HTTP round-trip
per point. Use this instead of looping `world.groundHeight` from your own
code — surveying hundreds of points individually is dominated by
per-call HTTP/JSON overhead, not the trace itself; batching moves the
loop server-side where only the trace cost remains.

Grid runs `minX..maxX` and `minY..maxY` inclusive, spaced `stepSize`
apart (`countX = floor((maxX-minX)/stepSize)+1`, same for Y). `z` is the
same ±1000-unit search-center anchor as `world.groundHeight`, applied to
**every** point — pick an anchor that plausibly covers the whole area's
real height range (issue separate calls with different anchors for an
area spanning a wide Z range, e.g. a cliff).

**Response is two parallel flat arrays, not per-point objects** —
deliberately compact for a bulk survey. Row-major: `index = row*countX +
col`, `x = minX + col*stepSize`, `y = minY + row*stepSize`. No surface
normal is included (unlike `world.groundHeight`) — query a specific point
directly if you need its normal; this is for height/gap survey, not full
characterization.

**Capped at 10000 points** (~a 100×100 grid) — this runs synchronously on
the game thread like every trace in this mod, so an unbounded grid risks
a real frame hitch. A request over the cap returns
`{"tooManyPoints":true,"requestedPoints":N,"maxPoints":10000,...}` with
empty arrays — tile a larger area into multiple calls, or use a coarser
`stepSize`, rather than expecting the cap to be raised.

**Cache the result yourself** — this function has no caching of its own,
it's a fast primitive. Satisfactory's map is static within a given game
version, so a survey of an area you care about (a build site, a platform
footprint) is valid to save to a local file and reuse across sessions
rather than re-querying every time.

---

## Write methods

All take `POST /rpc` with a `"params"` object (except
`world.cleanupOrphanedFlowIndicators`, which takes none). Most return the
standard `{"success":true,"result":{}}` / error shape; a few also include a
`buildableId` inside `result` on success (noted below). **Construction
methods are asynchronous** — real game ticks pass before the response
returns (a few hundred ms to a couple of seconds), because they drive the
game's own hologram/build-gun placement flow, not a synchronous spawn. The
HTTP call blocks until it resolves either way; there's no separate polling
step needed on your end.

### `world.setClockSpeed`
`params: {"buildableId", "clockSpeedPercent"}` (percent, 100 = normal).
Errors: `TARGET_NOT_FOUND`, `OPERATION_NOT_PERMITTED` (building doesn't
allow potential changes), `INVALID_CLOCK_SPEED` (message states the real
valid range, which accounts for installed Power Shards). Takes effect at
the *next* production cycle, not instantly — re-check `world.manufacturers`
after the machine's current cycle finishes, not immediately.

**Works on extractors too, not just manufacturers** (fixed 2026-08-30 —
previously silently `TARGET_NOT_FOUND` on a Miner even though a Miner
supports clock speed identically; both derive from the same
`AFGBuildableFactory` base). Overclocking a Miner uses this same method.

### `world.installPowerShard`
`params: {"buildableId", "count"}`. Inserts real Power Shard items from
the player's carried inventory into a building's overclock slot
inventory — the write half of the overclocking picture `world.setClockSpeed`
only reads the ceiling of. Works on any manufacturer *or* extractor (a
Miner included). Removes `count` shards from the player only after
verifying they're carried (`INSUFFICIENT_INGREDIENTS` otherwise); if
fewer than `count` actually fit (no free slots left), the unplaced
excess is returned to the player rather than lost. `OPERATION_NOT_PERMITTED`
if the target doesn't support clock changes at all, or if zero shards
could fit. `result.detail` reports `shardsAdded` (the real count that
fit) and `newMaxPotentialPercent` (the new overclock ceiling after
insertion — re-check this, or `world.setClockSpeed`'s own valid-range
error text, rather than assuming a fixed percent per shard).

**Not yet live-tested.** The real default shard-slot count per building
(when `world.buildableCatalog`'s `overridesShardSlotCount` is `false`,
which is the normal case) and the exact overclock percent granted per
shard are both real values this project hasn't confirmed live yet —
confirm both before relying on a specific shard count/clock ceiling for
a real build.

### `world.setRecipe`
`params: {"buildableId", "recipeClass"}`. Errors: `TARGET_NOT_FOUND`,
`INVALID_RECIPE` (not a real `UFGRecipe`), `RECIPE_NOT_COMPATIBLE` (not
producible in this building), `INVENTORY_NOT_EMPTY` (both input and output
inventories must be empty first — a real game constraint, not this mod's
own caution).

### `world.deleteBuilding`
`params: {"buildableId"}`. Real dismantle (proper connection/save
cleanup) — not a cheat despawn. Works on both normal and lightweight
(foundation-style) ids. Deleting a pipe also cleans up its flow-fill
indicator widget; deleting a Pipeline Junction does **not** delete pipes
still attached to it (they're left dangling, and the extractor/machine
at their other end stays "occupied" until you delete those too).

**The same non-cascading behavior applies to conveyor belts/lifts/
pipes/hypertubes/tracks connecting any two buildables** — deleting
either endpoint (e.g. a Splitter) leaves the belt/pipe/etc. standing as
its own orphaned buildable, exactly matching real Satisfactory (dismantling
a machine doesn't dismantle belts touching it either). **Repeated-
construction test scripts that reuse the same coordinates across trials
MUST delete the constructed belt/pipe too, not just the machines/
attachments they explicitly placed**, or leftover geometry piles up at
that spot and silently causes real, hard-to-diagnose construction
failures ("Conveyor Belt is too long!"/"Overlapping another object's
clearance") on later attempts that have nothing to do with distance,
timing, or player state — this cost a full investigation to root-cause
once already (2026-08-30, see `docs/splitter-port-control-test.md`).
`controller/splitter_matrix_test.py`'s `cleanup_cell()` is a real,
tested reference implementation of doing this correctly (symmetric
radius on X/Y/Z, deletes everything in the area) — reuse or copy it
rather than writing a fresh ad hoc cleanup loop.

**Refunds construction cost to the player's carried inventory** (fixed
2026-08-30 — a real bug before this: the previous implementation only
called the game's `Dismantle()`, which does **not** itself refund
anything; a separate `GetDismantleRefund()` call is required and this
mod never made it, silently destroying every dismantled buildable's full
construction cost with no refund at all. Confirmed live to have cost the
user several thousand real Iron Plates across repeated test rebuilds
before being caught — see `docs/placement-lessons.md`). If no local
player/inventory can be found the refund is logged as lost rather than
silently dropped; dismantling itself still succeeds either way.

Also works on vehicle ids returned by `world.constructVehicle`/
`world.vehicles`. `AFGVehicle` is not an `AFGBuildable`, so this falls back
to a `TActorIterator<AFGVehicle>` path-name match when the normal
`FindBuildableById` lookup misses.

### `world.setTimeOfDay`
`params: {"hour" (0-23, required), "minute" (0-59, optional, default 0)}`.
Forces the day/night cycle to that time; the cycle continues advancing
normally afterward (a one-shot jump, not a pause).

### `world.sendChatMessage`
`params: {"message" (required), "sender" (optional, default "AIMod AI")}`.
Posts into the player's in-game chat as a `"CustomMessage"`.

### `world.placeBuilding` — asynchronous, `result.buildableId` on success
`params`:
- `recipeClass` (required) — a real building recipe path.
- `x`, `y` (required, numbers).
- `z` (optional) — anchors the ground-trace search center; see
  `world.groundHeight` above. Default: player's current Z.
- `gridSnapSize` (optional, default `100` = snap to 1m grid; `0` disables).
- `rotationScrollDelta` (optional, default `0`) — **unreliable for anything
  beyond roughly ±1**, a raw, non-linear scroll-wheel-click count. Prefer
  `yaw` or `faceBuildableId` for a specific orientation.
- `yaw` (optional) — an exact absolute world yaw in degrees. Takes priority
  over `rotationScrollDelta`.
- `faceBuildableId` (optional) — resolves an existing buildable's real
  position and computes `yaw` automatically to face it. Takes priority over
  an explicit `yaw`. Fails with `FACE_TARGET_NOT_FOUND` if the id doesn't
  resolve. Only orients the whole building, not a specific connector on a
  multi-connector building (splitters etc.).
- `ignoreAimLocation`, `ignorePlayerEncroachment`, `ignoreClearance`,
  `ignoreInvalidFloor` (all optional booleans, default `false`) — named
  bypasses of specific placement-validity gates, for autonomous layouts
  where a real player's aim/proximity isn't meaningful. Using these accepts
  real collision/overlap risk; every *other* disqualifier (resource
  requirements, structural validity, snap requirements) still applies.
- `ignoreGroundTrace` (optional bool, default `false`) — skips the ground
  trace entirely and places at the literal `(x, y, z)` given, instead of
  letting a line trace resolve the real Z. **Requires `z` to be provided**
  (fails `MISSING_REFERENCE_Z` otherwise). The ground trace is unreliable
  in two confirmed ways this exists to route around: at an exact
  foundation-tile edge it can non-deterministically find either the real
  top surface or unrelated lower terrain, and above open interior space
  (e.g. a roof over a room) it always falls through to the floor below
  rather than the intended height. Use `world.groundHeight` once against a
  known real surface nearby, compute the true target Z from that (see
  `docs/placement-lessons.md`), then pass it here for a placement no trace
  can perturb.

**Never use this for an extractor recipe** (Miner/Water Pump/Fracking
building) — it will refuse with `WRONG_METHOD_FOR_EXTRACTOR`; use
`world.placeExtractor` instead. **Never use this for a vehicle recipe**
(Drone/Tractor/Truck/Explorer/Cyber Wagon/Golf Cart) either — it refuses
with `WRONG_METHOD_FOR_VEHICLE`; use `world.constructVehicle`. **Never use
this for a spline-snapped buildable** (Conveyor Monitor is the one known
example) — it refuses with `WRONG_METHOD_FOR_SPLINE_SNAPPED`; **this one
is a confirmed live crash, not a hypothetical** — placing
`Recipe_ConveyorMonitor` through this path crashed the entire game
process (`EXCEPTION_ACCESS_VIOLATION` in
`AFGBuildableSplineSnappedBase::SetSnappedSplineBuildable()`, a real null
dereference on the never-supplied snap target). No dedicated RPC exists
yet for this category — do not attempt to bypass the refusal. Errors:
`INVALID_RECIPE`, `WRONG_METHOD_FOR_EXTRACTOR`, `WRONG_METHOD_FOR_VEHICLE`,
`WRONG_METHOD_FOR_SPLINE_SNAPPED`, `CANNOT_CONSTRUCT` (a real disqualifier
blocked it — message names which one), `BUILD_DISTANCE_EXCEEDED` (only if
the player has enabled the "Limit RPC Build Distance" mod setting).

### `world.placeExtractor` — asynchronous, `result.buildableId` on success
`params: {"nodeId" (required), "recipeClass" (optional, default
Recipe_MinerMk1)}`. Works for any extractor: Miner Mk1-3, Water Pump, Oil
Pump, Fracking Smasher (Pressurizer — needs a `FrackingCore` node),
Fracking Extractor (needs an *activated* `FrackingSatellite` — poll
`world.resourceNodes`' `satelliteState` until `"Active"` after powering its
core). Errors: `NODE_NOT_FOUND`, `NODE_OCCUPIED`, `INVALID_RECIPE`,
`CANNOT_CONSTRUCT`, `BUILD_DISTANCE_EXCEEDED` (same mod-setting gate as
above).

### `world.constructVehicle` — asynchronous, `result.buildableId` on success
`params: {"recipeClass" (required), "droneStationId" (optional),
"x"/"y"/"z" (optional, numbers), "ignoreGroundTrace" (optional bool),
"yaw" (optional)}`. Drones and wheeled vehicles (Tractor/Truck/Explorer/
Cyber Wagon/Golf Cart) are hologram-driven, the same mechanism every other
`Construct*` method uses — confirmed from source, **not** the Portable
Miner's equipment-dispenser mechanism. `world.placeBuilding` refuses
vehicle recipes outright (`WRONG_METHOD_FOR_VEHICLE`); this is the only
path.

**Drones require `droneStationId`** — a real, placed, unoccupied
`AFGBuildableDroneStation`'s id (from `world.buildables`). The hologram
snaps to it the same way `world.placeExtractor` snaps to a resource node.
This is enforced because a Drone hologram has a mandatory station
reference (`UFGCDMustSnapStation`/`UFGCDOccupiedStation`/
`UFGCDDroneStationHasDrone` disqualifiers) that this call never bypasses —
by analogy with the confirmed extractor crash (a hard engine assertion on
an equally "mandatory but unset" reference), whether building a Drone
hologram unsnapped is actually safe is unconfirmed and not worth testing.

**Wheeled vehicles**: omit `droneStationId`, use `x`/`y`/`z` /
`ignoreGroundTrace` exactly like `world.placeBuilding` — no known mandatory
snap target for these.

A freshly-built vehicle needs fuel before it can move (no RPC for this
yet), and a freshly-built Drone additionally needs its station paired to a
destination (`AFGDroneSubsystem::Server_PairStations` exists in source as
a public function but is not yet exposed here) before it will fly a route
— this call only covers construction. Errors: `INVALID_RECIPE`,
`WRONG_METHOD_FOR_VEHICLE` (a normal building recipe was passed to this
instead of `world.placeBuilding`), `TARGET_NOT_FOUND` (bad
`droneStationId`), `HOLOGRAM_SPAWN_FAILED` (recipe did not actually
resolve to a vehicle), `CANNOT_CONSTRUCT`, `CONSTRUCTION_UNCONFIRMED`,
`MISSING_REFERENCE_Z`. **Not yet live-tested** — implemented from source
research (hologram class hierarchy, disqualifier classes), not confirmed
against a real build.

### `world.placePortableMiner` — asynchronous, `result.buildableId` on success
`params: {"nodeId" (required, must be a real solid ore node, not a
Fracking core/satellite), "itemClass" (optional, default the real Portable
Miner item)}`. **Requires the player to already have one crafted** — this
consumes a real inventory item, it does not synthesize one. Works whether
the item is in the player's general inventory or already sitting in the
ARMS equipment slot. Errors: `NODE_NOT_FOUND`, `NODE_OCCUPIED`,
`INVALID_NODE_TYPE` (not a real solid node), `PORTABLE_MINER_NOT_IN_INVENTORY`,
`INVALID_RECIPE`, `EQUIP_FAILED`, `CONSTRUCTION_UNCONFIRMED`.

### `world.retrievePortableMinerInventory` — synchronous
`params: {"portableMinerId"}`. Moves everything in the Portable Miner's
output into the player's own inventory. Errors: `TARGET_NOT_FOUND`,
`NOTHING_TO_RETRIEVE` (output empty), `INVENTORY_FULL` (output has items
but none fit — nothing is lost, they just stay in the miner for later).

### `world.movePortableMinerToInventory` — synchronous, no params
Moves a Portable Miner item from the player's ARMS equipment slot back
into their general inventory. Needed because a stationary Miner's real
construction-cost check does not see items equipped in ARMS — confirmed
live: `world.placeExtractor` failed `"Missing materials!"` for the
Portable Miner ingredient despite one being equipped. No-op success (not
an error) if ARMS does not currently hold one — the general inventory may
already have one, or the player may genuinely have none.

### `world.simulatedCraft` — synchronous, `result.buildableId` on success
`params: {"recipeClass"}`. Simulates crafting a **handheld item only** —
every product of the recipe must resolve to a `UFGEquipmentDescriptor`
subclass (`NOT_HANDHELD_ITEM` otherwise), rejecting building/part/bulk-
component recipes entirely. A deliberate alternative to driving the real
Workshop/WorkBench crafting UI (never implemented), for a player who has
the real ingredients but cannot reach a bench — e.g.
`/Game/FactoryGame/Recipes/Equipment/Recipe_PortableMiner.Recipe_PortableMiner_C`
(2 Iron Plate + 4 Iron Rod → 1 Portable Miner). Real inventory mutation:
verifies every ingredient is affordable first (`INSUFFICIENT_INGREDIENTS`,
naming what is short, if not) before removing any of them, then grants
the product(s) in the recipe's own stated amounts. Other error:
`INVALID_RECIPE`.

### `world.centralStorage` — read-only, no params
```json
{ "protocolVersion": 1, "isCentralStorageBuilt": true, "items": [{"itemClass","itemName","amount"}, ...] }
```
Reports everything currently held in the Dimensional Depot (real class
name `AFGCentralStorageSubsystem` — "Dimensional Depot" is only the
in-game display name). **Important**: items here are NOT automatically
usable for RPC-driven construction — `world.placeBuilding` and friends
only check the player's carried inventory, not Depot storage. Per the
user, real interactive player building pulls from both pools
automatically (with a player-configurable draw-order preference); this
RPC surface does not yet replicate that. Use
`world.withdrawFromCentralStorage` first if a build is failing
`"Missing materials!"` despite the Depot showing plenty.

**`isCentralStorageBuilt` is not reliable — trust `items` instead**
(fixed 2026-08-30, real bug: this call used to gate the item lookup
behind that flag, which tracks a separate container-registration
bookkeeping array that was confirmed live to read `false` even with 12
real, already-built Depot containers and thousands of real items
present — every query silently reported an empty Depot regardless of
actual contents). The flag is still returned for reference but no
longer gates anything; an empty `items` array is now the trustworthy
signal for "genuinely nothing in the Depot."

### `world.playerInventory` — read-only, no params
```json
{ "protocolVersion": 1, "hasPlayer": true, "items": [{"itemClass","itemName","amount"}, ...] }
```
Reports the local player's CARRIED inventory (general backpack, not the
ARMS equipment slot, not the Dimensional Depot) - added 2026-08-30,
since no RPC previously exposed carried-inventory counts at all (only
`world.centralStorage`'s Depot side was readable). One aggregated entry
per distinct item class, summed across every stack. `hasPlayer:false`
with empty `items` if no local `AFGCharacterPlayer` exists, rather than
an error. Sum this with `world.centralStorage`'s `items` for a
"combined" total of a given item across both pools.

### `world.withdrawFromCentralStorage` — synchronous
`params: {"itemClass","amount"}`. Moves items from the Dimensional Depot
into the player's carried inventory, making them usable for subsequent
construction calls. Clamps to whatever is actually in the Depot — asking
for more than available is not an error, it withdraws what it can.
Errors: `NO_CENTRAL_STORAGE` (the `AFGCentralStorageSubsystem` itself
doesn't exist for this world — should never happen in practice; fixed
2026-08-30 to no longer also fire whenever `isCentralStorageBuilt` was
`false`, the same unreliable-flag bug as `world.centralStorage`, which
silently blocked every withdrawal even with a real, populated Depot),
`NOTHING_WITHDRAWN` (Depot has none of the requested item),
`INVENTORY_FULL` (some of the withdrawn amount did not fit in inventory —
that portion is genuinely lost, no way to redeposit it programmatically).

### `world.spawnCreature` — synchronous, `result.buildableId` on success
`params: {"creatureClass" (required), "distanceFromPlayer" (optional,
default 800, clamped to [100, 5000]), "scale" (optional, default 1.0,
clamped to [0.05, 20.0])}`. Spawns a real `AFGCreature` a short distance in
front of the player, on real ground (same trace as `world.placeBuilding`).
`creatureClass` must resolve to an `AFGCreature`-derived Blueprint generated
class, e.g.
`/Game/FactoryGame/Character/Creature/Wildlife/SpaceRabbit/Char_SpaceRabbit.Char_SpaceRabbit_C`
for the harmless passive critter, or `Char_Hog`/`Char_Stinger`/`Char_SpitterForestSmall`
etc. under `Character/Creature/Enemy/` for hostile ones — check
`Content/FactoryGame/Character/Creature/` for the full roster. `scale` is a
uniform scale factor applied to the spawn transform — untested against
FactoryGame's own creature Blueprints, since collision/AI ranges are often
hardcoded independent of visual scale, so extreme values may look/behave
oddly even when they spawn cleanly.

**Off by default.** The player must explicitly enable "Allow Creature
Spawning" in AIMod's mod settings first — this is one of only two
capabilities (with `UnlimitedResources`) an external AI controller can never
turn on itself. A disabled request fails with `CREATURE_SPAWNING_DISABLED`
rather than silently no-op'ing. Other errors: `INVALID_CREATURE_CLASS`
(didn't resolve to an `AFGCreature` subclass), `SPAWN_FAILED`.

### `world.despawnCreature` — synchronous
`params: {"creatureId" (required, a spawnCreature `result.buildableId`)}`.
Removes a creature by id. Narrowly scoped to `AFGCreature` lookup, not a
generic "destroy any actor" capability. Errors: `TARGET_NOT_FOUND`.

### `world.testPowerConnection` / `world.connectPower` — asynchronous
Same params, `test` is a dry run (never touches the save), `connect` is
real. `params: {"buildableIdA", "buildableIdB", "ignoreAimLocation"
(optional bool), "ignoreWireSnap" (optional bool)}`. Check
`world.powerLineLimits`' `maxLength` before a long connection — exceeding
it fails and is not bypassable by either ignore flag (it's a real
deterministic distance check); chain through an intermediate power pole
instead.

### `world.testConveyorBelt` / `world.connectConveyor` — asynchronous
`params: {"sourceBuildableId", "destBuildableId", "recipeClass" (optional,
default Recipe_ConveyorBeltMk1, any Mk1-6), "routeMode" (optional,
`"Straight"`/`"Curve"`/`"Auto"`), "instigatorStrategy" (optional,
case-insensitive, default `"PlayerController"`), "sourceConnectorPosition"/
"destConnectorPosition" (optional, `{"x","y","z"}`)}`. `source`/`dest`
don't have to be machines — belts/splitters/mergers work too.

**`sourceConnectorPosition`/`destConnectorPosition`** (added 2026-08-30,
explicit user requirement for deterministic per-port control): without
these, source/dest resolve to "the first free connector of the right
direction" — on a multi-output buildable like a Splitter (3 outputs) or
multi-input Merger (3 inputs), which specific one gets used is otherwise
unspecified/unpredictable. Passing the real world position of the exact
connector you want (read from a prior `world.connections` call —
`controller/satisfactory_ai/splitters.py`'s `get_splitter_output_facing`/
`get_splitter_input` resolve a cardinal direction to that exact position)
targets that one specific connector — a ~10cm tolerance accounts for
float round-tripping through JSON, not for picking "close enough."
Errors `NO_FACTORY_CONNECTION` if nothing free is within tolerance,
never silently substitutes a different connector. See
`docs/splitter-port-control-test.md`.

**`instigatorStrategy`** (added 2026-08-30, real known issue, still being
worked): `"RealCharacter"` drives the actual player's real BuildGun —
proven reliable, but visibly hijacks the camera during construction
(confirmed live: rotates the player's view every poll tick for the
duration of each call). `"AIController"`/`"PlayerController"` spawn a
throwaway decoy pawn+controller as the construction instigator instead,
so the real player is never touched — both CONFIRMED (live-tested
back-to-back) to get stuck on a permanent `UFGCDInitializing`
disqualifier that never clears; controller class is ruled out as the
variable. `"LocalPlayer"` (added 2026-08-30, **written and compiled but
NOT YET LIVE-TESTED** — no redeploy was possible when it was written)
spawns a genuine second `ULocalPlayer` via `UGameInstance::
CreateLocalPlayer()` instead of a bare decoy, on the hypothesis that
genuine local-player identity is what's actually required — see
`docs/camera-hijack-and-second-player-research.md` for the full research
this is based on. Four interchangeable values on the same build
specifically so this can keep being debugged without a fresh compile per
attempt. Until `"LocalPlayer"` is confirmed working, `"RealCharacter"`
remains the only strategy that reliably finishes a real belt.

### `world.testConveyorLift` / `world.connectConveyorLift` — asynchronous
Same shape as belts, `recipeClass` default Recipe_ConveyorLiftMk1 (any
Mk1-6). No `routeMode`.

### `world.testPipe` / `world.connectPipe` — asynchronous
`params: {"sourceBuildableId", "destBuildableId", "recipeClass" (optional,
default Recipe_Pipeline, or Recipe_PipelineMK2)}`. Handles `PCT_ANY`-only
connectors (Storage Tanks, Pipeline Junctions) automatically. Real max
segment length is ~5600 units (see `world.pipelineTiers`) — for a longer
run, route through an intermediate Pipeline Junction, keeping each hop
under that limit.

### `world.testHypertube` / `world.connectHypertube` — asynchronous
`params: {"sourceBuildableId", "destBuildableId"}` — no `recipeClass`, only
one tube type exists. Real max segment length is ~10000 units (longer than
fluid pipes).

### `world.testRailroadTrack` / `world.constructRailroadTrack` — asynchronous
`params: {"sourceBuildableId", "destBuildableId", "recipeClass"
(required — no confirmed default, query `world.recipeCatalog` for the real
track recipe path first)}`. Same shape as `world.testPipe`/
`world.connectPipe` — both ends must be existing buildables with a free
railroad track connection component (a Train Station platform, or an
existing track segment's open end). Real numeric limits from source
(unconfirmed live): max segment length ~100 m, min curve radius ~30 m, max
grade 25°. Deliberately point-to-point only — switches (3+ track pieces
meeting at one point) and signals are out of scope; the disqualifiers for
too-long/too-short/too-steep/too-sharp-a-turn are never bypassed by this
call. **Not yet live-tested.**

### `world.constructVehiclePathSegment` — asynchronous, `result.buildableId` on success
`params: {"recipeClass" (required), "startX"/"startY" (required numbers),
"endX"/"endY" (required numbers), "startZ"/"endZ" (optional),
"ignoreGroundTrace" (optional bool)}`. Unlike every other segment-based
method above, the endpoints are **literal coordinates, not existing
buildable ids** — a real path node is automatically created at each end if
nothing existing is nearby (confirmed from source). Same
`ignoreGroundTrace`/literal-Z convention as `world.placeBuilding` — the
way to lay a path on a flat foundation platform instead of raw terrain.
Passing a point within ~8 m of an existing path node/segment lets it snap
into that network instead of creating a new one. Recipe paths live under
`Content/FactoryGame/Buildable/Vehicle/{Explorer,Golfcart,Tractor,Truck}/`
per vehicle type, plus a universal variant — query `world.recipeCatalog`
for the exact path. Does **not** cover assigning a built vehicle to
auto-drive a route over the segments you build — that's a separate,
not-yet-exposed capability (real source API exists:
`AFGWheeledVehicleIdentifier::SetVehicleRoute`/`AddWaypoint`/
`SetAutopilotEnabled`). **Not yet live-tested.**

### `world.milestoneProgress` — synchronous, no params
```json
{
  "protocolVersion": 1,
  "highestAvailableTechTier": 3,
  "maxAllowedTechTier": 6,
  "activeSchematic": "/Game/.../Schematic_2-3.Schematic_2-3_C",
  "tiers": [
    {
      "tier": 2,
      "techTierState": "Available",
      "schematics": [
        {
          "schematicClass": "...", "displayName": "Obstacle Traversal", "type": "Milestone",
          "purchased": false, "isActive": true,
          "cost": [{"itemClass":"...","itemName":"Iron Plate","amount":50}],
          "remainingCost": [{"itemClass":"...","itemName":"Iron Plate","amount":30}],
          "paidOffCost": [{"itemClass":"...","itemName":"Iron Plate","amount":20}]
        }
      ]
    }
  ],
  "spaceElevators": [
    { "id": "...", "buildableClass": "...", "isFullyUpgraded": false, "isReadyToUpgrade": false,
      "nextPhaseCost": [...], "inputInventory": [...] }
  ]
}
```
Real HUB milestone/tutorial progress by tier
(`AFGSchematicManager::GetHubSchematicsForTier`/`GetTechTierState`/
`GetRemainingCostFor`/`GetPaidOffCostFor`/`IsSchematicPurchased`/
`GetActiveSchematic` — all real, non-stub getters), plus every
`AFGBuildableSpaceElevator`'s phase-upgrade state. Tiers 0–14 are scanned;
a tier only appears if it has real schematics. **"The HUB" holds no
inventory of its own** — milestone payment is pure item-amount bookkeeping
on the schematic manager, not a physical buildable inventory; see
`world.payMilestone` for the write side. The Space Elevator, by contrast,
is a normal `AFGBuildableFactory` — already visible in `world.buildables`
and already belt-connectable via `world.connectConveyor` with zero new
code, matching that it "can be fed with conveyor belts" unlike the HUB.

### `world.payMilestone` — synchronous
`params: {"schematicClass" (optional — defaults to the manager's current
active schematic), "dryRun" (optional bool, default false)}`. Moves items
from the player's **carried** inventory (not the Dimensional Depot — see
`world.withdrawFromCentralStorage` above if the needed items are there
instead) toward a HUB milestone's remaining cost, then calls the real
`AFGSchematicManager::PayOffOnSchematic` to register the payment. Per item
still owed, submits `min(remainingAmount, carriedAmount)` — never more
than owed, never more than carried, so it can only ever move real,
affordable amounts. Always run `dryRun: true` first — it computes the same
plan without touching inventory or the schematic manager, returned as
`result.detail.wouldSubmit`/`result.detail.shortfall`.

On a real (non-dry-run) call: fails with `NOTHING_TO_SUBMIT` if the player
carries none of what's owed (never a silent no-op success). Otherwise
removes the submitted items, calls `PayOffOnSchematic`, and — if that
returns `false` — **restores every removed item back to the player's
inventory** before failing with `PAYOFF_REJECTED`, the same
restore-on-failure discipline as the Portable Miner ARMS-slot move. Detail
is reported under `result.detail` (`schematicClass`, `submitted`,
`shortfall`, and on a real successful call, `amountArrayAfterCall` — a
diagnostic dump of whatever `PayOffOnSchematic` left in its by-reference
`amount` parameter, since its real contract there is unconfirmed from
source).

**Not yet live-tested** — `PayOffOnSchematic`'s real behavior (whether it
requires the target to already be the *active* schematic, whether it
mutates the amount it's given) is unconfirmed; this function deliberately
does not guess at a "must be active" restriction that the source doesn't
actually state, trusting the real engine call to enforce or not enforce it
itself. Test with `dryRun: true` first, then a cheap/abundant item, before
relying on this for anything valuable.

### `world.mamStatus` — synchronous, no params
```json
{
  "protocolVersion": 1,
  "researchState": "Researching",
  "canConductMultipleResearch": false,
  "ongoingResearch": [
    { "schematicClass": "...", "displayName": "...", "type": "MAM",
      "initiatingResearchTree": "...", "timeLeftSeconds": 42.1 }
  ],
  "completedResearch": [
    { "schematicClass": "...", "displayName": "...", "type": "HardDrive", "initiatingResearchTree": "..." }
  ],
  "unclaimedHardDrives": [
    { "pendingRewards": [{"schematicClass":"...","displayName":"Alternate: Turbo Rifle Ammo"}],
      "canReroll": true, "hasReroll": true }
  ],
  "researchTrees": [
    { "researchTreeClass": "...", "displayName": "Alien Organisms", "status": "Unlocked",
      "nodes": [
        { "schematicClass": "...", "displayName": "...", "type": "MAM",
          "schematicState": "Available", "cost": [{"itemClass":"...","itemName":"Alien DNA Capsule","amount":1}] }
      ]
    }
  ]
}
```
Full M.A.M. status in one call. `schematicState` on each node is the same
`Locked`/`Available`/`Purchased`/`Hidden` enum `world.milestoneProgress`
uses for HUB schematics — `Available` is "locked but selectable right
now" (dependencies met, not yet purchased); `Purchased` is "already
unlocked." Only research trees that aren't fully `Locked` get their nodes
expanded (a locked tree isn't visible to the real player either).
`unclaimedHardDrives` are hard-drive analyses that finished and are
waiting for you to pick one of the randomly-rolled alternate-recipe
rewards — this is how to tell a player "hard drive research is complete,
go review your new alternate recipes."

**No stable hard-drive id is exposed** (the real engine field exists but
isn't reflectable — see `world.claimMamHardDriveReward` below) — identify
a specific hard drive by any one of its current `pendingRewards` schematic
classes instead.

### `world.startMamResearch` — synchronous
`params: {"schematicClass" (required), "researchTreeClass" (required),
"dryRun" (optional bool, default false)}`. Unlike `world.payMilestone`,
M.A.M. research cost is paid **atomically** — one call both submits the
full ingredient cost from carried inventory and starts the research timer
(there's no real "submit some ingredients now, more later" mechanic to
expose here). `dryRun: true` runs only the validation
(`CanResearchBeInitiated`/`CanAffordResearch`) and reports the schematic's
real cost via `result.detail.cost` without touching anything — always
check this first. Fails with `CANNOT_RESEARCH` (already researching/
researched, tree not unlocked, or dependencies unmet) or
`INSUFFICIENT_INGREDIENTS` (carried inventory short — same carried-only
scope as `world.payMilestone`/`SimulatedCraft`, not the Dimensional
Depot) before ever touching inventory.

### `world.claimMamResearch` — synchronous
`params: {"schematicClass" (required)}`. Claims a finished research's
results. For a normal M.A.M. schematic this grants the real unlock
immediately. For a hard-drive-analysis schematic, this is the step that
turns the finished analysis into a new entry in `world.mamStatus`'s
`unclaimedHardDrives` — claiming the research itself does **not** give you
a recipe yet, you still need `world.claimMamHardDriveReward` after this.
Fails with `NOT_COMPLETE` if the schematic isn't a completed, unclaimed
research.

### `world.claimMamHardDriveReward` — synchronous
`params: {"schematicClass" (required) — one of the schematics currently
listed in some hard drive's `pendingRewards`}`. Picks that alternate
recipe as the hard drive's permanent reward. The target hard drive is
found by which one currently offers the given schematic, not by a numeric
id (Coffee Stain's real hard-drive-id field exists internally but isn't
reflectable — plain unrelected private field, unlike everything else this
mod reads via reflection). Safe in practice: the game excludes a schematic
already offered by one unclaimed hard drive from being rolled onto
another at the same time. Fails with `REWARD_NOT_FOUND` if no unclaimed
hard drive currently offers it (stale id, already claimed, or never
existed — re-query `world.mamStatus`).

### `world.rerollMamHardDrive` — synchronous
`params: {"schematicClass" (required) — any ONE of the target hard
drive's current `pendingRewards`}`. Rerolls that hard drive's reward
choices to a new random set. Same identify-by-current-reward lookup as
`world.claimMamHardDriveReward`. Fails with `CANNOT_REROLL` if no rerolls
are left for that drive, or no alternate recipes currently exist to reroll
into (message distinguishes the two). Since the reward set changes,
**re-query `world.mamStatus` afterward** — the new choices aren't echoed
back in this response.

**None of the five methods above are live-tested yet** — implemented from
source research only. `AFGResearchManager`/`UFGResearchTree`/
`UFGResearchTreeNode`/`UFGHardDrive` all have real, well-documented public
APIs (unlike much of this codebase), but the actual construct-path-style
runtime behavior (e.g. what `InitiateResearch` does if called twice, or
whether claiming a hard drive reward can ever legitimately fail) hasn't
been observed live.

### `world.trainStations` — synchronous, no params
```json
{ "protocolVersion": 1, "stations": [
  { "id": "...", "name": "Iron Ore Pickup", "trackGraphId": 0, "buildableClass": "..." }
] }
```
Every real railroad station, with its display name and the SAME buildable
id `world.buildables` already uses for it. Needed because a timetable
stop is identified by buildable id, and `world.buildables` alone doesn't
expose the human station name — this is how you find the right id to
build a `world.setTrainTimetable` request.

### `world.trains` — synchronous, no params
```json
{ "protocolVersion": 1, "trains": [
  { "id": "...", "name": "Train 1", "status": "SelfDriving",
    "selfDrivingEnabled": true, "selfDrivingError": "NoError",
    "dockingState": "Docked", "hasTimeTable": true,
    "timetable": [
      { "stationId": "...", "stationName": "Iron Ore Pickup",
        "dockingDefinition": "FullyLoadUnload", "dockForDuration": 15,
        "isDurationAndRule": false,
        "ignoreFullLoadUnloadIfTransferBlockedByFilters": false,
        "loadFilter": [], "unloadFilter": [] }
    ]
  }
] }
```
Every train (`AFGTrain`, an `AActor` — `id` is a `GetPathName()`, usable
with `world.setTrainTimetable`/`world.setTrainSelfDriving`, **not** with
`world.buildables`/`world.deleteBuilding`) with its full status and
timetable. `selfDrivingError` is the real reason a self-driving train
isn't moving (`NoPower`/`NoTimeTable`/`InvalidNextStop`/`NoPath`/
`StationUnreachable`/`StationUnreachableWithSignals`/`LongWaitAtSignal`),
not just a boolean.

### `world.setTrainTimetable` — synchronous
`params: {"trainId" (required), "stops" (required, non-empty array)}`.
Each stop: `{"stationBuildableId" (required — from world.buildables or
world.trainStations), "dockingDefinition" (optional, "LoadUnloadOnce" |
"FullyLoadUnload", default "LoadUnloadOnce"), "dockForDuration" (optional
seconds, default 15), "isDurationAndRule" (optional bool, default false —
when true, BOTH the duration AND the load/unload condition must be met
before departing, not either/or), "ignoreFullLoadUnloadIfTransferBlockedByFilters"
(optional bool), "loadFilter"/"unloadFilter" (optional arrays of item
class paths — restrict what this stop loads/unloads; empty means no
restriction)}. **Always replaces the entire timetable** — this mirrors
the real `AFGRailroadTimeTable::SetStops`'s own "replace everything"
semantics, not an incremental add. Creates a new time table
automatically if the train doesn't have one yet. Fails per-stop with
`TARGET_NOT_FOUND` if a `stationBuildableId` isn't a real, currently-
existing railroad station — checked for every stop before any change is
made, so a bad id never leaves a half-applied timetable.

### `world.setTrainSelfDriving` — synchronous
`params: {"trainId" (required), "enabled" (required bool)}`. Turns
autopilot on/off for a train. Does **not** fail just because the train
reports a self-driving error afterward (no time table, no path, etc.) —
that's real, useful state surfaced via `result.detail.selfDrivingError`,
not a failure of this call. Re-query `world.trains` (or read
`result.detail`) to see why a train isn't moving once self-driving is on.

### `world.droneStations` — synchronous, no params
```json
{ "protocolVersion": 1, "droneStations": [
  { "id": "...", "pairedStationId": "...", "droneStatus": "EnRoute",
    "activeFuelType": "...", "allowedFuelTypes": ["..."],
    "latestRoundTripTimeSeconds": 42.1, "averageIncomingItemRate": 12.0,
    "averageOutgoingItemRate": 8.0,
    "inputInventory": [...], "outputInventory": [...], "fuelInventory": [...] }
] }
```
Every drone station's real pairing, drone status
(`NoDrone`/`Docked`/`Loading`/`Takeoff`/`EnRoute`/`Docking`/`Unloading`/
`NotEnoughFuel`/`CannotUnload`), fuel state, trip statistics, and cargo/
fuel inventories. **Drone pairing is a single mutual link, not a
directional source/destination pair** — `pairedStationId` is the one
partner this station's cargo flows to and from (confirmed from source:
`AFGDroneStationInfo::mPairedStation` is one pointer, not a route list).

### `world.pairDroneStations` — synchronous
`params: {"stationBuildableId" (required), "targetStationBuildableId"
(optional — empty/omitted unpairs instead)}`. This is the RPC that
configures where a drone route goes — pairs two drone stations so cargo
flows between them both ways, or clears an existing pairing. Fails with
`TARGET_NOT_FOUND` if either id isn't a real, currently-existing drone
station. Verifies the pairing actually took via `GetPairedStation()`
afterward.

**None of the six train/drone methods above are live-tested yet** —
implemented from source research only (`AFGTrain`/`AFGRailroadTimeTable`/
`AFGTrainStationIdentifier`/`AFGDroneStationInfo` all have real,
well-documented public APIs).

### `world.cleanupOrphanedFlowIndicators` — synchronous, no params
```json
{ "protocolVersion": 1, "totalIndicators": 20, "attachedCount": 15, "orphanCount": 5, "deletedIds": ["...", "..."] }
```
Deletes every pipe fill-indicator widget not currently attached to a real
pipe (debris from pipes deleted by an older version of this mod, before an
indicator-cleanup fix existed). Exact — uses each pipe's real indicator
reference, not position guessing, so it's safe to run any time.

---

## Practical patterns

- **Dry-run before committing** for anything with a `test*`/`connect*` (or
  `test*`/`place*`) pair — the dry run reports the same disqualifiers
  without touching the save, useful for probing geometry (e.g. connector
  normals via `world.connections`/`world.pipeConnections`) before spending
  a real construction attempt.
- **Verify after every write**, especially construction — re-read the
  relevant telemetry (`world.buildables`, `world.pipeConnections`, etc.)
  and confirm the actual object state, not just `success: true`. This has
  caught real bugs in this mod's own history.
- **A transient `CANNOT_CONSTRUCT` failure on an objectively valid target
  is a known, recurring engine quirk** — retrying the identical call once,
  unchanged, has resolved it repeatedly across this mod's development. If
  it fails a second time in a row, treat it as real and investigate rather
  than retrying further.
- **Set `gridSnapSize: 0` for any placement whose exact coordinates
  matter** (anything computed by a geometry tool rather than eyeballed to
  a round number) — the default of `100` silently rounds to the nearest
  meter, which is invisible for ordinary grid-aligned building but
  destroys precision layouts (confirmed live: a circular foundation
  ring's exact touching-corner geometry was visibly broken by the
  default snap, and came out exact to a fraction of a millimeter once
  `gridSnapSize: 0` was set).
- **IDs go stale on save/reload** — re-fetch before reusing one from an
  earlier session or after the player saves/reloads.
- **Player-facing safety settings can silently change behavior**: if a
  request that used to work now fails with `BUILD_DISTANCE_EXCEEDED`, or a
  request that used to fail with a material-cost error now succeeds, the
  player has toggled one of AIMod's mod settings ("Limit RPC Build
  Distance From Player" / "Unlimited Resources for RPC Builds") — these are
  player-controlled and cannot be set by a request; if their state matters
  to you, ask the player rather than assuming.

## What this file doesn't cover

- **Recipe-chain math** (how many machines of what, to hit a target rate) —
  compute this yourself from `world.recipeCatalog`/`world.buildableCatalog`
  data; AIMod deliberately doesn't do this arithmetic itself. See
  `PLAYBOOK.md` for the "worked example" this project already ran once
  (Heavy Modular Frames).
- **Manual/hand-crafting** (Craft Bench / Workshop, no automated machine) —
  research-only as of this writing, not yet a real method. Check whether
  `world.craftItem`-style method exists in a live `success` response to an
  unrelated call's method list before assuming it does.
- Anything not listed above doesn't exist yet — an unrecognized `method`
  returns `UNKNOWN_METHOD` rather than doing something unexpected.
