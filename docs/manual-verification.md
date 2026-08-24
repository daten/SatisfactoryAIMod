# Manual verification checklist (running document)

This is a living checklist of everything that has been implemented and
compile-verified from the CLI, but still needs a human to confirm at
runtime (Unreal Editor and/or Satisfactory). Updated as each phase adds
new things to check. Nothing in this file should be treated as done until
you've actually run it and it's been checked off here.

**How to use this:** open `FactoryGame.uproject` in Unreal Editor
(Development Editor — already built via `.\tools\build-editor.ps1`), and
work through the "Pending" items in order — later ones tend to build on
earlier ones being confirmed working. Update this file (or tell Claude the
results) once you've checked something, so future sessions don't re-ask.

---

## Pending

### 1. Module startup logging (Phase 2)

- Launch the Editor (or Satisfactory with the mod deployed via Alpakit).
- Check the Output Log (Editor) or `%LOCALAPPDATA%\FactoryGame\Saved\Logs\FactoryGame.log`
  (Satisfactory) for:
  ```
  LogDocModAI: Display: DocMod AI interface module initialized
  ```
- **Expected:** the line appears once, near other plugin/module startup
  messages, on every load.

### 2. Blueprint smoke test (Phase 3)

Full steps in [blueprint-smoke-test.md](blueprint-smoke-test.md). Summary:

- Open the `RootGameWorld_DocMod` level (or any Blueprint), add a
  **Get Interface Version** node (category `DocMod | AI Interface`), wire
  to Print String, run.
- **Expected:** node is findable in the palette, and prints `0.1.0`.

### 3. Resource node enumeration (Phase 4, Task 7)

- Load a save (or the default level) that actually has resource nodes in
  it — `RootGameWorld_DocMod` is a bare test level and may not have any;
  loading an actual Satisfactory map is likely needed here.
- Call **Log Resource Nodes** (category `DocMod | AI Interface`) from any
  Blueprint (Level Blueprint `BeginPlay` is fine for a one-off test),
  passing `self` as the world context.
- Check `LogDocModAI` output for lines like:
  ```
  LogDocModAI: Display: ResourceNode: id=... resource="Iron Ore" purity=Pure pos=(12345.0, -6789.0, 890.0) occupied=false
  LogDocModAI: Display: LogResourceNodes: enumerated 47 resource node(s)
  ```
- **Expected:** resource names/purity/positions look plausible for real
  nodes on the loaded map (not all zeros, not obviously wrong), and the
  count is a believable number of nodes for that map (dozens, not 0 and
  not millions).
- **If this reveals `AFGResourceNodeManager` actually works** (contrary to
  its stubbed source — see [resource-node-research.md](resource-node-research.md)),
  that's worth telling Claude about; it would justify switching off the
  `TActorIterator` scan later.

### 4. JSON telemetry output (Phase 4, Task 9 / PLAN.md Phase 6)

- Call **Log Resource Nodes As Json** (category `DocMod | AI Interface`)
  the same way as above.
- Check `LogDocModAI` for a single-line JSON blob starting with
  `{"protocolVersion":1,"resourceNodes":[...`.
- **Expected:** valid JSON (paste it into any JSON validator, or
  `python -m json.tool` it) containing one object per node with
  `id`, `resource`, `resourceClass`, `purity`, `position.{x,y,z}`,
  `occupied` fields, matching the shape documented in
  [telemetry-protocol.md](telemetry-protocol.md).
- **Important follow-up:** copy that real captured JSON output over
  `tests/fixtures/resource_nodes.json`, which currently holds
  **synthetic, hand-written placeholder data** (see that file and
  `tests/README.md`) — it was never captured from a real game session.
  Once replaced, re-run `python -m unittest discover tests` to confirm the
  real data still passes the same schema checks.

---

## Confirmed

*(nothing yet — move items here, with the date and what you observed, once
verified)*
