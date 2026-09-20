"""Power line distance/routing toolkit (added 2026-08-25).

Companion to satisfactory_ai.conveyors, same "toolkit, not solver"
posture and same motivation: the user asked whether power cables (like
conveyors) need distance-limit/intermediate-pole handling, and they
do. Every function here answers one question about ALREADY-QUERIED
data - none of them fetch telemetry, call any RPC, or chain a route on
their own.

Unlike ConveyorBeltTier.speed, PowerLineLimits.max_length is a
documented-unit ("[cm]") value directly comparable to a computed 3D
distance - no unknown-conversion caveat here. See PowerLineLimits' doc
comment (satisfactory_ai.models) and docs/telemetry-protocol.md's
powerLineLimits section.

Routing background: ConstructPowerConnection/world.connectPower's
source/dest were already generic before this - FindFreePowerConnection
searches any AFGBuildable for a free UFGPowerConnectionComponent via
GetComponents<>(), not hardcoded to machines - so a real power pole
(Recipe_PowerPoleMk1/Mk2/Mk3, confirmed present on disk) placed at
each satisfactory_ai.layout.compute_waypoint_positions() result via
world.placeBuilding, then world.connectPower between each consecutive
pair, should let an agent chain a connection past mMaxLength. NOT yet
live-verified - every power connection built so far has been one
direct machine-to-machine segment. Separately, per the user's own
earlier note this session (docs/conveyor-power-connection-research.md):
a machine's default single power connection slot may require routing
through a pole even for a SHORT connection, if the later-game
daisy-chain unlock isn't active in the current save - a real
constraint distinct from the mMaxLength distance question this module
addresses.
"""

from __future__ import annotations

import math
from typing import Tuple

from .layout import compute_waypoint_positions  # noqa: F401 - re-exported for convenience
from .models import PowerLineLimits, Position


def is_direct_connection_feasible(start: Position, end: Position, limits: PowerLineLimits) -> Tuple[bool, str]:
    """Would a single power line plausibly reach from start to end?
    Checks the 3D distance against limits.max_length.

    A geometry pre-check only, NOT a substitute for a live
    world.testPowerConnection dry run - it doesn't know about
    obstacles, whether either end even has a free power connection
    slot, or the pole-vs-daisy-chain constraint described in this
    module's docstring.
    """
    dx = end.x - start.x
    dy = end.y - start.y
    dz = end.z - start.z
    distance_3d = math.sqrt(dx * dx + dy * dy + dz * dz)

    if distance_3d > limits.max_length:
        return False, f"distance {distance_3d:.1f} exceeds maxLength {limits.max_length:.1f}"
    return True, ""
