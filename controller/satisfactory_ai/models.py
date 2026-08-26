"""Normalized protocol models, mirroring docs/telemetry-protocol.md.

Plain dataclasses independent of any Unreal/game-engine concept - the
Python-side counterpart to DocMod's FDocModResourceNodeTelemetry
(Mods/GameFeatures/DocMod/Source/DocMod/Public/DocModTelemetryTypes.h).
Kept in sync by hand for now; there is no schema-generation step.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Optional

VALID_PURITIES = frozenset({"Impure", "Normal", "Pure"})
VALID_CONNECTION_DIRECTIONS = frozenset({"Input", "Output", "Any", "SnapOnly"})


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


@dataclass(frozen=True)
class Rotation:
    pitch: float
    yaw: float
    roll: float

    @classmethod
    def from_dict(cls, data: dict) -> "Rotation":
        return cls(pitch=float(data["pitch"]), yaw=float(data["yaw"]), roll=float(data["roll"]))


@dataclass(frozen=True)
class Buildable:
    """Mirrors FDocModBuildableTelemetry. Generic fields for any placed
    AFGBuildable - see ManufacturerInfo for recipe/clock/production
    fields, which only apply to manufacturing buildings.

    id has the same session-local-only caveat as ResourceNode.id.
    """

    id: str
    buildable_class: str
    position: Position
    rotation: Rotation

    @classmethod
    def from_dict(cls, data: dict) -> "Buildable":
        return cls(
            id=data["id"],
            buildable_class=data["buildableClass"],
            position=Position.from_dict(data["position"]),
            rotation=Rotation.from_dict(data["rotation"]),
        )


@dataclass(frozen=True)
class FactoryConnection:
    """Mirrors FDocModFactoryConnectionTelemetry - one connection point,
    not a constructed edge. See satisfactory_ai.graph for how these are
    turned into a WorldGraph, and satisfactory_ai.layout for how
    position/normal are used to plan new-building placement.

    position/normal (added 2026-08-25) are the connector's real world
    position (no clearance offset) and outward-facing normal - see
    docs/telemetry-protocol.md's connections section for the
    Output-leaves-along-+normal / Input-arrives-along--normal rule these
    encode.
    """

    owner_buildable_id: str
    direction: str
    connected: bool
    connected_buildable_id: str
    position: Position
    normal: Position

    def __post_init__(self) -> None:
        if self.direction not in VALID_CONNECTION_DIRECTIONS:
            raise ValueError(
                f"direction {self.direction!r} not in {sorted(VALID_CONNECTION_DIRECTIONS)}"
            )

    @classmethod
    def from_dict(cls, data: dict) -> "FactoryConnection":
        return cls(
            owner_buildable_id=data["ownerBuildableId"],
            direction=data["direction"],
            connected=bool(data["connected"]),
            connected_buildable_id=data.get("connectedBuildableId", ""),
            position=Position.from_dict(data["position"]),
            normal=Position.from_dict(data["normal"]),
        )


@dataclass(frozen=True)
class ConveyorBeltTier:
    """Mirrors one entry of "world.conveyorBeltTiers" (added 2026-08-25).

    speed is AFGBuildableConveyorBase::GetSpeed(), read live off each
    tier's buildable class CDO - NOT a hardcoded/assumed items-per-minute
    figure. The unit isn't documented in the FactoryGame source
    ("Speed of this conveyor", no unit given) - treat as relative/
    comparable across tiers unless a live comparison against the game's
    own displayed items-per-minute numbers confirms the exact
    conversion. See docs/telemetry-protocol.md's conveyorBeltTiers
    section.

    max_spline_length/bend_radius/max_incline_degrees (added
    2026-08-25, may be None if the mod couldn't resolve that tier's
    hologram class) are the real per-tier limits
    (AFGConveyorBeltHologram::GetMaxSplineLength()/GetBendRadius(), and
    mMaxIncline read via reflection since it has no public getter) -
    use these with satisfactory_ai.conveyors to check whether a
    straight single-segment belt can reach between two connectors
    before attempting it.
    """

    recipe_class: str
    buildable_class: str
    speed: float
    max_spline_length: Optional[float] = None
    bend_radius: Optional[float] = None
    max_incline_degrees: Optional[float] = None

    @classmethod
    def from_dict(cls, data: dict) -> "ConveyorBeltTier":
        return cls(
            recipe_class=data["recipeClass"],
            buildable_class=data["buildableClass"],
            speed=float(data["speed"]),
            max_spline_length=float(data["maxSplineLength"]) if "maxSplineLength" in data else None,
            bend_radius=float(data["bendRadius"]) if "bendRadius" in data else None,
            max_incline_degrees=float(data["maxInclineDegrees"]) if "maxInclineDegrees" in data else None,
        )


@dataclass(frozen=True)
class PowerLineLimits:
    """Mirrors "world.powerLineLimits" (added 2026-08-25).

    Unlike ConveyorBeltTier.speed, these ARE documented-unit values -
    AFGBuildableWire::mMaxLength/mMaxPowerTowerLength/mLengthPerCost are
    plain public UPROPERTYs commented "[cm]" in FGBuildableWire.h, the
    same unit every position/distance value in this project's telemetry
    already uses, so max_length is directly comparable to a computed
    3D distance with no unknown-conversion caveat (contrast
    ConveyorBeltTier.speed's unconfirmed unit). Only one power line
    tier exists in the game (no Mk1..N like belts).
    """

    recipe_class: str
    buildable_class: str
    max_length: float
    max_power_tower_length: float
    length_per_cost: float

    @classmethod
    def from_dict(cls, data: dict) -> "PowerLineLimits":
        return cls(
            recipe_class=data["recipeClass"],
            buildable_class=data["buildableClass"],
            max_length=float(data["maxLength"]),
            max_power_tower_length=float(data["maxPowerTowerLength"]),
            length_per_cost=float(data["lengthPerCost"]),
        )


@dataclass(frozen=True)
class PipelineTier:
    """Mirrors one entry of "world.pipelineTiers" (added 2026-08-25,
    pipe groundwork - NOT YET LIVE-TESTED, see ConstructPipe's C++ doc
    comment for open questions).

    flow_limit is AFGBuildablePipeline::GetFlowLimit(), a documented-unit
    value ("[m^3/s]" per FGBuildablePipeline.h) - unlike
    ConveyorBeltTier.speed, directly usable without a unit-conversion
    caveat.

    max_spline_length/bend_radius/min_bend_radius (may be None if the
    mod couldn't resolve that tier's hologram class) are read via
    reflection off AFGPipelineHologram's CDO - all three are private
    fields with no public getter, unlike belts where two of three had
    public getters.
    """

    recipe_class: str
    buildable_class: str
    flow_limit: float
    max_spline_length: Optional[float] = None
    bend_radius: Optional[float] = None
    min_bend_radius: Optional[float] = None

    @classmethod
    def from_dict(cls, data: dict) -> "PipelineTier":
        return cls(
            recipe_class=data["recipeClass"],
            buildable_class=data["buildableClass"],
            flow_limit=float(data["flowLimit"]),
            max_spline_length=float(data["maxSplineLength"]) if "maxSplineLength" in data else None,
            bend_radius=float(data["bendRadius"]) if "bendRadius" in data else None,
            min_bend_radius=float(data["minBendRadius"]) if "minBendRadius" in data else None,
        )


@dataclass(frozen=True)
class ConveyorAttachmentInfo:
    """Mirrors one entry of "world.conveyorAttachments" (added
    2026-08-25, splitter/merger groundwork - NOT YET LIVE-TESTED). See
    docs/conveyor-attachment-research.md: splitters/mergers use the same
    simple, single-step hologram lineage already proven for Miners/
    Smelters/Constructors, NOT the spline branch belts/pipes needed -
    so placing and connecting them needs no dedicated toolkit module
    the way belts/power/pipes did (no distance/speed/flow limit to
    reason about, no chaining pattern) - just the existing
    ConveyorBeltTier-style catalog lookup below plus
    satisfactory_ai.graph/layout for wiring up connections.

    input_count/output_count are read live off each variant's real
    UFGFactoryConnectionComponents (not hardcoded to the commonly-known
    1-in/3-out splitter / 3-in/1-out merger figures).

    supports_sort_rules is true only for the Smart/Programmable
    Splitter variants (both backed by the same native
    AFGBuildableSplitterSmart class) - flags a REAL, separate,
    not-yet-built capability gap: per-output item-type routing needs
    its own future write operation DocMod does not have yet. Placement
    and belt connection work today for every variant regardless of this
    flag.
    """

    recipe_class: str
    buildable_class: str
    input_count: int
    output_count: int
    supports_sort_rules: bool

    @classmethod
    def from_dict(cls, data: dict) -> "ConveyorAttachmentInfo":
        return cls(
            recipe_class=data["recipeClass"],
            buildable_class=data["buildableClass"],
            input_count=int(data["inputCount"]),
            output_count=int(data["outputCount"]),
            supports_sort_rules=bool(data["supportsSortRules"]),
        )
