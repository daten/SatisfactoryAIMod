"""Conveyor belt tier selection and routing toolkit (added 2026-08-25).

Companion to satisfactory_ai.layout, same "toolkit, not solver"
posture: every function here answers one question about
ALREADY-QUERIED data for an agent to call - none of them fetch
telemetry, call any RPC, or decide a full route/plan on their own. See
ConveyorBeltTier's doc comment (satisfactory_ai.models) and
docs/telemetry-protocol.md's conveyorBeltTiers section for why `speed`
is FactoryGame's own raw AFGBuildableConveyorBase::GetSpeed() value
rather than an items-per-minute figure - no items-per-minute
conversion is assumed or implemented here, deliberately, since the
exact conversion has not been confirmed against the game's own
displayed numbers.

Multi-segment routing background (2026-08-25): a single
ConstructConveyorBelt/world.connectConveyor call only performs the
proven two-click start/end snap - it cannot bend around an obstacle or
exceed one tier's maxSplineLength within one segment (that needs the
real multi-click SHBS_AdjustPole flow, not implemented). The
recommended pattern for a route that's too long or needs to bend is
INSTEAD to chain multiple straight segments through intermediate
buildables - ConstructConveyorBelt's source/dest are not required to
be machines (belts themselves expose the same UFGFactoryConnectionComponent
type), so a real Recipe_ConveyorPole (confirmed present on disk,
`/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorPole.Recipe_ConveyorPole_C`)
placed at each waypoint via world.placeBuilding, then a
world.connectConveyor call between each consecutive pair, should work
- NOT yet live-verified (only machine-to-machine single-segment
connections have been tested so far).
satisfactory_ai.layout.compute_waypoint_positions() (re-exported here)
computes candidate waypoint positions for this pattern; it does not
place anything. See satisfactory_ai.power for the equivalent pattern
for power lines/poles.
"""

from __future__ import annotations

import math
from typing import List, Optional, Tuple

from .layout import compute_waypoint_positions  # noqa: F401 - re-exported, generic geometry now lives in layout.py
from .models import ConveyorBeltTier, Position


def select_cheapest_sufficient_tier(
    tiers: List[ConveyorBeltTier], minimum_speed: float
) -> Optional[ConveyorBeltTier]:
    """Among tiers whose speed >= minimum_speed, returns the one with
    the smallest speed (the "cheapest sufficient" tier) - or None if no
    tier meets minimum_speed at all.

    A single, pure selection - not a plan. The caller decides what
    minimum_speed should be (in whatever unit world.conveyorBeltTiers'
    speed values turn out to be), and is expected to validate the
    chosen tier's recipeClass live (e.g. a world.testConveyorBelt dry
    run) before treating it as correct.
    """
    candidates = [t for t in tiers if t.speed >= minimum_speed]
    if not candidates:
        return None
    return min(candidates, key=lambda t: t.speed)


def is_straight_segment_feasible(
    start: Position, end: Position, tier: ConveyorBeltTier
) -> Tuple[bool, str]:
    """Would a single straight belt segment of this tier plausibly
    reach from start to end? Checks 3D distance against
    tier.max_spline_length and the incline angle (from the horizontal
    run, matching AFGConveyorBeltHologram's own "maximum incline of the
    conveyor belt (degrees)" framing) against tier.max_incline_degrees.

    A geometry pre-check only, NOT a substitute for a live
    world.testConveyorBelt dry run - it doesn't know about obstacles,
    clearance, or the auto-router's own bend-radius behavior for
    non-collinear connectors (see docs/demo-production-chain.md's
    connector-geometry findings). Returns (True, "") if both limits are
    unknown (None) for this tier - can't rule it out without data.
    """
    dx = end.x - start.x
    dy = end.y - start.y
    dz = end.z - start.z
    horizontal_run = math.hypot(dx, dy)
    distance_3d = math.sqrt(dx * dx + dy * dy + dz * dz)

    if tier.max_spline_length is not None and distance_3d > tier.max_spline_length:
        return False, f"distance {distance_3d:.1f} exceeds maxSplineLength {tier.max_spline_length:.1f}"

    if tier.max_incline_degrees is not None:
        incline_degrees = math.degrees(math.atan2(abs(dz), horizontal_run)) if horizontal_run > 0 else 90.0
        if incline_degrees > tier.max_incline_degrees:
            return False, f"incline {incline_degrees:.1f} degrees exceeds maxInclineDegrees {tier.max_incline_degrees:.1f}"

    return True, ""


# compute_waypoint_positions moved to satisfactory_ai.layout (2026-08-25)
# - it's generic geometry, not belt-specific, and satisfactory_ai.power
# needs the exact same function. Still importable from here for
# backward compatibility (see the import above).
