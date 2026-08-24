"""Schema validation for DocMod resource-node telemetry JSON.

PLAN.md Phase 8 / Task 10. Stdlib only - no LLM dependency, no
optimization solver, no game-control intelligence, per PLAN.md Phase 8's
scope for the initial external controller.

See tests/README.md for fixture provenance and how to run this.
"""

import json
import unittest
from pathlib import Path

FIXTURE_PATH = Path(__file__).parent / "fixtures" / "resource_nodes.json"

VALID_PURITIES = {"Impure", "Normal", "Pure"}


class ResourceNodeTelemetrySchemaTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with FIXTURE_PATH.open("r", encoding="utf-8") as f:
            cls.payload = json.load(f)

    def test_top_level_shape(self):
        self.assertIsInstance(self.payload, dict)
        self.assertEqual(self.payload.get("protocolVersion"), 1)
        self.assertIn("resourceNodes", self.payload)
        self.assertIsInstance(self.payload["resourceNodes"], list)

    def test_at_least_one_node(self):
        # An empty list is technically valid JSON for this schema, but a
        # fixture with zero nodes wouldn't exercise per-node validation
        # below, so treat it as a fixture problem rather than letting the
        # rest of this test class silently pass on nothing.
        self.assertGreater(len(self.payload["resourceNodes"]), 0)

    def test_each_node_matches_schema(self):
        for index, node in enumerate(self.payload["resourceNodes"]):
            with self.subTest(index=index, node_id=node.get("id")):
                self._assert_node_valid(node)

    def _assert_node_valid(self, node):
        for field in ("id", "resource", "resourceClass", "purity"):
            self.assertIn(field, node)
            self.assertIsInstance(node[field], str)
            self.assertTrue(node[field], f"'{field}' must not be empty")

        self.assertIn(
            node["purity"],
            VALID_PURITIES,
            f"purity {node['purity']!r} not in {sorted(VALID_PURITIES)}",
        )

        self.assertIn("position", node)
        position = node["position"]
        self.assertIsInstance(position, dict)
        for axis in ("x", "y", "z"):
            self.assertIn(axis, position)
            self.assertIsInstance(position[axis], (int, float))

        self.assertIn("occupied", node)
        self.assertIsInstance(node["occupied"], bool)


if __name__ == "__main__":
    unittest.main()
