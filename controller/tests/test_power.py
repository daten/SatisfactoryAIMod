"""Tests for satisfactory_ai.power / the powerLineLimits parser.

Uses tests/fixtures/power_line_limits.json - hand-written SYNTHETIC
limit values (maxLength=2000.0 etc.), NOT real queried game data (this
project hasn't yet confirmed real AFGBuildableWire::mMaxLength values
live - see satisfactory_ai.models.PowerLineLimits' doc comment). Only
the shape/logic behavior is under test here, not any particular
numeric power-line fact.

Run from the controller/ directory:
    python -m unittest discover -s tests -t . -v
"""

import unittest
from pathlib import Path

from satisfactory_ai.models import Position
from satisfactory_ai.power import is_direct_connection_feasible
from satisfactory_ai.protocol import parse_power_line_limits

FIXTURE_PATH = Path(__file__).resolve().parents[2] / "tests" / "fixtures" / "power_line_limits.json"
MISSING_FIXTURE_JSON = '{"protocolVersion": 1}'


class ParsePowerLineLimitsTest(unittest.TestCase):
    def test_parses_fixture(self):
        limits = parse_power_line_limits(FIXTURE_PATH.read_text(encoding="utf-8"))
        self.assertIsNotNone(limits)
        self.assertEqual(limits.max_length, 2000.0)
        self.assertEqual(limits.max_power_tower_length, 4000.0)
        self.assertIn("PowerLine", limits.recipe_class)

    def test_returns_none_when_the_mod_could_not_resolve_the_cdo(self):
        limits = parse_power_line_limits(MISSING_FIXTURE_JSON)
        self.assertIsNone(limits)


class IsDirectConnectionFeasibleTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.limits = parse_power_line_limits(FIXTURE_PATH.read_text(encoding="utf-8"))

    def test_short_connection_is_feasible(self):
        feasible, reason = is_direct_connection_feasible(
            Position(0.0, 0.0, 0.0), Position(500.0, 0.0, 0.0), self.limits
        )
        self.assertTrue(feasible)
        self.assertEqual(reason, "")

    def test_too_far_connection_is_infeasible(self):
        feasible, reason = is_direct_connection_feasible(
            Position(0.0, 0.0, 0.0), Position(3000.0, 0.0, 0.0), self.limits
        )
        self.assertFalse(feasible)
        self.assertIn("distance", reason)

    def test_exact_max_length_is_feasible(self):
        feasible, _ = is_direct_connection_feasible(
            Position(0.0, 0.0, 0.0), Position(2000.0, 0.0, 0.0), self.limits
        )
        self.assertTrue(feasible)


if __name__ == "__main__":
    unittest.main()
