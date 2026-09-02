# Build-efficiency plan: faster large factory builds without baking in strategy

Written 2026-09-02, directly from the measured pain of the two live HMF
sessions (initial build 2026-09-01, full-rate optimization 2026-09-02).
The optimization session took ~4 hours of wall-clock agent time; a
post-hoc accounting of where it went:

- **Connector-orientation discovery loops** — place a splitter/machine,
  query `world.connections`, find the input faces the wrong way, delete,
  re-place at a new yaw, re-query. Some attachments took 3 placements.
  Entirely avoidable: the yaw→connector mapping is deterministic per
  buildable class and most of it is now empirically known.
- **Belt-shape trial and error** — "Invalid Conveyor Belt shape!",
  "too steep!" (often bogus, from clearance-lift), min-length flakes on
  ~200-unit belts, S-curves against a connector's facing. Each failure
  cost a retry loop and usually ended with a jog merger or relay
  splitter pattern we have now used dozens of times. The rules are
  stable and encodable.
- **Per-call overhead** — a player teleport + settle sleep before nearly
  every `connectConveyor`/`connectPower`, one HTTP round trip per
  primitive, and full-world `world.connections` dumps (250KB+) to verify
  single components.
- **Power-graph fragility** — machine-to-machine daisy chains meant one
  deletion blacked out 8 machines and a remote miner; re-anchoring
  islands consumed a long recovery arc.

The user's constraint (explicit, 2026-09-02): do NOT bake fully
pre-planned builds with one-shot automation - the agent must keep making
the layout/strategy decisions in real time, and competing agents should
be able to attempt the same goals differently. The efficiency has to
come from **better tools and better phase discipline**, not from taking
decisions away from the agent. This matches the existing
`satisfactory_ai` package's documented "toolkit, not solver" posture and
CLAUDE.md's LLM-vs-deterministic-solver split.

## The layered design

```text
agent (LLM)          - chooses what to build, where, in what topology
    ↓ composes
composite builders   - manifold / machine row / crossing as ONE unit each
    ↓ emit
op-lists             - ordered primitive ops (place/recipe/clock/belt/wire)
    ↓ executed by
executor + verifier  - runs ops, retries known-flaky classes, verifies per phase
    ↓ over
AIMod RPC            - the existing primitives (later: batched + filtered)
```

The agent's prompt-level work shifts from "place splitter, check
connectors, fix, connect, verify" (8+ turns) to "build a 4-drop output
manifold on machines M1..M4, west trunk exit" (1 turn, one verified
result).

## Phase 1 - Python only, no mod changes, usable immediately

**1a. Connector database (`connector_db.py`)** - a persisted library of
`ConnectorProfile`s (layout.py's existing local-frame format) per
buildable class, seeded from the live-confirmed observations of the
2026-09-01/02 sessions (constructor, assembler, foundry, manufacturer,
smelter, miner Mk3, splitter, merger, storage container Mk2), plus
`learn_and_store()` to grow it from any `world.buildables` +
`world.connections` snapshot. Answers "where will every connector be if
I place class C at (x,y,yaw)" **before** placing - no probe placements,
no delete/re-place cycles. Includes per-class connector z-offset
(machine connectors sit at base+100; attachment connectors at placement
z; container inputs at +100 and +500).

**1b. Geometry router (`router.py`)** - `route_connection(from, to)`
returns a plan: either a single direct belt, or belt(s) plus the
jog-merger/relay-splitter/elevation attachments needed, applying the
encoded rulebook:
  - min reliable belt run 300 units (200 is flaky, <150 fails)
  - max run 5600 per segment; auto-relay beyond
  - incline ≤ 30° per segment (35° is the hard limit; stay under)
  - entry heading must match the input connector's facing; exit heading
    must match the output's - S-curves against a facing get a jog
    merger, never a retry loop
  - crossings resolved by z-lanes (+200 per layer) with two-stage climbs
  - long runs hugging foundation tops get relay splitters at seams
The router picks HOW a connection is made; the agent still picks WHAT
connects to what. It never chooses machine positions.

**1c. RPC client + executor (`rpc_client.py`, `executor.py`)** - the
first network client in the controller package (controller/README.md
anticipated adding one "when there's a reason"; composite execution is
the reason). Executes op-lists with the known reliability envelope
built in: teleport-near-work before belt/wire ops, the
dismantle-cycle reset for the stuck-wire state, one retry for the
known-flaky classes ("Surface is too uneven!", first-attempt
stackable-support failures), structured per-op results. Halt-on-error
by default.

**1d. Composite builders (`composites.py`)** - parametric, agent-invoked
units that emit op-lists and self-verify with ONE filtered telemetry
diff at the end:
  - `machine_row(recipe_class, count, origin, spacing, yaw, clock,
    shards)` - places + configures a row
  - `manifold(machine_ids, side, kind, trunk_exit)` - splitter or merger
    rail with per-machine drops, as one unit
  - `vertical_pair_block(...)` - the proven rod→screw direct-pair layout
  - `elevated_crossing(from, to, clearance_z)` - lift-up, traverse,
    lift-down
  - `pole_backbone(waypoints)` - power spine FIRST, machines tap it
    (never machine-to-machine chains - the deletion-fragility lesson)

## Phase 2 - mod C++ (next recompile/redeploy window)

**2a. Filtered telemetry** - `world.connections` and `world.buildables`
gain optional `buildableIds[]` / bounding-box params so per-phase
verification stops shipping the whole world.

**2b. `world.batch`** - ordered op array executed server-side, per-op
structured results, halt-or-continue policy, and internal instigator
positioning (kills the teleport-per-call dance and most round trips).

**2c. `world.connectorLayout`** - connector offsets/facings for a
buildable class read from its CDO at yaw 0, so the connector DB
self-maintains across game updates instead of relying on hand-seeded
observations.

**2d. connectPower hardening** - already spawn-tasked separately
(stuck-state root cause + pinned connector positions).

## Phase 3 - process discipline (no code, agent practice)

Categorical build order, refined from what actually failed:
1. Terrain survey (`world.terrainHeightGrid`) over the full footprint
2. Foundations/platforms (all of them)
3. All machines (correct yaw first time, via connector DB)
4. All configuration (recipes, clocks, shards - order-independent)
5. Pole backbone, then power taps
6. Attachments + belts (router-planned, dry-run validated in bulk via
   `world.testConveyorBelt` before building any)
7. One verification sweep (filtered diff + rate sampling)

Demolition discipline: belts before their attachments, and check the
power graph for subtree severing before deleting anything wired.

## Explicitly out of scope

- Any function that picks a factory layout, chooses recipes/ratios, or
  sequences goals - that stays with the agent (and with the production
  math already in `production.py` as pure arithmetic the agent calls).
- One-shot "build the whole factory" automation.
- Multi-click spline routing (SHBS_AdjustPole) - segment chaining via
  waypoint buildables remains the pattern.

## Status

- **Phase 1a-1d: IMPLEMENTED 2026-09-02** (same session as this plan),
  80/80 controller tests passing. Modules: `connector_db.py`
  (9 seeded classes; smelter + miner marked UNVERIFIED-yaw),
  `router.py`, `rpc_client.py`, `executor.py`, `composites.py`
  (machine_row, manifold, pole_backbone, verify_connections). Tests
  reproduce real live geometries including the lift-top jog fix and the
  east screw rail (planned yaw matches the live build exactly).
  **Not yet exercised against a running game** - the first live use
  should start small (one machine_row, one manifold) and
  learn_and_store() any class whose seed is marked UNVERIFIED-yaw.
- Phase 2 (mod C++): **2a (filtered world.buildables/world.connections)
  and 2c (world.connectorLayout) IMPLEMENTED and compiled 2026-09-02** -
  staged for the next Alpakit redeploy, NOT yet live-tested (see
  RPC_REFERENCE.md for both). After redeploy: cross-check
  world.connectorLayout's constructor output against connector_db's
  live-verified seeds, then switch the DB to feed from it. 2b
  (world.batch) deferred to a focused session - batching the ASYNC
  construction ops means chaining their deferred-poll completions, which
  deserves its own careful pass. connectPower hardening (2d) is
  spawn-tasked separately (task_09ace681).
- Read-only live calibration DONE 2026-09-02 (game running, nothing
  built): rpc_client verified, connector seeds live-verified via
  learn_and_store (smelter/miner resolved, foundry cross-check exact),
  router calibrated against world.testConveyorBelt on real free
  connector pairs (11/14 agreement; remaining gaps are deliberate
  conservatism), executor.validate_plan() dry-run gate proven end-to-end.
- Typical agent flow with the new tools:
  ```python
  from satisfactory_ai.connector_db import ConnectorDb, CONSTRUCTOR
  from satisfactory_ai.composites import machine_row, manifold
  from satisfactory_ai.executor import Executor
  from satisfactory_ai.router import Endpoint, route_connection
  from satisfactory_ai.rpc_client import RpcClient

  db, ex = ConnectorDb(), Executor(RpcClient())
  row = machine_row(db, build_recipe=..., class_key=CONSTRUCTOR, count=4,
                    origin=..., spacing=800, yaw=180,
                    machine_recipe=..., clock_percent=220, shards=3)
  report = ex.execute(row.plan)          # place + configure, one call
  # outputs from db.predict(...) -> manifold(...) -> ex.execute(...)
  # trunk from the manifold warning -> route_connection(...) -> execute
  ```

## Success measure

The HMF optimization's belt/attachment work (~40 belts, ~20
attachments, ~30 config ops, with ~25 failed attempts) took ~3 hours.
The same work through Phase 1 tooling should be: one `machine_row` +
two `manifold` + a handful of `route_connection` calls - target
**under 30 minutes** wall clock with fewer than 5 unplanned failures,
measured on the next comparable build.
