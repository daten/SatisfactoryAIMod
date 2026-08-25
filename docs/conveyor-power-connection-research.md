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
open-connection wiring — **not usable from DocMod's module** (friend
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
`DebugCheckPowerConnection`, `DocModFunctionLibrary.cpp`) looks for a
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
