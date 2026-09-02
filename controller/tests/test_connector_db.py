"""Tests for satisfactory_ai.connector_db.

Every expected coordinate below is REAL live telemetry from the
2026-09-01/02 HMF sessions (docs/placement-lessons.md, memory
project_hmf_optimization.md) - the seed profiles must reproduce the
exact connector positions the game reported for those placements, so a
wrong seed fails against ground truth rather than against itself.

Run from the controller/ directory:
    python -m unittest discover -s tests -t . -v
"""

import tempfile
import unittest
from pathlib import Path

from satisfactory_ai.connector_db import (
    ASSEMBLER,
    CONSTRUCTOR,
    CONTAINER_MK2,
    FOUNDRY,
    MANUFACTURER,
    MERGER,
    SPLITTER,
    ConnectorDb,
    UnknownBuildableClass,
    _class_key_of,
)
from satisfactory_ai.models import Position


def P(x, y, z=0.0):
    return Position(x=float(x), y=float(y), z=float(z))


def positions(db, class_key, pos, yaw, direction):
    return sorted(
        (round(wp.x), round(wp.y), round(wp.z))
        for profile, wp, _ in db.predict(class_key, pos, yaw)
        if profile.direction == direction
    )


class SeedProfileGroundTruth(unittest.TestCase):
    """Seed profiles vs the game's own reported connector positions."""

    def setUp(self):
        self.db = ConnectorDb()

    def test_constructor_yaw0_concrete_machine(self):
        # conc1 at (4100,274600,-3869) yaw0: input (4100,274300,-3769)
        # output (4100,274900,-3769) - live 2026-09-01.
        pos = P(4100, 274600, -3869)
        self.assertEqual(positions(self.db, CONSTRUCTOR, pos, 0.0, "Input"), [(4100, 274300, -3769)])
        self.assertEqual(positions(self.db, CONSTRUCTOR, pos, 0.0, "Output"), [(4100, 274900, -3769)])

    def test_constructor_yaw180_beam_machine(self):
        # beam constructor at (6000,280800,201) yaw180: input
        # (6000,281100,301), output (6000,280500,301) - live 2026-09-01.
        pos = P(6000, 280800, 201)
        self.assertEqual(positions(self.db, CONSTRUCTOR, pos, 180.0, "Input"), [(6000, 281100, 301)])
        self.assertEqual(positions(self.db, CONSTRUCTOR, pos, 180.0, "Output"), [(6000, 280500, 301)])

    def test_assembler_yaw0_eib(self):
        # EIB assembler at (5200,276400,301) yaw0: inputs
        # (5000,275800,401),(5400,275800,401); output (5200,276900,401).
        pos = P(5200, 276400, 301)
        self.assertEqual(
            positions(self.db, ASSEMBLER, pos, 0.0, "Input"),
            [(5000, 275800, 401), (5400, 275800, 401)],
        )
        self.assertEqual(positions(self.db, ASSEMBLER, pos, 0.0, "Output"), [(5200, 276900, 401)])

    def test_assembler_yaw180_rip2(self):
        # rip2' at (8400,277200,401) yaw180: inputs (8200,277800,501),
        # (8600,277800,501); output (8400,276700,501) - live 2026-09-02.
        pos = P(8400, 277200, 401)
        self.assertEqual(
            positions(self.db, ASSEMBLER, pos, 180.0, "Input"),
            [(8200, 277800, 501), (8600, 277800, 501)],
        )
        self.assertEqual(positions(self.db, ASSEMBLER, pos, 180.0, "Output"), [(8400, 276700, 501)])

    def test_manufacturer_yaw0(self):
        # HMF manufacturer at (5800,278000,301) yaw0: inputs at
        # (5200/5600/6000/6400,277125,401); output (5800,278800,401).
        pos = P(5800, 278000, 301)
        self.assertEqual(
            positions(self.db, MANUFACTURER, pos, 0.0, "Input"),
            [(5200, 277125, 401), (5600, 277125, 401), (6000, 277125, 401), (6400, 277125, 401)],
        )
        self.assertEqual(positions(self.db, MANUFACTURER, pos, 0.0, "Output"), [(5800, 278800, 401)])

    def test_foundry_both_yaws(self):
        # foundry2 first placed yaw0 at (7600,282000,201): inputs
        # (7400/7800,281700,301), output (7400,282200,301). Re-placed
        # yaw180: inputs (7400/7800,282300,301), output (7800,281800,301).
        pos = P(7600, 282000, 201)
        self.assertEqual(
            positions(self.db, FOUNDRY, pos, 0.0, "Input"),
            [(7400, 281700, 301), (7800, 281700, 301)],
        )
        self.assertEqual(positions(self.db, FOUNDRY, pos, 0.0, "Output"), [(7400, 282200, 301)])
        self.assertEqual(
            positions(self.db, FOUNDRY, pos, 180.0, "Input"),
            [(7400, 282300, 301), (7800, 282300, 301)],
        )
        self.assertEqual(positions(self.db, FOUNDRY, pos, 180.0, "Output"), [(7800, 281800, 301)])

    def test_splitter_yaw270_north_input(self):
        # Splitter at (6200,281400,301) yaw270: input (6200,281500,301)
        # facing north; outputs south/west/east - live 2026-09-01.
        pos = P(6200, 281400, 301)
        self.assertEqual(positions(self.db, SPLITTER, pos, 270.0, "Input"), [(6200, 281500, 301)])
        self.assertEqual(
            positions(self.db, SPLITTER, pos, 270.0, "Output"),
            [(6100, 281400, 301), (6200, 281300, 301), (6300, 281400, 301)],
        )

    def test_merger_yaw90_north_output(self):
        # Merger yaw90 at (6800,280300,301): output (6800,280400,301)
        # north - live 2026-09-02 (the MJ probe).
        pos = P(6800, 280300, 301)
        self.assertEqual(positions(self.db, MERGER, pos, 90.0, "Output"), [(6800, 280400, 301)])

    def test_container_two_levels(self):
        # Container at (5800,279600,401) yaw0: inputs (5800,279200) at
        # z 501 and 901 - live 2026-09-02.
        pos = P(5800, 279600, 401)
        self.assertEqual(
            positions(self.db, CONTAINER_MK2, pos, 0.0, "Input"),
            [(5800, 279200, 501), (5800, 279200, 901)],
        )


class YawSelection(unittest.TestCase):
    def setUp(self):
        self.db = ConnectorDb()

    def test_splitter_yaw_for_north_facing_input(self):
        # "What yaw makes a splitter's input face north (accept flow
        # from the north)?" - the question that cost three placements
        # live. Answer: 270.
        self.assertEqual(
            self.db.yaw_for_connector_facing(SPLITTER, "Input", P(0, 1)), [270.0]
        )

    def test_merger_yaw_for_south_output(self):
        self.assertEqual(
            self.db.yaw_for_connector_facing(MERGER, "Output", P(0, -1)), [270.0]
        )

    def test_find_connector_ambiguity_is_loud(self):
        with self.assertRaises(LookupError):
            self.db.find_connector(SPLITTER, P(0, 0, 0), 0.0, "Output")  # three outputs


class LearningAndPersistence(unittest.TestCase):
    def test_unknown_class_is_loud(self):
        with self.assertRaises(UnknownBuildableClass):
            ConnectorDb().profiles_for("Build_Nonexistent_C")

    def test_class_key_of(self):
        self.assertEqual(_class_key_of("Build_ConstructorMk1_C_2147044451"), "Build_ConstructorMk1_C")
        self.assertEqual(
            _class_key_of(
                "/Game/.../Persistent_Level.Persistent_Level:PersistentLevel.Build_FoundryMk1_C_2147225575"
            ),
            "Build_FoundryMk1_C",
        )

    def test_persist_roundtrip(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "profiles.json"
            db = ConnectorDb(persist_path=path)
            # Simulate a learned override for the smelter (the one seed
            # marked UNVERIFIED-yaw).
            from satisfactory_ai.layout import ConnectorProfile

            db._profiles["Build_SmelterMk1_C"] = [
                ConnectorProfile("Input", P(0, -300, 100), P(0, -1)),
            ]
            db._save(path)
            db2 = ConnectorDb(persist_path=path)
            self.assertEqual(len(db2.profiles_for("Build_SmelterMk1_C")), 1)
            # Seeds unaffected.
            self.assertEqual(len(db2.profiles_for(CONSTRUCTOR)), 2)


if __name__ == "__main__":
    unittest.main()
