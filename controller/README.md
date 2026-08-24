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

Deliberately **does not** yet contain: an LLM dependency, an optimization
solver, game-control intelligence, or a network client — per PLAN.md
Phase 8's own scope ("Initially the controller should only: represent
protocol models, validate sample telemetry, parse recorded JSON files").
A network client would talk to Phase 9's `/rpc` endpoint
(`UDocModHttpServerSubsystem`, `http://127.0.0.1:51902/rpc`) once there's
a reason to add one beyond parsing already-captured JSON.

## Running the tests

```powershell
cd controller
python -m unittest discover -s tests -t . -v
```

Tests read the same fixtures as the mod-side schema tests in
`../tests/fixtures/` (`resource_nodes.json`, `buildables.json`,
`connections.json` — see `../tests/README.md` for their provenance:
currently hand-written synthetic data, not yet captured from a real game
session) rather than duplicating them.

## Why a separate package from `../tests/`

`../tests/test_resource_node_telemetry.py` (PLAN.md Task 10) validates
raw JSON *shape* with zero dependencies, for the fastest possible sanity
check on what the mod emits. This package is the actual reusable Python
API a future controller/planner would import (`from satisfactory_ai.graph
import build_world_graph`) — a different concern, sharing fixtures rather
than test files.
