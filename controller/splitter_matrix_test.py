"""Splitter port control test — Phases 6-9 (2026-08-30).

Live-constructs and verifies the exhaustive splitter-to-splitter cardinal
matrix from the user's "Splitter-to-Splitter Conveyor Control Test"
spec (docs/splitter-port-control-test.md tracks results; the full
original spec text lives in this project's session history). Phases
1-5 already passed against that doc before this script existed.

This script builds Phase 6/7 (12 cases: 4 source rotations x 3 outputs,
destination rotation fixed) and, with --dest-yaws 0,90,180,270, extends
to the full Phase 8 matrix (48 cases: 4 source rotations x 3 outputs x
4 destination rotations). Phase 9 (programmatic post-connection
verification — never trust connectConveyor's success:true alone) is
built into every case: after each belt, this re-fetches world.connections
and confirms the EXACT requested source-output and destination-input
connectors (matched by real position, via the same
satisfactory_ai.splitters toolkit the port-selection logic itself
uses) are the ones actually wired — not just "some output of S1 connects
to some input of S2".

**Placement geometry, empirically derived this session (2026-08-30),
not guessed**: an early attempt placing the destination splitter at a
small fixed offset regardless of direction failed repeatedly with
"Conveyor Belt is too long!" even at distances far under the real
5600cm max spline length (world.conveyorBeltTiers) - probed directly
and found the auto-routed path for an unfavorably-angled connector pair
(e.g. source output facing one way, destination input facing a
perpendicular/opposite way) needs real physical room to loop around,
well beyond the straight-line distance. Fix: place the destination
splitter offset from the source in the direction of the SOURCE's own
selected output normal (live-read from world.connections, not
assumed) - this always gives the belt a clean straight departure from
the source, which was sufficient (at 3000cm+) even for the worst
direction combination tested live. Destination-yaw variation (the
Phase 8 dimension) is placed on separate Z floors rather than spread
further out horizontally, specifically to avoid the corresponding
"how far apart is far enough" search a second time for a second axis -
each floor's cells only need clearance from their own floor's
neighbors, not from a whole second horizontal grid.

Deliberately builds a REAL, PERSISTENT physical test range — cells are
NOT cleaned up after testing, per the spec's own requirement that every
test case remain separately visible in-game.

Requires a running game with AIMod's HTTP server up (same requirement as
live_check.py). Not part of `python -m unittest discover` for the same
reason live_check.py isn't — a live diagnostic/construction script, not
a repeatable dependency-free test.

    python controller/splitter_matrix_test.py --dest-yaws 0
    python controller/splitter_matrix_test.py --dest-yaws 0,90,180,270
"""

from __future__ import annotations

import argparse
import json
import sys
import time
import urllib.error
import urllib.request
from dataclasses import asdict, dataclass
from typing import Any, List, Optional

from satisfactory_ai.models import FactoryConnection
from satisfactory_ai.splitters import (
    CARDINAL_DIRECTIONS,
    PortSelectionError,
    classify_ports,
    get_splitter_input,
    get_splitter_output_facing,
)

DEFAULT_URL = "http://127.0.0.1:51902/rpc"

FOUNDATION_RECIPE = "/Game/FactoryGame/Recipes/Buildings/Foundations/Recipe_Foundation_8x1_01.Recipe_Foundation_8x1_01_C"
SPLITTER_RECIPE = "/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorAttachmentSplitter.Recipe_ConveyorAttachmentSplitter_C"
SOURCE_YAWS = (0, 90, 180, 270)

# Elevated well above any nearby terrain (confirmed max ~9700 in this
# general area this session) so ground unevenness never enters into
# belt-routing disqualifiers. Location deliberately away from every
# other test/probe site used this session.
X_BASE = -60000.0
Y_BASE = 100000.0
Z_BASE = 13000.0

ROW_SPACING = 9000.0   # along X, one per source case (12: 4 yaws x 3 outputs)
FLOOR_SPACING = 4000.0  # along Z, one per destination yaw (up to 4)
OUTPUT_OFFSET = 5000.0  # S1 -> S2 distance, along the selected output's own real normal direction

# OUTPUT_OFFSET raised from 3500 to 5000 (2026-08-30, live-confirmed):
# with the poll-tick hologram-placement fix applied, 3500cm passed 9/12
# cases but failed the 3 where the source's selected output direction
# happened to be exactly opposite the destination's fixed input
# direction (a real U-turn geometry, not a bug) - "Invalid aim
# location!"/"Invalid Conveyor Belt shape!". Bumped to 5000cm (still
# under Mk1's real 5600cm max spline length), confirmed live to resolve
# that exact case cleanly.

CARDINAL_OFFSET = {
    "NORTH": (0.0, 1.0),
    "SOUTH": (0.0, -1.0),
    "EAST": (1.0, 0.0),
    "WEST": (-1.0, 0.0),
}


def rpc_call(url: str, method: str, params: Optional[dict] = None, request_id: str = "splitter-matrix", timeout: float = 30.0) -> dict:
    payload: dict[str, Any] = {"protocolVersion": 1, "requestId": request_id, "method": method}
    if params:
        payload["params"] = params
    data = json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(url, data=data, headers={"Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            return json.loads(resp.read().decode("utf-8"))
    except urllib.error.HTTPError as e:
        body = e.read().decode("utf-8")
        try:
            return json.loads(body)
        except Exception:
            return {"success": False, "httpError": e.code, "body": body}
    except Exception as e:
        return {"success": False, "transportError": str(e)}


@dataclass
class CaseResult:
    test_id: str
    source_yaw: int
    output_ordinal: int
    dest_yaw: int
    source_output_direction: str = "?"
    dest_input_direction: str = "?"
    source_id: str = ""
    dest_id: str = ""
    belt_success: Optional[bool] = None
    belt_error: Any = None
    source_port_verified: Optional[bool] = None
    dest_port_verified: Optional[bool] = None
    result: str = "NOT_RUN"
    detail: str = ""
    attempts: int = 0


def fetch_connections(url: str) -> List[FactoryConnection]:
    r = rpc_call(url, "world.connections")
    result = r.get("result")
    raw = result.get("connections", result) if isinstance(result, dict) else result
    return [FactoryConnection.from_dict(c) for c in raw]


def place_foundation(url: str, x: float, y: float, z: float) -> dict:
    return rpc_call(url, "world.placeBuilding", {
        "recipeClass": FOUNDATION_RECIPE, "x": x, "y": y, "z": z,
        "gridSnapSize": 0, "yaw": 0, "ignoreGroundTrace": True,
        "ignoreAimLocation": True, "ignorePlayerEncroachment": True,
    })


def place_splitter(url: str, x: float, y: float, z: float, yaw: int) -> dict:
    return rpc_call(url, "world.placeBuilding", {
        "recipeClass": SPLITTER_RECIPE, "x": x, "y": y, "z": z,
        "gridSnapSize": 0, "yaw": yaw, "ignoreGroundTrace": True,
        "ignoreAimLocation": True, "ignorePlayerEncroachment": True, "ignoreInvalidFloor": True,
    })


def cleanup_cell(url: str, x: float, y: float, z: float, radius: float = 400.0) -> None:
    r = rpc_call(url, "world.buildables")
    bs = r.get("result")
    if isinstance(bs, dict):
        bs = bs.get("buildables", bs)
    if not bs:
        return
    for b in bs:
        p = b["position"]
        if abs(p["x"] - x) < radius and abs(p["y"] - y) < radius and abs(p["z"] - z) < radius:
            rpc_call(url, "world.deleteBuilding", {"buildableId": b["id"]})


def _attempt_case(url: str, source_yaw: int, output_ordinal: int, dest_yaw: int,
                   s1_x: float, s1_y: float, s1_z: float) -> CaseResult:
    result = CaseResult(test_id="", source_yaw=source_yaw, output_ordinal=output_ordinal, dest_yaw=dest_yaw)

    place_foundation(url, s1_x, s1_y, s1_z - 50)
    r1 = place_splitter(url, s1_x, s1_y, s1_z, source_yaw)
    if not r1.get("success"):
        result.result = "FAIL_PLACEMENT"
        result.detail = f"source placement failed: {r1.get('error')}"
        return result
    s1_id = r1["result"]["buildableId"]
    result.source_id = s1_id

    time.sleep(0.5)
    conns = fetch_connections(url)
    s1_ports = classify_ports(conns, s1_id)
    try:
        s1_outputs_sorted = sorted(
            (p for p in s1_ports if p.connection_type == "Output"),
            key=lambda p: CARDINAL_DIRECTIONS.index(p.cardinal),
        )
        if len(s1_outputs_sorted) != 3:
            raise PortSelectionError("WRONG_OUTPUT_COUNT", f"expected 3 outputs, found {len(s1_outputs_sorted)}")
        source_port = get_splitter_output_facing(s1_ports, s1_outputs_sorted[output_ordinal].cardinal)
    except PortSelectionError as e:
        result.result = "FAIL_PORT_CLASSIFY"
        result.detail = f"{e.code}: {e}"
        return result
    result.source_output_direction = source_port.cardinal

    dx, dy = CARDINAL_OFFSET[source_port.cardinal]
    s2_x, s2_y, s2_z = s1_x + dx * OUTPUT_OFFSET, s1_y + dy * OUTPUT_OFFSET, s1_z

    place_foundation(url, s2_x, s2_y, s2_z - 50)
    r2 = place_splitter(url, s2_x, s2_y, s2_z, dest_yaw)
    if not r2.get("success"):
        result.result = "FAIL_PLACEMENT"
        result.detail = f"dest placement failed: {r2.get('error')}"
        return result
    s2_id = r2["result"]["buildableId"]
    result.dest_id = s2_id

    time.sleep(0.5)
    conns2 = fetch_connections(url)
    s2_ports = classify_ports(conns2, s2_id)
    try:
        dest_port = get_splitter_input(s2_ports)
    except PortSelectionError as e:
        result.result = "FAIL_PORT_CLASSIFY"
        result.detail = f"{e.code}: {e}"
        return result
    result.dest_input_direction = dest_port.cardinal

    belt = rpc_call(url, "world.connectConveyor", {
        "sourceBuildableId": s1_id, "destBuildableId": s2_id,
        "sourceConnectorPosition": {"x": source_port.raw.position.x, "y": source_port.raw.position.y, "z": source_port.raw.position.z},
        "destConnectorPosition": {"x": dest_port.raw.position.x, "y": dest_port.raw.position.y, "z": dest_port.raw.position.z},
        "routeMode": "Auto", "instigatorStrategy": "RealCharacter",
    })
    result.belt_success = bool(belt.get("success"))
    result.belt_error = belt.get("error")

    if not result.belt_success:
        result.result = "FAIL_CONSTRUCT"
        result.detail = f"connectConveyor failed: {belt.get('error')}"
        return result

    time.sleep(0.7)
    conns_after = fetch_connections(url)
    s1_ports_after = classify_ports(conns_after, s1_id)
    s2_ports_after = classify_ports(conns_after, s2_id)

    def find_by_position(ports, target_pos, tolerance=15.0):
        for p in ports:
            d = ((p.raw.position.x - target_pos.x) ** 2 + (p.raw.position.y - target_pos.y) ** 2 + (p.raw.position.z - target_pos.z) ** 2) ** 0.5
            if d <= tolerance:
                return p
        return None

    source_after = find_by_position(s1_ports_after, source_port.raw.position)
    dest_after = find_by_position(s2_ports_after, dest_port.raw.position)

    # A connector's connected_buildable_id points at the BELT buildable
    # sitting between the two splitters, not at the far splitter
    # directly - so "this exact output connects to this exact input" is
    # verified by both connectors pointing at the SAME belt, not by
    # either one's id matching s1_id/s2_id.
    result.source_port_verified = bool(source_after and source_after.connected)
    result.dest_port_verified = bool(dest_after and dest_after.connected)
    same_belt = bool(
        source_after and dest_after
        and source_after.connected_buildable_id
        and source_after.connected_buildable_id == dest_after.connected_buildable_id
    )
    result.source_port_verified = result.source_port_verified and same_belt
    result.dest_port_verified = result.dest_port_verified and same_belt

    if result.source_port_verified and result.dest_port_verified:
        result.result = "PASS"
    else:
        result.result = "FAIL_VERIFY"
        result.detail = (
            f"source_after={'found' if source_after else 'MISSING'} "
            f"connected={source_after.connected if source_after else None} "
            f"connectedTo={source_after.connected_buildable_id if source_after else None}; "
            f"dest_after={'found' if dest_after else 'MISSING'} "
            f"connected={dest_after.connected if dest_after else None} "
            f"connectedTo={dest_after.connected_buildable_id if dest_after else None}"
        )
    return result


def run_case(url: str, test_id: str, source_yaw: int, output_ordinal: int, dest_yaw: int,
             row: int, floor: int, max_attempts: int = 2) -> CaseResult:
    """Builds and verifies exactly one matrix cell, retrying once on a
    transient placement/construct failure (this project's established
    "confirmed-transient disqualifier flakiness" pattern - see
    docs/placement-lessons.md) after cleaning up whatever the failed
    attempt left behind, before giving up and reporting a real failure.
    """
    s1_x = X_BASE + row * ROW_SPACING
    s1_y = Y_BASE
    s1_z = Z_BASE + floor * FLOOR_SPACING

    last: Optional[CaseResult] = None
    for attempt in range(1, max_attempts + 1):
        result = _attempt_case(url, source_yaw, output_ordinal, dest_yaw, s1_x, s1_y, s1_z)
        result.test_id = test_id
        result.attempts = attempt
        last = result
        if result.result == "PASS":
            return result
        if attempt < max_attempts:
            cleanup_cell(url, s1_x, s1_y, s1_z, radius=OUTPUT_OFFSET + 500)
            time.sleep(1.5)
    return last


def run_matrix(url: str, dest_yaws: List[int]) -> List[CaseResult]:
    """floor is the dest_yaw's index in the canonical SOURCE_YAWS order
    (not its position in the possibly-partial `dest_yaws` list) so a
    partial run (e.g. just the 3 remaining yaws after dest_yaws=0 was
    already built and verified) lands on its own dedicated floor slot
    instead of colliding with cells built by an earlier, different
    --dest-yaws invocation.
    """
    results: List[CaseResult] = []
    row = 0
    for source_yaw in SOURCE_YAWS:
        for output_ordinal in range(3):
            for dest_yaw in dest_yaws:
                floor = SOURCE_YAWS.index(dest_yaw)
                test_id = f"SPLIT_TEST_{row:02d}{chr(ord('A') + floor)}"
                print(f"[{test_id}] source_yaw={source_yaw} output_ordinal={output_ordinal} dest_yaw={dest_yaw} ...", flush=True)
                result = run_case(url, test_id, source_yaw, output_ordinal, dest_yaw, row, floor)
                print(f"  -> {result.result} (source={result.source_output_direction} dest={result.dest_input_direction}, attempts={result.attempts}) {result.detail}", flush=True)
                results.append(result)
            row += 1
    return results


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", default=DEFAULT_URL)
    parser.add_argument("--dest-yaws", default="0", help="comma-separated destination yaws to test, e.g. 0 or 0,90,180,270")
    parser.add_argument("--output", default="splitter_matrix_results.json")
    args = parser.parse_args()

    dest_yaws = [int(x.strip()) for x in args.dest_yaws.split(",")]
    for dy in dest_yaws:
        if dy not in SOURCE_YAWS:
            print(f"invalid dest yaw {dy}, must be one of {SOURCE_YAWS}", file=sys.stderr)
            return 2

    results = run_matrix(args.url, dest_yaws)

    with open(args.output, "w", encoding="utf-8") as f:
        json.dump([asdict(r) for r in results], f, indent=2)

    n_pass = sum(1 for r in results if r.result == "PASS")
    print(f"\n=== {n_pass}/{len(results)} cases PASS ===")
    for r in results:
        if r.result != "PASS":
            print(f"  {r.test_id}: {r.result} - {r.detail}")

    return 0 if n_pass == len(results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
