"""Placement-geometry toolkit (added 2026-08-25).

Grew out of a live belt-routing investigation where explaining a
"belt geometrically impossible" `CanConstruct()` failure required
reading connector position/normal data via one-off diagnostic log
statements bolted onto the mod's C++ - see
docs/telemetry-protocol.md's "connections" section and
docs/demo-production-chain.md. That data is now ordinary queryable
telemetry (`world.connections`' `position`/`normal` fields,
`FactoryConnection.position`/`.normal` here); this module is the
geometry math built on top of it.

Deliberately a TOOLKIT, not a planner or auto-layout solver. Per
explicit direction: this module answers geometry questions - is this
connector pair compatible? where does a connector end up if its owner
is placed at position P, yaw Y? what position aligns a new building's
connector with an existing one? what yaw would face a connector a
given direction? - for an agent (human or LLM) to compose and iterate
over while planning or optimizing a layout. It does not choose a
layout, sequence placements, search for the "best" position, or decide
between alternatives - that belongs to the agent, matching CLAUDE.md's
LLM responsibilities ("deciding between valid plans... selecting
optimization objectives") and its Deterministic solver responsibilities
("geometry where deterministic geometry is appropriate") split, applied
at the Python layer as much as the SML/LLM boundary itself. Every
function here takes explicit inputs and returns one answer or one
candidate - no hidden defaults that quietly pick an outcome for the
caller (e.g. clearance_distance is always a required argument, never a
baked-in constant).

None of this executes anything - it never calls world.placeBuilding or
any other RPC. A computed placement is only ever a candidate; the agent
is expected to validate it live (e.g. a world.testConveyorBelt dry run)
before treating it as correct, same as CLAUDE.md's "CanConstruct() is
still the real gate" pattern used throughout the mod itself.
"""

from __future__ import annotations

import math
from dataclasses import dataclass
from typing import List, Optional, Tuple

from .models import Buildable, FactoryConnection, Position

# Satisfactory building placement/orientation is grid- and cardinal-
# aligned - every real placement's yaw captured this session
# (docs/demo-production-chain.md) was exactly 0.0 or 180.0, never an
# odd angle. Used by candidate_yaws_for_normal() to enumerate the
# orientations actually worth considering, not as a hard constraint
# elsewhere in this module (rotate_yaw/predict_connector_world_state
# accept any yaw).
CARDINAL_YAWS_DEGREES: Tuple[float, ...] = (0.0, 90.0, 180.0, 270.0)

# Two normals are treated as parallel/anti-parallel if their dot
# product is within this of +-1 - generous enough for float noise in
# captured telemetry (e.g. GetConnectorNormal() returning -0.0 instead
# of 0.0) without accepting a genuinely different direction.
_DIRECTION_DOT_TOLERANCE = 1e-3


def _sub(a: Position, b: Position) -> Position:
    return Position(x=a.x - b.x, y=a.y - b.y, z=a.z - b.z)


def _add(a: Position, b: Position) -> Position:
    return Position(x=a.x + b.x, y=a.y + b.y, z=a.z + b.z)


def _scale(a: Position, s: float) -> Position:
    return Position(x=a.x * s, y=a.y * s, z=a.z * s)


def _dot(a: Position, b: Position) -> float:
    return a.x * b.x + a.y * b.y + a.z * b.z


def rotate_yaw(v: Position, yaw_degrees: float) -> Position:
    """Rotates a vector about world +Z (up) by yaw_degrees, matching
    Unreal's FRotator/FRotationMatrix convention that every position/
    rotation value already captured in this project's telemetry uses
    (positive yaw turns +X toward +Y). z passes through unchanged."""
    rad = math.radians(yaw_degrees)
    cos_y, sin_y = math.cos(rad), math.sin(rad)
    return Position(
        x=v.x * cos_y - v.y * sin_y,
        y=v.x * sin_y + v.y * cos_y,
        z=v.z,
    )


def unrotate_yaw(v: Position, yaw_degrees: float) -> Position:
    """Inverse of rotate_yaw - transforms a world-space vector into a
    building's local frame given its current yaw."""
    return rotate_yaw(v, -yaw_degrees)


@dataclass(frozen=True)
class ConnectorProfile:
    """One connector's position/normal in its owning building's LOCAL
    frame (the building's own yaw rotated out) - reusable to predict
    where that connector would land at ANY future placement position/
    yaw, not just the one it was learned from.
    """

    direction: str
    local_position: Position
    local_normal: Position


def learn_connector_profile(buildable: Buildable, connection: FactoryConnection) -> ConnectorProfile:
    """Derives one connector's LOCAL (yaw-independent) position/normal
    from an already-placed instance's live telemetry.

    Rotation-aware (unrotates by the buildable's CURRENT yaw) rather
    than assuming yaw=0, even though reliable RPC-driven placement
    rotation is a separate, still-open investigation (see
    ConstructBuildingAtPosition's rotationScrollDelta doc comment in
    AIModFunctionLibrary.h) - a profile learned from a building
    sitting at any yaw is still valid input to
    predict_connector_world_state() for a hypothetical future placement
    at a different yaw, once placement rotation is reliable.
    """
    if connection.owner_buildable_id != buildable.id:
        raise ValueError(
            f"connection.owner_buildable_id ({connection.owner_buildable_id!r}) "
            f"does not match buildable.id ({buildable.id!r})"
        )
    local_position = unrotate_yaw(_sub(connection.position, buildable.position), buildable.rotation.yaw)
    local_normal = unrotate_yaw(connection.normal, buildable.rotation.yaw)
    return ConnectorProfile(direction=connection.direction, local_position=local_position, local_normal=local_normal)


def learn_all_connector_profiles(
    buildable: Buildable, connections: List[FactoryConnection]
) -> List[ConnectorProfile]:
    """learn_connector_profile() for every connection owned by buildable
    - a plain filter+map, not a selection/ranking of which connector
    matters (that's the agent's call, e.g. picking the free Output vs a
    connected one)."""
    return [learn_connector_profile(buildable, c) for c in connections if c.owner_buildable_id == buildable.id]


def predict_connector_world_state(
    profile: ConnectorProfile, building_position: Position, building_yaw: float
) -> Tuple[Position, Position]:
    """Forward transform: where would this connector be (position,
    normal) if its owning building were placed at building_position
    with building_yaw? Pure geometry - does not query or mutate game
    state, and does not check whether that placement is otherwise
    valid (ground, clearance, reach)."""
    world_position = _add(building_position, rotate_yaw(profile.local_position, building_yaw))
    world_normal = rotate_yaw(profile.local_normal, building_yaw)
    return world_position, world_normal


def connectors_are_compatible(
    normal_a: Position, normal_b: Position, tolerance: float = _DIRECTION_DOT_TOLERANCE
) -> bool:
    """True if two connectors facing normal_a/normal_b could be joined
    by a straight, no-bend belt - confirmed live (2026-08-25,
    docs/demo-production-chain.md): items leave an Output connector
    along +normal and must arrive at an Input connector along -normal,
    so a compatible pair's normals must be exact opposites (this check
    is symmetric - which one is Output vs Input doesn't change the
    result). Orientation only - does NOT check position, clearance, or
    physical obstruction; a full feasibility check still needs a real
    world.testConveyorBelt dry run.
    """
    return _dot(normal_a, normal_b) <= (-1.0 + tolerance)


def candidate_yaws_for_normal(
    local_normal: Position, desired_world_normal: Position, tolerance: float = _DIRECTION_DOT_TOLERANCE
) -> List[float]:
    """Which of the 4 cardinal yaws (0/90/180/270) rotate this
    connector's local_normal to point along desired_world_normal?

    Lets an agent ask "what orientation would make this connector face
    X?" before attempting placement rotation - purely the geometry
    answer, independent of whether the RPC rotation control can
    currently execute it (see ConstructBuildingAtPosition's
    rotationScrollDelta doc comment - that calibration is unresolved).
    """
    return [
        yaw
        for yaw in CARDINAL_YAWS_DEGREES
        if _dot(rotate_yaw(local_normal, yaw), desired_world_normal) >= (1.0 - tolerance)
    ]


def compute_aligned_placement_position(
    target_connector_position: Position,
    target_connector_normal: Position,
    new_connector_profile: ConnectorProfile,
    new_building_yaw: float,
    clearance_distance: float,
) -> Position:
    """Where should the NEW building's origin go so that
    new_connector_profile, at new_building_yaw, lands clearance_distance
    units along target_connector_normal from target_connector_position,
    facing back the opposite way?

    Returns ONE candidate for the yaw/clearance the caller chose - does
    not search yaws, rank candidates, or check for obstructions/other
    buildings. Raises ValueError if new_connector_profile at
    new_building_yaw would not actually face the required opposite
    direction (a cheap sanity check, not a substitute for calling
    connectors_are_compatible()/candidate_yaws_for_normal() to choose
    new_building_yaw in the first place). Confirm feasibility with a
    real world.testConveyorBelt dry run after placement - this is a
    geometry primitive, not a guarantee.
    """
    _, predicted_normal = predict_connector_world_state(
        new_connector_profile, Position(x=0.0, y=0.0, z=0.0), new_building_yaw
    )
    if not connectors_are_compatible(target_connector_normal, predicted_normal):
        raise ValueError(
            "new_connector_profile at new_building_yaw would not face the required "
            "opposite direction from target_connector_normal - try a different "
            "new_building_yaw (see candidate_yaws_for_normal())"
        )

    target_point = _add(target_connector_position, _scale(target_connector_normal, clearance_distance))
    rotated_local_offset = rotate_yaw(new_connector_profile.local_position, new_building_yaw)
    return _sub(target_point, rotated_local_offset)


@dataclass(frozen=True)
class RadialFoundationPlacement:
    """One foundation's placement in a rotated ring - x/y/z + yaw_degrees
    are a direct candidate for world.placeBuilding's position/yaw params
    (pair with ignoreGroundTrace=true and this position's z as the
    literal z, same determinism-first pattern as every other placement
    in this project)."""

    position: Position
    yaw_degrees: float


def solve_outer_touching_ring_tile_count(foundation_size: float, target_outer_radius: float) -> int:
    """Nearest whole tile_count for compute_outer_touching_ring() such
    that its returned outer_radius (where consecutive tiles' outer
    corners touch - see that function's docstring) lands as close as
    possible to target_outer_radius. tile_count must be a whole number,
    so call compute_outer_touching_ring() with the returned value and
    read its actual outer_radius back rather than assuming it exactly
    equals target_outer_radius.

    Clamped to a minimum of 3 (the ring's own minimum, see
    compute_outer_touching_ring) - a target_outer_radius smaller than
    what 3 tiles can produce (foundation_size / sqrt(3)) just returns 3,
    the smallest possible ring, rather than raising.

    Raises ValueError if foundation_size <= 0 or target_outer_radius <= 0.
    """
    if foundation_size <= 0:
        raise ValueError("foundation_size must be positive")
    if target_outer_radius <= 0:
        raise ValueError("target_outer_radius must be positive")

    ratio = (foundation_size / 2.0) / target_outer_radius
    if ratio >= 1.0:
        return 3
    return max(3, round(math.pi / math.asin(ratio)))


def compute_outer_touching_ring(
    center: Position,
    foundation_size: float,
    tile_count: int,
) -> Tuple[List[RadialFoundationPlacement], float]:
    """Positions/yaws for tile_count square foundations of
    foundation_size arranged so each tile's OUTER edge exactly spans the
    chord between two vertices of a regular tile_count-gon of
    circumradius outer_radius - i.e. consecutive tiles' outer corners
    coincide exactly, the classic "corner-touching circular foundation"
    technique. This is the community pattern the user is asking about:
    interactive players approximate it by hand (small rotations near the
    center, extra rings snapped on further out to refine the angle)
    specifically because the in-game build gun only rotates in coarse
    scroll increments. That workaround is unnecessary here:
    world.placeBuilding's "yaw" param sets absolute rotation directly
    (SetActorRotation, confirmed since the "Rotation" fix in
    docs/placement-lessons.md - NOT the player's incremental Scroll()),
    so every tile's exact center position and yaw can be solved in one
    closed-form pass instead of approximated by hand.

    Derivation: tile_count points evenly spaced around center at radius
    outer_radius form a regular polygon; the chord between consecutive
    points is set equal to foundation_size (a full edge-length), which
    IS each tile's outer edge. Each tile then extends INWARD from its
    outer chord by foundation_size, so for a small tile_count the tiles
    necessarily overlap near the center (harmless - this is exactly what
    the manual "small angles near the center" technique also produces,
    just computed exactly here instead of eyeballed; only the OUTER
    boundary needs to read as a clean circle). Filling the rest of the
    platform's interior is ordinary square-foundation gridding - no new
    geometry needed for that part.

    Returns (placements, outer_radius) - outer_radius is the real
    circumradius actually produced by this exact tile_count
    (foundation_size / (2 * sin(pi / tile_count))), report it back to
    the user as "here's the actual radius this tile_count produces"
    since target_outer_radius (if you used
    solve_outer_touching_ring_tile_count() to get tile_count) is
    unlikely to land on an exact whole-tile_count solution.

    Raises ValueError if tile_count < 3 or foundation_size <= 0.

    IMPORTANT when feeding these placements to world.placeBuilding:
    pass gridSnapSize=0. The RPC's default (100, a 1m grid snap) silently
    rounds these deliberately off-grid coordinates and destroys the
    touching-corner property - confirmed live (2026-08-30): the same
    ring came out visibly rough/gapped with the default snap, then exact
    to a fraction of a millimeter with gridSnapSize=0. Also pass
    ignoreAimLocation=true - a ring built away from the player's literal
    look direction can hit "Invalid aim location!" otherwise. See
    docs/placement-lessons.md's "gridSnapSize" section.
    """
    if tile_count < 3:
        raise ValueError("tile_count must be at least 3")
    if foundation_size <= 0:
        raise ValueError("foundation_size must be positive")

    half_delta = math.pi / tile_count
    outer_radius = (foundation_size / 2.0) / math.sin(half_delta)
    apothem = outer_radius * math.cos(half_delta)
    center_radius = apothem - foundation_size / 2.0

    delta_theta = 2.0 * half_delta
    placements = []
    for i in range(tile_count):
        theta = (i + 0.5) * delta_theta
        placements.append(
            RadialFoundationPlacement(
                position=Position(
                    x=center.x + center_radius * math.cos(theta),
                    y=center.y + center_radius * math.sin(theta),
                    z=center.z,
                ),
                yaw_degrees=math.degrees(theta) % 360.0,
            )
        )
    return placements, outer_radius


def ring_inner_edge_radius(foundation_size: float, tile_count: int) -> float:
    """The distance from center to compute_outer_touching_ring()'s
    tiles' INNER (center-facing) edge - i.e. the boundary of the "hole"
    that still needs filling once the ring itself is built. Same
    validity/derivation as compute_outer_touching_ring (this is just its
    apothem minus one more foundation_size) - kept as a separate small
    function rather than a third return value so existing callers of
    compute_outer_touching_ring aren't affected by adding it.

    Raises ValueError if tile_count < 3 or foundation_size <= 0.
    """
    if tile_count < 3:
        raise ValueError("tile_count must be at least 3")
    if foundation_size <= 0:
        raise ValueError("foundation_size must be positive")

    half_delta = math.pi / tile_count
    outer_radius = (foundation_size / 2.0) / math.sin(half_delta)
    apothem = outer_radius * math.cos(half_delta)
    return apothem - foundation_size


def compute_disk_fill_grid(
    center: Position,
    tile_size: float,
    disk_radius: float,
    max_reach_radius: float = None,
) -> List[Position]:
    """Center positions for an axis-aligned square grid of tile_size
    that fully covers a disk of disk_radius around center, using the
    MINIMUM set of grid cells that guarantees no gaps: a cell is
    included if its square footprint intersects the disk at all (the
    closest point on the cell to center is within disk_radius), excluded
    otherwise. This is the standard "fill the interior with an ordinary
    grid, letting it overlap the boundary where needed" half of the
    classic circular-platform technique - pair with
    compute_outer_touching_ring() (the boundary ring) and
    ring_inner_edge_radius() (to get disk_radius so the grid meets the
    ring with no gap and minimal overlap).

    max_reach_radius (optional): additionally excludes any included cell
    whose FARTHEST corner from center would exceed this radius - use the
    ring's real outer_radius here to cap how far grid tiles poke past
    the ring's own visible boundary. Confirmed live (2026-08-30, user
    feedback): without this, a cell near the boundary can be included
    because its NEAREST corner is within disk_radius while its farthest
    corner sticks out past the ring's outer edge (a square touching a
    circle at a corner, not a face, is the worst case - its opposite
    corner can overshoot by up to 2*tile_size*sqrt(2)/2 beyond
    disk_radius). Applying this filter can, in principle, drop a cell
    that was the only thing covering some sliver of the disk - always
    re-verify full coverage (ring union fill) after using it, same as
    every other geometry primitive in this module; if a real gap
    appears, that specific sliver needs a smaller/different piece (e.g.
    a half-foundation) rather than a full tile_size cell, which is
    outside this function's scope.

    The grid is aligned so ONE CELL IS CENTERED AT `center`, not a grid
    vertex - an arbitrary but reasonable choice; a circle has no natural
    alignment with a square grid at any offset, so this isn't "more
    correct" than a vertex-centered grid, just a fixed, predictable
    convention.

    Returns cell CENTER positions only, z taken from center.z - no yaw
    is returned because an axis-aligned fill needs none (unlike the
    ring, whose tiles are individually rotated to point radially); place
    every returned position with yaw=0 (or any multiple of 90, they're
    equivalent for a square).

    Raises ValueError if tile_size <= 0, disk_radius <= 0, or
    max_reach_radius is given and <= disk_radius (it must be at least as
    large as disk_radius or every boundary cell would be excluded).
    """
    if tile_size <= 0:
        raise ValueError("tile_size must be positive")
    if disk_radius <= 0:
        raise ValueError("disk_radius must be positive")
    if max_reach_radius is not None and max_reach_radius <= disk_radius:
        raise ValueError("max_reach_radius must be greater than disk_radius")

    half = tile_size / 2.0
    max_index = math.ceil(disk_radius / tile_size) + 1

    positions = []
    for i in range(-max_index, max_index + 1):
        for j in range(-max_index, max_index + 1):
            local_x = i * tile_size
            local_y = j * tile_size
            closest_x = min(max(0.0, local_x - half), local_x + half)
            closest_y = min(max(0.0, local_y - half), local_y + half)
            if math.hypot(closest_x, closest_y) > disk_radius:
                continue
            if max_reach_radius is not None:
                farthest_x = max(abs(local_x - half), abs(local_x + half))
                farthest_y = max(abs(local_y - half), abs(local_y + half))
                if math.hypot(farthest_x, farthest_y) > max_reach_radius:
                    continue
            positions.append(Position(x=center.x + local_x, y=center.y + local_y, z=center.z))
    return positions


def group_producers_by_flow_capacity(flow_rates: List[float], tier_flow_limit: float) -> List[List[int]]:
    """Greedily groups producers (by index into flow_rates, in the
    given order) into clusters whose combined flow stays within
    tier_flow_limit - "rows of pumps that feed pipe junctions until
    each predicted pipe is saturated" (added 2026-08-31, explicit user
    request for a water-pump-field planner, but deliberately generic:
    "producers" can be individual pumps at one level, or already-merged
    junction points feeding a higher trunk tier at the next level up -
    same data shape either way, just a position + a flow rate).

    Generalizes satisfactory_ai.pipes.max_producers_per_pipe (which
    assumes every producer has the SAME rate) to handle mixed rates too
    - e.g. grouping rows of different sizes, or a mix of pumps and
    other liquid producers. Greedy and order-preserving: fills the
    current group until the NEXT producer would push it over
    tier_flow_limit, then starts a new group - not a bin-packing
    optimizer (does not reorder producers to minimize group count).
    Reorder flow_rates yourself first if a different grouping strategy
    is wanted; this function makes no attempt to find an optimal
    packing.

    Raises ValueError if tier_flow_limit <= 0, any single flow_rates[i]
    is <= 0 (not a meaningful producer rate), or any single
    flow_rates[i] alone exceeds tier_flow_limit (that producer can
    never fit in a group of this tier at all - needs a larger tier or
    its own dedicated pipe).
    """
    if tier_flow_limit <= 0:
        raise ValueError(f"tier_flow_limit must be positive, got {tier_flow_limit}")

    groups: List[List[int]] = []
    current_group: List[int] = []
    current_total = 0.0
    for i, rate in enumerate(flow_rates):
        if rate <= 0:
            raise ValueError(f"flow_rates[{i}] must be positive, got {rate}")
        if rate > tier_flow_limit:
            raise ValueError(
                f"flow_rates[{i}] ({rate}) alone exceeds tier_flow_limit ({tier_flow_limit}) - "
                "needs a larger tier or its own dedicated pipe"
            )
        if current_group and current_total + rate > tier_flow_limit:
            groups.append(current_group)
            current_group = []
            current_total = 0.0
        current_group.append(i)
        current_total += rate
    if current_group:
        groups.append(current_group)
    return groups


@dataclass(frozen=True)
class SharedSupportColumn:
    """One X/Y support point shared by multiple routed systems at their
    own Z levels - added 2026-08-31 per explicit user follow-up ("there
    are stackable support structures for pipes and belts that could
    help with intentional routing of both and provide snap points").

    Real, confirmed recipes exist for exactly this:
    `Recipe_PipeSupportStackable`, `Recipe_ConveyorPoleStackable`, and
    `Recipe_HyperPoleStackable` (Content/FactoryGame/Recipes/Buildings/
    - confirmed present on disk) each let a player stack multiple
    copies at the SAME X/Y footprint via the build gun's own drag/"Zoop"
    mechanic (`AFGStackablePoleHologram::CreateZoopInstances`,
    `GetStackHeight()` - a real per-tier vertical increment). This
    project does NOT yet drive that Zoop-stacking mechanic via an RPC
    (a real, separate follow-up, not attempted here) - this dataclass
    only captures the LAYOUT idea (one shared column position, multiple
    Z levels) so a caller can place separate pole instances at each
    level today, or drive real stacking later once that RPC exists,
    without changing the geometry.
    """

    x: float
    y: float
    pipe_z: Optional[float] = None
    power_z: Optional[float] = None
    belt_z: Optional[float] = None


def plan_shared_support_columns(
    start: Position,
    end: Position,
    max_segment_length: float,
    pipe_z: Optional[float] = None,
    power_z: Optional[float] = None,
    belt_z: Optional[float] = None,
) -> List[SharedSupportColumn]:
    """compute_waypoint_positions() for the shared X/Y column positions,
    then attaches whichever real Z level(s) the caller supplies for
    each routed system - added 2026-08-31, see SharedSupportColumn's
    doc comment for the real stackable-support recipes this models.

    Pass only the systems actually being routed through this corridor
    (e.g. pipe_z and power_z, leaving belt_z=None) - a level left None
    means "not routed here," not "routed at z=0."

    Raises ValueError if max_segment_length <= 0 (same as
    compute_waypoint_positions) or if pipe_z, power_z, and belt_z are
    ALL None (nothing to route - not a meaningful call).
    """
    if pipe_z is None and power_z is None and belt_z is None:
        raise ValueError("at least one of pipe_z/power_z/belt_z must be given")
    waypoints = compute_waypoint_positions(start, end, max_segment_length)
    return [
        SharedSupportColumn(x=p.x, y=p.y, pipe_z=pipe_z, power_z=power_z, belt_z=belt_z)
        for p in waypoints
    ]


def compute_waypoint_positions(start: Position, end: Position, max_segment_length: float) -> List[Position]:
    """Evenly-spaced waypoint positions from start to end (inclusive of
    both endpoints) such that no consecutive pair is farther apart than
    max_segment_length. Generic geometry, not belt- or power-specific -
    used by satisfactory_ai.conveyors and satisfactory_ai.power as
    candidate anchor points for chaining multiple segments (conveyor
    poles or power poles respectively) when a route exceeds one
    segment's real distance limit. Pure geometry - does not place
    anything, does not know about terrain/obstacles, and does not
    choose max_segment_length (pass a real queried limit, e.g.
    ConveyorBeltTier.max_spline_length or PowerLineLimits.max_length,
    or something smaller for margin).

    Raises ValueError if max_segment_length <= 0.
    """
    if max_segment_length <= 0:
        raise ValueError("max_segment_length must be positive")

    dx = end.x - start.x
    dy = end.y - start.y
    dz = end.z - start.z
    total_distance = math.sqrt(dx * dx + dy * dy + dz * dz)

    if total_distance <= max_segment_length:
        return [start, end]

    segment_count = math.ceil(total_distance / max_segment_length)
    return [
        Position(
            x=start.x + dx * (i / segment_count),
            y=start.y + dy * (i / segment_count),
            z=start.z + dz * (i / segment_count),
        )
        for i in range(segment_count + 1)
    ]
