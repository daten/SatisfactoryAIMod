# Factory placement guide — foundations, machines, splitters/mergers, belts, lifts (distilled 2026-09-07)

Agent-facing quick reference for **predictable factory placement** through the
AIMod RPC. These specific issues (rotation, z-height, belt connections,
splitter/merger topology, lift height) cost days of trial-and-error; this doc
distils the hard-won rules so any agent gets them right the first time.

- **Fundamentals & full incident history:** `docs/placement-lessons.md` (the
  authoritative changelog — section names cited below).
- **Executable tooling (USE THIS, don't hand-roll loops):**
  `controller/satisfactory_ai/` — `composites` (machine_row, manifold,
  vertical_pair_block, elevated_crossing, pole_backbone), `router`
  (direct_belt_feasible, route_connection), `connector_db` (yaw + pinned
  connector geometry), `conveyors`, `splitters`, `layout` (foundation rings/
  fill, shared support columns), `power`, `executor` (execute_and_verify
  auto-repairs dangling belts).
- **Vehicles** (train/truck/drone) have their own guide:
  `docs/vehicle-placement-guide.md`.

Golden rule throughout: **never trust `success: true` alone** — verify with
`world.connections` (or let `executor.execute_and_verify` check + repair).

---

## 0. Cross-cutting placement rules

- **Always pass an explicit `yaw`** to `placeBuilding`. `rotationScrollDelta` is
  an unreliable no-real-tick loop — `yaw` is an absolute `SetActorRotation`,
  reasserted every poll tick (placement-lessons §Rotation).
- **`gridSnapSize: 0`** for precise geometry — the default 100 silently rounds
  positions and breaks alignment (§gridSnapSize).
- **`ignoreGroundTrace: true` + explicit `z`** = reliable literal placement.
  `world.groundHeight` can hit the mod's OWN already-placed buildables, and a
  ground trace can miss tile-edge geometry or open interior space (§groundHeight,
  §Multi-story). 
- **A buildable's reported `position` is its PIVOT, not its top surface.** Read
  `world.buildables[].bounds` before stacking. Foundation TYPES differ wildly in
  thickness — never assume a pivot-to-top offset (§foundation thickness).
- **Teleport the player NEAR the work** for connect/place calls — belt/pipe/
  power validation is camera/player-distance dependent; belts fail "too long"
  beyond ~5600u from the camera even when geometrically fine.
- **Delete only IDs you tracked** — never bounding-box sweeps.

---

## 1. Foundations

- Default to **8×1 (1m) foundations** (`Recipe_Foundation_8x1_01_C`); 4m slabs
  are space-inefficient — use only when justified.
- For a floating/raised platform, do a **generous `world.terrainHeightGrid`
  survey BEFORE picking a height** — rebuilding at the wrong height is expensive
  (§floating platforms). One `terrainHeightGrid` call replaces hundreds of
  `groundHeight` probes (params: minX/minY/maxX/maxY/stepSize).
- **Extend the deck to cover a belt's FULL path**, including short approach
  segments — a belt crossing from a supported tile to open air next to it trips
  "Surface is too uneven!" even on a flat run (§Belts).
- Circular/ring platforms: use `layout.compute_outer_touching_ring` /
  `compute_disk_fill_grid` ("N foundations wide" is ambiguous — these resolve it).
- **Lightweight/instanced** buildables (foundations) reject
  `setBuildableColor`/`setBuildableRotation` (their id is a `class|index`
  handle) — color/rotate works on machines, not foundations.

## 2. Machines

- **Row layout = side-by-side along the LONG axis, not end-to-end** — a common
  early mistake that produces overlapping/backwards rows (§machine row,
  user-caught). Use `composites.machine_row` — it places the row with correct
  yaw and pinned connectors in one planned unit.
- Recipe gotchas: **`Recipe_SmelterMk1` builds a FOUNDRY**, not a smelter
  (smelter = `Recipe_SmelterBasicMk1`); ingot recipe = `Recipe_IngotIron`.
  Verify recipe→building via `world.buildableCatalog`/`recipeCatalog`, don't
  guess from the name.
- Overclock: `setClockSpeed` range is dynamic — [1,100] with no shards; install
  power shards (`installPowerShard`) first to raise the cap (200% after 2).

## 3. Splitters & Mergers — fixed connector topology

- Both have a **rigid 4-connector fan**: one main pair 180° apart (splitter:
  input ↔ straight-through output; merger: main input ↔ output) + two side
  connectors ±90°. **Rotation spins the whole fan as a unit — you cannot
  independently aim one connector.** Pinning the output's direction FORCES the
  main input opposite; only the two side connectors remain free (§Splitter fixed
  topology). **Plan the whole fan before choosing yaw.**
- **Compute the yaw:** probe at `yaw=0`, note the 4 normals, pick the one
  connector you must pin, `delta = target_angle − current_angle`, place with
  that yaw — or just use `connector_db.yaw_for_connector_facing` /
  `composites.manifold`, which do this for you.
- **Pin the connector positions** (`sourceConnectorPosition`/
  `destConnectorPosition` on `connectConveyor`; `connectorPositionA/B` on
  `connectPower`). Do NOT rely on `FindFreeFactoryConnection` order — it's
  opaque, not geometry-aware, and different for inputs vs outputs. `connector_db`
  + the composites pin deterministically (this supersedes the old
  "empirical call-order" workaround).
- **Match connectors to buildables by DIRECTION, not call order** — feed the
  east-facing output to the east-most machine, etc., or you get belts that cross
  and double back.
- There is **no rotate-in-place** — `yaw` only applies at placement time. Get it
  right when you place; a wrong orientation means teardown + rebuild.

## 4. Belts

- A destination connector must be **approached from its +normal side, entering
  in the −normal direction**. A source whose exit ≈ the destination's normal (a
  ~180° reversal) generally won't route in one call.
- **Try `"Default"` → `"Curve"` → `"Straight"`** — they are not interchangeable;
  a Curve failure often succeeds as Straight and vice versa. Treat a single
  failure on a reversal-shaped belt as possibly transient and retry once.
- Real limits are real: "too long!"/"too steep!" are genuine — split a long/steep
  run across an intermediate buildable or a lift. `router.direct_belt_feasible`
  tells you at PLAN time whether a run is in-envelope (shape rules bind
  <1500u; length/player-distance beyond that).
- **`connectConveyor` can report `success: true` while leaving the destination
  end unattached** (§connectConveyor unattached). ALWAYS verify via
  `world.connections`; `executor.execute_and_verify` auto-repairs dangling belt
  ends. Persistent single-connector failure → route into a **spare input slot**
  as a workaround.
- **Conveyor SUPPORTS are cheap belt relays** — cheaper than splitters for a
  straight relay (§Conveyor supports). Conveyor WALLS are valid belt/lift targets
  too (their connector is `SNAP_ONLY`; note `IsConnected()` always reads false on
  those — don't judge free/occupied from `world.connections.connected` there).
- Don't route a belt over unsupported open air beside a platform — extend the
  foundation (see §1).

## 5. Vertical conveyor lifts

- A lift is a **straight vertical column** — same horizontal facing at both ends,
  locked to wherever its bottom snapped to the source. It **cannot** bridge a
  destination offset in X/Y *and* Z in one call.
- **Correct pattern:** place the destination **directly above** the lift's snap
  point (same X/Y, differ only in Z); if the top lands slightly short (tens–~100u),
  bridge the residual with a short `connectConveyor` belt.
- **Arbitrary rise works** as of the 2026-09-01 fix (synthetic hits now populate
  `FHitResult::TraceStart/TraceEnd` — the earlier "~400u default rise" was a real
  regression, now resolved). If you ever see a stuck-low or downward lift again,
  that fix has regressed — check the `ConstructConveyorLift` doc comment.
- Lift → splitter: put the splitter **to the SIDE** of the lift's locked axis (a
  lift fed from the south is input/output-south, so splitter east/west), at the
  lift's real output Z (read `world.connections`), then lift-up + a separate belt
  hop. Use `composites.vertical_pair_block` for the paired lift idiom.

## 6. Orientation planning

Plan orientation **before** placing, from the flow you want — not as a byproduct
of the connect calls. You can't rotate in place, and splitter/merger fans + lift
axes are locked once placed. `composites` compute correct yaw and pinned
connectors up front so the belts are short and straight the first time.

---

## 7. Which tool to reach for

| Task | Use |
|---|---|
| A row of machines wired to a manifold | `composites.machine_row` + `composites.manifold` |
| Is this belt run buildable? | `router.direct_belt_feasible` (plan-time) |
| Correct yaw / pinned connector position | `connector_db.predict` / `find_connector` / `yaw_for_connector_facing` |
| Cheapest belt/lift tier for a rate | `conveyors.select_cheapest_sufficient_tier` |
| Understand a placed splitter's ports | `splitters.classify_ports` / `get_splitter_output_facing` |
| Foundation ring / disk fill / shared supports | `layout.compute_outer_touching_ring` / `compute_disk_fill_grid` / `plan_shared_support_columns` |
| Power line feasibility | `power.is_direct_connection_feasible` |
| Build + auto-verify + repair dangling belts | `executor.execute_and_verify` |

Composites return a plan **plus a `verify_spec`**; check it with one
`world.connections` query (`composites.verify_connections`) — build idioms as
single planned units, one call per section, not one call per component.

---

## 8. Where this knowledge lives

- **Mechanisms + rationale:** `AIModFunctionLibrary.cpp` (dated "why" comments).
- **Executable strategy:** `controller/satisfactory_ai/` composites/router/
  connector_db/etc.
- **Distilled operational rules:** this guide + `docs/vehicle-placement-guide.md`.
- **Full incident history / fundamentals:** `docs/placement-lessons.md`.
Keep new live lessons landing in all three layers, not just Claude memory.
