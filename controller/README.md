# satisfactory_ai controller (skeleton)

PLAN.md Phase 8. The first platform-neutral external component. Currently
contains:

- `satisfactory_ai/models.py` — `Position`, `Rotation`, `ResourceNode`,
  `Buildable`, `FactoryConnection` dataclasses mirroring DocMod's
  `FDocMod*Telemetry` structs
  (`Mods/GameFeatures/DocMod/Source/DocMod/Public/DocModTelemetryTypes.h`).
- `satisfactory_ai/protocol.py` — parses the JSON shapes documented in
  [docs/telemetry-protocol.md](../docs/telemetry-protocol.md)
  (resourceNodes / buildables / connections) into those models, rejecting
  unsupported protocol versions.
- `satisfactory_ai/graph.py` — PLAN.md Phase 11: `build_world_graph()`
  turns `Buildable` + `FactoryConnection` facts into a directed
  `WorldGraph` (one edge per connected `"Output"` connection point). Per
  PLAN.md, graph construction/analysis belongs on the controller, not the
  mod — the mod only exposes the raw per-connection-point facts.

- `live_check.py` — a network client, but a diagnostic one, not part of
  the "controller" proper: connects to a **running** DocMod `/rpc`
  server (`http://127.0.0.1:51902/rpc` by default) and checks every RPC
  method against real (or at least live) game state, printing PASS/FAIL
  per method and exiting non-zero on any failure. See "Live integration
  check" below.

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
with DocMod loaded and its HTTP server up:

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

## Why a separate package from `../tests/`

`../tests/test_resource_node_telemetry.py` (PLAN.md Task 10) validates
raw JSON *shape* with zero dependencies, for the fastest possible sanity
check on what the mod emits. This package is the actual reusable Python
API a future controller/planner would import (`from satisfactory_ai.graph
import build_world_graph`) — a different concern, sharing fixtures rather
than test files.
