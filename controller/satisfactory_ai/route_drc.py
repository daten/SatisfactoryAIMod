"""Belt/lift route design-rule check (DRC) — added 2026-09-09.

The geometry "does this planned (or built) belt/lift route overlap anything it
shouldn't?" check, for laying out clean circuit-board-style factories. Given a
set of route SEGMENTS (each a straight run with a clearance radius) plus the
world's obstacle boxes (machine/foundation clearance AABBs from world.buildables
`bounds`), other belt segments, and optionally the terrain height, it returns a
list of violations - belt-vs-machine, belt-vs-foundation, belt-vs-belt,
belt-below-terrain - BEFORE you commit the build.

Toolkit, not a router (per [[feedback_dont_prebake_agent_decisions]]): it does
not choose or repair a layout, it verifies one. Pair it with belt_route.py
(which turns a chosen waypoint path into segments) and world.buildables /
world.splineGeometry / world.terrainHeightGrid for the real obstacle data.
Deterministic; every function takes explicit inputs.

Geometry model: each segment is a capsule (line + radius); each obstacle is an
AABB. Segment-vs-obstacle inflates the AABB by the segment radius and clips the
line against it (conservative - treats the capsule's rounded ends as boxy, which
only ever OVER-reports a near-miss, never misses a real overlap). Segment-vs-
segment uses the true closest distance between two 3D segments.
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field
from typing import Callable, List, Optional, Sequence, Tuple

from .models import Position

# A belt/lift's clearance half-width (approx; confirm live). The conveyor mesh +
# clearance is narrow; 100 units (~1 m) each side is a safe default for spacing
# checks. Callers can override per segment.
DEFAULT_BELT_CLEARANCE = 100.0


@dataclass(frozen=True)
class RouteSegment:
    """One straight belt/lift run to check."""
    p0: Position
    p1: Position
    radius: float = DEFAULT_BELT_CLEARANCE
    label: str = ""
    # obstacle ids this segment is ALLOWED to touch (the machines it connects
    # into at its endpoints - a belt legitimately meets its source/dest).
    ignore_ids: Tuple[str, ...] = ()


@dataclass(frozen=True)
class Obstacle:
    """An axis-aligned clearance box (e.g. from world.buildables `bounds`)."""
    id: str
    box_min: Position
    box_max: Position
    kind: str = "buildable"   # "machine" | "foundation" | "buildable" | ...


@dataclass(frozen=True)
class Violation:
    kind: str            # "belt-vs-obstacle" | "belt-vs-belt" | "belt-below-terrain"
    segment_label: str
    detail: str
    other: str = ""      # obstacle id / other segment label
    separation: float = 0.0  # how far short of clearance (negative = overlap depth)


# ---------------------------------------------------------------------------
# geometry primitives
# ---------------------------------------------------------------------------
def _seg_aabb_intersect(p0: Position, p1: Position, bmin: Position, bmax: Position) -> bool:
    """Liang-Barsky style slab clip: does the segment p0->p1 intersect the AABB?"""
    d = (p1.x - p0.x, p1.y - p0.y, p1.z - p0.z)
    lo, hi = 0.0, 1.0
    for o, dd, mn, mx in (
        (p0.x, d[0], bmin.x, bmax.x),
        (p0.y, d[1], bmin.y, bmax.y),
        (p0.z, d[2], bmin.z, bmax.z),
    ):
        if abs(dd) < 1e-12:
            if o < mn or o > mx:
                return False
            continue
        t1 = (mn - o) / dd
        t2 = (mx - o) / dd
        if t1 > t2:
            t1, t2 = t2, t1
        lo = max(lo, t1)
        hi = min(hi, t2)
        if lo > hi:
            return False
    return True


def _clamp(v: float, lo: float, hi: float) -> float:
    return lo if v < lo else hi if v > hi else v


def segment_segment_distance(a0: Position, a1: Position, b0: Position, b1: Position) -> float:
    """Closest distance between two 3D line segments."""
    ux, uy, uz = a1.x - a0.x, a1.y - a0.y, a1.z - a0.z
    vx, vy, vz = b1.x - b0.x, b1.y - b0.y, b1.z - b0.z
    wx, wy, wz = a0.x - b0.x, a0.y - b0.y, a0.z - b0.z
    a = ux * ux + uy * uy + uz * uz
    b = ux * vx + uy * vy + uz * vz
    c = vx * vx + vy * vy + vz * vz
    d = ux * wx + uy * wy + uz * wz
    e = vx * wx + vy * wy + vz * wz
    den = a * c - b * b
    if den < 1e-12:  # parallel
        sc = 0.0
        tc = (e / c) if c > 1e-12 else 0.0
    else:
        sc = (b * e - c * d) / den
        tc = (a * e - b * d) / den
    sc = _clamp(sc, 0.0, 1.0)
    tc = _clamp(tc, 0.0, 1.0)
    # refine after clamping (standard segment-segment)
    if c > 1e-12:
        tc = _clamp((e + b * sc) / c, 0.0, 1.0)
    if a > 1e-12:
        sc = _clamp((b * tc - d) / a, 0.0, 1.0)
    cx = (a0.x + sc * ux) - (b0.x + tc * vx)
    cy = (a0.y + sc * uy) - (b0.y + tc * vy)
    cz = (a0.z + sc * uz) - (b0.z + tc * vz)
    return math.sqrt(cx * cx + cy * cy + cz * cz)


def _shares_endpoint(s: RouteSegment, t: RouteSegment, tol: float = 1.0) -> bool:
    for pa in (s.p0, s.p1):
        for pb in (t.p0, t.p1):
            if abs(pa.x - pb.x) < tol and abs(pa.y - pb.y) < tol and abs(pa.z - pb.z) < tol:
                return True
    return False


# ---------------------------------------------------------------------------
# checks
# ---------------------------------------------------------------------------
def check_route(segments: Sequence[RouteSegment],
                obstacles: Sequence[Obstacle],
                ground_z: Optional[Callable[[float, float], Optional[float]]] = None,
                terrain_samples: int = 8) -> List[Violation]:
    """Return all design-rule violations for the given route.

    obstacles: machine/foundation/other clearance AABBs (map world.buildables
        `bounds` -> Obstacle; a segment won't be flagged against ids in its
        ignore_ids, i.e. the machines it connects into).
    ground_z: optional (x, y) -> terrain height (from world.terrainHeightGrid);
        a segment sampled below terrain is a belt-below-terrain violation.
    """
    out: List[Violation] = []

    # belt vs obstacle (inflate AABB by the segment's clearance radius)
    for s in segments:
        for ob in obstacles:
            if ob.id in s.ignore_ids:
                continue
            r = s.radius
            bmin = Position(ob.box_min.x - r, ob.box_min.y - r, ob.box_min.z - r)
            bmax = Position(ob.box_max.x + r, ob.box_max.y + r, ob.box_max.z + r)
            if _seg_aabb_intersect(s.p0, s.p1, bmin, bmax):
                out.append(Violation(kind="belt-vs-obstacle", segment_label=s.label,
                                     detail=f"belt '{s.label}' clears into {ob.kind} {ob.id}",
                                     other=ob.id))

    # belt vs belt (skip segments that legitimately share an endpoint, e.g. a
    # pole chain)
    for i in range(len(segments)):
        for j in range(i + 1, len(segments)):
            s, t = segments[i], segments[j]
            if _shares_endpoint(s, t):
                continue
            dist = segment_segment_distance(s.p0, s.p1, t.p0, t.p1)
            need = s.radius + t.radius
            if dist < need:
                out.append(Violation(kind="belt-vs-belt", segment_label=s.label,
                                     detail=f"belts '{s.label}' and '{t.label}' pass within {dist:.0f} (< {need:.0f})",
                                     other=t.label, separation=dist - need))

    # belt vs terrain
    if ground_z is not None:
        for s in segments:
            n = max(2, terrain_samples)
            for k in range(n + 1):
                f = k / n
                x = s.p0.x + f * (s.p1.x - s.p0.x)
                y = s.p0.y + f * (s.p1.y - s.p0.y)
                z = s.p0.z + f * (s.p1.z - s.p0.z)
                g = ground_z(x, y)
                if g is not None and z - s.radius < g:
                    out.append(Violation(kind="belt-below-terrain", segment_label=s.label,
                                         detail=f"belt '{s.label}' at ({x:.0f},{y:.0f}) z={z:.0f} is under terrain z={g:.0f}",
                                         separation=(z - s.radius) - g))
                    break
    return out
