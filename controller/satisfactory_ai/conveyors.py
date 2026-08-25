"""Conveyor belt tier selection toolkit (added 2026-08-25).

Companion to satisfactory_ai.layout, same "toolkit, not solver"
posture: this answers "which of these ALREADY-QUERIED tiers is the
cheapest one that meets a minimum speed?" for an agent to call - it
does not fetch telemetry itself, does not call any RPC, and does not
decide what the desired rate should be. See ConveyorBeltTier's doc
comment (satisfactory_ai.models) and docs/telemetry-protocol.md's
conveyorBeltTiers section for why `speed` is FactoryGame's own raw
AFGBuildableConveyorBase::GetSpeed() value rather than an
items-per-minute figure - no items-per-minute conversion is assumed or
implemented here, deliberately, since the exact conversion has not been
confirmed against the game's own displayed numbers.
"""

from __future__ import annotations

from typing import List, Optional

from .models import ConveyorBeltTier


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
