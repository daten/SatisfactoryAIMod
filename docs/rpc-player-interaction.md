# RPCs and the player: teleports, camera, proximity, photo mode, vehicles

This page sets expectations for what the AIMod `world.*` RPCs can do to **you, the
local player**, while you are also holding the controls. If an RPC runs while you
are moving the mouse, flying, or standing in your base, it can repoint your
camera, briefly move your character, or silently fail — depending on which RPC it
is. Everything below is grounded in the mod source (file:line references at the
end of each section).

> **One-line summary:** Read-only `world.*` telemetry never touches you.
> Construction RPCs (place/connect/construct) drive the real build gun, so they
> **repoint your camera and leave it there**, a few of them need you **physically
> near** the work (belts auto-teleport you there and back), and **all** of them
> **fail in photo mode or while driving a vehicle**.

---

## 1. Your camera/aim can be repointed — and is NOT put back

Every construction RPC that uses the build gun momentarily sets your **control
rotation** (where your character/camera is aiming) toward the build target. This
is required: some of the game's placement checks ("Invalid aim location!") consult
your *real* camera direction even when the RPC supplies its own target, so the mod
points you at the target to make the build succeed.

**There is no save/restore of your camera direction.** After the RPC finishes,
your view is left pointing at (the pitch toward) the last thing it built. If you
are actively looking somewhere when the RPC fires, expect your view to snap and
stay snapped. (The only RPC that sets your rotation to a value *you* chose is
`world.teleportPlayer`, which uses the yaw you pass.)

**RPCs that repoint your camera (control rotation), not restored:**
`placeBuilding`, `placeExtractor`, `constructWaterPumpAtPosition`,
`constructWaterPumpNearReference`, `constructStackableSupport`,
`constructStackableSupportOnTop`, `connectConveyor`, `testConveyorBelt`,
`connectConveyorLift`, `testConveyorLift`, `connectPipe`, `testPipe`,
`connectHypertube`, `testHypertube`, `constructRailroadTrack`,
`testRailroadTrack`, `constructBeam`, `constructVehicle`, `constructTrainPlatform`
(pitch only), `testTrainPlatform`, `constructVehiclePathSegment`.

**Does NOT touch your camera:** `connectPower` / `testPowerConnection` (wires via a
synthetic connection, no camera trace), `setBuildableRotation` (rotates the placed
object, not you), and everything in §6.

> Note: the `test*` dry-run variants still drive the build gun to validate, so they
> repoint your camera exactly like the real build. A "dry run" is not side-effect
> free for your camera.

*Code: `AIModFunctionLibrary_Construction.cpp:1047-1092` (rationale + `SetControlRotation`),
re-asserted each poll tick `:1225`; no restore exists anywhere (grep for
`SavedControlRotation`/`RestoreControlRotation` → none).*

---

## 2. You can be teleported

**Explicitly:** `world.teleportPlayer` moves your character (with an optional
ground trace) and sets your facing. This is the expected one.

**Implicitly — belts only:** `world.connectConveyor` (and its dry-run
`world.testConveyorBelt`) will **auto-teleport you to the belt's midpoint** (and a
bit above it) if you are **more than ~2000 units away**, build the belt, then
**teleport you back** to where you were. A belt connection fundamentally needs the
real build-gun camera within reach, and moving you is the only reliable fix.

- If you are already near (< ~2000 u) — the interactive case — you are **not**
  moved.
- The return trip happens at every normal exit, so the net effect is a brief
  round-trip. **But** if the operation is interrupted mid-poll (crash, disconnect,
  the game hitching), you can be **left displaced** at the belt site.

No other construction RPC moves your character. (Foundations, rail, pipes, etc. do
*not* teleport you — they either don't need proximity, or they just fail if you're
too far; see §3.)

*Code: explicit `AIModFunctionLibrary.cpp:966-1027`; implicit belt teleport+restore
`AIModFunctionLibraryInternal.h:2394-2404` and restores at `:2436,2456,2508,2594,2603,2618,2632`.*

---

## 3. Which RPCs need you physically near the target

There are two build styles in the mod, and they behave oppositely on distance.

**NOT distance-gated — work from anywhere** (they inject a synthetic hit and bypass
the aim checks, so your camera's reach is irrelevant):
`placeBuilding` (foundations, machines, poles, **train stations**), `placeExtractor`,
`constructWaterPump*`, `constructStackableSupport*`, `constructBeam`,
`constructVehiclePathSegment`, `connectPower`.

- The *only* distance limit on these is an **opt-in** setting
  (`LimitBuildDistance`, default **off**; `MaxBuildDistance` default 8000). With it
  off, these place at any distance.

**Distance-sensitive — you (or the build front) must be near** (they rely on the
real build-gun camera trace, which is clamped to the build distance; from far away
the trace falls short and the build reports "too long"/"too steep"/fails):
- `connectConveyor` — proximity-sensitive, but the mod hides it by auto-teleporting
  you (§2).
- `connectConveyorLift` — uses your camera location for the trace; **no** teleport
  workaround, so distance matters.
- `constructRailroadTrack` **free-end form** (building to an open end that lands on
  a foundation) — needs the build front near you. *Track-to-track* form (both ends
  snap to existing rail connectors) is far less distance-sensitive.
- `connectPipe`, `connectHypertube` — same click/trace pattern, no teleport
  workaround.

> Practical rule for driving these from a script: keep the player within a few
> thousand units of the work (or let the belt RPC move them). `placeBuilding` and
> `connectPower` are the exceptions you can fire from across the map.

*Code: synthetic-hit / no-distance-class `AIModFunctionLibrary_Construction.cpp:845-868, 1122`;
opt-in gate `:858-868`; belt camera-reach note `AIModFunctionLibraryInternal.h:2370-2422`;
lift camera-origin trace `AIModFunctionLibrary_Connections.cpp:2271-2292`; rail free-end
ground landing `:3746-3765`; power (no trace) `:465-499`.*

---

## 4. Photo mode blocks all building

`world.enterPhotoMode` / `world.exitPhotoMode` / `world.setPhotoCamera` /
`world.takePhoto` operate the photo-mode camera and are the intended photo RPCs.
`setPhotoCamera` moves the **photo camera pawn**, not your character.

**While you are in photo mode, every construction RPC fails.** There is no explicit
guard in the code — photo mode simply holsters/removes the build gun, so the next
`placeBuilding`/`connect*`/`construct*` can't get a build gun or hologram and
returns **`NO_BUILD_GUN`** or **`HOLOGRAM_SPAWN_FAILED`**. If a script suddenly sees
those errors on previously-working builds, check whether photo mode is on.

- Read-only telemetry (§6) still works fine in photo mode.
- Fix: `world.exitPhotoMode`, then retry the build.

*Code: all in `AIModFunctionLibrary_Photo.cpp:33-145`; no PhotoMode check exists in
any build path; failure surfaces as `NO_BUILD_GUN`/`HOLOGRAM_SPAWN_FAILED`
(e.g. `AIModFunctionLibrary_Construction.cpp:1098-1120`).*

---

## 5. Driving a vehicle breaks player reads and construction

While you are **driving** (car, truck, train, etc.), the game reports the *vehicle*
as the player pawn, so the mod's "local player" lookup fails. Consequences:

- `world.player` returns **position/rotation (0,0,0)** (not your real spot).
- `world.playerInventory` returns empty; `world.targetedManufacturer` /
  `world.targetedResourceNode` stop working (they need your character + aim).
- **Every construction RPC fails with `NO_PLAYER`** while you're driving.

The mod does **not** fall back to reading the vehicle's position for these. To get
real player data or build, **exit the vehicle first**. (Vehicle-*targeting* RPCs —
`setTrainTimetable`, `setTrainSelfDriving`, `setTruckAutopilot`,
`setVehicleEngineParams` — act on a vehicle by id and are unaffected by whether
*you* are driving. Read a specific vehicle's live position from `world.vehicles`.)

*Code: `AIModFunctionLibrary_Telemetry.cpp:488-511` (cast to `AFGCharacterPlayer`
fails → default telemetry); build paths `NO_PLAYER` e.g.
`AIModFunctionLibraryInternal.h:2213-2216`.*

---

## 6. Safe RPCs — never move you, rotate your camera, or need proximity

All read-only telemetry. Safe to call any time, including mid-interaction, in photo
mode, and while driving (though the player-specific ones in the second group return
degraded data while driving — see §5):

- **Pure world/subsystem reads (don't even look at your character):**
  `help`, `version`, `buildables`, `connections`, `connectorLayout`, `splineGeometry`,
  `resourceNodes`, `waterVolumes`, `damageVolumes`, `probeHazard`, `groundHeight`,
  `terrainHeightGrid`, `projectAssembly`, `mantas`, `creatures`, `vehicles`,
  `vehiclePathNodes`, `trains`, `trainStations`, `trainCargoPlatforms`,
  `truckStations`, `droneStations`, `manufacturers`, `conveyorAttachments`,
  `splitterSortRules`, `pipeConnections`, `pipeFluidBoxes`, `pipeReservoirTiers`,
  `pipelineTiers`, `pipelinePumpTiers`, `powerPoles`, `priorityPowerSwitches`,
  `powerLineLimits`, `conveyorBeltTiers`, `conveyorLiftTiers`, `milestoneProgress`,
  `mamStatus`, `recipeCatalog`, `itemCatalog`, `buildableCatalog`,
  `constructionCost`, `activeEvents`, `timeOfDay`, `mapMarkers`, `mapMarkerIcons`,
  `chatHistory`, `portableMiners`, `centralStorage`.
- **Read your character but don't move/rotate it** (degraded while driving, §5):
  `player`, `playerInventory`, `targetedManufacturer`, `targetedResourceNode`.

**State-changing but still no teleport/camera/proximity** (they edit the world, not
you): `deleteBuilding`, `setClockSpeed`, `setRecipe`, `installPowerShard`,
`setBuildableRotation`, `setBuildableColor`, `setPowerSwitchOn`, `setPriority*`,
`setSplitterSortRules`, `addItemsToInventory`/`removeItems*`,
`upload*`/`withdraw*`/`centralStorage`, `payMilestone`, `setActiveMilestone`,
`launchShip`, `upgradeSpaceElevator`, map-marker setters, creature/manta/
damage-volume/event setters, portable-miner retrieve/move, `saveGame`, `setTimeOfDay`,
`sendChat`, and the vehicle-targeting RPCs from §5.

---

## `world.batch`

`world.batch` runs up to 100 sub-operations in one call and **inherits the behavior
of whatever it contains** — if any member op is a camera-repointing build or a
belt (teleport), the batch does that too.

---

## Quick reference

| Behavior | RPCs |
|---|---|
| Teleports you (explicit) | `teleportPlayer` |
| Teleports you (implicit, >2000u, returns you) | `connectConveyor`, `testConveyorBelt` |
| Repoints your camera (not restored) | all build-gun builds: `placeBuilding`, `placeExtractor`, `constructWaterPump*`, `constructStackableSupport*`, `connectConveyor*`, `connectConveyorLift*`, `connectPipe*`, `connectHypertube*`, `constructRailroadTrack*`, `constructBeam`, `constructVehicle`, `constructTrainPlatform*`, `constructVehiclePathSegment` |
| Needs you near the work | `connectConveyor` (auto-moves you), `connectConveyorLift`, `constructRailroadTrack` (free-end), `connectPipe`, `connectHypertube` |
| Works from any distance | `placeBuilding`, `placeExtractor`, `constructWaterPump*`, `constructStackableSupport*`, `constructBeam`, `constructVehiclePathSegment`, `connectPower` |
| Fails in photo mode (`NO_BUILD_GUN`/`HOLOGRAM_SPAWN_FAILED`) | all build-gun builds (§1) |
| Fails / degrades while you drive (`NO_PLAYER`, `(0,0,0)`) | all builds; `player`, `playerInventory`, `targeted*` |
| Always safe | all telemetry reads (§6) |
