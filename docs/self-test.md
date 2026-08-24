# Automatic self-test

Runs every time a game world finishes loading — Editor Play-In-Editor,
standalone, or a packaged build — with no manual steps, no Blueprint
graph to build, and no HTTP calls to make.

## Why this exists

Working through
[manual-verification.md](manual-verification.md) by hand for every new
piece of functionality doesn't scale, and it means regressions only get
caught if someone happens to re-test the specific thing that broke. This
closes that gap for everything that's safe to check automatically, on
every launch, including against a real save.

It's trustworthy specifically because of what
[factorygame-binary-provenance.md](factorygame-binary-provenance.md)
established: the actual `FactoryGame` logic running in-process is Coffee
Stain's real, precompiled implementation, not a stub — so a check that
runs inside a loaded world is exercising production game logic, not
placeholder code.

## What it checks

`Mods/GameFeatures/DocMod/Source/DocMod/Private/DocModSelfTest.cpp`,
called from `FDocModModule::OnWorldInitializedActors`
(`Mods/GameFeatures/DocMod/Source/DocMod/Private/DocMod.cpp`) whenever
`FWorldDelegates::OnWorldInitializedActors` fires for a real game world
(`World->IsGameWorld()` — menu/editor-preview worlds are skipped):

- `GetInterfaceVersion` returns exactly `"0.1.0"`.
- Resource node / buildable / manufacturer / factory-connection
  telemetry: doesn't crash, every returned record is well-formed
  (non-empty ids/classes, enum-like string fields — purity, production
  status — are one of the known values), and every JSON serialization
  round-trips through a real `FJsonSerializer::Deserialize` parse.
- **Factory connection reciprocity** — a genuine data-integrity check,
  not just "didn't crash": every connected `"Output"` connection row
  should have a matching `"Input"` row on its peer pointing back (see
  [telemetry-protocol.md](telemetry-protocol.md)'s "connections"
  section). If these ever disagree, something's wrong with either the
  collection logic or an assumption about how FactoryGame represents
  connections.
- `SetManufacturerClockSpeed`/`SetManufacturerRecipe` **validation-path
  only**: calling them with a nonsense buildable id correctly returns
  `TARGET_NOT_FOUND` rather than crashing or silently succeeding.

## What it deliberately does NOT check

- **Positive-path mutation.** The self-test never calls
  `SetManufacturerClockSpeed`/`SetManufacturerRecipe` with a *real*
  target and asserts the game state actually changed correctly — doing
  that automatically, on every launch, against whatever save happens to
  be loaded, would mean the self-test itself mutates your game every time
  you play. That still needs deliberate manual testing on a disposable
  save — see manual-verification.md item 8 and
  [operations-protocol.md](operations-protocol.md).
- **Value plausibility for a specific map.** The self-test can confirm a
  resource node's purity is one of `Impure`/`Normal`/`Pure`, but it has
  no way to know whether *this particular* node's purity matches what
  you can see in-game — that needs a human looking at both.
- **Network reachability/binding.** It runs in-process, so it can't
  observe from the outside whether the HTTP server is genuinely bound to
  `127.0.0.1` only vs. `0.0.0.0` — that's an OS-level socket fact
  checked via `netstat`, covered in manual-verification.md item 5.
- **Building placement** — not implemented yet (Phase 13 is still at the
  research stage, see
  [building-placement-research.md](building-placement-research.md)).

## Reading the output

Look for `LogDocModAI` output bracketed by `=====` lines:

```
LogDocModAI: Display: ===== DocMod self-test: 12 passed, 0 failed (of 12) =====
LogDocModAI: Display:   [PASS] GetInterfaceVersion - expected "0.1.0", got "0.1.0"
LogDocModAI: Display:   [PASS] ResourceNodeTelemetry.shape - 631 node(s)
LogDocModAI: Display:   [PASS] ResourceNodeTelemetry.json - 118392 bytes
LogDocModAI: Display:   [PASS] BuildableTelemetry.shape - 83 buildable(s)
LogDocModAI: Display:   [PASS] BuildableTelemetry.json - 6104 bytes
LogDocModAI: Display:   [PASS] ManufacturerTelemetry.shape - 12 manufacturer(s)
LogDocModAI: Display:   [PASS] ManufacturerTelemetry.json - 2841 bytes
LogDocModAI: Display:   [PASS] FactoryConnectionTelemetry.shape - 156 connection point(s)
LogDocModAI: Display:   [PASS] FactoryConnectionTelemetry.reciprocity - 0 Output connection(s) with no matching Input row on the peer
LogDocModAI: Display:   [PASS] FactoryConnectionTelemetry.json - 9210 bytes
LogDocModAI: Display:   [PASS] SetManufacturerClockSpeed.rejectsUnknownTarget - success=false code="TARGET_NOT_FOUND"
LogDocModAI: Display:   [PASS] SetManufacturerRecipe.rejectsUnknownTarget - success=false code="TARGET_NOT_FOUND"
LogDocModAI: Display: ============================================================
```

A `[FAIL]` line also logs at `Error` level individually (so it's visible
even if you're filtering the log by severity, not just scrolling for the
summary block).

## Proven in practice, not just in theory

The first real run against an actual launched game (2026-08-24,
`GameLevel01`) found a real bug on the first try: `ResourceNodeTelemetry.shape`
failed because `purity` was `"<Bold>(Normal)</>"` (Slate rich-text UI
markup) instead of `"Normal"` — `GetResourcePurityText()` looked like a
plain accessor but its own doc comment says "For UI." Fixed to use the
raw `GetResourcePurity()` enum instead, mapped manually. That same run
also surfaced two lower-stakes issues worth knowing about even though
they wouldn't have failed a check: JSON output defaulted to
pretty-printed (turning one 631-node `resourceNodes` call into ~8,200 log
lines — switched to `TCondensedJsonPrintPolicy`), and the `*.json` checks'
detail text was hardcoded to a failure message regardless of actual
pass/fail (harmless, just confusing to read — fixed to depend on the
result). See `docs/manual-verification.md`'s Confirmed section for the
full writeup. This is exactly why the self-test exists — all three were
found without anyone hand-testing anything, just by having the checks run
automatically the first time a real save loaded.

## Extending it

Add a `Check*()` function to `DocModSelfTest.cpp` and call it from
`RunAll()` whenever new DocMod functionality is added, rather than only
adding a manual-verification.md entry for it. Keep new checks within the
same safety boundary as the existing ones: read-only assertions or
negative/validation-path checks only — never a positive-path mutation
against real game state.

## Why it's compiled out of Shipping builds

`FDocModModule`'s binding of `FWorldDelegates::OnWorldInitializedActors`
is wrapped in `#if !UE_BUILD_SHIPPING` (`DocMod.cpp`/`DocMod.h`) — the
self-test code doesn't exist at all in a Shipping build, not merely
disabled at runtime. This is a development-time convenience; it has no
reason to run for players of a released mod, and per CLAUDE.md's
Safety and Stability Boundary, dev-only capabilities shouldn't linger
into a shipped build even in dormant form.
