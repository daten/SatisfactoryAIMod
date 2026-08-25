"""Tests for satisfactory_ai.layout.

Uses real geometry captured live during 2026-08-25's belt-routing
session (docs/demo-production-chain.md) as ground truth rather than
made-up numbers: a Smelter's Output connector and a Constructor's Input
connector that were confirmed, live, to snap correctly but fail
CanConstruct() with "Invalid Conveyor Belt shape!" because of their
relative position (not their orientation, which was actually fine) -
and the real fix (moving the Constructor to the Smelter's north side)
that made a real world.connectConveyor call succeed.

Run from the controller/ directory:
    python -m unittest discover -s tests -t . -v
"""

import math
import unittest

from satisfactory_ai.layout import (
    ConnectorProfile,
    candidate_yaws_for_normal,
    compute_aligned_placement_position,
    connectors_are_compatible,
    learn_connector_profile,
    predict_connector_world_state,
    rotate_yaw,
    unrotate_yaw,
)
from satisfactory_ai.models import Buildable, FactoryConnection, Position, Rotation

# Real values from live telemetry, 2026-08-25 (see
# docs/demo-production-chain.md) - the Smelter's placement never
# changed; the Constructor values are its FIRST (failed-geometry)
# placement, south of the Smelter.
SMELTER = Buildable(
    id="Build_SmelterMk1_C_2147434176",
    buildable_class="/Game/FactoryGame/Buildable/Factory/SmelterMk1/Build_SmelterMk1.Build_SmelterMk1_C",
    position=Position(x=-52202.234375, y=160447.9375, z=8431.0576171875),
    rotation=Rotation(pitch=0.0, yaw=0.0, roll=0.0),
)
SMELTER_OUTPUT = FactoryConnection(
    owner_buildable_id=SMELTER.id,
    direction="Output",
    connected=False,
    connected_buildable_id="",
    position=Position(x=-52202.234375, y=160647.9375, z=8531.0576171875),
    normal=Position(x=0.0, y=1.0, z=0.0),
)

CONSTRUCTOR_FIRST_PLACEMENT = Buildable(
    id="Build_ConstructorMk1_C_2147436054",
    buildable_class="/Game/FactoryGame/Buildable/Factory/ConstructorMk1/Build_ConstructorMk1.Build_ConstructorMk1_C",
    position=Position(x=-51602.234375, y=159847.9375, z=8431.0498046875),
    rotation=Rotation(pitch=0.0, yaw=0.0, roll=0.0),
)
CONSTRUCTOR_INPUT_FIRST_PLACEMENT = FactoryConnection(
    owner_buildable_id=CONSTRUCTOR_FIRST_PLACEMENT.id,
    direction="Input",
    connected=False,
    connected_buildable_id="",
    position=Position(x=-51602.234375, y=159547.9375, z=8531.0498046875),
    normal=Position(x=0.0, y=-1.0, z=0.0),
)


class RotateYawTest(unittest.TestCase):
    def test_90_degrees_turns_x_toward_y(self):
        result = rotate_yaw(Position(x=1.0, y=0.0, z=0.0), 90.0)
        self.assertAlmostEqual(result.x, 0.0, places=6)
        self.assertAlmostEqual(result.y, 1.0, places=6)

    def test_180_degrees_negates_xy(self):
        result = rotate_yaw(Position(x=1.0, y=2.0, z=3.0), 180.0)
        self.assertAlmostEqual(result.x, -1.0, places=6)
        self.assertAlmostEqual(result.y, -2.0, places=6)
        self.assertAlmostEqual(result.z, 3.0, places=6)

    def test_unrotate_is_inverse(self):
        original = Position(x=12.5, y=-7.0, z=3.0)
        for yaw in (0.0, 37.0, 90.0, 180.0, 270.0):
            round_tripped = unrotate_yaw(rotate_yaw(original, yaw), yaw)
            self.assertAlmostEqual(round_tripped.x, original.x, places=5)
            self.assertAlmostEqual(round_tripped.y, original.y, places=5)
            self.assertAlmostEqual(round_tripped.z, original.z, places=5)


class LearnConnectorProfileTest(unittest.TestCase):
    def test_yaw_zero_local_position_is_plain_offset(self):
        profile = learn_connector_profile(SMELTER, SMELTER_OUTPUT)
        self.assertAlmostEqual(profile.local_position.x, 0.0, places=3)
        self.assertAlmostEqual(profile.local_position.y, 200.0, places=3)
        self.assertEqual(profile.direction, "Output")

    def test_rejects_mismatched_owner(self):
        with self.assertRaises(ValueError):
            learn_connector_profile(CONSTRUCTOR_FIRST_PLACEMENT, SMELTER_OUTPUT)

    def test_predict_round_trips_the_original_placement(self):
        profile = learn_connector_profile(SMELTER, SMELTER_OUTPUT)
        predicted_position, predicted_normal = predict_connector_world_state(
            profile, SMELTER.position, SMELTER.rotation.yaw
        )
        self.assertAlmostEqual(predicted_position.x, SMELTER_OUTPUT.position.x, places=3)
        self.assertAlmostEqual(predicted_position.y, SMELTER_OUTPUT.position.y, places=3)
        self.assertAlmostEqual(predicted_normal.y, SMELTER_OUTPUT.normal.y, places=3)


class ConnectorsAreCompatibleTest(unittest.TestCase):
    def test_opposite_normals_are_compatible(self):
        # This is the real, confirmed-live case where ORIENTATION was
        # fine (normals are exact opposites) but the belt still failed
        # because of POSITION - proves the function's documented "does
        # NOT check position" caveat with real data, not a made-up one.
        self.assertTrue(connectors_are_compatible(SMELTER_OUTPUT.normal, CONSTRUCTOR_INPUT_FIRST_PLACEMENT.normal))

    def test_same_direction_normals_are_not_compatible(self):
        self.assertFalse(connectors_are_compatible(Position(0.0, 1.0, 0.0), Position(0.0, 1.0, 0.0)))

    def test_perpendicular_normals_are_not_compatible(self):
        self.assertFalse(connectors_are_compatible(Position(1.0, 0.0, 0.0), Position(0.0, 1.0, 0.0)))


class CandidateYawsForNormalTest(unittest.TestCase):
    def test_finds_the_180_flip_needed_for_the_real_fix(self):
        # The real fix that made the belt work was equivalent to needing
        # the Constructor's input to face NORTH (0,1,0) instead of its
        # actual SOUTH-facing (0,-1,0) orientation at yaw=0 - this is
        # exactly the geometry question that fix answers.
        profile = learn_connector_profile(CONSTRUCTOR_FIRST_PLACEMENT, CONSTRUCTOR_INPUT_FIRST_PLACEMENT)
        yaws = candidate_yaws_for_normal(profile.local_normal, Position(x=0.0, y=1.0, z=0.0))
        self.assertEqual(yaws, [180.0])

    def test_no_match_returns_empty(self):
        # A horizontal normal (z=0) can never be rotated to point
        # straight up by a yaw rotation (yaw only turns the XY plane) -
        # a genuinely unreachable target, unlike a cardinal-aligned
        # horizontal one (any of the 4 yaws would reach some cardinal
        # direction, since the input is itself cardinal-aligned).
        yaws = candidate_yaws_for_normal(Position(0.0, 1.0, 0.0), Position(0.0, 0.0, 1.0))
        self.assertEqual(yaws, [])


class ComputeAlignedPlacementPositionTest(unittest.TestCase):
    def test_predicted_position_is_downstream_of_the_source(self):
        # Uses the Constructor's REAL learned profile (from its first,
        # failed placement) to ask: where should a Constructor go to
        # align with the Smelter's Output, at yaw=0? The real fix moved
        # the Constructor from Y=159847 (south of the Smelter) to
        # Y=161665 (north) - this should predict a position on the same
        # (north) side, not reproduce the original failing placement.
        profile = learn_connector_profile(CONSTRUCTOR_FIRST_PLACEMENT, CONSTRUCTOR_INPUT_FIRST_PLACEMENT)
        placement = compute_aligned_placement_position(
            target_connector_position=SMELTER_OUTPUT.position,
            target_connector_normal=SMELTER_OUTPUT.normal,
            new_connector_profile=profile,
            new_building_yaw=0.0,
            clearance_distance=500.0,
        )
        self.assertGreater(placement.y, SMELTER_OUTPUT.position.y)
        self.assertAlmostEqual(placement.x, SMELTER.position.x, places=3)

    def test_raises_when_yaw_does_not_face_opposite_direction(self):
        profile = learn_connector_profile(SMELTER, SMELTER_OUTPUT)  # an Output-facing-north profile
        with self.assertRaises(ValueError):
            compute_aligned_placement_position(
                target_connector_position=SMELTER_OUTPUT.position,
                target_connector_normal=SMELTER_OUTPUT.normal,
                new_connector_profile=profile,  # also faces north - not opposite
                new_building_yaw=0.0,
                clearance_distance=500.0,
            )


if __name__ == "__main__":
    unittest.main()
