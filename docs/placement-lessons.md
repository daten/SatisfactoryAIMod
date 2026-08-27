# Placement lessons

A living reference of practical rules for placing and connecting buildables
via DocMod's RPC interface. Unlike the `*-research.md` docs (which are
dated investigation logs), this document is meant to be **read before doing
placement work** and **appended to whenever a new mistake or fix earns its
keep**. Keep entries short and actionable — link to a research doc for the
full investigation if one exists.

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
