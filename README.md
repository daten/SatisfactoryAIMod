# Satisfactory AI Interface (DocMod)

An experimental Satisfactory Mod Loader (SML) mod that exposes a
controlled, machine-readable interface between a running Satisfactory
game and an external AI/planning system.

The mod itself is **not** the AI. It's a narrow, reliable bridge: it
reads world state out of FactoryGame and translates a small set of
explicit, validated write operations back in. Planning, optimization, and
decision-making are meant to live outside the mod, in an external
controller talking to it over a local RPC protocol.

```
Satisfactory (FactoryGame / Unreal Engine)
        |
        v
   SML / DocMod C++        <- this repo
        |
   loopback HTTP RPC
        |
        v
  External controller       (Python, controller/)
        |
        v
  Planning / optimization    (not yet built)
```

The long-term objective is an external agent that can observe the
Satisfactory world, plan production chains against real recipes and
resources, construct and configure factories through the mod's controlled
operations, watch actual production, and replan — see
[PLAN.md](PLAN.md) for the full objective and architecture, and
[CLAUDE.md](CLAUDE.md) for the project's working rules (safety boundaries,
source-of-truth priority, logging/testing conventions).

## Current functionality

Everything below is exposed as JSON-RPC methods over a **loopback-only**
HTTP server the mod runs inside the game (`world.*` namespace). All of it
has been exercised against a real, running Satisfactory session, not just
compiled.

**Read / telemetry:**
`world.resourceNodes`, `world.buildables`, `world.manufacturers`,
`world.connections`, `world.conveyorBeltTiers`, `world.conveyorLiftTiers`,
`world.conveyorAttachments`, `world.pipelineTiers`, `world.powerLineLimits`,
`world.targetedManufacturer`, `world.player`.

**Write / construct** (each has a `world.testX` dry-run counterpart where
noted):
`world.placeBuilding`, `world.placeExtractor`, `world.deleteBuilding`,
`world.setRecipe`, `world.setClockSpeed`,
`world.testConveyorBelt` / `world.connectConveyor`,
`world.testConveyorLift` / `world.connectConveyorLift`,
`world.testPipe` / `world.connectPipe`,
`world.testPowerConnection` / `world.connectPower`.

A full demo chain — Miner -> vertical conveyor lift -> splitter -> 3
parallel Constructors -> merger -> storage container, real power routed
from the map's existing grid — has been built entirely through this RPC
surface and confirmed producing (`world.manufacturers` showing
`productionStatus: "Producing"` with real material flow), not just placed.

Out of scope so far: no planning/optimization layer exists yet, and the
mod deliberately avoids any generic "call arbitrary function" style
operation — see CLAUDE.md's Safety and Stability Boundary.

## Where things live

- **Mod C++ source** (the actual interface):
  [`Mods/GameFeatures/DocMod/Source/DocMod`](Mods/GameFeatures/DocMod/Source/DocMod)
  — `DocModFunctionLibrary.cpp/.h` implements the construction/telemetry
  logic, `DocModHttpServerSubsystem.cpp/.h` is the RPC transport and
  method dispatch.
- **External controller** (Python side):
  [`controller/`](controller) — `satisfactory_ai/` is a toolkit of
  geometry/protocol helpers (not an auto-layout solver by design), with
  its own test suite under `controller/tests`.
- **Living documentation**: [`docs/`](docs) — in particular,
  [`docs/placement-lessons.md`](docs/placement-lessons.md) is a
  continuously-updated, practical reference for placing/connecting
  buildables reliably (read this before writing new placement code or
  debugging a placement failure). The rest of `docs/` are dated
  investigation logs (`*-research.md`) recording how specific FactoryGame
  APIs were reverse-engineered from stub-source headers, plus protocol
  references (`telemetry-protocol.md`, `operations-protocol.md`) and
  environment/build notes (`current-environment.md`, `build.md`).
- **Plan and working rules**: [`PLAN.md`](PLAN.md) (objective, phase
  breakdown, current milestone) and [`CLAUDE.md`](CLAUDE.md) (behavioral
  rules for AI-assisted development on this repo — safety boundaries,
  logging conventions, source-of-truth priority when FactoryGame's own
  `.cpp` bodies are stub-only).

## About this repository

This started from the standard [SML Starter
Project](https://docs.ficsit.app/) template and still contains the full
SML loader/Alpakit tooling needed to build and deploy the mod. See the
[Satisfactory Modding docs](https://docs.ficsit.app/) for general
SML/Alpakit setup instructions unrelated to DocMod itself.

## Disclaimer

This is experimental, research-stage software provided "as is," with no
warranty of any kind. It performs real, validated write operations
against a live Satisfactory save — back up saves before experimenting.
