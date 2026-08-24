"""Normalized protocol models, mirroring docs/telemetry-protocol.md.

Plain dataclasses independent of any Unreal/game-engine concept - the
Python-side counterpart to DocMod's FDocModResourceNodeTelemetry
(Mods/GameFeatures/DocMod/Source/DocMod/Public/DocModTelemetryTypes.h).
Kept in sync by hand for now; there is no schema-generation step.
"""

from __future__ import annotations

from dataclasses import dataclass

VALID_PURITIES = frozenset({"Impure", "Normal", "Pure"})


@dataclass(frozen=True)
class Position:
    x: float
    y: float
    z: float

    @classmethod
    def from_dict(cls, data: dict) -> "Position":
        return cls(x=float(data["x"]), y=float(data["y"]), z=float(data["z"]))


@dataclass(frozen=True)
class ResourceNode:
    """Mirrors FDocModResourceNodeTelemetry.

    id is NOT save-stable - see docs/telemetry-protocol.md and
    docs/resource-node-research.md Sec.4. It is a session-local debug
    label (the node actor's Unreal object path name) only; do not persist
    it across sessions or treat it as a database key.
    """

    id: str
    resource: str
    resource_class: str
    purity: str
    position: Position
    occupied: bool

    def __post_init__(self) -> None:
        if self.purity not in VALID_PURITIES:
            raise ValueError(f"purity {self.purity!r} not in {sorted(VALID_PURITIES)}")

    @classmethod
    def from_dict(cls, data: dict) -> "ResourceNode":
        return cls(
            id=data["id"],
            resource=data["resource"],
            resource_class=data["resourceClass"],
            purity=data["purity"],
            position=Position.from_dict(data["position"]),
            occupied=bool(data["occupied"]),
        )
