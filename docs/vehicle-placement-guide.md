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

## 3. TRAINS — a pure-RPC autonomous loop **WORKS** (verified 2026-09-18)

A self-driving train circulates a fully RPC-built circular loop, no in-game
touch. **The long "pure-RPC joint isn't drivable / StationUnreachable" saga
(2026-09-06 … 2026-09-09 in the git/memory history) was a SETUP problem, not a
code wall.** The joints from `constructRailroadTrack` are traversable. The two
things that were actually wrong every time:

1. **Power.** The first `selfDrivingError` on an unpowered loop is **`NoPower`**
   — easy to misread as the joint failing. Power one station and the whole rail
   graph is powered.
2. **Station orientation.** Stations are **directional**; if they don't all face
   the circulation direction consistently the loop splits and the train reports
   `StationUnreachable` and sits still. This is the crux — see the rule below.

Prerequisite already in the mod: the **rotation fix (commit a9ec3451e0)** makes a
station's integrated track inherit its placement yaw. Without it all connectors
point ±X and you can't orient a curve.

### Reliable build recipe (clockwise circle of radius R)

**Recipes:** station `/Game/…/Train/Station/Recipe_TrainStation.Recipe_TrainStation_C`;
track `/Game/…/Buildings/Recipe_RailroadTrack.Recipe_RailroadTrack_C`;
loco `/Game/…/Vehicle/Train/Recipe_Locomotive.Recipe_Locomotive_C`.

1. **Pick R ≥ ~7000** (center C, z on/over a surface). Adjacent anchors must be
   ≥~6000u apart or curves fail "turns too sharply"; R=7000 gives connector
   chords ~6300u. A bigger loop (R≥9000–10000) is needed if you later want
   freight platforms extending off each station.
2. **Place 4 stations** at C±(R,0) and C±(0,R) with `placeBuilding`
   (`ignoreGroundTrace/ignoreClearance/ignoreInvalidFloor/ignorePlayerEncroachment`).
   **The station yaw sets its arrow = its local +X axis in world:**
   `yaw 0→+X (East)`, `90→+Y (North)`, `180→-X (West)`, `270→-Y (South)`.
   Every station's arrow must point along the travel tangent. **For a CLOCKWISE
   loop (N→E→S→W): `N=0, E=270, S=180, W=90`.** (N/S tracks run along X, E/W
   along Y.) Getting two stations facing each way — e.g. `N=0,S=0` — splits the
   loop 2-and-2 and it will NOT circulate. If a station faces backwards, flip it
   180°. The player can verify arrows in-game; trust that over the math.
3. **Find each station's `RailroadTrackIntegrated` child** (a separate buildable
   ~765u offset from the station, class `Build_RailroadTrackIntegrated_C`; match
   the nearest one to each anchor via `world.buildables`). **Arcs connect the
   integrated-track children, NOT the station actors** (passing the station gives
   `NO_RAILROAD_CONNECTION`).
4. **Build 4 quarter-arcs** with `constructRailroadTrack(sourceBuildableId,
   destBuildableId, recipeClass, sourceConnectorPosition, destConnectorPosition)`.
   Each arc joins one integ's *facing* connector to the next. **Pin each
   connector ~2500u past the integ center along its axis** toward the target —
   nearest-free-connector then picks the correct end. Order: NE (N.east↔E.north),
   SE (E.south↔S.east), SW (S.west↔W.south), NW (W.north↔N.west). A single call
   can't make a 180° arc; that's why it's 4 quarters. Verify all 4 stations now
   report the SAME `trackGraphId` (`world.trainStations`) — that's the closed
   loop.
5. **Power it.** Wire the stations together and connect ONE to a powered source:
   `connectPower(stationA, poleWithPower, ignoreAimLocation, ignoreWireSnap,
   ignoreWireLength)`. Find a powered pole via `world.powerPoles` (has `hasPower`
   + free conns but NO position — cross-ref `world.buildables` for positions).
   A local Biomass generator is NOT a fallback: `addItemsToInventory` cannot fuel
   a generator (`itemsAdded:0`), so tap the base grid (a single loco's load is
   fine; the fuse held). Power lines have no real length limit with
   `ignoreWireLength`.
6. **Spawn the loco** with `constructVehicle(recipeClass=Locomotive, x,y,z+50,
   ignoreGroundTrace)` on the loop (near a station's integ works; it snaps to the
   nearest spline). **The train id ≠ the loco id** — a `BP_Train_C` is created
   alongside the `BP_Locomotive_C` (id number ≈ loco id − 3). Use
   `world.trains` to find it; identify yours by proximity of the trailing id
   number and `hasTimeTable:false`. **Never set a timetable on a train you didn't
   just spawn — the base has its own trains.**
7. **Route it:** `setTrainTimetable(trainId, stops=[{stationBuildableId,
   dockingDefinition:"LoadUnloadOnce"}] for each station in travel order)` then
   `setTrainSelfDriving(trainId, enabled=true)`. **stops key is
   `stationBuildableId`.**
8. **Verify** via `world.trains` `selfDrivingError` (expect `NoError`) and
   `world.vehicles` loco position over time — it should sweep through all four
   quadrants (compute `atan2(y-Cy, x-Cx)`) and lap continuously (~2700 u/s seen).

### Gotchas
- **Teleport the player AWAY from curves before building** — "too steep"/"player
  in the way" on a curve is a proximity flake (the *opposite* of belts). Rail
  builds fine with the player far off; no teleport to the site needed.
- **Dismantle:** RPC-built stations/track delete with `deleteBuilding`, EXCEPT
  while a train is docked/self-driving (fails "un-dismantled parent"). Cleanup:
  `setTrainSelfDriving(false)` → delete loco → delete stations (integrated track
  cascades) → delete arc `RailroadTrack` pieces.

**STILL BLOCKED — freight-platform attach (needs a snap-based construct, NOT a
bypass).** `placeBuilding` of `Recipe_TrainDockingStation` fails hard **"This
must be placed inline with another train platform!"**, and a disqualifier-bypass
is the WRONG fix: the game code shows freight platforms are **snap-to-connection
buildings, not free placements** —
`AFGBuildableTrainPlatform` has `mPlatformConnection0/1` (`UFGTrainPlatformConnection`,
each tied to a `UFGRailroadTrackConnectionComponent`) + its own `mRailroadTrack`,
and `AFGTrainPlatformHologram` has `mRequireSnapToPlatform`, `TrySnapToActor()`,
`SnapToConnection(UFGTrainPlatformConnection*)`, `FindOverlappingConnectionComponent()`
and owns a **child rail-track hologram** (`mRailroadTrackHologram`). Force-placing
past the disqualifier would leave `mConnectedPlatformComponents` + the child track
UNLINKED → a dead, non-loading platform (same class of bug as the old force-linked
rail joints). The RIGHT fix is a dedicated construct that **mirrors
`constructRailroadTrack`**: spawn `AFGTrainPlatformHologram`, position it at the
target station/platform's platform connection, drive `TrySnapToActor` /
`SnapToConnection` so `mConnectedPlatformComponents` + the child track link, then
`Construct()`. Reference geometry (from the base): platforms sit **1600u apart
along the track axis**, platform yaw = `station_yaw + 180`. Until that construct
exists, validate station direction by the arrows / the train circulating and add
freight platforms in-game. Also unsolved: multi-vehicle **coupling** (a station
platform holds one vehicle).

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
- **Open problems:** **pure-RPC rail joint not drivable** (main train wall — see
  Trains §3; needs the `PrimaryFire` build-path spike); truck drive needs a clean
  tree-free loop; train coupling + freight-platform snap unsolved;
  `setBuildableColor`/`setBuildableRotation` fail on lightweight (instanced)
  buildables like foundations.
- **Fixed this session:** station placement yaw now propagates to the integrated
  track (rotated stations give correctly-oriented rail connectors → clean 90°
  curves buildable); `addItemsToPlayerInventory(itemClass, amount)` adds to the
  PLAYER inventory; depot upload/withdraw + storage add/remove live-verified.
  Note: a self-driving train runs on RPC track only after a human makes one
  in-game connection to repair the joints — pure-RPC is not drivable yet.
