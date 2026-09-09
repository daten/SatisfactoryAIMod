"""Tests for satisfactory_ai.route_drc (belt/lift route DRC).

Synthetic geometry with round numbers. Run from controller/:
    python -m unittest tests.test_route_drc -v
"""

import unittest

from satisfactory_ai.models import Position
from satisfactory_ai.route_drc import (
    Obstacle,
    RouteSegment,
    check_route,
    segment_segment_distance,
)


def P(x, y, z=0.0):
    return Position(x=float(x), y=float(y), z=float(z))


BOX = Obstacle(id="mach1", box_min=P(0, 0, 0), box_max=P(1000, 1000, 1000), kind="machine")


class RouteDrcTest(unittest.TestCase):
    def test_segment_through_obstacle_flagged(self):
        seg = RouteSegment(p0=P(-500, 500, 500), p1=P(2000, 500, 500), label="thru")
        v = check_route([seg], [BOX])
        self.assertEqual(len(v), 1)
        self.assertEqual(v[0].kind, "belt-vs-obstacle")
        self.assertEqual(v[0].other, "mach1")

    def test_segment_clear_of_obstacle(self):
        seg = RouteSegment(p0=P(-500, 5000, 500), p1=P(2000, 5000, 500), label="clear")
        self.assertEqual(check_route([seg], [BOX]), [])

    def test_ignore_ids_suppresses(self):
        seg = RouteSegment(p0=P(-500, 500, 500), p1=P(2000, 500, 500), label="into",
                           ignore_ids=("mach1",))
        self.assertEqual(check_route([seg], [BOX]), [])

    def test_clearance_inflates_box(self):
        # segment 50u outside the box face; with radius 100 it should still flag.
        seg = RouteSegment(p0=P(1050, -500, 500), p1=P(1050, 2000, 500), radius=100.0, label="near")
        v = check_route([seg], [BOX])
        self.assertEqual(len(v), 1)
        # radius 10 -> clears
        seg2 = RouteSegment(p0=P(1050, -500, 500), p1=P(1050, 2000, 500), radius=10.0, label="near2")
        self.assertEqual(check_route([seg2], [BOX]), [])

    def test_belt_vs_belt(self):
        s1 = RouteSegment(p0=P(0, 0, 0), p1=P(1000, 0, 0), radius=100.0, label="s1")
        s2 = RouteSegment(p0=P(500, -50, 0), p1=P(500, 50, 0), radius=100.0, label="s2")  # crosses s1
        v = check_route([s1, s2], [])
        self.assertEqual(len(v), 1)
        self.assertEqual(v[0].kind, "belt-vs-belt")

    def test_belt_vs_belt_far_apart_ok(self):
        s1 = RouteSegment(p0=P(0, 0, 0), p1=P(1000, 0, 0), radius=100.0, label="s1")
        s3 = RouteSegment(p0=P(0, 1000, 0), p1=P(1000, 1000, 0), radius=100.0, label="s3")
        self.assertEqual(check_route([s1, s3], []), [])

    def test_belt_shared_endpoint_not_flagged(self):
        s1 = RouteSegment(p0=P(0, 0, 0), p1=P(1000, 0, 0), radius=100.0, label="s1")
        s4 = RouteSegment(p0=P(1000, 0, 0), p1=P(1000, 1000, 0), radius=100.0, label="s4")  # shares corner
        self.assertEqual(check_route([s1, s4], []), [])

    def test_below_terrain(self):
        def ground(x, y):
            return 300.0
        low = RouteSegment(p0=P(0, 0, 200), p1=P(1000, 0, 200), radius=100.0, label="low")
        v = check_route([low], [], ground_z=ground)
        self.assertEqual(len(v), 1)
        self.assertEqual(v[0].kind, "belt-below-terrain")
        high = RouteSegment(p0=P(0, 0, 1000), p1=P(1000, 0, 1000), radius=100.0, label="high")
        self.assertEqual(check_route([high], [], ground_z=ground), [])

    def test_segment_segment_distance(self):
        self.assertAlmostEqual(segment_segment_distance(P(0, 0, 0), P(1000, 0, 0),
                                                        P(500, -50, 0), P(500, 50, 0)), 0.0, places=3)
        self.assertAlmostEqual(segment_segment_distance(P(0, 0, 0), P(1000, 0, 0),
                                                        P(0, 300, 0), P(1000, 300, 0)), 300.0, places=3)


if __name__ == "__main__":
    unittest.main()
