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

Flow/headlift planning (added 2026-08-31, offline research/prep, NOT
YET LIVE-TESTED - see world.pipelinePumpTiers' RPC_REFERENCE.md entry
for the full mechanic writeup): required_parallel_pipes and
max_producers_per_pipe answer the FLOW budget question (a single pipe
segment has a hard m^3/s ceiling regardless of pumps - a fully
overclocked Pressurized Water Extractor can genuinely need 3-4 parallel
pipes, and a cluster of individual extractors can't all join one pipe
past that ceiling either). pump_required_for_elevation and
required_pumps_for_elevation answer the separate HEIGHT budget question
for LIQUIDS ONLY (confirmed from FGPipeNetwork.h source: liquid
networks track real per-group elevation/pump-height state in meters via
a genuine pressure-group simulation; gas has a fully separate physics
path with no elevation-tracking equivalent at all - never call the
pump-elevation functions for a gas pipeline). A real route may need
both independently - extra parallel pipes for flow AND pumps in series
along each one for elevation.
"""

from __future__ import annotations

import math
from typing import List, Optional, Tuple

from .layout import compute_waypoint_positions  # noqa: F401 - re-exported for convenience
from .models import PipelinePumpTier, PipelineTier, Position


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


def required_parallel_pipes(total_flow: float, tier: PipelineTier) -> int:
    """How many parallel pipes of this tier are needed to carry
    total_flow [m^3/s] end to end? Added 2026-08-31, offline research
    per explicit user request ("a fully overclocked pressurized well may
    require 3 or 4 parallel pipelines").

    A single AFGBuildablePipeline segment has a hard flow ceiling
    (PipelineTier.flow_limit) - it does not matter how many pumps are
    added, a pump only supplies headlift (see required_pumps_for_elevation
    below), never additional throughput (confirmed from
    AFGBuildablePipelinePump.h: GetDefaultFlowLimit() is itself capped
    "from the neighbouring pipes"). The only way to exceed one pipe's
    flow_limit is genuinely separate, parallel pipe runs, each with
    their own connectors at both ends - this function only answers "how
    many," not how to route or connect them (that's still the caller's
    job, same "toolkit not solver" posture as the rest of this module).

    Raises ValueError if tier.flow_limit <= 0 (nothing could ever get
    through) or total_flow < 0 (not a meaningful request).
    """
    if tier.flow_limit <= 0:
        raise ValueError(f"tier.flow_limit must be positive, got {tier.flow_limit}")
    if total_flow < 0:
        raise ValueError(f"total_flow must be >= 0, got {total_flow}")
    if total_flow == 0:
        return 0
    return math.ceil(total_flow / tier.flow_limit)


def max_producers_per_pipe(tier: PipelineTier, per_producer_flow: float) -> int:
    """How many equal-rate producers (e.g. identical Water Extractors at
    the same clock/purity) can share ONE pipe of this tier before
    exceeding its flow_limit? Added 2026-08-31, offline research per
    explicit user request ("a few of the individual extractors will
    maximize one pipe... they can't all be joined together").

    The inverse question from required_parallel_pipes: given a KNOWN
    per-producer rate, how large can one cluster be before it needs to
    split onto a second pipe? Returns 0 if even a single producer's flow
    already exceeds the tier's flow_limit on its own (that producer
    needs its own dedicated pipe, or a larger tier).

    Raises ValueError if tier.flow_limit <= 0 or per_producer_flow <= 0
    (not meaningful requests - a zero/negative producer rate isn't a
    real production rate).
    """
    if tier.flow_limit <= 0:
        raise ValueError(f"tier.flow_limit must be positive, got {tier.flow_limit}")
    if per_producer_flow <= 0:
        raise ValueError(f"per_producer_flow must be positive, got {per_producer_flow}")
    return math.floor(tier.flow_limit / per_producer_flow)


def pump_required_for_elevation(elevation_gain_meters: float) -> bool:
    """Does a run climbing elevation_gain_meters need a pump at all, for
    LIQUIDS? Added 2026-08-31, offline research per explicit user
    request ("if a liquid source comes from a higher elevation... pumps
    may not be necessary").

    True if elevation_gain_meters > 0 (net uphill - gravity does not
    help, a pump is needed to overcome it), False otherwise (flat or net
    downhill - gravity alone can move the fluid, matching
    AFGPipeNetwork.h's real liquid pressure-group simulation, which
    tracks HighestElevationZ/HighestPumpZ per group in meters).

    GAS DOES NOT APPLY HERE - confirmed from source
    (AFGPipeNetwork.h has a fully separate Gas physics path -
    TickPhysics_Gas/UpdatePressure_Gas/UpdateFlow_Gas - with no
    pressure-group/elevation-tracking equivalent to the liquid path's
    CreatePressureGroup/UpdatePressureGroups/FPressureGroup). Do not
    call this for a gas pipeline - per the user, gas flow isn't subject
    to the same elevation/headlift concern liquids are.

    This is a pure sign check, not a full pressure-group simulation -
    it doesn't know about multiple pumps already in the run, junctions
    that break the pressure group (AFGPipeNetwork.h's PipeJunction::
    ShouldBreakPressureGroup), or friction/viscosity losses along real
    pipe length. Use it as a first "do I even need to think about
    pumps here" filter, not a final answer for a complex multi-segment
    route.
    """
    return elevation_gain_meters > 0


def required_pumps_for_elevation(
    elevation_gain_meters: float, pump_tier: PipelinePumpTier, use_design_limit: bool = True
) -> int:
    """How many of this pump tier, in series, are needed to overcome a
    net uphill elevation_gain_meters for a LIQUID run? Added 2026-08-31,
    offline research per explicit user request.

    Returns 0 if elevation_gain_meters <= 0 (see pump_required_for_elevation
    - gravity alone suffices, no pump needed). Otherwise
    ceil(elevation_gain_meters / per-pump headlift), using
    pump_tier.design_head_lift by default (the safe/rated operating
    point) or pump_tier.max_head_lift if use_design_limit=False (the
    absolute ceiling - AFGBuildablePipelinePump.h documents this as
    "working outside of its specifications" territory, still functional
    but not the recommended target for a real build).

    GAS DOES NOT APPLY - same caveat as pump_required_for_elevation,
    do not call this for a gas pipeline.

    This answers a HEIGHT budget only, entirely separate from the FLOW
    budget answered by required_parallel_pipes/max_producers_per_pipe -
    a real route may need both extra parallel pipes (for flow) AND
    pumps in series along each one (for elevation), independently sized.
    Like the rest of this module, this doesn't know about real junction
    pressure-group breaks, friction, or multi-pump placement geometry -
    treat it as a first-pass budget check, confirm with a live
    world.testPipe-style construction once pumps are supported by this
    mod's RPC surface (not yet built as of this writing).

    Raises ValueError if pump_tier's chosen headlift limit is <= 0
    (a pump that provides no lift can never satisfy any positive
    elevation_gain_meters, however many are chained).
    """
    if elevation_gain_meters <= 0:
        return 0
    limit = pump_tier.design_head_lift if use_design_limit else pump_tier.max_head_lift
    if limit <= 0:
        raise ValueError(f"pump headlift limit must be positive, got {limit}")
    return math.ceil(elevation_gain_meters / limit)
