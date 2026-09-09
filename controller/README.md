# satisfactory_ai controller (skeleton)

PLAN.md Phase 8. The first platform-neutral external component. Currently
contains:

- `satisfactory_ai/models.py` — `Position`, `Rotation`, `ResourceNode`,
  `Buildable`, `FactoryConnection` dataclasses mirroring AIMod's
  `FAIMod*Telemetry` structs
  (`Mods/GameFeatures/AIMod/Source/AIMod/Public/AIModTelemetryTypes.h`).
- `satisfactory_ai/protocol.py` — parses the JSON shapes documented in
  [docs/telemetry-protocol.md](../docs/telemetry-protocol.md)
  (resourceNodes / buildables / connections) into those models, rejecting
  unsupported protocol versions.
- `satisfactory_ai/graph.py` — PLAN.md Phase 11: `build_world_graph()`
  turns `Buildable` + `FactoryConnection` facts into a directed
  `WorldGraph` (one edge per connected `"Output"` connection point). Per
  PLAN.md, graph construction/analysis belongs on the controller, not the
  mod — the mod only exposes the raw per-connection-point facts.
- `satisfactory_ai/layout.py` — a placement-geometry TOOLKIT (added
  2026-08-25), not a layout solver: `learn_connector_profile()` derives
  a connector's yaw-independent local position/normal from a live
  instance, `predict_connector_world_state()`/`compute_aligned_placement_position()`
  answer "where would this connector land at position P, yaw Y" /
  "what position aligns two connectors", and `candidate_yaws_for_normal()`
  answers "what orientation would face this connector a given
  direction". Each function answers one geometry question for an agent
  to compose and iterate over while planning/optimizing a layout — it
  never chooses a layout, searches for the best position, or calls any
  RPC itself. Tested against real geometry captured live from a
  Smelter/Constructor belt-routing session, not synthetic numbers — see
  `tests/test_layout.py`.
- `satisfactory_ai/conveyors.py` — same toolkit posture, for belt tier
  selection and multi-segment routing: `select_cheapest_sufficient_tier()`
  picks the cheapest of an already-queried `ConveyorBeltTier` list
  meeting a minimum speed; `is_straight_segment_feasible()` checks a
  candidate segment's distance/incline against a tier's real limits;
  `compute_waypoint_positions()` (re-exported from `layout.py`, see
  below) computes candidate anchor points for chaining segments (via
  intermediate `Recipe_ConveyorPole` placements) when a route is too
  long or steep for one segment. None of these fetch telemetry, call
  any RPC, or choose a route on their own - the caller decides
  thresholds and validates the result live. See `ConveyorBeltTier`'s
  doc comment (`models.py`) for why `speed` is FactoryGame's own raw
  queried value, not an assumed items-per-minute figure.
- `satisfactory_ai/power.py` — the same pattern for power lines,
  motivated by the user asking whether power (like conveyors) needs
  distance-limit/pole handling: `is_direct_connection_feasible()`
  checks a candidate connection's distance against `PowerLineLimits.max_length`
  (a documented-`"[cm]"`-unit value, unlike belt `speed` - directly
  comparable to a computed 3D distance, no conversion caveat). Chaining
  through a real power pole (`Recipe_PowerPoleMk1`/`Mk2`/`Mk3`,
  confirmed on disk) should already work with the existing
  `world.connectPower` (its source/dest were already generic, not
  machine-only) - not yet live-tested.
- `satisfactory_ai/layout.py`'s `compute_waypoint_positions()` (moved
  here 2026-08-25 from `conveyors.py` since it's generic geometry, not
  belt-specific) computes evenly-spaced anchor points between two
  positions respecting a max segment length - shared by
  `conveyors.py`, `power.py`, and now `pipes.py`'s chaining patterns.
- `satisfactory_ai/pipes.py` - pipe groundwork (added 2026-08-25,
  NOT YET LIVE-TESTED), same toolkit posture as `conveyors.py`/
  `power.py`: `select_cheapest_sufficient_tier()` picks the cheapest
  `PipelineTier` meeting a minimum `flow_limit` (a documented-`[m^3/s]`
  value, unlike belt `speed` - no unit caveat);
  `is_straight_segment_feasible()` checks a candidate segment's
  distance against `max_spline_length` only (no incline check - unlike
  belts, no incline-vs-limit relationship for pipes has been confirmed
  from source). Unlike belts (`Recipe_ConveyorPole`) and power
  (`Recipe_PowerPoleMk1/Mk2/Mk3`), **no standalone
  `Recipe_PipelineSupport`/pole recipe was found on disk** - whether
  the same place-a-pole-then-chain-connect pattern transfers to pipes,
  or whether pipe poles are only auto-spawned internally by
  `AFGPipelineHologram`'s own `mDefaultPipelineSupportRecipe`
  mechanism, is an open question for the first live pipe test to
  answer - see `pipes.py`'s module docstring.

- `satisfactory_ai/models.py`'s `ConveyorAttachmentInfo` /
  `satisfactory_ai/protocol.py`'s `parse_conveyor_attachment_catalog_telemetry`
  (added 2026-08-25, splitter/merger groundwork, NOT YET LIVE-TESTED) -
  mirrors `world.conveyorAttachments`' real recipe catalog for
  Splitter/Smart Splitter/Programmable Splitter/Merger/Priority Merger,
  each with real `input_count`/`output_count`/`supports_sort_rules`. No
  dedicated toolkit module (unlike belts/power/pipes) - see
  `docs/conveyor-attachment-research.md`: splitters/mergers use the
  same simple hologram already proven for Miners/Smelters/Constructors,
  so placement (`world.placeBuilding`) and connection
  (`world.connectConveyor`) already work generically with no new
  construction code or chaining pattern to build a toolkit around.

- `satisfactory_ai/production.py` — deterministic production-rate /
  clock-speed balancing math for a single recipe and a two-stage
  (upstream row -> manifold -> downstream row) line. Explicit numbers in,
  computed answer out; no game queries.
- `satisfactory_ai/recipe_tree.py` — the recursive dependency / **bill-of-
  materials solver** (added 2026-09-09): given a target part + output
  rate, expands the whole recipe tree and returns, per intermediate,
  machine counts (fractional + whole-with-clock%), total RAW-resource
  rates, byproducts, and machine totals. Reads the offline
  `catalog_cache.json` (or explicit recipe lists) — works with the game
  closed. Deterministic per CLAUDE.md ("recipe dependency calculations...
  do not use an LLM"); it does NOT choose plans — a part with multiple
  non-alternate recipes raises `RecipeChoiceNeeded` (caller passes
  `recipe_choices=`), and the caller picks what to treat as raw
  (`raw_items=`); `/RawResources/` parts are leaves by default. CLI:
  `python -m satisfactory_ai.recipe_tree "Reinforced Iron Plate" 10`.
  Tested offline against a hand-verified fixture (`tests/test_recipe_tree.py`).
  Also carries per-machine + total **power (MW)** on the clock^log2(2.5) curve:
  fixed machines use `producingPowerConsumptionBase`; VARIABLE-power machines
  (Particle Accelerator / Quantum Encoder / Converter) are sized at the MAX of
  their recipe's range (const+factor) to avoid outages. `--max-clock 150/200/250`
  trades machines for overclocking and reports the **Power Shards** needed and
  the higher resulting draw. **Somersloops** (non-default): `--sloop "Item"` (or
  `Item:N`) production-amplifies that item's machines (output x up to 2x for the
  SAME input), so its machine count AND ingredient demand drop and cascade
  upstream — use on the most expensive items; the BOM reports total Somersloops
  and the amplification^2 power cost.
- `satisfactory_ai/siting.py` — resource-node siting toolkit (added
  2026-09-09): turns `world.resourceNodes` into siting facts — nodes grouped
  by resource, `extraction_rate(purity, miner_mk)`, spatial `cluster_nodes()`
  (candidate multi-resource sites with centroids), and
  `rank_sites_for_demand(nodes, demand, ...)` which composes with the BOM's
  `raw_totals` to rank clusters by whether their extraction meets the raw
  demand (with per-resource deficits). Exposes ranked options; the agent
  picks the site/miner tier. Solid nodes only. Offline-tested
  (`tests/test_siting.py`). End-to-end playbook:
  [docs/production-planning-workflow.md](../docs/production-planning-workflow.md).
- `live_check.py` — a network client, but a diagnostic one, not part of
  the "controller" proper: connects to a **running** AIMod `/rpc`
  server (`http://127.0.0.1:51902/rpc` by default) and checks every RPC
  method against real (or at least live) game state, printing PASS/FAIL
  per method and exiting non-zero on any failure. See "Live integration
  check" below.
- `export_catalog.py` — also a network client, not the controller proper:
  fetches `world.recipeCatalog`/`world.itemCatalog`/`world.buildableCatalog`
  (docs/telemetry-protocol.md, added 2026-08-27) and writes them to a
  local JSON snapshot (`catalog_cache.json` by default, gitignored). A
  deliberate on-demand cache, not a static markdown reference — the
  combined catalog is ~2,000 entries and would make a poor
  hand-maintained doc; regenerate it whenever you want a fresh snapshot
  rather than trusting an old one (the file's `exportedAt` field makes
  staleness visible). See "Exporting the game database" below.

Deliberately **does not** yet contain: an LLM dependency, an optimization
solver, or game-control intelligence — per PLAN.md Phase 8's own scope
("Initially the controller should only: represent protocol models,
validate sample telemetry, parse recorded JSON files").

## Running the tests

These need no running game - stdlib only, pure functions and fixtures:

```powershell
cd controller
python -m unittest discover -s tests -t . -v
```

Tests read the same fixtures as the mod-side schema tests in
`../tests/fixtures/` (`resource_nodes.json`, `buildables.json`,
`connections.json` — see `../tests/README.md` for their provenance:
currently hand-written synthetic data, not yet captured from a real game
session) rather than duplicating them.

## Live integration check

Requires Satisfactory (or the Editor in Play-In-Editor) actually running
with AIMod loaded and its HTTP server up:

```powershell
python controller/live_check.py
python controller/live_check.py --url http://127.0.0.1:51902/rpc --timeout 5
```

Calls every RPC method (`world.resourceNodes`, `world.buildables`,
`world.manufacturers`, `world.connections`), validates each response
against `satisfactory_ai.protocol`'s parsers (or an inline shape check
for `world.manufacturers`, which has no dedicated dataclass yet), and
checks the negative/validation paths (`UNKNOWN_METHOD` for a bogus
method, `TARGET_NOT_FOUND` for `world.setClockSpeed` against a
nonexistent buildable — never a positive-path mutation). Prints one
`[PASS]`/`[FAIL]` line per check and exits non-zero if anything failed,
so it's scriptable — this is how Claude (or anyone) can validate live
functionality by connecting to the mod, without needing a human to
manually click through Blueprint nodes or read the Output Log.

Complements, doesn't replace,
[docs/self-test.md](../docs/self-test.md)'s in-process self-test: that
one runs *inside* the game and can check things this can't (JSON
round-tripping through Unreal's own parser); this one runs *outside* and
checks things the in-process test can't (that the RPC envelope and HTTP
transport genuinely work end-to-end, as an external client sees them).

Verified the script itself works correctly (both success and failure
paths) against `tests/_mock_rpc_server.py`, a throwaway mock server —
6/6 checks pass against realistic mock responses, and it fails
gracefully with a helpful hint when nothing is listening on the port.
Has **not** yet been run against the real mod — that needs Satisfactory
or the Editor actually running.

## Exporting the game database

Same "requires a running game" caveat as the live integration check:

```powershell
python controller/export_catalog.py
python controller/export_catalog.py --url http://127.0.0.1:51902/rpc --output my_catalog.json
```

Writes `{recipes, items, buildables, exportedAt, sourceUrl}` to
`controller/catalog_cache.json`. Fails loudly (non-zero exit) rather than
writing a partial file if any of the three RPC calls fails — a half
snapshot would be worse than no snapshot. Not part of the git history:
the live RPC methods are the real source of truth, and this file exists
purely to save the round-trip cost of re-querying them during a build
session, not to become a second copy of the data someone edits by hand.

## Why a separate package from `../tests/`

`../tests/test_resource_node_telemetry.py` (PLAN.md Task 10) validates
raw JSON *shape* with zero dependencies, for the fastest possible sanity
check on what the mod emits. This package is the actual reusable Python
API a future controller/planner would import (`from satisfactory_ai.graph
import build_world_graph`) — a different concern, sharing fixtures rather
than test files.
