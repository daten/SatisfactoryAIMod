# Conveyor belt and power connection research — Phase 13/14 continuation

Recorded 2026-08-25. Toward the demo goal: place a Miner, Smelter, and
Constructor, configure recipes, connect them with conveyors and power,
and produce Iron Ingots → Iron Plates, all RPC-driven. This researches
the two capabilities not yet attempted: conveyor belts and power
connections. An initial pass was delegated to a research agent; every
load-bearing claim below was then independently re-verified directly
against the headers (per this project's established practice) before
being trusted for planning — noted inline where re-verified myself vs.
only reported by the agent.

## Power connections — the more tractable of the two

**`AFGWireHologram::SetConnection(int32 ix, UFGCircuitConnectionComponent* connection)`
is genuinely public** — re-verified directly at `FGWireHologram.h:51`
(the whole block from `public:` at `:19` through `protected:` at `:61`
covers it). This is a real, direct "set both endpoints" API — unlike
belts (below), no click-simulation/multi-step driving should be needed
in principle. `GetConnection(int32 ix) const` (`:52`) is the matching
getter. Both operate on the private `mConnections[2]` array
(`TObjectPtr<UFGCircuitConnectionComponent>`, `Replicated,
CustomSerialization`, `:117`).

**Wires ARE multi-step holograms** (`DoMultiStepPlacement` overridden,
`:30`) — contradicts an initial assumption that wires might be a
special connector-click-only build-gun mode. `EBuildGunState`
(`FGBuildGun.h:19-28`, already directly read this session) has exactly
five values — `BGS_NONE, BGS_MENU, BGS_BUILD, BGS_DISMANTLE, BGS_PAINT` —
**no dedicated wire-connecting state**. Combined with a real, verified
`Recipe_PowerLine.uasset` asset (`Content/FactoryGame/Recipes/Buildings/`,
confirmed to exist on disk), wires almost certainly go through the exact
same `HotKeyRecipe(Recipe_PowerLine)` → `BGS_BUILD` →
`UFGBuildGunStateBuild` → hologram flow already proven for Miner/
Constructor — just resulting in an `AFGWireHologram` instead of an
`AFGBuildableHologram`-only class.

**Working hypothesis, not yet tested**: `HotKeyRecipe(Recipe_PowerLine)`
→ retrieve the `AFGWireHologram` via the same `GetBuildGunStateFor`/
`GetHologram()` pattern already used → `Cast<AFGWireHologram>` →
`SetConnection(0, ConnA)` / `SetConnection(1, ConnB)` (both real
`UFGPowerConnectionComponent*` found via
`Buildable->GetComponents<UFGPowerConnectionComponent>()`, mirroring the
already-working `CollectFactoryConnectionTelemetry` pattern) → check
`CanConstruct()` → `InternalConstructHologram()` if true. **Whether
`SetConnection` alone is sufficient, or whether internal private
validation (`CheckValidSnap()`/`CheckLength()`, both private,
`FGWireHologram.h:74-75`) requires the multi-step flow to have run first
to populate some other internal flag, is genuinely unverified** — the
`.cpp` body is a stub. This is exactly the kind of thing to test with a
small, isolated dry-run experiment before committing to a full RPC
method, matching this project's established methodology.

**Power connectors on manufacturers**: not declared in
`AFGBuildableManufacturer.h`/`AFGBuildableFactory.h` in C++ (only a
`UFGPowerInfoComponent* mPowerInfo` exists at the base, a different
class from `UFGPowerConnectionComponent`). `AFGBuildable.h` does declare
generic `GetNumPowerConnections()`/`GetNumPowerConnectionsCached()`
alongside the equivalent factory-connection counters — strong
circumstantial evidence (not proof) that power connectors, like factory
connectors on some buildable types, are added at the Blueprint-class
level per building and discoverable the same generic way:
`Buildable->GetComponents<UFGPowerConnectionComponent>()`. Confirmed
independently: `UFGCircuitConnectionComponent` (the base class
`UFGPowerConnectionComponent` derives from) has public
`GetNumFreeConnections()`/`GetMaxNumConnections()`/`GetConnections()`
(`FGCircuitConnectionComponent.h:39-60`) — real, usable enumeration
API, same shape as the existing factory-connection telemetry.

## Conveyor belts — harder, no direct-connect shortcut

**Hierarchy**: `AFGConveyorBeltHologram` → `AFGSplineHologram` →
`AFGBuildableHologram` → `AFGHologram`. Belts are a real multi-step
state machine: `AFGSplineHologram.h` declares `ESplineHologramBuildStep`
(`SHBS_FindStart, SHBS_AdjustStartingPole, SHBS_PlacePoleOrSnapEnding,
SHBS_AdjustPole`), tracked in a replicated `mBuildStep`, readable via
public `GetCurrentBuildStep()`.

**No `SetConnection`-equivalent exists for belts.**
`AFGConveyorBeltHologram`'s `mConnectionComponents[2]`/
`mSnappedConnectionComponents[2]` are both **`private`**, no public
setter — a real, structural difference from wires. The only path in is
the inherited `TrySnapToActor(const FHitResult&)` (public, overridden)
fed a synthetic hit result **per click**, driven across
`DoMultiStepPlacement(bool)` calls until `CanTakeNextBuildStep()`
(public override on `AFGConveyorBeltHologram`) signals completion —
architecturally the same shape as everything already working, just
requiring 2+ steps instead of 1, with unknown-until-tested per-step
requirements (exact snap-target-selection logic lives in the stubbed
`.cpp`).

A `friend class FGBlueprintOpenFactoryConnectionManager` exists on
`AFGConveyorBeltHologram` with a `ConnectStateDirectly`/
`CanDirectlyConnectOpenState` fast-path used internally by blueprint
open-connection wiring — **not usable from AIMod's module** (friend
grants aren't extensible from outside the declaring module), and it
explicitly bypasses spawning any real belt actor (only for
directly-overlapping connections). Not a shortcut for the general case.

**Verdict**: belts need the same "drive the real flow, empirically
discover per-step behavior since the .cpp is a stub" approach that
worked for buildings, just with materially more unknowns (which hit
result satisfies `SHBS_FindStart`? does `TrySnapToActor` need to target
the connection component precisely, or just be near it?). Should be
tested incrementally and in isolation, not attempted as part of the
first full demo pass - if it turns out too costly, the demo's first
version could fall back to placing buildings close enough together that
their connectors directly overlap (if that's sufficient for the game's
own auto-connect behavior - unverified) rather than blocking on a full
belt implementation.

## Recipe assets — all verified to exist

`Content/FactoryGame/Recipes/Buildings/`: `Recipe_ConveyorBeltMk1.uasset`
(through Mk6), `Recipe_PowerPoleMk1.uasset` (+ Mk2/Mk3 + Wall variants),
`Recipe_PowerLine.uasset`, `Recipe_SmelterMk1.uasset` (already verified
earlier session), `Recipe_ConstructorMk1.uasset` (already verified),
`Recipe_MinerMk1.uasset` (already verified and used).

## SML

Confirmed (delegated pass): zero matches in `Mods/SML/Source/SML` for
"ConveyorBelt", "PowerLine", "PowerConnection", "SplineHologram" - same
established pattern, nothing to build on.

## Important gameplay constraint (from the user, 2026-08-25): direct machine-to-machine wiring may not be available by default

The game has two distinct ways to wire power, and this matters a lot for
`DebugCheckPowerConnection`'s design and any real power-connect
operation built on it:

- **Default/early-game**: machines connect to **power poles** (or
  directly to a power-producing building like a generator) - a machine
  typically has exactly **one** power connection slot.
- **Later-game unlock**: a progression unlock allows machines to be
  **daisy-chained** directly to each other, two power connections per
  machine, no pole needed in between.

**Implication for this project**: `FindFreePowerConnection` (in
`DebugCheckPowerConnection`, `AIModFunctionLibrary.cpp`) looks for a
`UFGPowerConnectionComponent` with `GetNumFreeConnections() > 0` on each
machine. If the daisy-chain unlock isn't active in the target save, a
production machine likely only has its one slot, reserved conceptually
for a pole connection - meaning `DebugCheckPowerConnection` called on
two machines directly may legitimately report `NO_POWER_CONNECTION`
(or a `CanConstruct()` disqualifier) not because of a code bug, but
because that's a correct reflection of the game's real constraint.
**Before concluding a power-connection test result is a code problem,
check whether the save has this unlock active.**

For this project's demo specifically: **if daisy-chaining isn't
unlocked, the real power step needs a power pole placed between the
Smelter and Constructor** (wire pole→Smelter and pole→Constructor,
rather than one direct Smelter→Constructor wire) - `Recipe_PowerPoleMk1`
is already confirmed to exist as a real asset (§ above). Not yet
determined whether the current save has the unlock active; find out
empirically via the dry-run test before assuming either way.

## Plan

1. **Power first** (more tractable, direct API exists): small, isolated
   dry-run experiment - place two buildings with power connectors close
   together, retrieve their `UFGPowerConnectionComponent`s via generic
   `GetComponents<>()`, drive `AFGWireHologram` via `HotKeyRecipe(Recipe_PowerLine)`
   + `SetConnection(0/1, ...)`, check `CanConstruct()` before attempting
   real construction - mirroring the extractor placement dry-run
   methodology exactly.
2. **Belts second**, only after power is validated, given the higher
   unknown-risk - start even smaller: confirm a single `TrySnapToActor`
   call can fix a start point (`SHBS_FindStart`) before attempting the
   full multi-step sequence.
3. **RPC exposure**: `world.placeBuilding` ({"recipeClass","x","y"})
   already added (commit pending as of this doc) as a genuinely
   asynchronous RPC method - `world.connectPower`/`world.connectConveyor`
   should follow the same async-callback pattern once their underlying
   mechanisms are validated via console-command dry runs first.
4. **Verify production**: once wired and powered, poll
   `world.manufacturers` for `productionStatus`/`productionProgress` on
   the Smelter/Constructor to confirm actual Iron Ingot/Iron Plate
   production, not just successful placement/connection.

## Update 2026-08-25 (later the same night): pole connections resolved, full chain producing

All four plan steps above are now DONE, live-verified. Key findings
that updated the picture above:

- **The `SetConnection(0/1, ...)` mechanism from §"Power connections"
  was correct all along** for every connection type tested (machine↔machine,
  pole↔pole, pole↔machine). Two attempts to replace it with a
  click-based `TrySnapToActor()` flow (reasoning that pole connections
  needed the belt-style multi-step drive) were both live-tested,
  found to regress the previously-working machine↔machine case, and
  reverted. Do not revisit that theory without new evidence.
- **The real cause of intermittent pole-connection failures was
  disqualifier flakiness tied to live player/camera state**, not a
  geometry or mechanism bug: identical dry-run calls against the same
  buildable pair, zero code changes between calls, returned three
  different disqualifiers across attempts - `UFGCDWireSnap`,
  `UFGCDWireTooLong` (even when real distance was well under
  `maxLength`), and `UFGCDInvalidAimLocation`. Same class of issue as
  the building-placement aim-location flakiness documented in
  `docs/buildgun-driven-placement-research.md`.
- **Fix**: `ConstructPowerConnection` gained `bIgnoreAimLocation`/
  `bIgnoreWireSnap` params (`world.testPowerConnection`/
  `world.connectPower`: `"ignoreAimLocation"`/`"ignoreWireSnap"`, both
  optional, default `false`), mirroring `ConstructBuildingAtPosition`'s
  proven named-disqualifier-bypass pattern instead of trusting the
  opaque `CanConstruct()` bool. Confirmed live, repeatedly, across all
  connection-type combinations. `UFGCDWireTooLong` was deliberately
  left non-ignorable - it's presumed to be the real, deterministic
  `maxLength` check (confirmed live as 10000 real units via
  `world.powerLineLimits`, not the 2000 placeholder used in early
  Python test fixtures).
- **Wire length cap (`world.powerLineLimits`, live 2026-09-10):**
  `maxLength: 10000` (pole↔pole), `maxPowerTowerLength: 30000` (both
  ends power towers), `lengthPerCost: 2500`. This is the source of the
  earlier "no-length-limit power lines" note - it was the 30000 tower
  cap, 3× the pole cap, not truly unlimited. Confirms the historical
  live results: 7392u wired (< 10000), 10504u failed pole↔pole
  (> 10000), and the HMF 20940/26894 runs worked because they used
  power **towers** (< 30000). Remote mining/smelting outposts are
  ~1,000,000u (10km) from the main grid - past even a chain of towers
  without hand-placing dozens.
- **`bIgnoreWireLength` (added 2026-09-10, `world.connectPower`/
  `world.testPowerConnection` param `"ignoreWireLength"`, default
  `false`):** opts out of `UFGCDWireTooLong` in the same
  named-disqualifier-bypass poll loop. Rationale: the cap is a pure
  BUILD-TIME gate - `FGPowerConnectionComponent` merges the two power
  circuits logically, with no runtime dependency on wire length - so a
  wire built past the cap still carries power; only the visual spline
  stretches. Lets one wire span any distance instead of a pole chain.
  The Python `Executor.connect_power` now passes both `ignoreWireSnap`
  and `ignoreWireLength` `True` by default.
- **`ignoreWireSnap` (no-rebuild) confirmed 2026-09-10 to build a REAL
  circuit-joining wire**, not just clear the false-negative: a pad-pole
  pair's `world.powerPoles` free-connection count dropped 6→5 on both
  ends after the connect (delta test), proving an actual `FGBuildableWire`.
  The Python side had never been passing `ignoreWireSnap`, which is why
  "Must be hooked up to a connection!" kept recurring; now default-on.
- **`ignoreWireLength` live-verified 2026-09-10**: one wire from the iron
  outpost's pole to a powered grid pole **16,754cm away** (past the 10,000cm
  pole cap) flipped the outpost pole `hasPower False→True`, first attempt.
  Remote outposts can now be grid-powered with a single span. `world.powerPoles`
  `hasPower` is the verification signal; machines/generators are NOT poles and
  don't appear there - verify a wired machine indirectly (pole `hasPower` +
  the pole's free-connection count dropping when you wire the machine to it,
  visible in the `ConstructPowerConnection ... construction attempted` log).

## Belt delete-corpse on a connector blocks rebuild (2026-09-10)

Deleting a conveyor belt can leave the SOURCE machine's output connector in a
state where every subsequent `world.connectConveyor` from that connector
returns `CANNOT_CONSTRUCT: Initializing (hard)` for the full 120-tick poll
(`connectedCount=2, stepComplete=true` in the log, but the hologram never
leaves UFGCDInitializing). Diagnosis that pins it to the connector, not a
global wedge or the destination:
- a fresh container→container belt on the same deck got PAST Initializing to
  the real cost check (`Missing: Desc_IronPlate_C`), so the subsystem is fine;
- belts from the affected merger's output to BOTH a drone-station input AND a
  brand-new container both hung on Initializing → the SOURCE connector is the
  common factor;
- a far-teleport streaming unload/reload did NOT clear it.
Belts cost materials when `UnlimitedResources` is off - seed the player with
`world.addItemsToPlayerInventory` (Desc_IronPlate_C) before bulk belt work, or
the belt fails `Missing ingredients` (this is separate from the corpse issue).
Clean fix for the corpse: `world.saveGame` then reload the save (actor path-id
names are stable across load, so stored buildable ids survive). Lesson: do NOT
delete a working belt to "test" it - verify flow first (drone/station
inventories, downstream container via removeItemsFromInventory).
- **The daisy-chain-unlock caveat in §"pole-vs-daisy-chain" above did
  NOT block this session's direct machine↔machine test** - the save
  already had it unlocked (or the constraint doesn't apply the way
  originally guessed). Machine↔machine power connections worked
  directly once the aim-location flakiness was accounted for.
- **A pole can look part of a live grid (have an existing wire) but not
  actually be generator-connected** - live-diagnosed when the demo
  site's newly-chained pole stayed on `productionStatus: "Error"` after
  a geometrically-valid connection to a distant pre-existing pole. The
  user manually reconnected that pole to the real grid in-game to
  unblock the test. AIMod currently has NO telemetry to distinguish a
  "live" pole from an "orphaned" one - flagged as a future
  `world.powerCircuits`/per-connection `"circuitId"` addition, not yet
  built.
- **End state**: Miner→Smelter→Constructor chain fully built via RPC
  (`world.placeExtractor`/`world.placeBuilding`, `world.setRecipe`,
  `world.connectConveyor`, `world.connectPower`), real power from the
  grid, Smelter `productionStatus: "Producing"` at ~99.6% productivity,
  genuinely converting Iron Ore → Iron Ingot with the Constructor
  queued to follow. First time this project's full observe→plan→build→
  verify-production loop has closed end-to-end.
