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

The SML mod itself is **not the AI**. Its purpose is to provide a narrow, reliable bridge between Satisfactory and an external controller.

---

## Status (updated 2026-09-08)

**The bottom-up interface is built.** The mod exposes ~104 `world.*` JSON-RPC
methods over a loopback HTTP server, self-described at runtime by `world.help`
(and rendered to `docs/rpc-reference.md`). Real factories (a copper line and a
Heavy Modular Frame factory) and rail/truck/drone proofs-of-concept have been
built through it against a live game.

- **COMPLETE — Phases 0–15** (baseline, CLI build, logging, C++↔Blueprint,
  read-only world access, normalized telemetry, JSON, stable IDs, external
  controller skeleton, loopback transport, read API, world graph, first write
  ops, building placement, conveyor/pipe/power/logistics construction).
- **Built BEYOND the original plan:** vehicles (trains/trucks/drones), map
  markers, M.A.M./milestone control, hypertube, beams, creatures,
  `addItemsToInventory`, and the `world.help` self-description; a deterministic
  Python toolkit (`controller/satisfactory_ai`: composites, router,
  connector_db, executor); and distilled agent guides
  (`docs/factory-placement-guide.md`, `docs/vehicle-placement-guide.md`).
- **PARTIAL — Phase 14** (constraint modes): we effectively operate in a
  "creative" mode via placement ignore-flags plus an `UnlimitedResources`
  config toggle; the explicit multi-mode framework is not formalized.
  **PARTIAL — Phases 16 & 18** (production model, spatial layout): these exist
  as a deterministic *toolkit* (production/router/layout/connector_db), by
  design not an auto-solver.
- **OUTSTANDING — the real remaining work:** Phase 17 (optimization solver),
  Phase 19 (closed-loop autonomous construction), Phase 20 (autonomous
  progression), and the Experimental End Goal. Plus known engineering gaps in
  the interface itself: a fully drivable RPC-built train joint, a truck driving
  a clean tree-free loop, freight-platform snap, and rail coupling (see
  `docs/vehicle-placement-guide.md`).

Guiding rule for what remains: keep the planner/optimizer/agent **outside** the
mod (see CLAUDE.md — Keep the Unreal Mod Small).

---

# Current Environment

Satisfactory (Steam, Windows 10) with the Coffee Stain custom Unreal Engine,
SML, Alpakit, and the `AIMod` Game Feature mod under
`Mods/GameFeatures/AIMod/`. Development is CLI-oriented (Claude Code) with a
repeatable editor build via `tools/build-editor.ps1`; Unreal Editor + Alpakit
remain necessary for packaging and in-game testing. See
`docs/current-environment.md` and `docs/build.md` for specifics.

---

# High-Level Architecture

```text
┌──────────────────────────────────────┐
│            Satisfactory              │
│   FactoryGame / Unreal Engine        │
│               │                      │
│               ▼                      │
│        SML / AIMod C++              │   <- built
│  - World inspection                  │
│  - Object identification             │
│  - Telemetry                         │
│  - Controlled game actions           │
│  - Validation                        │
│  - RPC transport                     │
└─────────────────┬────────────────────┘
                  │  loopback JSON-RPC (built)
                  ▼
┌──────────────────────────────────────┐
│       External Controller            │   <- toolkit built; persistent
│  (Python, controller/)               │      world model / orchestration
│  - protocol client, geometry toolkit │      still minimal
└─────────────────┬────────────────────┘
                  │
                  ▼
┌──────────────────────────────────────┐
│     Planning / Optimization Layer    │   <- NOT built (Phase 17)
└─────────────────┬────────────────────┘
                  │
                  ▼
┌──────────────────────────────────────┐
│              LLM Agent               │   <- closed loop NOT built (19/20)
└──────────────────────────────────────┘
```

The boundaries between these components are intentional. The Unreal/SML mod
stays as small and deterministic as practical; planning and optimization live
outside it.

---

# Guiding Principle

Build the interface from the bottom upward and prove each layer independently
(this order has been followed through construction; it continues to apply to
new capability):

```text
Existing working mod → Reliable C++ module → Reliable logging →
Read-only world inspection → Structured telemetry → External process
communication → Read-only world model → Controlled game actions →
Factory construction primitives → [done to here] →
Production planner → Closed-loop control → Autonomous progression
```

---

# Phases 0–15 — COMPLETE (foundation → construction)

Delivered and live-tested; kept here as a summary rather than the original
step-by-step (git history has the detail):

- **0–3 Foundation:** inspected/preserved the working mod; repeatable CLI
  Development Editor build (`tools/build-editor.ps1`); dedicated `LogAIModAI`
  logging; a C++ function callable from Blueprint.
- **4–7 Read + telemetry:** resource-node enumeration, then normalized
  telemetry structures (nodes, buildings, connections, production status), JSON
  serialization, and stable identifiers (`GetPathName` / buildable ids).
- **8–11 Controller + transport + model:** external Python controller
  (`controller/`), loopback JSON-RPC HTTP server, read-only world API, and
  world-graph/connectivity telemetry.
- **12–13 First writes:** `setRecipe`/`setClockSpeed` against existing
  machines, then `placeBuilding` (with ground-trace/literal-Z, explicit yaw,
  `gridSnapSize`, validation).
- **15 Logistics construction:** belts, splitters, mergers, lifts, pipes,
  pumps, power poles and lines — all endpoint-referenced (no visual guessing),
  each connect op with a `testX` dry-run.

Operational rules learned across these phases live in
`docs/factory-placement-guide.md` and the full log `docs/placement-lessons.md`.

---

# Phase 14 — Construction Constraint Modes (PARTIAL)

The system should support explicit experiment modes:

- **Creative Interface Mode** — construction without material/progression
  requirements (validate AI factory design). *We effectively run in this mode
  today via placement ignore-flags + the `UnlimitedResources` config toggle.*
- **Progression Mode** — require unlocked building/recipe, materials, normal
  progression (test autonomous economic progression).
- **Geographic Mode** — additionally enforce collision, valid terrain/foundation
  placement, physical logistics (test layout/terrain reasoning).
- **Embodied Mode** — additionally require player proximity/travel/inventory and
  normal interaction restrictions (approximate a genuine autonomous player).

These should be explicit configuration, not accidental behavior. **Outstanding:**
a single formal mode selector; today the constraints are toggled ad hoc.

---

# Phase 16 — Production Model (PARTIAL — toolkit)

An external deterministic production database: item, recipe, machine,
inputs/outputs per minute, energy, clock-rate effects, alternate recipes, unlock
dependencies. The solver should do the arithmetic, not the LLM.

*Status:* `controller/satisfactory_ai/production.py` plus the live
`world.recipeCatalog`/`itemCatalog`/`constructionCost` telemetry provide the raw
model; a complete standalone production database is not yet consolidated.

---

# Phase 17 — Optimization Solver (OUTSTANDING)

Production planning via an appropriate deterministic method (LP / MILP /
constraint programming / graph algorithms). Expose objective *weighting* (raw
resource use, node/machine count, power, footprint, logistics distance,
construction cost, completion time, rare-resource use) rather than hard-coding
one definition of "optimal." **Not built.**

---

# Phase 18 — Spatial Layout Planner (PARTIAL — toolkit)

Reduce factory geometry to a tractable placement problem (sky platform / fixed
grid / standard floor heights first; real terrain, elevation, clearance, belts,
pipes, train corridors later). The LLM gives design intent; a deterministic
geometry engine computes exact transforms.

*Status:* `router`, `layout`, `connector_db`, and the `composites` idioms
provide validated geometry/connector math and reusable build units; a
higher-level automatic layout planner is not built.

---

# Phase 19 — Closed-Loop Factory Construction (OUTSTANDING)

The first significant autonomous experiment — given "produce X items/min of a
product," run the loop **Observe → Plan → Solve → Layout → Build → Observe →
Validate → Correct**, comparing expected vs observed production and diagnosing
*why* before rebuilding. Today this loop is driven by an agent per request, not
automated end to end. **Not built as an autonomous loop.**

---

# Phase 20 — Autonomous Progression (OUTSTANDING)

From a constrained fresh save, track milestone/tech/recipes/inventory/production/
power/Space-Elevator/research state; the AI chooses the next objective and
achieves it through the same world-state API and construction primitives.
**Not built.**

---

# Experimental End Goal

Given a fresh save + known Satisfactory rules + a defined completion objective,
the system independently creates an industrial progression from basic extraction
through Project Assembly, recording every plan, solver result, action, failure,
replan, and telemetry sample — so an autonomous playthrough is reproducible and
analyzable, not merely watchable.
