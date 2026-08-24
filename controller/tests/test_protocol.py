"""Tests for satisfactory_ai.protocol / models.

Reuses the repo-root tests/fixtures/resource_nodes.json fixture (shared
with the mod-side schema test in tests/test_resource_node_telemetry.py)
rather than duplicating it - see tests/README.md for that fixture's
provenance (currently hand-written synthetic data, not yet captured from
a real game session).

Run from the controller/ directory:
    python -m unittest discover -s tests -t . -v
"""

import json
import unittest
from pathlib import Path

from satisfactory_ai.protocol import (
    UnsupportedProtocolVersionError,
    parse_resource_node_telemetry,
)

FIXTURE_PATH = Path(__file__).resolve().parents[2] / "tests" / "fixtures" / "resource_nodes.json"


class ParseResourceNodeTelemetryTest(unittest.TestCase):
    def test_parses_fixture(self):
        json_text = FIXTURE_PATH.read_text(encoding="utf-8")
        telemetry = parse_resource_node_telemetry(json_text)

        self.assertEqual(telemetry.protocol_version, 1)
        self.assertGreater(len(telemetry.resource_nodes), 0)
        for node in telemetry.resource_nodes:
            self.assertTrue(node.id)
            self.assertTrue(node.resource)
            self.assertIn(node.purity, {"Impure", "Normal", "Pure"})

    def test_rejects_unsupported_protocol_version(self):
        payload = json.dumps({"protocolVersion": 999, "resourceNodes": []})
        with self.assertRaises(UnsupportedProtocolVersionError):
            parse_resource_node_telemetry(payload)

    def test_rejects_invalid_purity(self):
        payload = json.dumps(
            {
                "protocolVersion": 1,
                "resourceNodes": [
                    {
                        "id": "x",
                        "resource": "y",
                        "resourceClass": "z",
                        "purity": "Ultra Pure",
                        "position": {"x": 0, "y": 0, "z": 0},
                        "occupied": False,
                    }
                ],
            }
        )
        with self.assertRaises(ValueError):
            parse_resource_node_telemetry(payload)

    def test_rejects_missing_field(self):
        payload = json.dumps({"protocolVersion": 1, "resourceNodes": [{"id": "x"}]})
        with self.assertRaises(KeyError):
            parse_resource_node_telemetry(payload)


if __name__ == "__main__":
    unittest.main()
