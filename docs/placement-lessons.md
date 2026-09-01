# Placement lessons

A living reference of practical rules for placing and connecting buildables
via AIMod's RPC interface. Unlike the `*-research.md` docs (which are
dated investigation logs), this document is meant to be **read before doing
placement work** and **appended to whenever a new mistake or fix earns its
keep**. Keep entries short and actionable — link to a research doc for the
full investigation if one exists.

## CRITICAL: `world.constructWaterPumpAtPosition` on a genuinely dry-land position CRASHED THE GAME (fixed 2026-08-31, found live)

First live test of the water-pump RPCs (added the day before, never
run against a real game). Test sequence and results:

1. `world.waterVolumes` near the player (standing at the edge of a
   real ocean extending east) correctly identified `FGWaterVolume32`
   (the map's main ocean), bounds sane, player position ~330 units
   from its bounds.
2. `world.constructWaterPumpNearReference`, offset **2000** units east
   of an existing reference pump - correctly failed
   `CANNOT_CONSTRUCT` / "Encroaching another object's clearance!
   (hard)". **First real data point on minimum pump spacing** (an
   explicitly unconfirmed value the Python planner had left as a
   required caller input): fails at 2000, succeeds at 3000+ (tested
   3000/4000/5000, all succeeded and verified via `world.buildables`
   at the exact expected offset positions). Real minimum spacing is
   somewhere in (2000, 3000] - not yet narrowed further.
3. `world.constructWaterPumpAtPosition`, literal X/Y/Z far out in the
   same ocean - succeeded, verified via `world.buildables`.
4. `world.constructWaterPumpAtPosition` again, this time with a
   **literal position on dry land** (the reference foundations near
   the player, well above the water surface) as a deliberate
   negative-path test (expecting a clean `CANNOT_CONSTRUCT` or
   similar) - **crashed the entire game process**. Confirmed via the
   user's own pasted crash callstack: `Assertion failed:
   mSnappedExtractableResource [FGResourceExtractorHologram.cpp:235]`,
   called from `AFGBuildableHologram::ConstructInstance()` ←
   `UFGBuildGunStateBuild::InternalConstructHologram()` ← AIMod's
   `ConstructWaterPumpAtCandidatePosition` lambda.

**Root cause**: `ConstructWaterPumpAtCandidatePosition`'s
water-volume-lookup loop tried `AFGWaterVolume::EncompassesPoint()`
first, but fell back to the volume NEAREST by actor-location distance
if no volume actually contained the point - "so a caller isn't forced
to hit a boundary exactly." For a literal on-land position, no volume
ever really contains it, so the fallback always fires - and since the
main ocean's actor origin is huge, it's the "nearest" volume for
almost anywhere on that side of the map, including dry land. The code
then treated that wrong volume as a genuine target:
`CanPlaceResourceExtractor()` is a volume-level flag (true for the
whole ocean, not a check of the exact point), so it passed too. The
hologram was driven at a synthetic hit that doesn't correspond to real
water geometry - `TrySnapToActor()` apparently failed to populate
`mSnappedExtractableResource`, but `GetConstructDisqualifiers()` did
NOT reliably flag this (the exact same "disqualifier isn't a reliable
gate for this precondition" failure mode already documented below for
`world.placeBuilding` + extractor recipes - see "never use
`world.placeBuilding` for extractor recipes"). `bCanConstruct` came
out true, construction was attempted, and the engine's own
unconditional assert took the whole process down - a hard crash, not
a catchable disqualifier.

**Fix**: `EncompassesPoint()` is now a HARD requirement for both
`world.constructWaterPumpAtPosition` and
`world.constructWaterPumpNearReference` (they share the same internal
helper) - no more nearest-by-distance fallback as an actual
construction target. A point outside every real water volume's actual
collision shape now fails cleanly with `NO_WATER_VOLUME_FOUND` before
the hologram is ever touched (the nearest volume is still computed,
but only to name it in the error message). Same "refuse outright
rather than gamble on `GetConstructDisqualifiers()`" posture this
project already uses for extractor recipes going through
`world.placeBuilding` and for spline-snapped buildables - this is now
the THIRD confirmed instance of this exact failure class in this
codebase (spline-snapped Conveyor Monitor, resource-node extractors
through the wrong entry point, now water volumes) - **standing
lesson, reinforced again: never trust `GetConstructDisqualifiers()`
alone to catch a missing mandatory hologram reference; a wrong-target
guess must be refused by our OWN code before `InternalConstructHologram()`,
not left to the engine's own disqualifier list.**

Compiled clean on both `FactoryEditor` and `FactoryGameSteam`
Shipping. **The crash killed the running game process** - this fix is
NOT yet redeployed/re-verified live (needs Alpakit + relaunch before
the rest of the water-pump/stackable-support test backlog can
continue). Positive-path results (steps 1-3 above) stand as
CONFIRMED LIVE WORKING.

**UPDATE, same day, after redeploy**: user redeployed and placed a
fresh reference pump/foundations on a different shoreline. Re-ran the
EXACT on-land call that crashed the game before — now correctly
returns `CANNOT_CONSTRUCT`/"Must be placed on deep water!" instead of
crashing. **Fix confirmed live.** Also found a new real fact while
re-confirming the positive path: this shoreline needed pumps ~5000+
units offshore before water was deep enough (3000 failed "Must be
placed on deep water!", matching disqualifiers `EncompassesPoint()`
now correctly surfaces instead of the old wrong-fallback silently
letting a bad point through) — a real, per-shoreline depth constraint
distinct from the pump-to-pump clearance spacing found earlier; the
first test site happened to succeed at 3000.

## LESSON: never assume a foundation's pivot-to-top offset — different foundation TYPES have wildly different thickness (found live 2026-08-31)

While re-testing `world.constructStackableSupportOnTop` after the
crash fix above, built a 3-level stackable-support column at a literal
Z computed from a nearby `Build_Foundation_8x1_01`'s pivot position
(`z=-1650`) plus an assumed 400-unit thickness (the value this project
had previously confirmed for a DIFFERENT foundation type,
`Foundation_8x4_01` - see the "Multi-story stacked builds" section
below). User caught it live: "your test stackables are floating above
the foundation." Real `world.groundHeight` at a clean (unbuilt-on)
tile of the SAME foundation row: `z=-1600` — only 50 units above the
pivot, not 400. `Foundation_8x1_01` is evidently a much thinner piece
than `Foundation_8x4_01` despite both being "a foundation" - the
naming (`8x1` vs `8x4`) does encode a real, different thickness, not
just footprint. **Standing lesson, sharpened**: a pivot-to-top offset
confirmed for ONE foundation recipe/class does NOT transfer to a
different foundation class, even one that looks superficially similar.
Always re-verify with `world.groundHeight` at a clean tile of the
SPECIFIC foundation type in play, every time, rather than reusing a
number from a previous session or a different recipe.

## LESSON: `world.constructStackableSupportOnTop` calls are occasionally flaky on the first attempt, and `result.buildableId` can be stale in a dense column (found live 2026-08-31, after the `GetStackHeight()` fix)

Re-testing the fixed `world.constructStackableSupportOnTop` (see the
water-pump crash entry above for the redeploy context) surfaced two
real, practical findings, neither a residual correctness bug:

1. **Retry-once flakiness, same class already documented elsewhere in
   this file** ("Surface is too uneven!" on other buildables): roughly
   half of the on-top calls in this session failed on the FIRST
   attempt with `CANNOT_CONSTRUCT`/"An identical buildable is already
   built there!" and then succeeded on an IMMEDIATE retry with no
   other change (same reference, same recipe, same everything). Budget
   for this specifically on `constructStackableSupportOnTop` - retry
   once before treating a failure here as real.
2. **`result.buildableId` can report the WRONG buildable** - observed
   it returning the REFERENCE pole's own id (not a newly-constructed
   one) immediately after a delete+rebuild at the same X/Y, even though
   `result.detail.foundCount`/`buildableIds` correctly showed 2 real
   nearby matches. Root cause: the proximity-based "what did we just
   build" confirmation (shared with `world.constructStackableSupport`)
   picks whichever real buildable is CLOSEST to the intended construct
   location - normally fine, but stackable supports genuinely dock
   with real vertical overlap between segments (confirmed by the user:
   "stacking supports dock with about 50% vertical overlap of each
   other"), so a pre-existing neighbor can legitimately be closer than
   the actual new instance, especially right after a delete that
   hasn't fully settled. **Always cross-check
   `result.detail.buildableIds` (or a before/after `world.buildables`
   diff)** rather than trusting `buildableId` alone for this RPC family
   in a dense column - the same caution `world.constructStackableSupport`'s
   own docs already give for a single Zoop call, now confirmed to also
   apply across SEPARATE calls in a tight column.

## ONGOING: camera-hijack during construction, and the `instigatorStrategy` multi-fix build (2026-08-30)

Real, user-reported: `world.connectConveyor` (and separately,
`world.placeBuilding`) visibly hijacks the real player's camera during
construction — both drive the REAL player's BuildGun/controller, and a
prior fix forces the controller's rotation every poll tick to get
deterministic belt routing (`AutoRouteSpline` empirically reads it,
reason unknown - stub source). That forced rotation IS the hijack.

Fix attempt: `world.connectConveyor` gained `instigatorStrategy`
(`"RealCharacter"`/`"AIController"`/`"PlayerController"`) so several
theories are live-testable on ONE compiled build, no fresh redeploy per
attempt (explicit user request, since each attempt otherwise costs a full
game restart). Status: both decoy strategies (`AIController`,
`PlayerController`) get permanently stuck on `UFGCDInitializing` —
present immediately after the first click, never clears even after the
full 120-tick poll, despite `stepComplete`/`connectedCount` both
reporting correctly. `SetActorTickEnabled(true)` and continuous
`UpdateHologramPlacement()` reassertion in the poll loop were both tried
and neither helped. `"RealCharacter"` (the original, camera-hijacking
approach) is the only one confirmed to actually finish a belt right now.

**Update, same day**: tried both live, in one session (no recompile
between them, exactly what the multi-strategy switch was for) -
`"AIController"` and `"PlayerController"` fail IDENTICALLY, same
`CANNOT_CONSTRUCT "Initializing (hard)"`. This conclusively rules out the
"needs a Cast<APlayerController>" hypothesis - controller class isn't the
variable. Four real hypotheses tried total (AIController, PlayerController,
SetActorTickEnabled, continuous UpdateHologramPlacement), all failed the
same way - diminishing returns on further blind guessing.

**Full research pass done 2026-08-30** (two parallel deep-research agents
plus SML's native hooking system found independently) -
see `docs/camera-hijack-and-second-player-research.md` for the complete
findings, ranked next steps, and a concrete recommended smallest-safe
experiment (create a REAL second local player via
`UGameInstance::CreateLocalPlayer()`, force-disable its viewport, then
feed it into the already-scaffolded `"PlayerController"` strategy branch
- confirmed real, clean, revocable engine-level mechanism, with a real,
already-proven Central Storage inventory-bridging pattern to reuse for
its material costs). Read that doc before continuing this investigation
- it has file/line citations for everything and explicitly separates
confirmed findings from inference so you don't waste time re-deriving or
re-testing anything already settled. Short version: the next real lever
(`UGameInstance::CreateLocalPlayer()`) as construction instigator — a
much bigger change (would need to source materials from the Dimensional
Depot or a bridged inventory instead of the real player's carried
inventory) - user has already flagged interest in this, deferred pending
a dedicated design discussion. `ConstructBuildingAtPosition`'s own,
separate camera-rotation issue (confirmed live to exist for ordinary
`world.placeBuilding` calls too) has NOT been investigated yet.

## CRITICAL: `Recipe_SmelterMk1` is the Foundry, not the Smelter (found 2026-08-30)

`/Game/FactoryGame/Recipes/Buildings/Recipe_SmelterMk1.Recipe_SmelterMk1_C`
produces a **Foundry** (`Build_FoundryMk1`, displayName "Foundry" — the
advanced Alloy-tier building, 10 Modular Frame + 10 Rotor + 20 Concrete).
The real basic Smelter (`Build_SmelterMk1`, displayName "Smelter", 5 Iron
Rod + 8 Wire) is `Recipe_SmelterBasicMk1`. Caught live when the user
looked at what actually got built and it wasn't a Smelter — cost 2
buildings' worth of Modular Frame/Rotor/Concrete before being caught
(refunded via the dismantle fix, no material loss). **Always resolve a
building recipe by searching `world.recipeCatalog`'s real `displayName`
first** (`find_real_smelter.py` pattern: filter recipes by displayName
substring, not by guessing from the recipe's own class-name string) —
this project already has one precedent for this exact trap (Usable vs
Useable interface naming); recipe class names are not a reliable proxy
for what building they actually produce.

## CRITICAL: always pass explicit `yaw` on `world.placeBuilding` (found 2026-08-30)

Omitting `yaw` does NOT default to 0 — it silently fell back to something
like the player's live aim direction, live-confirmed inconsistent across
calls in the same batch (39 foundation tiles came back at yaw≈70° except
one stray tile at yaw≈-92° that also drifted ~140cm off its requested
X/Y). Square tiles at mismatched non-90°-multiple rotations don't tile
edge-to-edge even at identical centers — this is what produced a visibly
"chaotic diagonal platform with holes" the user caught live. **Always
pass an explicit `yaw` for any placement where orientation matters,
foundations included** — never rely on an implicit default.

## CRITICAL: machine row layout — side-by-side on the long axis, not end-to-end (found 2026-08-30, user-caught)

A rectangular machine (Smelter: 500×1000, short×long) should be arranged
**side-by-side along its short axis**, like books on a shelf — all
machines facing the same flow direction, packed tight along the narrow
dimension. The instinctive-but-wrong layout is end-to-end along the flow
axis (like train cars) with large gaps between each pair to fit a
splitter/merger — this wastes footprint and produces convoluted belt
paths. The efficient version: one splitter row north of the whole
machine row and one merger row south of it, each splitter/merger aligned
in the CROSS-axis with its own machine, chained to each other along the
row's long direction. Real connector layout confirmed live at yaw=0 for
both attachments (mirror images of each other): **Splitter** — Input
faces -X (west), Outputs face +X (east), -Y (north), +Y (south).
**Merger** — Output faces +X (east), Inputs face -X (west), -Y (north),
+Y (south). This lets a west-to-east splitter/merger chain tap
north/south into a machine row directly below/above it using the
pre-existing yaw=0 orientation — no rotation needed for the attachments
themselves, only correct axis placement.

## CRITICAL: `world.connectConveyor` can report `success:true` while leaving the destination end unattached (found 2026-08-30, reproduced twice)

Confirmed live, twice, both times on the same long-distance connection
(Miner Mk3 → a Splitter ~2900cm away, going through both routeMode=Curve
and routeMode=Straight at different attempts): the RPC returns
`success:true`, the SOURCE connector shows `connected:true` pointing at a
real belt actor, but that belt is a dangling stub near the source and the
DESTINATION connector still shows `connected:false` — the belt never
really reached its target. **`success:true` alone is not sufficient
evidence a conveyor connection is real, especially over longer spans or
larger Z deltas** — always re-check both endpoints via `world.connections`
after the call. Recovery pattern that worked: find the dangling belt via
`world.buildables` (it sits right next to the source, not at the
destination), `world.deleteBuilding` it, then retry `connectConveyor` —
different `routeMode` values succeeded on different attempts with no
clear rule found yet for which one will actually land the far end
(`Curve` failed on the same connection `Straight` later fixed, and
vice versa in the very same session). The C++ source itself documents a
related, probably-connected caveat: `AutoRouteSpline()`'s internal
pathing reads the player CONTROLLER's live rotation as an implicit
routing hint, independent of the connector-anchored hit data this mod
builds — the live player's position/facing at call time may be an
uncontrolled variable in whether a given attempt actually lands. Treat
any `connectConveyor` success as provisional until confirmed via
`world.connections`, and budget for at least one retry on longer belts.

## Workaround for a persistently-broken single conveyor connection: use a spare input slot instead (found 2026-08-30)

One specific Merger-to-Merger belt (in an otherwise-identical chain of 4)
failed `CANNOT_CONSTRUCT`/`"Conveyor Belt is too long!"` on every retry —
all 3 routeModes, a full delete-and-rebuild of both buildings, debris
cleanup, and even the player physically relocating next to it. An
intermediate relay Splitter didn't fix it either (and had its own side
effect: a nearby unrelated belt silently re-terminated onto the new
relay's free connector instead of the target it was actually asked to
connect to — inspect `world.connections` broadly after inserting any new
buildable near existing belts, not just the two endpoints you touched).
**The actual fix wasn't the belt at all**: a Merger has 3 input slots:
if the topology allows it, route the "extra" producer straight into
another merger's spare input instead of chaining through a dedicated
merger for it. 4 producers only need `ceil((n-1)/2)` merge points this
way, not `n-1` — cheaper AND sidesteps whatever made that one specific
segment unbuildable. When a single connection in an otherwise-working
repeated pattern refuses to build no matter what you change about it,
suspect the specific pair/segment itself rather than continuing to vary
routeMode/rebuild-the-same-two-buildings — try routing around it via a
free port on a neighboring buildable instead.

## NEW CAPABILITY: `world.terrainHeightGrid` - batched terrain survey, one call instead of hundreds (added 2026-08-30)

Direct follow-up to the terrain-scan lesson below: manually looping
`world.groundHeight` from Python (hundreds of calls during the circular-
platform terrain investigation) is dominated by per-call HTTP/JSON
overhead, not the trace itself. Added `world.terrainHeightGrid` - same
real `FindGroundAtXY` trace, run for a whole rectangular grid server-side
in one call. Response is two parallel flat arrays (`heights`/`found`,
row-major) rather than per-point objects, deliberately compact for a
bulk survey. Capped at 10000 points (~100x100 grid) since it's a
synchronous game-thread loop like every other trace in this file - an
unbounded grid risks a real frame hitch; over-cap requests get
`tooManyPoints:true` instead of quietly truncating.

Motivated by, and directly addresses, the broader "can we bake in
terrain knowledge in advance" question raised this session: since
Satisfactory's map is static within a game version, a caller can now
survey an area of interest in one fast call and cache the result to a
local file for reuse - this function itself does no caching, it's just
the fast primitive underneath a caller's own cache-building pass. Same
2.5D "highest point per XY column" limitation as `world.groundHeight`
itself applies (can't represent overhangs/cave ceilings) - not a new
constraint, just inherited from the same underlying trace.

**Not yet live-tested** - implemented and compiled same session as the
circular-platform work, game wasn't in a state to test immediately.

## CRITICAL: `world.deleteBuilding` NEVER refunded construction cost - a real bug, cost the user thousands of real items (fixed 2026-08-30)

`DismantleBuildable`'s C++ only ever called `Execute_Dismantle()` on the
target. That function does **not** refund anything by itself -
`GetDismantleRefund()` is a completely separate `IFGDismantleInterface`
function the real player-driven dismantle path
(`UFGBuildGunStateDismantle`) calls independently to compute what to give
back, confirmed from `FGDismantleInterface.h`'s own doc comments. This
mod never called it. Every `world.deleteBuilding` call, since this
project began, silently destroyed the FULL construction cost of whatever
it deleted, with no refund at all - and `RPC_REFERENCE.md` had claimed
"refunds construction cost" the entire time, an assumption that was
never actually verified live.

**Real, quantified cost**: the same session's circular-platform rebuilds
(three full 379-tile cycles, each requiring delete-then-rebuild for a
height correction) destroyed several thousand real Iron Plates with
nothing returned - directly caught by the user noticing dismantled
foundations weren't giving Iron Plate back. The user's own mitigation:
restoring a slightly older save snapshot on their next relaunch to
recover some of the lost materials.

**Fix**: `GetDismantleRefund(out_refund, noBuildCostEnabled=false)` is
now called BEFORE dismantling (the target must still be valid), and its
stacks are added directly to the player's carried inventory via
`AddStack(allowPartialAdd=true)` afterward - same direct-inventory-
manipulation pattern already proven reliable elsewhere in this codebase
(`SimulatedCraft`, `MovePortableMinerToInventory`), deliberately not
relying on the uncertain, stub-bodied `FDismantleHelpers::DropRefundOnGround`
ground-spawn path. If no local player/inventory exists, the loss is
logged rather than silently dropped.

**Lesson for this whole project, not just this bug**: a doc comment or
`RPC_REFERENCE.md` claim about what an engine call "does" is not evidence
it was ever actually verified live - this claim survived unchallenged for
the entire project until a live user directly noticed missing items. Any
existing "this refunds/returns/restores X" claim elsewhere in this
codebase that hasn't been explicitly live-verified should be treated with
the same suspicion until checked.

## CRITICAL: `world.groundHeight` can hit the mod's OWN already-placed buildables, not just terrain (found live 2026-08-30)

Checking terrain clearance directly under an already-built platform gave
a suspicious result: identical `-201cm` "intrusion" readings at TWO
completely different platform heights (6500 and 10500) - the exact same
offset at two unrelated Z values is not a plausible terrain coincidence.
Root cause: `world.groundHeight` runs a real physics ground trace, which
hits ANY solid collision geometry in range - including the mod's own
just-placed foundation tiles, not only real terrain. A foundation's
placement Z is not its top surface (`Build_Foundation_8x4_01` is 4m
*thick*), so its own collision geometry can register as "ground" a
couple meters above where it was placed.

**Practical rule**: only trust `world.groundHeight` readings taken
BEFORE anything is built at that location - a clearance check run against
an already-occupied footprint is contaminated by self-collision and
cannot be used to validate that same build. If you need to re-verify
clearance after building, sample well outside the built footprint, not
underneath it.

## CRITICAL: floating platforms need a real, generous terrain scan BEFORE picking a height - and rebuilding at the wrong height is expensive (found live 2026-08-30)

Built a floating circular platform (see the ring/fill sections below)
at a height chosen from the player's current position + a fixed offset,
without first checking what terrain existed under the chosen footprint.
User caught a real rock formation (desert biome) poking through the
platform - `world.groundHeight` sampling (with the correct z-anchor
usage - the search only covers +-1000 around the given anchor, easy to
miss taller terrain if the anchor doesn't reach it) found real terrain
up to ~7992cm locally, one search band higher than what a naive
±1000-around-original-height check would have found. A full rebuild at
a corrected height was needed - twice, since the SECOND height chosen
(informed by an incomplete search) still wasn't quite high enough either
attempt, until a systematic progressively-higher scan confirmed nothing
was found at all in a comfortably-high band.

**Practical rule**: before floating a platform, scan a real GRID of
`world.groundHeight` points across the full intended footprint (not just
a few random samples - narrow rock spires are easy to miss), using a
z-anchor high enough to search the full plausible height range, and keep
raising the anchor until a search band comes back with ZERO hits before
trusting that height as clear. Random sampling can miss a narrow spire
entirely, understating the real peak.

**Real, expensive cost of getting this wrong**: each height correction
meant deleting and rebuilding the entire platform (up to 379 tiles) from
scratch, and repeated build+delete+refund cycles for the same materials
drained the player's carried Concrete/Iron Plate stock enough to run out
partway through a later rebuild (see the "material exhaustion" note in
the fill section below) - a real, live-experienced cost of not getting
the height right the first time, not just wasted RPC calls.

## CRITICAL: `world.placeBuilding` on a spline-snapped buildable (Conveyor Monitor) CRASHED THE GAME (fixed 2026-08-29)

**This was a real, confirmed live crash during an unattended testing
session**, not a hypothetical - `world.placeBuilding` with
`Recipe_ConveyorMonitor` took down the entire `FactoryGameSteam` process.
User-supplied crash report:

```
Unhandled Exception: EXCEPTION_ACCESS_VIOLATION reading address 0x0000000000000010
AFGBuildableConveyorMonitor::SetSnappedSplineBuildable()
AFGBuildableHologram::ConstructInstance()
AFGBuildableHologram::Construct()
UFGBuildGunStateBuild::InternalConstructHologram()
UAIModFunctionLibrary::ConstructBuildingAtPosition's deferred poll lambda
```

**Root-caused from source, not guessed**: `AFGBuildableConveyorMonitor :
AFGBuildableSplineSnappedBase`. The base class's own real (non-stub, right
in the header) implementation:
```cpp
virtual void SetSnappedSplineBuildable( AFGBuildable* buildable )
{
    fgcheck( buildable->Implements< UFGSplineBuildableInterface>() )
    mSnappedSplineBuildable = buildable;
}
```
calls `buildable->Implements<...>()` with **no null check**.
`AFGBuildableHologram::ConstructInstance()` calls this during `Construct()`
with whatever got snapped during placement - `world.placeBuilding`'s
generic path never attempts a real spline (belt/pipe) snap at all, so
`buildable` is `nullptr` and the dereference crashes. Exactly the same
*class* of bug as the resource-extractor crash (a "must be snapped to X"
disqualifier exists specifically to prevent this, but the real construct
path doesn't itself defensively re-check before dereferencing) - except
this time it was actually triggered live, not just structurally analogous.

**Fixed**: `ConstructBuildingAtPosition` now refuses any recipe whose
buildable class derives from `AFGBuildableSplineSnappedBase` outright
(`WRONG_METHOD_FOR_SPLINE_SNAPPED`), same unconditional-refusal posture as
the extractor/vehicle refusals - keyed on the base class so it also covers
any other spline-snapped buildable this project hasn't encountered yet,
not just Conveyor Monitor. **No dedicated construction path exists for
this category yet** - would need the same "resolve a real target, snap to
it" treatment `ConstructExtractorOnNode`/`world.constructVehicle` already
got, applied to a real belt/pipe target instead of a node/station. Real
future work, not attempted here.

**Practical impact on the in-progress building-type sweep this crash
interrupted**: everything tested *before* Conveyor Monitor in that run is
valid data; everything logged immediately after it (blank error
code/message - the connection simply died mid-call) is not real findings,
just cascade noise from the dead connection, and needs retesting once the
game is back up.

## CRITICAL: `world.placeBuilding`'s default `gridSnapSize` (100) silently breaks precision geometry - set it to 0 (live-verified 2026-08-30)

First live use of `compute_outer_touching_ring()` (the circular-foundation
toolkit function) produced a visibly rough, gap-riddled circle instead of
the exact touching-corner ring the geometry predicts. Root cause: every
`world.placeBuilding` call defaults `gridSnapSize` to 100 (1m grid snap,
documented in `RPC_REFERENCE.md`) - the tool's exact, deliberately
off-grid coordinates were silently rounded to the nearest meter before
placement, breaking the touching-corner property (position deviations up
to ~130cm observed, scattered around the whole ring, not just one seam).

**Fix**: pass `gridSnapSize: 0` explicitly for any placement whose exact
coordinates matter (anything computed by a geometry tool rather than
eyeballed to a round number) - re-running the same ring with `gridSnapSize:
0` landed every one of 66 tiles within 0.02cm of the planned position,
and every adjacent pair's spacing matched the ideal touching-corner
chord to within 0.03cm. **Lesson for any future toolkit-driven
placement**: default grid snapping is fine for ordinary building, but
must be turned off for anything where the Python geometry layer computed
a specific non-round coordinate on purpose.

Also hit, and worth remembering: `ignoreAimLocation: true` is required
for placements far from/unrelated to the player's literal look direction
(a floating ring built around the player still needs it - the
deterministic-look fix aims at the TARGET position, but "Invalid aim
location!" can still hard-fail depending on the geometry) - this is a
named, documented bypass (`RPC_REFERENCE.md`), not a new finding, but
easy to forget to pass for a non-interactive layout like this one.

## LESSON: "N foundations wide" is ambiguous - diameter-in-tiles vs ring tile-count (2026-08-30)

User asked for a circular platform "21 foundations wide" - first attempt
misread this as `tile_count=21` (21 foundations AROUND the ring), which
produced a small ring (<7 foundations across) instead of the intended
~21-foundation-diameter (168m) circle. Corrected by treating "N
foundations wide" as a target DIAMETER (`N * foundation_size`), then
using `solve_outer_touching_ring_tile_count()` to find the tile count
that actually produces that diameter (168m → 66 tiles, landing at
168.13m - see the geometry note in
`controller/satisfactory_ai/layout.py` for why this small rounding gap
is unavoidable and harmless). **When a user specifies a circle's size in
"foundations," ask/confirm or default to diameter, not ring tile-count -
they're very different scales for the same phrase.**

## NEW CAPABILITY: interior fill for circular platforms - `compute_disk_fill_grid()`, with a real overhang trade-off found live (2026-08-30)

Follow-on to the circular ring: fill the interior with an ordinary
axis-aligned square grid, keeping only cells whose footprint intersects
the ring's inner disk (`ring_inner_edge_radius()`) - the minimum set that
guarantees no gaps. Verified numerically before touching the game (30000
random samples inside the disk, 0 uncovered) and live (313/313 tiles,
positions exact to 0.02cm, coverage re-verified against real
`world.buildables` data).

**Real trade-off found from user feedback, not caught by the numeric
check**: some boundary cells' FARTHEST corner (not their nearest point,
which is what the inclusion test checks) can stick out past the ring's
true outer edge - up to ~3.2m in the worst case, where the disk boundary
grazes a cell CORNER rather than a face. Added an optional
`max_reach_radius` param to `compute_disk_fill_grid()` to cap this by
additionally excluding any cell whose farthest corner would exceed it -
confirmed this eliminates the overhang, but tested numerically that it
also creates real gaps (~0.6% of the disk area, ~20 cells' worth,
concentrated right at the boundary) that would need smaller pieces (see
below) to close without reintroducing either the gap or the overhang.
**Not yet deployed live** - shipped the uncapped (guaranteed-no-gap,
minor-overhang) version for the actual rebuild since a real terrain
clearance issue (below) took priority; the capped+patched version is a
real, scoped follow-up, not abandoned.

**Half-foundations exist and are the right tool for that follow-up**:
`Recipe_QuarterPipeMiddle_Ficsit_4x1`/`_4x2`/`_4x4` ("Half Foundation (1/2/4m)")
- found in `world.recipeCatalog`, real half-footprint pieces (user's own
suggestion) that could patch the ~20 boundary slivers precisely instead
of a full 800cm tile overhanging past the ring. Real footprint/clearance
dimensions not yet confirmed via `world.buildableCatalog` - needed before
computing exact patch placements.

**Material cost of experimentation is real, not free**: building the
ring+fill (379 tiles) three times over (two height corrections, see the
terrain-scan lesson above) drained the player's carried Concrete/Iron
Plate enough that the third rebuild's fill ran out partway through (24/313
placed, `Missing materials!`) - each `Recipe_Foundation_8x4_01` costs 5
Concrete + 2 Iron Plate (`world.constructionCost`), so 289 remaining
tiles needed 1445 Concrete + 578 Iron Plate the player didn't have on
hand. `world.deleteBuilding` does refund cost, but repeated build/delete
cycles are not free in practice (inventory cap losses, or simply not
having enough stock cycling through several rebuilds) - **budget for
this when a live experiment might need several corrective rebuilds**,
and prefer getting height/geometry right via `world.groundHeight`
scanning BEFORE building over relying on being able to just delete and
retry cheaply.

## NEW CAPABILITY: train timetables + drone station pairing - `world.trains`/`world.trainStations`/`world.setTrainTimetable`/`world.setTrainSelfDriving`, `world.droneStations`/`world.pairDroneStations` (added 2026-08-29, not yet live-tested)

User asked whether AIMod could configure the timetable for a set of
train stations, or the source/destination for a drone. Both turned out to
have real, clean, well-documented public APIs - researched
(`FGTrain.h`, `FGRailroadTimeTable.h`, `FGTrainStationIdentifier.h`,
`FGTrainDockingRules.h`, `Buildables/FGBuildableRailroadStation.h`,
`FGRailroadSubsystem.h`, `FGDroneStationInfo.h`,
`Buildables/FGBuildableDroneStation.h`, `FGDroneSubsystem.h`) before
implementing, same discipline as every other addition this session.

**Trains**: `AFGRailroadTimeTable::AddStop`/`RemoveStop`/`GetStops`/
`SetStops`/`GetStop` are real, public, `BlueprintCallable` - a genuine
"replace the whole timetable" setter, not incremental. Each stop
(`FTimeTableStop`) is a station reference (`AFGTrainStationIdentifier*`,
obtained from `AFGBuildableRailroadStation::GetStationIdentifier()`) plus
a real `FTrainDockingRuleSet` (load-once vs fully-load-unload, dock
duration, AND-vs-OR combination of the two, and optional per-stop item
load/unload filters). `AFGTrain::GetTimeTable()`/`NewTimeTable()`/
`HasTimeTable()`/`SetSelfDrivingEnabled()`/`GetSelfDrivingError()` round
out the picture - `ESelfDrivingLocomotiveError` is a real, specific enum
(`NoPower`/`NoTimeTable`/`InvalidNextStop`/`NoPath`/`StationUnreachable`/
`StationUnreachableWithSignals`/`LongWaitAtSignal`), not just a boolean,
so `world.setTrainSelfDriving` surfaces it as informational detail rather
than treating a post-enable error as this RPC's own failure.

`AFGRailroadSubsystem::GetAllTrains`/`GetAllTrainStations` gave clean
enumeration for free - no `TActorIterator` scan needed for either (unlike
`AFGVehicle`, which has no subsystem-level "get all" and genuinely needed
one earlier this session).

**A stop's docking rule set doesn't fit cleanly as flat UFUNCTION
params** (a variable-length list of structs, each with two optional item-
class array filters) - `world.setTrainTimetable` takes the stops as a raw
JSON array string instead, parsed inside `SetTrainTimetable` itself via
the engine's own `FJsonSerializer`. The RPC dispatch layer
(`AIModHttpServerSubsystem.cpp`) just re-serializes the `params.stops`
array field straight back to a compact string before handing it off - a
new, pragmatic pattern for this file (every other write RPC so far has
used flat scalar params), worth remembering as the template for any
future RPC needing a variable-length nested-struct input.

**Drones**: `AFGDroneStationInfo::PairStation(otherStation)` is a real,
public, single-call pairing function - simpler than going through
`AFGDroneSubsystem::Server_PairStations` directly. Confirmed from source
that pairing is a single MUTUAL link (`mPairedStation` is one pointer per
station, not a list) - so "source and destination for a drone" is really
"which ONE station this one is paired with," cargo flows both directions
automatically, not a directional route to configure separately.
`PairStation(nullptr)` unpairs (confirmed via `OnPairedStationUpdate`'s
own doc comment: "newStation can be nullptr"). `AFGDroneSubsystem::
GetAllStations()` gave free enumeration, same as the railroad subsystem
above - genuinely well-designed subsystems across the board for this
feature, unlike much of this project's other research targets.

**Both new IDs are consistent with this project's established
conventions**: train ids are `AFGTrain::GetPathName()` (an `AActor`, same
non-`AFGBuildable` category as `AFGVehicle` from earlier this session -
NOT usable with `world.buildables`/`world.deleteBuilding`); station ids
(both rail and drone) are the underlying buildable's normal id, already
usable with `world.buildables` since both `AFGBuildableRailroadStation`
and `AFGBuildableDroneStation` are ordinary `AFGBuildable`s.

**Not yet live-tested at all** - implemented from source research only,
same posture as this session's other same-session additions. Full RPC
docs in `RPC_REFERENCE.md`.

## NEW CAPABILITY: full M.A.M. (research) status + automation - `world.mamStatus`, `world.startMamResearch`, `world.claimMamResearch`, `world.claimMamHardDriveReward`, `world.rerollMamHardDrive` (added 2026-08-29, not yet live-tested)

User asked to query which M.A.M. items are unlocked and the current
research/hard-drive-analysis status, plus automate M.A.M. functions to
the extent possible: selecting a locked-but-available item, submitting
ingredients, activating research, and telling the player when hard drive
analysis completes so they can review new alternate recipes.

**Researched from source first** (`FGResearchManager.h`, `FGResearchTree.h`,
`FGResearchTreeNode.h`, `FGHardDrive.h`) - unlike much of this project's
other FactoryGame research targets, `AFGResearchManager` has real,
well-documented public getters/setters (not just empty stub `.cpp`
bodies with no doc comments) - this system was straightforward to build
against with high confidence.

**Key finding: M.A.M. research is atomic, unlike HUB milestone
payment.** `AFGResearchManager::InitiateResearch(controller, schematic,
tree)` pays the FULL cost from carried inventory and starts the research
timer in one call - there's no analog to `AFGSchematicManager`'s
per-item `FSchematicCost` partial-payment tracking. So `world.startMamResearch`
is one call that does what the user asked for as two things ("submitting
ingredients" + "activating research") - the real engine doesn't separate
them. `InitiateResearch` itself returns `void` despite its own doc
comment claiming a bool return (stale/wrong doc text, not trusted) - this
function pre-validates with the real `CanResearchBeInitiated`/
`CanAffordResearch` gates before calling it, and verifies
`IsResearchBeingConducted` actually flipped true afterward, rather than
trusting a void call blindly.

**"Locked but available" is a real, exposed engine concept**: `world.mamStatus`
reports each research-tree node's `UFGSchematic::GetSchematicState()` -
the exact same `Locked`/`Available`/`Purchased`/`Hidden` enum already used
for `world.milestoneProgress`'s HUB schematics. `Available` = dependencies
met, not yet purchased = exactly "locked but available" in the user's own
words.

**Enumerating ALL ongoing research needed a reflective read**:
`AFGResearchManager::GetResearchBeingConducted()` returns only a SINGLE
schematic, but `mCanConductMultipleResearch` is a real flag - trusting the
singular getter would silently drop entries when multi-research is
active, the same class of mistake this project has hit before (vehicle
telemetry, ongoing-list gaps). Fixed by reading the protected-but-real-
`UPROPERTY` `mOngoingResearch` array via `FindFProperty<FArrayProperty>` +
`FScriptArrayHelper`, then `reinterpret_cast`-ing each raw element to the
real, fully-public `FResearchTime` struct type (the struct LAYOUT is
public even though the CONTAINER FIELD access is blocked by C++ access
specifiers - reflection bypasses the field access, not the type
definition) - a new variant of this project's established "read a
protected/private UPROPERTY via FindFProperty" pattern, one level deeper
(an array of structs, not a single struct field).

**Hard drives have no usable stable id, by design decision not
oversight**: `AFGResearchManager::mUnclaimedHardDriveData`'s real
`FHardDriveData::HardDriveID` field exists but has no public accessor;
`UFGHardDrive::mHardDriveID` is a genuinely PLAIN C++ int with no
`UPROPERTY` at all - unlike every other "reflect around a missing public
getter" case in this file, this one is truly unreflectable (never
registered with Unreal's reflection system in the first place). Solved by
identifying a target hard drive by CONTENT instead of id - any one of its
current `pendingRewards` schematic classes, via `UFGHardDrive::GetSchematics()`
(a real public method, no reflection needed at all for this part).
Confirmed safe from source:
`AFGResearchManager::GetAvailableAlternateSchematics` explicitly excludes
schematics already offered by an unclaimed hard drive from future rolls,
so a given reward schematic should only ever be offered by one unclaimed
hard drive at a time.

**`world.claimMamResearch` on a hard-drive-analysis schematic does NOT
itself grant a recipe** - confirmed from source
(`ProcessCompletedHardDriveResearch`'s own doc comment: "Creates a new
hard drive data struct with the completed research and then removes it").
Claiming the research just turns it into an entry in
`world.mamStatus`'s `unclaimedHardDrives` - `world.claimMamHardDriveReward`
is the separate follow-up step that actually picks one of the randomly-
rolled alternate recipes. Two real, distinct write operations for what
might look like one action in the in-game UI.

**Genuinely unclear, flagged rather than guessed at**: `UFGResearchMachine`
(a `USceneComponent` on the MAM building, `FGResearchMachine.h`) and
`UFGResearchRecipe` (a `UFGRecipe` subclass with its own
`GetResearchTriggerItems`/ingredient system, `FGResearchRecipe.h`) look
like a SEPARATE system from everything above - `UFGResearchMachine::
OnResearchStarted`/`OnResearchConcluded` match `AFGResearchManager`'s
real delegate signatures exactly, suggesting it's a reactive visual-
feedback layer (the "item mesh shrinks over time" animation during
analysis) rather than a second way to drive research. Not used by any of
the new RPCs - if live testing shows this assumption is wrong (e.g. some
research genuinely requires going through `UFGResearchMachine::SetResearchRecipe`
instead of `AFGResearchManager::InitiateResearch`), revisit this section.

**Confirmed from source, matches this project's established
player-independence pattern**: `AFGBuildableMAM` itself gates nothing -
`InitiateResearch`/`ClaimResearchResults` take no building reference at
all, so none of these RPCs require the player to be near a physical M.A.M.
building, same as every other write function in this file.

**Not yet live-tested at all** - implemented from source research only,
same posture as this session's other same-session additions built while
the game wasn't running. Full RPC docs in `RPC_REFERENCE.md`.

## NEW CAPABILITY: `world.milestoneProgress` + `world.payMilestone` - the HUB has no inventory, payment is pure bookkeeping (added 2026-08-29, not yet live-tested)

User asked to query HUB/Space Elevator milestone progress and whether
AIMod could move items from the player's carried inventory to the HUB,
since (per the user, confirmed from source) the HUB can only be fed by
the player directly - unlike the Space Elevator, which accepts belts.

**Researched from source first** (`FGSchematicManager.h`, `FGSchematic.h`,
`FGBuildableHubTerminal.h`, `FGBuildableTradingPost.h`,
`FGBuildableSpaceElevator.h`) - real finding: "the HUB" in player terms is
`AFGBuildableTradingPost::mHubTerminal` (`AFGBuildableHubTerminal`), which
holds **no inventory of its own at all** - confirmed by reading its full
header, a thin `AFGBuildable` with just a `mTradingPost` back-reference.
Milestone payment is tracked purely as `FItemAmount` bookkeeping on
`AFGSchematicManager::mPaidOffSchematic`, via the real, public,
`BlueprintCallable` `PayOffOnSchematic(schematic, TArray<FItemAmount>&
amount)`. The Space Elevator, by contrast, is a genuinely different,
normal `AFGBuildableFactory` with a real `mInputInventory` and real
`UFGFactoryConnectionComponent`s (`GetInputInventory()`,
`GetNextPhaseCost()`, `IsReadyToUpgrade()`) - already visible to
`world.buildables` and already belt-connectable via the existing generic
`world.connectConveyor` path (`FindFreeFactoryConnection` scans any
`AFGBuildable`, not a specific class) with **zero new code needed** -
directly matching the user's own framing that it "can be fed with
conveyor belts" unlike the HUB.

**`world.milestoneProgress`** (read) reports both: every HUB
milestone/tutorial schematic by tier (`GetHubSchematicsForTier`/
`GetTechTierState`/`GetRemainingCostFor`/`GetPaidOffCostFor`/
`IsSchematicPurchased`/`GetActiveSchematic`) plus every Space Elevator's
phase-upgrade state.

**`world.payMilestone`** (write) moves items from **carried** inventory
only (`AFGCharacterPlayer::GetInventory()`, same scope as the existing
`SimulatedCraft` - explicitly NOT the Dimensional Depot, same established
gap as construction; use `world.withdrawFromCentralStorage` first if
needed items are there). Per item still owed, submits
`min(remainingAmount, carriedAmount)` - can only ever move real,
affordable amounts, never fabricates or over-submits. Supports
`dryRun:true` (computes the plan, touches nothing) - always test this way
first, especially since a real redeploy wasn't available this session to
verify live.

**Real safety discipline applied, matching this project's established
restore-on-failure pattern** (`MovePortableMinerToInventory`'s ARMS-slot
move): removes items from the player's inventory FIRST, then calls
`PayOffOnSchematic` - if that returns `false`, every removed item is
immediately restored via `AddStack(allowPartialAdd=true)` before failing
with `PAYOFF_REJECTED`. Never destroys real items on a rejected payment,
even though the real rejection conditions are unconfirmed.

**Genuinely unconfirmed from source** (like nearly every other
FactoryGame class this project has researched, `FGSchematicManager.cpp`
is a fully auto-generated stub with every method body empty):
`PayOffOnSchematic`'s real contract - specifically whether it requires the
target schematic to already be the manager's *active* one
(`SetActiveSchematic`/`GetActiveSchematic` exist as a seemingly separate
concept, but `mPaidOffSchematic` is plural/tracks-multiple, suggesting
payoff may not actually require the active-schematic match), and whether
it mutates the `amount` array it's passed by reference (a
`UPARAM(ref)` - possibly to report leftover/excess, the same
ProcessEvent-by-reference uncertainty already documented for the Portable
Miner's reflective RPC call, though this one is a normal public
BlueprintCallable function, not reflection). Deliberately NOT gated on
"must be active" here - trusting the real engine call to enforce or not
enforce that itself rather than guessing at a restriction the source
doesn't actually state. `PayOffMilestone` logs the post-call `amount`
array's contents and echoes it in `result.detail.amountArrayAfterCall` on
a successful call specifically so the first live test can observe the
real contract instead of guessing further.

**Not yet live-tested at all** - the user was away from the game when
this was implemented ("not available to redeploy at this time"),
implemented and compiled from source research only, same posture as this
project's other same-session, not-yet-tested additions. First live test
should use `dryRun:true`, then a cheap/abundant milestone item, before
trusting this for anything valuable.

## Rail/vehicle path layout tips from the user (2026-08-29, not yet acted on)

Standing design guidance for the first real attempt at using
`world.constructRailroadTrack`/`world.constructVehiclePathSegment`:

- **Pairs, not single lines**: one lane/track each direction, like a
  divided highway or double-track railway.
- **Raised platforms, not raw terrain**: this is *why* both RPCs support
  `ignoreGroundTrace`/literal-Z - build a flat foundation bed first, lay
  paths/track on top.
- **Intersections as roundabouts**, not direct crossings.
- **A circular track loop with ingress/egress on all 4 sides** (N/S/E/W)
  gets real junction behavior - a train entering from any side can path
  onward in any direction - without needing the switch/signal system
  this project's first-pass track implementation deliberately skips.
- **Decorative pillars under long raised platforms** at regular
  intervals - purely visual, the game's physics don't require support,
  just simulates an elevated viaduct look.

Compute the roundabout/circle geometry with a real Python toolkit helper
when this is attempted (radius + track-piece length + tile count ->
placement points), not hand-computed inline - same
toolkit-not-solver principle already established for radial foundation
platforms elsewhere in this project.

## Railroad tracks and vehicle paths: same spline/two-click pattern as belts/pipes, both need a real build to confirm (added 2026-08-29)

Researched and implemented in the same session as vehicle/drone
construction, per the user's explicit "autonomously... if you find enough
information, proceed" direction, tracks prioritized. Both systems turned
out to be direct extensions of patterns already proven in this file, not
new mechanisms:

**Railroad tracks** (`world.constructRailroadTrack`): `AFGRailroadTrackHologram
: AFGSplineHologram` - the exact same spline base `ConstructPipe`/
`ConstructConveyorBelt` already drive. `ConstructRailroadTrack` is a
near-verbatim mirror of `ConstructPipe`'s two-click
`TrySnapToActor`+`DoMultiStepPlacement` flow, connecting two existing
buildables' `UFGRailroadTrackConnectionComponent`s (mirrors
`FindFreeFactoryConnection`/`FindFreePipeConnection` exactly - same
`IsConnected()` shape). Real numeric limits found in source (max length
~100m, min curve radius ~30m, max grade 25°) - trusted as plausible CDO-
dump values, not confirmed against a live build. Switches/signals
deliberately out of scope for a first pass - a plain track connector
reports "connected" once ANY piece is attached (`GetNumSwitchPositions()`
would distinguish a switch's open positions, but this project's first
pass doesn't need to).

**Vehicle paths** (`world.constructVehiclePathSegment`): genuinely
different shape from every other segment-based function in this file -
`AFGVehiclePathSegmentHologram : AFGBuildableHologram` directly (not
`AFGSplineHologram`), and critically, path nodes are FREE and
auto-created by segment placement (confirmed via
`AFGVehiclePathSegment::SetNodeConnections`'s own doc comment) - so
there's no "existing buildable with a connector" bootstrapping
requirement the way belts/pipes/tracks have. This function takes literal
X/Y/Z for both ends instead of buildable ids, same
`ignoreGroundTrace`/literal-Z convention as `world.placeBuilding` -
directly serves the "lay it on a flat platform" goal, since the hologram
itself does real floor-tracing/projection onto terrain
(`mPathFloorTraceElevation`, confirmed real header default, not a CDO
dump) that a flat foundation bed sidesteps the same way it already helps
every other placement in this project.

**Deliberately not built**: assigning a constructed vehicle to
auto-drive a route over these path segments. A real, public,
`BlueprintCallable` API exists for this
(`AFGWheeledVehicleIdentifier::SetVehicleRoute`/`AddWaypoint`/
`SetAutopilotEnabled`, resolved via `AFGVehicleSubsystem`'s path-node GUID
lookups) - structurally the same shape as `AFGDroneSubsystem::
Server_PairStations` for drones - but needs its own telemetry layer
(path node GUIDs aren't exposed anywhere yet) that wasn't built this
pass. Same "real, separate follow-up, not a blocker" posture as drone
station-pairing.

**Both systems' actual construct-path behavior is unconfirmed from
source** - `FGRailroadTrackHologram.cpp` and `FGVehiclePathSegmentHologram.cpp`
are both fully auto-generated stubs (empty/trivial bodies), same
situation this whole project has hit repeatedly. Compiled clean.

**Railroad track live-tested 2026-08-29, open issue found**: placed two
Buffer Stops (`Recipe_RailroadEndStop`) ~2000 units apart on the test
platform, both succeeded and were later cleanly deleted via
`world.deleteBuilding` (confirming real `AFGBuildable`-derived rail pieces
have no telemetry/dismantle gap, unlike vehicles). `world.testRailroadTrack`
between them (dry run) returned a safe, non-crashing failure:
`PLACEMENT_INCOMPLETE`, "step=0 connectionSnapped=false" - the two-click
`TrySnapToActor` snap never actually attaches to either End Stop's
`UFGRailroadTrackConnectionComponent`, unlike the equivalent pipe/belt
flow this was mirrored from. Not yet root-caused - candidates: End Stops'
connector may not be found by the same `FindFreeRailroadConnection` search
radius/logic used for factory/pipe connections, or `TrySnapToActor`'s
snap-candidate detection may need a track-specific hint the pipe/belt path
doesn't. A `world.railroadConnections` telemetry RPC (mirroring
`world.pipeConnections`) would help debug this by exposing the connector's
actual position/normal/connected-state directly, instead of inferring from
opaque disqualifier strings. **Open item, real RPC gap, not yet fixed.**

Vehicle paths (`world.constructVehiclePathSegment`) have not been
live-tested at all yet - budget for the first real attempt needing a few
iterations, consistent with every other new construction category added
to this project so far.

## Drones and wheeled vehicles: hologram-driven like buildings, NOT the Portable Miner's equipment mechanism (added 2026-08-29)

Researched from source (headers only - stub `.cpp` bodies tell nothing
about real behavior) before implementing, per the user's explicit "if you
find enough info, proceed" request, drones prioritized. Key finding:
`AFGDroneVehicle`/wheeled vehicles are built via `AFGVehicleHologram :
AFGHologram` - the exact same hologram root every other `Construct*`
function in this file already drives (`GetConstructDisqualifiers()`,
`CanConstruct()`, `Construct()` are all non-virtual/virtual members
inherited from `AFGHologram` itself, not something `AFGBuildableHologram`
adds). This is architecturally the SAME pattern as normal buildings, not
the Portable Miner's `AFGEquipment`+reflection mechanism - confirmed via
`AFGVehicle::mHologramClass` and `UFGVehicleDescriptor : UFGBuildDescriptor`
mirroring `AFGBuildable::mHologramClass`/`UFGBuildingDescriptor` exactly.

**Drones have one real structural difference**: `AFGBuildableDroneHologram`
carries a mandatory `mSnappedStation` reference, and three dedicated
disqualifiers exist specifically to block construction without one
(`UFGCDMustSnapStation`/`UFGCDOccupiedStation`/`UFGCDDroneStationHasDrone`
in `FGConstructDisqualifier.h`) - the same *shape* of precondition
(`UFGCDNeedsResourceNode` exists) that turned out not to protect against
the real resource-extractor crash (`ConfigureActor()` dereferences the
snapped reference unconditionally, the disqualifier is a soft safety net
the real construct path does not itself re-check). Whether
`AFGBuildableDroneHologram::ConstructVehicle()` has the same unguarded
dereference is **unconfirmed from source** (stub `.cpp`) - treated as a
real risk by analogy, not a confirmed fact. `world.constructVehicle`
requires a real `droneStationId` and always snaps to it
(`Hologram->TrySnapToActor(Hit)`, same call `world.placeExtractor` uses
for nodes) rather than gambling on an unsnapped attempt being safe.

**Wheeled vehicles have no equivalent mandatory-reference disqualifier**
found in source - `AFGWheeledVehicleHologram`'s `mSnappedPathSegment`/
`mSnappedPathNode` look optional (path-system snapping, not a hard
precondition), so `world.constructVehicle` treats them as free-placement,
same `x`/`y`/`z`/`ignoreGroundTrace` shape as `world.placeBuilding`.

**Confirming construction succeeded needed a different approach than every
other `Construct*` function**: `AFGVehicle` is not an `AFGBuildable` (it's
`AFGDriveablePawn`, a completely separate hierarchy), so
`AFGBuildableSubsystem::GetAllBuildablesRef()` - the proximity-match
confirmation every other function here uses - cannot find a newly-built
vehicle. `ConstructVehicle` instead does a real `TActorIterator<AFGVehicle>`
proximity scan after `InternalConstructHologram()`, same "never just trust
success" posture, different actor registry.

**Deliberately not built yet, both real and separate from construction
itself**: a freshly-built vehicle needs fuel (`GetFuelInventory()`) before
it can move under its own power, and a freshly-built Drone additionally
needs its station paired to a destination
(`AFGDroneSubsystem::Server_PairStations`, confirmed public/BlueprintCallable
in source, straightforward to add later) before it will fly a route.
Neither blocks construction - just means a built Drone sits docked, idle,
until both are set up.

**Live-tested 2026-08-29**: `world.constructVehicle` confirmed working
against the real game for Cyber Wagon (free-placement, no station snap
needed - consistent with the wheeled-vehicle no-mandatory-snap hypothesis
above). Construction itself succeeded cleanly.

**Two real gaps found live, both now fixed**: the built vehicle was
invisible to `world.buildables` and `world.deleteBuilding` returned
`TARGET_NOT_FOUND` for it - exactly the `AFGVehicle`-is-not-`AFGBuildable`
consequence predicted above, but it hadn't been extended to the *read* and
*delete* paths, only to construction-confirmation. Fixed with:
- `world.vehicles` (new RPC) - `TActorIterator<AFGVehicle>` telemetry,
  same id/buildableClass/position/rotation shape as `world.buildables`.
- `DismantleBuildable` (used by `world.deleteBuilding`) - falls back to a
  `TActorIterator<AFGVehicle>` path-name match when `FindBuildableById`
  misses, before giving up with `TARGET_NOT_FOUND`.

Drone station-snap, drone-unsnapped-crash-risk, and fuel/pairing remain
unconfirmed/unbuilt - only Cyber Wagon (a free-placement wheeled vehicle)
has been live-tested so far.

## CRITICAL: building recipe `ingredients` is only the BASE cost - customization (swatch/pattern/material) charges extra, use `world.constructionCost` for the real total (fixed 2026-08-28)

Discovered live: an RPC wall placement failed `"Missing materials!"` even
though the player had 2,600+ Concrete on hand and `Recipe_Wall_8x4_01`'s
own `ingredients` (via `world.recipeCatalog`) list only 2 Concrete. Root
cause, confirmed by the player's own manual build attempts: Satisfactory's
building customization system (swatch/pattern/material, set via the
Customizer or picked up from whatever was last "sampled") charges its OWN
separate ingredient cost on top of the base recipe, every time a NEW
instance of that category is built - not a `world.recipeCatalog` reporting
bug, a real second cost component the catalog was never designed to
reflect (it enumerates static per-recipe class data; the active
customization is dynamic, per-player, per-category state).

**This affects any buildable category with a customization system** - the
user's own report: a wall via the build menu cost 2 Concrete + 2 Iron
Plate; the same wall via hotbar with the "Concrete" pattern selected cost
4 Concrete (2 base + 2 from `Recipe_Material_Wall_Concrete`, confirmed in
the catalog). Not unique to walls - foundations, roofs, pipes, and likely
every other customizable building category work the same way
(`UFGCustomizationRecipe` is itself a `UFGRecipe` subclass with its own
`mIngredients`, and `FFactoryCustomizationData::GetAppliedRecipes()` is
generic across all of them).

**Fix**: `ConstructBuildingAtPosition`'s OWN construction-cost enforcement
(via the real `GetConstructDisqualifiers()`/`UFGCDUnaffordable` check) was
ALREADY correctly combining base + active customization cost the whole
time, matching a real interactive build exactly - confirmed by the fact
that the RPC wall placement genuinely needed the extra Iron Plate to
succeed. The gap was purely in TELEMETRY: nothing exposed what that real
total would be before attempting to build. New **`world.constructionCost`**
(params: `{"recipeClass"}`) spawns a real hologram for the recipe (same
`HotKeyRecipe` path construction uses, so it inherits whatever
customization the engine itself would apply), reads its `mCustomizationData`
(protected, read via `FStructProperty` reflection - no public getter),
sums `GetIngredients()` across the base recipe and every recipe returned
by `GetAppliedRecipes()`, and reports `baseIngredients` (matches
`world.recipeCatalog`) alongside the real `totalIngredients`. Never calls
`CanConstruct`/`Construct` - nothing is placed. Extractor recipes are
deliberately excluded (skip the hologram spawn, report base cost only) -
see the crash precedent below; not worth the risk for a category that
realistically has no meaningful swatch cost anyway.

**Always check `world.constructionCost`'s `totalIngredients` before a
placement, not `world.recipeCatalog`'s `ingredients` alone** - especially
before a multi-piece autonomous build (a house, a factory wall run) where
a mid-build "Missing materials!" wastes real player-visible feedback and
partial-build cleanup. Not yet live-verified against the packaged game
(implemented and compiled clean the same session it was found; pending
redeploy).

## CRITICAL: `world.placeExtractor` for solid ore Miners (Mk1/2/3) requires a real Portable Miner ITEM in inventory (discovered 2026-08-28)

Verified directly from `world.recipeCatalog`, not guessed: `Recipe_MinerMk1`,
`Recipe_MinerMk2`, and `Recipe_MinerMk3` all list one **Portable Miner**
(`BP_ItemDescriptorPortableMiner`) as a real construction-cost ingredient,
alongside the expected Iron Plate/Concrete - i.e. building a permanent
stationary Miner genuinely *consumes* a Portable Miner item, on top of raw
parts. `Recipe_WaterPump` and `Recipe_OilPump` do **not** have this
requirement - it's specific to solid-ore extractors.

If the player has zero Portable Miners in inventory, `world.placeExtractor`
fails with `CANNOT_CONSTRUCT` / `"Missing materials!"` even when targeting a
perfectly valid, unoccupied node - the error is real, not a bug in this
mod. Since `world.placePortableMiner`'s underlying `Server_SpawnPortableMiner`
call is still unresolved (see the Portable Miner section elsewhere in this
file / RPC_REFERENCE.md), there is currently **no RPC path to manufacture a
Portable Miner from scratch** - the player must already have one (crafted by
hand, or via the working ARMS-equip flow if one already exists), or enable
"Unlimited Resources for RPC Builds" in AIMod's mod settings (untested
whether that bypass covers this specific ingredient check, but it's designed
to cover exactly this class of disqualifier).

**Practical implication**: before starting ANY solid-resource extraction
chain via RPC (a build request that needs Miners, not just Water/Oil
Pumps), consider checking for this failure mode early rather than assuming
a "Missing materials!" on a Miner placement is about Iron Plate/Concrete
alone.

## CRITICAL: never use `world.placeBuilding` for extractor recipes (fixed 2026-08-27)

`world.placeBuilding` (`ConstructBuildingAtPosition`) is the generic
single-step placement path - it does **not** snap a real resource node
reference. Extractor recipes (Miners, Water/Oil Pumps, Fracking
Extractor/Smasher) have their own dedicated entry point,
**`world.placeExtractor`** (`ConstructExtractorOnNode`), specifically
because they need one.

**This was a live, confirmed CRASH, not just a bad result**: placing
`Recipe_MinerMk2` through `world.placeBuilding` with no real resource node
underneath (during a systematic placement stress-test, floating in open
air) resolved `canConstruct=true` - unlike `Recipe_MinerMk1` moments
earlier at a different test location, which correctly refused with
`"Must be placed on a Resource Node!"` - proving the "no resource node"
disqualifier is not reliably present for every extractor/location
combination through this path. Construction proceeded into
`AFGResourceExtractorHologram::ConfigureActor()`, which unconditionally
asserts on a valid `mSnappedExtractableResource` - a hard engine
assertion, not a catchable disqualifier, that **took the entire game
process down** (confirmed by the user's own crash dialog, same assert/
callstack as the log).

**Fixed** in `ConstructBuildingAtPosition` itself: it now refuses any
recipe whose buildable class derives from `AFGBuildableResourceExtractorBase`
outright (`WRONG_METHOD_FOR_EXTRACTOR`), unconditionally - not just
another `bIgnore*`-bypassable disqualifier, since the whole point is not
to gamble on `GetConstructDisqualifiers()` catching every case. Always
use `world.placeExtractor` for Miners/Pumps/Fracking buildings; if you
need to systematically test many building types (e.g. a placement
stress-test), route extractor recipes through `world.placeExtractor`
against a real resource node ID instead of the generic path.

**Fix confirmed live 2026-08-27**: re-ran the same crash scenario
(`Recipe_MinerMk2` at the exact location/coordinates that crashed the
game) - now cleanly refuses with `WRONG_METHOD_FOR_EXTRACTOR`, server
stays up. Followed by a full systematic place-then-delete sweep of all 7
extractor recipes through `world.placeBuilding` (all correctly refused,
no crash) plus every other practical building category (manufacturers,
generators, conveyor attachments, pipelines, poles, storage, vehicle
infrastructure, structural pieces) - 55 of 62 recipes placed, verified,
and deleted with zero issues; the other 7 were genuine, expected
`"Missing materials!"` inventory shortages (not bugs - real construction
cost the player's inventory didn't cover), not silent failures.

**Fixed 2026-08-27** (was "not yet fixed" as of the note above):
`world.placeExtractor` (`ConstructExtractorOnNode`) got the same
deterministic-look + disqualifier-ignore-list treatment as
`ConstructPipe`/belts - confirmed live it failed with `"Invalid aim
location!"` on a genuinely-valid Fracking Core node before the fix, and
placed correctly after it (see "Resource Well Pressurizers/Extractors
now supported" below). `world.placeExtractor` still has no explicit
`ignoreAimLocation` param the way `world.placeBuilding` does - the fix
here is unconditional (the aim disqualifier is always ignored, matching
belts/pipes' posture of "this is anchored to an explicit target ID, aim
should never matter").

## Resource Well Pressurizers/Extractors now supported (2026-08-27)

Per explicit user request. `world.placeExtractor`/`ConstructExtractorOnNode`
now accepts **any** extractor recipe via a new `recipeClass` param
(default `Recipe_MinerMk1` for backward compatibility) - previously
hardcoded to Miner Mk1 and restricted to solid resources only. The manual
`RF_SOLID`-only gate is gone; the real engine-side gating
(`mAllowedResourceForms`/`mRestrictToNodeType` and their disqualifiers)
already handles every extractor type correctly, so it's trusted the same
way `CanConstruct()` already is for everything else. Full mechanics in
`docs/resource-well-research.md`.

**The Pressurizer's real target is a distinct node type**
(`AFGResourceNodeFrackingCore`, `nodeType: "FrackingCore"` in the new
`world.resourceNodes` fields below) - **not** the same `AFGResourceNode`
class a Miner or the satellite Extractor uses. The node lookup was
widened from `AFGResourceNode` to `AFGResourceNodeBase` specifically to
reach it; this is why the Pressurizer was completely unplaceable before
today regardless of recipe/form fixes.

**`world.resourceNodes` gained `nodeType`, `coreId`, `satelliteState`**
specifically to make the required build order pollable instead of
guessed: Pressurizer on the core → power it → **poll `satelliteState`
until it leaves `"Untouched"`** (confirmed live: transitions to
`"Active"` within moments of the Pressurizer's power connection landing,
not a fixed/guessed delay) → only then can `Recipe_FrackingExtractor` be
built on each satellite (a real, engine-enforced construction
disqualifier, `UFGCDNeedsFrackingSatelliteNode` - not bypassable, and
correctly not bypassed here).

**A remote build site may have a much closer power option than a
`PowerPole`-only search suggests** - confirmed live wiring a Pressurizer
150 units from any existing pole/generator by finding a
`Build_PowerTowerPlatform_C` ("Power Tower", a real distinct buildable
class from `PowerPoleMk1/2/3`, built for very-long-distance transmission)
only ~7000 units away. **Search `buildableClass` for `"PowerTower"` too,
not just `"PowerPole"`, before concluding a site has no power option
short of building a whole new generator.**

**The satellite-extractor disqualifier flickered once even on an
already-`"Active"` satellite** (`"Must be placed on an activated Fracking
Satellite Node!"`, alongside a soft clearance disqualifier) - resolved by
an immediate retry with zero other changes; two more extractors on
different satellites both succeeded on the first attempt right after.
Same "transient disqualifier flakiness, retry once" pattern documented
elsewhere in this file - don't treat a single such failure as proof the
activation state is wrong.

## Pipes fixed, Hypertube support added (2026-08-27)

- **`world.connectPipe`/`ConstructPipe` had the exact same untreated
  camera-dependency bug** `ConstructExtractorOnNode` above still has -
  it predated the deterministic-look fix and failed consistently with
  `"Invalid aim location!"` even for a fully valid, in-range connector
  pair. **Fixed** with the same pattern as belts/lifts (deterministic
  look computed from the two connectors, reasserted every poll tick).
  Live-verified over a real ~4000-unit run (near the 5600-unit tier cap)
  - genuinely connected, confirmed by the resulting `Build_Pipeline`
  segment (plus its flow indicator landing exactly at the midpoint), not
  just `success: true`.
- **New `world.testHypertube`/`world.connectHypertube`** construct real
  Hypertube tube segments (`Recipe_PipeHyper` - NOT the
  `Recipe_HyperTube*` family, which are all attachments; see
  `docs/hypertube-research.md`), built with the camera-independence fix
  from day one rather than retrofitted. No `recipeClass` param - only one
  tube recipe exists.
- **Hypertube entrance/junction connectors need the same "opposite
  normals dock, same-direction normals don't" planning as every other
  connector in this doc** - confirmed live and by direct user visual
  observation: two entrances placed at the same default yaw both had
  their single connector facing the *same* world direction, so a
  "successful" `connectHypertube` call still produced a tube that curved
  around one of them instead of running straight. Diagnosed and fixed
  using the newly-added `world.pipeConnections` telemetry (below) rather
  than guessing: read both connectors' real normals, rotated the
  downstream entrance 180° (`world.placeBuilding`'s `yaw` param) so its
  normal pointed back at the first entrance, re-verified via
  `world.pipeConnections` that the normals were now opposite, then
  reconnected - genuinely straight run, confirmed visually.
- **New `world.pipeConnections`** (mirrors `world.connections`'
  `{ownerBuildableId, connectionType, connected, connectedBuildableId,
  position, normal}` shape, plus `isHypertube`) - added because
  `world.connections` was discovered live to **only ever cover
  `UFGFactoryConnectionComponent`** (belts/machines/splitters), leaving
  no way to read a real pipe's or hypertube's connector position/normal
  before placing one. Use this the same way `world.connections` is
  already used elsewhere in this doc: read it before rotating anything,
  don't guess.
- **The stale-ID-echo quirk (see "Known engine quirks" below) bit hard
  during the hypertube rotation fix**: a `world.deleteBuilding` +
  `world.placeBuilding` pair used to re-place a rotated entrance returned
  the *deleted* actor's ID in its response, not the genuinely new one -
  querying/connecting against that stale ID silently operated on nothing
  real. Always re-resolve via `world.buildables`/`find_near` by position
  after a delete-then-place, exactly as that section already warns -
  don't trust the `placeBuilding` response's `buildableId` blindly right
  after a delete of something at the same spot.

## Test builds: use a location near the player, not a remote floating test bed

Earlier placement/deletion stress-testing in this session (and the first
pipe/Hypertube tests) used a deliberately remote, high-altitude test area
(`(400000+, -200000+, 20000)`) specifically to avoid any risk of
interfering with the real build - reasonable for avoiding collisions, but
it meant the user couldn't see any of it happening and said so directly.
**Prefer placing test builds within a few hundred to ~1500 units of the
player's own position** (`world.player`) instead, still using a Z clearly
above real terrain when a level/deterministic float is needed (real
terrain near the player can have a genuine slope - confirmed live, a
196-unit height difference across 1000 units at this session's test
spot - float above it rather than fighting it, same as the general
"Coordinates and `z`" guidance above). Clean up test debris immediately
after the user has had a chance to look, same as always.

## Golden rule: never trust `"success": true` alone

Every RPC that reports success on a connection (`world.connectConveyor`,
`world.connectConveyorLift`, `world.connectPower`) can report success while
the actual result is wrong — a belt whose far end lands on a stale/unrelated
point, a wire that never reached the intended connector, etc. **After every
real (non-dry-run) connect call, re-query the destination's own connector
state (`world.connections`) and confirm `connected: true` with the expected
`connectedBuildableId`.** Checking only the source side is not enough — the
source can show `connected: true` to a real belt while that belt's *other*
end is dangling.

## Coordinates and `z`

- `z` in `world.placeBuilding` is **not a literal placement height**. It's
  the center of a ±1000-unit vertical search range for a downward ground
  trace (`ReferenceZ`). The real Z is wherever that trace hits real terrain,
  or — if nothing is found in range — the object floats at the literal
  requested Z instead.
- To build a genuinely **level, deterministic platform** (e.g. a foundation
  grid), pick a Z clearly outside real terrain's range at that location so
  every placement reliably falls into the "float" branch instead of
  ground-snapping to whatever bump happens to be nearby. `+1500` to `+2000`
  above the surrounding terrain has worked reliably.
- `gridSnapSize` rounds your X/Y to the nearest multiple of that size from
  *some* origin — expect up to `±gridSnapSize/2` drift from your requested
  coordinate. That's intended grid-alignment behavior, not a bug. When
  extending an existing grid, compute new positions as exact multiples
  matching the existing rows/columns (don't eyeball a "nearby" value — a
  half-step-off value can silently overlap the existing grid).
- Placing a new foundation or **power pole** *near* an existing one of the
  same type can trigger real snap-to-neighbor magnetism, silently pulling
  your X/Y/Z far from what you requested (observed drift: thousands of
  units, snapping toward an existing pole 5000+ units away). If a
  placement's returned position looks suspiciously close to unrelated
  existing infrastructure instead of your request, that's why — always
  verify the actual landing position via `world.buildables`/`find_near`,
  never trust the request coordinates blindly.
- **Repeated delete-then-place at the exact same X/Y climbs the resolved Z
  every time** (confirmed 2026-08-27, splitter and merger rebuilds both hit
  this): deleting and re-placing at an identical spot 3-4 times in a row
  produced a Z that kept rising by ~250-400 units on *every single retry*,
  even with an unchanged `z` reference and even after clearing every real
  foundation/buildable nearby. This looks like the same family as the
  "Phantom 'already built' collisions" quirk below, but manifests as a
  silently-wrong landing height instead of an outright placement failure -
  much easier to miss. **Always verify the landed Z via `world.buildables`/
  `find_near` after every place call**, not just the position; if it's
  jumped noticeably from a known-good value (e.g. an adjacent connector's
  real Z) or keeps climbing across retries, stop fighting the exact spot -
  offset the X or Y by a few hundred units instead (bridge the gap with a
  short belt if needed). Don't try to "fix" the Z by tightening the
  `z`/`ReferenceZ` search band or setting `ignoreClearance`/
  `ignoreInvalidFloor` - neither changed the outcome when this was tested
  live.
- When you know the exact height a placement *should* land at (e.g. a
  splitter that should sit right at a lift's output), pass that real,
  already-known Z as the `z` reference (read it from `world.connections` on
  the adjacent buildable) rather than a guessed/rounded value - this
  reliably avoids the ground trace latching onto unrelated nearby geometry
  (a taller foundation tile, another buildable's collision, etc.) within
  its ±1000 search band.

## Rotation (fixed 2026-08-26, `rotationScrollDelta` superseded 2026-08-27)

- `ConstructBuildingAtPosition` used to derive a building's default
  (pre-scroll) yaw from the player's camera bearing to the target — meaning
  identical `rotationScrollDelta` values produced different results
  depending on where the player stood. **Fixed 2026-08-26**: yaw is now
  pinned to a deterministic 0° baseline regardless of player position.
- **`rotationScrollDelta` itself is unreliable beyond `|delta|==1`** —
  confirmed live 2026-08-27 by sweeping delta=-1..-9 against the same
  recipe/location: resolved yaw was -10, +70, +40, 0, -50, -110, -180, +90,
  +170 degrees — not linear, not monotonic, no usable per-click increment.
  Root cause: `AFGHologram::Scroll()` called N times in a tight synchronous
  loop (no real tick between calls) behaves nothing like N real mouse-wheel
  notches. **Use `world.placeBuilding`'s `yaw` param instead** (an absolute
  target in degrees) whenever you need a *specific* orientation — it
  bypasses `Scroll()` entirely via a direct `SetActorRotation()`,
  re-asserted every poll tick. `rotationScrollDelta` still exists for
  callers that only need *some* non-zero rotation, not a chosen one.

## Splitter/merger have a FIXED internal connector topology (2026-08-27)

Confirmed live by probing a splitter and a merger at `yaw=0` and reading
every connector's real normal via `world.connections`: both hologram
classes have exactly 4 factory connectors arranged in a **rigid, fixed
local layout that a rotation can only spin as a whole, never reshape**:

- One "main" pair 180° apart (splitter: input ↔ straight-through output;
  merger: main input ↔ output). This pair's relationship never changes -
  whatever direction the output faces, the main input always faces exactly
  the opposite direction.
- Two "side" connectors, each ±90° from the main pair (splitter: the other
  two outputs; merger: the other two inputs).

Because the whole fan rotates together, **you cannot independently choose
"which side gets which direction" - only the whole fan's absolute
orientation.** E.g. a merger whose output must point a specific way (to
reach an already-placed downstream buildable) *forces* its main input to
face exactly opposite; only the two perpendicular side inputs are still
free to reason about. Plan the whole fan before rotating, not just the one
connector you care about most.

**How to compute the yaw you need**: probe the buildable at `yaw=0` first
(place it, read `world.connections`, note the 4 normals as angles), pick
the ONE known-normal you actually need pinned (e.g. "output must face
north"), compute `delta = target_angle - current_angle`, and pass
`yaw: (0 + delta) mod 360` to `world.placeBuilding`. The other 3
connectors' final directions fall out automatically from the same delta -
compute them too so you know what you're committing to before you place
and re-wire everything. Live-verified this way for both a splitter
(rotated 90° so its main output pointed at a constructor row) and a merger
(rotated 90° so its output matched the constructors' own flow direction).

## Lift → splitter layout: what actually works (2026-08-27, revised after live testing)

The original plan here ("rotate the lift's output to face the desired
direction, then place the splitter a short distance away in that
direction") turned out to rest on two false assumptions, both corrected
by live-testing a full teardown/rebuild the same day:

- **There is no "rotate an existing buildable in place" capability.**
  `world.placeBuilding`'s `yaw` only applies at placement time. A lift
  that's already connected at its input (to the source it's rising from)
  can't be re-oriented via any current RPC - don't plan around this
  existing.
- **A lift's output direction is NOT independently choosable - it's
  locked to match its input's direction.** Confirmed live: a lift is a
  straight, non-bending vertical column, so both ends share the same
  horizontal facing. Since the input's facing is itself determined by
  where the *source* (e.g. a Miner) actually sits relative to the lift,
  the output ends up facing that same, often "backward" (toward the
  source), direction - not whatever direction you'd like it to continue
  in. Placing a splitter in the "forward" direction and calling
  `ConstructConveyorLift` to reach it produced a dangling, unconnected
  output every time (it physically can't turn to reach a destination
  that isn't roughly along its locked axis).

**What actually works**: place the splitter to the **side** of the lift's
column (perpendicular to its locked input/output axis - e.g. lift
receiving from the south locks it to input/output-south, so put the
splitter east or west of it, not north), at the lift's **real output Z**
(read via `world.connections`), then connect lift → splitter with a
**separate `ConstructConveyorLift`-then-`ConstructConveyorBelt` two-step**
- don't expect `ConstructConveyorLift` alone to bridge any real offset;
build the lift up to its own natural (possibly dangling) top first, then
bridge to the splitter with an ordinary belt call. A same-direction
"reversal" belt (source's exit direction equals destination's input
normal) is unreliable even with `Curve` mode - it failed on the first
attempt and succeeded on an identical retry, so treat one failure here as
possibly transient and retry once before concluding the geometry is
infeasible.

## Match splitter/merger connectors to buildables by direction, not call order (2026-08-27, user-suggested; ordering trick found live)

Once a splitter's or merger's fan orientation is known (main connector +
two side connectors, each facing a specific real-world direction per the
topology math above), **connect each one to whichever downstream/upstream
buildable is actually closest to/in that direction** - e.g. an
east-facing splitter output should feed the east-most constructor, and a
west-facing merger input should come from the west-most constructor - not
whichever buildable happens to get called first. Getting this wrong still
"works" (`connectConveyor` doesn't care), but produces belts that visibly
cross or double back even though a short/straight routing existed.

**The catch**: `FindFreeFactoryConnection` doesn't let you choose *which*
free connector on the splitter/merger gets used - it just hands out the
next free one matching the requested direction, in a fixed internal
order. Confirmed live this session that this order is **different for
outputs than for inputs** (splitter outputs: main, then east, then west;
merger inputs: main, then west, then east) and is NOT geometry-aware - it
has nothing to do with which buildable you're connecting to. **Workaround**:
after using up the main connector, connect the *side* buildables in
whichever call order you empirically observe claims the correct connector
first (verify via `world.connections` after each call, delete and
re-order if swapped - this took exactly one correction each for the
splitter and the merger in this session, in opposite directions from each
other). There is no way to specify a target connector directly - ordering
your calls is the only lever.

## Player independence, take 2 (fixed 2026-08-27)

- The 2026-08-26 fix below (permanently ignoring the aim disqualifier) was
  **not sufficient on its own** — live-confirmed 2026-08-27 that
  `ConstructConveyorBelt`/`ConstructConveyorLift`'s internal pathing
  (inside `TrySnapToActor`/`DoMultiStepPlacement`, stub source) separately
  reads the player controller's *live* rotation as an implicit routing
  hint, completely independent of the disqualifier check and independent
  of the correctly connector-anchored `FHitResult`s passed in. Symptom: a
  `connectConveyor` call reports `success: true`, but the belt's far end
  lands near wherever the player was actually looking, not the destination
  — confirmed by the far endpoint's Y-coordinate matching the live player
  Y to full float precision, and independently by direct visual
  observation in-game ("visually it appears the player's camera position
  affects belt placement"). **Fixed**: both functions now point the
  controller at a *deterministic* target computed from the source/dest
  connector positions themselves (never the player's real aim) before the
  first click, and **re-assert it every poll tick** (`UpdateHologramPlacement`
  can reset it). No manual player aiming is required or has any effect
  anymore — connection results depend only on the two buildable IDs passed
  in.
- `ConstructPowerConnection` already had `ignoreAimLocation`/
  `ignoreWireSnap` params from earlier work — pass both `true` for
  deterministic, player-independent power wiring. (Not yet re-verified
  against this same "internal pathing reads live camera" class of bug —
  if power wiring shows the same symptom, the fix is the same shape:
  deterministic `SetControlRotation()`, reasserted per tick.)
- A connection that "used to work but now fails/mis-terminates for no code
  reason" after the player moved is the signature of this whole class of
  bug. The fix is always the same shape: replace the real (opaque)
  `CanConstruct()` poll-loop check with the manual disqualifier-ignore-list
  pattern **AND** point the controller at a value computed from the
  buildable geometry, reasserted every tick — never leave the camera
  either untouched (opaque internals may still read it) or a one-time-only
  `SetControlRotation()` (can be overridden before a later poll tick).

## Belts

- `FindFreeFactoryConnection` does **not** reliably pick the geometrically
  "obvious" connector (nearest, or first-in-declaration-order) — its exact
  selection logic is opaque. Don't predict which connector a call will use;
  place the connection, then read back which one it actually took.
- A destination connector requires approach from its **+normal side**,
  entering in the **-normal direction**. A source whose exit direction is
  the *same* as the destination's normal (i.e. a ~180° reversal) generally
  won't route with a simple `Curve`/`Straight`/`Default` call.
- As of the player-independence fix, direct routing that previously *looked*
  like a hard geometric limitation (e.g. a splitter's "backward-facing"
  output reaching a distant constructor) may now just work — **retest
  directly before reaching for an intermediate merger/splitter workaround.**
  Only fall back to routing through an intermediate buildable if a direct
  attempt genuinely fails after this fix.
- Try `"Default"`/`"Curve"`/`"Straight"` in that rough order when one mode
  fails — they are not interchangeable; a `Curve`-mode failure has
  succeeded with `Straight` (and vice versa) on the same connector pair.
- A single belt call has real length and slope limits: "Conveyor Belt is
  too long!" and "Conveyor Belt is too steep!" are genuine, not bugs. For a
  long or steep run, either use a vertical lift (see below) or split the
  run across two shorter belt segments via an intermediate buildable.
- Never route a belt over unsupported open air right next to a supported
  foundation platform — the transition can trip "Surface is too uneven!"
  even when the belt's own slope is trivial. Extend the foundation to cover
  the belt's full path, including any short "approach" segment near the
  source.

## Vertical conveyor lifts

- A lift travels **straight up/down only** — its X/Y is locked to wherever
  its bottom snapped to the source. It cannot bridge a destination that's
  offset diagonally (different X/Y *and* Z) in one call; if you try, the
  hologram may build a short segment in an unexpected (even wrong)
  direction rather than failing cleanly.
- Correct pattern: place the destination **directly above** (same X/Y as)
  the lift's natural snap point, only differing in Z. The lift's top may
  still land a small amount short of the destination's exact connector
  (tens to ~100 units) — bridge that residual gap with a short
  `ConstructConveyorBelt` call, same as the miner→lift→splitter chain.
- A single lift call has successfully spanned ~1600 units of rise in one
  shot; a much larger request (~1700+) that also required horizontal
  travel produced a broken (downward) result — keep dest directly above
  source and let a follow-up belt handle any remaining offset instead of
  asking one lift call to do both.

**History of this investigation (kept for context - the two sections
below supersede the intermediate conclusions)**: a copper-line build
originally hit what looked like a 100%-reproducible construction bug
(lift `Output` never connects, height stuck at a low default), got
misdiagnosed as "NOT a regression, already documented" via `git log`
(2026-08-30), then that "documented" conclusion was ITSELF disputed and
disproven by the user with a real, hand-built counter-example (a
conveyor wall → lift → conveyor wall structure the user built manually,
achieving a 1200-unit rise in one piece - flatly contradicting the
"fixed ~400cm" claim that had just been written down as fact). The two
findings below are what actually held up under that live counter-example.
**Lesson reinforced twice over in one investigation**: checking `git
log`/existing docs before diagnosing is good practice, but a documented
conclusion is only as good as the testing that produced it - a
plausible-sounding "confirmed non-bug" can still be wrong, and a live
counter-example from the user is stronger evidence than a prior
session's own diagnostic logging.

**Finding #1, fixed and live-verified 2026-08-30**: conveyor walls
(`Build_Wall_Conveyor_8x4_*`) ARE real, valid connection targets for
lifts and belts - the user's counter-example proved it, and
`world.connections` confirms each wall exposes one real
`UFGFactoryConnectionComponent` with `GetDirection() == FCD_SNAP_ONLY`
("special case for conveyor poles" per the engine header). The actual
bug: this mod's `FindFreeFactoryConnection`/`FindFreeFactoryConnectionNear`
required an exact Input/Output direction match, so a wall's SnapOnly-only
connector was never found at all - any `connectConveyorLift`/
`connectConveyor` call targeting a wall failed immediately with
`NO_FACTORY_CONNECTION`, before ever reaching hologram placement. Fixed
by falling back to a free SnapOnly connector when no exact-direction
match exists. Also worth knowing: `IsConnected()` on a SnapOnly connector
is *documented* (engine header) to always read `false` regardless of
real attachment state - don't use `world.connections`' `connected` field
to judge whether a wall/pole slot is free.

**Finding #2, genuinely open, actively being re-investigated (2026-08-31
update - supersedes the 2026-08-30 "deliberately postponed" framing)**:
even with Finding #1 fixed, a single `ConstructConveyorLift` RPC call
still only produces the hologram's ~400-unit default rise, regardless of
the real destination's distance - confirmed even feeding the exact
correct target position 40 times in a row with no change in height. A
real player CAN build an arbitrary height in one piece by however far
their camera happens to be aimed when they click (scroll wheel only
rotates the destination end's Input/Output orientation, not height) -
confirmed twice now: the user's hand-built wall→lift→wall reference
(1200 units, 2026-08-30) AND a real historical build the user pointed to
directly, on a 2026-08-27 11:51AM save, that achieved a 1600-unit rise
via THIS MOD'S OWN RPC, repeatedly.

**This is a real regression, not an inherent limitation** - `git log`
traces it to commit `524f4f951e` ("Fix belt/lift camera dependency for
real", 2026-08-27 12:31, ~40 minutes after the working save), which
added a `SetControlRotation()` override to `ConstructConveyorLift` for
player-independence. Before that commit there was no override at all,
and height came from the player's real, live camera - meaning the
2026-08-27 build only worked because a real human was actually standing
there aiming at the target when the RPC call fired. See
`AIModFunctionLibrary.cpp`'s `ConstructConveyorLift` doc comment for the
full numbered list of hypotheses tried - five ruled out with real log
evidence (rotation origin, connector type, absolute-vs-incremental hit
updates, a real `Hit.Component` reference, genuinely elapsed real time
up to 500ms), three more added 2026-08-31 from careful `FGHologram.h`
doc-comment reading: #6 injects the hit into `AFGBuildGun`'s own cached
trace; #7 and #8 both stem from `TrySnapToActor()`'s doc comment ("no
further location and rotation will be updated this frame *by the build
gun*") - #7 reads this as `UpdateHologramPlacement()` already calling
the real placement logic internally (so this mod's own separate,
explicit `TrySnapToActor()` call might be redundantly resetting a
correct height); #8 is a stronger, more literal reading of the same
sentence - since it names the BUILD GUN (not the hologram) as whatever
normally makes that call, and this function bypasses the real build
gun's per-frame tick entirely, NOTHING in this code path may have ever
called `SetHologramLocationAndRotation()` at all, in any hypothesis
tried so far. #8 calls it explicitly.

**#6/#7/#8 live-tested 2026-08-31/09-01, CONFIRMED STILL BROKEN** -
none of the three fixed it. Cleanly isolated this time (the first
attempt used two Storage Containers 1400/400 units apart in X/Y, a
real setup mistake caught live by the user - "the containers you built
are not vertically aligned... when doing these types of test it's
worth validating placements" - redone with both containers at matching
X/Y, 851 units apart in Z only). Real connector data via
`world.connections`, same result both times: the lift's `Input`
genuinely attaches to the source, but its `Output` lands at a FIXED
default offset from the source (`+300` local-Y, `+400` Z) completely
independent of the real destination position - not "close but short,"
literally the same offset regardless of whether the real target was
400 units away or 851. `success:true` on the RPC call the whole time.
This rules out #6/#7/#8 as fixes for the free-end-tracking problem -
whatever the real mechanism is, it isn't any of the three theories
tried so far. **A new, separate, unexplained finding from the same
retest**: after the second attempt, a duplicate Storage Container
(identical class, identical position to the real source) appeared in
`world.buildables` - stable, not transient, never explicitly built by
anything in that session. Not root-caused; flagged as its own
background investigation (background task `task_6f0276ff`) - possibly
related to `ConstructConveyorLift`'s snap/connector logic, possibly
unrelated, genuinely unknown.

**Hypothesis #9, QUEUED 2026-09-01 (code written + compiled, awaiting
redeploy - NOT yet live-tested)**: a fresh review of the ruled-out
evidence (rather than a ninth variant of the same hit-data idea) found
the decisive detail in the 2026-08-31/09-01 logs: on the END click,
`hitValid=true snapped=false` every time - so #8's explicit
`SetHologramLocationAndRotation(EndHit)` genuinely RAN and still left
height at exactly 400.0, while the BOTTOM click's identical synthetic
hit is consumed fine (the input really attaches). The top step ignores
`Hit.Location` specifically - which matches real gameplay: the bottom
is placed by pointing AT a thing, but height is set by aiming INTO THE
AIR, where there is often no blocking hit at all. Only two channels can
drive height in that case, and hypotheses #1-#8 never touched either:
(a) **`FHitResult::TraceStart`/`TraceEnd`** - the camera ray a real
build-gun trace always carries, left at zero-vectors by every synthetic
hit ever passed (degenerate ray → clamped to the 400 minimum, 100%
consistently); (b) the **live camera POV** read directly - the
PlayerCameraManager consumes `SetControlRotation()` during its own
per-frame update, and both clicks used to fire synchronously in ONE
frame, so any POV read was always stale. The queued fix does both:
**#9a** populates `TraceStart`/`TraceEnd`/`Distance`/`Time` on both
hits (the end click gets a horizontal ray at exactly the dest
connector's Z, through the connector toward the lift column, so every
plausible ray-based height computation agrees on the dest height);
**#9b** defers the end click one real tick with the deterministic look
re-asserted first. The existing height log lines will discriminate
which one mattered.

**Also found 2026-09-01, re-checking the "aligned" container test's
geometry**: that rig was geometrically unsatisfiable for FULL DOCKING
regardless of height logic. A lift arm connector docks 300 units along
its facing normal with normals opposed (verified from the working
reference lift's real connector data: column at
`destConnectorLoc + 300 * destNormal`) - and the test dest container's
free Input connector (y=-201900, normal -Y) faced AWAY from the lift
column (y=-200900), 1000 units of unreachable horizontal separation.
Height-tracking to ~850 should still have happened, so stuck-at-400 is
a real bug either way - but **"validate test placements" extends to
connector FACING, not just position**. The first known-satisfiable
verification target is the user's splitter rig near the player
(2026-09-01): source splitter `2147463102` output at z=-599, dest
splitter `2147462741` input at z=+401, connectors facing each other
across the column at (407800, -201200) - a real lift (`2147461808`)
already bridges them at exactly 1000 units rise, with its free end
rotated 180° from its input. Post-redeploy verification: dismantle that
lift (user-approved), call `world.connectConveyorLift`
source=`2147463102` dest=`2147462741` `freeEndRotationSteps=2`, expect
height 1000.0 in the logs and both lift connectors `connected=true` via
`world.connections` at the same positions the reference lift occupied.

**The practical fallback remains until #9 is live-verified**: chaining
lift segments each rising their own ~400 units works (each segment's
real `Output`, read via `world.connections`, becomes the next segment's
destination), or **design platform/miner-interface heights as multiples
of the ~400-unit default** so a single lift call reaches the target with
no bridging or chaining needed at all. If a platform's height can't land
on that offset, the residual gap needs a belt with enough horizontal run
to incline to it (see belts' real max-incline limit,
`world.conveyorBeltTiers`) rather than a steep short bridge.

**Existing-connection direction inheritance (2026-08-30, user-reported,
not yet independently verified in this codebase)**: if either end of a
new lift/belt lands on a connector that already has something attached,
the new piece's Input/Output orientation follows whatever's already
there rather than the requested direction. If both ends already have
conflicting orientations, the connection will likely fail outright.
Worth checking `world.connections` on both intended endpoints first if
either might not be genuinely empty.

**Open issue, not yet solved**: bridging FROM a lift's own real `Output`
connector using a normal `world.connectConveyor` call (lift as the
belt's SOURCE, not another lift) reproduced the one-sided "dangling
belt" pattern 3/3 times in a row - the destination's input connects fine
every time, but the lift's own output never shows `connected`
afterward, even after the established cleanup-and-retry routine.
Lift→lift chaining works perfectly; it's specifically lift→ordinary-belt
that's suspect so far. This echoes an earlier finding the same day where
a Miner (an extractor, also a "special" buildable class) as a belt
SOURCE showed the same one-sided dangling symptom - worth checking
whether `ConstructConveyorBelt_RealCharacterStrategy` has a similar gap
for non-machine source buildable classes as `ConstructConveyorLift` did
before its `EndHit` reassertion fix. Untried alternative: since a
platform build needs the lift to feed a genuine machine/splitter anyway,
try making the lift's destination the platform splitter directly (skip
the separate bridging-belt step) rather than lift→belt→splitter.

## Orientation: plan it, don't let it fall out of the connect calls

The first full demo chain (2026-08-26) was functional end-to-end but had
several visual/orientation problems, all from the same root cause: each
piece's rotation was left to whatever the placement or connect call
happened to produce, instead of being planned in advance relative to its
neighbors. Confirmed by direct user inspection in-game:

- A vertical lift's own output orientation is decided by the two-click
  connect process, not chosen by the caller. If the downstream buildable
  (e.g. a splitter) is placed with its own independent default rotation,
  the two can end up 90° mismatched relative to each other — functional
  (a `Curve`-mode belt can still bridge it) but visually asymmetric, and it
  cascades: a splitter rotated 90° off from "natural" puts all three of its
  outputs 90° off from a symmetric downstream row too.
- **Fix going forward**: decide the intended final orientation of the whole
  chain *before* placing the pieces (e.g. "flow runs north, lift and
  splitter both face north"), then explicitly set each buildable's rotation
  to match via `rotationScrollDelta` at placement time, rather than placing
  everything at default rotation and letting belts bend around whatever
  mismatch results.
- **User-confirmed 2026-08-26, root cause pinned down 2026-08-31**: a
  vertical lift's free (unconnected) end's facing direction lands
  unpredictably - the user described it as likely inheriting from
  whatever orientation the player's last-placed lift used, a convenience
  default for chaining similar builds, not something to rely on. Real
  players can only rotate it (in 90° increments, via mouse wheel) while
  the hologram is still being placed - **not after construction**,
  confirmed live 2026-08-31: `world.setBuildableRotation`'s
  `SetActorRotation()` reported `success:true` on an already-built lift
  but produced zero real change (`world.buildables`/`world.connections`
  read back completely identical afterward - very likely `Static`
  component mobility applied once a buildable is placed, matching the
  user's own real-gameplay experience exactly). Use
  `world.connectConveyorLift`'s `freeEndRotationSteps` param instead
  (`AFGHologram::ScrollRotate()` called on the hologram before the final
  click, added 2026-08-31, **not yet live-tested**) - it only affects
  whichever end is still free; the already-connected end's orientation
  is forced by its snap target regardless of scroll value, matching the
  user's own description ("not talking about changing which end is
  input/output, just the orientation of the unconnected end").
- A splitter can be placed to **dock directly onto a lift's (or belt's)
  output** — no intermediate connecting belt needed — if its input
  connector is positioned to exactly coincide with the upstream output.
  The first demo chain instead left a small gap (~100 units) and bridged it
  with a separate belt, which visually overlaps/clips with the splitter at
  that distance. Two acceptable outcomes: dock them with zero gap (no
  belt), or space them far enough apart that a visible connecting belt
  reads as intentional rather than as clipping — don't leave a
  half-measure gap in between.
- The same 90°-mismatch problem applies symmetrically on the **output**
  side: a final merger placed at default rotation after a row of
  constructors is not guaranteed to have its input(s) facing back toward
  those constructors or its output facing away cleanly. Plan the merger's
  rotation relative to the row the same way as the splitter on the input
  side.
- When placing a buildable that's meant to sit **on** a foundation
  platform, verify it actually landed within the platform's real footprint
  (both X/Y extent and matching Z) — not just "close to" the platform.
  The first demo's storage container landed hanging partially off the
  platform's edge at a rotation that didn't match the rest of the row,
  because its position/rotation were never deliberately chosen relative to
  the platform bounds or the merger it connects to, only to the merger's
  output direction. Pick the container's position and rotation explicitly
  (inline with the row, fully inside the foundation extent) rather than
  wherever the connect geometry happens to allow.

## Power

- Machines commonly have **exactly one** free power connector each in a
  fresh save — don't assume the "2 connections per machine" daisy-chain
  unlock is active; verify empirically (`world.testPowerConnection`
  reporting `NO_POWER_CONNECTION` on a machine that already has one wire is
  the machine being full, not a bug).
- Existing power poles in a large pre-built grid may already be at
  capacity from their own chain wiring (2 slots for prev/next neighbor +
  whatever else was already wired) — "No empty Power Line connections!"
  from a pole you haven't touched just means it's saturated; try a
  different pole rather than assuming a bug.
- Power line max length is a real, non-bypassable ~10000 unit cap ("Wire is
  too long!"). When the nearest pole with free capacity is farther than
  that, place a local pole partway and chain: local pole → machine, local
  pole → a grid pole within range.
- Placing a **new** power pole near existing ones is exactly where the
  snap-to-neighbor drift described above tends to bite — verify its actual
  landing position before wiring anything to it.
- Missing-materials failures (e.g. "Missing materials!") are a real
  inventory constraint, not a placement bug — check the player's/dimensional
  depot's inventory rather than debugging code.
- **A `Build_PowerPoleMk1` genuinely supports (at least) 4 simultaneous
  wire connections in this build**, confirmed live 2026-08-27 (one pole
  successfully took a grid link + 3 separate machine connections, only
  failing on a 5th attempt with `"has no free power connection
  component"`) — don't assume the commonly-cited "2 slots" figure without
  checking; it under-plans real capacity here.
- **There is no read RPC for power connection/circuit state** - to verify
  a wire genuinely exists (the same "never trust `success: true` alone"
  rule as belts), the reliable proxy is counting real `Build_PowerLine_C`
  actors near a pole via `world.buildables`/`find_near` (each real wire is
  its own actor - a pole with N genuine connections shows N `PowerLine`
  objects at/near its position), or - the real ground truth - checking
  whether downstream machines' `productionStatus`/`productivity` in
  `world.manufacturers` actually change over successive polls (a real,
  live-changing `productionProgress` proves genuine power+throughput, not
  just a stale one-time snapshot).
- **A freshly-placed pole can be silently corrupted** even when it reports
  a real, sane landing position (distinct from the "lands at (0,0,0)"
  total-failure case elsewhere in this doc): confirmed live a pole that
  accepted exactly one real connection, then refused every subsequent
  connection attempt (`"No empty Power Line connections!"`/`"Already
  connected with another wire!"`) even from a fresh, unrelated pole placed
  right next to it - while an otherwise-identical pole elsewhere in the
  same session genuinely supported 4 connections. Verified via the
  `PowerLine`-counting technique above that the stuck pole really did only
  have 1 real wire, ruling out "it's actually full." **Fix**: delete and
  re-place the pole (at a slightly offset position, per the general
  debris-avoidance pattern) rather than debugging further - this
  immediately resolved it.
- When the nearest pole with free capacity is farther than the ~10000 unit
  wire cap, split the gap with an intermediate pole roughly at the
  midpoint rather than assuming the route is infeasible - a single
  ~13000 unit gap was successfully bridged this way with one extra pole,
  two ~6600 unit segments.

## Known engine quirks to watch for

- **Stale-ID echo**: `world.placeBuilding` called immediately after
  `world.deleteBuilding` can return the *deleted* actor's ID instead of the
  new one. Always verify the returned ID actually resolves to the expected
  new position/class via `world.buildables` after a delete-then-place
  sequence.
- **Phantom "already built" collisions**: a deleted lightweight/instanced
  foundation can leave an invisible collision-only remnant that doesn't
  show up in `world.buildables` queries, causing a fresh placement at that
  exact spot to fail with "An identical buildable is already built there!"
  Workaround: place at a slightly offset position rather than fighting it.
- Transient `NO_PLAYER` errors on an otherwise-valid call have resolved on
  a simple retry — don't treat a single occurrence as a real failure.
- **Colinear-overshoot connector mismatch** (2026-08-27, even after the
  deterministic-look fix above): if two *different* buildables both have a
  free input roughly along the same line from your source (e.g. a merger's
  input, then a storage container's input ~500 units further along the
  same axis), a `connectConveyor` call explicitly targeting the *nearer*
  one can still land on the *farther* one instead - confirmed live via
  `world.connections` showing the belt's other end genuinely attached to
  the wrong building, not just a dangling stale point. `FindFreeFactoryConnection`
  itself is correctly scoped to only the named destination buildable (read
  from source), so this is happening downstream in the same opaque
  spline/pathing internals already implicated elsewhere in this doc.
  **Workaround**: connect the farther/"downstream" leg *first* so its free
  connector is no longer available to be mistakenly claimed by an earlier,
  nearer-intended connection - e.g. wire merger→storage before any
  constructor→merger calls when they're roughly colinear. Always re-verify
  via `world.connections` regardless.
- **A belt can dangle unconnected on the very first attempt** even under
  the deterministic-camera fix, for a short/simple run - retry with a
  different `routeMode` before assuming something is actually blocked.
  Live example: a 300-unit dead-straight run failed with `Straight`
  ("Invalid Conveyor Belt shape!") but succeeded immediately with `Curve`.

## Debris discipline

Delete stray/failed test buildings (mergers, poles, belts) as soon as
they're identified as unneeded, rather than leaving them for a later
cleanup pass — leftover debris has repeatedly turned out to physically
block or confuse later connection attempts in the same area.

- **Sweep for stray `Build_ConveyorPole_C` actors too, not just belts**
  (2026-08-27): a partially-failed or later-superseded belt construction
  can leave behind an auto-placed support pole even after the belt itself
  is deleted or was never fully connected. These don't show up when you
  only search for `ConveyorBelt`/the buildable class you were placing -
  include `ConveyorPole` in any post-cleanup `find_near` sweep of a work
  area.
- **`Recipe_Pipeline`/`Recipe_PipelineMK2` leave stray `Build_PipelineSupport_C`
  actors behind after `world.deleteBuilding` on the pipe itself**, confirmed
  live during a systematic place/delete sweep of every building type
  (2026-08-27) - the pipe segment auto-spawns support structures the same
  way belts auto-spawn `ConveyorPole`s, and deleting the pipe doesn't take
  them with it. Include `PipelineSupport` in debris sweeps of any area
  where pipes were placed and removed.
- A `world.deleteBuilding` call that reports `success: true` can still show
  up in the *very next* `world.buildables`/`find_near` call before
  disappearing on a subsequent one - a real, brief propagation delay, not
  a failed delete. Confirmed live 2026-08-27 (a `Build_RailroadTrackIntegrated_C`
  auto-spawned by a Train Station placement showed up in a `find_near`
  sweep immediately after its own successful delete, then was gone on a
  second check moments later). If a just-deleted actor still shows up
  once, re-check before concluding the delete failed.

## Storage Tanks and Pipeline Junctions are `PCT_ANY`-only, not Producer/Consumer (fixed 2026-08-27)

`ConstructPipe`'s original connector lookup used
`FindFreePipeConnection(Buildable, PCT_PRODUCER/PCT_CONSUMER)` - an exact
`EPipeConnectionType` match. This works for machines that explicitly tag
their connectors (Refineries, Pumps, Blenders, Fracking Extractors -
`Producer`; consumers - `Consumer`), but **`Recipe_PipeStorageTank` (the
"liquid storage buffer") and `Recipe_PipelineJunction_Cross`/`_T` never
override the type - every one of their connectors stays at the base
`PCT_ANY`**, confirmed live via probe placements read back through
`world.pipeConnections` (Storage Tank: 2 connectors, both `Any`; Junction
Cross: 4 connectors, all `Any`, a real N/E/S/W cross). The strict
Producer/Consumer lookup found nothing on either, so `ConstructPipe` could
never connect to a Storage Tank or a Junction at all.

**Fixed** with a new `FindFreeFluidPipeConnection(Buildable, PreferredType)`
helper: tries the exact `PreferredType` match first (unchanged behavior for
Refineries/Pumps/etc.), then falls back to any free `PCT_ANY` connector
(explicitly excluding `UFGPipeConnectionComponentHyper` and `PCT_SNAP_ONLY`
connectors). `ConstructPipe`'s two connector lookups now both route through
this helper. Live-verified building a real 7-extractor → 2 new Junctions →
existing Junction → Storage Tank network (see below) - every segment a
genuine `Build_Pipeline` actor, confirmed via `world.pipeConnections`
showing all connectors `connected: true`.

**Real fluid pipe max spline length is ~5600 units** (`Recipe_Pipeline`
and `Recipe_PipelineMK2` both report `maxSplineLength: 5600.1` via
`world.pipelineTiers` - notably shorter than Hypertube's ~10000 limit).
A `connectPipe` attempt beyond this distance fails with `"Pipe is too
long!"` as a hard disqualifier - not bypassable via any `bIgnore*` flag,
since it's a real geometric constraint of the spline hologram, not an
aim/clearance check. When extractors are farther than ~5600 units from
the target buffer, route through an intermediate Pipeline Junction placed
so both its extractor-side and buffer-side hops stay under the limit,
rather than assuming every source can reach the destination directly.

**Merging N sources into a buffer with only 2 connectors**: use Pipeline
Junction Cross buildings (4 `Any` connectors each) as a merge tree. Reserve
one connector on the junction nearest the buffer for the buffer connection
itself - e.g. for 7 sources feeding one tank: junction C (nearest the tank)
takes 1 direct extractor + 2 uplinks from junctions B and D + the tank,
using all 4 connectors; B and D each take 3 extractors + 1 uplink to C.
This fits exactly (3+3+1 = 7 sources, 3 junctions, 10 pipe segments total)
and leaves the tank's second connector free. Confirmed live 2026-08-27 on
the `BP_FrackingCore10` water cluster.

## CRITICAL: `world.connectPipe` can report `success: true` while silently connecting the WRONG buildables (found 2026-08-27)

Rebuilding the same network above with a cleaner topology (per user
feedback that the first version's junction placement caused visually
tangled/crossing pipes) surfaced a real, reproducible correctness bug,
**twice**, on two different segments:

1. A `connectPipe(462537, Hub)` call and a separate `connectPipe(W2, Hub)`
   call, both attempted while `Hub` was geometrically hard to reach
   cleanly, each reported `success: true` — but **neither actually
   connected to `Hub` at all**. Both pipes instead routed up to a wildly
   wrong location (Z≈3900, matching the *player's own altitude and
   position*, ~150 units from where the player was standing on an
   elevated platform) and **snapped onto each other's dangling free end**,
   forming one long connected chain from the source extractor all the way
   to the other call's source, joined by a stray `Build_PipelineSupport_C`
   at the joint — confirmed live by the user directly observing a support
   and pipes appear near them. Traced via `world.pipeConnections`
   (`ownerBuildableId`/`connectedBuildableId` chase) since `world.buildables`
   alone doesn't show connectivity.
2. A later, simpler case: `connectPipe(443723, WA)` reported
   `success: true`, but `WA` showed **zero** connected connectors
   afterward — the pipe had one end genuinely on `443723` and the other
   end **dangling in open air**, ~4000 units away, connected to nothing.

**Root cause, best understanding without deeper C++ investigation**:
`ConstructPipe`'s multi-step spline construction (`TrySnapToActor`/
`DoMultiStepPlacement`, `AutoRouteSpline` internals — stub source, same
opacity noted elsewhere in this project) can complete and report success
without its final click's hit-test actually landing on the requested
`DestBuildableId`'s connector component. When the real destination is
geometrically awkward to reach (bad connector-normal alignment, or the
same underlying issue driving `"Pipe is too long!"` hard failures — see
above), the router appears to fall back to *something* physically nearby
rather than failing cleanly - in one observed case grabbing a totally
unrelated dangling connector far from either intended endpoint. **This
was NOT caught by the existing `success: true` result** - a deeper,
more dangerous version of "never trust `success: true` alone" than
previously documented, since even inspecting the *source*'s connector
state looked fine (`connected: true`) - only checking the **destination
side too**, and confirming both share the same real pipe segment ID,
revealed the problem.

**Not yet fixed in C++** - `ConstructPipe` does not verify post-construction
that the actual connected component's owner matches the requested
`DestBuildableId` before returning success. A real fix would need to
check this and either fail cleanly or auto-delete the wrongly-routed
segment, rather than leaving it for the caller to catch. Flagged, not
built - this needs dedicated investigation given `AutoRouteSpline`'s
opacity.

**Mandatory workaround for now, used successfully to finish this
network**: after every `world.connectPipe` call that reports
`success: true`, query `world.pipeConnections` and confirm the source
and destination buildable actually **share the same connected pipe
segment ID** - not just that each individually shows `connected: true`
(a misrouted pipe still marks its own real endpoint as connected). If
they don't share a segment, the call silently misrouted: find the real
pipe segment via the source's `connectedBuildableId`, delete it (and any
stray `Build_PipelineSupport_C` created alongside it - sweep by
proximity to both the intended route AND the player's own position, not
just the work site, since a misroute can travel there), and retry rather
than trusting the result.

**Also discovered while debugging this**: deleting a Pipeline Junction
does **not** delete the real pipe segments still attached to it - they're
left dangling with one end connected to whatever real buildable they
reached and the other end orphaned in open air, **still occupying that
buildable's connector slot** (confirmed: re-attempting to connect an
extractor whose old pipe-to-a-now-deleted-junction was never cleaned up
fails with `"has no free Producer or Any pipe connection component"`,
even though the junction itself is long gone). When tearing down a
junction to rebuild the network around it, delete every pipe segment
still attached to it FIRST (check via `world.pipeConnections` before
deleting the junction), not just the junction itself.

## Portable Miner: a genuinely different construction mechanism (equipment, not hologram) (2026-08-27)

Per explicit user request ("build support for placing this and other
related machines... also build support for managing machine inventory").
Every other `Construct*` function in this codebase drives
`AFGBuildGunStateBuild`/a hologram. The **Portable Miner**
(`AFGPortableMiner`) does not work that way at all - confirmed from
source, not guessed:

- `AFGPortableMiner` derives directly from `AActor`, not `AFGBuildable` -
  no hologram, no `IFGDismantleInterface`, no `GetConstructDisqualifiers()`.
- It's deployed as **equipment** (like the Golf Cart), via
  `AFGPortableMinerDispenser : AFGEquipment`. The real placement call is
  `Server_SpawnPortableMiner(location, resourceNode)` - a `protected
  UFUNCTION(Server, Reliable)`.
- Equipping it the "sanctioned" way is NOT
  `AFGCharacterPlayer::EquipEquipment()`/`SpawnEquipment()` directly -
  `SpawnEquipment` is `private`, plain (non-`UFUNCTION`) C++, with no
  reflectable or public entry point at all. The REAL public path a
  player's own hotbar key-press uses is
  `UFGInventoryComponentEquipment::SetActiveEquipmentIndex(index)`
  (public, `BlueprintCallable`) on the character's ARMS equipment slot
  (`AFGCharacterPlayer::GetEquipmentSlot(EEquipmentSlot::ES_ARMS)`) -
  found the item's stack index there first (`GetStackFromIndex`/
  `Stack.Item.GetItemClass()`), then called `SetActiveEquipmentIndex`,
  then polled `GetEquipmentInSlot(ES_ARMS)` until it resolved to a real
  `AFGPortableMinerDispenser*`.

**New technique for this codebase: calling a `protected` UFUNCTION via
reflection.** `Server_SpawnPortableMiner` is protected in C++, but it's
still a `UFUNCTION` - Unreal's reflection dispatch (`FindFunction`/
`ProcessEvent`) isn't gated by C++ access specifiers, only genuinely
`private`/non-`UFUNCTION` methods (like `SpawnEquipment` above) are truly
unreachable from outside the class. Pattern used:
```cpp
UFunction* SpawnFunction = Dispenser->FindFunction(TEXT("Server_SpawnPortableMiner"));
struct FSpawnPortableMinerParams { FVector Location; AFGResourceNode* ResourceNode; };
FSpawnPortableMinerParams Params{ TargetNode->GetActorLocation(), TargetNode };
Dispenser->ProcessEvent(SpawnFunction, &Params);
```
A plain local struct mirroring the UFUNCTION's declared parameters in
order works as the `ProcessEvent` params buffer for a simple RPC like
this (no return value, no other complications for a two-plain-value-param
Server RPC). This deliberately
bypasses `TraceForPortableMinerPlacementLocation`'s camera-dependent aim
trace entirely, using the real resolved node location instead - the same
player-independence principle as every other `Construct*` function, just
achieved differently since there's no hologram/disqualifier system to
plug into here.

**Real prerequisite, not a limitation**: the player must already have a
real Portable Miner item in inventory (crafted via `Recipe_PortableMiner`)
- `world.placePortableMiner` consumes a real inventory item exactly like
placing one by hand, it does not synthesize one. Fails with
`PORTABLE_MINER_NOT_IN_INVENTORY` if absent.

**Only works on real `AFGResourceNode`, not Fracking cores/satellites** -
`Server_SpawnPortableMiner` takes `AFGResourceNode*` specifically (unlike
`ConstructExtractorOnNode`'s wider `AFGResourceNodeBase` search), so
`nodeId` must resolve to a normal solid-ore node.

**Retrieval** (`world.retrievePortableMinerInventory`) is much simpler -
`AFGPortableMiner::GetOutputInventory()` is a clean public
`UFGInventoryComponent*`, moved via `AddStack(allowPartialAdd=true)` +
`Remove()` (only removing what was actually successfully added, so a
partly-full player inventory never loses items - just leaves the rest in
the miner for a later retrieval).

**No other similar "manually placed, must be emptied by hand, equipment-
dispensed" machine was found** - searched for other `AFGEquipment`-based
"Dispenser" classes; only `AFGGolfCartDispenser` (a vehicle, unrelated)
and `AFGPortableMinerDispenser` exist. Portable Miner appears to be
unique in this category, not one of several similar devices.

**Live-tested 2026-08-27, found and fixed a real bug**: the ARMS
equipment slot (`UFGInventoryComponentEquipment`) turned out to be a
genuinely SEPARATE small inventory component, not a view/filter over the
player's general backpack inventory. Confirmed two ways: (1) a Portable
Miner sitting only in the general inventory never showed up scanning the
ARMS slot's own stacks - `world.placePortableMiner` failed with
`PORTABLE_MINER_NOT_IN_INVENTORY` even though `HasItems()` on the general
inventory returned true; (2) once the user manually moved the item into
the ARMS slot via the in-game UI, the ORIGINAL check (which only looked
at the general inventory) failed instead - proving the two locations are
mutually exclusive, not a mirror. **Fixed**: `ConstructPortableMinerOnNode`
now checks the ARMS slot FIRST (covers "already equipped/slotted"), and
only falls back to moving the item there via
`Remove()`+`AddStack()` from the general inventory if it's not already
present - with the item restored to the general inventory if the move
itself fails, so a failed RPC call never leaves the player short an item.

## Reflective UFUNCTION calls need the function's OWN property layout, not a hand-rolled struct (2026-08-28)

Follow-up to the Portable Miner section above. The first live attempt at
calling `Server_SpawnPortableMiner` via reflection (`FindFunction`+
`ProcessEvent` with a hand-rolled `struct { FVector Location;
AFGResourceNode* ResourceNode; }` as the params buffer) compiled clean,
ran with `success: true` on the equip step, and reported no error - but
**silently produced no real actor** (`world.portableMiners` came back
empty after the call). No exception, no log warning, nothing - it just
quietly did nothing useful.

**Root cause**: a plain C++ struct's memory layout (size/alignment/
padding) is not guaranteed to match the `UFunction`'s own
UHT-generated parameter layout (`SpawnFunction->ParmsSize` and each
property's real offset) - there's no contract that a struct declared by
hand lines up byte-for-byte with what `ProcessEvent` actually expects to
find at each property's offset. `ProcessEvent` doesn't validate the
buffer's shape against anything; it just reads whatever bytes are at
each property's known offset, so a mismatch reads garbage (most likely
here: a garbage/invalid `AFGResourceNode*`) rather than crashing or
erroring - which is exactly why this failed silently instead of loudly.

**Fixed**: build the params buffer using the `UFunction`'s own
reflection data instead of assuming a layout:
```cpp
TArray<uint8> ParamsBuffer;
ParamsBuffer.SetNumZeroed(SpawnFunction->ParmsSize);
for (TFieldIterator<FProperty> PropIt(SpawnFunction); PropIt; ++PropIt)
{
    FProperty* Prop = *PropIt;
    if (Prop->GetName() == TEXT("location"))
    {
        auto* StructProp = CastField<FStructProperty>(Prop);
        *StructProp->ContainerPtrToValuePtr<FVector>(ParamsBuffer.GetData()) = TargetLocation;
    }
    else if (Prop->GetName() == TEXT("resourceNode"))
    {
        auto* ObjectProp = CastField<FObjectProperty>(Prop);
        ObjectProp->SetObjectPropertyValue_InContainer(ParamsBuffer.GetData(), TargetNode);
    }
}
Dispenser->ProcessEvent(SpawnFunction, ParamsBuffer.GetData());
```
Matching by the exact parameter NAME (`"location"`, `"resourceNode"` -
read directly from the function's declaration, not guessed) rather than
positional order. **This is the general, correct pattern for calling any
non-exported UFUNCTION reflectively in this codebase going forward** -
never assume a hand-rolled struct matches a UFunction's real layout,
always build the params buffer from `ParmsSize`+`TFieldIterator<FProperty>`.
Not yet confirmed this fully resolves the Portable Miner spawn (pending
redeploy + retest), but the earlier silent-failure symptom is now
explained and addressed.

## Orphaned pipe flow indicators: exact cleanup via `GetFlowIndicator()`, not proximity guessing (2026-08-27)

Follow-up to the section above - `AFGBuildablePipeline` has a real,
public, `BlueprintCallable` accessor for its own `mFlowIndicator`
UPROPERTY: `AFGBuildablePipelineFlowIndicator* GetFlowIndicator() const`.
No reflection needed (unlike the Portable Miner's protected Server RPC).
`world.cleanupOrphanedFlowIndicators` (no params) builds the set of every
indicator any live pipe's own `GetFlowIndicator()` actually returns, then
deletes every `AFGBuildablePipelineFlowIndicator` actor in the world NOT
in that set, via the same real `IFGDismantleInterface::Execute_Dismantle()`
path as `DismantleBuildable` - not `AActor::Destroy()`. This is exact,
not a guess, and safe to run even in a dense pipe cluster where proximity
heuristics were rejected as unreliable (see above).

## Two new determinism tools: `world.groundHeight` and `faceBuildableId` (2026-08-27)

Per explicit user request: repeated Z-height and rotation inconsistencies
across this whole project (see the "real z/gridSnapSize semantics",
`rotationScrollDelta`'s non-linearity, and connector-normal-matching
sections elsewhere in this doc) all shared the same root shape - the
caller (a human or an agent) had to already know a non-obvious mod-level
quirk and/or do real-world vector math externally to get a reliable
result. Two new tools remove that "special knowledge" requirement rather
than making placement more forgiving of guesses (a deliberate choice -
see the note at the end of this section):

- **`world.groundHeight`** (`{"x","y"}`, optional `"z"` anchor) - runs
  the EXACT SAME ground trace `world.placeBuilding` uses internally
  (factored into a shared `FindGroundAtXY` helper so the two can't drift
  out of sync) and reports the real resolved Z, as a plain read-only
  query with no hologram/construction involved. Query this FIRST, then
  pass the returned `"z"` straight back in as `world.placeBuilding`'s
  `"z"` - guaranteed to match, no more guess-and-iterate on what Z a
  given X/Y will actually resolve to.
- **`faceBuildableId`** (new optional param on `world.placeBuilding`) -
  resolves an existing buildable's real position
  (`world.buildables`-equivalent lookup done server-side) and computes
  the exact yaw needed to face it - `(TargetPos - PlacementLocation)
  .Rotation().Yaw` - fed through the same proven absolute-yaw mechanism
  the `"yaw"` param already used. This automates the manual "place at
  yaw=0, read the real connector normal via `world.connections`/
  `world.pipeConnections`, compute the needed delta, delete and
  re-place, re-verify" dance that was repeated by hand for the
  hypertube-entrance rotation fix and elsewhere this session. Takes
  priority over an explicit `"yaw"` if both are given. Fails with
  `FACE_TARGET_NOT_FOUND` if the id doesn't resolve.

**Scope note, worth remembering**: `faceBuildableId` orients the WHOLE
building to face a point - it does NOT (yet) reason about which SPECIFIC
connector on a multi-connector building (e.g. a splitter's 3 outputs)
ends up facing the target. For a single-connector building (most simple
machines, hypertube entrances) this is a complete, correct fix. For
multi-connector buildings, the connector-geometry-probing workflow
documented elsewhere in this file is still the right approach.

**Why this is "give exact numbers," not "make placement more forgiving"**
(a deliberate design line, matching this project's broader stance after
the pipe misroute bug earlier this session): both tools compute a REAL,
EXACT value from REAL geometry and hand it to the caller/mechanism that
already existed and was already proven reliable - they don't add new
tolerance, fallback logic, or silent auto-correction on the construction
path itself. A call that fails now still fails loudly for a real reason;
it just doesn't require the caller to already know an internal mod quirk

## Multi-story stacked builds (foundation → wall → roof): audit every layer's real Z before computing the next (2026-08-28)

A player-requested "2x2 foundation house with a doorway" (4 foundations,
8 walls, 4 roof pieces) needed **five** separate rebuild passes before
the walls sat level and the roof cleared the walls, live-diagnosed by
comparing what was *requested* against what `world.buildables` reported
was *actually* placed at every step. The underlying lessons apply to any
build that stacks one buildable on top of another, not just houses:

### A buildable's reported "position" is its pivot, not necessarily its top surface

`Recipe_Foundation_8x4_01` is a 4m-thick foundation slab (confirmed by
the user's own domain knowledge before this was root-caused). Its
`world.buildables`/`world.placeBuilding` "position" is the PIVOT, sitting
at the BOTTOM of that 4m block - the real TOP surface (where a wall
should rest) is `pivot.z + 200` (half the slab thickness) for this
specific recipe. Walls placed using the raw foundation pivot Z as their
search center sink 200 units into the foundation - visually, the
foundation fills most of the interior and the walls all end up flush
with each other but at the WRONG (too low) height.

**Never assume a "position" value is a usable surface height for
stacking.** Verify the real top surface with `world.groundHeight` at a
point already covered by the piece you're stacking on (e.g. query at a
foundation's center once at least one foundation is placed) BEFORE
computing the next layer's Z. This one query - `world.groundHeight` at
the foundation center, right after placing foundations - would have
caught the 200-unit-pivot mistake on the very first pass instead of the
fourth.

### Ground-trace-based Z placement (`world.placeBuilding`'s `"z"`) can miss real geometry sitting exactly on a tile-edge boundary, non-deterministically

Wall segments sit exactly on a foundation tile's edge by design (that's
what makes a flush perimeter). Live-confirmed: placing a wall's search
X/Y exactly on that boundary line sometimes finds the foundation's real
top surface and sometimes falls through to a much lower real surface
(raw terrain beneath/around the foundation) - **for the identical X/Y/Z
request, repeated back-to-back**. This is not a fixed function of input
coordinates; it visibly changed between otherwise-identical calls during
this session (proven by an isolated single-position test giving a
different result than the same request inside a batch moments later).

**Workaround, verified empirically**: nudge the search X/Y **100 units**
inward (i.e. all the way to the next `gridSnapSize` line, not a token 10
units - 10 units made no difference in this session's testing) toward
the piece you're trying to land on. This reliably escapes the edge
ambiguity. The tradeoff: because 100 units happens to also be exactly
one `gridSnapSize` step, this can shift the FINAL snapped position to a
different grid cell than intended (confirmed: a 50-unit nudge snapped to
the *next* grid line over, landing 100 units off from the un-nudged
edge). **Always audit the result's real X/Y/Z after every placement**
(`world.buildables`, matched by the returned `buildableId`) - never trust
the RPC's `success:true` alone for a stacked/edge-adjacent placement, and
budget for occasionally needing 2-4 delete-and-retry cycles per
problem piece even with the nudge applied.

### Ground-trace placement cannot resolve a Z above open interior space at all

A roof piece centered over the middle of a room (i.e. NOT directly above
any wall) has nothing solid within the trace's search range at roof
height - only the far-below floor. `world.placeBuilding`'s ground-trace
will walk straight past the intended (empty-air) roof height and land on
the floor instead, every time, regardless of the requested `"z"` search
center - confirmed by testing search values from the correct height all
the way up to +1000 units higher with no change in the (wrong, floor-
level) result.

The trace DOES fall back to a literal flat placement at the exact
requested `"z"` when nothing is found within roughly 1000 units either
side of the search center (confirmed: a deliberately absurd search
height like 2500, over 1000 units from the nearest real surface,
produces a placement at ~2500) - but this only helps when the *intended*
height is itself more than ~1000 units from the nearest real surface,
which a roof sitting one wall-height above a foundation floor (~400
units) is not. There is currently no way to place a buildable at a
precise literal height directly above open interior space through
`world.placeBuilding`.

**Working mitigation**: anchor the roof piece's search X/Y directly
above a REAL WALL SEGMENT (not the tile's open center) - the trace then
correctly finds that wall's top surface. This does mean the roof's pivot
ends up aligned with a wall line rather than perfectly centered over its
tile; for a 2x2 grid this is a minor, acceptable-looking offset, not a
structural problem, but it is a real compromise worth calling out to
whoever's reviewing the result rather than silently declaring the build
"done." If a future task needs precisely-centered elevated pieces, that
likely needs a genuine C++ addition (an explicit "place at this literal
world Z, skip ground-trace entirely" mode) rather than another RPC-level
workaround.

### General workflow this earns: build one layer, audit before computing the next

Don't compute an entire multi-layer plan (foundation Z → wall Z → roof
Z) up front from assumed offsets and batch-place all of it. Per layer:
place it, query `world.buildables` for the REAL resulting position of at
least one representative piece (or `world.groundHeight` at a point now
covered by that layer), and only THEN compute the next layer's Z from
that verified number. This is slower per-layer but produces a correct
result on the first real attempt instead of requiring a full audit-and-
rebuild pass after the fact - exactly the class of mistake this section
exists to prevent repeating.

### Fixed 2026-08-28: `ignoreGroundTrace` gives real literal-coordinate placement - the two workarounds above are now superseded

Both problems above (edge non-determinism, roof-over-open-space
fall-through) share one root cause: `world.placeBuilding` always resolves
Z via a real line trace, and a line trace can only find Z where something
solid actually exists to hit. `world.placeBuilding` now takes an
`ignoreGroundTrace` bool - when `true`, it skips the trace entirely and
places at the literal `(x, y, z)` given, with no line trace involved at
all. Requires `z` to be provided explicitly (fails `MISSING_REFERENCE_Z`
otherwise).

**Correct usage pattern**: query `world.groundHeight` once against a
point where a REAL surface is known to exist (an already-placed wall's
own reported Z from `world.buildables`, or a foundation's top via the
pivot+thickness/2 formula above), compute the true target Z for the NEW
piece algebraically from that known-good number, then place with
`ignoreGroundTrace:true` at the TRUE intended X/Y (the tile edge, the
quadrant center - whatever geometry actually calls for) and the computed
Z. No inward nudge needed for walls; no wall-anchoring-instead-of-
centering needed for roofs, since Z no longer depends on what a trace
happens to hit at that X/Y.

**This was root-caused live** (the house's roof tiles were found
overhanging 50% outside the walls, because they'd been centered on wall
corners rather than the quadrant center per the older workaround) but
**the fix itself has not yet been live-tested** - implemented and
compiled clean the same session the bug was found, after the game had
already been closed for the day. First priority next session: rebuild
the house's roof with `ignoreGroundTrace`, and re-run the wall perimeter
without the 100-unit nudge, to confirm this actually resolves both
issues before relying on it for new builds.
