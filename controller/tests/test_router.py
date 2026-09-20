"""Tests for satisfactory_ai.router.

Fixtures are real geometries from the 2026-09-01/02 live sessions -
including ones that FAILED live and were fixed with jog mergers, so the
router must reproduce the fix that actually worked in the game.

Run from the controller/ directory:
    python -m unittest discover -s tests -t . -v
"""

import unittest

from satisfactory_ai.connector_db import ConnectorDb
from satisfactory_ai.models import Position
from satisfactory_ai.router import (
    Endpoint,
    RoutingError,
    direct_belt_feasible,
    route_connection,
)

BELT_MK4 = "/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorBeltMk4.Recipe_ConveyorBeltMk4_C"


def P(x, y, z=0.0):
    return Position(x=float(x), y=float(y), z=float(z))


class DirectFeasibility(unittest.TestCase):
    def test_proven_direct_pair_belt(self):
        # rodE1 out (7600,280100,501) south-facing -> scrE1 in
        # (7600,279100,501) north-facing: built first try live ("Straight").
        src = Endpoint("rodE1", P(7600, 280100, 501), P(0, -1))
        dst = Endpoint("scrE1", P(7600, 279100, 501), P(0, 1))
        ok, reason = direct_belt_feasible(src, dst)
        self.assertTrue(ok, reason)

    def test_short_run_rejected(self):
        # MFo out (6000,281600) -> A in (6000,281500): the 100-unit belt
        # class that failed repeatedly live.
        src = Endpoint("a", P(6000, 281600, 301), P(0, -1))
        dst = Endpoint("b", P(6000, 281500, 301), P(0, 1))
        ok, reason = direct_belt_feasible(src, dst)
        self.assertFalse(ok)
        self.assertIn("under reliable minimum", reason)

    def test_s_against_input_facing_rejected(self):
        # SpE0 south-out (7400,280100,301) -> plate2 north-facing input
        # (6800,279900,301): displacement is south-west but the input
        # wants entry heading SOUTH from above... actually the failure
        # live was the S-shape; entry heading (0,-1) vs displacement
        # (-600,-200) has positive dot, but the SOURCE normal (0,-1)
        # vs displacement is fine too - the live failure was curvature,
        # which the min-run rule doesn't catch. Use the geometry that IS
        # detectable: an input whose entry heading opposes the
        # displacement outright.
        src = Endpoint("src", P(4500, 275500, 331), P(0, -1))  # lift top, faces south
        dst = Endpoint("eib", P(5000, 275800, 401), P(0, -1))  # input wants entry heading north
        # displacement is north-east; source exits SOUTH -> opposes.
        ok, reason = direct_belt_feasible(src, dst)
        self.assertFalse(ok)
        self.assertIn("opposes", reason)

    def test_steep_rejected(self):
        src = Endpoint("a", P(0, 0, 0), P(0, 1))
        dst = Endpoint("b", P(0, 300, 300), P(0, -1))  # 45 degrees
        ok, reason = direct_belt_feasible(src, dst)
        self.assertFalse(ok)
        self.assertIn("incline", reason)


class JogPlanning(unittest.TestCase):
    """The lift-top -> EIB fix from 2026-09-02, reproduced by the router.

    Live: Mk4 lift top at (4500,275500,331) facing SOUTH; EIB concrete
    input at (5000,275800,401) facing SOUTH (entry heading north). The
    direct belt failed ("Invalid shape"); the fix that worked was a
    merger at (4500,275100,331) yaw0 - lift-south 300 into its north
    input, then east-out and a curve up to the EIB input.
    """

    def setUp(self):
        self.db = ConnectorDb()
        self.src = Endpoint("lift", P(4500, 275500, 331), P(0, -1))
        self.dst = Endpoint("eib", P(5000, 275800, 401), P(0, -1))

    def test_jog_merger_planned(self):
        plan = route_connection(self.db, self.src, self.dst, BELT_MK4)
        kinds = [op.kind for op in plan.ops]
        self.assertEqual(kinds, ["place", "belt", "belt"])
        jog = plan.ops[0]
        # Stand-off along the input's facing (south of the input).
        self.assertAlmostEqual(jog.position.x, 5000.0)
        self.assertAlmostEqual(jog.position.y, 275800.0 - 300.0)
        # Merger output must face the entry heading (north): yaw 90.
        self.assertEqual(jog.yaw, 90.0)
        # Final leg runs straight along the input's facing.
        final = plan.ops[2]
        self.assertAlmostEqual(final.dest_pin.x, 5000.0)
        self.assertAlmostEqual(final.dest_pin.y, 275800.0)


class RelayPlanning(unittest.TestCase):
    def test_long_run_gets_relays(self):
        db = ConnectorDb()
        # The plate2' -> west-rip2 idea from 2026-09-02: ~7400 units,
        # over one segment's max - needs at least one relay.
        src = Endpoint("plate2", P(10800, 280100, 501), P(0, -1))
        dst = Endpoint("rip2", P(3400, 278200, 301), P(0, 1))
        plan = route_connection(db, src, dst, BELT_MK4)
        places = [op for op in plan.ops if op.kind == "place"]
        belts = [op for op in plan.ops if op.kind == "belt"]
        self.assertGreaterEqual(len(places), 1)
        self.assertEqual(len(belts), len(places) + 1)
        # Every belt segment inside the max length.
        import math

        for b in belts:
            run = math.hypot(
                b.dest_pin.x - b.source_pin.x,
                b.dest_pin.y - b.source_pin.y,
            )
            self.assertLessEqual(run, 5600.0)


if __name__ == "__main__":
    unittest.main()
