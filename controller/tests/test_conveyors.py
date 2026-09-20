"""Tests for satisfactory_ai.conveyors / the conveyorBeltTiers parser.

Uses tests/fixtures/conveyor_belt_tiers.json - hand-written SYNTHETIC
speed values (100/200/450), NOT real queried game data (this project
hasn't yet confirmed real per-tier AFGBuildableConveyorBase::GetSpeed()
values live - see satisfactory_ai.models.ConveyorBeltTier's doc
comment). Only the shape/ordering behavior is under test here, not any
particular numeric belt-tier fact.

Run from the controller/ directory:
    python -m unittest discover -s tests -t . -v
"""

import unittest
from pathlib import Path

from satisfactory_ai.conveyors import (
    is_straight_segment_feasible,
    select_cheapest_sufficient_tier,
)
from satisfactory_ai.models import ConveyorBeltTier, Position
from satisfactory_ai.protocol import parse_conveyor_belt_tier_telemetry

FIXTURE_PATH = Path(__file__).resolve().parents[2] / "tests" / "fixtures" / "conveyor_belt_tiers.json"


class ParseConveyorBeltTierTelemetryTest(unittest.TestCase):
    def test_parses_fixture(self):
        telemetry = parse_conveyor_belt_tier_telemetry(FIXTURE_PATH.read_text(encoding="utf-8"))
        self.assertEqual(telemetry.protocol_version, 1)
        self.assertEqual(len(telemetry.tiers), 3)
        self.assertEqual(telemetry.tiers[0].speed, 100.0)
        self.assertIn("Mk1", telemetry.tiers[0].recipe_class)


class SelectCheapestSufficientTierTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        telemetry = parse_conveyor_belt_tier_telemetry(FIXTURE_PATH.read_text(encoding="utf-8"))
        cls.tiers = list(telemetry.tiers)

    def test_picks_smallest_tier_meeting_the_minimum(self):
        chosen = select_cheapest_sufficient_tier(self.tiers, minimum_speed=150.0)
        self.assertIsNotNone(chosen)
        self.assertEqual(chosen.speed, 200.0)

    def test_exact_match_is_sufficient(self):
        chosen = select_cheapest_sufficient_tier(self.tiers, minimum_speed=200.0)
        self.assertEqual(chosen.speed, 200.0)

    def test_returns_none_when_nothing_is_fast_enough(self):
        chosen = select_cheapest_sufficient_tier(self.tiers, minimum_speed=1000.0)
        self.assertIsNone(chosen)

    def test_zero_minimum_picks_the_slowest_tier(self):
        chosen = select_cheapest_sufficient_tier(self.tiers, minimum_speed=0.0)
        self.assertEqual(chosen.speed, 100.0)


class IsStraightSegmentFeasibleTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        telemetry = parse_conveyor_belt_tier_telemetry(FIXTURE_PATH.read_text(encoding="utf-8"))
        cls.tier = telemetry.tiers[0]  # maxSplineLength=5600.1, maxIncline=30 (fixture placeholders)

    def test_short_flat_segment_is_feasible(self):
        feasible, reason = is_straight_segment_feasible(
            Position(0.0, 0.0, 0.0), Position(1000.0, 0.0, 0.0), self.tier
        )
        self.assertTrue(feasible)
        self.assertEqual(reason, "")

    def test_too_long_segment_is_infeasible(self):
        feasible, reason = is_straight_segment_feasible(
            Position(0.0, 0.0, 0.0), Position(6000.0, 0.0, 0.0), self.tier
        )
        self.assertFalse(feasible)
        self.assertIn("distance", reason)

    def test_too_steep_segment_is_infeasible(self):
        # 10 units horizontal run, 1000 units of rise - ~89.4 degrees.
        feasible, reason = is_straight_segment_feasible(
            Position(0.0, 0.0, 0.0), Position(10.0, 0.0, 1000.0), self.tier
        )
        self.assertFalse(feasible)
        self.assertIn("incline", reason)

    def test_unknown_limits_cannot_be_ruled_out(self):
        unknown_tier = ConveyorBeltTier(recipe_class="x", buildable_class="y", speed=1.0)
        feasible, reason = is_straight_segment_feasible(
            Position(0.0, 0.0, 0.0), Position(999999.0, 0.0, 999999.0), unknown_tier
        )
        self.assertTrue(feasible)


if __name__ == "__main__":
    unittest.main()
