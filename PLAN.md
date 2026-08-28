# Satisfactory AI Interface — Development Plan

## Project Objective

Build a Satisfactory Mod Loader (SML) mod that exposes a controlled, machine-readable interface between a running Satisfactory game and an external AI/planning system.

The long-term objective is to support an autonomous industrial-planning agent capable of:

1. Observing the current Satisfactory world.
2. Understanding available resources, machines, recipes, progression, and infrastructure.
3. Designing production systems to satisfy high-level objectives.
4. Constructing and configuring factories through controlled game-engine operations.
5. Observing actual production behavior.
6. Diagnosing bottlenecks or errors.
7. Modifying its plans and factories.
8. Progressing through the game while respecting selected in-game constraints.

The SML mod itself is **not the AI**.

Its purpose is to provide a narrow, reliable bridge between Satisfactory and an external controller.

---

# Current Environment

The current Satisfactory modding workspace is:

`F:\Claude\SatisfactoryModLoader\`

The existing example mod is:

`F:\Claude\SatisfactoryModLoader\Mods\GameFeatures\AIMod\`

Current C++ source is located under:

`F:\Claude\SatisfactoryModLoader\Mods\GameFeatures\AIMod\Source\`

The environment currently has:

- Satisfactory installed through Steam on Windows 10.
- Current Satisfactory Mod Loader development environment.
- Coffee Stain/Satisfactory custom Unreal Engine.
- SML Starter Project.
- Alpakit.
- Existing `AIMod` Game Feature mod.
- Working Blueprint example content.
- Initial generated C++/C# templates.
- Successful C++ compilation.
- Successful Unreal Editor loading.
- Successful Alpakit packaging/deployment.
- Successful in-game testing of the example mod.
- Visual Studio/Visual Studio Build Tools available for compilation.
- Claude Code intended as the primary source-code development interface.

The project should support a CLI-oriented development workflow even though Unreal Editor remains necessary for Unreal-specific assets, packaging, and testing.

---

# High-Level Architecture

The intended architecture is:

```text
┌──────────────────────────────────────┐
│            Satisfactory              │
│                                      │
│   FactoryGame / Unreal Engine        │
│               │                      │
│               ▼                      │
│        SML / AIMod C++              │
│                                      │
│  - World inspection                  │
│  - Object identification             │
│  - Telemetry                         │
│  - Controlled game actions           │
│  - Validation                        │
│  - RPC transport                     │
└─────────────────┬────────────────────┘
                  │
          localhost protocol
                  │
                  ▼
┌──────────────────────────────────────┐
│       External Controller            │
│                                      │
│  Initially likely Python             │
│                                      │
│  - persistent world model            │
│  - protocol client                   │
│  - state normalization               │
│  - logging                           │
│  - experiment orchestration          │
└─────────────────┬────────────────────┘
                  │
                  ▼
┌──────────────────────────────────────┐
│     Planning / Optimization Layer    │
│                                      │
│  - recipe dependency solver          │
│  - production optimization           │
│  - resource-node selection           │
│  - logistics planning                │
│  - spatial layout                    │
│  - progression planning              │
└─────────────────┬────────────────────┘
                  │
                  ▼
┌──────────────────────────────────────┐
│              LLM Agent               │
│                                      │
│  - strategic reasoning               │
│  - goal decomposition                │
│  - replanning                        │
│  - diagnosis                         │
│  - high-level decisions              │
└──────────────────────────────────────┘
```

The boundaries between these components are intentional.

The Unreal/SML mod should remain as small and deterministic as practical.

---

# Guiding Principle

Do not begin by attempting autonomous gameplay.

Build the interface from the bottom upward and prove each layer independently.

The initial development sequence is:

```text
Existing working mod
        ↓
Reliable C++ module
        ↓
Reliable logging
        ↓
Read-only world inspection
        ↓
Structured telemetry
        ↓
External process communication
        ↓
Read-only world model
        ↓
Controlled game actions
        ↓
Factory construction primitives
        ↓
Production planner
        ↓
Closed-loop control
        ↓
Autonomous progression
```

---

# Phase 0 — Inspect and Preserve the Working Baseline

## Goal

Understand the existing generated `AIMod` project before changing it.

## Tasks

Claude should inspect:

- `Mods/GameFeatures/AIMod/AIMod.uplugin`
- `Mods/GameFeatures/AIMod/Source/`
- all `.Build.cs` files
- all `.h` files
- all `.cpp` files
- relevant Game Feature assets/configuration where inspectable as text
- root `.uproject`
- relevant `.Target.cs` files
- existing build scripts
- existing `.gitignore`
- current Git state, if Git is already initialized

Determine:

- exact C++ module name
- exported module API macro
- module startup/shutdown implementation
- current dependencies
- existing SML dependencies
- existing FactoryGame dependencies
- current generated template behavior
- build command currently used
- whether the project is already under Git

## Rules

Do not rename `AIMod`.

Do not reorganize directories.

Do not modify the custom Unreal Engine.

Do not modify SML source.

Do not modify FactoryGame source.

Do not add third-party libraries.

Do not delete existing Blueprint functionality.

## Deliverable

Create:

`docs/current-environment.md`

Document the discovered module layout and actual build process.

---

# Phase 1 — Establish a Reproducible CLI Build

## Goal

Make the C++ compile step reproducible without requiring source editing through an IDE.

The Unreal Editor may still be used for packaging/testing.

## Tasks

Determine the exact command required to build:

- Configuration: `Development Editor`
- Platform: `Win64`
- Project: `FactoryGame`

Prefer the project's existing Unreal Build Tool/build scripts rather than inventing a new CMake build.

Create:

`tools/build-editor.ps1`

The script should:

1. Locate or use the known custom Unreal Engine installation.
2. Build the correct FactoryGame Development Editor target.
3. Return the actual compiler exit status.
4. Avoid swallowing build errors.
5. Produce readable console output.
6. Require no Visual Studio GUI interaction.

Do not attempt to replace Unreal Build Tool.

## Validation

Run:

```powershell
.\tools\build-editor.ps1
```

A successful invocation must produce a valid Development Editor build.

Record the command and prerequisites in:

`docs/build.md`

---

# Phase 2 — Establish Project Logging

## Goal

Create a reliable diagnostic channel for all future work.

## Tasks

Create a dedicated Unreal log category for this mod.

Preferred conceptual name:

`LogAIModAI`

or another project-specific name consistent with existing module naming.

Add log messages for:

- module startup
- module shutdown
- Game Feature activation if accessible from the current architecture
- significant initialization failures

Example desired runtime behavior:

```text
LogAIModAI: Display: AIMod AI interface module initialized
```

Use Unreal's normal C++ logging system.

Do not use `printf`, `std::cout`, Windows message boxes, or ad hoc file logging.

## Validation

Build and package the mod.

Launch Satisfactory.

Verify the expected entry appears in:

`%LOCALAPPDATA%\FactoryGame\Saved\Logs\FactoryGame.log`

---

# Phase 3 — C++ ↔ Blueprint Smoke Test

## Goal

Prove that native C++ functionality can safely be consumed from existing Blueprint content.

## Tasks

Create the smallest reasonable Blueprint-callable C++ function.

Example semantics:

```text
GetInterfaceVersion()
```

Expected result:

```text
0.1.0
```

Use appropriate Unreal reflection macros.

The exact class type should be selected based on the current mod architecture rather than assumed.

Possible implementation:

- `UBlueprintFunctionLibrary`, or
- an appropriate service/subsystem class if the existing project already provides one.

Prefer the smallest implementation.

## Validation

1. Compile the C++ module.
2. Open Unreal Editor.
3. Confirm the function appears in Blueprint.
4. Invoke it from the existing example Blueprint.
5. Log or visibly verify the returned value.
6. Package with Alpakit.
7. Test in Satisfactory.

## Milestone

At completion:

```text
C++ → UHT → Blueprint → Alpakit → Satisfactory
```

is proven end-to-end.

---

# Phase 4 — Read-Only World Access

## Goal

Prove that the C++ module can discover useful Satisfactory game objects.

Do not expose network access yet.

## First Target: Resource Nodes

Enumerate resource nodes in the loaded world.

For each node, attempt to obtain:

- stable runtime identifier
- resource descriptor/class
- human-readable resource name if available
- purity
- world X coordinate
- world Y coordinate
- world Z coordinate
- whether occupied
- associated extractor/miner if determinable

Desired normalized conceptual representation:

```json
{
  "id": "resource-node-runtime-id",
  "resource": "Iron Ore",
  "resourceClass": "/Game/...",
  "purity": "Pure",
  "position": {
    "x": 12345.0,
    "y": -56789.0,
    "z": 890.0
  },
  "occupied": false
}
```

## Critical Development Rule

Do **not** guess FactoryGame class/function names.

Search the installed FactoryGame/SML headers and existing code.

Prefer documented or clearly intended Satisfactory APIs.

Avoid:

- hard-coded memory offsets
- arbitrary UObject memory inspection
- binary patching
- process memory manipulation

## Output

Initially simply log discovered nodes through `LogAIModAI`.

Add a debug command or Blueprint-callable entry point that triggers enumeration.

## Validation

Run the game and confirm that real resource nodes from the current save appear in the log with plausible types, purity, and coordinates.

---

# Phase 5 — Introduce Internal Telemetry Types

## Goal

Separate game-engine objects from protocol/data objects.

Do not allow Unreal actor pointers or UObject references to become part of the external API.

## Create normalized data structures for:

### Resource Node

Fields should include:

- ID
- type
- purity
- coordinates
- occupied state

### Building

Initial fields:

- ID
- Unreal/Satisfactory class
- position
- rotation
- recipe if applicable
- clock speed if applicable
- current power state if applicable

### Connection

Conceptual fields:

- source object ID
- source port
- destination object ID
- destination port
- connection type

### Production Status

Conceptual fields:

- building ID
- configured recipe
- clock rate
- current production status
- efficiency
- input inventory summary
- output inventory summary

Do not implement every field immediately.

Define structures incrementally as real game APIs are discovered.

---

# Phase 6 — JSON Serialization

## Goal

Convert telemetry into a machine-readable representation before adding networking.

Use Unreal-supported JSON facilities if available and sufficient.

Avoid introducing an external JSON library unless there is a demonstrated need.

## Initial operation

Conceptually:

```text
GetResourceNodes()
```

should eventually serialize to:

```json
{
  "protocolVersion": 1,
  "resourceNodes": [
    {
      "id": "...",
      "resource": "Iron Ore",
      "purity": "Pure",
      "position": {
        "x": 0,
        "y": 0,
        "z": 0
      }
    }
  ]
}
```

For initial testing, write the JSON to the Unreal log.

Do not add sockets yet.

## Validation

Parse captured output externally with Python to prove that it is valid JSON.

---

# Phase 7 — Stable AI Object Identification

## Goal

Provide the external controller with identifiers that do not require knowledge of Unreal pointers.

Never expose raw memory addresses as protocol identifiers.

Research what identifiers Satisfactory already provides.

Determine whether an appropriate stable identifier exists for:

- actors
- buildings
- resource nodes
- connections

If no game-provided identifier is suitable, design an interface-level identifier strategy.

Document lifecycle characteristics:

- persistence across save/load
- persistence across game restart
- behavior after dismantling
- behavior after rebuilding

Create:

`docs/object-identifiers.md`

Do not prematurely invent a UUID system without understanding existing Satisfactory identity semantics.

---

# Phase 8 — External Controller Skeleton

## Goal

Create the first platform-neutral external component.

Create:

```text
controller/
```

Initial language:

Python.

Suggested early structure:

```text
controller/
    pyproject.toml
    satisfactory_ai/
        __init__.py
        protocol.py
        models.py
        client.py
    tests/
```

Initially the controller should only:

- represent protocol models
- validate sample telemetry
- parse recorded JSON files
- contain no LLM dependency
- contain no optimization solver
- contain no game-control intelligence

Create saved telemetry fixtures under:

```text
tests/fixtures/
```

These allow development without launching Satisfactory for every controller change.

---

# Phase 9 — Localhost Transport

## Goal

Allow the external Python controller to request telemetry from a running game.

Only implement this after Phases 1–8 are reliable.

## Requirements

Transport must:

- listen only on loopback by default
- reject malformed requests safely
- enforce message-size limits
- never deserialize arbitrary executable objects
- use a versioned protocol
- return structured errors
- isolate Unreal objects from transport objects
- avoid blocking the game thread
- respect Unreal thread-safety requirements

Possible transports:

1. TCP with framed JSON
2. WebSocket
3. HTTP

Do not choose based solely on implementation convenience.

Investigate what Unreal/SML already provides.

For the initial local system, plain framed TCP or WebSocket is likely sufficient.

## Example conceptual request

```json
{
  "protocolVersion": 1,
  "requestId": "42",
  "method": "world.resourceNodes"
}
```

Response:

```json
{
  "protocolVersion": 1,
  "requestId": "42",
  "success": true,
  "result": {
    "resourceNodes": []
  }
}
```

---

# Phase 10 — Read-Only World API

Expand telemetry in this approximate order:

1. resource nodes
2. buildings
3. machine recipes
4. machine inventories
5. machine production status
6. conveyor connection components
7. conveyor topology
8. power connections
9. power networks
10. pipes/fluid networks
11. storage inventories
12. player inventory
13. unlocked schematics/milestones
14. available recipes
15. research state
16. Space Elevator state

Each feature must be individually testable.

Avoid implementing broad generic UObject introspection as a substitute for explicit APIs.

---

# Phase 11 — World Graph

## Goal

Construct an external machine-readable factory graph.

Conceptually:

```text
Resource Node
    ↓
Miner
    ↓
Conveyor
    ↓
Splitter
   ↙  ↓  ↘
Machine Machine Machine
```

The game mod should expose the facts necessary to build this graph.

The **external controller**, not the SML mod, should normally perform graph analysis.

Possible Python representation:

```text
Node:
    resource
    miner
    machine
    storage
    splitter
    merger

Edge:
    belt
    pipe
    power
```

Do not make an LLM infer this topology from prose.

---

# Phase 12 — First Controlled Write Operation

Only after read-only telemetry is reliable should the mod mutate game state.

Start with a low-risk reversible operation.

Candidates:

- set a recipe on an existing machine
- set clock speed within allowed bounds
- toggle a safe configurable property

Do not begin with arbitrary actor spawning.

Every mutation API should perform validation before modifying game state.

Expected response:

```json
{
  "success": false,
  "error": {
    "code": "INVALID_RECIPE",
    "message": "Recipe is not valid for this building."
  }
}
```

The game bridge must never simply trust the external AI.

---

# Phase 13 — Building Placement

## Goal

Expose controlled construction primitives.

Research and use FactoryGame's normal build systems wherever practical.

Avoid directly manufacturing partially initialized actors unless there is no safe public/internal game mechanism.

Initial supported machines should be limited:

1. Foundation
2. Smelter
3. Constructor
4. Assembler
5. Manufacturer

Conceptual command:

```json
{
  "method": "build.place",
  "params": {
    "buildingClass": "...",
    "position": {
      "x": 0,
      "y": 0,
      "z": 30000
    },
    "rotation": {
      "yaw": 90
    }
  }
}
```

The implementation should return:

- success/failure
- created object ID
- actual resulting transform
- structured failure reason

---

# Phase 14 — Construction Constraint Modes

The system should eventually support explicit experiment modes.

### Creative Interface Mode

Allows construction without material or progression requirements.

Purpose:

Validate AI factory design.

### Progression Mode

Require:

- unlocked building
- unlocked recipe
- required construction materials
- normal game progression

Purpose:

Test autonomous economic progression.

### Geographic Mode

Also enforce:

- collision
- valid terrain/foundation placement
- physical logistics constraints

Purpose:

Test factory layout and terrain reasoning.

### Embodied Mode

Eventually require:

- player proximity
- player travel
- possibly player inventory
- normal interaction restrictions

Purpose:

Approximate a genuine autonomous player.

These modes should be explicit configuration, not accidental behavior.

---

# Phase 15 — Conveyor and Logistics Construction

Once machine placement is reliable:

1. conveyor belts
2. splitters
3. mergers
4. lifts
5. pipes
6. pumps
7. power poles
8. power lines

Each connection API should refer to explicit connection endpoints whenever possible.

Do not make the external AI position belts by guessing visually.

---

# Phase 16 — Production Model

Build an external deterministic production database.

Represent:

- item
- recipe
- machine
- inputs/min
- outputs/min
- energy requirement
- clock-rate effects
- alternate recipes
- unlock dependencies

The production solver should perform arithmetic.

The LLM should not manually calculate large dependency trees when deterministic code can do so.

---

# Phase 17 — Optimization Solver

Implement production planning using an appropriate deterministic method such as:

- linear programming
- mixed integer linear programming
- constraint programming
- graph algorithms

Potential objective terms:

- raw resource consumption
- node count
- power consumption
- machine count
- factory footprint
- logistics distance
- construction cost
- completion time
- rare-resource consumption

Expose objective weighting rather than hard-coding one definition of "optimal."

---

# Phase 18 — Spatial Layout Planner

Initial geometry assumptions:

- sky platform
- fixed foundation grid
- standardized floor heights
- minimal terrain interaction

This deliberately reduces factory geometry to approximately a 2D placement problem.

Later introduce:

- real terrain
- elevation
- clearance
- belts
- pipes
- train corridors
- factory expansion constraints

The LLM should provide high-level design intent.

A deterministic geometry engine should calculate exact transforms where practical.

---

# Phase 19 — Closed-Loop Factory Construction

The first significant autonomous experiment:

```text
Objective:
Produce X items/min of a selected product.
```

Agent cycle:

```text
Observe
   ↓
Plan
   ↓
Solve
   ↓
Layout
   ↓
Build
   ↓
Observe
   ↓
Validate
   ↓
Correct
```

Measure expected production against observed production.

Example:

```text
Expected: 120 Iron Plates/min
Observed: 90 Iron Plates/min
```

The system should identify why before blindly rebuilding.

---

# Phase 20 — Autonomous Progression

Eventually begin from a constrained fresh save.

The controller must track:

- current milestone
- unlocked technologies
- available recipes
- inventory
- existing production
- power
- Space Elevator requirements
- research dependencies

The AI chooses the next objective.

Example:

```text
Goal:
Unlock Coal Power

Derived subgoals:
    Reinforced Iron Plates
    Rotors
    Cable
    milestone submission
```

Progression should occur using the same world-state API and construction primitives developed earlier.

---

# Experimental End Goal

Given:

```text
Fresh save
+
known Satisfactory rules
+
defined completion objective
```

the system should be capable of independently creating an industrial progression from basic resource extraction through Project Assembly completion.

The final experiment should record:

- every strategic plan
- every solver result
- every game action
- every failure
- every replan
- production telemetry
- progression milestones
- machine counts
- resource usage
- logistics lengths
- elapsed game time
- model/API usage

This should make an autonomous playthrough reproducible and analyzable rather than merely entertaining to watch.

---

# Immediate Claude Work Queue

Claude should initially work only through these tasks:

## Task 1

Inspect the current `AIMod` source and document its actual structure.

No significant code changes.

## Task 2

Establish or verify a repeatable CLI Development Editor build.

Create `tools/build-editor.ps1` only if it adds value over an existing reliable command.

## Task 3

Add a dedicated C++ log category.

Verify startup logging.

## Task 4

Create one minimal C++ function callable from Blueprint.

Return an interface version string.

Compile successfully.

## Task 5

Document how to invoke/test the C++ function from the existing Blueprint.

## Task 6

Research the actual FactoryGame/SML C++ APIs for resource nodes.

Search installed source/headers before coding.

Document findings in:

`docs/resource-node-research.md`

## Task 7

Implement read-only resource-node enumeration.

Log:

- type
- purity
- coordinates
- occupied state where safely available

## Task 8

Refactor resource-node data into an internal normalized telemetry structure.

## Task 9

Serialize the resulting telemetry to JSON.

Log valid JSON.

## Task 10

Create a small Python test that consumes recorded telemetry and verifies the JSON schema.

**Stop here before implementing networking.**

Review the architecture and results before moving to transport or game-state mutation.

---

# Definition of Initial Success

The first development milestone is complete when all of the following are true:

```text
[ ] Existing Blueprint mod still works.
[ ] C++ module builds cleanly.
[ ] Development Editor builds from a repeatable CLI process.
[ ] Dedicated C++ logging works in Satisfactory.
[ ] C++ function is callable from Blueprint.
[ ] Running game resource nodes can be enumerated.
[ ] Resource type/purity/coordinates are captured.
[ ] Telemetry is represented independently of Unreal object pointers.
[ ] Telemetry serializes as valid JSON.
[ ] External Python code can parse a captured telemetry response.
[ ] No network server has been added yet.
[ ] No game state mutation has been added yet.
```

At this point the project will have demonstrated the core feasibility of the Satisfactory AI interface without prematurely introducing the most failure-prone components.