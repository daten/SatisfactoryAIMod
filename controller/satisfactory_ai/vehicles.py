"""Vehicle transport composites: drones, trucks/vehicle-paths, trains
(2026-09-07).

Executable counterpart to docs/vehicle-placement-guide.md. Each helper encodes
a PROVEN (or clearly-caveated) live sequence so an agent does not have to
re-discover the nuanced gotchas (power, fuel arming, directional paths, docking
nodes, connector pinning, station spacing, curve player-proximity). This is a
toolkit, not a solver: the caller chooses every position, spacing, and class;
nothing here picks a layout on its own.

Status of each area (see the guide for detail):
  * drones  - WORKS end to end (build_drone_transport).
  * trucks  - infra + autopilot ARM correctly; the truck only physically drives
              on a clean tree/terrain-free directed loop (a SITE constraint).
  * trains  - construction/vehicle/timetable/power WORK; a fully drivable
              RPC-built joint is pending a hologram-connection-snap fix.

All calls go through rpc_client.RpcClient. Power uses executor.Executor
(optional). Nothing here verifies silently - helpers return the ids/telemetry
the caller should check.
"""

from __future__ import annotations

import math
import time
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple

# ---------------------------------------------------------------------------
# Recipes / item classes (live-confirmed paths)
# ---------------------------------------------------------------------------
DRONE_STATION_RECIPE = "/Game/FactoryGame/Recipes/Buildings/Recipe_DroneStation.Recipe_DroneStation_C"
DRONE_RECIPE = "/Game/FactoryGame/Recipes/Buildings/Recipe_DroneTransport.Recipe_DroneTransport_C"
TRUCK_STATION_RECIPE = "/Game/FactoryGame/Recipes/Buildings/Recipe_TruckStation.Recipe_TruckStation_C"
TRUCK_RECIPE = "/Game/FactoryGame/Recipes/Vehicle/Recipe_Truck.Recipe_Truck_C"
TRUCK_PATH_RECIPE = "/Game/FactoryGame/Buildable/Vehicle/Truck/Recipe_VehiclePath_Truck.Recipe_VehiclePath_Truck_C"
TRAIN_STATION_RECIPE = "/Game/FactoryGame/Buildable/Factory/Train/Station/Recipe_TrainStation.Recipe_TrainStation_C"
RAIL_TRACK_RECIPE = "/Game/FactoryGame/Recipes/Buildings/Recipe_RailroadTrack.Recipe_RailroadTrack_C"
LOCOMOTIVE_RECIPE = "/Game/FactoryGame/Recipes/Vehicle/Train/Recipe_Locomotive.Recipe_Locomotive_C"

BATTERY_ITEM = "/Game/FactoryGame/Resource/Parts/Battery/Desc_Battery.Desc_Battery_C"
COAL_ITEM = "/Game/FactoryGame/Resource/RawResources/Coal/Desc_Coal.Desc_Coal_C"

# Placement bypass flags for autonomous (player-independent) builds. See the
# guide §0: always explicit yaw, literal z via ignoreGroundTrace.
_BYP = {
    "ignoreAimLocation": True,
    "ignorePlayerEncroachment": True,
    "ignoreGroundTrace": True,
    "ignoreInvalidFloor": True,
    "ignoreClearance": True,
}

_LONG = 200  # generous timeout for list/telemetry calls


class VehicleBuildError(RuntimeError):
    pass


# ---------------------------------------------------------------------------
# Shared helpers
# ---------------------------------------------------------------------------
def _place(client, recipe: str, x: float, y: float, z: float, yaw: float = 0.0) -> str:
    r = client.call("world.placeBuilding",
                    {"recipeClass": recipe, "x": float(x), "y": float(y), "z": float(z),
                     "yaw": float(yaw), **_BYP}, timeout_seconds=90)
    bid = r.get("buildableId")
    if not bid:
        raise VehicleBuildError(f"placeBuilding({recipe}) returned no buildableId: {r}")
    return bid


def _near_buildable(client, class_substr: str, cx: float, cy: float, radius: float = 1600.0) -> Optional[dict]:
    for b in client.call("world.buildables", {}, timeout_seconds=_LONG)["buildables"]:
        p = b.get("position")
        if p and class_substr in b["buildableClass"] and abs(p["x"] - cx) < radius and abs(p["y"] - cy) < radius:
            return b
    return None


def add_inventory(client, buildable_id: str, item_class: str, amount: int, role: str = "input") -> dict:
    """Seed a buildable inventory. role: drone station "input"|"output"|"fuel",
    truck station "fuel", storage "store"/"auto". For a drone/truck station
    "fuel" role this ALSO arms the station's active fuel type (a raw add does
    not) - required or the vehicle never departs. Returns the RPC detail."""
    r = client.call("world.addItemsToInventory",
                    {"buildableId": buildable_id, "inventoryRole": role,
                     "itemClass": item_class, "amount": int(amount)}, timeout_seconds=90)
    return r.get("detail", r)


# ---------------------------------------------------------------------------
# DRONES - WORKS end to end
# ---------------------------------------------------------------------------
@dataclass
class DroneRoute:
    station_a_id: str
    station_b_id: str
    drone_spawned: bool
    detail: Dict = field(default_factory=dict)


def build_drone_transport(client, a_xy: Tuple[float, float], b_xy: Tuple[float, float], z: float,
                          cargo_item: str, cargo_amount: int,
                          fuel_item: str = BATTERY_ITEM, fuel_amount: int = 100,
                          executor=None, power_source_id: Optional[str] = None,
                          teleport: bool = True) -> DroneRoute:
    """Build a working drone route A->B: two stations, MUTUAL pairing, a drone
    at A, cargo into A's OUTPUT (send buffer) and fuel into both stations.

    REQUIRES POWER: drone stations are powered factory buildings; without power
    the drone stays Docked forever (this looked like a "frozen" drone for a long
    time - it was just unpowered). Pass an `executor` + `power_source_id` (a
    powered pole/tower) to wire power to both stations, or power them yourself
    before/after and ignore those args.

    Cargo model (easy to get backwards): a station's OUTPUT is what the drone
    LOADS to carry away; INPUT is where the paired drone DROPS OFF. So cargo to
    send from A goes in A.output; it arrives in B.input.
    """
    from .models import Position  # local import to avoid hard dep at module load

    ax, ay = a_xy
    bx, by = b_xy
    if teleport:
        client.call("world.teleportPlayer", {"x": float((ax + bx) / 2), "y": float(ay), "z": float(z + 1500),
                                             "ignoreGroundTrace": True}, timeout_seconds=30)
        time.sleep(0.3)

    a_id = _place(client, DRONE_STATION_RECIPE, ax, ay, z, 0.0)
    b_id = _place(client, DRONE_STATION_RECIPE, bx, by, z, 180.0)
    time.sleep(0.5)

    # Power both stations (required). Best-effort; caller can also do it.
    if executor is not None and power_source_id is not None:
        for sid, sx, sy in ((a_id, ax, ay), (b_id, bx, by)):
            executor.connect_power(power_source_id, sid, near=Position(x=(sx + ax) / 2, y=sy, z=z + 400))

    # MUTUAL pairing - one-way leaves the far station unpaired and the route
    # never completes.
    client.call("world.pairDroneStations", {"stationBuildableId": a_id, "targetStationBuildableId": b_id}, timeout_seconds=60)
    client.call("world.pairDroneStations", {"stationBuildableId": b_id, "targetStationBuildableId": a_id}, timeout_seconds=60)
    time.sleep(0.4)

    # Spawn the drone at A (RPC snaps it to the station dock).
    spawned = True
    try:
        client.call("world.constructVehicle",
                    {"recipeClass": DRONE_RECIPE, "droneStationId": a_id,
                     "x": float(ax), "y": float(ay), "z": float(z + 130), **{"ignoreGroundTrace": True}},
                    timeout_seconds=90)
    except Exception:
        spawned = False  # id-based confirm is cosmetic; station telemetry is truth

    # Cargo -> A.output (send buffer); fuel -> both (arms activeFuelType).
    add_inventory(client, a_id, cargo_item, cargo_amount, role="output")
    fa = add_inventory(client, a_id, fuel_item, fuel_amount, role="fuel")
    fb = add_inventory(client, b_id, fuel_item, fuel_amount, role="fuel")
    return DroneRoute(a_id, b_id, spawned, {"fuelA": fa, "fuelB": fb})


def drone_status(client, station_ids: List[str]) -> List[dict]:
    """Return per-station status (droneStatus, activeFuelType, inventories) for
    the given stations - use to confirm the drone is cycling."""
    out = []
    for s in client.call("world.droneStations", {}, timeout_seconds=90).get("droneStations", []):
        if s["id"] in station_ids:
            out.append(s)
    return out


# ---------------------------------------------------------------------------
# TRUCKS / VEHICLE PATHS - infra + autopilot ARM; driving needs a clean loop
# ---------------------------------------------------------------------------
def build_vehicle_path_segment(client, sx, sy, sz, ex, ey, ez, recipe: str = TRUCK_PATH_RECIPE) -> None:
    """One directed path segment start->end. Endpoints on a docking node's XY
    usually snap onto it. NB: success here does NOT mean the segment is
    traversable - a segment that clips terrain/trees builds but is invalid for
    the vehicle preset (probe candidate lines first; avoid the parked truck's
    own line for the return leg)."""
    client.call("world.constructVehiclePathSegment",
                {"recipeClass": recipe, "startX": float(sx), "startY": float(sy), "startZ": float(sz),
                 "endX": float(ex), "endY": float(ey), "endZ": float(ez), "ignoreGroundTrace": True},
                timeout_seconds=120)


def merge_path_nodes(client, source_node_id: str, dest_node_id: str) -> dict:
    """Fold a coincident default path node into a station docking node so a
    hand-built loop actually reaches the station (world.mergeVehiclePathNodes)."""
    return client.call("world.mergeVehiclePathNodes",
                       {"sourceNodeId": source_node_id, "destNodeId": dest_node_id},
                       timeout_seconds=60).get("detail", {})


def arm_truck_autopilot(client, truck_id: str, station_ids: List[str],
                        fuel_item: str = COAL_ITEM, fuel_amount: int = 100) -> dict:
    """Enable a truck's autopilot with a station route. Loads fuel, pins the
    truck onto the nearest path segment, sets the first target waypoint, forces
    path validation, and enables autopilot. Returns rich diagnostics
    (onPath/hasFuel/currentSegmentValidForPreset/validSegmentsForPreset/
    autopilotError/shouldTickAutopilot/...). All-green + StationUnreachable ==
    the loop's return leg is invalid (trees/terrain) - fix the SITE, not the RPC."""
    return client.call("world.setTruckAutopilot",
                       {"vehicleId": truck_id, "enabled": True, "stationIds": station_ids,
                        "fuelItemClass": fuel_item, "fuelAmount": int(fuel_amount)},
                       timeout_seconds=90).get("detail", {})


def build_truck_stations(client, a_xy, b_xy, z, teleport: bool = True) -> Tuple[str, str]:
    """Place two truck stations; each auto-spawns a docking node at its
    placement XY (that XY is where the path must reach to dock)."""
    ax, ay = a_xy
    bx, by = b_xy
    if teleport:
        client.call("world.teleportPlayer", {"x": float((ax + bx) / 2), "y": float(ay - 1500), "z": float(z + 1200),
                                             "ignoreGroundTrace": True}, timeout_seconds=30)
        time.sleep(0.3)
    a_id = _place(client, TRUCK_STATION_RECIPE, ax, ay, z, 0.0)
    b_id = _place(client, TRUCK_STATION_RECIPE, bx, by, z, 180.0)
    return a_id, b_id


# ---------------------------------------------------------------------------
# TRAINS - construction WORKS; drivable joint pending
# ---------------------------------------------------------------------------
def construct_rail_link(client, source_station_id: str, dest_station_id: str,
                        src_connector_pos: Optional[Tuple[float, float, float]] = None,
                        dst_connector_pos: Optional[Tuple[float, float, float]] = None,
                        recipe: str = RAIL_TRACK_RECIPE) -> dict:
    """Build track between two rail buildables. PIN src/dst connector positions
    to choose which free connector each end joins (essential for loops).
    Reliability notes (see guide §3): stations >= ~6000u apart for end curves;
    teleport the player AWAY from a curve before building (proximity flake,
    opposite of belts). NB: a fully drivable RPC-built joint is pending a
    hologram-connection-snap fix - the track builds and graph-merges, but verify
    selfDrivingError clears past StationUnreachable before trusting it."""
    params = {"sourceBuildableId": source_station_id, "destBuildableId": dest_station_id, "recipeClass": recipe}
    if src_connector_pos:
        params["sourceConnectorPosition"] = {"x": src_connector_pos[0], "y": src_connector_pos[1], "z": src_connector_pos[2]}
    if dst_connector_pos:
        params["destConnectorPosition"] = {"x": dst_connector_pos[0], "y": dst_connector_pos[1], "z": dst_connector_pos[2]}
    return client.call("world.constructRailroadTrack", params, timeout_seconds=120)


def set_train_route(client, train_id: str, station_ids: List[str],
                    docking: str = "LoadUnloadOnce", self_driving: bool = True) -> dict:
    """Timetable a train across stations then enable self-driving. NB: the stop
    key is stationBuildableId. Untestable end-to-end until the drivable-joint
    fix lands."""
    stops = [{"stationBuildableId": sid, "dockingDefinition": docking} for sid in station_ids]
    tt = client.call("world.setTrainTimetable", {"trainId": train_id, "stops": stops}, timeout_seconds=60)
    sd = client.call("world.setTrainSelfDriving", {"trainId": train_id, "enabled": self_driving}, timeout_seconds=60)
    return {"timetable": tt.get("detail", tt), "selfDriving": sd.get("detail", sd)}
