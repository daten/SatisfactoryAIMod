# Research spike: making a pure-RPC rail joint drivable (2026-09-08)

## Problem
`constructRailroadTrack` builds track between two station connectors and
graph-merges it (both stations get the same `trackGraphId`), but a locomotive on
the result cannot path across the joint — `selfDrivingError = StationUnreachable`,
loco never moves. Verified definitively with the minimal case: two stations, one
straight connecting segment, loco on it, **both** stations powered → stuck. So
the graph merges but the joint is not a **drivable track-position edge**. Power
is ruled out (error is `StationUnreachable`, not `NoPower`).

The station-rotation fix (commit a9ec3451e0) is unrelated to this and *works* —
it only fixed connector orientation so curves can be aimed; it does not affect
joint traversability.

## What the source tells us (and doesn't)
The FactoryGame railroad classes ship as **auto-generated stubs** in this
workspace — real logic is in the game binary:
- `AFGRailroadTrackHologram::ConfigureComponents(){ }` (empty) — this is what
  wires the constructed track's real connections/track-position. Runs in the
  binary during construct regardless of whether we call `InternalConstructHologram`
  directly or go through `PrimaryFire → Server_ConstructHologram`.
- `SetHologramLocationAndRotation(){ }`, `DoMultiStepPlacement(){ }`,
  `TryFindAndSnapToOverlappingConnection(){ }`, `GetSnappedConnectionComponents(){}`,
  `AutoRouteSpline(){ }` — all stubs here, real in binary.
- `UFGBuildGunStateBuild::PrimaryFire_Implementation(){ }`,
  `InternalConstructHologram(){ }` — stubs here, real in binary, **callable via
  vtable** (they are UFUNCTION/virtual).

So we cannot *reproduce* the drivable-joint setup; we can only *drive the binary
functions* and observe. `ConfigureComponents` reads hologram state to wire the
joint, and the two construction paths differ only in the **hologram state** when
it runs.

### Hard constraints (from the hologram constructor — these ARE readable)
- `mMaxLength = 10000` — **a single track segment must be ≤ 10000u.** Our failing
  tests exceeded this (straight = 16000, quarter-arcs = 11044). Over-length track
  builds (we bypass `CanConstruct`) but is a strong suspect for "builds but not
  drivable."
- `mMinBendRadius = 3000` — curves tighter than 3000u radius are invalid ("turns
  too sharply"). The user's "wide turns" guidance matches this.
- `mSnapDistance = 500` — a hologram endpoint must be within 500u of a connector
  to snap to it.
- Drivable-joint state the binary `ConfigureComponents` consumes:
  `mConnectionComponents[2]` (the new track's own ends), `mSnappedConnectionComponents[2]`
  (the station connectors it snapped to), `mSnappedRailroadTrack` /
  `mSnappedRailroadTrackDistance` / `mFlipSnappedDirection`, and
  `mSnappedStartLocation` / `mSnappedStartTangent` (START-side snap only).

## Current approach (what `ConstructRailroadTrack` does now)
Manually: `SetHologramLocationAndRotation(StartHit)` → `UpdateHologramPlacement`
→ `TrySnapToActor` → `DoMultiStepPlacement(true)` for the START, repeat for the
END, then `InternalConstructHologram`. Post-construct it also does
`RemoveTrack`→`AddConnection`(force-link)→`AddTrack` for the graph merge. Result:
graph-merged, `GetSnappedConnectionComponents` reports both ends snapped, but the
joint is not drivable.

## Prioritized experiments (each needs an editor build + Alpakit redeploy to test)

1. **Respect `mMaxLength` (≤10000) and `mMinBendRadius` (≥3000).** ❌ FALSIFIED
   2026-09-08 (no redeploy needed): built a clean **4400u** straight segment
   (well within the 10000 max), loco on it, BOTH stations powered → still
   `StationUnreachable`, zero movement. So segment length is NOT the cause; the
   joint is non-traversable regardless of length. (Still respect ≤10000 / ≥3000
   for validity, but it does not make the joint drivable.) → go to experiment 2.

2. **Drive the real fire path instead of manual `DoMultiStepPlacement` +
   `InternalConstructHologram`.** ❌ FALSIFIED 2026-09-08 (both forms, param
   `usePrimaryFire`, commits baef6c238d + 0025f0dc2c, live-tested). Form (a):
   `BuildState->PrimaryFire_Implementation()` twice (hit at source, then dest) →
   build step stayed 0 (FindStart), no track. Form (b): the build gun's
   `Gun->OnPrimaryFirePressed()` + `OnPrimaryFireReleased()` per click → ALSO
   left step at 0, no track. So calling the fire entry points headlessly does not
   advance the hologram: the interactive path depends on context we don't
   reproduce — input-device state, hold-time accumulated in the build gun's
   `Tick` (`mPrimaryFireHoldTime`, `mBuildModeSelectHoldTime`), and cross-tick
   `mWaitingForPrimaryFireRelease` transitions. NB the MANUAL path's
   `DoMultiStepPlacement(true/false)` DOES advance the step (that is why track
   builds today) — it just yields a non-drivable joint. Next ideas if pursuing
   this lever: (i) let several real ticks elapse between press and release so the
   gun's Tick accumulates hold-time; (ii) call `Gun->Input_PrimaryFire(...)` with
   a synthesized `FInputActionValue` (the actual input entry) rather than the
   On*Pressed/Released helpers; (iii) find where the binary gates the fire on
   input state and satisfy it. All redeploy-gated and uncertain.

3. **`SetLocationAndRotationFromPlatformConnections`** — the station's integrated
   platform track is always drivable; this binary function positions a track from
   two platform connections. Could be repurposed to build drivable connecting
   track between platform-adjacent points.

4. Last resort: request decompiled `FGBuildableRailroadTrack` /
   `FGRailroadTrackHologram` `ConfigureComponents` to reproduce the exact wiring.

## Status / recommendation
Experiments 1 and 2 are both **falsified** live. Length isn't the cause, and the
headless fire-path (both `PrimaryFire_Implementation` and gun `On*Pressed/Released`)
does not advance the hologram build step at all, so it can't construct — the
interactive input/tick context is the missing piece and reproducing it is
uncertain and redeploy-gated. The `usePrimaryFire` param and its scaffolding are
committed (default off; the proven manual straight-build path is unchanged and
still works), so a future session can iterate on forms (i)–(iii) above without
rebuilding the plumbing.

**Recommended: pause the drivable-joint goal here.** Banked wins remain solid —
the station-rotation fix (geometry/curves), power, cleanup, and full research
trail. The one unsolved piece is the non-traversable joint, which needs either
the input-simulation iterations above or experiment 3
(`SetLocationAndRotationFromPlatformConnections` — build drivable track the way
the station's own always-drivable platform track is built) or experiment 4
(decompiled `ConfigureComponents`). Until then, a human in-game connection to
RPC-built track repairs the joints and the train runs.
