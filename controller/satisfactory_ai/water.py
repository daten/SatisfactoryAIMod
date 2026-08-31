"""Water pump field layout toolkit (added 2026-08-31, explicit user
request: "if as part of a larger build you determined you required a
large amount of water... would you be able to locate it and plan a
layout of multiple water pumps including a clean arrangement of pipes
and power lines... consider large scale builds that might include rows
of pumps that feed pipe junctions until each predicted pipe is
saturated. Not just a few pumps scattered near the shore.")

Companion to satisfactory_ai.pipes/power/layout, same "toolkit, not
solver" posture as the rest of this package: every function here
answers a geometry/grouping question about ALREADY-QUERIED data
(world.waterVolumes, world.pipelineTiers) - none of them fetch
telemetry, call any RPC, or decide the final layout on their own. A
computed plan is only ever a candidate; validate it live (a real
world.constructWaterPumpAtPosition/NearReference attempt) before
trusting it, same as every other module here.

Real capability this builds on (see RPC_REFERENCE.md/
docs/buildable-coverage.md for the full finding): world.placeExtractor
never actually supported Water Pump - it only searches
AFGResourceNodeBase actors, and a water body is an AFGWaterVolume
(APhysicsVolume), not a resource node. world.waterVolumes exposes real,
discoverable water bodies (position, bounds, occupancy);
world.constructWaterPumpAtPosition/world.constructWaterPumpNearReference
are the real construction paths - AFGWaterPumpHologram is the same
simple single-click AFGFactoryHologram lineage ordinary buildings use,
not a spline/multi-step hologram, so there's no routing/pathing
complexity in the CONSTRUCTION step itself - only in deciding WHERE
each pump goes, which is what this module is for.

**Real, unresolved unknown - flagged, not guessed**: no confirmed
minimum spacing distance between two Water Pumps exists anywhere in
this project's source research. Every spacing parameter below is a
REQUIRED, explicit input (no hidden default) for exactly this reason -
pass a conservative value and confirm with a live construction attempt
(or a sequence of them) before trusting a tight layout. The first real
water-pump-field live test should specifically try to find this real
minimum, since every function here currently takes the caller's word
for it.

Per-pump flow rate is also a required, explicit input, not looked up
here - derive it from a live world.recipeCatalog query of
Recipe_WaterPump's product amount / manufacturingDuration (converted to
the same m^3/s unit satisfactory_ai.models.PipelineTier.flow_limit
uses), matching this package's "the caller supplies real queried
numbers, this module never fabricates one" convention throughout.
"""

from __future__ import annotations

import math
from dataclasses import dataclass
from typing import List, Optional

from .layout import group_producers_by_flow_capacity
from .models import PipelineTier, Position
from .pipes import max_producers_per_pipe


@dataclass(frozen=True)
class WaterPumpRowPlan:
    """One row's real placement candidates - pump_positions are direct
    inputs to world.constructWaterPumpAtPosition (the first row's first
    pump) or world.constructWaterPumpNearReference (every subsequent
    pump, using whichever prior pump in the field is already confirmed
    built as the reference - this module does not care which, that's
    the caller's construction-sequencing choice). junction_position is
    a candidate for where this row's pumps should pipe-merge together
    (e.g. a Pipeline Junction) before continuing toward the main
    project or a higher-level trunk merge.

    foundation_positions is empty unless foundation_offset/
    foundation_size were given to plan_water_pump_row/
    plan_water_pump_field - a walkway strip ALONGSIDE the row (not
    underneath the pumps - a Water Pump sits in real water, a
    foundation almost certainly can't share that spot), giving pipes
    exiting each pump somewhere to route onto and letting power poles
    be built on solid ground instead of floating. See
    plan_water_pump_row's doc comment for the corrected 2026-08-31
    semantics.
    """

    pump_positions: List[Position]
    junction_position: Position
    foundation_positions: List[Position]


def _normalize(v: Position) -> Position:
    length = math.sqrt(v.x * v.x + v.y * v.y + v.z * v.z)
    if length == 0:
        raise ValueError("direction vector must not be the zero vector")
    return Position(x=v.x / length, y=v.y / length, z=v.z / length)


def plan_water_pump_row(
    row_start: Position,
    pump_direction: Position,
    pump_spacing: float,
    pump_count: int,
    junction_offset: Position,
    foundation_offset: Optional[Position] = None,
    foundation_size: Optional[float] = None,
) -> WaterPumpRowPlan:
    """pump_count pump positions starting at row_start, each
    pump_spacing apart along pump_direction (need not be pre-normalized
    - normalized internally). junction_position = the LAST pump's
    position + junction_offset (junction_offset is an explicit vector,
    not computed - the caller decides which direction routes back
    toward the main project or the next merge level).

    foundation_offset/foundation_size (optional, BOTH required
    together): a strip of square foundation tiles ALONGSIDE the pump
    row, NOT underneath the pumps - corrected 2026-08-31 per explicit
    user clarification: a Water Pump sits directly in/on a real
    AFGWaterVolume, a foundation almost certainly can't be placed at
    that same spot (and even where it could, that isn't the intent).
    The real purpose is a walkway next to the row so pipes exiting each
    pump have a surface to route onto, and power poles can be built on
    solid ground above water instead of needing to float.

    foundation_offset is the vector from row_start to the strip's own
    start point - perpendicular to pump_direction is the natural
    choice for a walkway running alongside the row, but this function
    does not assume a direction; pass whatever offset actually clears
    the water for your specific lake. Tiles are centered
    foundation_size apart along pump_direction (same direction as the
    pump row, so the strip runs parallel to it) and span enough length
    to run the full length of the row, not just one tile per pump -
    pass the real foundation tile size (e.g. 800.0 for a standard 8m
    foundation); this function does not assume one.

    Raises ValueError if pump_count < 1, pump_spacing <= 0,
    pump_direction is the zero vector, foundation_size is given and
    <= 0, or exactly one of foundation_offset/foundation_size is given
    (both or neither are required - a strip needs both a location and
    a tile size to be a meaningful request).
    """
    if pump_count < 1:
        raise ValueError(f"pump_count must be at least 1, got {pump_count}")
    if pump_spacing <= 0:
        raise ValueError(f"pump_spacing must be positive, got {pump_spacing}")
    if (foundation_offset is None) != (foundation_size is None):
        raise ValueError("foundation_offset and foundation_size must both be given, or both omitted")
    if foundation_size is not None and foundation_size <= 0:
        raise ValueError(f"foundation_size must be positive if given, got {foundation_size}")

    unit = _normalize(pump_direction)
    pump_positions = [
        Position(
            x=row_start.x + unit.x * pump_spacing * i,
            y=row_start.y + unit.y * pump_spacing * i,
            z=row_start.z + unit.z * pump_spacing * i,
        )
        for i in range(pump_count)
    ]

    last = pump_positions[-1]
    junction_position = Position(
        x=last.x + junction_offset.x,
        y=last.y + junction_offset.y,
        z=last.z + junction_offset.z,
    )

    foundation_positions: List[Position] = []
    if foundation_offset is not None and foundation_size is not None:
        strip_start = Position(
            x=row_start.x + foundation_offset.x,
            y=row_start.y + foundation_offset.y,
            z=row_start.z + foundation_offset.z,
        )
        # Span the same real length the pump row covers, tiled at
        # foundation_size - not just one tile per pump, enough tiles to
        # run the walkway the full length of the row.
        row_length = pump_spacing * (pump_count - 1)
        tile_count = max(1, math.ceil(row_length / foundation_size) + 1)
        foundation_positions = [
            Position(
                x=strip_start.x + unit.x * foundation_size * i,
                y=strip_start.y + unit.y * foundation_size * i,
                z=strip_start.z + unit.z * foundation_size * i,
            )
            for i in range(tile_count)
        ]

    return WaterPumpRowPlan(
        pump_positions=pump_positions,
        junction_position=junction_position,
        foundation_positions=foundation_positions,
    )


def plan_water_pump_field(
    shoreline_start: Position,
    pump_direction: Position,
    row_offset_direction: Position,
    row_spacing: float,
    pump_spacing: float,
    junction_offset: Position,
    total_pump_count: int,
    row_pipe_tier: PipelineTier,
    per_pump_flow_rate: float,
    foundation_offset: Optional[Position] = None,
    foundation_size: Optional[float] = None,
) -> List[WaterPumpRowPlan]:
    """Lays out total_pump_count pumps into as many ROWS as needed so
    each row's combined output stays within row_pipe_tier.flow_limit -
    "rows of pumps that feed pipe junctions until each predicted pipe
    is saturated" (the user's own framing), not a handful scattered
    near shore. pumps_per_row comes from
    satisfactory_ai.pipes.max_producers_per_pipe(row_pipe_tier,
    per_pump_flow_rate) - reuses the SAME real flow-budget calculator
    already built for pipe planning generally, not a new formula
    invented for this module.

    Each row is plan_water_pump_row() (see its doc comment for
    foundation_offset/foundation_size/junction_offset), offset from the
    previous row by
    row_offset_direction * row_spacing (row_offset_direction need not
    be pre-normalized). The LAST row may have fewer than pumps_per_row
    pumps if total_pump_count doesn't divide evenly.

    Returns one WaterPumpRowPlan per row - each row's OWN
    junction_position is a real, independent producer point for a
    SECOND, higher-level call to
    satisfactory_ai.layout.group_producers_by_flow_capacity() (passing
    each row's own combined flow - pumps_per_row * per_pump_flow_rate
    for a full row, the last row's actual count * per_pump_flow_rate
    for a partial one) if multiple rows need to merge further before
    reaching the main project or a bigger trunk pipe. This function
    does NOT do that second merge itself - same "toolkit not solver"
    posture as the rest of this package: it answers "how do I lay out
    enough pumps," not "how do I route everything all the way back to
    base." Compose it with group_producers_by_flow_capacity and
    satisfactory_ai.layout.plan_shared_support_columns (for the
    pipe+power trunk corridor back to the project, using real
    Recipe_PipeSupportStackable/Recipe_ConveyorPoleStackable support
    columns) for the rest of the route.

    pump_spacing/row_spacing are NOT validated against any real minimum
    distance - see this module's own docstring for why (a genuinely
    unconfirmed value). Pass a conservative estimate and confirm with a
    live construction attempt before trusting a tight layout.

    Raises ValueError if total_pump_count < 1, or if per_pump_flow_rate
    alone exceeds row_pipe_tier.flow_limit (a single pump already
    saturates the chosen tier - needs a larger pipe tier, not a layout
    fix; see max_producers_per_pipe).
    """
    if total_pump_count < 1:
        raise ValueError(f"total_pump_count must be at least 1, got {total_pump_count}")

    pumps_per_row = max_producers_per_pipe(row_pipe_tier, per_pump_flow_rate)
    if pumps_per_row < 1:
        raise ValueError(
            f"a single pump's flow rate ({per_pump_flow_rate}) already exceeds "
            f"row_pipe_tier.flow_limit ({row_pipe_tier.flow_limit}) - choose a larger pipe tier"
        )

    row_count = math.ceil(total_pump_count / pumps_per_row)
    unit_row_offset = _normalize(row_offset_direction)

    rows: List[WaterPumpRowPlan] = []
    remaining = total_pump_count
    for row_index in range(row_count):
        this_row_count = min(pumps_per_row, remaining)
        remaining -= this_row_count
        row_start = Position(
            x=shoreline_start.x + unit_row_offset.x * row_spacing * row_index,
            y=shoreline_start.y + unit_row_offset.y * row_spacing * row_index,
            z=shoreline_start.z + unit_row_offset.z * row_spacing * row_index,
        )
        rows.append(
            plan_water_pump_row(
                row_start=row_start,
                pump_direction=pump_direction,
                pump_spacing=pump_spacing,
                pump_count=this_row_count,
                junction_offset=junction_offset,
                foundation_offset=foundation_offset,
                foundation_size=foundation_size,
            )
        )
    return rows


def plan_trunk_merge(
    row_plans: List[WaterPumpRowPlan],
    row_flow_rates: List[float],
    trunk_pipe_tier: PipelineTier,
) -> List[List[int]]:
    """Groups MULTIPLE rows' junction points into however many trunk
    pipes are needed so each trunk stays within
    trunk_pipe_tier.flow_limit - the "merge the row-junctions into a
    bigger trunk toward the main project" half of the hierarchy the
    user described, kept separate from plan_water_pump_field itself
    (same "toolkit not solver" composability as the rest of this
    package - trunk_pipe_tier is very likely a LARGER tier than
    row_pipe_tier, and that tier choice belongs to the caller, not a
    hidden default here).

    row_flow_rates[i] must correspond to row_plans[i] - typically each
    row's actual pump count * per_pump_flow_rate (the last row from
    plan_water_pump_field may have fewer pumps than a full row, so
    don't assume every row's rate is identical - pass the real
    per-row total).

    Returns groups of ROW INDICES (into row_plans/row_flow_rates), via
    satisfactory_ai.layout.group_producers_by_flow_capacity - each
    group's rows should pipe-connect to one shared trunk junction/pipe.
    If every row fits in a single group, the whole field only needs one
    trunk pipe back to the main project; if not, this tells you exactly
    how many trunk pipes are needed and which rows feed each one -
    directly answering "clean arrangement... feed pipe junctions until
    each predicted pipe is saturated" at the SECOND level of the
    hierarchy, not just the first.

    Raises ValueError if len(row_plans) != len(row_flow_rates), or (via
    group_producers_by_flow_capacity) if trunk_pipe_tier.flow_limit <=
    0 or any single row's flow rate alone exceeds it.
    """
    if len(row_plans) != len(row_flow_rates):
        raise ValueError(
            f"row_plans ({len(row_plans)}) and row_flow_rates ({len(row_flow_rates)}) must be the same length"
        )
    return group_producers_by_flow_capacity(row_flow_rates, trunk_pipe_tier.flow_limit)
