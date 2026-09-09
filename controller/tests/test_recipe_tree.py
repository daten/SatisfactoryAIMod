"""Tests for satisfactory_ai.recipe_tree (offline BOM solver).

Uses tests/fixtures/mini_catalog.json - a small synthetic chain with
round-number amounts/durations so the expected machine counts and raw
rates are hand-verifiable:

  Iron Ore --(Smelter, 1->1, 2s => 30/min)--> Iron Ingot
  Iron Ingot --(Constructor, 3->2, 6s => 20/min)--> Iron Plate
  Iron Ingot --(Constructor, 1->1, 4s => 15/min)--> Iron Rod
  Iron Rod   --(Constructor, 1->4, 6s => 40/min)--> Screw
  6 Iron Plate + 12 Screw --(Assembler, ->1, 12s => 5/min)--> Reinforced Iron Plate

Run from controller/:  python -m unittest discover -s tests -t . -v
"""

import json
import os
import unittest

from satisfactory_ai.recipe_tree import (
    Catalog,
    RecipeChoiceNeeded,
    RecipeCycle,
    solve_bom,
)

FIX = os.path.join(os.path.dirname(__file__), "fixtures", "mini_catalog.json")

ORE = "/Game/Test/Desc_OreIron.Desc_OreIron_C"
WATER = "/Game/Test/Desc_Water.Desc_Water_C"
INGOT = "/Game/Test/Desc_IronIngot.Desc_IronIngot_C"
PLATE = "/Game/Test/Desc_IronPlate.Desc_IronPlate_C"
RIP = "/Game/Test/Desc_IronPlateReinforced.Desc_IronPlateReinforced_C"
SMELTER = "/Game/Test/Build_SmelterMk1.Build_SmelterMk1_C"
CONSTRUCTOR = "/Game/Test/Build_ConstructorMk1.Build_ConstructorMk1_C"


def load():
    with open(FIX, encoding="utf-8") as f:
        d = json.load(f)
    return Catalog(d["recipes"], d["items"], d["buildables"])


class RecipeTreeTest(unittest.TestCase):
    def setUp(self):
        self.cat = load()

    def _node(self, bom, item_class):
        return next(n for n in bom.nodes if n.item_class == item_class)

    def test_building_recipes_excluded(self):
        # Smelter is only produced by a building recipe -> it must NOT be a
        # producible part (would be a leaf/raw if ever referenced).
        self.assertNotIn(SMELTER, self.cat.producers)

    def test_simple_two_tier_exact(self):
        # Iron Plate @ 20/min: 1 plate Constructor (20/min), 30 ingot/min -> 1
        # Smelter, 30 ore/min raw.
        bom = solve_bom(self.cat, "Iron Plate", 20)
        self.assertAlmostEqual(bom.raw_totals[ORE], 30.0, places=6)
        self.assertNotIn(WATER, bom.raw_totals)  # standard ingot recipe, no water
        plate = self._node(bom, PLATE)
        ingot = self._node(bom, INGOT)
        self.assertEqual(plate.machines_ceil, 1)
        self.assertAlmostEqual(plate.machines_exact, 1.0, places=6)
        self.assertEqual(plate.machine_class, CONSTRUCTOR)
        self.assertEqual(ingot.machines_ceil, 1)
        self.assertEqual(ingot.machine_class, SMELTER)
        self.assertEqual(bom.machine_totals[CONSTRUCTOR], 1)
        self.assertEqual(bom.machine_totals[SMELTER], 1)

    def test_ceil_and_clock(self):
        # Iron Plate @ 25/min: 25/20 = 1.25 machines -> build 2 @ 62.5%.
        bom = solve_bom(self.cat, "Iron Plate", 25)
        plate = self._node(bom, PLATE)
        self.assertAlmostEqual(plate.machines_exact, 1.25, places=6)
        self.assertEqual(plate.machines_ceil, 2)
        self.assertAlmostEqual(plate.clock_percent_if_ceil, 62.5, places=4)
        # ingot demand 1.25*30 = 37.5/min -> ore 37.5
        self.assertAlmostEqual(bom.raw_totals[ORE], 37.5, places=6)

    def test_shared_intermediate_aggregated(self):
        # Reinforced Iron Plate @ 5/min. Both the plate path and the screw/rod
        # path pull Iron Ingot; demand must AGGREGATE to 60 ingot/min -> 2
        # Smelters, 60 ore/min raw.
        bom = solve_bom(self.cat, "Reinforced Iron Plate", 5)
        ingot = self._node(bom, INGOT)
        self.assertAlmostEqual(ingot.rate_per_min, 60.0, places=6)   # 45 (plate) + 15 (rod)
        self.assertAlmostEqual(ingot.machines_exact, 2.0, places=6)
        self.assertEqual(ingot.machines_ceil, 2)
        self.assertAlmostEqual(bom.raw_totals[ORE], 60.0, places=6)
        self.assertEqual(bom.byproducts, {})

    def test_alternate_recipe_not_auto_selected_but_choosable(self):
        # Default uses the standard ingot recipe (ore only). Forcing the
        # Alternate: Pure Iron Ingot recipe brings Water into the raw totals.
        default = solve_bom(self.cat, "Iron Ingot", 130)
        self.assertNotIn(WATER, default.raw_totals)
        forced = solve_bom(self.cat, "Iron Ingot", 130,
                           recipe_choices={INGOT: "Recipe_Alternate_PureIronIngot_C"})
        self.assertIn(WATER, forced.raw_totals)
        # 13 ingot / 2s = 390/min per machine; 130/min -> 1/3 machine; ore 7*(1/3)*30
        self.assertGreaterEqual(forced.raw_totals[ORE], 0.0)
        self.assertGreater(forced.raw_totals[WATER], 0.0)

    def test_raw_override_truncates_tree(self):
        # Treating Iron Ingot as raw stops expansion there: no ore, ingot shows
        # up as a raw input instead.
        bom = solve_bom(self.cat, "Iron Plate", 20, raw_items=["Iron Ingot"])
        self.assertNotIn(ORE, bom.raw_totals)
        self.assertAlmostEqual(bom.raw_totals[INGOT], 30.0, places=6)
        self.assertNotIn(SMELTER, bom.machine_totals)

    def test_resolve_item_by_name_and_class(self):
        self.assertEqual(self.cat.resolve_item("Iron Plate"), PLATE)
        self.assertEqual(self.cat.resolve_item(PLATE), PLATE)

    def test_bad_rate_raises(self):
        with self.assertRaises(ValueError):
            solve_bom(self.cat, "Iron Plate", 0)

    def test_cycle_detected(self):
        # A depends on B, B depends on A -> RecipeCycle.
        cyclic = {
            "recipes": [
                {"recipeClass": "R_A", "displayName": "A", "isBuildingRecipe": False,
                 "manufacturingDuration": 1, "ingredients": [{"itemClass": "B", "itemName": "B", "amount": 1}],
                 "products": [{"itemClass": "A", "itemName": "A", "amount": 1}], "producedIn": ["M"]},
                {"recipeClass": "R_B", "displayName": "B", "isBuildingRecipe": False,
                 "manufacturingDuration": 1, "ingredients": [{"itemClass": "A", "itemName": "A", "amount": 1}],
                 "products": [{"itemClass": "B", "itemName": "B", "amount": 1}], "producedIn": ["M"]},
            ]
        }
        cat = Catalog(cyclic["recipes"])
        with self.assertRaises(RecipeCycle):
            solve_bom(cat, "A", 10)


if __name__ == "__main__":
    unittest.main()
