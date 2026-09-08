# Satisfactory AI Interface (AIMod)

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
   SML / AIMod C++        <- this repo
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

Everything is exposed as JSON-RPC methods over a **loopback-only** HTTP
server the mod runs inside the game (`world.*` namespace). There are
**~104 methods**, most exercised against a real running session (not just
compiled). Rather than list them here, the interface is **self-describing**:

- **`world.help`** returns a live catalog of every method with its params
  and a one-line summary — so an external agent can discover the whole
  interface at runtime without the mod source.
- [`docs/rpc-catalog.md`](docs/rpc-catalog.md) is the same catalog
  rendered as a browsable doc (generated from the dispatcher).

Coverage, by area:

- **Telemetry (read):** resource nodes, buildables (+bounds), factory/pipe
  connections, manufacturers, power poles/switches, inventories, central
  storage, catalogs (recipes/items/buildables), tiers (belt/lift/pipe/pump),
  water volumes, terrain height grid, milestones/M.A.M. status, time of day,
  map markers, vehicles/trains/trucks/drones/creatures, and more.
- **Construction & wiring:** place/delete buildings and extractors, connect
  belts/lifts/pipes/hypertubes/power (each with a `world.testX` dry-run),
  beams, stackable supports, water pumps.
- **Configuration:** set recipe / clock / power shards / splitter sort rules /
  power-switch state / buildable color & rotation, add items to a buildable
  inventory, time of day, map markers.
- **Vehicles & logistics:** trains (track, stations, timetable, self-driving),
  trucks (vehicle paths, autopilot), and **drones** (stations, pairing,
  cargo/fuel) — drone transport works end-to-end; see
  [`docs/vehicle-placement-guide.md`](docs/vehicle-placement-guide.md) for
  the current status of each.

Real factories have been built entirely through this surface and confirmed
producing (a copper line and a Heavy Modular Frame factory), plus rail/truck/
drone proofs-of-concept.

Out of scope so far: **no planning/optimization layer exists yet** (Phases
17/19/20 in [PLAN.md](PLAN.md)) — the Python side is a deterministic
*toolkit*, not an auto-solver — and the mod deliberately avoids any generic
"call arbitrary function" operation (see CLAUDE.md's Safety and Stability
Boundary).

## Where things live

- **Mod C++ source** (the actual interface):
  [`Mods/GameFeatures/AIMod/Source/AIMod`](Mods/GameFeatures/AIMod/Source/AIMod)
  — `AIModFunctionLibrary.cpp/.h` implements the construction/telemetry
  logic, `AIModHttpServerSubsystem.cpp/.h` is the RPC transport and
  method dispatch.
- **External controller** (Python side):
  [`controller/`](controller) — `satisfactory_ai/` is a toolkit of
  geometry/protocol helpers (not an auto-layout solver by design), with
  its own test suite under `controller/tests`.
- **Living documentation**: [`docs/`](docs). Start with the distilled,
  task-oriented guides:
  [`docs/factory-placement-guide.md`](docs/factory-placement-guide.md)
  (foundations, machines, splitters/mergers, belts, lifts) and
  [`docs/vehicle-placement-guide.md`](docs/vehicle-placement-guide.md)
  (trains, trucks, drones); the full method catalog is
  [`docs/rpc-catalog.md`](docs/rpc-catalog.md) (also live via `world.help`).
  [`docs/placement-lessons.md`](docs/placement-lessons.md) is the complete
  chronological log the guides distil. The rest of `docs/` are dated
  investigation logs (`*-research.md`) recording how specific FactoryGame
  APIs were reverse-engineered from stub-source headers, plus environment/
  build notes (`current-environment.md`, `build.md`). (The older
  `telemetry-protocol.md` / `operations-protocol.md` predate most of the
  interface — prefer `world.help` / `rpc-catalog.md`.)
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
SML/Alpakit setup instructions unrelated to AIMod itself.

## Disclaimer

This is experimental, research-stage software provided "as is," with no
warranty of any kind. It performs real, validated write operations
against a live Satisfactory save — back up saves before experimenting.
