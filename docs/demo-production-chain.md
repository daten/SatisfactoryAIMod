# Demo: RPC-driven Iron Ingot → Iron Plate production chain

Recorded 2026-08-25, in progress. Goal: place a Miner, Smelter, and
Constructor entirely via RPC (no console commands, no player aiming),
configure their recipes, connect them with conveyors and power, and
verify real Iron Ingot → Iron Plate production. Tracks live findings as
the demo is built, same spirit as `docs/manual-verification.md`.

## Real finding: `Recipe_SmelterMk1` does NOT build a Smelter

**`Recipe_SmelterMk1.uasset` (`Content/FactoryGame/Recipes/Buildings/`)
actually produces a `Build_FoundryMk1`, not a `Build_SmelterMk1`** —
confirmed live: placed via `world.placeBuilding` with that recipe class,
the RPC response's `buildableId` named a `Build_FoundryMk1_C` instance,
`world.buildables` confirmed no `SmelterMk1` was created anywhere near
the build spot, and the user independently confirmed visually that a
Foundry appeared in-game. This is a genuine asset-naming trap, not a
DocMod bug — do not assume a recipe's filename matches its actual
product building without verifying live.

**The correct recipe for an actual Smelter is `Recipe_SmelterBasicMk1.uasset`**
(also present in the same directory) — verified live: placing with this
recipe class correctly produced a `Build_SmelterMk1_C` instance, and the
user confirmed visually. Use `Recipe_SmelterBasicMk1`, not
`Recipe_SmelterMk1`, for any future Smelter placement in this project.

No equivalent double-check has been done yet for Constructor
(`Recipe_ConstructorMk1` — confirmed correct, matches `Build_ConstructorMk1`)
or Miner (`Recipe_MinerMk1` — confirmed correct, matches `Build_MinerMk1`,
including this session's node-targeted placement) recipes; both have
been used successfully multiple times with the expected building
resulting. Worth spot-checking `buildableId`/`world.buildables` after
any *new* recipe class is used for the first time, given this precedent.

## `world.player` was necessary, not optional

`ConstructBuildingAtPosition`'s ground trace searches only within the
*player's own current* ±1000 unit Z range at the given X/Y. An arbitrary
existing buildable's position is not a safe placement reference — this
world spans thousands of units of elevation, and a buildable ~4000 units
of elevation away from the player made every trace attempt miss real
terrain entirely (`groundTraceHit=false` on every attempt), falling back
to a floating synthetic point and failing `CanConstruct()` with
`Surface is too uneven!` regardless of which X/Y offset was tried. Added
`GetPlayerTelemetry`/`"world.player"` specifically to fix this - see
commit `78455da5f7`.

## Placement near an existing base can fail from real density, not bugs

A sweep of offsets up to ~2400 units from the player's existing (dense,
established) base all failed with `Encroaching another object's
clearance!`; going further out (~8500 units) changed to `Surface is too
uneven!` instead. The buildable count before/after every failed attempt
was identical (confirms `CanConstruct()` correctly gated construction
every time, nothing was silently built). This is real base density and
real terrain, not a bug - resolved by having the user build a small flat
foundation platform in an open area, and separately relocate near an
actual resource node for the Miner (existing base was ~200m from the
nearest unoccupied Iron Ore node, too far for a practical belt).

## Extractor placement has a real build-range limit

`ConstructExtractorOnNode`/`"world.placeExtractor"` failed with
`Invalid aim location!` when the player was ~2378 units (~24m) from the
target node, and succeeded once the player moved to ~958 units (~9.6m)
away. **Real build range for this operation is somewhere between 958 and
2378 units** - not yet narrowed further. This is presumably a normal
build-gun range limit (matches the real game's own reach constraint on
handheld construction), not a bug - `ConstructExtractorOnNode`'s
synthetic hit result being geometrically valid (a real point on/near the
node) doesn't help if the player themselves is standing too far away for
the build gun to reach it. **Any future RPC-driven placement should
account for this** - check `world.player`'s position against the target
location before attempting placement, and if the caller doesn't control
the player's position (e.g. a real human is walking around), the
placement may need to wait until they're close enough.

## Important gameplay constraint (from the user): direct machine-to-machine wiring may not be available by default

See `docs/conveyor-power-connection-research.md` for the full note - the
game has two power-wiring modes (pole-based by default, direct
machine-to-machine daisy-chaining only after a later-game unlock). This
affects how `DebugCheckPowerConnection` results should be interpreted
and may mean the demo's real power step needs a power pole placed
between the Smelter and Constructor.

## Progress so far

- Two Smelters/Foundries and one Constructor placed near the original
  base location during recipe-verification testing - the erroneous
  Foundry and the first (correctly-recipe'd) Smelter are still standing
  there, unused for this demo chain. Not cleaned up - user's call whether
  to dismantle them.
- A Constructor and a (correct) Smelter placed on a new platform built
  near the target Iron Ore node.
- **Miner placed successfully on the target Iron Ore node** via
  `world.placeExtractor`, once the player was within build range.
- All three buildings for the actual demo chain are now placed:
  - Miner: `Build_MinerMk1_C_2147464131` (on the node)
  - Smelter: `Build_SmelterMk1_C_2147434176` (on the platform)
  - Constructor: `Build_ConstructorMk1_C_2147436054` (on the platform)

## Recipes configured and verified (2026-08-25)

`world.setRecipe` set the Smelter to `Recipe_IngotIron`
(`/Game/FactoryGame/Recipes/Smelter/Recipe_IngotIron.uasset`) and the
Constructor to `Recipe_IronPlate`
(`/Game/FactoryGame/Recipes/Constructor/Recipe_IronPlate.uasset`), both
`success:true` - and since `SetManufacturerRecipe` validates against
`UFGRecipe::IsProducedIn` before applying, this is real confirmation
they're compatible, unlike the placement-recipe mixup above which had no
such check. Verified via `world.manufacturers`:
`recipe:"Iron Ingot"`/`"Iron Plate"` respectively.
`productionStatus:"Error"` on both is expected at this stage - no power
or input material connected yet, not a bug.

## Power connection: validated, both dry-run and real (2026-08-25)

`world.testPowerConnection` (dry run) and `world.connectPower` (real)
both succeeded on the first live test, connecting the Smelter and
Constructor - confirms `AFGWireHologram::SetConnection()` alone is
sufficient, no multi-step `TrySnapToActor`/`DoMultiStepPlacement` flow
needed (the open question from
`docs/conveyor-power-connection-research.md` is answered). Both
machines had a free power connection slot, so this save's progression
has the daisy-chain unlock active - the pole-vs-daisy-chain constraint
noted earlier did not block this.

**The wire alone doesn't provide real power**, though - `world.buildables`
confirmed the new `Build_PowerLine_C` exists between the two machines,
but the nearest *other* power infrastructure (poles/generators/existing
lines) is 7000+ units away, presumably back at the main base. Both
machines still report `productionStatus:"Error"` after connecting -
electrical continuity between two unpowered machines doesn't create
power. **User decision: skip getting real electricity flowing for now**
(options considered: a local Biomass Burner needing manual fuel DocMod
can't insert yet, or a long power-pole chain back to the main base, both
real effort) - move on to belts, revisit power later if needed.

## Still to do

1. ~~Configure recipes~~ - done, see above.
2. ~~Power connections~~ - done (mechanism validated), real electricity
   deliberately deferred, see above.
3. Conveyor belts - no implementation started yet; higher risk/unknown
   per `docs/conveyor-power-connection-research.md` (no direct
   `SetConnection`-equivalent exists for belts, unlike wires - needs the
   real multi-step `TrySnapToActor`/`DoMultiStepPlacement` flow driving
   `ESplineHologramBuildStep`). Per that doc's plan, start smaller than a
   full attempt: confirm a single `TrySnapToActor` call can fix a start
   point (`SHBS_FindStart`) before attempting the full sequence. The
   Miner-to-Smelter and Smelter-to-Constructor distances in this layout
   should be checked once belts are attempted (Miner is on the node,
   Smelter/Constructor are on the nearby platform - not verified how far
   apart exactly).
4. Verify actual production via `world.manufacturers`'
   `productionStatus`/`productionProgress` once wired, belted, and
   (eventually) powered.
