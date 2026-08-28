# Placement lessons

A living reference of practical rules for placing and connecting buildables
via DocMod's RPC interface. Unlike the `*-research.md` docs (which are
dated investigation logs), this document is meant to be **read before doing
placement work** and **appended to whenever a new mistake or fix earns its
keep**. Keep entries short and actionable — link to a research doc for the
full investigation if one exists.

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
"Unlimited Resources for RPC Builds" in DocMod's mod settings (untested
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
- **User-confirmed, not yet re-verified in this codebase**: a vertical
  lift's input/output *can* be rotated in 90° increments — but only while
  it isn't yet connected to another belt/machine. Rotate it into the
  desired facing first, then connect. This wasn't used for the first demo
  chain; try it before falling back to letting the connect call decide.
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
to get the *inputs* right in the first place.
