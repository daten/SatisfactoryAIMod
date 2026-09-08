# Vehicle placement guide — trains, trucks, drones (live-verified 2026-09-07)

Agent-facing operational guide for building **rail, road-vehicle, and drone
transport** through the AIMod RPC. The goal is that any agent can drive these
systems predictably **without re-discovering the nuanced gotchas** we spent
weeks finding live.

This distils and supersedes the pre-live-test vehicle notes in
`placement-lessons.md` (§"train timetables + drone station pairing", §"Rail/
vehicle path layout tips", §"Railroad tracks and vehicle paths", §"Drones and
wheeled vehicles"). For the shared placement **fundamentals** referenced below,
`placement-lessons.md` remains authoritative.

Executable counterpart: `controller/satisfactory_ai/vehicles.py` encodes the
proven sequences here as callable helpers.

Status legend: **WORKS** (live-verified end to end) · **ARMS** (RPC sets up all
state correctly but the engine won't finish autonomously in-world) · **BLOCKED**
(a known wall) · **PENDING** (built, awaiting a redeploy to verify).

---

## 0. Cross-cutting fundamentals (apply to ALL placement)

These bit us repeatedly; they are not vehicle-specific but you must obey them.
Authoritative detail in `placement-lessons.md`; the short version:

- **Always pass an explicit `yaw`** on `placeBuilding` — never rely on a default.
- **Set `gridSnapSize: 0`** for precise geometry (default 100 silently rounds).
- **Prefer `ignoreGroundTrace: true` + an explicit `z`** for literal-coordinate
  placement. A buildable's reported `position` is its **pivot, not its top
  surface** — read `world.buildables[].bounds` to get the real top before
  stacking anything on it.
- **Teleport the player near the work for CONNECT/PLACE calls** — belt/pipe/power
  connection validation is camera/player-distance dependent. (Exception: rail
  curves — see Trains §3, teleport *away*.)
- **Never trust `success: true` alone** — verify via telemetry
  (`world.connections`, `world.droneStations`, `world.vehiclePathNodes`,
  `world.trainStations`, `world.vehicles`).
- **Delete only IDs you tracked** — never bounding-box sweeps (a broad sweep
  once deleted a pre-existing miner's lift). `deleteBuilding` is vehicle-aware
  (dismantles `AFGVehicle`/rail vehicles/drones too).
- **Long hops need the player near** (~<5000u) or a `teleportPlayer` first;
  belts fail "too long" beyond ~5600u from the camera.

---

## 1. DRONES — **WORKS end to end**

A drone autonomously carries cargo from station A to station B. Fully
live-verified 2026-09-07.

**Recipes / items**
- Station: `Recipe_DroneStation_C`
- Drone: `Recipe_DroneTransport_C`
- Valid fuel (any): `Desc_Battery_C` (also Fuel/Turbofuel/RocketFuel/etc.)

**Build sequence (each step matters):**
1. `placeBuilding` two `Recipe_DroneStation_C` (footprint 2400×2400×1450; keep
   them well apart). Explicit yaw, `z` = deck top.
2. **Run power to BOTH stations.** Drone stations are powered factory buildings;
   an unpowered station's drone sits `Docked` forever. *(This was the single
   biggest time-sink — the drone looked "frozen" but was just unpowered.)*
3. **Pair them BOTH ways**: `pairDroneStations(A→B)` **and** `pairDroneStations(B→A)`.
   One-way pairing leaves B unpaired and the route never completes.
4. Spawn the drone at A: `constructVehicle(recipeClass=Recipe_DroneTransport_C,
   droneStationId=<A id>, x,y,z)`. The RPC snaps the drone to the station's
   dock; if you ever see "Must snap to a Drone Port!" the hit must target the
   dock location (the RPC handles this).
5. **Load cargo into A's OUTPUT** and **fuel into each station's FUEL** with
   `addItemsToInventory`:
   - `inventoryRole="output"` = the **send/pickup** buffer (drone loads FROM output).
   - `inventoryRole="input"` = the **drop-off** buffer (drone unloads INTO input at the destination).
   - `inventoryRole="fuel"` = the dedicated fuel input. **`addItemsToInventory`
     also ARMS the station's active fuel type** (a raw inventory add does not —
     `activeFuelType` would stay empty and the drone won't fly).

**Verify:** `world.droneStations` reports `pairedStationId`, `droneStatus`
(`Docked`→`Takeoff`→`EnRoute`→`Docking`→…), `activeFuelType`, and input/output/
fuel contents. A working run shows the destination's receiving inventory filling
and `droneStatus` cycling.

**Gotchas recap:** power required · mutual pairing · output=send / input=dropoff ·
fuel must be armed via `addItemsToInventory` · drone spawns snapped to the dock.

---

## 2. TRUCKS / VEHICLE PATHS — infra **WORKS**, autopilot **ARMS**, driving **BLOCKED by terrain**

The truck-autopilot RPC arms every piece of state correctly; whether the truck
physically drives depends on a **clean, traversable path**, which is a *site*
problem, not an RPC problem.

**Recipes / items**
- Truck station: `Recipe_TruckStation_C` (footprint 1700×1500×1245)
- Truck: `Recipe_Truck_C`; path segment: `Recipe_VehiclePath_Truck_C`
- Fuel: `Desc_Coal_C` (or any truck fuel)

**Model (learned live):**
- Each truck station auto-spawns a `VehiclePathNode_DockingStation` **exactly at
  its placement XY** — that node is the point a path must reach to dock.
- **Vehicle path segments are DIRECTIONAL** (start→end, arriving/leaving
  connections). A loop is a *directed* cycle; a truck can only go forward.
- **`constructVehiclePathSegment` succeeding does NOT mean the segment is
  drivable.** The game computes a per-vehicle-type traversability mask from
  path validation; a segment that clips terrain/trees builds but is **invalid**
  (`validSegmentsForPreset` < total) → the route solver skips it → the truck
  reports **`StationUnreachable`**.
- Path-segment endpoints placed exactly on a docking node's XY usually **snap**
  onto it; when they instead create a separate coincident node, wire them
  together with `mergeVehiclePathNodes(sourceNodeId, destNodeId)`.

**Build sequence:**
1. `placeBuilding` two `Recipe_TruckStation_C` (note their docking-node XY = placement XY).
2. Build a **directed loop** of `constructVehiclePathSegment` calls through both
   docking nodes. Keep every leg on **tree/terrain-free ground** — probe a
   candidate line first (a segment that fails "collides with terrain" or only
   builds on retry is marginal and will be **invalid**). The **return leg must
   not overlap the line the parked truck sits on** (the truck's own body
   invalidates a coincident-direction segment).
3. `mergeVehiclePathNodes` to fold any coincident loop nodes into the two
   station docking nodes so the loop actually reaches the stations.
4. `constructVehicle(recipeClass=Recipe_Truck_C, x,y,z)` on the loop.
5. `setTruckAutopilot(vehicleId, enabled=true, stationIds=[A,B],
   fuelItemClass=Desc_Coal_C, fuelAmount=100)` — this loads fuel, pins the truck
   onto the nearest path segment, sets the first target waypoint, forces path
   validation, and enables autopilot.

**Diagnostics returned by `setTruckAutopilot`:** `onPath`, `hasFuel`,
`canEnableAutopilot`, `currentSegmentValidForPreset`, `validSegmentsForPreset`,
`autopilotError` (None/StationUnreachable/NotOnPath/TooFewStations/NoFuel/
Deadlocked), `shouldTickAutopilot`, `autopilotForwardSpeed`, `vehicleInProxyMode`.
All-green + `StationUnreachable` = the loop has an invalid/broken return leg
(trees/terrain). **Fix = a clean directed loop on flat, clear ground** (or a
raised platform above the bumps), not more RPC calls.

**Telemetry:** `world.vehiclePathNodes` (per node: guid, position, pathNetworkId,
arriving/leaving connection counts) is how you inspect/repair the graph;
`world.vehicles` for truck position.

---

## 3. TRAINS — **WORKS end to end** (self-driving train runs on RPC-built track)

Track, stations, rail vehicles, timetable, self-driving, and power all work, and
a self-driving train **physically circulates on RPC-built track** — confirmed
live 2026-09-08 (loco moving ~1600 u/s at z=5000 on a sky loop, `NoError`). The
drivable joint IS traversable once the track is built with connector snapping +
subsystem re-registration (both happen inside `constructRailroadTrack`). The
earlier "not traversable" reading was a **test-topology** problem, not a joint
problem. Three things MUST be right or the train reports `StationUnreachable`/
`NoPower` and sits still:

1. **Power ≥ one station.** Connect a powered pole to at least one station — rail
   power flows across the whole track graph, so one connection powers the line
   (`selfDrivingError` `NoPower`→`NoError`). Without it the train never departs.
2. **Locomotive must face the arrival direction.** The loco has to be oriented so
   its forward approach matches the route/station docking direction; a loco
   facing the wrong way can't arrive and dock. Orient the loco (or the loop) so
   it drives *into* each station the correct way.
3. **Build a FULL LOOP, not two stations joined by one dead-end track.** Two
   stations with a single connecting segment leaves dead ends the self-driver
   won't reverse through → `StationUnreachable`. Close the circuit (curves back
   around) so the train always has a forward path to the next stop.

**Recipes:** station `Recipe_TrainStation_C`; track `Recipe_RailroadTrack_C`;
loco `Recipe_Locomotive_C`; wagon `Recipe_FreightWagon_C`.

**What works:**
- `placeBuilding` a `Recipe_TrainStation_C` (auto-spawns a
  `RailroadTrackIntegrated` child holding the rail connectors; at yaw 0 the
  integrated track runs along **Y** at x = station_x + 800).
- `constructRailroadTrack(sourceBuildableId, destBuildableId, recipeClass,
  sourceConnectorPosition, destConnectorPosition)` — builds straight + curved
  track. **Pass the station's `RailroadTrackIntegrated` child as source/dest, NOT
  the station actor** — the station itself has no rail connector components
  (passing it gives `NO_RAILROAD_CONNECTION`); the connectors live on the
  integrated-track child (`Build_RailroadTrackIntegrated_C`, at station_x+800 at
  yaw 0). **Pin `source/destConnectorPosition`** to choose which free connector
  each end joins (matters for loops).
- Rail-vehicle placement via `constructVehicle` (snaps to nearest track spline).
- `setTrainTimetable(trainId, stops=[{stationBuildableId, dockingDefinition:
  "LoadUnloadOnce"|"FullyLoadUnload"}])` — **stops key is `stationBuildableId`**.
- `setTrainSelfDriving(trainId, enabled)`; rail **power** (connect a powered pole
  to each station — error goes `NoPower`→`NoError`).

**Train-specific gotchas (opposite of belts in places):**
- **Stations ≥ ~6000u apart** or end curves fail "turns too sharply".
- **Teleport the player AWAY from a curve before building it** — "too steep"/
  "player in the way" on curves is a player-proximity flake. (This is the
  *opposite* of belts, where you teleport the player *near*.)
- The engine `.cpp` for railroad is a shipped stub in the workspace — real logic
  is in the game binary. `constructRailroadTrack` gets a drivable joint by doing
  BOTH: (a) driving the hologram's own connection-snap during placement so
  `ConfigureComponents` wires the joint, AND (b) re-registering the new track
  with `AFGRailroadSubsystem` (`RemoveTrack`→link→`AddTrack`) so it merges into
  the pathfinding graph. Both are needed — snap alone leaves it unreachable,
  re-register alone leaves it non-traversable.
- **Teardown gap:** RPC-built stations/track currently **cannot be dismantled**
  (`deleteBuilding` → `CanDismantle` false); plan loop placement so you don't
  need to remove it, or dismantle in-game.

**BLOCKED / unsolved:** multi-vehicle **coupling** (a station platform track
holds one vehicle; wagons need adjacent plain track + coupling) · **freight-
platform inline-snap** (`Recipe_TrainDockingStation` rejects free placement,
needs a platform-extension snap not yet implemented) · **rail dismantle**
(`CanDismantle` false on RPC-built rail — see teardown gap above).

---

## 4. RPC quick reference

| Purpose | Drones | Trucks | Trains |
|---|---|---|---|
| Build station | `placeBuilding` (Recipe_DroneStation_C) | `placeBuilding` (Recipe_TruckStation_C) | `placeBuilding` (Recipe_TrainStation_C) |
| Build path | — | `constructVehiclePathSegment` | `constructRailroadTrack` (+`testRailroadTrack`) |
| Spawn vehicle | `constructVehicle` (+droneStationId) | `constructVehicle` | `constructVehicle` |
| Pair / route | `pairDroneStations` (both ways) | `setTruckAutopilot` (stationIds) | `setTrainTimetable` |
| Enable driving | (automatic once powered+paired+fueled) | `setTruckAutopilot enabled` | `setTrainSelfDriving` |
| Load cargo/fuel | `addItemsToInventory` (output/input/fuel) | `addItemsToInventory` (fuel) | conveyor/pipe to platform |
| Repair path graph | — | `mergeVehiclePathNodes` | force-link is internal to `constructRailroadTrack` |
| Telemetry | `droneStations`, `creatures` | `vehiclePathNodes`, `vehicles` | `trainStations`, `trains`, `trainCargoPlatforms` |

`addItemsToInventory(buildableId, inventoryRole, itemClass, amount)` seeds a
**buildable** inventory (drone `input`/`output`/`fuel`, truck-station `fuel`,
storage). To add to the **PLAYER** inventory use
`addItemsToPlayerInventory(itemClass, amount)` — so a held-item feature (e.g.
`placePortableMiner` needs a portable-miner item) can now be bootstrapped from
empty. `removeItemsFromInventory` is the buildable-side counterpart, and
`uploadToCentralStorage`/`withdrawFromCentralStorage` move items player↔depot.

---

## 5. Known gaps / where the knowledge lives

- **Mechanisms + rationale:** baked into `AIModFunctionLibrary.cpp` with dated
  "why" comments (search the method names above).
- **Operational procedure:** this doc + `vehicles.py`.
- **Open problems:** truck drive needs a clean tree-free loop; train coupling +
  freight-platform snap unsolved; RPC-built rail can't be dismantled
  (`CanDismantle` false); `setBuildableColor`/`setBuildableRotation` fail on
  lightweight (instanced) buildables like foundations.
- **Solved this session:** a self-driving train runs on RPC-built track (needs
  power + correct loco facing + a full loop — see Trains §3);
  `addItemsToPlayerInventory(itemClass, amount)` now adds to the PLAYER
  inventory, and depot upload/withdraw + storage add/remove are all live-verified.
