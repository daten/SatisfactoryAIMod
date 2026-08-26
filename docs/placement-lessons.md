# Placement lessons

A living reference of practical rules for placing and connecting buildables
via DocMod's RPC interface. Unlike the `*-research.md` docs (which are
dated investigation logs), this document is meant to be **read before doing
placement work** and **appended to whenever a new mistake or fix earns its
keep**. Keep entries short and actionable — link to a research doc for the
full investigation if one exists.

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

## Rotation (fixed 2026-08-26)

- `ConstructBuildingAtPosition` used to derive a building's default
  (pre-scroll) yaw from the player's camera bearing to the target — meaning
  identical `rotationScrollDelta` values produced different results
  depending on where the player stood. **Fixed**: yaw is now pinned to a
  deterministic 0° baseline regardless of player position. Verified:
  repeated placements from wildly different player positions now all
  produce `yaw=0.0`.
- `rotationScrollDelta` still isn't a clean "degrees" value — treat it
  empirically per building type if you need a specific facing, and verify
  the resulting `yaw`/connector normals afterward rather than assuming.

## Player independence (fixed 2026-08-26)

- `ConstructConveyorBelt`/`ConstructConveyorLift` used to point the
  player's camera at each connector to satisfy an "Invalid aim location!"
  disqualifier — meaning belt/lift construction could fail depending on
  where the player's actual camera happened to be pointed. **Fixed**: these
  two functions now permanently ignore that disqualifier instead (same
  ignore-list pattern `ConstructBuildingAtPosition` already used), and no
  longer touch the camera at all. Verified reliable (10/10 across straight
  and diagonal geometry) from ~19,000 units away from the player.
- `ConstructPowerConnection` already had `ignoreAimLocation`/
  `ignoreWireSnap` params from earlier work — pass both `true` for
  deterministic, player-independent power wiring.
- A connection that "used to work but now fails for no code reason" after
  the player moved is the signature of this whole class of bug. If it
  recurs anywhere else, the fix is the same: replace the real (opaque)
  `CanConstruct()` poll-loop check with the manual disqualifier-ignore-list
  pattern, not a `SetControlRotation()` workaround.

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

## Debris discipline

Delete stray/failed test buildings (mergers, poles, belts) as soon as
they're identified as unneeded, rather than leaving them for a later
cleanup pass — leftover debris has repeatedly turned out to physically
block or confuse later connection attempts in the same area.
