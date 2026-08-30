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
row of **10** Constructors (Copper Ingot → Copper Wire, count adjusted
from the original 1:1 ask - see "Optimized machine ratio" below) one
level up, same manifold pattern, whose combined wire output goes up a
second vertical lift to a Dimensional Depot uploader on a third level.
Miner and both machine rows may be overclocked with Power Shards to use
the miner's real output rate as fully as the machine counts allow.

## Rate math (real, computed - `controller/satisfactory_ai/production.py`)

Real recipe data (`world.recipeCatalog`, cached this session):
- `Recipe_IngotCopper` (Smelter): 1 Copper Ore → 1 Copper Ingot / 2s = **30 ore-in, 30 ingot-out per min at 100%**.
- `Recipe_Wire` (Constructor): 1 Copper Ingot → 2 Wire / 4s = **15 ingot-in, 30 wire-out per min at 100%**.

**Key finding, not obvious from the recipes looking "1:1"**: a Smelter
produces ingots twice as fast as a single Constructor consumes them (30
vs 15/min at equal clock). With an EQUAL machine count on both rows (the
original 5:5 ask), the constructors are forced to 250% (maxed) while the
smelters sit at only 125% (half their own headroom unused) just to keep
up - `plan_two_stage_line()` found this exactly, not by assumption.

**Assumed, not yet confirmed live** (flagged in `production.py` itself,
and in `world.installPowerShard`'s doc comment): 3 Power Shard slots per
machine, +50% max potential per shard → 250% max clock. Confirm both
before trusting the numbers below for a real build - install one shard
live and read `world.setClockSpeed`'s valid-range error text, or the new
`result.detail.newMaxPotentialPercent` `world.installPowerShard` itself
reports.

### Optimized machine ratio (per explicit user request)

The real fix for the 125%/250% imbalance isn't a clock trick, it's the
machine count ratio: since one Smelter's max output (30/min) feeds
exactly two Constructors' max input (15/min each), **Constructors should
outnumber Smelters 2:1** for both rows to hit their OWN clock ceiling at
the same time - no machine sitting on idle capacity relative to its
neighbor. Recommended: **5 Smelters : 10 Constructors** (keeps the
originally-specified Smelter row, right-sizes Constructors to match).
Real computed results per purity (standard Satisfactory Mk3 Miner base
rates - 120/240/480 per min at Impure/Normal/Pure - also not yet
confirmed against this project's own telemetry):

| Node purity | Miner clock % | Smelters (×5) clock % | Constructors (×10) clock % | Final wire output | Limited by |
|---|---|---|---|---|---|
| Impure | 250% | 200% | 200% | 600/min | the miner itself |
| Normal | 156.25% | 250% | 250% | 750/min | the smelters |
| Pure | 78.12% | 250% | 250% | 750/min | the smelters |

Roughly double the original 5:5 plan's throughput (375→750/min wire) on
Normal/Pure nodes, since neither row now caps out before the other.

**Nuance on Impure nodes specifically**: even 5:10 has slack there - the
miner's own 250%-clocked ceiling (300 ore/min) is reached before either
machine row would be maxed, so a leaner **4 Smelters : 8 Constructors**
(same 2:1 ratio, smaller footprint) hits that exact ceiling with zero
wasted machine capacity anywhere. Use 5:10 for headroom (useful if a
richer node turns out to be more convenient); use 4:8 if the target node
is confirmed Impure and a smaller build is preferred. Recompute either
with `plan_two_stage_line(stage_a_machine_count=..., stage_b_machine_count=...)`
once the real node is picked - this is a count CHOICE, not something the
toolkit should silently pick for you.

**Implication for node choice**: only an **Impure** node lets the miner
run AT its own 250%-shard ceiling while still keeping the line fully fed
(the miner itself is the binding constraint there). On Normal or Pure
nodes, the miner must be deliberately *underclocked* well below 100% -
at 5:10, the 5-Smelter row becomes the binding constraint on those
richer nodes (375 ore/min max at 250%), so the miner is throttled to
match the smelters, not the other way around. If "take full advantage of
the miner's output rate" is meant literally (maximize the miner itself),
prefer an Impure node. If it means "the line consumes 100% of whatever
the miner makes" (no waste), any purity works and the table above already
gives the exact clock speeds. **Ask/decide before executing** if this
distinction matters to the test's goal - the code doesn't choose this,
per the project's toolkit-not-solver discipline.

To recompute for a different real node purity, a different machine-count
ratio, or a confirmed real shard-boost value once shards are
live-tested:

```python
from satisfactory_ai.production import plan_two_stage_line, max_clock_percent_for_shards, RecipeRate

smelter = RecipeRate.from_recipe_catalog_entry(1, 1, 2)
constructor = RecipeRate.from_recipe_catalog_entry(1, 2, 4)
max_clock = max_clock_percent_for_shards(3)  # update percent_per_shard= if confirmed different

plan = plan_two_stage_line(
    extractor_base_rate_per_min_at_100=240.0,  # real node's base rate at the purity you find
    extractor_max_clock_percent=max_clock,
    stage_a_machine_count=5, stage_a_base_output_per_min_at_100=smelter.product_per_min_at_100, stage_a_max_clock_percent=max_clock,
    stage_b_machine_count=10, stage_b_base_input_per_min_at_100=constructor.ingredient_per_min_at_100, stage_b_max_clock_percent=max_clock,
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

- **A wide manifold has never been built by this mod** - only single
  splitter/merger connections are proven (see `docs/placement-lessons.md`'s
  "manifold" pattern note, PLANNED but not yet executed before now). The
  Constructor row (10-wide, after the ratio optimization below) is a
  bigger version of this same unproven pattern than the 5-wide Smelter
  row. Build ONE splitter→machine tap first, confirm it connects, before
  chaining the rest - the established "smallest useful change, verify,
  then scale" discipline, and worth doing the scale-up in stages (e.g.
  5, then 10) for the Constructor row specifically.
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

## Live build in progress (2026-08-30) — Smelter stage complete

Picked an unoccupied **Impure** copper node (~5800 units from the
player, genuinely flat/empty site per `world.terrainHeightGrid`) and
built the full input+output manifold for the 5-Smelter row. Both
manifolds are verified live, every real connection confirmed via
`world.connections` (not just `success:true`).

**Real, reusable lesson for the rest of this build (Constructor stage,
both lifts)**: a naive "linear chain fed from one end" splitter/merger
layout breaks when the miner/source isn't aligned with the chain's
end - the belt has to physically cross through other splitters/mergers
and their belts sitting in between, producing misleading "Conveyor Belt
is too long!"/clearance errors that look like a distance problem but
are really a topology problem. **Fix: always feed a fan-out manifold
from a HUB position aligned with the source**, with the chain
branching in both directions from there (west arm + east arm + direct
middle tap for splitters; symmetric merge-arms + direct middle tap for
mergers) - not a single linear chain from one edge. Apply this same hub
layout to the Constructor manifold and its merger before hitting the
same issue there.

**Second lesson**: when a specific input/output DIRECTION choice on a
splitter/merger produces a persistent `"Invalid Conveyor Belt shape!"`
error (not `"too long"`), the fix is usually to pick a *different*,
more geometrically natural direction (matching which side the other
buildable is actually on), not to keep retrying the same direction -
retrying resolved the transient `"too long"`/clearance flakiness cases,
but never resolved a wrong-direction case; only picking the correct
cardinal did.

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
   then repeat the single-then-scale-up Constructor manifold pattern
   (10-wide, per the ratio optimization above - stage the scale-up, e.g.
   verify at 5 before going to 10).
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
