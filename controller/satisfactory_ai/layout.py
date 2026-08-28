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
from typing import List, Tuple

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
