"""World graph construction (PLAN.md Phase 11).

PLAN.md is explicit that graph construction/analysis belongs on the
external controller, not the mod: "The external controller, not the SML
mod, should normally perform graph analysis." The mod (Phase 10) only
exposes raw facts - one row per buildable, one row per connection point
(see docs/telemetry-protocol.md's "buildables" and "connections" shapes,
satisfactory_ai.models.Buildable / FactoryConnection). This module turns
those facts into a directed graph.

Deliberately minimal: does not attempt to classify buildables into
PLAN.md's conceptual node types (resource/miner/machine/storage/splitter/
merger) by inspecting buildableClass name substrings - that would be
undocumented guessing about asset naming conventions never confirmed
against real game data (no runtime capture has happened yet - see
../docs/manual-verification.md). WorldGraph.buildables keeps the full
Buildable record (including its real buildable_class string) so that
classification can be added later once real captured data shows what
class paths actually look like, rather than guessed now.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Dict, Iterable, List

from .models import Buildable, FactoryConnection


@dataclass(frozen=True)
class Edge:
    from_id: str
    to_id: str


@dataclass(frozen=True)
class WorldGraph:
    buildables: Dict[str, Buildable]
    edges: tuple[Edge, ...]

    def outgoing(self, buildable_id: str) -> List[Edge]:
        return [e for e in self.edges if e.from_id == buildable_id]

    def incoming(self, buildable_id: str) -> List[Edge]:
        return [e for e in self.edges if e.to_id == buildable_id]


def build_world_graph(buildables: Iterable[Buildable], connections: Iterable[FactoryConnection]) -> WorldGraph:
    """Builds a directed graph from buildable/connection facts.

    One edge per Output-direction connection point that is actually
    connected. Input-direction rows are skipped: a single physical
    belt/pipe link produces two connection-point rows (an Output row on
    the source, an Input row on the destination, each naming the other
    via connected_buildable_id) - using only Output rows avoids emitting
    the same edge twice.
    """
    buildable_map = {b.id: b for b in buildables}

    edges = tuple(
        Edge(from_id=connection.owner_buildable_id, to_id=connection.connected_buildable_id)
        for connection in connections
        if connection.direction == "Output" and connection.connected and connection.connected_buildable_id
    )

    unknown_endpoints = {
        endpoint
        for edge in edges
        for endpoint in (edge.from_id, edge.to_id)
        if endpoint not in buildable_map
    }
    if unknown_endpoints:
        # Not fatal - a connection can reference a buildable telemetry
        # didn't happen to include in this particular snapshot (e.g. two
        # separate captures taken at different times) - but worth
        # surfacing rather than silently dropping.
        import warnings

        warnings.warn(
            f"{len(unknown_endpoints)} edge endpoint(s) have no matching Buildable record: "
            f"{sorted(unknown_endpoints)[:5]}{'...' if len(unknown_endpoints) > 5 else ''}"
        )

    return WorldGraph(buildables=buildable_map, edges=edges)
