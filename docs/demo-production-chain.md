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
or Miner (`Recipe_MinerMk1` — confirmed correct in earlier sessions,
matches `Build_MinerMk1`) recipes; both have been used successfully
multiple times with the expected building resulting. Worth spot-checking
`buildableId`/`world.buildables` after any *new* recipe class is used
for the first time, given this precedent.

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

## Progress so far

- Two Smelters/Foundries and one Constructor placed near the original
  base location during recipe-verification testing - the erroneous
  Foundry and the first (correctly-recipe'd) Smelter are still standing
  there, unused for this demo chain. Not cleaned up - user's call whether
  to dismantle them.
- A Constructor and a (correct) Smelter placed on a new platform built
  near the target Iron Ore node.
- Miner placement (via the new `world.placeExtractor`/`ConstructExtractorOnNode`,
  commit `78455da5f7`) not yet attempted live - pending an Alpakit
  redeploy to get this new RPC method into the running game.

## Still to do

1. Redeploy, then place the Miner on the target Iron Ore node via
   `world.placeExtractor`.
2. Configure the Smelter's recipe (Iron Ingot) and the Constructor's
   recipe (Iron Plate) via the already-working `world.setRecipe`.
3. Power connections - `DebugCheckPowerConnection`/`DocMod.TestPowerConnection`
   exists as a dry-run only; needs live testing (untested as of this
   writing) before building a real (non-dry-run) power-connect RPC
   method, per `docs/conveyor-power-connection-research.md`'s plan.
4. Conveyor belts - no implementation started yet; higher risk/unknown
   per the same research doc, to be tackled after power is validated.
5. Verify actual production via `world.manufacturers`'
   `productionStatus`/`productionProgress` once wired and powered.
