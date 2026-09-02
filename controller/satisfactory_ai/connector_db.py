"""Persisted per-class connector layouts (2026-09-02).

Phase 1a of docs/build-efficiency-plan.md: the single biggest time sink
in the live HMF builds was discovering connector positions/facings
empirically - place, query world.connections, find the input faces the
wrong way, delete, re-place at another yaw. The yaw->connector mapping
is deterministic per buildable class, so this module holds a library of
layout.ConnectorProfile records (local, yaw-independent frame) per
class, seeded from live-confirmed telemetry of the 2026-09-01/02
sessions and growable from any world.buildables + world.connections
snapshot via learn_and_store().

Same "toolkit, not solver" posture as the rest of this package: this
answers "where will class C's connectors be if placed at (x, y, z,
yaw)?" so an agent (or the router/composites built on it) can compute a
correct placement the FIRST time. It never chooses positions.

Coordinate/units conventions (all live-confirmed):
- 1 unit = 1 cm. Cardinals: +Y=north, -Y=south, +X=east, -X=west
  (satisfactory_ai.splitters' convention).
- MACHINE connectors sit 100 units above the machine's placement z
  (constructor placed at z=201 -> connectors at z=301).
- ATTACHMENT (splitter/merger) connectors sit AT the placement z.
- Local frames below are for yaw=0; layout.predict_connector_world_state
  applies any placement yaw.

Seed-data provenance: every profile below was derived from live
world.connections telemetry of buildings placed via world.placeBuilding
with an explicit yaw during the 2026-09-01/02 sessions (see
docs/placement-lessons.md and memory project_hmf_optimization.md).
A profile marked UNVERIFIED-yaw was observed at one yaw only and
un-rotated on the assumption the observation's yaw report was accurate.
"""

from __future__ import annotations

import json
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Tuple

from .layout import ConnectorProfile, predict_connector_world_state, unrotate_yaw
from .models import Buildable, FactoryConnection, Position

# Short class keys are the Build_*_C class name without path - matching
# what world.buildables ids embed (Build_ConstructorMk1_C_12345).
CONSTRUCTOR = "Build_ConstructorMk1_C"
ASSEMBLER = "Build_AssemblerMk1_C"
MANUFACTURER = "Build_ManufacturerMk1_C"
FOUNDRY = "Build_FoundryMk1_C"
SMELTER = "Build_SmelterMk1_C"
MINER_MK3 = "Build_MinerMk3_C"
SPLITTER = "Build_ConveyorAttachmentSplitter_C"
MERGER = "Build_ConveyorAttachmentMerger_C"
CONTAINER_MK2 = "Build_StorageContainerMk2_C"

#: Machine classes whose connectors sit +100 above placement z.
#: Attachments (splitter/merger) are omitted deliberately: their
#: connectors sit AT placement z (live-confirmed both ways).
MACHINE_CONNECTOR_Z_OFFSET: Dict[str, float] = {
    CONSTRUCTOR: 100.0,
    ASSEMBLER: 100.0,
    MANUFACTURER: 100.0,
    FOUNDRY: 100.0,
    SMELTER: 100.0,
    MINER_MK3: 100.0,
    CONTAINER_MK2: 100.0,  # lower belt level; a second pair sits at +500
}


def _p(x: float, y: float, z: float = 0.0) -> Position:
    return Position(x=x, y=y, z=z)


# ---------------------------------------------------------------------------
# Seed profiles, local frame at yaw=0. z in these profiles is the offset
# from the PLACEMENT z (so machine profiles carry the +100 already).
# ---------------------------------------------------------------------------

SEED_PROFILES: Dict[str, List[ConnectorProfile]] = {
    # Constructor, yaw0: input on the SOUTH face pulling from the south,
    # output on the NORTH face pushing north. (Live: concrete constructor
    # at (4100,274600,-3869) yaw0 -> input (4100,274300,-3769) n=(0,-1),
    # output (4100,274900,-3769) n=(0,+1). yaw180 mirror confirmed on the
    # beam/pipe/rod/screw rows.)
    CONSTRUCTOR: [
        ConnectorProfile("Input", _p(0.0, -300.0, 100.0), _p(0.0, -1.0)),
        ConnectorProfile("Output", _p(0.0, 300.0, 100.0), _p(0.0, 1.0)),
    ],
    # Assembler, yaw0: two inputs on the SOUTH face, output NORTH.
    # (Live: EIB assembler at (5200,276400,301) yaw0 -> inputs
    # (5000/5400,275800,401) n=(0,-1), output (5200,276900,401) n=(0,+1);
    # yaw180 mirror confirmed on rip2'/mf2'.)
    ASSEMBLER: [
        ConnectorProfile("Input", _p(-200.0, -600.0, 100.0), _p(0.0, -1.0)),
        ConnectorProfile("Input", _p(200.0, -600.0, 100.0), _p(0.0, -1.0)),
        ConnectorProfile("Output", _p(0.0, 500.0, 100.0), _p(0.0, 1.0)),
    ],
    # Manufacturer, yaw0: four inputs on the SOUTH face, output NORTH.
    # (Live: HMF manufacturer at (5800,278000,301) yaw0 -> inputs
    # (5200/5600/6000/6400,277125,401) n=(0,-1), output (5800,278800,401)
    # n=(0,+1).)
    MANUFACTURER: [
        ConnectorProfile("Input", _p(-600.0, -875.0, 100.0), _p(0.0, -1.0)),
        ConnectorProfile("Input", _p(-200.0, -875.0, 100.0), _p(0.0, -1.0)),
        ConnectorProfile("Input", _p(200.0, -875.0, 100.0), _p(0.0, -1.0)),
        ConnectorProfile("Input", _p(600.0, -875.0, 100.0), _p(0.0, -1.0)),
        ConnectorProfile("Output", _p(0.0, 800.0, 100.0), _p(0.0, 1.0)),
    ],
    # Foundry, yaw0: two inputs SOUTH (x -200/+200 of center), output on
    # the NORTH face but offset WEST of center. (Live: foundry2 first
    # placement at (7600,282000,201) yaw0 -> inputs (7400/7800,281700,301)
    # n=(0,-1), output (7400,282200,301) n=(0,+1); the yaw180 re-place
    # mirrored all three exactly.)
    FOUNDRY: [
        ConnectorProfile("Input", _p(-200.0, -300.0, 100.0), _p(0.0, -1.0)),
        ConnectorProfile("Input", _p(200.0, -300.0, 100.0), _p(0.0, -1.0)),
        ConnectorProfile("Output", _p(-200.0, 200.0, 100.0), _p(0.0, 1.0)),
    ],
    # Smelter: observed on the iron row (machines at y=282000, z=201):
    # input (x, y+300, 301) n=(0,+1), output (x, y-200, 301) n=(0,-1).
    # The row's placement yaw was not recorded at build time -> these
    # locals assume the row was yaw=180 (input north like its neighbors),
    # giving yaw0 locals of input SOUTH (0,-300), output NORTH (0,+200).
    # UNVERIFIED-yaw: confirm with one fresh yaw0 placement before
    # trusting a non-180 smelter placement.
    SMELTER: [
        ConnectorProfile("Input", _p(0.0, -300.0, 100.0), _p(0.0, -1.0)),
        ConnectorProfile("Output", _p(0.0, 200.0, 100.0), _p(0.0, 1.0)),
    ],
    # Miner Mk3: single output. (Live: limestone miner at
    # (4128,273242,-3969) -> output (4128,274042,-3869) n=(0,+1); iron/
    # coal miners match the +800 pattern.) UNVERIFIED-yaw: the placement
    # yaw of those miners was not recorded; treat the +y offset as the
    # yaw0 local until a fresh survey confirms.
    MINER_MK3: [
        ConnectorProfile("Output", _p(0.0, 800.0, 100.0), _p(0.0, 1.0)),
    ],
    # Splitter, yaw0: ONE input facing WEST (pulls from the west), three
    # outputs E/N/S. Attachment connectors at placement z (offset 0).
    # (Live-confirmed repeatedly, including the yaw90/180/270 rotations.)
    SPLITTER: [
        ConnectorProfile("Input", _p(-100.0, 0.0, 0.0), _p(-1.0, 0.0)),
        ConnectorProfile("Output", _p(100.0, 0.0, 0.0), _p(1.0, 0.0)),
        ConnectorProfile("Output", _p(0.0, -100.0, 0.0), _p(0.0, -1.0)),
        ConnectorProfile("Output", _p(0.0, 100.0, 0.0), _p(0.0, 1.0)),
    ],
    # Merger, yaw0: ONE output facing EAST, three inputs W/N/S.
    MERGER: [
        ConnectorProfile("Output", _p(100.0, 0.0, 0.0), _p(1.0, 0.0)),
        ConnectorProfile("Input", _p(-100.0, 0.0, 0.0), _p(-1.0, 0.0)),
        ConnectorProfile("Input", _p(0.0, -100.0, 0.0), _p(0.0, -1.0)),
        ConnectorProfile("Input", _p(0.0, 100.0, 0.0), _p(0.0, 1.0)),
    ],
    # Industrial Storage Container, yaw0: inputs on the SOUTH face at TWO
    # belt levels (+100 and +500 above placement z), outputs NORTH at the
    # same two levels. (Live: container at (5800,279600,401) yaw0 ->
    # inputs (5800,279200) at z 501 and 901, outputs (5800,280000) same.)
    CONTAINER_MK2: [
        ConnectorProfile("Input", _p(0.0, -400.0, 100.0), _p(0.0, -1.0)),
        ConnectorProfile("Input", _p(0.0, -400.0, 500.0), _p(0.0, -1.0)),
        ConnectorProfile("Output", _p(0.0, 400.0, 100.0), _p(0.0, 1.0)),
        ConnectorProfile("Output", _p(0.0, 400.0, 500.0), _p(0.0, 1.0)),
    ],
}


class UnknownBuildableClass(KeyError):
    """Raised when a class has no stored profile - the caller should
    learn one from live telemetry (learn_and_store) rather than guess."""


@dataclass
class ConnectorDb:
    """Profile store: seeds + anything learned this or prior sessions.

    persist_path, when given, is a JSON file the learned (non-seed)
    entries are saved to / loaded from, so one session's learning
    carries to the next without re-querying a live game.
    """

    persist_path: Optional[Path] = None
    _profiles: Dict[str, List[ConnectorProfile]] = field(default_factory=dict)

    def __post_init__(self) -> None:
        self._profiles = {k: list(v) for k, v in SEED_PROFILES.items()}
        if self.persist_path is not None and Path(self.persist_path).exists():
            self._load(Path(self.persist_path))

    # -- queries ----------------------------------------------------------

    def known_classes(self) -> List[str]:
        return sorted(self._profiles)

    def profiles_for(self, class_key: str) -> List[ConnectorProfile]:
        if class_key not in self._profiles:
            raise UnknownBuildableClass(
                f"No connector profile for {class_key!r} - learn one from live "
                f"telemetry (learn_and_store) before placing this class blind."
            )
        return list(self._profiles[class_key])

    def predict(
        self, class_key: str, position: Position, yaw: float
    ) -> List[Tuple[ConnectorProfile, Position, Position]]:
        """All connectors' (profile, world_position, world_normal) for a
        hypothetical placement. z offsets in the profiles are relative
        to the placement z, so pass the intended placement position
        as-is (machine vs attachment z conventions are already baked
        into the stored locals)."""
        results = []
        for profile in self.profiles_for(class_key):
            world_pos, world_normal = predict_connector_world_state(profile, position, yaw)
            results.append((profile, world_pos, world_normal))
        return results

    def find_connector(
        self,
        class_key: str,
        position: Position,
        yaw: float,
        direction: str,
        facing: Optional[Position] = None,
        tolerance: float = 1e-3,
    ) -> Tuple[Position, Position]:
        """The single connector matching direction (and optionally a
        world facing) for a hypothetical placement - the typical router
        query ("where is the north-facing Input if I place a merger at P
        yaw 270?"). Raises if zero or multiple match, so ambiguity is a
        loud error rather than a silent pick."""
        matches = []
        for profile, world_pos, world_normal in self.predict(class_key, position, yaw):
            if profile.direction != direction:
                continue
            if facing is not None:
                dot = world_normal.x * facing.x + world_normal.y * facing.y + world_normal.z * facing.z
                if dot < (1.0 - tolerance):
                    continue
            matches.append((world_pos, world_normal))
        if len(matches) != 1:
            raise LookupError(
                f"{class_key} at yaw {yaw}: expected exactly 1 {direction!r} connector"
                + (f" facing ({facing.x},{facing.y},{facing.z})" if facing else "")
                + f", found {len(matches)}"
            )
        return matches[0]

    def yaw_for_connector_facing(
        self, class_key: str, direction: str, desired_world_normal: Position, tolerance: float = 1e-3
    ) -> List[float]:
        """Which cardinal yaws give this class a connector of the given
        direction facing desired_world_normal - the "what yaw do I place
        this splitter at so its input faces north" question that cost
        multiple delete/re-place cycles per attachment in the live
        builds."""
        from .layout import CARDINAL_YAWS_DEGREES, rotate_yaw

        yaws = []
        for yaw in CARDINAL_YAWS_DEGREES:
            for profile in self.profiles_for(class_key):
                if profile.direction != direction:
                    continue
                rotated = rotate_yaw(profile.local_normal, yaw)
                dot = (
                    rotated.x * desired_world_normal.x
                    + rotated.y * desired_world_normal.y
                    + rotated.z * desired_world_normal.z
                )
                if dot >= (1.0 - tolerance):
                    yaws.append(yaw)
                    break
        return yaws

    # -- learning ---------------------------------------------------------

    def learn_and_store(
        self, buildable: Buildable, connections: List[FactoryConnection]
    ) -> List[ConnectorProfile]:
        """Derive this instance's profiles from live telemetry and store
        them under its class, REPLACING any prior entry for that class
        (live data beats seeds). Machine-z convention: the stored local z
        is (connector z - placement z), exactly as observed - no +100 is
        assumed or added here."""
        from .layout import learn_all_connector_profiles

        class_key = _class_key_of(buildable.id)
        learned = learn_all_connector_profiles(buildable, connections)
        if not learned:
            raise ValueError(f"No connections owned by {buildable.id!r} in the given list")
        self._profiles[class_key] = learned
        if self.persist_path is not None:
            self._save(Path(self.persist_path))
        return learned

    # -- persistence ------------------------------------------------------

    def _save(self, path: Path) -> None:
        payload = {
            class_key: [
                {
                    "direction": p.direction,
                    "local_position": [p.local_position.x, p.local_position.y, p.local_position.z],
                    "local_normal": [p.local_normal.x, p.local_normal.y, p.local_normal.z],
                }
                for p in profiles
            ]
            for class_key, profiles in self._profiles.items()
            if class_key not in SEED_PROFILES or profiles != SEED_PROFILES[class_key]
        }
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(payload, indent=2), encoding="utf-8")

    def _load(self, path: Path) -> None:
        payload = json.loads(path.read_text(encoding="utf-8"))
        for class_key, entries in payload.items():
            self._profiles[class_key] = [
                ConnectorProfile(
                    direction=e["direction"],
                    local_position=_p(*e["local_position"]),
                    local_normal=_p(*e["local_normal"]),
                )
                for e in entries
            ]


def _class_key_of(buildable_id: str) -> str:
    """Build_ConstructorMk1_C_2147044451 -> Build_ConstructorMk1_C.
    Also accepts full path-style ids (splits on the final '.')."""
    tail = buildable_id.rsplit(".", 1)[-1]
    # Strip a trailing _<digits> instance suffix if present.
    stem, _, suffix = tail.rpartition("_")
    if suffix.isdigit():
        return stem
    return tail
