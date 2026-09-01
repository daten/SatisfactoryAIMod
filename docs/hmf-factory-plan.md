# Heavy Modular Frame end-to-end factory — plan + build log (2026-09-01)

User request: a much-more-complex follow-on build — raw nodes through
Heavy Modular Frames at the default Manufacturer rate (2/min), with
deliberate pre-planning and an intentional layout; failures welcome as
bug-hunting. This doc is the plan of record; live results get appended.

## Dependency tree (real data from world.recipeCatalog, all-default recipes)

HMF (Manufacturer, 30s): 5 MF + 20 Steel Pipe + 5 EIB + 120 Screws -> 1.
At 2 HMF/min the chain needs (per min): MF 10, Pipe 40, EIB 10, Screw
420 (240 direct + 180 into RIP), Rod 165 (60 MF + 105 screws), Plate 90,
Beam 30, Concrete 60, Steel Ingot 180, Iron Ingot 300, and raw:
**Iron Ore 480, Coal 180, Limestone 180**.

## Machine plan (mixed clocks, chosen by MATERIAL scarcity, not elegance)

This save's building costs are nonstandard and drove the design:
- REAL TRAP FOUND: `Recipe_SmelterMk1` is the FOUNDRY (displayName
  "Foundry", product Desc_FoundryMk1, 10 Modular Frame + 10 Rotor + 20
  Concrete). The actual Smelter is `Recipe_SmelterBasicMk1` (5 Rod +
  8 Wire). Never trust the class name; check displayName/products.
- Modular Frames are the scarce input (18 on hand): Foundries,
  Manufacturer, Mk2 Miners cost 10-20 MF each. Crafting MF from
  RIP+Rod stock (world.simulatedCraft) covers the ~52 shortfall.
- Iron Plate is the other shortage (~514 needed for slabs/attachments/
  belts vs 120 on hand) -> a BOOTSTRAP mini-line at the iron node
  (miner -> smelter -> plate constructor @250% -> depot uploader) feeds
  plates into central storage for withdrawal while the main build
  proceeds.

| Stage | Machine | Count | Clock |
|---|---|---|---|
| Iron ore | Miner Mk3 ×2 (nodes 543+544, Normal) | 2 | 100% |
| Coal | Miner Mk2 (node 590, Pure) | 1 | 100% (240≥180) |
| Limestone | Miner Mk2 (node 584, Normal) | 1 | 150% (1 shard) |
| Iron Ingot | Smelter ×10 (cheap: 5 Rod+8 Wire) | 10 | 100% |
| Steel Ingot | Foundry ×2 (MF-expensive) | 2 | 250% |
| Rods/Screws/Plates/Concrete/Pipes/Beams | Constructor 5+5+2+2+1+1 | 16 | 250% |
| MF / RIP / EIB | Assembler 2+2+1 | 5 | 250% |
| HMF | Manufacturer | 1 | 100% (per spec) |

Power estimate ~900 MW at full clocks (overclock ^1.6). Ramp: wire and
verify everything at 100% first, then shard up in stages watching for a
grid trip (risk accepted, save taken).

## Site & layout

Plant: floating platform, **1m foundations** (the new standing
preference), L1 top z=+200 (clears all terrain in the rectangle,
surveyed via world.terrainHeightGrid after teleporting the player in —
NOTE: terrain does NOT trace until the player is nearby; distant
regions aren't streamed, a real constraint discovered this session).
Rect: x 1600..9600, y 278400..283200. Levels at z_top = 200 / 1700 /
3200 / 5200:
- L1: 10 Smelters (row group A) + 2 Foundries (row group B, dual
  ore+coal manifolds).
- L2: constructors — R1: 5 rods + 2 plates (iron-ingot manifold);
  R2: 5 screws + 1 pipe + 1 beam + 2 concrete (rod / steel / limestone
  manifolds).
- L3: 5 assemblers (1600 pitch): 2 MF + 2 RIP + 1 EIB.
- L4: Manufacturer (4 direct input belts from lifts) + Storage
  Container sink.
Row template per group (y, 800 pitch): splitter chain / machines /
merger chain, machines at x=2000+i*800. Lifts (Mk4) carry every
inter-level flow; trunks are Mk4 belts, taps Mk1 (Manufacturer screw
input Mk4 at 240/min).

Outposts: iron pair at (572,283802)+(-1126,285301) merged to one ore
trunk; coal from (17164,280788,+2768) via ~4 relay pads; limestone from
(-8701,283440) via ~2 relay pads. Relays = 1m slab + splitter every
≤4500 units (belt maxSplineLength 5600).

Power: nearest powered grid point is Build_PowerPoleMk1_C_2147045614 at
(-20373,269656), ~26k away — bridge with a 3-pole chain (wire cap
10000/segment).

## Execution stages (save between each)

1. Power chain to site. 2. Iron miner #1 + bootstrap plate line +
depot. 3. Remaining miners + trunks/relays. 4. L1 + manifolds.
5. L2. 6. L3. 7. L4 + sink. 8. Recipes, clock ramp, end-to-end verify
(storage receiving HMFs).

## LIVE BUILD LOG — Stage 1 (2026-09-01): partial, HIGH bug-hunting yield

Built and verified working: 3-pole power chain from the grid
(26k away) to the site; Iron Miner Mk3 on node 543 (powered);
3-Smelter iron-ingot manifold on a 1m-foundation L1 platform (splitter
+ merger chains, all taps/drops wired, powered, recipes set, inputs
3/3 connected); a miner→merger→Mk4-lift riser that climbs the 1273-unit
platform rise. Ore is ONE belt from flowing; that last belt is blocked
by the entry-splitter orientation issue (#7 below). Saves:
`pre-hmf-factory`, `hmf-stage1-smelters`, `hmf-stage1-findings`.

**Real findings (the point of this run):**

1. **Recipe class-name TRAP (high impact).** `Recipe_SmelterMk1` is
   the **Foundry** (displayName "Foundry", product `Desc_FoundryMk1`,
   costs 10 Modular Frame + 10 Rotor + 20 Concrete). The real Smelter
   is `Recipe_SmelterBasicMk1` (5 Rod + 8 Wire). Class names are
   unreliable for machines — always resolve by `displayName`/`products`
   from `world.recipeCatalog`. (Adds to the existing exact-item-match
   lesson.)

2. **`world.terrainHeightGrid`/`groundHeight` return `found:false`
   until the player is near.** Distant map regions aren't streamed, so
   a survey of a remote site silently returns all-not-found. Must
   `teleportPlayer` to the site FIRST, then survey. Confirmed: same
   grid call went 0/24 found (remote) → 54/54 found (after teleport).

3. **Belt player-distance failure ALSO presents as "Conveyor Belt is
   too steep!" and "too long", not just "too long".** The
   `PopulateSyntheticTraceRay` fix removed the pure-distance "too long"
   for the copper build, but INCLINE routing still depends on player
   proximity: identical belts failed "too steep"/"too long" with the
   player far and succeeded after nothing but a `teleportPlayer` near.
   The trace-ray fix is necessary but NOT sufficient — the spline
   incline/route computation still consults live player/camera state.
   **Refinement candidate**: extend the synthetic-hit approach (or a
   deterministic camera override) to the belt's `AutoRouteSpline`
   incline path. Practical rule for now: teleport within ~1-2k of every
   belt connection.

4. **Deleting an attachment does NOT delete belts attached to it** —
   the belts dangle and keep the OTHER endpoint's connector occupied
   (a later connect there fails `NO_FACTORY_CONNECTION` until the
   dangler is found and deleted). Same class as the documented
   pipe-junction behavior; worth having `world.deleteBuilding`
   optionally cascade attached belts, or a `world.cleanupDanglingBelts`
   helper.

5. **A conveyor LIFT cascade-deletes when its dest attachment is
   deleted** (good) — but NOT the belts feeding its source (see #4).

6. **`connectConveyorLift` can't cleanly bridge two attachments and
   has no connector pinning.** It picks "first free output" on source
   / "first free input" on dest with no `sourceConnectorPosition`
   equivalent (belts have this; lifts don't). With two same-x/y
   stacked splitters it repeatedly chose non-coaxial side connectors,
   building a lift whose top landed nowhere near the dest. Even after
   forcing coaxial geometry (merger east-output under splitter
   west-input on the same column), the lift rose to the right column
   but its top **overshot the dest connector by ~29 units (z 331 vs
   302) and did not snap** — leaving a free lift output. **Two
   refinement candidates**: (a) add `sourceConnectorPosition`/
   `destConnectorPosition` to `connectConveyorLift` like belts have;
   (b) investigate the ~29-unit Mk4-lift top overshoot vs the dest
   connector (likely a lift-mesh joint height the #9 height calc
   doesn't account for).

7. **Orientation planning bites hard in multi-level builds.** Every
   platform splitter placed at yaw 0 has its INPUT facing west, but ore
   arrives from the NORTH (miner/riser north of the platform), so every
   feed belt needs an impossible U-wrap into a west-facing input.
   The "plan orientation before placing" lesson (already documented for
   flat builds) is critical and easy to miss when a feed comes from a
   different level/side. **Fix for a retry**: orient each row's ENTRY
   splitter so its input faces the incoming trunk direction, decided
   up front.

8. **Placement Z isn't uniform across a fixed-`z` platform.** Machines
   placed via `ignoreGroundTrace` at the same reference z landed with
   connectors at different heights (smelter S0 at z=302 vs S1/S2 at
   z=451 — a 149 delta) because the buildable's own origin/mesh offset
   varies with... something not yet pinned (possibly a residual ground
   interaction even with the flag). Always read each machine's real
   connector z via `world.connections` before wiring — assuming a
   uniform platform z caused several `NO_FACTORY_CONNECTION` failures.

9. **Confirmed working live this run**: `ignoreInvalidFloor` lets
   attachments float in open air (no slab needed); the
   `UFGCDEncroachingPlayer` ignore fix works for belts ("A player is in
   the way! (hard, ignored)" logged); `world.teleportPlayer`,
   `world.withdrawFromCentralStorage`, `world.setRecipe`,
   `world.connectPower` on a 26k grid bridge all solid.

**Assessment**: the full 63-machine build is achievable but each
multi-level feed currently costs many iterations because of #3, #6, and
#7. Highest-leverage refinements before a serious retry: (a) belt
incline routing player-independence (#3), and (b) `connectConveyorLift`
connector pinning + overshoot fix (#6). With those, the same plan
should build far more smoothly.
