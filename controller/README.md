# satisfactory_ai controller (skeleton)

PLAN.md Phase 8. The first platform-neutral external component. Currently
contains only:

- `satisfactory_ai/models.py` — `Position`, `ResourceNode` dataclasses
  mirroring `FDocModResourceNodeTelemetry`
  (`Mods/GameFeatures/DocMod/Source/DocMod/Public/DocModTelemetryTypes.h`).
- `satisfactory_ai/protocol.py` — parses the JSON envelope documented in
  [docs/telemetry-protocol.md](../docs/telemetry-protocol.md)
  (`{"protocolVersion": 1, "resourceNodes": [...]}`) into those models,
  rejecting unsupported protocol versions.

Deliberately **does not** yet contain: an LLM dependency, an optimization
solver, game-control intelligence, or a network client — per PLAN.md
Phase 8's own scope ("Initially the controller should only: represent
protocol models, validate sample telemetry, parse recorded JSON files").
A network client belongs here once PLAN.md Phase 9 (localhost transport)
exists on the mod side for it to talk to.

## Running the tests

```powershell
cd controller
python -m unittest discover -s tests -t . -v
```

Tests read the same fixture as the mod-side schema test in
`../tests/fixtures/resource_nodes.json` (see `../tests/README.md` for its
provenance — currently hand-written synthetic data, not yet captured from
a real game session) rather than duplicating it.

## Why a separate package from `../tests/`

`../tests/test_resource_node_telemetry.py` (PLAN.md Task 10) validates the
raw JSON *shape* with zero dependencies, for the fastest possible sanity
check on what the mod emits. This package is the actual reusable Python
API a future controller/planner would import (`from satisfactory_ai.protocol
import parse_resource_node_telemetry`) — a different concern, sharing the
same fixture rather than the same test file.
