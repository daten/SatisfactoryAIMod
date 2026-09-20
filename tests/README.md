# AIMod telemetry tests

PLAN.md Phase 8 / Task 10: a small, dependency-free Python test that
consumes recorded telemetry JSON and validates it against the schema
documented in [docs/telemetry-protocol.md](../docs/telemetry-protocol.md).
This deliberately does **not** contain an LLM dependency, an optimization
solver, or any game-control intelligence — per PLAN.md Phase 8, the
external controller starts out only representing/validating protocol
models. This is narrower than that: just enough to prove the JSON shape
`UAIModFunctionLibrary::LogResourceNodesAsJson` emits is well-formed and
Python-consumable, without needing Satisfactory running.

## Fixture provenance

`fixtures/resource_nodes.json` is **hand-written synthetic data**, not
captured from a real game session. No runtime verification of the C++
telemetry code has happened yet as of when this fixture was written — see
[docs/manual-verification.md](../docs/manual-verification.md). Once someone
runs `LogResourceNodesAsJson` in a loaded Satisfactory save and copies the
real logged JSON over this file, re-run the test to confirm real captured
data still passes.

## Running

No dependencies beyond the Python 3 standard library:

```powershell
python -m unittest discover -s tests -t . -v
```

(or run the module directly: `python tests/test_resource_node_telemetry.py -v`)

## What it checks

`test_resource_node_telemetry.py` validates, against the fixture:

- Top level: `protocolVersion == 1`, `resourceNodes` is a list.
- Per node: `id`/`resource`/`resourceClass`/`purity` are non-empty
  strings; `purity` is one of `"Impure"`, `"Normal"`, `"Pure"` (the
  `EResourcePurity` display strings — see resource-node-research.md);
  `position` has numeric `x`/`y`/`z`; `occupied` is a bool.

It does not (yet) validate anything about *values* being plausible for a
real map (e.g. coordinate ranges) — that's a runtime/manual-verification
concern, not something a synthetic fixture can prove.
