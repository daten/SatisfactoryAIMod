# Satisfactory AI Interface — Claude Project Instructions

## Purpose

This repository contains an experimental interface intended to allow an external AI/planning system to observe and eventually control selected aspects of Satisfactory through an SML mod.

The long-term research objective is an autonomous Satisfactory industrial agent capable of planning and constructing factories and eventually progressing through the game.

The immediate objective is much narrower:

> Build a robust, well-defined Satisfactory/SML telemetry and control interface.

Do not confuse the interface layer with the AI itself.

---

# Current Development Environment

Operating system:

`Windows 10`

Satisfactory:

`Steam installation`

Satisfactory modding workspace:

```text
F:\Claude\SatisfactoryModLoader\
```

Current mod:

```text
F:\Claude\SatisfactoryModLoader\Mods\GameFeatures\AIMod\
```

Current native source:

```text
F:\Claude\SatisfactoryModLoader\Mods\GameFeatures\AIMod\Source\
```

The current mod was generated through the Satisfactory/SML tooling.

It already contains initial C# Build.cs and C++ templates.

The following have already been successfully tested:

- Satisfactory custom Unreal Engine installation
- SML Starter Project
- Game Feature mod creation
- Blueprint example mod
- C++ compilation
- project compilation through the current C++ toolchain
- Unreal Editor project loading
- Alpakit packaging/deployment
- loading and testing the mod in Satisfactory

Do not spend time recreating these prerequisites unless an actual problem is detected.

---

# Existing Mod Identity

The current mod reference is:

`AIMod`

Do **not** rename the mod, module, plugin, directory, or existing Game Feature assets unless explicitly instructed to perform a rename.

The name is temporary but functional.

Changing Unreal/SML module identity introduces unnecessary risk while the C++ interface is still being established.

---

# Development Style

The preferred development workflow is command-line oriented.

Claude Code is expected to:

- inspect files directly
- edit C++
- edit C#
- edit PowerShell
- edit Python
- run build commands
- inspect compiler output
- inspect logs
- write tests
- maintain documentation

Do not assume VS Code or Visual Studio is the primary development interface.

Visual Studio may exist as a compiler/debugger provider.

Unreal Editor remains necessary for:

- Unreal asset creation
- Blueprint work
- Game Feature configuration
- Alpakit
- packaging
- interactive testing

Do not unnecessarily move source-code work into Blueprint merely because Unreal Editor is available.

---

# Build System

Unreal Build Tool is authoritative for the Unreal/SML C++ code.

Do not introduce an independent CMake build for the mod.

Do not attempt to compile Unreal classes directly with `cl.exe`.

UHT-generated code and Unreal reflection must remain part of the normal Unreal build process.

The normal editor build target is expected to be:

```text
Configuration: Development Editor
Platform: Win64
Project: FactoryGame
```

Use the environment's actual build scripts/tooling after inspecting them.

Do not guess custom Unreal Engine paths if they can be discovered from the existing project/environment.

Alpakit is used for packaging/deploying the mod.

---

# Source of Truth

When determining how Satisfactory or SML works, use this priority:

1. Current installed FactoryGame/SML headers and source available in this workspace.
2. Current Satisfactory Modding documentation.
3. Existing working examples in the Starter Project.
4. Current upstream SML repositories/documentation if necessary.
5. General Unreal Engine documentation.
6. Memory or assumptions only as a last resort.

The installed headers are especially important because documentation can lag behind the currently installed game/SML version.

Never fabricate an Unreal or FactoryGame API because its name sounds plausible.

If a required API is uncertain:

1. Search the source tree.
2. Identify candidate classes.
3. inspect their declarations.
4. trace existing usages.
5. document findings.
6. implement only after sufficient evidence exists.

---

# Architectural Boundary

The system has four conceptual layers:

```text
Satisfactory
    ↓
SML C++ interface
    ↓
External controller
    ↓
Planner / AI
```

## SML C++ interface responsibilities

The mod may:

- inspect game state
- normalize game state
- expose telemetry
- validate commands
- perform explicitly supported game operations
- translate external commands into safe game-engine actions
- report results
- maintain protocol-facing object identity where needed

## External controller responsibilities

The external controller should handle:

- persistent normalized world state
- protocol communication
- recording telemetry
- experiment management
- graph construction
- state reconciliation
- retries
- telemetry history

## Deterministic solver responsibilities

Deterministic code should handle:

- production arithmetic
- recipe dependency calculations
- graph traversal
- optimization
- resource allocation
- geometry where deterministic geometry is appropriate
- constraint solving

## LLM responsibilities

The LLM should primarily handle:

- high-level goal interpretation
- strategic planning
- decomposing objectives
- deciding between valid plans
- diagnosis of unusual situations
- replanning
- selecting optimization objectives
- explaining decisions

Do not use an LLM to perform large volumes of arithmetic that can be handled reliably by deterministic code.

---

# Keep the Unreal Mod Small

Do not place the production solver inside Unreal.

Do not embed an LLM runtime inside Unreal.

Do not embed Python into the Unreal process without explicit architectural approval.

Do not place strategic AI logic in Blueprint.

Do not make the SML mod responsible for long-term experiment state unless game integration specifically requires it.

The mod should behave primarily like a controlled adapter:

```text
FactoryGame objects
        ↕
SML interface
        ↕
normalized protocol
```

---

# Safety and Stability Boundary

The external AI must never receive unrestricted access to arbitrary Unreal functionality.

Do not create generic interfaces such as:

```text
CallFunctionByName(...)
SetArbitraryProperty(...)
SpawnAnyUClass(...)
WriteMemory(...)
ExecuteConsoleCommand(...)
```

unless explicitly approved for a specific debugging purpose.

Prefer explicit operations:

```text
GetResourceNodes
GetBuildings
GetMachineStatus
SetMachineRecipe
PlaceBuilding
ConnectConveyor
ConnectPower
```

Each write operation should:

1. validate input
2. verify target identity
3. verify target type
4. verify requested operation is permitted
5. invoke the game operation
6. report actual success/failure

The mod is a security/stability boundary between an AI-generated command stream and the game process.

Treat all external commands as untrusted input.

---

# No Direct Memory Manipulation

Do not use:

- hard-coded offsets
- pointer scanning
- DLL injection
- binary patches
- arbitrary process-memory reads/writes
- undocumented structure overlays

unless the project direction explicitly changes.

The entire reason for using SML is to operate through Unreal and FactoryGame's object model rather than reverse-engineering runtime memory.

---

# Unreal Object Rules

Do not expose raw:

- pointers
- memory addresses
- `UObject*`
- `AActor*`
- component pointers

through the external protocol.

Convert them into normalized values or stable interface IDs.

Before storing an Unreal object reference across frames/events, verify that the chosen storage method properly participates in Unreal lifetime/garbage-collection rules.

Never assume an actor remains valid.

Use appropriate Unreal validity checks.

Be particularly careful with:

- destroyed actors
- world teardown
- save/load
- map transition
- Game Feature activation/deactivation
- multiplayer/server ownership
- asynchronous operations

---

# Threading

Assume game-object access belongs on the Unreal game thread unless the relevant API explicitly guarantees otherwise.

Networking, parsing, or expensive calculations must not block the game thread.

If a transport thread receives a command requiring game-world access, marshal that operation appropriately onto the game thread.

Do not call arbitrary FactoryGame/Unreal APIs from worker threads.

Document any non-obvious threading requirement discovered during development.

---

# Logging

Use a dedicated Unreal C++ log category for this project.

Preferred conceptual name:

```text
LogAIModAI
```

Use appropriate levels:

- `Verbose` for detailed diagnostics
- `Display` or `Log` for normal lifecycle events
- `Warning` for recoverable abnormal states
- `Error` for failed operations
- avoid `Fatal` except for genuinely unrecoverable programmer errors

Do not use:

- `printf`
- `std::cout`
- arbitrary text files
- Windows console popups

unless explicitly needed for a temporary isolated diagnostic.

Runtime logs should be inspectable in:

```text
%LOCALAPPDATA%\FactoryGame\Saved\Logs\FactoryGame.log
```

---

# Error Handling

Do not silently ignore failures.

Prefer structured errors.

Conceptually:

```json
{
  "success": false,
  "error": {
    "code": "INVALID_BUILDING_ID",
    "message": "No active building exists with the supplied identifier."
  }
}
```

Expected failures should not crash the game.

Assertions should be reserved for programmer invariants, not malformed external requests.

---

# Protocol Design

The external interface should be versioned from the beginning.

Conceptually:

```json
{
  "protocolVersion": 1,
  "requestId": "123",
  "method": "world.resourceNodes"
}
```

Do not expose Unreal serialization directly.

Protocol structures must be independent of implementation classes.

Prefer:

```text
FactoryGame object
    ↓
normalized internal DTO
    ↓
protocol serializer
```

rather than:

```text
FactoryGame UObject
    ↓
JSON reflection dump
```

The external API should remain reasonably stable even if FactoryGame internals change.

---

# Networking

Do not implement network transport until read-only telemetry and JSON serialization have been proven locally.

When networking is implemented:

- bind only to loopback by default
- do not expose the API to the LAN by default
- define message-size limits
- validate JSON/schema
- reject unknown operations
- reject invalid argument types
- use request IDs
- return structured errors
- avoid game-thread blocking
- include protocol versioning

Do not implement authentication initially if the service is strictly loopback-only, but design the transport so remote access is not accidentally enabled.

---

# Third-Party Dependencies

Avoid new native dependencies in the Unreal module unless they provide substantial value.

Prefer Unreal facilities for:

- strings
- containers
- JSON
- sockets/networking
- async execution
- logging
- filesystem access

External Python code may use appropriate Python libraries more freely.

Any new native dependency must be justified before being introduced.

---

# Blueprint Boundary

Blueprints are acceptable for:

- simple UI
- test harnesses
- triggering/debugging C++ functions
- visual Unreal configuration
- small pieces where Blueprint is clearly more convenient

Do not implement complex protocol handling, production planning, graph algorithms, or AI logic in Blueprint.

Existing Blueprint content must continue to function unless explicitly being replaced.

---

# C++ ↔ Blueprint Exposure

Expose C++ functions to Blueprint only when there is a clear use case.

Do not mark every function `BlueprintCallable`.

Keep internal implementation details private.

Blueprint exposure is an interface, not a default.

---

# Resource Discovery

The first meaningful world-inspection feature is resource-node enumeration.

For each resource node, eventually capture:

```text
ID
resource type
purity
position X/Y/Z
occupied state
```

Additional information may be added where safely available.

Do not infer node characteristics from position or visual assets if the actual object exposes them.

---

# Stable Identifiers

The external controller needs stable identifiers.

Do not use pointer values.

Before inventing identifiers, research whether FactoryGame already exposes suitable identifiers.

Document:

- uniqueness
- persistence
- save/load behavior
- destruction behavior
- recreation behavior

If a project-specific identity system becomes necessary, design it deliberately.

---

# Read Before Write

Development order is deliberately read-first.

Do not implement factory construction while world telemetry remains incomplete or unreliable.

Preferred progression:

```text
read resource nodes
read buildings
read recipes
read inventories
read factory connectivity
read progression
    ↓
model world externally
    ↓
only then mutate game state
```

This lets us understand how Satisfactory represents the world before attempting to create it.

---

# First Write Operations

When mutation work eventually begins, start with operations against existing objects.

Examples:

```text
SetRecipe
SetClockRate
```

Only after these are validated should work proceed to:

```text
PlaceBuilding
ConnectConveyor
ConnectPipe
ConnectPower
```

Do not jump directly from telemetry to arbitrary actor spawning.

---

# Building Placement

When construction is implemented, prefer Satisfactory's normal construction/buildable systems.

Do not simply `SpawnActor` a factory building and assume it is valid.

A valid Satisfactory buildable may require:

- construction metadata
- ownership
- replication
- save registration
- connection components
- subsystem registration
- initialization
- recipe state
- hologram/build validation

Research the existing construction code before implementing placement.

---

# Multiplayer

Multiplayer support is not an initial requirement.

The primary target is:

```text
single-player/local Satisfactory session
```

However, do not deliberately create architecture that assumes every operation is valid on both client and server.

When relevant, document whether an API:

- must execute on server
- executes on client
- is replicated
- is authority-only

Do not solve multiplayer unless needed, but do not conceal multiplayer implications.

---

# Save Compatibility

Prefer normal Satisfactory save mechanisms.

Do not independently rewrite `.sav` files from inside this mod.

Any state the mod adds to the save must follow appropriate SML/Unreal save mechanisms.

The eventual AI controller should be able to restart and reconstruct its world model from telemetry rather than requiring fragile in-process state.

---

# Performance

Do not scan every UObject every frame.

Prefer:

- explicit subsystems
- known actor classes
- event-driven updates
- periodic low-frequency polling where necessary

Initial debug scans may be brute-force if needed to understand the game, but production implementation should avoid unnecessarily expensive world scans.

Profile before performing large optimization work.

---

# Determinism and Reproducibility

The eventual system is an experiment.

Prefer behavior that can be:

- logged
- replayed
- measured
- compared

Important operations should eventually have:

- timestamp
- request ID
- objective/context
- command
- result
- resulting object ID
- failure reason

Avoid opaque autonomous behavior inside the SML mod.

---

# Testing Strategy

There are three testing layers.

## Layer 1 — Native compile tests

Verify that C++ and UHT compile.

## Layer 2 — Unreal/SML integration tests

Run through Unreal Editor and/or packaged mod.

Verify:

- module loads
- Game Feature activates
- functions behave correctly

## Layer 3 — Actual Satisfactory runtime tests

Launch the Steam game with the deployed mod.

Verify real FactoryGame behavior.

A successful editor compile does not prove the runtime feature works.

---

# External Test Fixtures

When telemetry is developed, record representative outputs in:

```text
tests/fixtures/
```

Examples:

```text
resource_nodes.json
buildings.json
factory_graph.json
```

External Python tests should use these fixtures so that controller development does not require starting Satisfactory.

Do not commit personal save files unless intentionally selected as test fixtures.

---

# Documentation

Document discoveries that future Claude sessions would otherwise need to rediscover.

Use:

```text
docs/
```

Important topics include:

```text
current-environment.md
build.md
architecture.md
resource-node-research.md
object-identifiers.md
factorygame-api-notes.md
protocol.md
threading.md
```

When a difficult Satisfactory/Unreal API behavior is discovered, document it immediately.

Especially record rules such as:

```text
Do not call X before Y initializes.
Object Z exists only on the server.
Function A looks correct but fails during save load.
```

This accumulated project knowledge is valuable.

---

# Comments

Do not fill source with comments explaining obvious syntax.

Comments should primarily explain:

- Unreal lifecycle constraints
- FactoryGame peculiarities
- threading requirements
- ownership rules
- non-obvious API behavior
- architectural reasons
- dangerous assumptions

Prefer documenting *why* over *what*.

---

# Generated Files

Do not manually edit Unreal-generated files.

Do not commit unnecessary build output.

Respect the existing Unreal/SML `.gitignore`.

Typical generated artifacts should remain generated unless the project already intentionally versions them.

Before changing ignore rules, inspect the current Starter Project conventions.

---

# Git Discipline

Before large changes:

1. inspect `git status`
2. understand existing modifications
3. do not overwrite unrelated user work

Prefer small logical commits when asked to commit.

Do not commit automatically unless the user requests or the current task explicitly authorizes commits.

Never perform destructive Git operations such as:

```text
reset --hard
clean -fd
force push
```

without explicit permission.

---

# Refactoring

Do not perform opportunistic repo-wide refactors during exploratory work.

When investigating an API:

1. make the smallest useful change
2. compile
3. validate
4. then refactor once behavior is understood

Unreal compile cycles are expensive; preserve known-working states.

---

# Current Near-Term Milestone

The current milestone is:

> Export trustworthy read-only Satisfactory resource-node information from C++ in a normalized JSON form.

The required progression is:

```text
1. Inspect existing AIMod source.
2. Verify repeatable C++ build.
3. Establish dedicated logging.
4. Expose minimal C++ function to Blueprint.
5. Verify it in the existing Blueprint test mod.
6. Research resource-node APIs in installed headers.
7. Enumerate resource nodes.
8. Normalize resource-node data.
9. Serialize it to JSON.
10. Validate captured JSON with an external Python test.
```

Do not implement networking before this milestone is complete.

Do not implement game-state mutation before this milestone is complete.

---

# Definition of Done for a Coding Task

Before declaring a native-code task complete:

1. source changes are internally consistent
2. required module dependencies are present
3. UHT succeeds
4. Development Editor compilation succeeds
5. relevant automated tests pass
6. runtime verification steps are described
7. logs/errors are checked when runtime testing is available
8. documentation is updated if a non-obvious discovery was made

If Unreal Editor or Satisfactory must be launched manually by the user to complete validation, explicitly state exactly what should be tested and what result is expected.

Never claim runtime success merely because compilation succeeded.

---

# Claude Behavior for Uncertain APIs

When uncertain:

Do not guess.

Instead:

```text
Search → inspect declarations → inspect usages → form hypothesis → make small test → compile → verify
```

If multiple possible Satisfactory APIs exist, explain the evidence for the selected one in the relevant research note.

Compiler errors are useful evidence.

Runtime logs are useful evidence.

Treat exploratory development as investigation rather than trying to produce a large implementation in one pass.

---

# Long-Term Success Criteria

Eventually, the interface should make it possible for an external controller to express operations similar to:

```text
GetWorldState()
GetResourceNodes()
GetBuildings()
GetFactoryGraph()
GetInventory()
GetProgression()
GetAvailableRecipes()

PlaceBuilding(...)
DeleteBuilding(...)
SetRecipe(...)
SetClock(...)
ConnectConveyor(...)
ConnectPipe(...)
ConnectPower(...)
```

without exposing arbitrary Unreal execution.

Above this interface, independent software should eventually be capable of:

```text
observe
plan
optimize
build
measure
diagnose
correct
progress
```

until a Satisfactory end-game objective is completed.

That is the long-term direction.

The present priority is building a reliable foundation rather than attempting autonomous gameplay prematurely.