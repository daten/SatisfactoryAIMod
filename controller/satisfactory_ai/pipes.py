"""Pipeline tier selection and routing toolkit (added 2026-08-25, pipe
groundwork - NOT YET LIVE-TESTED, see ConstructPipe's C++ doc comment).

Companion to satisfactory_ai.conveyors/power, same "toolkit, not
solver" posture: every function here answers one question about
ALREADY-QUERIED data for an agent to call - none of them fetch
telemetry, call any RPC, or decide a full route/plan on their own.

Unlike ConveyorBeltTier.speed, PipelineTier.flow_limit is a
documented-unit ("[m^3/s]") value - no unit-conversion caveat there.
But unlike PowerLineLimits.max_length (a simple straight-line distance
limit), PipelineTier.max_spline_length is a SPLINE length, not
necessarily a straight-line distance - the same caveat
is_straight_segment_feasible already carries for belts applies here
too: this is a geometry pre-check against straight-line distance, not
a guarantee about the real (possibly curved) construction path.
bend_radius/min_bend_radius are read but NOT checked here - unlike
belts, no incline-angle-vs-limit relationship for pipes has been
confirmed from source, so no incline check is implemented; treat any
steep vertical run as unverified until a live world.testPipe dry run
confirms it.

Routing background: ConstructPipe/world.connectPipe's source/dest are
not required to be machines - any AFGBuildable with a free
UFGPipeConnectionComponentBase works, same generic posture as
ConstructConveyorBelt/ConstructPowerConnection. However, UNLIKE belts
(Recipe_ConveyorPole) and power (Recipe_PowerPoleMk1/Mk2/Mk3), no
standalone Recipe_PipelineSupport/pole recipe was found on disk during
this groundwork pass, even though Build_PipelineSupport.uasset exists
as a buildable - AFGPipelineHologram appears to have its own
mDefaultPipelineSupportRecipe mechanism for auto-spawning supports
during a real multi-click placement instead. Whether the
place-a-pole-then-chain-connect pattern used by
satisfactory_ai.conveyors/power transfers directly to pipes is
therefore an OPEN QUESTION for the first live pipe test to answer, not
assumed here - compute_waypoint_positions() is still re-exported below
since the geometry itself is unaffected, but callers should not assume
placing a pipeline-support buildable independently is possible until
that's confirmed live.
"""

from __future__ import annotations

import math
from typing import List, Optional, Tuple

from .layout import compute_waypoint_positions  # noqa: F401 - re-exported for convenience
from .models import PipelineTier, Position


def select_cheapest_sufficient_tier(
    tiers: List[PipelineTier], minimum_flow: float
) -> Optional[PipelineTier]:
    """Among tiers whose flow_limit >= minimum_flow, returns the one
    with the smallest flow_limit (the "cheapest sufficient" tier) - or
    None if no tier meets minimum_flow at all.

    A single, pure selection - not a plan. The caller decides what
    minimum_flow should be (in the documented m^3/s unit), and is
    expected to validate the chosen tier's recipeClass live (e.g. a
    world.testPipe dry run) before treating it as correct.
    """
    candidates = [t for t in tiers if t.flow_limit >= minimum_flow]
    if not candidates:
        return None
    return min(candidates, key=lambda t: t.flow_limit)


def is_straight_segment_feasible(
    start: Position, end: Position, tier: PipelineTier
) -> Tuple[bool, str]:
    """Would a single straight pipe segment of this tier plausibly
    reach from start to end? Checks 3D distance against
    tier.max_spline_length only - see this module's docstring for why
    no incline check is implemented (unconfirmed from source, unlike
    belts).

    A geometry pre-check only, NOT a substitute for a live
    world.testPipe dry run - it doesn't know about obstacles,
    clearance, fluid type compatibility (UFGCDPipeFluidTypeMismatch),
    or the auto-router's own bend-radius behavior for non-collinear
    connectors. Returns (True, "") if max_spline_length is unknown
    (None) for this tier - can't rule it out without data.
    """
    dx = end.x - start.x
    dy = end.y - start.y
    dz = end.z - start.z
    distance_3d = math.sqrt(dx * dx + dy * dy + dz * dz)

    if tier.max_spline_length is not None and distance_3d > tier.max_spline_length:
        return False, f"distance {distance_3d:.1f} exceeds maxSplineLength {tier.max_spline_length:.1f}"

    return True, ""
