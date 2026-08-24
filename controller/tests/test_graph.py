"""Tests for satisfactory_ai.graph (PLAN.md Phase 11).

Uses the shared synthetic fixtures (tests/fixtures/buildables.json,
tests/fixtures/connections.json - see tests/README.md for provenance:
hand-written, not captured from a real game session), which model a
small chain: Constructor -> Belt -> Constructor.

Run from the controller/ directory:
    python -m unittest discover -s tests -t . -v
"""

import unittest
from pathlib import Path

from satisfactory_ai.graph import build_world_graph
from satisfactory_ai.protocol import parse_buildable_telemetry, parse_factory_connection_telemetry

FIXTURES_DIR = Path(__file__).resolve().parents[2] / "tests" / "fixtures"


class BuildWorldGraphTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        buildables_json = (FIXTURES_DIR / "buildables.json").read_text(encoding="utf-8")
        connections_json = (FIXTURES_DIR / "connections.json").read_text(encoding="utf-8")

        cls.buildable_telemetry = parse_buildable_telemetry(buildables_json)
        cls.connection_telemetry = parse_factory_connection_telemetry(connections_json)

    def test_parses_fixtures(self):
        self.assertEqual(len(self.buildable_telemetry.buildables), 3)
        self.assertEqual(len(self.connection_telemetry.connections), 5)

    def test_builds_expected_edges(self):
        graph = build_world_graph(self.buildable_telemetry.buildables, self.connection_telemetry.connections)

        # Two Output+connected rows in the fixture -> two edges. The
        # unconnected Output row and both Input rows must not produce edges.
        self.assertEqual(len(graph.edges), 2)

        constructor_a = self.buildable_telemetry.buildables[0].id
        belt = self.buildable_telemetry.buildables[1].id
        constructor_b = self.buildable_telemetry.buildables[2].id

        edge_pairs = {(e.from_id, e.to_id) for e in graph.edges}
        self.assertEqual(edge_pairs, {(constructor_a, belt), (belt, constructor_b)})

    def test_traversal_helpers(self):
        graph = build_world_graph(self.buildable_telemetry.buildables, self.connection_telemetry.connections)
        belt = self.buildable_telemetry.buildables[1].id

        self.assertEqual(len(graph.incoming(belt)), 1)
        self.assertEqual(len(graph.outgoing(belt)), 1)

    def test_all_buildables_present_even_without_edges(self):
        graph = build_world_graph(self.buildable_telemetry.buildables, self.connection_telemetry.connections)
        self.assertEqual(len(graph.buildables), 3)

    def test_warns_on_unknown_edge_endpoint(self):
        from satisfactory_ai.models import FactoryConnection

        dangling = FactoryConnection(
            owner_buildable_id="known",
            direction="Output",
            connected=True,
            connected_buildable_id="does-not-exist-in-buildables",
        )
        with self.assertWarns(UserWarning):
            build_world_graph([], [dangling])


if __name__ == "__main__":
    unittest.main()
