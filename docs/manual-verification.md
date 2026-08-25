# Manual verification checklist (running document)

This is a living checklist of everything that still needs a human to
confirm at runtime (Unreal Editor and/or Satisfactory), beyond what runs
automatically now.

**Most of the "does it crash / is the returned data well-formed" checking
below is now automatic** — see
[self-test.md](self-test.md). Every time a game world loads (Editor PIE,
standalone, or a packaged build), `DocModSelfTest::RunAll` exercises every
read-only telemetry function and the write operations' validation logic,
and logs a `PASS`/`FAIL` summary block to `LogDocModAI` with no manual
Blueprint work or HTTP calls needed. **Check that summary block first** —
if everything in it passes, most of the "shape/doesn't-crash" items below
are already covered, and what's left is the things a fixed self-test
genuinely can't judge: whether *specific* values are plausible for the
map you're looking at, whether a positive-path mutation actually changed
the game correctly, and whether the HTTP server is really loopback-only
(the self-test runs in-process, so it can't observe the network binding
from outside).

**How to use this:** open `FactoryGame.uproject` in Unreal Editor
(Development Editor — already built via `.\tools\build-editor.ps1`), load
a real map, and press Play — check the self-test summary in the Output
Log first, then work through whichever "Pending" items below it didn't
cover. Update this file (or tell Claude the results) once you've checked
something, so future sessions don't re-ask.

### What "Call **Some Function**" actually means

Every item below that says "Call **X**" means one of two things — pick
whichever's less friction for that item:

**Blueprint** (needed for item 2, and for anything with no JSON/RPC
equivalent, e.g. `LogResourceNodes`'s human-readable log format):

1. Load a level that actually has the game state you're checking (an
   in-game map — `RootGameWorld_DocMod` is a bare test level, likely
   empty).
2. **Blueprints** menu → **Open Level Blueprint**.
3. Right-click empty graph space → search **"BeginPlay"** → add
   `Event BeginPlay` if it's not already there.
4. Drag off its exec pin, release, search for the function's display
   name (e.g. **"Log Resource Nodes"**) — it's filed under the
   `DocMod | AI Interface` category. Click to add it, wired to
   `BeginPlay`.
5. Functions taking a world-context object auto-hide that pin and wire
   it to the graph's implicit **Self** (the Level Blueprint actor) — you
   shouldn't need to fill anything in. If a "World Context Object" pin
   *is* visible, connect a **Self** node to it.
6. **Compile** (toolbar button in the Blueprint editor).
7. Press **Play** in the main editor toolbar — the log only fires once
   `BeginPlay` actually runs, i.e. once you're playing, not just after
   compiling.
8. Check **Window → Developer Tools → Output Log** for the
   `LogDocModAI` lines.

**Console command** (no Blueprint editing, works immediately — see
[chat-and-console-commands.md](chat-and-console-commands.md)): open the
in-game/PIE console (`~`) and type e.g. `DocMod.SelfTest` or
`DocMod.ResourceNodes`. Also usable via `/docmod <subcommand>` in chat,
but that needs a one-time Editor wiring step first — see the same doc.

**RPC endpoint** (Phase 9+, no Blueprint editing — works for anything
with a `*AsJson`/`world.*` RPC method, with the game already running):

```powershell
Invoke-RestMethod -Method Post -Uri "http://127.0.0.1:51902/rpc" -ContentType "application/json" -Body '{"protocolVersion":1,"requestId":"1","method":"world.resourceNodes"}'
```

Swap `"method"` for `world.buildables` / `world.manufacturers` /
`world.connections` etc. This exercises the same underlying collection
code as the equivalent Blueprint node, so for most read-only items below
it's the faster path.

**Python** (fastest, scriptable, no manual PowerShell/Blueprint steps —
this is what Claude uses to self-check without asking you to click
through the Editor):

```powershell
python controller/live_check.py
```

Calls every RPC method and reports PASS/FAIL — see
`controller/README.md`'s "Live integration check" section.

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
- **Critical safety check:** run `netstat -an | findstr 51902` (Windows)
  while the game is running. **Expected:** a line showing `127.0.0.1:51902`
  in `LISTENING` state. **This check was finally run live on 2026-08-24
  and failed** — the real Steam session showed `0.0.0.0:51902`, not
  loopback — see the Confirmed section below and
  [networking-research.md](networking-research.md)'s "Confirmed broken
  live" note for the root cause and the two-layer fix applied
  (application-layer `PeerAddress` check + a new
  `Mods/GameFeatures/DocMod/Config/DefaultEngine.ini`). **Re-checked
  2026-08-25: still `0.0.0.0:51902`** — the plugin-level ini fix did not
  work (see networking-research.md's "Plugin-level ini fix confirmed NOT
  working" note). The application-layer `PeerAddress` check remains the
  real protection and is unaffected. **Still not tested**: an actual
  request from another machine on the LAN, to confirm it genuinely gets
  rejected with `403 FORBIDDEN`.
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

### 8. First write operations (Phase 12) — use a disposable save

**This is the first capability that actually changes game state. Test it
against a save you don't mind corrupting — duplicate your save file
first, or use a throwaway creative-mode save — not your main
progression save,** until it's been exercised enough to trust.

**Fastest way to try this:** walk up to a machine and press the DocMod
hotkey (**F6** by default — F11 was tried first and found to collide
with the engine's fullscreen toggle; configurable, see
[hotkey.md](hotkey.md)). It prints the targeted machine and attempts a
real +10%-clock-speed change in one keypress, reporting success/failure
both in the log and as an in-game chat message. Entirely untested so far
(added after these playtest sessions) — if it works, it's also the
fastest way to confirm chat messages actually reach the screen, which
none of the RPC/console testing above can show. The manual steps below
still apply if you want to test specific values/edge cases (out-of-range
percent, bad recipe path, etc.) that the hotkey doesn't exercise.

- Find a manufacturer's `buildableId` either via `world.manufacturers`
  (item 6 above), or — easier — stand in front of the specific machine
  you want to test on in-game and use `world.targetedManufacturer` /
  `DocMod.Target` / `/docmod target` (item 9 below) instead of picking
  one out of a potentially huge list. Ids are session-local either way,
  so re-fetch every session.
- **Clock speed:** call `world.setClockSpeed` with a valid percent (e.g.
  the current value, or 150 if the building supports overclocking).
  - **Expected:** `success:true`, empty `result`. Because
    `SetPendingPotential` takes effect next cycle (not instantly — see
    [operations-protocol.md](operations-protocol.md)), the change may
    not be visible in `world.manufacturers`' `clockSpeedPercent` until
    the machine finishes its current production cycle — wait and
    re-check rather than assuming failure.
  - Also test an out-of-range value (e.g. `9999`) and confirm you get
    `INVALID_CLOCK_SPEED` with the real valid range in the message, not
    a crash or a silently-clamped value.
  - Test a bogus `buildableId` and confirm `TARGET_NOT_FOUND`.
- **Recipe:** call `world.setRecipe` with a real recipe class path (get
  one from the game's own data, or from `UFGRecipe::GetProducedIn`
  research — a wiki/community recipe path list works too) that's valid
  for the target building, **with both inventories empty** (an idle,
  freshly-placed machine works).
  - **Expected:** `success:true`; the building now shows the new recipe
    in-game and in a follow-up `world.manufacturers` call.
  - Test with a non-empty inventory (put an item in the input) and
    confirm you get `INVENTORY_NOT_EMPTY` rather than the game silently
    accepting it and doing something undefined.
  - Test with a recipe that's real but not valid for that building class
    (e.g. a Smelter recipe on a Constructor) and confirm
    `RECIPE_NOT_COMPATIBLE`.
  - Test with a garbage class path (e.g. `"/Game/DoesNotExist.Foo_C"`)
    and confirm `INVALID_RECIPE`, not a crash.
- **If any of these produces a crash, a silent no-op where success:true
  was returned, or a state the game itself considers invalid (visual
  glitches, being unable to interact with the building afterward), stop
  and report it before using these operations again** — that's exactly
  the kind of gap CLAUDE.md's validation requirements exist to catch, and
  finding one now (on a disposable save) is the point of this checklist.

### 9. Targeted manufacturer (added in response to "can I target what I'm looking at?")

- Look directly at a manufacturing machine in-game (close enough that
  you'd normally see the interact prompt).
- Call `world.targetedManufacturer`, `DocMod.Target`, or `/docmod target`
  (all equivalent — see [chat-and-console-commands.md](chat-and-console-commands.md)).
- **Expected:** returns that specific machine's telemetry (recipe, clock
  speed, id, etc.) — not some other, arbitrary machine. Look away from
  any machine (e.g. at open sky) and confirm it correctly reports
  "nothing targeted" (`"manufacturer": null` over RPC) rather than
  returning stale data from the last thing you looked at or crashing.
- Look at something that isn't a manufacturer (a resource node, a
  foundation, a belt) and confirm it also correctly reports "nothing
  targeted" rather than misidentifying it as a manufacturer.
- **Not yet runtime-verified at all** — this is new, added after the two
  playtest sessions above.

### 10. Extractor placement dry-run (Phase 13, first experiment)

- Stand facing a real, unoccupied **solid** resource node (Iron/Copper/
  Coal/Limestone/etc - not Crude Oil/Water/SAM, which are out of scope
  for this first pass) from a normal build-range distance, close enough
  that you're clearly aiming at it, not just nearby.
- Run `DocMod.TargetNode` first to confirm it found the node you're
  looking at (prints resource/purity/occupied/id, or "not currently
  looking at a resource node"). This now uses
  `AFGCharacterPlayer::GetBestUsableActor()` - the same game-truth
  targeting `DocMod.Target` uses for manufacturers - so it should agree
  with whatever the game's own "Press E to start mining..." prompt shows
  (see `docs/extractor-placement-research.md`'s correction note: an
  earlier heuristic-based version failed this exact check live and was
  replaced).
- Once `DocMod.TargetNode` finds it, run `DocMod.TestExtractorPlacement`.
  **This never calls `Construct()` and never touches your save** - it
  spawns a temporary hologram, checks it, and destroys it before
  returning, regardless of outcome.
- **Expected, and what we're actually trying to learn:**
  - `DocMod: CanConstruct() = true - placement would succeed` — the best
    outcome: confirms both the synthetic `FHitResult` snapping and
    buildgun-free hologram construction assumptions hold for the happy
    path. This is the strongest possible signal to move forward with an
    actual `Construct()` call next.
  - `DocMod: CanConstruct() = false - ...` with a **specific, sensible**
    disqualifier (e.g. "Too close to another building", "Resource node
    is occupied") — also a good outcome: means the flow up through
    `UpdateHologramPlacement`/`CanConstruct` genuinely works and is
    correctly evaluating real placement rules, just not on this
    particular spot/node. Try a different unobstructed node if this
    happens on a clearly-valid spot.
  - `DocMod: CanConstruct() = false - <none>` (empty disqualifier list)
    or a disqualifier that reads as clearly wrong for what you're
    looking at (e.g. `UFGCDNeedsResourceNode` while aiming directly at a
    node) — **this is the signal that the synthetic hit result isn't
    snapping correctly** (the risk flagged in
    `docs/extractor-placement-research.md` §2) - report the exact
    disqualifier text back, this is exactly the evidence needed to
    debug it further.
  - `HOLOGRAM_SPAWN_FAILED` or a crash — would mean
    `SpawnHologramFromRecipe` doesn't work the way its header signature
    implies from a non-buildgun caller (§5's risk). A crash specifically
    should be reported immediately with the exact log output leading up
    to it.
- Check `LogDocModAI` regardless of the console output shown - it logs
  the full disqualifier list with each one's soft/hard status, more
  detail than the one-line console summary.
- **Standalone hologram (`DocMod.TestExtractorPlacement`): consistently
  `CANNOT_CONSTRUCT: Initializing (hard)` across five fix attempts** -
  see `docs/extractor-placement-research.md`'s "will not clear" section.
  Node targeting itself is confirmed correct (`DocMod.TargetNode`
  matches the game's own "Press E to mine" prompt).
- **Build-gun-driven hologram (`DocMod.TestExtractorPlacementViaBuildGun`)
  — SUCCEEDED, first real test, twice in a row (2026-08-24):
  `canConstruct=true disqualifiers=[<none>]`, resolved after 1 real
  tick.** See `docs/buildgun-driven-placement-research.md`'s "Result"
  section. This is the confirmed-working dry-run path going forward -
  use this command, not the standalone one, for further placement
  testing. **This command visibly equips the build gun** (shows the
  normal build-mode HUD briefly) unlike the standalone one - expected,
  not a bug.
- **`DocMod.ConstructExtractorOnTargetedNode` — first real building
  placement, SUCCEEDED on the first attempt (2026-08-24).** Log showed
  `nodeNowOccupied=true` plus the real `SK_MinerMk1` mesh loading via the
  game's own build-effect system - independent confirmation, not just a
  flag flip. A `Build_MinerMk1` genuinely exists on the targeted node
  now. See `docs/buildgun-driven-placement-research.md`'s "First real
  building placement" section.
- **Save/reload survival and telemetry visibility CONFIRMED (2026-08-25).**
  A `ConstructBuildingNearPlayer`-built Constructor Mk1 survived a full
  save/exit/reload and correctly appeared in `world.buildables` at its
  exact built position (`X=-54853.1 Y=160796.7 Z=3341.4`, matched to the
  fractional unit) - confirms proper `AddBuildable` registration, not a
  floating unregistered actor. (Tested via the generalized command, not
  the Miner specifically, but exercises the identical
  `InternalConstructHologram` path both share.) Still not checked:
  whether a placed extractor is actually producing (not just occupying
  the node) once powered - lower priority, registration/persistence was
  the real open question.

### 11. Generalized building placement (`DocMod.PlaceBuildingNearPlayer`)

- Stand somewhere with reasonably open, roughly flat ground in front of
  you (a real vertical trace finds the actual floor height, but there's
  no clearance-avoidance logic - if the spot 800 units in front of you
  overlaps something, `CanConstruct()` will correctly say no).
- Run `DocMod.PlaceBuildingNearPlayer` with no argument (defaults to
  Constructor Mk1), or with a specific recipe class path, e.g.
  `DocMod.PlaceBuildingNearPlayer /Game/FactoryGame/Recipes/Buildings/Recipe_AssemblerMk1.Recipe_AssemblerMk1_C`.
- **Expected:** same visible build-gun-equip side effect as the other
  build-gun-driven commands, then `LogDocModAI` reports either a
  successful construction (with the real placement location logged) or
  a specific `CANNOT_CONSTRUCT` disqualifier if the spot wasn't valid -
  try moving to different ground and re-running if so.
- **Scope reminder:** only tested for simple, single-step, non-snapping
  buildings (production machines). Belts, pipes, walls, and foundations
  are explicitly out of scope for this function - if tried, expect
  either `HOLOGRAM_SPAWN_FAILED` or genuinely undefined behavior, not a
  supported case.
- **CONFIRMED WORKING (2026-08-25)**, including the placement-accuracy
  fix (`docs/buildgun-driven-placement-research.md`'s "§3 correction"):
  a Constructor Mk1 built via this command matched its computed
  ground-trace candidate to within a unit, then survived save/reload and
  showed up correctly in `world.buildables`. See the Confirmed section
  below.

---

## Confirmed

### 2026-08-24 — first real playtest (fresh game, `GameLevel01`)

Reviewed `%LOCALAPPDATA%\FactoryGame\Saved\Logs\FactoryGame.log` from an
actual launched-game session. Real findings, not a synthetic test:

- **Item 1 (module startup) — confirmed.** `DocMod AI interface module
  initialized` and `DocMod AI interface module shutting down` both fired
  correctly at session start/end.
- **Item 5 (HTTP transport) — partially confirmed.** `DocMod HTTP server
  listening on http://127.0.0.1:51902/rpc...` fired correctly. **Still
  not done:** the `netstat` loopback-binding check, and an actual HTTP
  round-trip request (the self-test calls the underlying functions
  in-process, not over HTTP) — still genuinely pending.
- **Items 3/4 (resource node enumeration + JSON) — confirmed working,
  with one real bug found and fixed.** The self-test (see
  [self-test.md](self-test.md)) ran automatically and found **631 real
  resource nodes** across all 14 real Satisfactory resource types (Iron
  Ore, Copper Ore, Crude Oil, Geyser, SAM, Uranium, etc.) with real
  coordinates and class paths — `TActorIterator<AFGResourceNode>` and
  `UFGItemDescriptor::GetItemName()` work correctly against the actual
  game. **Bug found:** the `purity` field returned Slate rich-text UI
  markup (`"<Bold>(Normal)</>"`) instead of plain text — traced to
  `GetResourcePurityText()`, whose own doc comment says "For UI" and
  which I used anyway. Fixed: now uses the raw `GetResourcePurity()`
  enum, mapped manually (matching how production status and connection
  direction were already handled). **Also fixed as a result of this
  run:** JSON output was pretty-printed by default
  (`TJsonWriterFactory`'s default policy), which turned one `resourceNodes`
  call into ~8,200 log lines for 631 nodes — switched to
  `TCondensedJsonPrintPolicy` (compact, single-line) for all JSON output.
  **Needs a fresh test run** to confirm both fixes actually resolve the
  issue — not yet re-verified against a real game session.
- **Items 6/7 (buildings/manufacturers/connections) — inconclusive, needs
  your input.** All three came back empty (0 buildables, 0 manufacturers,
  0 connections). A `LogUI: UpdateFocusHighlights [mCreateNewGame]` line
  appeared shortly after in the log, suggesting this may have been a
  freshly created game with nothing built yet — in which case 0 is
  correct, not a bug. **Unconfirmed until you tell Claude whether
  anything was actually built before this test ran.** If yes and it
  still came back empty, that needs real investigation (see
  [buildable-research.md](buildable-research.md) §1's note about
  `AFGBuildableSubsystem`'s unverified population).
- **Item 8 (write operations), negative path only — confirmed.** Both
  `SetManufacturerClockSpeed` and `SetManufacturerRecipe`, called
  automatically by the self-test against a deliberately bogus buildable
  id, correctly returned `TARGET_NOT_FOUND` rather than crashing or
  silently succeeding. **The positive path (an actual successful
  mutation) is still unverified** — still needs the disposable-save
  testing described below.
- Self-test itself had one cosmetic bug, also fixed: all four `*.json`
  checks printed the same hardcoded "did not parse as JSON" message
  regardless of whether they passed or failed (the pass/fail
  determination itself was correct — just the printed detail text was
  misleading on a PASS). Fixed to show a size/byte-count message on pass.
- Session ended with a clean `LogExit: Exiting.` — no crash, no
  exception.

### 2026-08-24 (later same day) — second playtest, established save, live HTTP

This session used a save with real buildings (**10,114 buildables, 352
manufacturers, 1,265 connection points** — clearly not the fresh game
from the first session above; the earlier 0/0/0 result was consistent
with a genuinely empty save, not a bug). Two things got exercised for
the first time ever:

- **Item 5 (HTTP transport) — now fully confirmed, live and over the
  actual network.** With the game running, `python controller/live_check.py`
  connected from *outside* the game process to `http://127.0.0.1:51902/rpc`
  and got real responses for `world.resourceNodes` (628 nodes),
  `world.buildables` (10,114), `world.manufacturers` (352),
  `world.connections` (1,265), plus both negative-path checks
  (`UNKNOWN_METHOD`, `TARGET_NOT_FOUND`) — 6/6 passed. One bug found and
  fixed in the process, in the *test script*, not the mod:
  `urllib.request.urlopen` raises `HTTPError` on any non-2xx status
  instead of returning it, so the script was treating the mod's correct
  404 responses (with valid structured JSON error bodies) as connection
  failures. Fixed `rpc_call()` in `live_check.py` to catch `HTTPError`
  and read its body like a normal response. **The `netstat`
  loopback-binding check itself is still not done** — worth doing next.
- **A second real bug found, this time via the self-test's
  `FactoryConnectionTelemetry.reciprocity` check** (item 7): 435 of 1,265
  connection points had no matching reciprocal row. Investigated using
  the live connection (pulled real `world.connections` data and analyzed
  it in Python) rather than guessing — found every single unmatched
  `"Output"` row pointed at a `ConveyorBeltMk5`/`ConveyorBeltMk2` peer
  that had zero connection rows of its own. Root cause: conveyor
  belts/lifts (`AFGBuildableConveyorBase`) derive from `AFGBuildable`
  directly, not `AFGBuildableFactory`, so the original
  `TActorIterator<AFGBuildableFactory>` scan structurally could never
  find them. Fixed by adding a second iteration pass over
  `AFGBuildableConveyorBase` using its `GetConnection0()`/
  `GetConnection1()` accessors — see
  `docs/buildable-research.md`'s correction note. **Not yet re-verified**
  — needs another self-test run to confirm the reciprocity count drops
  to 0.
- Item 8's negative path re-confirmed again in this session
  (`TARGET_NOT_FOUND` for both write operations, now also confirmed
  reachable *over real HTTP*, not just in-process). Positive-path
  mutation testing on a disposable save is still the one thing from the
  original checklist that hasn't been attempted at all yet.

### 2026-08-24 (later still) — third playtest: F6 diagnosed as a missed hook, plus a third connection bug

The user reported "F6 has no obvious in-game effect" after a save-load
session. Log review showed something more specific than a hotkey bug:
**neither the self-test summary nor the hotkey-binding message appeared
at all** for that session, even though item 1's module-startup line was
present. The user then manually ran `DocMod.SelfTest` and `DocMod.Target`
from the console mid-session specifically to generate fresh log output
for diagnosis — both worked perfectly (self-test produced a normal
PASS/FAIL summary; `DocMod.Target` correctly reported the manufacturer
being looked at), proving the underlying self-test/hotkey/targeting logic
was never the problem.

Root cause: this session's map load happened via a save-load
(`ProcessServerTravel` → `LoadMap` with `loadgame=moved power` visible in
the log), and `FWorldDelegates::OnWorldInitializedActors` — the only
delegate driving the automatic self-test/hotkey setup — apparently never
fired for that specific load path, even though it had worked for a fresh
"New Game" load in the first playtest session above. **Fixed** by adding
`FCoreUObjectDelegates::PostLoadMapWithWorld` as a second, complementary
hook feeding the same `RunPerWorldSetup()` (de-duplicated via a
`LastSetupWorld` weak pointer, since both delegates can fire for the same
world). **Not yet re-verified against a real save-load** — needs another
playtest to confirm the self-test/hotkey messages now appear
automatically after loading a save, not just after a fresh game.

While investigating, the reciprocity self-test check (item 7) was
re-checked against this session's live data and found **still failing,
worse than before**: 920 of 6,791 connection points unmatched (up in
absolute count from 435/1,265, though the connection total grew
correctly from 1,265→6,791, confirming the earlier belt/lift fix was
active). Live analysis showed every unmatched peer's `buildableClass` was
`Build_ConveyorAttachmentMerger`/`Build_ConveyorAttachmentSplitter` (and
Lift/Smart variants) — a **third** sibling hierarchy
(`AFGBuildableConveyorAttachment`) neither existing enumeration pass
could reach (see `docs/buildable-research.md`'s second correction note).
Rather than add a third specific-class pass, `CollectFactoryConnectionTelemetry`
was redesigned around generic `AActor::GetComponents<UFGFactoryConnectionComponent>()`
discovery, which finds connection components regardless of which
`AFGBuildable` subclass owns them. **Not yet re-verified** — needs
another self-test run to confirm the reciprocity count drops to 0 this
time.

### 2026-08-24 (fourth playtest) — reciprocity fix confirmed, F6 root cause found, and a real security gap found and fixed

User launched a fresh session, tried F6 (no effect), then manually ran
`DocMod.SelfTest`/`DocMod.Target` from the console. Reviewing
`FactoryGame.log`:

- **Reciprocity fix (Bug A from the third playtest) — confirmed working.**
  `FactoryConnectionTelemetry.reciprocity` now reads **0 unmatched out of
  10,667 connection points** (up from 6,791 the prior session — more got
  built). The generic `GetComponents<UFGFactoryConnectionComponent>()`
  redesign is proven correct against real, larger live data.
- **F6/self-test root cause was not the dual-hook fix at all — it was
  never going to run in this build.** The log's `ExecutableName` line
  confirms the session runs `FactoryGameSteam-Win64-Shipping.exe`, a
  genuine Shipping build. `DocMod.cpp`'s automatic self-test hook and
  `DocModHotkey::SetupForWorld` are both wrapped in `#if
  !UE_BUILD_SHIPPING` (a deliberate choice, citing CLAUDE.md's "dev-only
  capabilities shouldn't linger into a shipped build") — that code
  doesn't exist at all in this binary, so the previous session's
  dual-hook fix (correct in itself, and still valuable for Development
  Editor PIE sessions) could never have fixed F6 here regardless. Console
  commands still work because `RegisterConsoleCommands()` sits outside
  that gate. **User decision: the hotkey isn't needed — prioritize
  driving progress through the RPC API directly instead of fixing/testing
  the hotkey further.** No code change made for this specifically; it's
  documented as expected behavior, not a bug.
- **First-ever positive-path write mutation test, live.** Used
  `world.targetedManufacturer` to find a real Constructor Mk1 the user
  was looking at (`Power Shard (1)` recipe, 250% clock, Standby), then
  called `world.setClockSpeed` on it — first with the same value (250%,
  proving the write path executes cleanly against a real target:
  `success:true`), then with a different value (200%) to test whether it
  actually takes effect. **Read-back still showed 250%,** consistent with
  the already-documented `SetPendingPotential`-takes-effect-next-cycle
  behavior (`docs/operations-protocol.md`) — the machine was in
  `Standby`, not actively cycling, so the pending change had no
  production cycle to apply on. Restored to 250% afterward. **Not a bug,
  but the positive-path test wasn't fully conclusive** — still don't have
  a confirmed case of a `clockSpeedPercent` read-back actually changing
  after a `setClockSpeed` call. Worth repeating against a machine that's
  actively `Producing`, where a cycle will complete during the test.
- **A real security gap found and fixed.** Ran the long-pending `netstat`
  loopback check for the first time ever — it failed:
  `0.0.0.0:51902 LISTENING`, not `127.0.0.1:51902`. The DocMod RPC API
  (including write operations) was reachable from the LAN, not just this
  machine, this entire time the mod has been in use. Root cause and fix
  documented in detail in
  [networking-research.md](networking-research.md)'s "Confirmed broken
  live" section: the loopback-forcing ini override lived only in this
  dev workspace's project-level config, which doesn't reach the
  Alpakit-packaged, Steam-deployed mod. Fixed with two layers: (1)
  `UDocModHttpServerSubsystem::HandleRpcRequest` now rejects any request
  whose `PeerAddress` isn't loopback, regardless of socket binding — this
  is the fix to trust; (2) added
  `Mods/GameFeatures/DocMod/Config/DefaultEngine.ini` with the same
  override, hypothesized to actually reach the packaged deploy via UE's
  plugin-config-merging mechanism — **unverified, may not work for a
  GameFeature plugin specifically.** **Next session must re-run
  `netstat -an | findstr 51902`** to check whether the socket itself is
  now loopback-only, and ideally test an actual request from another
  device on the LAN to confirm it gets `403 FORBIDDEN`.

### 2026-08-25 — placement-override fix confirmed, netstat re-checked, save/reload confirmed

Three follow-ups from the prior session, all resolved:

- **Placement-override fix confirmed live.** `DocMod.PlaceBuildingNearPlayer`
  (Constructor Mk1): ground-trace candidate `X=-54853.050 Y=160796.653
  Z=3337.826`, actual constructed location `X=-54853.103 Y=160796.700
  Z=3341.401` - matching to within a unit. Every pre-fix test had
  diverged by hundreds to thousands of units (up to ~4000/40m), so this
  is strong confirmation `AFGHologram::UpdateHologramPlacement()`
  re-asserted every poll tick genuinely overrides the build gun's own
  live-aim trace. See `docs/buildgun-driven-placement-research.md`'s
  "Confirmed live (2026-08-25)" note.
- **`netstat` re-checked - still `0.0.0.0:51902`.** The plugin-level ini
  fix from the prior session (item (2) above) does **not** work - root
  cause still unknown. Item (1), the application-layer `PeerAddress`
  check, is unaffected and remains the real protection. Still not
  tested: an actual cross-machine LAN request to confirm `403 FORBIDDEN`.
- **Save/reload survival and telemetry visibility confirmed.** The
  Constructor Mk1 built above survived a full save/exit/reload and
  appeared correctly in `world.buildables` at its exact built position
  (`X=-54853.1 Y=160796.7 Z=3341.4`, matching to the fractional unit) -
  confirms proper `AddBuildable` registration via the real
  `InternalConstructHologram` path, not a floating unregistered actor.
