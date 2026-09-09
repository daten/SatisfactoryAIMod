"""Tests for satisfactory_ai.belt_route (waypoint lane planner).

Run from controller/:  python -m unittest tests.test_belt_route -v
"""

import unittest

from satisfactory_ai.models import Position
from satisfactory_ai.belt_route import (
    MAX_STRAIGHT_RUN,
    BeltLanePlan,
    plan_belt_lane,
    subdivide_path,
)
from satisfactory_ai.route_drc import Obstacle, check_route
from satisfactory_ai.router import Endpoint, RoutingError


def P(x, y, z=0.0):
    return Position(x=float(x), y=float(y), z=float(z))


def _end(bid, x, y, z=0.0):
    return Endpoint(buildable_id=bid, position=P(x, y, z), normal=P(1, 0, 0))


class BeltRouteTest(unittest.TestCase):
    def test_subdivide_long_span(self):
        pts = subdivide_path([P(0, 0, 0), P(3000, 0, 0)], max_run=1400.0)
        # 3000/1400 -> ceil 3 sub-spans -> 4 points, each 1000u
        self.assertEqual(len(pts), 4)
        for i in range(1, len(pts)):
            self.assertLessEqual(pts[i].x - pts[i - 1].x, 1400.0 + 1e-6)
        self.assertAlmostEqual(pts[0].x, 0.0)
        self.assertAlmostEqual(pts[-1].x, 3000.0)

    def test_subdivide_preserves_waypoints(self):
        pts = subdivide_path([P(0, 0), P(500, 0), P(1000, 0)], max_run=1400.0)
        self.assertEqual([p.x for p in pts], [0.0, 500.0, 1000.0])  # short spans untouched

    def test_plan_places_poles_and_belts(self):
        src = _end("SRC", 0, 0, 100)
        dst = _end("DST", 2000, 2000, 100)
        # one interior waypoint (a right-angle turn); each leg 2000u -> subdivides
        plan = plan_belt_lane(src, dst, [P(2000, 0, 100)], max_straight=1400.0)
        places = [o for o in plan.plan.ops if o.kind == "place"]
        belts = [o for o in plan.plan.ops if o.kind == "belt"]
        # anchors: src, wp(=dst here since collinear), dst ... waypoint == dst pos,
        # so effectively src->(2000,0). 2000/1400 -> 2 spans -> 1 interior pole.
        self.assertGreaterEqual(len(places), 1)
        self.assertEqual(len(belts), len(plan.segments))
        # first belt starts at SRC, last ends at DST
        self.assertEqual(belts[0].source_ref, "SRC")
        self.assertEqual(belts[-1].dest_ref, "DST")
        # interior belts reference the pole op
        self.assertTrue(any(b.source_ref.startswith("op:") or b.dest_ref.startswith("op:") for b in belts))

    def test_incline_too_steep_raises(self):
        src = _end("SRC", 0, 0, 0)
        dst = _end("DST", 100, 0, 1000)  # 84 deg
        with self.assertRaises(RoutingError):
            plan_belt_lane(src, dst, [])

    def test_short_span_warns(self):
        src = _end("SRC", 0, 0, 0)
        dst = _end("DST", 150, 0, 0)  # < MIN_RELIABLE_RUN 300
        plan = plan_belt_lane(src, dst, [])
        self.assertTrue(plan.warnings)

    def test_lane_then_drc_clean_vs_obstacle(self):
        # Route an L-shaped lane around a machine, then DRC it.
        src = _end("SRC", 0, 0, 500)
        dst = _end("DST", 4000, 4000, 500)
        # clean path that detours around a box at (1500..2500, 1500..2500)
        box = Obstacle(id="mach", box_min=P(1500, 1500, 0), box_max=P(2500, 2500, 1000), kind="machine")
        clean = plan_belt_lane(src, dst, [P(0, 4000, 500)])   # up first (x=0), then across (y=4000) - clear of box
        self.assertEqual(check_route(clean.segments, [box]), [])
        # a straight diagonal path would cut the box corner
        thru = plan_belt_lane(src, dst, [P(2000, 2000, 500)])
        self.assertTrue(any(v.kind == "belt-vs-obstacle" for v in check_route(thru.segments, [box])))


if __name__ == "__main__":
    unittest.main()
