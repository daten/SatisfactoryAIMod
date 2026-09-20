"""Resource-node siting toolkit (added 2026-09-09).

Turns raw `world.resourceNodes` telemetry into the facts an agent needs to pick
where to put a factory: nodes grouped by resource, extraction rates per node
(purity x miner Mk), spatial clusters (candidate multi-resource sites with a
centroid), and - composing directly with recipe_tree - which clusters can supply
a given BOM's raw demand, with any deficits.

Toolkit, not a chooser (per [[feedback_dont_prebake_agent_decisions]]): it
EXPOSES rates/clusters/coverage and can rank them, but it does not pick THE site,
place miners, or call any RPC. The agent decides, then builds with the placement
toolkit. Deterministic; every function takes explicit inputs.

Scope: SOLID resource nodes mined by the standard Miner (Mk1/2/3). Liquids
(water pumps, crude-oil extractors) and resource wells have different extraction
mechanics and are out of scope here - filter them out or handle separately.

Miner rates are standard community constants (like production.py's): confirm live
against world telemetry before trusting an exact number for a real build.
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field
from typing import Dict, Iterable, List, Optional

from .models import Position, ResourceNode

# Miner Mk1 mines a NORMAL node at 60/min; Impure = 0.5x, Pure = 2x; each miner
# tier doubles (Mk1 1x, Mk2 2x, Mk3 4x). All scale linearly with clock %.
MINER_MK1_NORMAL_RATE = 60.0
PURITY_MULTIPLIER = {"Impure": 0.5, "Normal": 1.0, "Pure": 2.0}
MINER_MK_MULTIPLIER = {1: 1.0, 2: 2.0, 3: 4.0}


def extraction_rate(purity: str, miner_mk: int = 1, clock_percent: float = 100.0) -> float:
    """Items/min a single Miner produces on a node of the given purity."""
    if purity not in PURITY_MULTIPLIER:
        raise ValueError(f"unknown purity {purity!r}")
    if miner_mk not in MINER_MK_MULTIPLIER:
        raise ValueError(f"miner_mk must be one of {sorted(MINER_MK_MULTIPLIER)}")
    return MINER_MK1_NORMAL_RATE * PURITY_MULTIPLIER[purity] * MINER_MK_MULTIPLIER[miner_mk] * (clock_percent / 100.0)


def distance(a: Position, b: Position) -> float:
    return math.sqrt((a.x - b.x) ** 2 + (a.y - b.y) ** 2 + (a.z - b.z) ** 2)


def _centroid(nodes: List[ResourceNode]) -> Position:
    n = len(nodes)
    return Position(x=sum(nd.position.x for nd in nodes) / n,
                    y=sum(nd.position.y for nd in nodes) / n,
                    z=sum(nd.position.z for nd in nodes) / n)


def group_by_resource(nodes: Iterable[ResourceNode]) -> Dict[str, List[ResourceNode]]:
    """{resource_class: [ResourceNode]}."""
    out: Dict[str, List[ResourceNode]] = {}
    for nd in nodes:
        out.setdefault(nd.resource_class, []).append(nd)
    return out


def available_rate_by_resource(nodes: Iterable[ResourceNode], miner_mk: int = 1,
                               include_occupied: bool = False) -> Dict[str, float]:
    """Total items/min available per resource_class if every (free) node gets a
    Miner of miner_mk at 100%. Occupied nodes are excluded unless asked."""
    out: Dict[str, float] = {}
    for nd in nodes:
        if nd.occupied and not include_occupied:
            continue
        out[nd.resource_class] = out.get(nd.resource_class, 0.0) + extraction_rate(nd.purity, miner_mk)
    return out


@dataclass(frozen=True)
class NodeCluster:
    """A spatial group of resource nodes = a candidate factory site region."""
    nodes: List[ResourceNode]
    centroid: Position

    @property
    def resource_classes(self) -> List[str]:
        return sorted({nd.resource_class for nd in self.nodes})

    def rates(self, miner_mk: int = 1) -> Dict[str, float]:
        return available_rate_by_resource(self.nodes, miner_mk)

    def radius(self) -> float:
        return max((distance(self.centroid, nd.position) for nd in self.nodes), default=0.0)


def cluster_nodes(nodes: Iterable[ResourceNode], link_radius: float,
                  include_occupied: bool = False) -> List[NodeCluster]:
    """Single-linkage spatial clustering: nodes within `link_radius` (3D) of a
    cluster member join it. Each cluster is a candidate site region spanning
    whatever resources fall in it. Occupied nodes excluded unless asked.
    Clusters are returned largest-first (by node count)."""
    pool = [nd for nd in nodes if include_occupied or not nd.occupied]
    parent = list(range(len(pool)))

    def find(i: int) -> int:
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i

    def union(i: int, j: int) -> None:
        ri, rj = find(i), find(j)
        if ri != rj:
            parent[ri] = rj

    r2 = link_radius * link_radius
    for i in range(len(pool)):
        pi = pool[i].position
        for j in range(i + 1, len(pool)):
            pj = pool[j].position
            if (pi.x - pj.x) ** 2 + (pi.y - pj.y) ** 2 + (pi.z - pj.z) ** 2 <= r2:
                union(i, j)

    groups: Dict[int, List[ResourceNode]] = {}
    for i, nd in enumerate(pool):
        groups.setdefault(find(i), []).append(nd)
    clusters = [NodeCluster(nodes=g, centroid=_centroid(g)) for g in groups.values()]
    clusters.sort(key=lambda c: len(c.nodes), reverse=True)
    return clusters


@dataclass(frozen=True)
class SiteCoverage:
    """How well one cluster supplies a demand (e.g. a BOM's raw_totals)."""
    cluster: NodeCluster
    demand: Dict[str, float]                 # resource_class -> needed items/min
    available: Dict[str, float]              # resource_class -> available items/min
    deficits: Dict[str, float]               # resource_class -> shortfall (>0 means short)
    satisfied: bool                          # all demanded resources fully covered
    total_deficit: float
    covered_count: int                       # demanded resources with 0 deficit


def evaluate_site(cluster: NodeCluster, demand: Dict[str, float], miner_mk: int = 1) -> SiteCoverage:
    """Does this cluster's extraction meet `demand` (resource_class -> per-min)?"""
    avail = cluster.rates(miner_mk)
    deficits: Dict[str, float] = {}
    covered = 0
    for rc, need in demand.items():
        have = avail.get(rc, 0.0)
        short = max(0.0, need - have)
        deficits[rc] = short
        if short <= 1e-9:
            covered += 1
    total_def = sum(deficits.values())
    return SiteCoverage(cluster=cluster, demand=dict(demand),
                        available={rc: avail.get(rc, 0.0) for rc in demand},
                        deficits=deficits, satisfied=(total_def <= 1e-9),
                        total_deficit=total_def, covered_count=covered)


def rank_sites_for_demand(nodes: Iterable[ResourceNode], demand: Dict[str, float],
                          link_radius: float, miner_mk: int = 1) -> List[SiteCoverage]:
    """Cluster `nodes`, evaluate each against `demand`, and return them ranked:
    fully-satisfied first, then most demanded-resources covered, then smallest
    total deficit, then most compact. EXPOSES the ranked options - the agent
    picks (miner tier, whether a partial site + imports is acceptable, etc.).
    `demand` composes directly with recipe_tree's Bom.raw_totals."""
    clusters = cluster_nodes(nodes, link_radius)
    scored = [evaluate_site(c, demand, miner_mk) for c in clusters]
    scored.sort(key=lambda s: (not s.satisfied, -s.covered_count, s.total_deficit, s.cluster.radius()))
    return scored
