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

2. **Drive the real `PrimaryFire_Implementation()` instead of manual
   `DoMultiStepPlacement` + `InternalConstructHologram`.** Sequence: set the build
   gun's hit to the SOURCE connector → `BuildState->PrimaryFire_Implementation()`
   (advances/places start) → set hit to DEST connector →
   `PrimaryFire_Implementation()` again (final step constructs via the binary's
   own `Server_ConstructHologram`). This makes the ENGINE own the full placement
   + connection setup, so `ConfigureComponents` sees exactly the state the
   interactive player path produces (which IS drivable). Risks: PrimaryFire may
   read its own aim trace rather than `GetHitResult()`; single-player server- RPC
   nuance (should be fine — SP is authority). Prototype lives behind a param so
   it doesn't regress the working straight-build.

3. **`SetLocationAndRotationFromPlatformConnections`** — the station's integrated
   platform track is always drivable; this binary function positions a track from
   two platform connections. Could be repurposed to build drivable connecting
   track between platform-adjacent points.

4. Last resort: request decompiled `FGBuildableRailroadTrack` /
   `FGRailroadTrackHologram` `ConfigureComponents` to reproduce the exact wiring.

## Recommendation
Experiment 1 is falsified — length isn't it. **Next: prototype experiment 2**
(drive `PrimaryFire_Implementation()` with the hit set to source then dest
connector, instead of manual `DoMultiStepPlacement` + `InternalConstructHologram`),
behind a param so it can't regress the working straight-build, then redeploy and
retest drivability. Treat this as an empirical redeploy-iterate loop; the binary
opacity means we cannot predict which lever works without testing. If experiment
2 also fails, experiment 3 (`SetLocationAndRotationFromPlatformConnections`) or 4
(decompiled `ConfigureComponents`) remain.
