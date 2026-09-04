"""Catwalk / walkway placement helpers.

Deterministic geometry for placing catwalk T-junctions, turns and ramps with
CORRECT orientation, so future builds don't repeat the manual "which way does
the rail face" round-trips from the HUB-building session (2026-09-03).

Two sources of truth, in priority order:

1. world.connectorLayout(buildableClass) now returns a ``walkway`` object
   (AIMod 2026-09-03) with ``size``, ``elevation``, ``rampHighLocalDir`` and
   ``railLocalNormals`` (the mDisableSnapOn sides == railed/closed sides, as
   local-frame normals). From those, the yaw to face a rail (or climb a ramp)
   in any world direction is COMPUTED - no eyeballing. This is the robust path
   and works for piece types we've never used.

2. If that telemetry is unavailable (older mod), the module falls back to the
   conventions calibrated live against the finished HUB building:
     * Catwalk_T rail faces outward: E=0, N=90, W=180, S=270 (deg yaw)
     * Catwalk_Turn corner (rails on the two outer sides):
         SE=0, NE=90, NW=180, SW=270
     * Catwalk_Ramp HIGH end by yaw: 0->+X, 90->+Y, 180->-X, 270->-Y
   Pieces are 400u square; deck sits at the placed Z; a ramp rises 200u over its
   run (low deck at placed Z, high at +200).

Yaw sign: +yaw is a CCW rotation of a local vector in world XY (empirically:
a catwalk ramp's local +X high end points world +X at yaw0, world +Y at yaw90).
World axes here: +X = East, +Y = North (matches the game's HUB-area frame).
"""
from __future__ import annotations

import math
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Sequence, Tuple

CATWALK_SIZE = 400.0        # one catwalk cell, world units (verified via bounds)
CATWALK_RAMP_RISE = 200.0   # deck rise over one ramp piece (verified via bounds)

# Catwalks AND walkways are both AFGBuildableWalkway, so the same walkway
# telemetry + the same solvers/ring builder drive either family - only the
# recipe/class paths differ. A PieceSet bundles one family's paths.
@dataclass
class PieceSet:
    name: str
    recipe_t: str
    recipe_turn: str
    recipe_ramp: str
    class_t: str        # Build_* path for world.connectorLayout (walkway info)
    class_turn: str
    class_ramp: str


_CAT = "/Game/FactoryGame/Recipes/Buildings/Catwalks/"
_WALK = "/Game/FactoryGame/Recipes/Buildings/Walkways/"


def _catwalk_pieceset() -> "PieceSet":
    return PieceSet(
        name="catwalk",
        recipe_t=_CAT + "Recipe_Catwalk_T.Recipe_Catwalk_T_C",
        recipe_turn=_CAT + "Recipe_Catwalk_Turn.Recipe_Catwalk_Turn_C",
        recipe_ramp=_CAT + "Recipe_Catwalk_Ramp.Recipe_Catwalk_Ramp_C",
        # Build_* class paths are resolved lazily from world.buildables if a
        # literal path ever drifts; these are the observed ones.
        class_t="/Game/FactoryGame/Buildable/Building/Catwalk/Build_CatwalkT.Build_CatwalkT_C",
        class_turn="/Game/FactoryGame/Buildable/Building/Catwalk/Build_CatwalkCorner.Build_CatwalkCorner_C",
        class_ramp="/Game/FactoryGame/Buildable/Building/Catwalk/Build_CatwalkRamp.Build_CatwalkRamp_C",
    )


def _walkway_pieceset() -> "PieceSet":
    return PieceSet(
        name="walkway",
        recipe_t=_WALK + "Recipe_Walkway_T.Recipe_Walkway_T_C",
        recipe_turn=_WALK + "Recipe_Walkway_Turn.Recipe_Walkway_Turn_C",
        recipe_ramp=_WALK + "Recipe_Walkway_Ramp.Recipe_Walkway_Ramp_C",
        class_t="/Game/FactoryGame/Buildable/Building/Walkways/Build_WalkwayT.Build_WalkwayT_C",
        class_turn="/Game/FactoryGame/Buildable/Building/Walkways/Build_WalkwayCorner.Build_WalkwayCorner_C",
        class_ramp="/Game/FactoryGame/Buildable/Building/Walkways/Build_WalkwayRamp.Build_WalkwayRamp_C",
    )


CATWALK = _catwalk_pieceset()
WALKWAY = _walkway_pieceset()

# Back-compat aliases (catwalk family).
RECIPE_T, RECIPE_TURN, RECIPE_RAMP = CATWALK.recipe_t, CATWALK.recipe_turn, CATWALK.recipe_ramp
CLASS_T, CLASS_TURN, CLASS_RAMP = CATWALK.class_t, CATWALK.class_turn, CATWALK.class_ramp

# Cardinal world directions (unit XY).
EAST, NORTH, WEST, SOUTH = (1.0, 0.0), (0.0, 1.0), (-1.0, 0.0), (0.0, -1.0)


def _ang(v: Tuple[float, float]) -> float:
    return math.degrees(math.atan2(v[1], v[0]))


def _rot(v: Tuple[float, float], yaw_deg: float) -> Tuple[float, float]:
    """Rotate a local XY vector by +yaw (CCW) into world XY."""
    r = math.radians(yaw_deg)
    c, s = math.cos(r), math.sin(r)
    return (v[0] * c - v[1] * s, v[0] * s + v[1] * c)


def _snap90(yaw: float) -> int:
    return int(round((yaw % 360) / 90.0) * 90) % 360


@dataclass
class WalkwayInfo:
    """Parsed ``walkway`` block from world.connectorLayout, or a fallback."""
    size: float = CATWALK_SIZE
    elevation: float = 0.0
    ramp_high_local: Tuple[float, float] = (1.0, 0.0)      # local +X
    rail_local_normals: List[Tuple[float, float]] = field(default_factory=list)
    from_telemetry: bool = False


def get_walkway_info(rpc, buildable_class_path: str) -> WalkwayInfo:
    """Query world.connectorLayout and parse the walkway block (if present)."""
    try:
        r = rpc.call("world.connectorLayout", {"buildableClass": buildable_class_path}, timeout_seconds=30)
    except Exception:
        return WalkwayInfo()
    w = r.get("walkway") if isinstance(r, dict) else None
    if not w:
        return WalkwayInfo()
    rails = [(n.get("x", 0.0), n.get("y", 0.0)) for n in w.get("railLocalNormals", [])]
    hd = w.get("rampHighLocalDir", {"x": 1.0, "y": 0.0})
    return WalkwayInfo(
        size=float(w.get("size", CATWALK_SIZE)) or CATWALK_SIZE,
        elevation=float(w.get("elevation", 0.0)),
        ramp_high_local=(hd.get("x", 1.0), hd.get("y", 0.0)),
        rail_local_normals=[(x, y) for x, y in rails if abs(x) + abs(y) > 0.5],
        from_telemetry=True,
    )


# ---- yaw solvers ---------------------------------------------------------

def t_yaw_for_outer_rail(world_out: Tuple[float, float], info: Optional[WalkwayInfo] = None) -> int:
    """Yaw for a T so its single rail faces `world_out` (branch opposite)."""
    if info and info.from_telemetry and len(info.rail_local_normals) == 1:
        return _snap90(_ang(world_out) - _ang(info.rail_local_normals[0]))
    # fallback: rail local +X (Front) => E=0/N=90/W=180/S=270
    return _snap90(_ang(world_out))


def turn_yaw_for_corner(world_outs: Sequence[Tuple[float, float]], info: Optional[WalkwayInfo] = None) -> int:
    """Yaw for a Turn so its two rails face the two `world_outs` directions."""
    target = {(_snap90(_ang(d))) for d in world_outs}
    if info and info.from_telemetry and len(info.rail_local_normals) == 2:
        for yaw in (0, 90, 180, 270):
            got = {_snap90(_ang(_rot(n, yaw))) for n in info.rail_local_normals}
            if got == target:
                return yaw
    # fallback conventions: SE=0, NE=90, NW=180, SW=270
    fallback = {frozenset({0, 270}): 0, frozenset({0, 90}): 90,
                frozenset({90, 180}): 180, frozenset({180, 270}): 270}
    key = frozenset(_snap90(_ang(d)) for d in world_outs)
    return fallback.get(key, 0)


def ramp_yaw_for_climb(climb_dir: Tuple[float, float], info: Optional[WalkwayInfo] = None) -> int:
    """Yaw for a ramp so the HIGH (up) end faces `climb_dir`."""
    if info and info.from_telemetry:
        return _snap90(_ang(climb_dir) - _ang(info.ramp_high_local))
    return _snap90(_ang(climb_dir))  # local +X high => 0->+X, 90->+Y ...


# ---- ring builder --------------------------------------------------------

def catwalk_ring(cx0: float, cy0: float, cx1: float, cy1: float, level: float,
                 t_info: Optional[WalkwayInfo] = None, turn_info: Optional[WalkwayInfo] = None,
                 pieces: "PieceSet" = CATWALK) -> List[dict]:
    """Placement ops for one flat ring hugging a rectangle (catwalk OR walkway).

    (cx0,cy0)-(cx1,cy1) are the RING centerline corners (already offset outside
    the building). Emits T's along each side (rail outward) at CATWALK_SIZE
    spacing and a Turn at each corner. `pieces` selects the family (CATWALK or
    WALKWAY). Returns world.placeBuilding op dicts (recipeClass/x/y/z/yaw);
    caller adds bypass flags + batches.
    """
    x0, x1 = sorted((cx0, cx1))
    y0, y1 = sorted((cy0, cy1))
    ops: List[dict] = []
    RECIPE_T, RECIPE_TURN = pieces.recipe_t, pieces.recipe_turn

    def op(recipe, x, y, yaw):
        ops.append({"recipeClass": recipe, "x": float(x), "y": float(y), "z": float(level), "yaw": float(yaw)})

    def span(a, b):
        # interior cell centers between corners a<b at CATWALK_SIZE, no overhang
        n = int(round((b - a) / CATWALK_SIZE))
        return [a + CATWALK_SIZE * (i + 0.5) for i in range(n - 1)] if n >= 2 else []
    # sides
    for x in span(x0, x1):
        op(RECIPE_T, x, y0, t_yaw_for_outer_rail(SOUTH, t_info))
        op(RECIPE_T, x, y1, t_yaw_for_outer_rail(NORTH, t_info))
    for y in span(y0, y1):
        op(RECIPE_T, x0, y, t_yaw_for_outer_rail(WEST, t_info))
        op(RECIPE_T, x1, y, t_yaw_for_outer_rail(EAST, t_info))
    # corners
    op(RECIPE_TURN, x1, y0, turn_yaw_for_corner((SOUTH, EAST), turn_info))   # SE
    op(RECIPE_TURN, x1, y1, turn_yaw_for_corner((NORTH, EAST), turn_info))   # NE
    op(RECIPE_TURN, x0, y1, turn_yaw_for_corner((NORTH, WEST), turn_info))   # NW
    op(RECIPE_TURN, x0, y0, turn_yaw_for_corner((SOUTH, WEST), turn_info))   # SW
    return ops
