"""Production-rate and clock-speed balancing math (added 2026-08-30).

Deterministic arithmetic only - per CLAUDE.md's "Deterministic solver
responsibilities" ("production arithmetic, recipe dependency
calculations... optimization, resource allocation") and its explicit
instruction not to use an LLM for this. Nothing here queries or mutates
game state; every function takes explicit real numbers (pulled from
world.recipeCatalog/world.buildableCatalog by the caller) and returns a
computed answer or raises ValueError - same "no hidden defaults, no
silent guessing" contract as satisfactory_ai.layout.

Built for a specific planned test (a row of N upstream machines feeding
a row of M downstream machines via a shared manifold, both rows
individually clock-speed-tunable, sized against a single extractor's
output) but the primitives are generic - reusable for any two-stage
balancing problem, not hardcoded to copper/smelters/constructors.
"""

from __future__ import annotations

import math
from dataclasses import dataclass
from typing import Optional

# Real, standard Satisfactory constants - NOT read from this project's
# own verified telemetry (the game wasn't running when this module was
# written), flagged here so a future session knows to confirm them live
# rather than trust this file blindly. See world.buildableCatalog's
# potentialShardSlots gap (AIModFunctionLibrary.h's LogBuildableCatalogAsJson
# doc comment) and world.installPowerShard's doc comment for the exact
# gap this constant papers over.
DEFAULT_MAX_CLOCK_PERCENT_UNSHARDED = 100.0
DEFAULT_MIN_CLOCK_PERCENT = 1.0
ASSUMED_SHARD_SLOTS = 3
ASSUMED_PERCENT_PER_SHARD = 50.0  # UFGPowerShardDescriptor::GetBoostValue() - not yet read live for this project


def max_clock_percent_for_shards(shard_count: int, percent_per_shard: float = ASSUMED_PERCENT_PER_SHARD) -> float:
    """Max overclock percent achievable with shard_count Power Shards
    installed (world.installPowerShard). Pure arithmetic on the ASSUMED
    per-shard boost - confirm the real value live via
    UFGPowerShardDescriptor::GetBoostValue() (not yet exposed by this
    project's RPC layer) or by installing one shard and reading
    world.setClockSpeed's own "valid range" error message, before
    trusting this for a real build.

    Raises ValueError if shard_count is negative.
    """
    if shard_count < 0:
        raise ValueError("shard_count must not be negative")
    return DEFAULT_MAX_CLOCK_PERCENT_UNSHARDED + shard_count * percent_per_shard


@dataclass(frozen=True)
class RecipeRate:
    """A recipe's real per-machine rate at 100% clock, derived from
    world.recipeCatalog's raw ingredients/products/manufacturingDuration
    - never guessed. Construct via from_recipe_catalog_entry(), not by
    hand, so the amount/duration -> per-minute conversion can't drift.
    """

    ingredient_amount: float
    product_amount: float
    duration_seconds: float

    @classmethod
    def from_recipe_catalog_entry(cls, ingredient_amount: float, product_amount: float, duration_seconds: float) -> "RecipeRate":
        if duration_seconds <= 0:
            raise ValueError("duration_seconds must be positive")
        if ingredient_amount < 0 or product_amount < 0:
            raise ValueError("ingredient_amount/product_amount must not be negative")
        return cls(ingredient_amount=ingredient_amount, product_amount=product_amount, duration_seconds=duration_seconds)

    @property
    def ingredient_per_min_at_100(self) -> float:
        """How much of the primary ingredient ONE machine consumes per
        minute at 100% clock. For a single-ingredient recipe pass its
        amount directly; for a multi-ingredient recipe call this once
        per ingredient of interest (this dataclass models one
        ingredient/product pair at a time, matching the copper line's
        recipes - both Recipe_IngotCopper and Recipe_Wire are single-
        ingredient)."""
        return self.ingredient_amount * (60.0 / self.duration_seconds)

    @property
    def product_per_min_at_100(self) -> float:
        """How much of the primary product ONE machine outputs per
        minute at 100% clock."""
        return self.product_amount * (60.0 / self.duration_seconds)


@dataclass(frozen=True)
class ClockRequirement:
    """Result of solving for the clock% a row of machines needs to hit
    a target throughput."""

    target_rate_per_min: float
    machine_count: int
    required_clock_percent: float
    feasible: bool
    max_achievable_rate_per_min: float


def required_clock_percent_for_target_rate(
    target_rate_per_min: float,
    machine_count: int,
    base_rate_per_min_per_machine_at_100: float,
    max_clock_percent: float = DEFAULT_MAX_CLOCK_PERCENT_UNSHARDED,
    min_clock_percent: float = DEFAULT_MIN_CLOCK_PERCENT,
) -> ClockRequirement:
    """What clock% does EVERY machine in a row of machine_count need to
    run at (all machines the same speed) so the row's combined
    throughput exactly equals target_rate_per_min?

    Generic - base_rate_per_min_per_machine_at_100 can be an
    ingredient-consumption rate (sizing a downstream row against a known
    upstream output) or a product-output rate (sizing an upstream row
    against a known downstream demand) - the caller decides which by
    which RecipeRate property it passes in.

    feasible=False (with max_achievable_rate_per_min reporting the real
    ceiling) if target_rate_per_min exceeds what machine_count machines
    can sustain even at max_clock_percent - this is a real answer, not
    an error, since "the target is unreachable with this machine count"
    is exactly the kind of finding a caller needs before committing to a
    build (raise the machine count, or throttle the upstream rate
    instead).

    Raises ValueError for non-positive machine_count/base_rate/target,
    or if max_clock_percent < min_clock_percent.
    """
    if machine_count <= 0:
        raise ValueError("machine_count must be positive")
    if base_rate_per_min_per_machine_at_100 <= 0:
        raise ValueError("base_rate_per_min_per_machine_at_100 must be positive")
    if target_rate_per_min < 0:
        raise ValueError("target_rate_per_min must not be negative")
    if max_clock_percent < min_clock_percent:
        raise ValueError("max_clock_percent must be >= min_clock_percent")

    max_achievable = machine_count * base_rate_per_min_per_machine_at_100 * (max_clock_percent / 100.0)
    required_percent = (target_rate_per_min / (machine_count * base_rate_per_min_per_machine_at_100)) * 100.0
    feasible = min_clock_percent <= required_percent <= max_clock_percent

    return ClockRequirement(
        target_rate_per_min=target_rate_per_min,
        machine_count=machine_count,
        required_clock_percent=required_percent,
        feasible=feasible,
        max_achievable_rate_per_min=max_achievable,
    )


@dataclass(frozen=True)
class TwoStageLinePlan:
    """Full balance plan for {miner} -> {stage_a row} -> {stage_b row}.

    ore_rate_per_min is what the extractor should actually be clocked to
    produce - the binding constraint of the whole line, since a fixed
    machine-count row downstream has a real throughput ceiling the
    extractor cannot be allowed to exceed without overflowing (per
    "ensuring there are no gaps" - wait, wrong module's phrase, but same
    idea: never plan for material to have nowhere to go).
    """

    ore_rate_per_min: float
    extractor_clock_percent: float
    stage_a: ClockRequirement
    stage_b: ClockRequirement
    limited_by: str  # "extractor_max_clock" | "stage_a_max_clock" | "stage_b_max_clock" | "requested_rate"


def plan_two_stage_line(
    extractor_base_rate_per_min_at_100: float,
    extractor_max_clock_percent: float,
    stage_a_machine_count: int,
    stage_a_base_output_per_min_at_100: float,
    stage_a_max_clock_percent: float,
    stage_b_machine_count: int,
    stage_b_base_input_per_min_at_100: float,
    stage_b_max_clock_percent: float,
    requested_ore_rate_per_min: Optional[float] = None,
) -> TwoStageLinePlan:
    """Solves the whole {extractor} -> {stage A row} -> {stage B row}
    chain at once: finds the MAXIMUM ore rate the two fixed-size
    machine rows can jointly sustain (both stages at their own max
    clock simultaneously), then clamps to the extractor's own max
    output and to requested_ore_rate_per_min if given (pass None to
    just get the maximum feasible rate - "take full advantage of the
    extractor's output rate" reads either as "run the extractor as fast
    as this line can use" or "run the extractor flat out and size the
    line to match"; passing None answers the FIRST reading, which is
    the one a fixed machine count can actually guarantee no material is
    wasted for).

    stage_a_base_output_per_min_at_100 is stage A's PRODUCT rate (e.g.
    a Smelter's Copper Ingot output); stage_b_base_input_per_min_at_100
    is stage B's INGREDIENT consumption rate (e.g. a Constructor's
    Copper Ingot consumption) - these are typically NOT the same number
    even for a recipe pair that looks 1:1 on paper, because the two
    recipes' own cycle times differ (confirmed for this project's actual
    copper line: a Smelter outputs an ingot every 2s, a Constructor only
    consumes one every 4s - a naive equal machine count under-uses the
    downstream row unless its clock is raised to compensate; this
    function does that compensation, not the caller).
    """
    stage_a_ceiling = stage_a_machine_count * stage_a_base_output_per_min_at_100 * (stage_a_max_clock_percent / 100.0)
    stage_b_ceiling = stage_b_machine_count * stage_b_base_input_per_min_at_100 * (stage_b_max_clock_percent / 100.0)
    extractor_ceiling = extractor_base_rate_per_min_at_100 * (extractor_max_clock_percent / 100.0)

    candidates = [
        (stage_a_ceiling, "stage_a_max_clock"),
        (stage_b_ceiling, "stage_b_max_clock"),
        (extractor_ceiling, "extractor_max_clock"),
    ]
    if requested_ore_rate_per_min is not None:
        if requested_ore_rate_per_min < 0:
            raise ValueError("requested_ore_rate_per_min must not be negative")
        candidates.append((requested_ore_rate_per_min, "requested_rate"))

    ore_rate, limited_by = min(candidates, key=lambda c: c[0])

    extractor_clock_percent = (ore_rate / extractor_base_rate_per_min_at_100) * 100.0
    stage_a_req = required_clock_percent_for_target_rate(
        ore_rate, stage_a_machine_count, stage_a_base_output_per_min_at_100, stage_a_max_clock_percent
    )
    stage_b_req = required_clock_percent_for_target_rate(
        ore_rate, stage_b_machine_count, stage_b_base_input_per_min_at_100, stage_b_max_clock_percent
    )

    return TwoStageLinePlan(
        ore_rate_per_min=ore_rate,
        extractor_clock_percent=extractor_clock_percent,
        stage_a=stage_a_req,
        stage_b=stage_b_req,
        limited_by=limited_by,
    )
