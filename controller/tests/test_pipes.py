"""Tests for satisfactory_ai.pipes / the pipelineTiers parser.

Uses tests/fixtures/pipeline_tiers.json - hand-written SYNTHETIC
flowLimit/maxSplineLength/bendRadius/minBendRadius values (300/600,
5000.0, 100.0, 50.0), NOT real queried game data (pipe groundwork,
2026-08-25, has not been live-tested - see
satisfactory_ai.models.PipelineTier's doc comment). Only the
shape/ordering behavior is under test here, not any particular
numeric pipe-tier fact.

Run from the controller/ directory:
    python -m unittest discover -s tests -t . -v
"""

import unittest
from pathlib import Path

from satisfactory_ai.models import PipelineTier, Position
from satisfactory_ai.pipes import (
    is_straight_segment_feasible,
    select_cheapest_sufficient_tier,
)
from satisfactory_ai.protocol import parse_pipeline_tier_telemetry

FIXTURE_PATH = Path(__file__).resolve().parents[2] / "tests" / "fixtures" / "pipeline_tiers.json"


class ParsePipelineTierTelemetryTest(unittest.TestCase):
    def test_parses_fixture(self):
        telemetry = parse_pipeline_tier_telemetry(FIXTURE_PATH.read_text(encoding="utf-8"))
        self.assertEqual(telemetry.protocol_version, 1)
        self.assertEqual(len(telemetry.tiers), 2)
        self.assertEqual(telemetry.tiers[0].flow_limit, 300.0)
        self.assertIn("Recipe_Pipeline.", telemetry.tiers[0].recipe_class)
        self.assertIn("MK2", telemetry.tiers[1].recipe_class)


class SelectCheapestSufficientTierTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        telemetry = parse_pipeline_tier_telemetry(FIXTURE_PATH.read_text(encoding="utf-8"))
        cls.tiers = list(telemetry.tiers)

    def test_picks_smallest_tier_meeting_the_minimum(self):
        chosen = select_cheapest_sufficient_tier(self.tiers, minimum_flow=400.0)
        self.assertIsNotNone(chosen)
        self.assertEqual(chosen.flow_limit, 600.0)

    def test_exact_match_is_sufficient(self):
        chosen = select_cheapest_sufficient_tier(self.tiers, minimum_flow=300.0)
        self.assertEqual(chosen.flow_limit, 300.0)

    def test_returns_none_when_nothing_flows_enough(self):
        chosen = select_cheapest_sufficient_tier(self.tiers, minimum_flow=1000.0)
        self.assertIsNone(chosen)

    def test_zero_minimum_picks_the_smallest_tier(self):
        chosen = select_cheapest_sufficient_tier(self.tiers, minimum_flow=0.0)
        self.assertEqual(chosen.flow_limit, 300.0)


class IsStraightSegmentFeasibleTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        telemetry = parse_pipeline_tier_telemetry(FIXTURE_PATH.read_text(encoding="utf-8"))
        cls.tier = telemetry.tiers[0]  # maxSplineLength=5000.0 (fixture placeholder)

    def test_short_segment_is_feasible(self):
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

    def test_unknown_limit_cannot_be_ruled_out(self):
        unknown_tier = PipelineTier(recipe_class="x", buildable_class="y", flow_limit=1.0)
        feasible, reason = is_straight_segment_feasible(
            Position(0.0, 0.0, 0.0), Position(999999.0, 0.0, 999999.0), unknown_tier
        )
        self.assertTrue(feasible)


if __name__ == "__main__":
    unittest.main()
