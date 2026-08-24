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
  Once replaced, re-run `python tests/test_resource_node_telemetry.py -v`
  to confirm the real data still passes the same schema checks (the test
  itself is confirmed working: verified it both passes on valid data and
  fails when a field is corrupted).

### 5. Localhost HTTP transport (Phase 9)

- Launch the Editor or Satisfactory with the mod loaded.
- Check `LogDocModAI` for:
  ```
  LogDocModAI: Display: DocMod HTTP server listening on http://127.0.0.1:51902/rpc (loopback only - see Config/DefaultEngine.ini ListenerOverrides)
  ```
- **Critical safety check — do this before anything else with the server:**
  run `netstat -an | findstr 51902` (Windows) while the game is running.
  **Expected:** a line showing `127.0.0.1:51902` in `LISTENING` state.
  **If instead it shows `0.0.0.0:51902`,** the loopback override in
  `Config/DefaultEngine.ini` (`[HTTPServer.Listeners]` `ListenerOverrides`)
  did not take effect and the server is reachable from the LAN — stop
  using it and tell Claude immediately; see
  [networking-research.md](networking-research.md) for why this override
  exists and how it's supposed to work.
- Once loopback-only is confirmed, test the endpoint itself, e.g. from
  PowerShell:
  ```powershell
  Invoke-RestMethod -Method Post -Uri "http://127.0.0.1:51902/rpc" -ContentType "application/json" -Body '{"protocolVersion":1,"requestId":"1","method":"world.resourceNodes"}'
  ```
- **Expected:** a JSON response with `"success":true` and a `"result"`
  object containing `"resourceNodes"` (same shape as item 4 above, just
  wrapped in the RPC envelope from
  [telemetry-protocol.md](telemetry-protocol.md)'s sibling doc,
  [networking-research.md](networking-research.md)).
- Also worth testing the error paths: an unknown `method`, a missing
  `protocolVersion`, and malformed JSON should each return
  `"success":false` with a structured `"error":{"code":...,"message":...}`
  rather than a raw HTTP 500 or a crash.

### 6. Buildings / manufacturers telemetry (Phase 10)

- Load a save with actual buildings placed (constructors, smelters, etc).
- Call **Log Buildables** and **Log Manufacturers** (category
  `DocMod | AI Interface`) from any Blueprint.
- Check `LogDocModAI` for plausible output, e.g.:
  ```
  LogDocModAI: Display: Buildable: id=... class=.../Build_ConstructorMk1.Build_ConstructorMk1_C pos=(...) rot=(...)
  LogDocModAI: Display: LogBuildables: enumerated 83 buildable(s)
  LogDocModAI: Display: Manufacturer: id=... class=... recipe="Iron Plate" clock=100% status=Producing progress=0.42 productivity=1.00 inputItems=1 outputItems=1
  LogDocModAI: Display: LogManufacturers: enumerated 12 manufacturer(s)
  ```
- **Expected:** buildable count is plausible for the map (not 0, not
  absurd), classes look like real building asset paths, and for
  manufacturers specifically: recipe names match what's actually set in
  the game, clock speed matches the in-game slider, and production status
  matches what you can see in-game (a machine you can see actively
  producing should report `status=Producing`, not `status=None`).
- **Specifically worth noting if it happens:** if `AFGBuildableSubsystem`
  turns out to be populated (see
  [buildable-research.md](buildable-research.md) §1), `LogBuildables`
  should still enumerate correctly since it's the primary path already —
  no special test needed beyond confirming a plausible count. But if the
  count comes back as **zero** even on a save with visible buildings,
  that's a sign the subsystem fallback logic needs attention.
- Also call **Log Buildables As Json** / **Log Manufacturers As Json** (or
  hit `/rpc` with `"method":"world.buildables"` /
  `"world.manufacturers"`) and validate the JSON shape against
  [telemetry-protocol.md](telemetry-protocol.md).

### 7. Factory connections / world graph (Phase 10 connections + Phase 11)

- Call **Log Factory Connections** (or hit `/rpc` with
  `"method":"world.connections"`) on a save with an actual belt line
  running between machines.
- **Expected:** for two machines you know are physically linked by a
  belt, you should see matching rows — an `Output` row on the source
  buildable's id pointing at the belt (or directly at the destination, if
  belts themselves aren't separately enumerated as connection owners —
  check both), and an `Input` row on the destination side pointing back.
  `connected=false` rows are normal for connection points nothing is
  plugged into (e.g. an idle machine's unused output).
- This one doesn't need the game running to partially verify: run
  `cd controller && python -m unittest discover -s tests -t . -v` — the
  `test_graph.py` suite already confirms `build_world_graph()` correctly
  turns synthetic buildable+connection fixtures into edges (verified
  passing: 9/9 tests). What still needs real game data is confirming the
  *mod's* connection telemetry matches real in-game belt topology, not
  just that the Python graph-building logic is internally consistent.
- Once `world.buildables`/`world.connections` output has been captured
  from a real save, replace `tests/fixtures/buildables.json` and
  `tests/fixtures/connections.json` (currently hand-written synthetic
  data, same caveat as `resource_nodes.json`) and re-run both the
  mod-side and controller-side test suites.

---

## Confirmed

*(nothing yet — move items here, with the date and what you observed, once
verified)*
