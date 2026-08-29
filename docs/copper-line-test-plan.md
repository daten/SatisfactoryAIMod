# Planned test: 3-tier overclocked copper line (Ore → Ingot → Wire → Depot)

Not yet executed. Written 2026-08-30 per explicit user request ("plan
for this test now if that will make executing it faster later") - the
game was not running when this was written, so the rate math below is
real and computed, but the physical placement plan is an algorithm/
checklist to run at execution time, not precomputed coordinates (see
"What can't be precomputed" below for why).

## The test

A row of 5 Smelters (Copper Ore → Copper Ingot) on a platform, fed by an
input manifold (splitters) from a Mk3 Miner on a real copper node, output
manifold (mergers) combining their ingots, sent up a vertical lift to a
row of 5 Constructors (Copper Ingot → Copper Wire) one level up, same
manifold pattern, whose combined wire output goes up a second vertical
lift to a Dimensional Depot uploader on a third level. Miner and/or any
machine row may be overclocked with Power Shards to use the miner's real
output rate as fully as the fixed 5+5 machine counts allow.

## Rate math (real, computed - `controller/satisfactory_ai/production.py`)

Real recipe data (`world.recipeCatalog`, cached this session):
- `Recipe_IngotCopper` (Smelter): 1 Copper Ore → 1 Copper Ingot / 2s = **30 ore-in, 30 ingot-out per min at 100%**.
- `Recipe_Wire` (Constructor): 1 Copper Ingot → 2 Wire / 4s = **15 ingot-in, 30 wire-out per min at 100%**.

**Key finding, not obvious from the recipes looking "1:1"**: a Smelter
produces ingots twice as fast as a single Constructor consumes them (30
vs 15/min at equal clock). With an EQUAL machine count on both rows (5
and 5), the constructors must run at roughly **double** the smelters'
clock percent to keep up - `plan_two_stage_line()` solves this exactly,
not by assumption.

**Assumed, not yet confirmed live** (flagged in `production.py` itself,
and in `world.installPowerShard`'s doc comment): 3 Power Shard slots per
machine, +50% max potential per shard → 250% max clock. Confirm both
before trusting the numbers below for a real build - install one shard
live and read `world.setClockSpeed`'s valid-range error text, or the new
`result.detail.newMaxPotentialPercent` `world.installPowerShard` itself
reports.

With that assumption, **the 5-Constructor row is always the binding
constraint**, not the miner or the smelters - the line can never
usefully process more than **187.5 Copper Ore/min**, regardless of node
purity, because 5 Constructors at their 250% ceiling can only consume
187.5 ingots/min. Real computed results per purity (standard Satisfactory
Mk3 Miner base rates - 120/240/480 per min at Impure/Normal/Pure - also
not yet confirmed against this project's own telemetry):

| Node purity | Miner clock % | Smelters (×5) clock % | Constructors (×5) clock % | Final wire output |
|---|---|---|---|---|
| Impure | 156.25% | 125% | 250% | 375/min |
| Normal | 78.12% | 125% | 250% | 375/min |
| Pure | 39.06% | 125% | 250% | 375/min |

**Implication for node choice**: only an **Impure** node lets the miner
run AT OR ABOVE its own 100% baseline while still keeping the line fully
fed - on Normal or Pure nodes, the miner must be deliberately
*underclocked* below 100% (not overclocked at all) to avoid producing
ore faster than 5 Constructors could ever consume. If "take full
advantage of the miner's output rate" is meant literally (maximize the
miner itself), prefer an Impure node. If it means "the line consumes
100% of whatever the miner makes" (no waste), any purity works and this
table already gives the exact clock speeds - the miner is just tuned
down on richer nodes rather than sped up. **Ask/decide before executing**
if this distinction matters to the test's goal - the code doesn't choose
this, per the project's toolkit-not-solver discipline.

To recompute for a different real node purity or a confirmed real
shard-boost value once shards are live-tested:

```python
from satisfactory_ai.production import plan_two_stage_line, max_clock_percent_for_shards, RecipeRate

smelter = RecipeRate.from_recipe_catalog_entry(1, 1, 2)
constructor = RecipeRate.from_recipe_catalog_entry(1, 2, 4)
max_clock = max_clock_percent_for_shards(3)  # update percent_per_shard= if confirmed different

plan = plan_two_stage_line(
    extractor_base_rate_per_min_at_100=240.0,  # real node's base rate at the purity you find
    extractor_max_clock_percent=max_clock,
    stage_a_machine_count=5, stage_a_base_output_per_min_at_100=smelter.product_per_min_at_100, stage_a_max_clock_percent=max_clock,
    stage_b_machine_count=5, stage_b_base_input_per_min_at_100=constructor.ingredient_per_min_at_100, stage_b_max_clock_percent=max_clock,
)
```

## What can't be precomputed, and why

Unlike the circular-platform ring/fill (pure geometry, no live
dependency), this build's exact coordinates genuinely depend on facts
only known live:

- **Which real copper node** (`world.resourceNodes`, filtered to Copper
  Ore, checked for purity and real surrounding terrain/space) - drives
  the whole layout's world-space origin and the rate table lookup above.
- **Real connector positions/orientations** on a Smelter/Constructor/
  splitter/merger/lift once actually placed - this project's own history
  (see `docs/placement-lessons.md`) repeatedly found these need live
  `world.connections` probing (the `learn_connector_profile` pattern in
  `satisfactory_ai/layout.py`), not assumed from clearance boxes alone.
- **Real terrain height** under the chosen node (`world.terrainHeightGrid`,
  the batched survey tool built this same session - use it BEFORE
  picking platform Z, per the circular-platform terrain-intrusion lesson).

## Genuinely new territory for this project - budget extra iteration for these

- **A 5-wide manifold has never been built by this mod** - only single
  splitter/merger connections are proven (see `docs/placement-lessons.md`'s
  "manifold" pattern note, PLANNED but not yet executed before now).
  Build ONE splitter→machine tap first, confirm it connects, before
  chaining all 5 - the established "smallest useful change, verify,
  then scale" discipline.
- **Two vertical lifts in series across 3 stacked levels** is also new -
  prior lift work was single-level, single-lift.
- **Feeding a Dimensional Depot uploader from a live belt** has not been
  attempted - `world.withdrawFromCentralStorage`/`LogCentralStorageAsJson`
  (read/withdraw) exist and are proven, but never a belt INTO it. It's an
  ordinary `AFGBuildable` with a factory input connector per every other
  buildable this project connects to, so the existing generic
  `world.connectConveyor` path should work with no new code - confirm
  live, don't assume.
- **`world.installPowerShard` and the widened `world.setClockSpeed`
  (now also works on extractors, not just manufacturers) are both
  brand new this session, compiled but never live-tested.**

## Suggested execution order

1. `world.terrainHeightGrid` survey near a real Copper Ore node candidate
   (`world.resourceNodes` filtered by resource + purity) to confirm a
   clear, flat-enough build site; pick final purity, look up/replan the
   rate table above for that real value.
2. Place Level 1 foundations, then the Mk3 Miner on the node
   (`world.placeExtractor`), then ONE Smelter + ONE splitter + ONE belt
   tap - confirm connection before scaling to 5.
3. Scale to the full 5-Smelter input manifold, then the 5-Smelter output
   merger manifold - verify via `world.pipeConnections`/`world.connections`-
   equivalent (`world.connections`) after each stage, not just success:true.
4. Level 2 foundations directly above, vertical lift Level 1 → Level 2,
   then repeat the single-then-scale-to-5 Constructor manifold pattern.
5. Level 3 foundations, second vertical lift Level 2 → Level 3, Depot
   uploader, connect the Constructor output manifold to it.
6. Power the whole line (a single Power Line run from an existing grid
   connection, or a Power Pole chain - check `world.powerLineLimits` for
   max single-run distance).
7. `world.installPowerShard` on the Miner and both machine rows per the
   rate table, then `world.setClockSpeed` to the computed percentages.
8. Verify: `world.manufacturers` on every machine (real `clockSpeedPercent`/
   `productivity`, not just the write call's success), and watch
   `world.resourceNodes`/inventory levels over real time to confirm the
   Depot is actually receiving Wire at the expected ~375/min (scaled to
   whichever purity was actually used).
