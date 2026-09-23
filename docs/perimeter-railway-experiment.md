# Perimeter railway stress test — findings log (2026-09-23)

Large-scale exploratory test: build a continuous railway loop around the outer
perimeter of the playable map, near the terrain, over valid ground, handling
real elevation change. Run entirely against the **already-running** mod build
(no rebuild permitted mid-test). Trial-and-error is expected; document bugs and
work around them with existing RPC capabilities.

Player-teleport rule for this run (user-mandated): only ever land the player on
a **foundation perch placed well clear above local terrain** — never drop them a
significant height and never over the void. (See
[[reference_floating_belt_structures]] platform-perch method.) In practice
`constructRailroadTrack` only needs the player *alive* (not near), so we keep the
player parked safely and avoid teleporting at all where possible.

## Map geometry (surveyed via world.probeHazard)

Playable interior (`insideWorldBounds2D` && !`insideDamageVolume`) is an
irregular blob ringed by 15 damage-over-time volumes:

- X ≈ **[-300k, +400k]**, Y ≈ **[-350k, +300k]** (cm), widest through the middle
  latitudes; corners are clipped by the D-volume ring.
- killZ = -1,048,575. `probeHazard(x,y,z)` returns `insideDamageVolume`,
  `insideWorldBounds2D`, `belowKillZ`, and nearest-volume distance — the
  authoritative "is this point safe/over land" test for route planning.

## ⚠️ MAJOR FINDING: terrain scan can't see natural landscape

`world.groundHeight` / `world.terrainHeightGrid` use `FindGroundAtXY`, a
**±1000-unit `ECC_Visibility` line trace** around a reference z
(`AIModFunctionLibraryInternal.h`). In this build that channel is **only blocked
by placed buildables, not the Satisfactory landscape** — so both methods return
`found:false` everywhere except directly over existing buildings/foundations.
`world.placeBuilding`'s own ground trace has the same blind spot (a trace-place
with no explicit z can't find the ground either; with an explicit z it just
places at that z).

**Consequence:** the running build effectively **cannot scan natural terrain
height**. (Candidate fix for a later branch: trace `ECC_WorldStatic` /
`ECC_GameTraceChannel` for the landscape, or a multi-channel trace, and/or widen
the trace window. Not applicable to this test — no rebuild.)

**Workaround used here:** build a terrain-elevation model from objects that DO
sit on the real ground — `world.resourceNodes` (640 nodes, spread map-wide) plus
the user's base buildables at low z (~22k points). Interpolate (nearest / IDW)
to estimate terrain height along the route, floating each foundation pad at
`terrain_est + offset` via `ignoreGroundTrace`. Rail stays near the ground where
samples exist; where samples are sparse we hold the previous elevation (gentle
grade) — a documented limitation, not a sky-rail-by-choice.

Sampled elevations span roughly **-95 m to +465 m** across the interior — plenty
of variation to exercise grades/ramps.

## Rail primitive (from vehicle-placement-guide §3 + source)

- `constructRailroadTrack(sourceBuildableId, destBuildableId?, recipeClass,
  sourceConnectorPosition?, destConnectorPosition?)`. Empty `destBuildableId` =
  **free-end build** to `destConnectorPosition` (must land over a foundation).
  Track-to-track chaining: next segment's `sourceBuildableId` = the prior
  track's `Build_RailroadTrack_C` id, pinned via `sourceConnectorPosition`.
- Requires the player pawn alive (NO_PLAYER otherwise); does NOT require player
  proximity (rail curves are the opposite of belts — build with player far off).
- Curve limit: adjacent anchors ≥ ~6000u or "turns too sharply"; a single call
  can't exceed ~90°.
- ⚠️ Drivability risk: a source comment (train-drivable-joint-research.md) warns
  the manual build path can leave a joint non-traversable (loco
  `StationUnreachable`). Guide §3 says the 2026-09-18 `train-t2t-loop` DID
  circulate across plain-track joints. **Must re-validate drivability of chained
  free-end joints at small scale before the marathon (Phase A).**

## Phase log

### Phase A — primitive validation & calibration (2026-09-23) ✅

Built a throwaway 4-station CW quarter-arc loop (center (150000,100000), R=8000,
z=50000 sky test) to validate the core mechanic in the RUNNING build:

- **Arcs between real connectors build at ~7000–8000u chords** (R=8000). Straights
  (aligned tangents) fail `too long`/`too steep` beyond ~2000u — so the loop must
  be built from **arcs**, not straight segments. (The `too long` fires on
  aligned-tangent geometry regardless of true length; a curved multi-segment arc
  of ~9000u arc-length builds fine.)
- **Station settle time:** the FIRST arc off a freshly-placed station can fail
  `too long` if fired < ~5s after placement (connectors still initializing).
  Wait ~5–6s and/or retry — a retry after the others were built succeeded.
- **Connector pinning:** get each station's `RailroadTrackIntegrated` child, read
  its two spline endpoints (`world.splineGeometry`), pin source/dest at the
  endpoints nearest each other. Integ is centered ON the station, spanning ±800
  along the station's local X (yaw φ → connectors at center ± 800·(cosφ,sinφ)).
- **Closure:** all 4 stations ended on ONE `trackGraphId` (world.trainStations
  key is `stations`, each has `trackGraphId`).
- **Power:** `connectPower(stationId, poweredPoleId, ignoreAimLocation,
  ignoreWireSnap, ignoreWireLength)`. 770 base poles have a free slot (parse
  `connections[].numFreeConnections`); pick the nearest, wire length is ignored.
  First symptom of no power is `selfDrivingError: NoPower` (NOT a joint problem).
- **✅ DRIVABLE:** loco spawned via `constructVehicle(Locomotive, …, ignoreGroundTrace)`;
  the `BP_Train_C` (id ≈ loco−3, `hasTimeTable:false`) got a timetable
  (`stationBuildableId` stops) + `setTrainSelfDriving`. After power:
  `selfDrivingError: NoError` and the loco **laps the loop CW** (position angle
  sweeps through all quadrants). Pure-RPC arc joints ARE traversable in this build.

**Consequence for the perimeter:** model the loop as a **rounded polygon of
arcs**. Open question under test: the minimum turn angle a long (~7000u) arc
needs — determines whether near-straight edges can be gentle arcs (~30–50 arcs
total) or must be many short segments.

### ⚠️ MAJOR FINDING: RPC rail can only build TIGHT arcs

Extensive sweeps against the running build (dry-run AND real construct):

- **Straight / colinear track cannot be built at all** via `constructRailroadTrack`
  — every straight segment (free-end OR connector-to-connector, 1600u to 7000u,
  with retries) fails `Railroad Track is too long! (hard)`. (Curiously the
  **dry-run `testRailroadTrack` PASSES** several of these — a dry/real
  discrepancy; do not trust a straight dry-run.)
- **Only sufficiently-CURVED arcs build.** At a 7000u chord: δ=90° (R≈4950) builds;
  δ=20° (R≈20000), 10°, 5°, 3° all fail `too long` even with 8 retries. So there
  is a **maximum radius** (~8–15k units) beyond which an arc degenerates to
  "straight" and is refused. Curved track subdivides into mesh pieces and spans
  long arc-length (Phase A's R=8000 90° arc = ~12500u built fine); straight track
  does NOT subdivide (hard mesh cap ~2000u) and so never builds.
- **First-arc settle flake:** the first `constructRailroadTrack` off freshly-placed
  stations often fails `too long`/`NO_RAILROAD_CONNECTION`; retry ~6–8× with a
  short delay (connectors finish initializing).

**Implication:** a large-radius perimeter loop that hugs the map edge is **NOT
constructible** with this build — you cannot make gentle curves or straights, only
tight ones (≲150 m radius). A large circular loop is therefore capped at that
radius. The only way to traverse long perimeter *distance* is a **serpentine of
alternating tight arcs** (net-straight average path, every arc within the buildable
curvature window) — wiggly but drivable. (Candidate fix for a later branch: the
straight/gentle build path — the manual `DoMultiStepPlacement` likely needs the
engine's segmented build or a higher mesh cap.)

The base already runs its own rail network (trackGraphId 1, a live self-driving
train "Nuclear Plants"/"Factory Main") — the perimeter build stays well clear of it.

### Phase B — serpentine corridor (the distance workaround)

Because only tight arcs build, a long corridor is a **serpentine of alternating
sharp free-end arcs** (net-straight average heading; every arc within the
buildable curvature window):

- **Primitive:** free-end arc, chord ~5000u, offset **±50°** from the travel
  heading, alternating each arc. `constructRailroadTrack` (free-end) returns NO
  `resultBuildableId`, so the new `Build_RailroadTrack_C` is found by a
  **buildables-diff**; its far endpoint (from the source pin) is the next source.
- **Free-end needs a sharp offset (≥~45–50°)** — a gentle offset (≤40°) fails
  `too long`. Consequence: the path slaloms ±50° around the travel axis (wide
  wiggle), advancing ~**3280u north per arc** (chord·cos50). Reliable: a 12-arc
  test built **12/12, zero failures, drift x = 0** (the ±50° offsets cancel, so
  the corridor stays centered on the travel axis).
- **Elevation:** pads float at the terrain **estimate** (IDW point cloud) with a
  per-arc **grade clamp** (≤6%), so the rail follows the ground gently. Edge
  estimates are rough (sparse samples) — documented limitation, not a sky rail.
- **Safety:** every landing is `probeHazard`-checked (inside bounds, not in a
  damage volume, above killZ) before placing.

Builder: `controller/tools/experiments/perimeter_rail.py` (from scratch
`corridor.py`).

### Phase C — drivable loop + ⚠️ drivable-joint reliability

Self-driving needs a **closed loop** — a linear corridor (even 2 stations, one
graph, terminate arc built) reports `selfDrivingError: StationUnreachable` (the
self-driver won't path a dead end). Confirmed live.

Closed loops built (conn-conn arcs, terrain-estimate elevation, center
(250000,-30000)):

- **4-station square, 4×90° arcs (R=8000): DRIVES** — one graph, powered (tap a
  base pole, `ignoreWireLength`), loco spawned on an arc track (spawning on the
  short station integ fails "Not enough space on track"), timetable all 4 stops,
  `setTrainSelfDriving` → **`selfDrivingError: NoError`, loco laps** (position
  angle advances continuously). This is the reliable drivable primitive.
- **8-station octagon, 8×45° arcs (R=9000): does NOT drive** — closed (one graph)
  and all 8 arcs built, but `StationUnreachable`. ⚠️ **Drivable-joint reliability
  drops with more/sharper joints:** the manual build path graph-merges every arc
  but does not guarantee a *traversable* joint at each one (a force-link vs a true
  connector snap); 4×90° happened to snap all, 8×45° left at least one
  non-traversable. The `usePrimaryFire` path meant to fix this is **non-functional
  in this build** (`PRIMARYFIRE_NO_TRACK`). So a *large* drivable loop (many
  joints) is unreliable — another hard limit for a full perimeter loop.

**Delivered drivable artifact:** the 4-station square loop at terrain elevation,
map-marked (N/S/centre), saved as `perimeter-railway-2026-09-23`. Small (~160 m)
— the largest *reliably drivable* pure-RPC loop, given the tight-arc + joint
limits.

## Verdict on the objective

A continuous **map-perimeter railway loop near terrain is not achievable** in the
running build, blocked by three independent hard limits, each documented above:
1. **No terrain scan** — can't read natural landscape height (elevation is only
   an estimate from a ground-truth point cloud).
2. **No straight / gentle-curve track** — only tight arcs (R ≲ 9k) build; a
   large-radius perimeter is impossible; long distance needs a wiggly serpentine.
3. **Drivable-joint reliability** — large loops (many joints) report
   StationUnreachable; only small loops drive reliably.

What IS delivered: a full map survey, a terrain-estimate model, a **drivable**
4-station loop over terrain, and a long **terrain-following serpentine corridor**
(below) demonstrating large-scale placement, curvature, elevation change,
continuity, and failure-recovery — the stress-test's learning objectives.

### Delivered serpentine corridor (large-scale placement)

Built at x=-180000, from y=-250000 heading north, terrain-estimate elevation
(IDW + 3 m clearance, ≤6% grade clamp):

- **100 arcs, ZERO failures**, one continuous chain.
- **3.22 km** of continuous rail (net-straight, drift x = 0 — the ±50° serpentine
  offsets cancel exactly), crossing varied terrain **including near the base**
  with no collisions.
- **83 m of elevation change** (z 524 → 8918) handled as gentle grade-clamped
  climbs/descents — the terrain-elevation-change objective.
- S and N stations at the two ends (both map-marked); one track graph. Being a
  linear corridor it does not self-drive (dead-end rule above) — its purpose is
  the placement/terrain/continuity/recovery stress test, which it passes.

Saved as `perimeter-railway-2026-09-23` (drivable loop + corridor + markers).
Tooling: `controller/tools/experiments/perimeter_rail.py`.
