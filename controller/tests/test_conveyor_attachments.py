"""Tests for the conveyorAttachments (splitter/merger) parser.

Uses tests/fixtures/conveyor_attachments.json - hand-written SYNTHETIC
inputCount/outputCount/supportsSortRules values (the commonly-known
1-in/3-out splitter / 3-in/1-out merger figures), NOT live-captured
game data (splitter/merger groundwork, 2026-08-25, has not been
live-tested - see docs/conveyor-attachment-research.md). Only the
shape/parsing behavior is under test here, not any particular
real-game connection count.

Placement/connection of splitters and mergers needs no dedicated
toolkit module (unlike belts/power/pipes) - they use the same simple
hologram as any other building, so satisfactory_ai has no
conveyor_attachments.py; this test only covers the parser.

Run from the controller/ directory:
    python -m unittest discover -s tests -t . -v
"""

import unittest
from pathlib import Path

from satisfactory_ai.protocol import parse_conveyor_attachment_catalog_telemetry

FIXTURE_PATH = Path(__file__).resolve().parents[2] / "tests" / "fixtures" / "conveyor_attachments.json"


class ParseConveyorAttachmentCatalogTelemetryTest(unittest.TestCase):
    def test_parses_fixture(self):
        telemetry = parse_conveyor_attachment_catalog_telemetry(FIXTURE_PATH.read_text(encoding="utf-8"))
        self.assertEqual(telemetry.protocol_version, 1)
        self.assertEqual(len(telemetry.attachments), 5)

    def test_plain_splitter_has_one_input_three_outputs(self):
        telemetry = parse_conveyor_attachment_catalog_telemetry(FIXTURE_PATH.read_text(encoding="utf-8"))
        splitter = telemetry.attachments[0]
        self.assertIn("Recipe_ConveyorAttachmentSplitter.", splitter.recipe_class)
        self.assertEqual(splitter.input_count, 1)
        self.assertEqual(splitter.output_count, 3)
        self.assertFalse(splitter.supports_sort_rules)

    def test_plain_merger_has_three_inputs_one_output(self):
        telemetry = parse_conveyor_attachment_catalog_telemetry(FIXTURE_PATH.read_text(encoding="utf-8"))
        merger = telemetry.attachments[3]
        self.assertIn("Recipe_ConveyorAttachmentMerger.", merger.recipe_class)
        self.assertEqual(merger.input_count, 3)
        self.assertEqual(merger.output_count, 1)
        self.assertFalse(merger.supports_sort_rules)

    def test_smart_and_programmable_splitters_support_sort_rules(self):
        telemetry = parse_conveyor_attachment_catalog_telemetry(FIXTURE_PATH.read_text(encoding="utf-8"))
        smart, programmable = telemetry.attachments[1], telemetry.attachments[2]
        self.assertTrue(smart.supports_sort_rules)
        self.assertTrue(programmable.supports_sort_rules)

    def test_priority_merger_does_not_support_sort_rules(self):
        telemetry = parse_conveyor_attachment_catalog_telemetry(FIXTURE_PATH.read_text(encoding="utf-8"))
        priority_merger = telemetry.attachments[4]
        self.assertIn("MergerPriority", priority_merger.recipe_class)
        self.assertFalse(priority_merger.supports_sort_rules)


if __name__ == "__main__":
    unittest.main()
