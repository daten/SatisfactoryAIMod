# Satisfactory AI Interface — Agent Instructions

Canonical instructions for any AI agent working in this repository
(Claude Code, Codex, and others). `CLAUDE.md` points here; keep this file
as the single source of truth.

## Purpose

This repository is a working interface that lets an external AI/planning
system observe and control selected aspects of Satisfactory through an SML
mod. The mod (`AIMod`, published as `SatisfactoryAIMod`) is a **bridge, not
the AI**: it reads FactoryGame world state and exposes a set of explicit,
validated write operations over a local RPC protocol. Planning,
optimization, and decision-making live *outside* the mod, in an external
controller. Never conflate the interface layer with the AI itself.

See `PLAN.md` for the phase-by-phase roadmap and `README.md` for the
user-facing overview.

## Current state

The bottom-up interface is built and live-tested: logging, telemetry, a
loopback JSON-RPC transport, stable identifiers, full game-state mutation,
and factory/logistics construction. The interface is 120+ `world.*` methods,
self-describing via `world.help` (+ `docs/rpc-catalog.md`). Real factories
(a copper line, a Heavy Modular Frame factory) and rail/truck/drone
proofs-of-concept have been built entirely through it.

Not built yet, and deliberately kept *out* of the mod: the deterministic
production/optimization solver and the closed-loop observe→plan→build→verify
agent (PLAN.md's upper phases). The Python side (`controller/satisfactory_ai`)
is a deterministic *toolkit*, not a solver.

The read-first, prove-each-layer discipline still applies to any new
capability.

## Environment

- OS: Windows 10. Satisfactory: Steam installation.
- Workspace: this repository (the SML monorepo fork; the mod lives at
  `Mods/GameFeatures/AIMod/`, native source under its `Source/`).
- The prerequisites are already set up and working — the custom Satisfactory
  Unreal Engine, the SML starter project, Game Feature mod creation, the C++
  toolchain, Unreal Editor project loading, and Alpakit packaging/deploy. Do
  not recreate these unless an actual problem is detected.
- Discover build scripts/tooling and engine paths from the existing
  project/environment; do not guess absolute paths.

## Mod identity

The mod reference is `AIMod` (module, plugin, `ConfigId.ModReference`). It is
baked into save files and is the published mod ID, so it is effectively
permanent — **do not rename the mod, module, plugin, directory, or Game
Feature assets** unless explicitly instructed. The public display name is
`SatisfactoryAIMod` (`.uplugin` `FriendlyName`), which is cosmetic and may
change freely.

## Development style

Command-line oriented. The agent inspects files directly; edits C++, C#,
PowerShell, and Python; runs build commands; inspects compiler output and
logs; writes tests; and maintains documentation. Do not assume VS Code or
Visual Studio is the primary interface (Visual Studio may serve as a
compiler/debugger provider).

Unreal Editor remains necessary for Unreal asset creation, Blueprint work,
Game Feature configuration, Alpakit, packaging, and interactive testing. Do
not move source-code work into Blueprint merely because the editor is open.

## Build system

Unreal Build Tool is authoritative for the Unreal/SML C++ code. Do not
introduce an independent CMake build, and do not compile Unreal classes
directly with `cl.exe` — UHT-generated code and Unreal reflection must stay
part of the normal Unreal build. The editor build target is Development
Editor / Win64 / FactoryGame (via the repo's build script). Alpakit packages
and deploys the mod.

## Source of truth

When determining how Satisfactory or SML works, prefer, in order:

1. Installed FactoryGame/SML headers and source in this workspace.
2. Current Satisfactory Modding documentation.
3. Working examples in the SML tooling.
4. Upstream SML repositories/documentation.
5. General Unreal Engine documentation.
6. Memory or assumptions — last resort only.

Installed headers matter most because documentation lags the installed
game/SML version. **Never fabricate an Unreal or FactoryGame API because its
name sounds plausible.** Many FactoryGame `.cpp` bodies in this workspace are
link stubs — the real implementation is in the shipping binary, so header
declarations plus live testing are the evidence, not the stub bodies. If an
API is uncertain: search the tree, identify candidates, inspect declarations,
trace usages, then implement only once the evidence supports it.

## Architecture

Four conceptual layers: **Satisfactory → SML C++ interface → external
controller → planner/AI.**

- **SML C++ interface (the mod):** inspect and normalize game state, expose
  telemetry, validate commands, perform explicitly supported operations,
  translate external commands into safe engine actions, report results,
  maintain protocol-facing object identity.
- **External controller:** persistent normalized world state, protocol
  communication, telemetry recording/history, experiment management, graph
  construction, reconciliation, retries.
- **Deterministic solver:** production arithmetic, recipe dependency
  calculations, graph traversal, optimization, allocation, geometry,
  constraint solving.
- **LLM:** high-level goal interpretation, strategic planning, decomposing
  objectives, choosing among valid plans, diagnosing unusual situations,
  replanning, selecting objectives, explaining decisions. Do not use an LLM
  for large volumes of arithmetic deterministic code can do reliably.

### Keep the Unreal mod small

Do not put the production solver, an LLM runtime, embedded Python, strategic
AI logic, or long-term experiment state inside the Unreal mod (unless game
integration specifically requires the last one). The mod is a controlled
adapter between FactoryGame objects and the normalized protocol.

## Safety and stability boundary

The external AI must never receive unrestricted access to arbitrary Unreal
functionality. Do not create generic interfaces such as
`CallFunctionByName`, `SetArbitraryProperty`, `SpawnAnyUClass`,
`WriteMemory`, or `ExecuteConsoleCommand` unless explicitly approved for a
specific debugging purpose. Prefer explicit operations (`GetResourceNodes`,
`SetMachineRecipe`, `PlaceBuilding`, `ConnectConveyor`, …).

Each write operation must: validate input, verify target identity, verify
target type, verify the operation is permitted, invoke the game operation,
and report actual success/failure. Treat all external commands as untrusted
input — the mod is a security/stability boundary between an AI-generated
command stream and the game process.

Cheat-capable operations with no legitimate in-game equivalent (free item
injection, achievement re-fire, event forcing, world/entity manipulation)
are gated behind the off-by-default **Allow Creative Features** player
setting and flagged in `world.help`. An external caller can never enable
that setting itself.

### No direct memory manipulation

No hard-coded offsets, pointer scanning, DLL injection, binary patches,
arbitrary process-memory access, or undocumented structure overlays. The
whole point of using SML is to operate through Unreal/FactoryGame's object
model, not to reverse-engineer runtime memory.

### Unreal object rules

Do not expose raw pointers, memory addresses, `UObject*`, `AActor*`, or
component pointers through the protocol — convert them to normalized values
or stable interface IDs. Before storing an Unreal object reference across
frames/events, ensure the storage participates in Unreal
lifetime/GC rules; never assume an actor stays valid; use proper validity
checks. Be especially careful with destroyed actors, world teardown,
save/load, map transitions, Game Feature activation/deactivation,
multiplayer/server ownership, and async operations.

### Stable identifiers

The external controller needs stable identifiers; never use pointer values.
Prefer identifiers FactoryGame already exposes; document their uniqueness,
persistence, and save/load/destruction/recreation behavior. Design a
project-specific identity system deliberately only if one becomes necessary.

## Threading

Assume game-object access belongs on the Unreal game thread unless an API
explicitly guarantees otherwise. Networking, parsing, and expensive
calculations must not block the game thread. A transport thread that
receives a command needing game-world access must marshal it onto the game
thread. Do not call arbitrary FactoryGame/Unreal APIs from worker threads.
Document any non-obvious threading requirement discovered.

## Logging

Use the dedicated log category `LogAIModAI`. Levels: `Verbose` for detailed
diagnostics, `Display`/`Log` for normal lifecycle events, `Warning` for
recoverable abnormal states, `Error` for failed operations; avoid `Fatal`
except for genuinely unrecoverable programmer errors. Do not use `printf`,
`std::cout`, arbitrary text files, or console popups except for a temporary
isolated diagnostic. Runtime logs:
`%LOCALAPPDATA%\FactoryGame\Saved\Logs\FactoryGame.log`.

## Error handling

Do not silently ignore failures. Return structured errors, e.g.
`{"success": false, "error": {"code": "INVALID_BUILDING_ID", "message": "…"}}`.
Expected failures must not crash the game. Reserve assertions for programmer
invariants, not malformed external requests.

## Protocol design

The external interface is versioned. Requests carry `protocolVersion`, a
`requestId`, and a `method`. Do not expose Unreal serialization directly;
protocol structures are independent of implementation classes (FactoryGame
object → normalized DTO → protocol serializer, never a raw JSON reflection
dump). The API should stay reasonably stable even if FactoryGame internals
change.

**Self-describing interface (`world.help`):** the RPC must stay discoverable
by an agent that does not have the mod source. `world.help` returns a live
catalog of every `world.*` method (name, category, params with
name/type/required, one-line summary), generated from the dispatcher by
`controller/tools/gen_rpc_catalog.py`, which also emits the embedded catalog
(`AIModRpcCatalog.gen.cpp`) and `docs/rpc-catalog.md`. Keep it current (see
Definition of Done); `gen_rpc_catalog.py --check` fails when it is stale and
is wired into CI. `RPC_REFERENCE.md` is a richer hand-written companion;
operational guides live in `docs/factory-placement-guide.md` and
`docs/vehicle-placement-guide.md`.

## Networking

The loopback JSON-RPC HTTP server (`AIModHttpServerSubsystem`) must keep
meeting these standing constraints: bind to loopback only by default (the
socket bind is forced via `GConfig` at startup, not just an app-layer
check); do not expose the API to the LAN by default (a player-only,
off-by-default setting gates remote access); enforce message-size limits;
validate JSON/schema; reject unknown methods and invalid argument types; use
request IDs; return structured errors; never block the game thread; and
include protocol versioning. Authentication is unnecessary while strictly
loopback-only, but the transport must not enable remote access accidentally.

## Third-party dependencies

Avoid new native dependencies in the Unreal module unless they add
substantial value; prefer Unreal facilities for strings, containers, JSON,
sockets, async, logging, and filesystem access. External Python code may use
appropriate libraries more freely. Justify any new native dependency before
adding it.

## Blueprint boundary

Blueprints are fine for simple UI, test harnesses, triggering/debugging C++
functions, visual configuration, and small conveniences. Do not implement
protocol handling, production planning, graph algorithms, or AI logic in
Blueprint. Existing Blueprint content must keep working unless explicitly
replaced. Expose C++ to Blueprint only with a clear use case — keep internal
details private; Blueprint exposure is an interface, not a default.

## Read before write

Observe a capability reliably before mutating it: read the relevant world
state (nodes, buildings, recipes, inventories, connectivity, progression)
and model it before constructing or changing it. This is an enduring
principle for every new capability, not just the original build order.

## Building placement

Prefer Satisfactory's normal construction/buildable systems — do not just
`SpawnActor` a building and assume it is valid. A valid buildable may require
construction metadata, ownership, replication, save registration, connection
components, subsystem registration, initialization, recipe state, and
hologram/build validation. Research the existing construction code before
implementing new placement.

## Multiplayer

The primary target is a single-player/local session; multiplayer is not a
requirement and is largely untested. Do not architect as if every operation
is valid on both client and server. When relevant, document whether an API
must run on the server, runs on the client, is replicated, or is
authority-only. Do not solve multiplayer unless needed, but do not conceal
its implications.

## Save compatibility

Prefer normal Satisfactory save mechanisms; do not rewrite `.sav` files from
inside the mod. Any state the mod adds to a save must use appropriate
SML/Unreal save mechanisms. The external controller should be able to
restart and reconstruct its world model from telemetry rather than relying
on fragile in-process state.

## Performance

Do not scan every UObject every frame. Prefer explicit subsystems, known
actor classes, event-driven updates, and low-frequency polling where
necessary. Brute-force debug scans are acceptable while investigating, but
production code must avoid unnecessarily expensive world scans. Profile
before large optimization work.

## Determinism and reproducibility

Prefer behavior that can be logged, replayed, measured, and compared.
Important operations should be traceable (timestamp, request ID,
objective/context, command, result, resulting object ID, failure reason).
Avoid opaque autonomous behavior inside the mod.

## Testing

Three layers, none a substitute for the next:

1. **Native compile** — C++ and UHT compile.
2. **Unreal/SML integration** — module loads, Game Feature activates,
   functions behave (editor and/or packaged mod).
3. **Satisfactory runtime** — launch the Steam game with the deployed mod
   and verify real FactoryGame behavior.

A successful compile does not prove the runtime feature works. Record
representative telemetry outputs as fixtures under `tests/fixtures/` so
external Python tests run without launching Satisfactory. Do not commit
personal save files unless intentionally chosen as fixtures.

## Documentation

Record discoveries a future session would otherwise have to rediscover, in
`docs/`. Immediately document difficult Satisfactory/Unreal API behavior —
especially ordering constraints ("do not call X before Y initializes"),
server-only objects, and functions that look correct but fail during save
load. This accumulated knowledge is valuable; land it in `docs/`, the
`controller/satisfactory_ai` toolkit, and code comments rather than only in
session memory.

## Comments

Do not comment obvious syntax. Comments should explain *why*: Unreal
lifecycle constraints, FactoryGame peculiarities, threading requirements,
ownership rules, non-obvious API behavior, architectural reasons, and
dangerous assumptions.

## Generated files and git

Do not manually edit Unreal-generated files or commit unnecessary build
output; respect the existing `.gitignore`. Before large changes, inspect
`git status` and understand existing modifications — do not overwrite
unrelated work. Prefer small logical commits. Never run destructive git
operations (`reset --hard`, `clean -fd`, force push) without explicit
permission.

## Refactoring

Do not do opportunistic repo-wide refactors during exploratory work. When
investigating an API: make the smallest useful change, compile, validate,
then refactor once behavior is understood. Unreal compile cycles are
expensive — preserve known-working states.

## Definition of Done (coding task)

Before declaring a native-code task complete:

1. Source changes are internally consistent.
2. Required module dependencies are present.
3. UHT succeeds.
4. Development Editor compilation succeeds.
5. Relevant automated tests pass.
6. Runtime verification steps are described.
7. Logs/errors are checked when runtime testing is available.
8. Documentation is updated for any non-obvious discovery.
9. **If you added, removed, or changed the params of a `world.*` method**,
   re-run `python controller/tools/gen_rpc_catalog.py` (add a one-line
   summary for any new method) and rebuild, so `world.help` and
   `docs/rpc-catalog.md` stay in sync. `gen_rpc_catalog.py --check` (run in
   CI) fails when they are stale.

If the user must launch Unreal Editor or Satisfactory to finish validation,
state exactly what to test and the expected result. **Never claim runtime
success merely because compilation succeeded.**

## Behavior for uncertain APIs

Do not guess. Instead: search → inspect declarations → inspect usages → form
a hypothesis → make a small test → compile → verify. When several candidate
APIs exist, record the evidence for the chosen one in the relevant research
note. Compiler errors and runtime logs are useful evidence. Treat
exploratory development as investigation, not a single large speculative
implementation.

## Long-term direction

Eventually, independent software above this interface should be able to
observe, plan, optimize, build, measure, diagnose, correct, and progress
toward a Satisfactory end-game objective — without exposing arbitrary Unreal
execution. The present priority remains a reliable foundation over premature
autonomous gameplay.
