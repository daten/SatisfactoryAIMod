"""Waypoint belt-lane planner — added 2026-09-09.

Turns a CHOSEN path (an ordered list of waypoints, like a trace on a circuit
board) into a concrete build plan: a conveyor pole at each interior vertex and
SHORT, STRAIGHT belt segments between consecutive anchors. Short spans (kept
below the shape-constraint range) make the game's auto-routed spline behave as a
predictable straight line, which is what gives deterministic, non-overlapping,
circuit-board-style routing instead of the long-span "chaotic but functional"
belts. Long spans the agent draws are auto-subdivided with extra collinear poles.

Toolkit, not a router (per [[feedback_dont_prebake_agent_decisions]]): the AGENT
supplies the waypoint path (where the trace goes, which z-lane, how to avoid
obstacles); this tool only realizes that path as poles + straight belts and
hands back RouteSegments for route_drc to verify. It does not search for a path
or choose the lane. Output is a router.RoutePlan (place + belt ops) that the
existing executor runs, plus route_drc.RouteSegment list for the DRC.

CALIBRATION CAVEAT: a Conveyor Pole's belt-connector sits at an offset from its
placement point that this project has not yet measured precisely (a live-seed
TODO, like connector_db's other profiles). By default the pole is placed AT the
waypoint and the belt endpoint is pinned to the waypoint; pass pole_base_offset
to shift the pole base below the connector once that offset is known.
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field
from typing import List, Optional, Sequence

from .models import Position
from .route_drc import DEFAULT_BELT_CLEARANCE, RouteSegment
from .router import (
    MAX_PLANNED_INCLINE_DEGREES,
    MIN_RELIABLE_RUN,
    Endpoint,
    RouteOp,
    RoutePlan,
    RoutingError,
    _dist2d,
    _incline_degrees,
)

CONVEYOR_POLE_RECIPE = "/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorPole.Recipe_ConveyorPole_C"
# Keep each straight belt span below the ~1500u shape-constraint range (router
# SHAPE_CONSTRAINT_RANGE) so the spline stays essentially straight and the built
# geometry matches the planned segment - the crux of deterministic routing.
MAX_STRAIGHT_RUN = 1400.0


@dataclass
class BeltLanePlan:
    plan: RoutePlan                       # place-pole + belt ops for the executor
    segments: List[RouteSegment]          # straight spans, for route_drc.check_route
    warnings: List[str] = field(default_factory=list)


def _dist3d(a: Position, b: Position) -> float:
    return math.sqrt((a.x - b.x) ** 2 + (a.y - b.y) ** 2 + (a.z - b.z) ** 2)


def _lerp(a: Position, b: Position, t: float) -> Position:
    return Position(x=a.x + (b.x - a.x) * t, y=a.y + (b.y - a.y) * t, z=a.z + (b.z - a.z) * t)


def _yaw_toward(a: Position, b: Position) -> float:
    """Yaw (deg) of the horizontal direction a->b, 0 = +X."""
    return math.degrees(math.atan2(b.y - a.y, b.x - a.x))


def subdivide_path(anchors: Sequence[Position], max_run: float = MAX_STRAIGHT_RUN) -> List[Position]:
    """Insert evenly-spaced COLLINEAR points so every consecutive gap <= max_run.
    Endpoints and the caller's waypoints are preserved exactly; only extra
    straight-line supports are added on long spans."""
    if len(anchors) < 2:
        return list(anchors)
    out: List[Position] = [anchors[0]]
    for i in range(1, len(anchors)):
        a, b = anchors[i - 1], anchors[i]
        d = _dist3d(a, b)
        n = max(1, math.ceil(d / max_run - 1e-9)) if d > 0 else 1
        for k in range(1, n):
            out.append(_lerp(a, b, k / n))
        out.append(b)
    return out


def plan_belt_lane(source: Endpoint, dest: Endpoint, waypoints: Sequence[Position], *,
                   pole_recipe: str = CONVEYOR_POLE_RECIPE,
                   clearance: float = DEFAULT_BELT_CLEARANCE,
                   max_straight: float = MAX_STRAIGHT_RUN,
                   pole_base_offset: Optional[Position] = None,
                   validate: bool = True) -> BeltLanePlan:
    """Plan a belt lane from `source` connector through `waypoints` to `dest`.

    waypoints: ordered belt-connector points (the trace path) the agent chose.
    Returns a BeltLanePlan. Raises RoutingError if a span violates the incline
    limit or (when validate) is below the reliable-run minimum.
    """
    anchors = [source.position] + [Position(x=w.x, y=w.y, z=w.z) for w in waypoints] + [dest.position]
    dense = subdivide_path(anchors, max_straight)
    warnings: List[str] = []

    # validate spans
    for i in range(1, len(dense)):
        a, b = dense[i - 1], dense[i]
        inc = _incline_degrees(a, b)
        if inc > MAX_PLANNED_INCLINE_DEGREES + 1e-6:
            raise RoutingError(f"span {i} inclines {inc:.1f} deg > {MAX_PLANNED_INCLINE_DEGREES:.0f} - use a lift or add a z-step")
        run = _dist3d(a, b)
        if validate and run < MIN_RELIABLE_RUN:
            warnings.append(f"span {i} is {run:.0f}u (< {MIN_RELIABLE_RUN:.0f} reliable-run min); belts this short can fail 'Invalid shape' - widen waypoint spacing")

    plan = RoutePlan()
    segments: List[RouteSegment] = []
    off = pole_base_offset or Position(0.0, 0.0, 0.0)
    # interior dense points (not source/dest) each get a pole; belts connect
    # consecutive anchors, pinned to the connector points.
    ref_for_index: List[str] = [source.buildable_id]  # ref that produces the connector at dense[i]
    for i in range(1, len(dense) - 1):
        p = dense[i]
        nxt = dense[i + 1]
        pole_pos = Position(p.x + off.x, p.y + off.y, p.z + off.z)
        op_index = len(plan.ops)
        plan.ops.append(RouteOp(kind="place", recipe_class=pole_recipe, position=pole_pos,
                                yaw=_yaw_toward(p, nxt), note=f"belt pole {i} @({p.x:.0f},{p.y:.0f},{p.z:.0f})"))
        ref_for_index.append(f"op:{op_index}")
    ref_for_index.append(dest.buildable_id)

    for i in range(1, len(dense)):
        a, b = dense[i - 1], dense[i]
        plan.ops.append(RouteOp(kind="belt", source_ref=ref_for_index[i - 1], dest_ref=ref_for_index[i],
                                source_pin=a, dest_pin=b, note=f"belt span {i}/{len(dense) - 1}"))
        # DRC segment: let the belt ignore the machines it connects into at the
        # very ends (source at i==1, dest at last span).
        ignore = []
        if i == 1:
            ignore.append(source.buildable_id)
        if i == len(dense) - 1:
            ignore.append(dest.buildable_id)
        segments.append(RouteSegment(p0=a, p1=b, radius=clearance,
                                     label=f"span{i}", ignore_ids=tuple(ignore)))

    plan.warnings.extend(warnings)
    return BeltLanePlan(plan=plan, segments=segments, warnings=warnings)
