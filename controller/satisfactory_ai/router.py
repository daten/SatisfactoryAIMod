"""Geometry-aware connection router (2026-09-02).

Phase 1b of docs/build-efficiency-plan.md. Turns "connect output A to
input B" into a concrete PLAN - a direct belt when the geometry allows
it, otherwise belt segments plus the jog-merger / relay-splitter /
elevation attachments the live sessions repeatedly discovered by trial
and error. Encodes the belt rulebook measured across the 2026-08/09
sessions (docs/placement-lessons.md, memory project_hmf_optimization.md):

- runs under ~300 units are flaky ("Invalid Conveyor Belt shape!" on
  some 200s, hard failure below ~150) - MIN_RELIABLE_RUN
- one segment cannot exceed maxSplineLength (5600 for Mk1-4) - longer
  routes need relay attachments
- incline: 35 degrees is the hard limit; plan to 30 to leave margin
  (bogus "too steep!" also fires when a flat belt's clearance lifts
  over machines/foundations - that is an OBSTRUCTION problem this
  router treats via z-lanes, not retries)
- a belt ENTERS an Input travelling along -input_normal and LEAVES an
  Output travelling along +output_normal; when those headings oppose
  the displacement (an "S against the facing"), the Auto router
  usually fails - fixed by a jog merger whose input accepts the
  approach heading and whose output faces the target
- crossings between same-z lanes collide; the proven fix is a +200 z
  layer reached by <=18-degree two-stage climbs, or conveyor lifts for
  large drops

The router decides HOW a requested connection is realized. It never
decides WHAT to connect - that stays with the agent (toolkit-not-solver,
same as the rest of this package).

Output format: a RoutePlan containing ordered RouteOps. "place" ops
carry recipe class + position/yaw; "belt" ops carry the two endpoint
descriptors with PINNED connector positions (the live sessions proved
unpinned connections grab wrong connectors). The executor
(satisfactory_ai.executor) maps these to world.placeBuilding /
world.connectConveyor calls; the plan is also readable/loggable so
competing agents' plans can be diffed.

Limitations (deliberate, documented):
- Plans at most ONE intermediate attachment (a jog) per incompatibility
  plus relays for length - it does not do full obstacle-avoiding
  pathfinding. If a plan still fails against live geometry, the failure
  is surfaced for the agent to reroute (e.g. pick a different z lane) -
  a 10-second decision with this tooling vs the old 10-minute loop.
- Assumes cardinal (axis-aligned) connector normals, which every
  live-confirmed profile has.
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field
from typing import List, Optional, Tuple

from .connector_db import MERGER, SPLITTER, ConnectorDb
from .models import Position

# Rulebook constants - sources in module docstring.
MIN_RELIABLE_RUN = 300.0
MAX_SEGMENT_RUN = 5600.0
MAX_PLANNED_INCLINE_DEGREES = 30.0
JOG_STANDOFF = 300.0  # how far a jog attachment sits from the input it feeds


@dataclass(frozen=True)
class Endpoint:
    """One side of a requested connection: an existing buildable's
    connector, fully pinned."""

    buildable_id: str
    position: Position  # exact connector position (pin this in the RPC)
    normal: Position  # connector's world facing


@dataclass
class RouteOp:
    kind: str  # "place" | "belt" | "lift"
    # place:
    recipe_class: Optional[str] = None
    position: Optional[Position] = None
    yaw: Optional[float] = None
    # belt/lift (source/dest may name a placeholder from a prior place op):
    source_ref: Optional[str] = None  # buildable id or "op:<index>"
    dest_ref: Optional[str] = None
    source_pin: Optional[Position] = None
    dest_pin: Optional[Position] = None
    note: str = ""


@dataclass
class RoutePlan:
    ops: List[RouteOp] = field(default_factory=list)
    warnings: List[str] = field(default_factory=list)

    def describe(self) -> str:
        lines = []
        for i, op in enumerate(self.ops):
            if op.kind == "place":
                lines.append(
                    f"[{i}] place {op.recipe_class.rsplit('.', 1)[-1]} at "
                    f"({op.position.x:.0f},{op.position.y:.0f},{op.position.z:.0f}) yaw={op.yaw:.0f} - {op.note}"
                )
            else:
                lines.append(
                    f"[{i}] {op.kind} {op.source_ref} -> {op.dest_ref} "
                    f"({op.source_pin.x:.0f},{op.source_pin.y:.0f},{op.source_pin.z:.0f}) -> "
                    f"({op.dest_pin.x:.0f},{op.dest_pin.y:.0f},{op.dest_pin.z:.0f}) - {op.note}"
                )
        for w in self.warnings:
            lines.append(f"WARNING: {w}")
        return "\n".join(lines)


class RoutingError(Exception):
    """The router cannot produce a plan within its documented limits -
    the agent should choose a different approach (other z lane, other
    connector, repositioned machine)."""


def _dist2d(a: Position, b: Position) -> float:
    return math.hypot(b.x - a.x, b.y - a.y)


def _incline_degrees(a: Position, b: Position) -> float:
    run = _dist2d(a, b)
    rise = abs(b.z - a.z)
    if run < 1e-6:
        return 90.0
    return math.degrees(math.atan2(rise, run))


def _snap_cardinal(v: Position) -> Position:
    """Snap a horizontal direction to its dominant cardinal axis (unit
    vector). Raises on a zero vector."""
    if abs(v.x) < 1e-9 and abs(v.y) < 1e-9:
        raise RoutingError("cannot snap a zero-length heading to a cardinal")
    if abs(v.x) >= abs(v.y):
        return Position(x=1.0 if v.x > 0 else -1.0, y=0.0, z=0.0)
    return Position(x=0.0, y=1.0 if v.y > 0 else -1.0, z=0.0)


def _heading_dot_displacement(normal: Position, from_pos: Position, to_pos: Position) -> float:
    """Positive when travelling along `normal` moves you toward to_pos."""
    dx, dy = to_pos.x - from_pos.x, to_pos.y - from_pos.y
    length = math.hypot(dx, dy)
    if length < 1e-6:
        return 1.0
    return (normal.x * dx + normal.y * dy) / length


def direct_belt_feasible(source: Endpoint, dest: Endpoint) -> Tuple[bool, str]:
    """Whether ONE belt between these pinned connectors is inside the
    reliability envelope. Returns (ok, reason-if-not)."""
    run = _dist2d(source.position, dest.position)
    total = math.hypot(run, dest.position.z - source.position.z)
    if total < MIN_RELIABLE_RUN:
        return False, f"run {total:.0f} under reliable minimum {MIN_RELIABLE_RUN:.0f}"
    if total > MAX_SEGMENT_RUN:
        return False, f"run {total:.0f} over max segment {MAX_SEGMENT_RUN:.0f}"
    if _incline_degrees(source.position, dest.position) > MAX_PLANNED_INCLINE_DEGREES:
        return False, (
            f"incline {_incline_degrees(source.position, dest.position):.1f} deg over planned max "
            f"{MAX_PLANNED_INCLINE_DEGREES:.0f}"
        )
    # Exit heading must not oppose the displacement, and the approach to
    # the input must not oppose ITS required entry heading (-dest.normal).
    if _heading_dot_displacement(source.normal, source.position, dest.position) < -0.1:
        return False, "source exit heading opposes the displacement (S against the output facing)"
    entry_heading = Position(x=-dest.normal.x, y=-dest.normal.y, z=-dest.normal.z)
    if _heading_dot_displacement(entry_heading, source.position, dest.position) < -0.1:
        return False, "required entry heading opposes the displacement (S against the input facing)"
    return True, ""


def route_connection(
    db: ConnectorDb,
    source: Endpoint,
    dest: Endpoint,
    belt_recipe: str,
    attachment_z: Optional[float] = None,
) -> RoutePlan:
    """Plan source Output -> dest Input.

    Tries, in order:
    1. one direct belt (inside the envelope)
    2. a single JOG MERGER placed JOG_STANDOFF back along the input's
       facing, oriented so its own input accepts the natural approach
       heading - the fix that resolved every live S-curve failure
    3. relay subdivision for over-length runs (straight midpoints)

    attachment_z: z for any inserted attachment (defaults to the dest
    pin's z - correct when both connectors are at belt level; pass
    explicitly when routing between z-lanes).
    """
    plan = RoutePlan()

    ok, reason = direct_belt_feasible(source, dest)
    if ok:
        plan.ops.append(
            RouteOp(
                kind="belt",
                source_ref=source.buildable_id,
                dest_ref=dest.buildable_id,
                source_pin=source.position,
                dest_pin=dest.position,
                note="direct",
            )
        )
        return plan

    total = math.hypot(_dist2d(source.position, dest.position), dest.position.z - source.position.z)
    if total > MAX_SEGMENT_RUN:
        return _plan_relayed(db, source, dest, belt_recipe, plan)

    # Jog-merger fix: stand a merger JOG_STANDOFF back along the input's
    # facing (so merger-out -> input is a short straight the input
    # accepts), and pick the merger yaw whose OUTPUT faces the input's
    # entry heading. The merger's own free inputs then accept the
    # approach from the source side.
    entry = Position(x=-dest.normal.x, y=-dest.normal.y, z=0.0)  # heading INTO the input
    jz = attachment_z if attachment_z is not None else dest.position.z
    jog_pos = Position(
        x=dest.position.x + dest.normal.x * JOG_STANDOFF,
        y=dest.position.y + dest.normal.y * JOG_STANDOFF,
        z=jz,
    )
    out_yaws = db.yaw_for_connector_facing(MERGER, "Output", entry)
    if not out_yaws:
        raise RoutingError(f"no merger yaw gives an Output facing ({entry.x},{entry.y})")
    jog_yaw = out_yaws[0]
    jog_out_pos, _ = db.find_connector(MERGER, jog_pos, jog_yaw, "Output", facing=entry)

    # Choose the merger input whose position/facing best accepts the
    # source: prefer the one whose entry heading has the largest
    # alignment with the source->jog displacement.
    best = None
    for profile, in_pos, in_normal in db.predict(MERGER, jog_pos, jog_yaw):
        if profile.direction != "Input":
            continue
        in_entry = Position(x=-in_normal.x, y=-in_normal.y, z=0.0)
        score = _heading_dot_displacement(in_entry, source.position, in_pos)
        exit_score = _heading_dot_displacement(source.normal, source.position, in_pos)
        if exit_score < -0.1:
            continue  # source cannot exit toward this input at all
        if best is None or score > best[0]:
            best = (score, in_pos, in_normal)
    if best is None or best[0] < -0.1:
        raise RoutingError(
            "no jog-merger input accepts the approach from the source - pick a "
            "different z lane or a different source connector"
        )
    _, jog_in_pos, _ = best

    approach_run = math.hypot(
        _dist2d(source.position, jog_in_pos), jog_in_pos.z - source.position.z
    )
    if approach_run < MIN_RELIABLE_RUN:
        plan.warnings.append(
            f"approach run {approach_run:.0f} is under the reliable minimum - expect a "
            f"possible shape failure; consider moving the jog further out"
        )

    plan.ops.append(
        RouteOp(
            kind="place",
            recipe_class=_MERGER_RECIPE,
            position=jog_pos,
            yaw=jog_yaw,
            note=f"jog merger fixing: {reason}",
        )
    )
    plan.ops.append(
        RouteOp(
            kind="belt",
            source_ref=source.buildable_id,
            dest_ref="op:0",
            source_pin=source.position,
            dest_pin=jog_in_pos,
            note="source -> jog",
        )
    )
    plan.ops.append(
        RouteOp(
            kind="belt",
            source_ref="op:0",
            dest_ref=dest.buildable_id,
            source_pin=jog_out_pos,
            dest_pin=dest.position,
            note="jog -> input (straight along the input's facing)",
        )
    )
    return plan


def _plan_relayed(
    db: ConnectorDb, source: Endpoint, dest: Endpoint, belt_recipe: str, plan: RoutePlan
) -> RoutePlan:
    """Subdivide an over-length run with relay SPLITTERS at straight-line
    midpoints (splitter chosen over pole: proven passthrough with a
    single input, and its spare outputs are useful taps later)."""
    total = math.hypot(_dist2d(source.position, dest.position), dest.position.z - source.position.z)
    segments = max(2, math.ceil(total / (MAX_SEGMENT_RUN * 0.9)))
    prev_ref = source.buildable_id
    prev_pin = source.position
    prev_normal = source.normal
    for i in range(1, segments):
        t = i / segments
        mid = Position(
            x=source.position.x + (dest.position.x - source.position.x) * t,
            y=source.position.y + (dest.position.y - source.position.y) * t,
            z=source.position.z + (dest.position.z - source.position.z) * t,
        )
        # Relay input should accept the incoming heading (from prev).
        # Connector normals are always cardinal, so snap a diagonal
        # travel heading to its DOMINANT axis - belts curve the last
        # stretch into a cardinal connector without complaint as long as
        # the entry doesn't oppose it (live-confirmed repeatedly by the
        # Auto route mode on diagonal approaches).
        heading = _snap_cardinal(
            Position(x=mid.x - prev_pin.x, y=mid.y - prev_pin.y, z=0.0)
        )
        # Splitter input faces OPPOSITE the travel heading (belt enters
        # travelling along +heading, input normal points back at it).
        want_input_facing = Position(x=-heading.x, y=-heading.y, z=0.0)
        yaws = db.yaw_for_connector_facing(SPLITTER, "Input", want_input_facing)
        if not yaws:
            raise RoutingError("no splitter yaw accepts the relay heading")
        yaw = yaws[0]
        in_pos, _ = db.find_connector(SPLITTER, mid, yaw, "Input", facing=want_input_facing)
        op_index = len(plan.ops)
        plan.ops.append(
            RouteOp(
                kind="place",
                recipe_class=_SPLITTER_RECIPE,
                position=mid,
                yaw=yaw,
                note=f"relay {i}/{segments - 1} (run {total:.0f} over {MAX_SEGMENT_RUN:.0f})",
            )
        )
        plan.ops.append(
            RouteOp(
                kind="belt",
                source_ref=prev_ref,
                dest_ref=f"op:{op_index}",
                source_pin=prev_pin,
                dest_pin=in_pos,
                note=f"segment {i}/{segments}",
            )
        )
        out_pos, out_normal = db.find_connector(SPLITTER, mid, yaw, "Output", facing=heading)
        prev_ref = f"op:{op_index}"
        prev_pin = out_pos
        prev_normal = out_normal
    plan.ops.append(
        RouteOp(
            kind="belt",
            source_ref=prev_ref,
            dest_ref=dest.buildable_id,
            source_pin=prev_pin,
            dest_pin=dest.position,
            note=f"segment {segments}/{segments}",
        )
    )
    return plan


_MERGER_RECIPE = (
    "/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorAttachmentMerger."
    "Recipe_ConveyorAttachmentMerger_C"
)
_SPLITTER_RECIPE = (
    "/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorAttachmentSplitter."
    "Recipe_ConveyorAttachmentSplitter_C"
)
