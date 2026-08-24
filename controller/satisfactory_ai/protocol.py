"""Envelope parsing for the DocMod telemetry protocol.

See docs/telemetry-protocol.md at the repo root for the authoritative
schema. Only protocolVersion 1 / the resourceNodes payload exists on the
mod side so far (Phase 6). No request/response RPC envelope exists yet
since no transport exists yet (Phase 9) - this only parses the
logged/captured JSON blob DocMod currently produces via
LogResourceNodesAsJson.
"""

from __future__ import annotations

import json
from dataclasses import dataclass

from .models import ResourceNode

SUPPORTED_PROTOCOL_VERSION = 1


class UnsupportedProtocolVersionError(ValueError):
    """Raised when telemetry declares a protocolVersion this package doesn't understand."""


@dataclass(frozen=True)
class ResourceNodeTelemetry:
    protocol_version: int
    resource_nodes: tuple[ResourceNode, ...]

    @classmethod
    def from_dict(cls, data: dict) -> "ResourceNodeTelemetry":
        version = data["protocolVersion"]
        if version != SUPPORTED_PROTOCOL_VERSION:
            raise UnsupportedProtocolVersionError(
                f"protocolVersion {version} not supported (expected {SUPPORTED_PROTOCOL_VERSION})"
            )
        nodes = tuple(ResourceNode.from_dict(n) for n in data["resourceNodes"])
        return cls(protocol_version=version, resource_nodes=nodes)


def parse_resource_node_telemetry(json_text: str) -> ResourceNodeTelemetry:
    return ResourceNodeTelemetry.from_dict(json.loads(json_text))
