"""Envelope parsing for the DocMod telemetry protocol.

See docs/telemetry-protocol.md at the repo root for the authoritative
schema. This parses the payload shapes DocMod's Log*AsJson functions
produce (and what the "world.*" RPC methods on the Phase 9 /rpc endpoint
return in their "result" field) - not the RPC request/response envelope
itself, which this package doesn't have a client for yet (see
controller/README.md - no network client until there's a reason to add
one beyond parsing already-captured JSON).
"""

from __future__ import annotations

import json
from dataclasses import dataclass
from typing import Optional

from .models import Buildable, ConveyorBeltTier, FactoryConnection, PipelineTier, PowerLineLimits, ResourceNode

SUPPORTED_PROTOCOL_VERSION = 1


class UnsupportedProtocolVersionError(ValueError):
    """Raised when telemetry declares a protocolVersion this package doesn't understand."""


def _check_protocol_version(data: dict) -> int:
    version = data["protocolVersion"]
    if version != SUPPORTED_PROTOCOL_VERSION:
        raise UnsupportedProtocolVersionError(
            f"protocolVersion {version} not supported (expected {SUPPORTED_PROTOCOL_VERSION})"
        )
    return version


@dataclass(frozen=True)
class ResourceNodeTelemetry:
    protocol_version: int
    resource_nodes: tuple[ResourceNode, ...]

    @classmethod
    def from_dict(cls, data: dict) -> "ResourceNodeTelemetry":
        version = _check_protocol_version(data)
        nodes = tuple(ResourceNode.from_dict(n) for n in data["resourceNodes"])
        return cls(protocol_version=version, resource_nodes=nodes)


@dataclass(frozen=True)
class BuildableTelemetry:
    protocol_version: int
    buildables: tuple[Buildable, ...]

    @classmethod
    def from_dict(cls, data: dict) -> "BuildableTelemetry":
        version = _check_protocol_version(data)
        buildables = tuple(Buildable.from_dict(b) for b in data["buildables"])
        return cls(protocol_version=version, buildables=buildables)


@dataclass(frozen=True)
class FactoryConnectionTelemetry:
    protocol_version: int
    connections: tuple[FactoryConnection, ...]

    @classmethod
    def from_dict(cls, data: dict) -> "FactoryConnectionTelemetry":
        version = _check_protocol_version(data)
        connections = tuple(FactoryConnection.from_dict(c) for c in data["connections"])
        return cls(protocol_version=version, connections=connections)


@dataclass(frozen=True)
class ConveyorBeltTierTelemetry:
    protocol_version: int
    tiers: tuple[ConveyorBeltTier, ...]

    @classmethod
    def from_dict(cls, data: dict) -> "ConveyorBeltTierTelemetry":
        version = _check_protocol_version(data)
        tiers = tuple(ConveyorBeltTier.from_dict(t) for t in data["tiers"])
        return cls(protocol_version=version, tiers=tiers)


@dataclass(frozen=True)
class PipelineTierTelemetry:
    """Mirrors "world.pipelineTiers" (added 2026-08-25, pipe groundwork
    - NOT YET LIVE-TESTED). Same array shape as ConveyorBeltTierTelemetry
    - LogPipelineTiersAsJson skips (not errors on) any recipe whose
    buildable CDO fails to resolve, so "tiers" may contain fewer than
    two entries."""

    protocol_version: int
    tiers: tuple[PipelineTier, ...]

    @classmethod
    def from_dict(cls, data: dict) -> "PipelineTierTelemetry":
        version = _check_protocol_version(data)
        tiers = tuple(PipelineTier.from_dict(t) for t in data["tiers"])
        return cls(protocol_version=version, tiers=tiers)


def parse_resource_node_telemetry(json_text: str) -> ResourceNodeTelemetry:
    return ResourceNodeTelemetry.from_dict(json.loads(json_text))


def parse_buildable_telemetry(json_text: str) -> BuildableTelemetry:
    return BuildableTelemetry.from_dict(json.loads(json_text))


def parse_factory_connection_telemetry(json_text: str) -> FactoryConnectionTelemetry:
    return FactoryConnectionTelemetry.from_dict(json.loads(json_text))


def parse_conveyor_belt_tier_telemetry(json_text: str) -> ConveyorBeltTierTelemetry:
    return ConveyorBeltTierTelemetry.from_dict(json.loads(json_text))


def parse_power_line_limits(json_text: str) -> Optional[PowerLineLimits]:
    """Returns None if the mod couldn't resolve Recipe_PowerLine's
    buildable CDO (the JSON then has only "protocolVersion", no other
    fields) - matches LogPowerLineLimitsAsJson's C++ behavior of
    omitting all fields rather than erroring in that case."""
    data = json.loads(json_text)
    _check_protocol_version(data)
    if "maxLength" not in data:
        return None
    return PowerLineLimits.from_dict(data)


def parse_pipeline_tier_telemetry(json_text: str) -> PipelineTierTelemetry:
    return PipelineTierTelemetry.from_dict(json.loads(json_text))
