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

## LIVE BUILD RESULT (2026-09-01): real, working copper factory achieved autonomously - Ore → Ingot → Wire, no recompile

User request: attempt this build independently, entirely via already-
deployed RPCs (no rebuild/relaunch possible for hours), get as creative
as needed to route around anything that couldn't be solved directly.
Full new site (not the 2026-08-30 partial build above, which was at a
different node/location and not reused).

**Real result**: a genuinely complete, currently-running production
line - real Copper Ore flows from a Mk3 Miner (Pure node) through 3
real Smelters (Copper Ingot) into a Constructor (Wire), verified via
`world.manufacturers` showing growing real output counts over time
(`productionStatus: "Producing"`, Copper Ingot/Wire counts increasing
across repeated queries) - not just `success:true` on construction
calls. Scoped down from the original 5-Smelter/10-Constructor plan to
3 Smelters/1 Constructor after hitting a real, only partly-understood
belt-routing limit (see below) - a real, working smaller line beats an
ambitious one left half-wired.

**Real blockers hit and worked around, in order**:

1. **No Portable Miner in player inventory** - placing a stationary
   Miner needs one as a real construction ingredient (see the
   `world.placeExtractor` CRITICAL note above), and there's no RPC to
   craft one from scratch. Fix: `world.centralStorage` had 16 stockpiled
   - `world.withdrawFromCentralStorage` pulled 2 into the player's
   carried inventory, `world.placeExtractor` then worked normally. Also
   used the same withdraw path for extra Cable (only 59 on hand, needed
   more for the Constructor row - 1000 available in storage).
2. **`world.connectConveyor`'s default `instigatorStrategy` is
   `"PlayerController"`** - the one already documented (see the
   camera-hijack section above) to get permanently stuck on
   `UFGCDInitializing`. Every belt call in this build explicitly passed
   `"instigatorStrategy": "RealCharacter"` - without it, nothing
   connects, ever, regardless of geometry.
3. **`routeMode` isn't universally interchangeable** - some real belt
   connections only succeeded with `"Straight"`, others only with
   `"Curve"`, for what looked like very similar geometry (a short, well-
   aligned hop). No pattern found for which one a given connection
   needs - just try `"Straight"` then `"Curve"` then `"Auto"` and use
   whichever succeeds, don't assume the first failure means the
   connection is impossible.
4. **A REAL, significant finding: unpinned multi-output splitter
   connections can silently grab the WRONG connector.** A Conveyor
   Splitter has 3 outputs (one THROUGH + two SIDE) - calling
   `world.connectConveyor` without `sourceConnectorPosition` lets the
   engine pick "the first free output," which in every case observed
   here picked the THROUGH connector even when a SIDE connector was the
   intended one (e.g. a splitter meant to tap one output down to a
   Smelter while keeping THROUGH free to continue a manifold chain -
   the smelter tap kept consuming THROUGH instead, silently breaking
   the chain continuation with a confusing, unrelated-looking
   `"Conveyor Belt is too long!"` on the NEXT connection attempt rather
   than any error on the tap itself). **Fix, and now the standing rule
   for any multi-output buildable (splitter, merger) in this project**:
   always pass explicit `sourceConnectorPosition`/`destConnectorPosition`
   computed from the buildable's own real placed position plus its
   known local connector offsets (±100 units along the facing axis,
   confirmed via a `world.connections` read on one placed instance
   first) - never rely on default "first free connector" selection when
   more than one output exists.
5. **Belt connections heading in the -X (west) direction failed
   consistently** (`"Conveyor Belt is too long!"`/`"Invalid Conveyor
   Belt shape!"`) even for short, cleanly-aligned, pinned-connector
   hops that were geometrically identical (just mirrored) to +X (east)
   connections that worked immediately. All three `routeMode`s failed
   the same way. Not root-caused - possibly related to the
   already-documented `AutoRouteSpline`-reads-camera-rotation quirk
   (see the camera-hijack section above), possibly something else.
   **Workaround used**: abandoned the symmetric hub-and-spoke layout
   (planned: arms both east AND west of the miner) and extended the
   manifold east-only instead. Real, unresolved - worth investigating
   directly if a future build needs to route west.
6. **A genuine, still-unexplained distance/hop limit on chained
   splitter manifolds.** Even after fixing finding #4 (correct
   connector pinning) and confirming the fix worked cleanly for 3
   consecutive hops (miner→center→+800→+1600, all verified via
   `world.connections`), the 4th hop (+1600→+2400) and beyond
   consistently failed `"Conveyor Belt is too long!"` on freshly-
   verified, unconsumed, correctly-oriented connectors - not a
   connector-picking bug this time. Not root-caused (candidates: real
   cumulative-network distance from the source, a hop-count limit, or
   something specific to that stretch of the map) - flagged as a real,
   open question for future investigation rather than guessed at
   further under time pressure. This is why the build stopped at 3
   Smelters instead of the full 5.
7. **No RPC exists to insert items into any buildable's inventory**
   (fuel a generator, stock a storage container with a starting
   supply) - confirmed by reading the full `RPC_REFERENCE.md` method
   list, not assumed. This ruled out a Biomass Burner (would need
   manually-inserted Biofuel) as the power source. **Real, creative
   fix**: `world.powerPoles` found a real, already-powered Power Tower
   20,940 units from the build site - well within the `maxPowerTowerLength`
   (~30000) confirmed working in the 2026-08-31 Power Tower connectPower
   fix testing. Placed a new Power Tower at the site,
   `world.connectPower` (with `ignoreAimLocation`/`ignoreWireSnap`)
   bridged the two directly - real, substantial validation that last
   night's Power Tower fix holds up for genuinely long-range,
   practical use, not just the original test distance.
8. **Placing a building doesn't configure what it produces** -
   `world.manufacturers` showed `recipe: ""`/`productionStatus: "Error"`
   on freshly-placed, freshly-powered Smelters/Constructor until
   `world.setRecipe` was called explicitly (Copper Ingot / Wire) - an
   obvious-in-hindsight but real separate step, not a construction
   parameter.

**Real numbers from a running build tonight** (Pure Copper Ore node,
100% clock, no Power Shards installed): 3 Smelters converging into 1
Constructor, verified via repeated `world.manufacturers` queries
showing real, growing Copper Ingot/Wire counts over elapsed real time -
confirmed genuinely running, not just placed.

**Left in place, not chased further given the findings above**: 2 more
Smelter+Splitter pairs (`+2400`, `+3200` offsets) are built and
correctly oriented but not connected to the manifold (the finding #6
hop limit). A second Constructor row, vertical lifts, and a
Dimensional Depot uploader (the original plan's remaining stages)
were not attempted this pass - the lift height-matching fix from
2026-09-01 is confirmed working elsewhere in this project by now, so
that stage should be revisited with fresh confidence once the manifold
distance question (#6) is resolved or the scope is deliberately kept
to what a single platform level can reach.

## FOLLOW-UP DIAGNOSTIC (same day, 2026-09-01): finding #6 is NOT distance or hop-count - it's a real, reproducible SESSION-STATE degradation

Per explicit user request, built an isolated diagnostic: 5 fresh
splitters in a straight line, no machine taps, at a separate nearby
site, specifically to isolate whether #5/#6 above were really about
geometry/hop-count. Result: **every single splitter-to-splitter
connection failed** with `"Conveyor Belt is too long!"` - not just
hop 4+, but hop 1, at THREE different fresh locations tried (one
accidentally buried in un-surveyed terrain - a real mistake, found and
ruled out by re-surveying with `world.terrainHeightGrid` before
retrying; two others on confirmed-clear ground). Systematically ruled
out, each with a dedicated clean test:
- **Not a connector-picking bug** - explicit `sourceConnectorPosition`/
  `destConnectorPosition` pinning (the finding #4 fix) made no
  difference; every mode (`Straight`/`Curve`/`Auto`) was tried on every
  attempt.
- **Not terrain burial** - re-surveyed and rebuilt on confirmed-clear
  ground (`world.terrainHeightGrid` showing "not found" well above the
  platform), same failure.
- **Not real distance** - failed identically at 3200 units, 800 units,
  AND a deliberately tiny ~200-unit gap between two splitters placed
  seconds apart.
- **Not stale/batched placement** - failed even for two splitters
  placed immediately before the single connection attempt, mirroring
  the exact call shape that worked in the original copper-factory
  build earlier in the SAME session.
- **Not a real material shortage** - `world.playerInventory` confirmed
  253 Iron Plate on hand (belts cost 1, splitters cost 2) when a
  `"Missing materials!"` disqualifier appeared alongside `"too long"`
  on one attempt - a red herring, not the real blocker.

**The only variable identified**: this exact class of connection
(`world.connectConveyor` between two Conveyor Splitters, via
`instigatorStrategy: "RealCharacter"`) worked reliably EARLIER in this
same live session (the original copper-factory build: miner→center→
+800→+1600, 3 clean hops, all verified via `world.connections`) and
then stopped working entirely for the rest of the session, regardless
of location, distance, or geometry. The copper factory's own machines
(verified separately) are unaffected and still running/producing -
this is specific to NEW belt CONSTRUCTION via this exact RPC path, not
a general game-state problem. Leading hypothesis, not confirmed:
`"RealCharacter"` drives the REAL player's `AFGBuildGun`/controller
state directly (see the camera-hijack section above) - after enough
real construction calls in one long session, something in that shared,
long-lived state (the build gun's own internal step/hologram tracking,
the deterministic-look controller override, or something else
entirely) may degrade in a way a fresh game session wouldn't exhibit.
Not proven - flagged as the most likely lead for `task_422f5883` rather
than guessed at further live. **Practical implication for future
sessions**: if `world.connectConveyor` starts failing `"too long"`
across every geometry/location tried after a long session of many
belt-construction calls, suspect session-length degradation rather
than the specific connection's own geometry - a fresh game
relaunch is the most likely fix, worth trying before further live
diagnosis.

## SECOND LIVE BUILD (2026-09-01, after game restart): THE FULL PLAN IS BUILT AND RUNNING

Same site as the LIVE BUILD RESULT above, extended from the 3-Smelter/
1-Constructor partial to the plan's full shape, entirely via existing
RPCs, no recompile. **Final, verified state**: Mk3 Miner (Pure copper
node, underclocked to 78.125% per the plan table) → Mk4 trunk manifold
→ **5 Smelters @250%** (3 shards each) → 5-merger chain → **conveyor
lift #1 (Mk4, 1201-unit rise in ONE call** - the hypothesis #9 lift fix
in real production use) → Level-2 platform (31 floating foundations) →
10-splitter manifold → **10 Constructors @250% making Wire** (2:1 ratio
per the plan's optimization) → 10-merger chain → **lift #2 (Mk4, 1200
rise)** → Level-3 platform → **real Dimensional Depot Uploader**. All 15
machines verified `ProducingWithCrystal` at `clockSpeedPercent=250`,
`world.installPowerShard` live-confirmed for the first time
(3 slots, +50%/shard, `newMaxPotentialPercent=250` - the plan's
assumed values are now real). End-to-end flow PROVEN by withdrawing
Wire from central storage and watching the depot re-upload it: 450
wire re-uploaded in <60s (measurement capped by the store limit below).
Saves: `pre-copper-factory-fable` (before), `copper-factory-complete`
(after).

**Real findings from this build, most of them root-causes of earlier
mysteries**:

1. **The belt-connect "session degradation" (task_422f5883) is ROOT-
   CAUSED: it's PLAYER DISTANCE, not session state.** The identical
   `world.connectConveyor` call fails `"Conveyor Belt is too long!"`
   (all routeModes) when the real player stands too far from the
   connection and succeeds after nothing but a `world.teleportPlayer`
   closer - reproduced in controlled A/B pairs at ~4900 units (fail) →
   ~1000 (success), and again at ~6100 (fail) → near (success), with
   the working threshold consistent with the belt's own
   `maxSplineLength` (5600.1, `world.conveyorBeltTiers`). Last night's
   "worked early then never again" = the player character had been left
   far from every attempted site. **Standing rule: teleport the player
   within ~2-3000 units of any belt connection before calling it.**
2. **CORRECTED 2026-09-01 (same day, on code review + a live
   re-count): the "failed placement actually constructed" claim was
   WRONG.** The failure path in `ConstructBuildingAtPosition` provably
   never reaches `InternalConstructHologram`, and a careful re-query
   found exactly ONE foundation at the spot in question - no phantom
   build ever happened. What actually happened: the retry's "An
   identical buildable is already built there!" hard disqualifier came
   from the foundation HOLOGRAM snapping onto a NEIGHBORING, occupied
   grid cell (foundations snap to each other; the two adjacent slabs
   had just been placed), not from a hidden duplicate at the requested
   spot - a third attempt at the same coordinates then succeeded and
   produced the one real slab. Lesson recorded instead: a
   plausible-sounding two-datapoint inference ("failure then
   identical-exists = phantom construction") needs a direct occupancy
   re-count before being called a bug. The duplicate-container mystery
   (task_6f0276ff) therefore remains UNEXPLAINED - this mechanism is
   no longer a supported explanation for it.
3. **`world.placeBuilding` can report the WRONG `buildableId`**: one
   successful foundation placement returned a nearby MERGER's id as
   `result.buildableId` (the real new slab showed up separately in
   `world.buildables`). Don't trust the returned id blindly for
   anything destructive - re-verify via a position query.
4. **Deletion is not instantly visible**: after `world.deleteBuilding`
   reported OK, the deleted merger still appeared in an immediate
   `world.connections` read, and a same-batch placement at that spot
   STACKED a new merger on top of the not-yet-gone one (z +300). A
   ~500ms settle delay before dependent reads/placements avoids it.
5. **Placement bypass flags are load-bearing for autonomous builds**:
   `"Invalid aim location!"` (the real camera's aim cone) and `"A
   player is in the way!"` (a generous encroachment radius) block
   remote placements unpredictably - `ignoreAimLocation` +
   `ignorePlayerEncroachment` on every `placeBuilding` call fixed all
   of it. Belt connects have no such flags - player positioning is the
   only tool there (see finding 1).
6. **Ground-trace placement pitfalls**: a machine traced onto a
   missing-floor spot sinks to the terrain below the platform
   (a merger landed 111 low through a foundation gap); a machine traced
   onto an occupied spot stacks on top of what's there. Verify real
   z via `world.buildables` after placing on elevated platforms.
7. **Attachments snap to the 100-unit grid** even when given off-grid
   coordinates (384014 → 384000): the site's original machines sit on
   an off-by-14 grid, so every splitter/merger is 14 off its smelter
   axis - belts absorb it (usually `Curve` mode).
8. **Dimensional Depot Uploader realities**: it has NO power connector
   at all (`connectPower` correctly refuses; it uploads anyway -
   unpowered building); its input connector is 390 units from actor
   center (off the 100-grid - use `gridSnapSize: 10` to fine-position
   it for a lift dock); and **central storage caps each item at 2500**
   - a full item stalls uploads and backpressures the whole line (Wire
   was already at cap in this save; the line only runs while below
   cap). For a permanently-running demo, consume the wire or add an
   AWESOME Sink downstream.
9. **`world.teleportPlayer` is now live-tested and essential** (both
   success and the real `TELEPORT_BLOCKED` failure observed). It's the
   workaround for findings 1 and 5, and turns the "RealCharacter
   strategy needs the player nearby" constraint into a non-issue.
10. **Power on a busy site**: the Power Tower's standard slots were
    full - `world.testPowerConnection` sweeps found free connectors on
    3 of 5 smelters + the miner instead; a 6-pole chain (4 wires per
    Mk1 pole, planned exactly) powers all 10 constructors. Belt-tier
    data for this save reports speeds 120/240/540/960/1560/2400 -
    trunk lines were rebuilt Mk4 accordingly (Mk1's 120 would have
    capped the plan's 375 ore/min trunk).

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
