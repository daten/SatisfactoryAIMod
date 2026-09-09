"""Recursive recipe dependency / bill-of-materials solver (added 2026-09-09).

Answers the "what do I actually need to build?" question deterministically:
given a target part and a desired output rate, expand the full recipe tree and
return, for every intermediate part, how many machines (of which building) at
what clock %, the total RAW-resource rates the chain consumes, and any
byproducts. This is the recipe-dependency / production-arithmetic /
resource-allocation work CLAUDE.md explicitly assigns to the deterministic
Python layer ("Do not use an LLM to perform large volumes of arithmetic") -
`production.py` only did single-recipe rates and two-stage balancing; this does
the arbitrary-depth tree.

Design boundary (per [[feedback_dont_prebake_agent_decisions]]): this COMPUTES
the requirement, it does not silently choose a plan. When a part has more than
one non-alternate recipe, or the caller wants an Alternate recipe, the solver
does NOT pick for you - it raises RecipeChoiceNeeded listing the options, and
the caller passes `recipe_choices={itemClass: recipeClass}`. Likewise the caller
decides what to treat as RAW (a leaf it will source externally) via `raw_items`;
by default the leaves are exactly the parts no non-building recipe produces
(ores, water, etc.). Nothing here queries or mutates the game.

Data source: the offline catalog snapshot (controller/catalog_cache.json, from
export_catalog.py) or any recipes/items list passed in. Catalog is a snapshot -
re-export it after a game/mod update; this module never trusts it as live truth.

CLI:
    python -m satisfactory_ai.recipe_tree "Iron Plate" 60
    python -m satisfactory_ai.recipe_tree "Reinforced Iron Plate" 10 --raw "Iron Ingot"
    python -m satisfactory_ai.recipe_tree "Iron Ingot" 30 --choose Desc_IronIngot_C=Recipe_IngotIron_C
"""

from __future__ import annotations

import json
import math
import os
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple


# ---------------------------------------------------------------------------
# Catalog model
# ---------------------------------------------------------------------------
@dataclass(frozen=True)
class RecipeDef:
    recipe_class: str
    display_name: str
    duration_seconds: float
    # (itemClass, amount_per_craft)
    ingredients: Tuple[Tuple[str, float], ...]
    products: Tuple[Tuple[str, float], ...]
    produced_in: Tuple[str, ...]
    is_building_recipe: bool

    def per_min(self, amount_per_craft: float) -> float:
        """Convert an amount-per-craft to items/minute at 100% clock."""
        if self.duration_seconds <= 0:
            raise ValueError(f"recipe {self.recipe_class} has non-positive duration")
        return amount_per_craft * (60.0 / self.duration_seconds)

    def product_amount(self, item_class: str) -> Optional[float]:
        for ic, amt in self.products:
            if ic == item_class:
                return amt
        return None

    @property
    def is_alternate(self) -> bool:
        return self.display_name.strip().lower().startswith("alternate")


class RecipeChoiceNeeded(Exception):
    """Raised when a part has >1 candidate recipe and the caller must pick.
    `options` are the RecipeDefs to choose among; pass the chosen one's
    recipe_class in solve_bom(recipe_choices={item_class: recipe_class})."""

    def __init__(self, item_class: str, item_name: str, options: List[RecipeDef]):
        self.item_class = item_class
        self.item_name = item_name
        self.options = options
        opt = "; ".join(f"{o.display_name} [{_short(o.recipe_class)}]" for o in options)
        super().__init__(f"'{item_name}' ({_short(item_class)}) has {len(options)} recipes - choose one via "
                         f"recipe_choices={{'{item_class}': '<recipeClass>'}}. Options: {opt}")


class RecipeCycle(Exception):
    """A recipe cycle was hit (a part depends on itself through the chosen
    recipes). Break it by marking one item raw or choosing a different recipe."""


def _short(class_path: str) -> str:
    return class_path.rsplit(".", 1)[-1] if class_path else class_path


# Extracted raw resources live under /RawResources/ (ore, coal, water, sulfur,
# limestone, SAM, quartz, bauxite, crude oil, nitrogen, uranium, ...). They are
# tree LEAVES by default even though converter/alternate recipes (e.g. "Iron Ore
# (Limestone)") can technically "produce" them - expanding those by default
# would turn a simple ore requirement into a limestone/sulfur/SAM rabbit hole.
# An agent that deliberately wants a converter chain marks the resource NON-raw
# by choosing its recipe explicitly (advanced; out of scope for the default BOM).
RAW_RESOURCE_MARKER = "/RawResources/"

# Satisfactory machine power scales with clock as base * clock^exponent, with
# exponent = log2(2.5) (a 2.5x clock draws 5x power). Underclocking is therefore
# power-sublinear (2 machines at 75% draw less than 1.5 at 100%). Standard
# community constant - confirm live if a build depends on an exact wattage.
POWER_CLOCK_EXPONENT = 1.321928094887362  # log2(2.5)

# FLUID/GAS recipe amounts are stored in millilitres in the catalog (Water
# "4000" = 4 m3); the in-game rate is m3/min, i.e. amount/1000. Solids are whole
# units. We normalize Liquid/Gas amounts to m3 at parse so every rate, machine
# count, and raw total is in consistent game units. (Machine counts alone would
# cancel the 1000x, but raw totals / byproducts / displayed fluid rates would be
# 1000x wrong without this - confirmed live: Plastic = 3000 mL crude -> 30/min.)
FLUID_FORMS = frozenset({"Liquid", "Gas"})
FLUID_SCALE = 1000.0


def _building_token(class_path: str) -> str:
    """Normalize a Desc_/Build_ building class to a shared token, e.g.
    Desc_AssemblerMk1_C and Build_AssemblerMk1_C both -> 'AssemblerMk1'."""
    s = _short(class_path)
    for pre in ("Desc_", "Build_"):
        if s.startswith(pre):
            s = s[len(pre):]
            break
    if s.endswith("_C"):
        s = s[:-2]
    return s


class Catalog:
    """Indexed recipe catalog. Build from the exported cache or explicit lists.
    Only NON-building recipes (parts, not structures) participate in production
    expansion; building recipes are indexed only for name lookups."""

    def __init__(self, recipes: List[dict], items: Optional[List[dict]] = None,
                 buildables: Optional[List[dict]] = None):
        # item form (Solid/Liquid/Gas) is needed BEFORE parsing so fluid amounts
        # can be normalized to m3. Built from the items list.
        self.item_form: Dict[str, str] = {}
        for it in (items or []):
            ic = it.get("itemClass") or it.get("class")
            fm = it.get("form")
            if ic and fm:
                self.item_form[ic] = fm
        self.recipes: List[RecipeDef] = [self._parse(r, self.item_form) for r in recipes]
        # itemClass -> [RecipeDef] that produce it as their PRIMARY product
        # (products[0]), non-building only. Keying on any product would treat a
        # part as "producible" by every recipe that merely emits it as a
        # BYPRODUCT (e.g. Dark Matter Residue is a byproduct of 6+ recipes but
        # has ONE dedicated recipe), which pollutes the recipe-choice list and
        # can create nonsense/circular options. Byproducts are still accounted
        # for during expansion (Bom.byproducts); they just don't make a part
        # look like it has many ways to be made.
        # "Unpackage X" recipes are inverse-logistics (Packaged X -> X); they are
        # never a real way to SOURCE a fluid (using one needs the packaged form,
        # which comes from packaging the fluid - a loop), so exclude them from
        # the producer index. The fluid's real production recipe always coexists,
        # so nothing is orphaned.
        self.producers: Dict[str, List[RecipeDef]] = {}
        for r in self.recipes:
            if r.is_building_recipe or not r.products:
                continue
            if r.display_name.strip().lower().startswith("unpackage"):
                continue
            primary_ic = r.products[0][0]
            self.producers.setdefault(primary_ic, []).append(r)
        # display names: recipe ingredient/product itemName first, then the
        # items list (authoritative - real catalog uses "name"), then a
        # class-short fallback so every referenced class always resolves.
        self.item_names: Dict[str, str] = {}
        for r in recipes:  # raw dicts carry itemName; the parsed RecipeDefs drop it
            for side in ("ingredients", "products"):
                for e in r.get(side, []) or []:
                    ic, nm = e.get("itemClass"), e.get("itemName")
                    if ic and nm:
                        self.item_names.setdefault(ic, nm)
        if items:
            for it in items:
                ic = it.get("itemClass") or it.get("class")
                nm = it.get("name") or it.get("displayName") or it.get("itemName")
                if ic and nm:
                    self.item_names[ic] = nm  # items list wins
        for r in self.recipes:
            for ic, _ in list(r.ingredients) + list(r.products):
                self.item_names.setdefault(ic, _short(ic))
        # Friendly building names. The real catalog's `buildables` carry no
        # name; the building's descriptor item (Desc_AssemblerMk1_C, name
        # "Assembler") does, but producedIn references the Build_ class. Map by
        # the shared token (strip Desc_/Build_ prefix + _C), with an explicit
        # buildables-list name winning if present.
        self.buildable_names: Dict[str, str] = {}
        self._building_names_by_token: Dict[str, str] = {}
        for it in (items or []):
            if it.get("isBuildingDescriptor"):
                nm = it.get("name") or it.get("displayName")
                tok = _building_token(it.get("itemClass") or it.get("class") or "")
                if nm and tok:
                    self._building_names_by_token[tok] = nm
        # Per-building power draw (MW) at 100% clock, from world.buildableCatalog.
        self.buildable_power: Dict[str, float] = {}
        for b in (buildables or []):
            bc = b.get("buildableClass") or b.get("class")
            nm = b.get("name") or b.get("displayName")
            if bc and nm:
                self.buildable_names[bc] = nm
            if bc:
                p = b.get("producingPowerConsumptionBase")
                if p is None:
                    p = b.get("defaultProducingPowerConsumption")
                if p is not None:
                    self.buildable_power[bc] = float(p)

    @staticmethod
    def _parse(r: dict, forms: Dict[str, str]) -> RecipeDef:
        def norm(entries):
            out = []
            for e in entries:
                ic = e["itemClass"]
                amt = float(e["amount"])
                if forms.get(ic) in FLUID_FORMS:
                    amt /= FLUID_SCALE  # mL -> m3
                out.append((ic, amt))
            return tuple(out)
        return RecipeDef(
            recipe_class=r["recipeClass"],
            display_name=r.get("displayName", _short(r["recipeClass"])),
            duration_seconds=float(r.get("manufacturingDuration", 0) or 0),
            ingredients=norm(r.get("ingredients", [])),
            products=norm(r.get("products", [])),
            produced_in=tuple(r.get("producedIn", []) or []),
            is_building_recipe=bool(r.get("isBuildingRecipe", False)),
        )

    @classmethod
    def from_cache(cls, path: Optional[str] = None) -> "Catalog":
        if path is None:
            path = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "catalog_cache.json")
        with open(path, "r", encoding="utf-8") as f:
            d = json.load(f)
        return cls(d.get("recipes", []), d.get("items"), d.get("buildables"))

    def is_raw_resource(self, item_class: str) -> bool:
        """True for extracted raw resources (leaves by default). See
        RAW_RESOURCE_MARKER."""
        return RAW_RESOURCE_MARKER in item_class

    def name(self, item_class: str) -> str:
        return self.item_names.get(item_class, _short(item_class))

    def buildable_name(self, buildable_class: str) -> str:
        if buildable_class in self.buildable_names:
            return self.buildable_names[buildable_class]
        tok = _building_token(buildable_class)
        return self._building_names_by_token.get(tok, tok)

    def power_base_mw(self, buildable_class: str) -> float:
        """Per-machine power draw (MW) at 100% clock, 0.0 if unknown."""
        return self.buildable_power.get(buildable_class, 0.0)

    def resolve_item(self, query: str) -> str:
        """Resolve a display-name or class fragment to an itemClass. Exact class
        wins; else case-insensitive exact display-name; else unique substring.
        Raises ValueError if nothing or if ambiguous."""
        if query in self.item_names:
            return query
        q = query.strip().lower()
        exact = [ic for ic, nm in self.item_names.items() if nm.lower() == q]
        if len(exact) == 1:
            return exact[0]
        if len(exact) > 1:
            raise ValueError(f"item name '{query}' is ambiguous across {len(exact)} classes")
        subs = [ic for ic, nm in self.item_names.items() if q in nm.lower() or q in ic.lower()]
        if len(subs) == 1:
            return subs[0]
        if not subs:
            raise ValueError(f"no item matches '{query}'")
        sample = ", ".join(sorted(self.name(ic) for ic in subs)[:8])
        raise ValueError(f"'{query}' is ambiguous ({len(subs)} matches: {sample}...); use an exact name or class")

    def choose_recipe(self, item_class: str, recipe_choices: Dict[str, str]) -> Optional[RecipeDef]:
        """The recipe to produce item_class. Explicit choice wins. Else the sole
        non-alternate recipe. Returns None if nothing produces it (a raw leaf).
        Raises RecipeChoiceNeeded if the choice is genuinely ambiguous."""
        producers = self.producers.get(item_class, [])
        if not producers:
            return None
        chosen_class = recipe_choices.get(item_class)
        if chosen_class:
            for r in producers:
                if r.recipe_class == chosen_class or _short(r.recipe_class) == _short(chosen_class):
                    return r
            raise ValueError(f"chosen recipe '{chosen_class}' does not produce {self.name(item_class)}")
        standard = [r for r in producers if not r.is_alternate]
        if len(standard) == 1:
            return standard[0]
        if len(standard) == 0:
            # only alternates exist - caller must pick
            raise RecipeChoiceNeeded(item_class, self.name(item_class), producers)
        raise RecipeChoiceNeeded(item_class, self.name(item_class), standard)


# ---------------------------------------------------------------------------
# BOM result
# ---------------------------------------------------------------------------
@dataclass(frozen=True)
class BomNode:
    item_class: str
    item_name: str
    rate_per_min: float          # required output rate of this part
    recipe_class: str
    recipe_name: str
    machine_class: str           # the building it's produced in
    machine_name: str
    machines_exact: float        # fractional machines at 100% clock
    machines_ceil: int           # whole machines to build (underclock to hit rate)
    clock_percent_if_ceil: float # clock each of `machines_ceil` runs at to hit rate
    power_mw: float              # total draw of this node: machines_ceil at that clock
    depth: int


@dataclass
class Bom:
    target_item: str
    target_name: str
    target_rate: float
    nodes: List[BomNode] = field(default_factory=list)      # one per produced part
    raw_totals: Dict[str, float] = field(default_factory=dict)   # itemClass -> per-min
    byproducts: Dict[str, float] = field(default_factory=dict)   # itemClass -> per-min
    machine_totals: Dict[str, int] = field(default_factory=dict) # machine_class -> total ceil count
    total_power_mw: float = 0.0  # sum of node power draws (machines at their clock)

    def format(self, catalog: "Catalog") -> str:
        lines = [f"BOM for {self.target_name} @ {self.target_rate:g}/min", ""]
        lines.append("MACHINES (build these):")
        for n in self.nodes:
            lines.append(f"  {n.machines_ceil:>3d} x {n.machine_name:<20s} @ {n.clock_percent_if_ceil:6.2f}%  "
                         f"{n.power_mw:7.1f} MW  -> {n.item_name} {n.rate_per_min:g}/min  "
                         f"({n.recipe_name}; exact {n.machines_exact:.3f} machines)")
        lines.append("")
        lines.append("RAW INPUTS (per min):")
        for ic, rate in sorted(self.raw_totals.items(), key=lambda kv: -kv[1]):
            lines.append(f"  {rate:10.2f}  {catalog.name(ic)}")
        if self.byproducts:
            lines.append("")
            lines.append("BYPRODUCTS (per min, must be sunk/consumed):")
            for ic, rate in sorted(self.byproducts.items(), key=lambda kv: -kv[1]):
                lines.append(f"  {rate:10.2f}  {catalog.name(ic)}")
        lines.append("")
        lines.append("MACHINE TOTALS:")
        for mc, cnt in sorted(self.machine_totals.items(), key=lambda kv: -kv[1]):
            lines.append(f"  {cnt:>3d} x {catalog.buildable_name(mc)}")
        lines.append("")
        lines.append(f"TOTAL POWER: {self.total_power_mw:.1f} MW (at the clocks above)")
        return "\n".join(lines)


def solve_bom(catalog: Catalog, target_item: str, rate_per_min: float,
              recipe_choices: Optional[Dict[str, str]] = None,
              raw_items: Optional[List[str]] = None) -> Bom:
    """Expand the full recipe tree for `target_item` at `rate_per_min`.

    target_item may be an itemClass or a display name (resolved via the catalog).
    recipe_choices: {itemClass: recipeClass} to pick alternates / disambiguate.
    raw_items: itemClasses (or names) to treat as leaves (sourced externally) in
    addition to natural raws (parts nothing produces).

    Returns a Bom. Raises RecipeChoiceNeeded (ambiguous recipe) or RecipeCycle.
    """
    if rate_per_min <= 0:
        raise ValueError("rate_per_min must be positive")
    recipe_choices = dict(recipe_choices or {})
    target_class = catalog.resolve_item(target_item)
    raw_set = set()
    for r in (raw_items or []):
        raw_set.add(catalog.resolve_item(r))

    def is_raw(item_class: str) -> bool:
        return (item_class in raw_set
                or catalog.is_raw_resource(item_class)
                or not catalog.producers.get(item_class))

    # 1. Discover the sub-DAG (item -> ingredient edges) and topo-order it,
    #    parents (consumers) before children (ingredients), so demand is fully
    #    accumulated before a part is expanded. Cycle -> RecipeCycle.
    recipe_for: Dict[str, RecipeDef] = {}
    children: Dict[str, List[str]] = {}
    order: List[str] = []
    WHITE, GREY, BLACK = 0, 1, 2
    color: Dict[str, int] = {}

    def visit(item_class: str, path: List[str]):
        color[item_class] = GREY
        if not is_raw(item_class):
            recipe = catalog.choose_recipe(item_class, recipe_choices)
            if recipe is not None:
                recipe_for[item_class] = recipe
                kids = [ic for ic, _ in recipe.ingredients]
                children[item_class] = kids
                for k in kids:
                    st = color.get(k, WHITE)
                    if st == GREY:
                        cyc = " -> ".join(catalog.name(x) for x in path[path.index(k):] + [k]) if k in path else f"{catalog.name(item_class)} -> {catalog.name(k)}"
                        raise RecipeCycle(f"recipe cycle: {cyc}. Mark one item raw or choose a different recipe.")
                    if st == WHITE:
                        visit(k, path + [k])
        color[item_class] = BLACK
        order.append(item_class)  # post-order: children before parents

    visit(target_class, [target_class])
    order.reverse()  # now parents before children

    # 2. Propagate demand in that order.
    need: Dict[str, float] = {target_class: float(rate_per_min)}
    bom = Bom(target_item=target_class, target_name=catalog.name(target_class), target_rate=float(rate_per_min))
    depth: Dict[str, int] = {target_class: 0}

    for item_class in order:
        req = need.get(item_class, 0.0)
        d = depth.get(item_class, 0)
        if is_raw(item_class):
            if req > 0:
                bom.raw_totals[item_class] = bom.raw_totals.get(item_class, 0.0) + req
            continue
        recipe = recipe_for[item_class]
        out_amt = recipe.product_amount(item_class)
        out_per_min = recipe.per_min(out_amt)
        machines_exact = req / out_per_min if out_per_min > 0 else 0.0
        machines_ceil = max(1, math.ceil(machines_exact - 1e-9)) if machines_exact > 0 else 0
        clock = (machines_exact / machines_ceil * 100.0) if machines_ceil else 0.0
        machine_class = recipe.produced_in[0] if recipe.produced_in else ""
        # power: N machines each at (clock)^exponent of base draw
        base_mw = catalog.power_base_mw(machine_class)
        node_power = machines_ceil * base_mw * ((clock / 100.0) ** POWER_CLOCK_EXPONENT) if machines_ceil and base_mw else 0.0
        bom.nodes.append(BomNode(
            item_class=item_class, item_name=catalog.name(item_class), rate_per_min=req,
            recipe_class=recipe.recipe_class, recipe_name=recipe.display_name,
            machine_class=machine_class, machine_name=catalog.buildable_name(machine_class),
            machines_exact=machines_exact, machines_ceil=machines_ceil,
            clock_percent_if_ceil=clock, power_mw=node_power, depth=d))
        bom.total_power_mw += node_power
        if machine_class:
            bom.machine_totals[machine_class] = bom.machine_totals.get(machine_class, 0) + machines_ceil
        # ingredient demand (fractional machines drive it)
        for ic, amt in recipe.ingredients:
            need[ic] = need.get(ic, 0.0) + machines_exact * recipe.per_min(amt)
            depth[ic] = max(depth.get(ic, 0), d + 1)
        # other products of this recipe are byproducts
        for pc, amt in recipe.products:
            if pc == item_class:
                continue
            bom.byproducts[pc] = bom.byproducts.get(pc, 0.0) + machines_exact * recipe.per_min(amt)

    # order nodes shallow-first for readability
    bom.nodes.sort(key=lambda n: (n.depth, -n.rate_per_min))
    return bom


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------
def _main(argv: List[str]) -> int:
    import argparse
    ap = argparse.ArgumentParser(description="Recipe dependency / BOM solver (offline, deterministic).")
    ap.add_argument("item", help="target part display name or itemClass")
    ap.add_argument("rate", type=float, help="desired output rate per minute")
    ap.add_argument("--catalog", default=None, help="path to catalog_cache.json (default: controller/catalog_cache.json)")
    ap.add_argument("--raw", default="", help="comma-separated parts to treat as raw leaves (sourced externally)")
    ap.add_argument("--choose", action="append", default=[], help="itemClass=recipeClass override; repeatable")
    args = ap.parse_args(argv)
    try:
        catalog = Catalog.from_cache(args.catalog)
    except FileNotFoundError:
        print("No catalog_cache.json found - run: python controller/export_catalog.py (game must be running).")
        return 2
    choices = {}
    for c in args.choose:
        if "=" in c:
            k, v = c.split("=", 1)
            choices[catalog.resolve_item(k) if not k.startswith("/") else k] = v
    raw = [r for r in args.raw.split(",") if r.strip()]
    try:
        bom = solve_bom(catalog, args.item, args.rate, recipe_choices=choices, raw_items=raw)
    except RecipeChoiceNeeded as e:
        print("RECIPE CHOICE NEEDED:\n  " + str(e))
        return 3
    except (RecipeCycle, ValueError) as e:
        print("ERROR:", e)
        return 1
    print(bom.format(catalog))
    return 0


if __name__ == "__main__":
    import sys
    raise SystemExit(_main(sys.argv[1:]))
