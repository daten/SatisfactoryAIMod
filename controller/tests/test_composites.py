"""Tests for satisfactory_ai.composites.

The manifold fixture reproduces the REAL east screw-collection rail
built live 2026-09-02 (four screw constructors at x 7600..10000,
outputs at (x,278500,501) facing south; mergers in a west-flowing rail
below them at yaw 180) - the composite must plan the same topology that
was proven in the game.

Run from the controller/ directory:
    python -m unittest discover -s tests -t . -v
"""

import unittest

from satisfactory_ai.composites import machine_row, manifold, pole_backbone, verify_connections
from satisfactory_ai.connector_db import ConnectorDb
from satisfactory_ai.models import FactoryConnection, Position
from satisfactory_ai.router import Endpoint, RoutingError

CONSTRUCTOR_RECIPE = (
    "/Game/FactoryGame/Recipes/Buildings/Recipe_ConstructorMk1.Recipe_ConstructorMk1_C"
)
ROD_RECIPE = "/Game/FactoryGame/Recipes/Constructor/Recipe_IronRod.Recipe_IronRod_C"


def P(x, y, z=0.0):
    return Position(x=float(x), y=float(y), z=float(z))


class MachineRow(unittest.TestCase):
    def test_row_plan_shape(self):
        db = ConnectorDb()
        cp = machine_row(
            db,
            build_recipe=CONSTRUCTOR_RECIPE,
            class_key="Build_ConstructorMk1_C",
            count=3,
            origin=P(7600, 280400, 401),
            spacing=800.0,
            yaw=180.0,
            machine_recipe=ROD_RECIPE,
            clock_percent=220.0,
            shards=3,
        )
        kinds = [op.kind for op in cp.plan.ops]
        # place, recipe, shards, clock - per machine, in that order.
        self.assertEqual(kinds, ["place", "call", "call", "call"] * 3)
        # Explicit yaw on every placement (implicit yaw is nondeterministic).
        for op in cp.plan.ops:
            if op.kind == "place":
                self.assertEqual(op.yaw, 180.0)
        # Config calls reference their own machine's placement op.
        self.assertEqual(cp.plan.ops[1].dest_ref, "op:0")
        self.assertEqual(cp.plan.ops[5].dest_ref, "op:4")
        # Spacing applied on x.
        self.assertEqual(cp.plan.ops[8].position.x, 7600 + 1600)


class MergerManifold(unittest.TestCase):
    """The live east screw-collection rail, re-planned."""

    def setUp(self):
        self.db = ConnectorDb()
        self.targets = [
            Endpoint(f"scrE{i + 1}", P(7600 + 800 * i, 278500, 501), P(0, -1))
            for i in range(4)
        ]

    def test_rail_topology_matches_live_build(self):
        cp = manifold(self.db, self.targets, kind="merger", rail_standoff=400.0, trunk_end="first")
        places = [op for op in cp.plan.ops if op.kind == "place"]
        belts = [op for op in cp.plan.ops if op.kind == "belt"]
        self.assertEqual(len(places), 4)
        # 4 drops + 3 rail links.
        self.assertEqual(len(belts), 7)
        # Live: mergers at yaw180 (drop input north, output west toward trunk).
        for op in places:
            self.assertEqual(op.yaw, 180.0)
        # Rail sits 400 south of the outputs (standoff along the facing).
        self.assertEqual(places[0].position.y, 278500 - 400)
        # Drops are machine-output -> rail-input (merger collects).
        drop0 = belts[0]
        self.assertEqual(drop0.source_ref, "scrE1")
        self.assertTrue(drop0.dest_ref.startswith("op:"))
        # A verify pin per drop.
        self.assertEqual(len(cp.verify_spec), 4)
        # Trunk output reported at the west (first) end.
        self.assertTrue(any("trunk Output" in w for w in cp.plan.warnings))

    def test_mixed_facings_rejected(self):
        bad = list(self.targets)
        bad[2] = Endpoint("scrE3", P(9200, 278500, 501), P(0, 1))
        with self.assertRaises(RoutingError):
            manifold(self.db, bad, kind="merger")

    def test_too_small_standoff_rejected(self):
        with self.assertRaises(ValueError):
            manifold(self.db, self.targets, kind="merger", rail_standoff=200.0)

    def test_splitter_manifold_feeds_inputs(self):
        # Distribution variant: four machine INPUTS facing north (yaw180
        # constructors), rail above.
        inputs = [
            Endpoint(f"rodE{i + 1}", P(7600 + 800 * i, 280700, 501), P(0, 1))
            for i in range(4)
        ]
        cp = manifold(self.db, inputs, kind="splitter", rail_standoff=400.0, trunk_end="first")
        drop0 = next(op for op in cp.plan.ops if op.kind == "belt")
        # Splitter distributes: rail -> machine.
        self.assertTrue(drop0.source_ref.startswith("op:"))
        self.assertEqual(drop0.dest_ref, "rodE1")


class PoleBackbone(unittest.TestCase):
    def test_backbone_wiring(self):
        cp = pole_backbone([P(4400, 278400, 201), P(8400, 279600, 401)], grid_source_id="Build_X_C_1")
        kinds = [op.kind for op in cp.plan.ops]
        self.assertEqual(kinds, ["place", "wire", "place", "wire"])
        self.assertEqual(cp.plan.ops[1].source_ref, "Build_X_C_1")
        self.assertEqual(cp.plan.ops[3].source_ref, "op:0")
        self.assertEqual(cp.plan.ops[3].dest_ref, "op:2")


class VerifyConnections(unittest.TestCase):
    def test_verify_reports_unconnected(self):
        pin = P(7600, 278100, 501)
        spec = [("op:0", pin)]
        refs = {"op:0": "Build_ConveyorAttachmentMerger_C_999"}
        conns = [
            FactoryConnection(
                owner_buildable_id="...PersistentLevel.Build_ConveyorAttachmentMerger_C_999",
                direction="Input",
                connected=False,
                connected_buildable_id="",
                position=pin,
                normal=P(0, 1),
            )
        ]
        problems = verify_connections(conns, spec, refs)
        self.assertEqual(len(problems), 1)
        self.assertIn("NOT connected", problems[0])
        conns2 = [
            FactoryConnection(
                owner_buildable_id="...PersistentLevel.Build_ConveyorAttachmentMerger_C_999",
                direction="Input",
                connected=True,
                connected_buildable_id="belt",
                position=pin,
                normal=P(0, 1),
            )
        ]
        self.assertEqual(verify_connections(conns2, spec, refs), [])


if __name__ == "__main__":
    unittest.main()
