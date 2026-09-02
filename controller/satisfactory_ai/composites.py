"""Composite builders: whole build idioms as single planned units
(2026-09-02).

Phase 1d of docs/build-efficiency-plan.md. Each function here emits a
RoutePlan for one PROVEN idiom from the live 2026-08/09 builds - a
configured machine row, a manifold rail with drops, a power backbone -
so the agent issues one call per section instead of one call per
component. Parameters are explicit and the agent chooses all of them
(position, spacing, orientation, clocks, tiers); nothing here selects a
layout, ratio, or recipe on its own - toolkit, not solver.

All geometry is computed through connector_db (correct yaw and pinned
connector positions the FIRST time) and validated segment-by-segment
with the same rulebook router.direct_belt_feasible applies - a plan
that violates the envelope raises at PLANNING time, before anything is
built.

Verification: composites do not verify themselves silently - they
return the plan plus a `verify_spec` describing exactly which
connectors should read `connected: true` afterward, for the caller to
check with ONE world.connections query (see verify_connections()).
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Sequence, Tuple

from .connector_db import MERGER, SPLITTER, ConnectorDb
from .models import FactoryConnection, Position
from .router import (
    MIN_RELIABLE_RUN,
    Endpoint,
    RouteOp,
    RoutePlan,
    RoutingError,
    _MERGER_RECIPE,
    _SPLITTER_RECIPE,
    direct_belt_feasible,
)

POLE_RECIPE = (
    "/Game/FactoryGame/Recipes/Buildings/Recipe_PowerPoleMk1.Recipe_PowerPoleMk1_C"
)


@dataclass
class CompositePlan:
    plan: RoutePlan
    #: (ref, pinned connector position) pairs that must read connected
    #: after execution - ref is a buildable id or "op:<index>".
    verify_spec: List[Tuple[str, Position]] = field(default_factory=list)


def machine_row(
    db: ConnectorDb,
    build_recipe: str,
    class_key: str,
    count: int,
    origin: Position,
    spacing: float,
    yaw: float,
    machine_recipe: Optional[str] = None,
    clock_percent: Optional[float] = None,
    shards: int = 0,
    axis: str = "x",
) -> CompositePlan:
    """Place `count` machines in a line and configure each (recipe,
    shards, clock - in that order, matching the live-proven sequence).
    origin is the FIRST machine's placement position; subsequent
    machines step `spacing` along `axis`. All parameters are the
    agent's choices; this just executes them consistently."""
    if axis not in ("x", "y"):
        raise ValueError("axis must be 'x' or 'y'")
    plan = RoutePlan()
    for i in range(count):
        pos = Position(
            x=origin.x + (spacing * i if axis == "x" else 0.0),
            y=origin.y + (spacing * i if axis == "y" else 0.0),
            z=origin.z,
        )
        op_index = len(plan.ops)
        plan.ops.append(
            RouteOp(
                kind="place",
                recipe_class=build_recipe,
                position=pos,
                yaw=yaw,
                note=f"row machine {i + 1}/{count}",
            )
        )
        ref = f"op:{op_index}"
        if machine_recipe:
            plan.ops.append(
                RouteOp(
                    kind="call",
                    method="world.setRecipe",
                    dest_ref=ref,
                    params={"recipeClass": machine_recipe},
                    note=f"recipe machine {i + 1}",
                )
            )
        if shards > 0:
            plan.ops.append(
                RouteOp(
                    kind="call",
                    method="world.installPowerShard",
                    dest_ref=ref,
                    params={"count": shards},
                    note=f"shards machine {i + 1}",
                )
            )
        if clock_percent is not None:
            plan.ops.append(
                RouteOp(
                    kind="call",
                    method="world.setClockSpeed",
                    dest_ref=ref,
                    params={"clockSpeedPercent": float(clock_percent)},
                    note=f"clock machine {i + 1}",
                )
            )
    return CompositePlan(plan=plan)


def manifold(
    db: ConnectorDb,
    targets: Sequence[Endpoint],
    kind: str,
    rail_standoff: float = 400.0,
    trunk_end: str = "first",
    attachment_z: Optional[float] = None,
) -> CompositePlan:
    """A splitter (distribution) or merger (collection) rail serving a
    line of machine connectors, as one unit.

    targets: the machine-side connectors to serve, in rail order - for
      kind="splitter" these are machine INPUTS (all facing the rail);
      for kind="merger", machine OUTPUTS. All must share the same facing
      axis (the proven pattern: a rail parallel to the machine row).
    rail_standoff: rail distance from the connectors along their normal
      (>= MIN_RELIABLE_RUN + 100 so every drop is a reliable straight).
    trunk_end: "first" or "last" - which end of the rail carries the
      trunk connector (feed for a splitter rail, exit for a merger
      rail). The trunk endpoint is reported in the returned
      CompositePlan for the agent to route separately (route_connection)
      - the trunk's far side is a layout decision, not this unit's.
    """
    if kind not in ("splitter", "merger"):
        raise ValueError("kind must be 'splitter' or 'merger'")
    if len(targets) < 1:
        raise ValueError("at least one target")
    if rail_standoff < MIN_RELIABLE_RUN + 100.0:
        raise ValueError(
            f"rail_standoff {rail_standoff:.0f} leaves drops under the reliable "
            f"minimum - use >= {MIN_RELIABLE_RUN + 100.0:.0f}"
        )

    normal = targets[0].normal
    for t in targets:
        if (t.normal.x, t.normal.y) != (normal.x, normal.y):
            raise RoutingError("all manifold targets must share one facing")

    attachment_class = SPLITTER if kind == "splitter" else MERGER
    attachment_recipe = _SPLITTER_RECIPE if kind == "splitter" else _MERGER_RECIPE
    plan = RoutePlan()
    verify: List[Tuple[str, Position]] = []

    # Rail attachments sit along the targets' shared normal.
    rail_positions = [
        Position(
            x=t.position.x + normal.x * rail_standoff,
            y=t.position.y + normal.y * rail_standoff,
            z=attachment_z if attachment_z is not None else t.position.z,
        )
        for t in targets
    ]

    # Rail flow direction: from the trunk end toward the far end for a
    # splitter (items spread outward), and toward the trunk end for a
    # merger (items collect).
    if len(rail_positions) > 1:
        a, b = rail_positions[0], rail_positions[-1]
        along = Position(x=b.x - a.x, y=b.y - a.y, z=0.0)
        length = math.hypot(along.x, along.y) or 1.0
        along = Position(x=along.x / length, y=along.y / length, z=0.0)
    else:
        # Single target: rail direction is arbitrary; pick +x.
        along = Position(x=1.0, y=0.0, z=0.0)
    if trunk_end == "last":
        along = Position(x=-along.x, y=-along.y, z=0.0)
    flow = along if kind == "splitter" else Position(x=-along.x, y=-along.y, z=0.0)

    # Attachment yaw: drop side must face the machines. For a splitter,
    # the drop is an Output facing -normal (toward the machine); for a
    # merger, an Input facing -normal (accepting flow from the machine).
    drop_facing = Position(x=-normal.x, y=-normal.y, z=0.0)
    drop_dir = "Output" if kind == "splitter" else "Input"
    through_dir_in = "Input" if kind == "splitter" else "Output"  # unused marker

    yaw_candidates = [
        yaw
        for yaw in db.yaw_for_connector_facing(attachment_class, drop_dir, drop_facing)
        # The through-flow must also work: splitter needs its single
        # Input accepting `flow` (facing -flow); merger needs its single
        # Output facing `flow`.
        if (
            db.yaw_for_connector_facing(
                attachment_class,
                "Input" if kind == "splitter" else "Output",
                Position(x=-flow.x, y=-flow.y, z=0.0) if kind == "splitter" else flow,
            ).count(yaw)
            > 0
        )
    ]
    if not yaw_candidates:
        raise RoutingError(
            f"no {kind} yaw provides both a drop {drop_dir} facing "
            f"({drop_facing.x},{drop_facing.y}) and through-flow along ({flow.x},{flow.y})"
        )
    yaw = yaw_candidates[0]

    op_index_of_rail: Dict[int, int] = {}
    for i, (target, rail_pos) in enumerate(zip(targets, rail_positions)):
        op_index = len(plan.ops)
        op_index_of_rail[i] = op_index
        plan.ops.append(
            RouteOp(
                kind="place",
                recipe_class=attachment_recipe,
                position=rail_pos,
                yaw=yaw,
                note=f"{kind} rail {i + 1}/{len(targets)}",
            )
        )
        drop_pin, _ = db.find_connector(attachment_class, rail_pos, yaw, drop_dir, facing=drop_facing)
        if kind == "splitter":
            src_ref, src_pin = f"op:{op_index}", drop_pin
            dst_ref, dst_pin = target.buildable_id, target.position
        else:
            src_ref, src_pin = target.buildable_id, target.position
            dst_ref, dst_pin = f"op:{op_index}", drop_pin
        feasible, reason = direct_belt_feasible(
            Endpoint(src_ref, src_pin, normal if kind == "merger" else drop_facing),
            Endpoint(dst_ref, dst_pin, drop_facing if kind == "merger" else normal),
        )
        if not feasible:
            raise RoutingError(f"drop {i + 1} infeasible at planning time: {reason}")
        plan.ops.append(
            RouteOp(
                kind="belt",
                source_ref=src_ref,
                dest_ref=dst_ref,
                source_pin=src_pin,
                dest_pin=dst_pin,
                note=f"drop {i + 1}",
            )
        )
        verify.append((f"op:{op_index}", drop_pin))

    # Chain the rail. Splitter rail: through-Output of attachment i
    # feeds the Input of attachment i+1 (flow order). Merger rail:
    # Output of the FAR attachment flows toward the trunk end.
    order = range(len(targets) - 1)
    for i in order:
        upstream_i, downstream_i = (i, i + 1) if kind == "splitter" else (i + 1, i)
        if trunk_end == "last":
            upstream_i, downstream_i = (
                (len(targets) - 1 - i, len(targets) - 2 - i)
                if kind == "splitter"
                else (len(targets) - 2 - i, len(targets) - 1 - i)
            )
        up_pos = rail_positions[upstream_i]
        down_pos = rail_positions[downstream_i]
        out_pin, _ = db.find_connector(attachment_class, up_pos, yaw, "Output", facing=flow)
        in_pin, _ = db.find_connector(
            attachment_class, down_pos, yaw, "Input", facing=Position(x=-flow.x, y=-flow.y, z=0.0)
        )
        plan.ops.append(
            RouteOp(
                kind="belt",
                source_ref=f"op:{op_index_of_rail[upstream_i]}",
                dest_ref=f"op:{op_index_of_rail[downstream_i]}",
                source_pin=out_pin,
                dest_pin=in_pin,
                note=f"rail link {upstream_i + 1}->{downstream_i + 1}",
            )
        )

    # Report the trunk connector for the agent to route.
    trunk_i = 0 if trunk_end == "first" else len(targets) - 1
    trunk_pos = rail_positions[trunk_i]
    if kind == "splitter":
        trunk_pin, trunk_normal = db.find_connector(
            attachment_class, trunk_pos, yaw, "Input",
            facing=Position(x=-flow.x, y=-flow.y, z=0.0),
        )
    else:
        trunk_pin, trunk_normal = db.find_connector(attachment_class, trunk_pos, yaw, "Output", facing=flow)
    plan.warnings.append(
        f"trunk {'Input' if kind == 'splitter' else 'Output'} at "
        f"({trunk_pin.x:.0f},{trunk_pin.y:.0f},{trunk_pin.z:.0f}) on op:{op_index_of_rail[trunk_i]} "
        f"- route it with route_connection()"
    )
    return CompositePlan(plan=plan, verify_spec=verify)


def pole_backbone(waypoints: Sequence[Position], grid_source_id: Optional[str] = None) -> CompositePlan:
    """A power-pole spine along waypoints, wired pole-to-pole, with an
    optional first wire from an existing grid buildable. Machines then
    tap the NEAREST pole (executor.connect_power) instead of forming
    machine-to-machine chains - the deletion-fragility lesson of
    2026-09-02 (one severed chain node blacked out 8 machines)."""
    plan = RoutePlan()
    prev_ref: Optional[str] = None
    prev_pos: Optional[Position] = None
    for i, wp in enumerate(waypoints):
        op_index = len(plan.ops)
        plan.ops.append(
            RouteOp(kind="place", recipe_class=POLE_RECIPE, position=wp, yaw=0.0, note=f"pole {i + 1}")
        )
        ref = f"op:{op_index}"
        if i == 0 and grid_source_id is not None:
            plan.ops.append(
                RouteOp(
                    kind="wire",
                    source_ref=grid_source_id,
                    dest_ref=ref,
                    source_pin=wp,
                    dest_pin=wp,
                    note="grid anchor",
                )
            )
        if prev_ref is not None:
            plan.ops.append(
                RouteOp(
                    kind="wire",
                    source_ref=prev_ref,
                    dest_ref=ref,
                    source_pin=prev_pos,
                    dest_pin=wp,
                    note=f"backbone {i}->{i + 1}",
                )
            )
        prev_ref, prev_pos = ref, wp
    return CompositePlan(plan=plan)


def verify_connections(
    connections: List[FactoryConnection],
    verify_spec: List[Tuple[str, Position]],
    resolved_refs: Dict[str, str],
    tolerance: float = 5.0,
) -> List[str]:
    """Check a CompositePlan's verify_spec against a fresh
    world.connections snapshot. resolved_refs maps "op:<i>" refs to the
    real buildable ids the executor reported. Returns a list of
    human-readable problems - empty means verified."""
    problems: List[str] = []
    for ref, pin in verify_spec:
        buildable_id = resolved_refs.get(ref, ref)
        found = None
        for c in connections:
            if buildable_id not in c.owner_buildable_id:
                continue
            if (
                abs(c.position.x - pin.x) <= tolerance
                and abs(c.position.y - pin.y) <= tolerance
                and abs(c.position.z - pin.z) <= tolerance
            ):
                found = c
                break
        if found is None:
            problems.append(f"{buildable_id}: no connector found at ({pin.x:.0f},{pin.y:.0f},{pin.z:.0f})")
        elif not found.connected:
            problems.append(
                f"{buildable_id}: connector at ({pin.x:.0f},{pin.y:.0f},{pin.z:.0f}) is NOT connected"
            )
    return problems
