# ficsit.app mod-page description (paste-ready)

Paste the section below into the SatisfactoryAIMod page's long description on
ficsit.app (SMR). It mirrors the README so the two stay consistent — if you
edit one, update the other. The short/tagline field is:

> Connect AI agents to Satisfactory over a local RPC interface.

---

## SatisfactoryAIMod — an RPC interface for AI agents

SatisfactoryAIMod exposes a controlled, machine-readable interface between a
running Satisfactory game and an external program — so an AI agent (or any
script) can read the world and perform explicit, validated build/config
operations over a local connection.

**The mod is a bridge, not the AI.** It reads FactoryGame world state and
translates a small, safe set of write operations back into the game. All the
planning and decision-making happens in *your* external program; the mod just
gives it a reliable, well-defined way to observe and act.

### What it can do

Everything is JSON-RPC over a **loopback-only** HTTP server the mod runs
inside the game (120+ `world.*` methods). The interface is **self-describing**
— call `world.help` for a live catalog of every method, its parameters, and a
one-line summary, so a program can discover the whole API at runtime.

- **Read (telemetry):** resource nodes, buildings (with bounds), belt/pipe
  connections, machines & recipes, power, inventories, the Dimensional Depot,
  full recipe/item/buildable catalogs, milestones & M.A.M. progress,
  vehicles/trains/trucks/drones/creatures, terrain height, and more.
- **Build & wire:** place/delete buildings and miners; connect
  belts/lifts/pipes/hypertubes/power (each with a dry-run validator); beams,
  supports, water pumps — with real material cost, like a normal player.
- **Configure:** set recipes, clock speed, power shards, splitter sort rules,
  power switches, colors/rotation.
- **Logistics:** trains (track, stations, timetables, self-driving), trucks
  (paths, autopilot), and drones (stations, pairing, cargo/fuel).

Real factories have been built end to end through this interface and
confirmed producing — a copper line and a Heavy Modular Frame factory — plus
rail, truck, and drone proofs-of-concept.

### Getting started

1. Install the mod (it depends on **SML**) and launch a session.
2. The mod starts an RPC server at `http://127.0.0.1:51902/rpc` (this machine
   only, by default).
3. POST JSON to `/rpc` — start with `world.help`. Every request carries
   `protocolVersion: 1`, a `requestId`, and a `method`; write methods take a
   `params` object. Full docs and an optional Python client are on GitHub.

### Please read — safety & disclosures

- **Back up your saves.** This performs real writes against a live save and is
  experimental software, provided "as is" with no warranty.
- **Not achievement-safe.** This is an automation/control tool. Even by
  default it can do things a normal session can't (e.g. teleport the player),
  and with optional settings it can inject items, re-fire achievements, and
  manipulate the world. Treat any save you use it on as a modded/creative save.
- **Creative/cheat features are OFF by default.** Free item injection,
  achievement re-fire, event forcing, and world manipulation are gated behind
  an **Allow Creative Features** setting that defaults off; an external client
  can never enable it — only you, in the settings menu. A default install is
  telemetry + normal, material-cost construction.
- **Networking:** loopback-only by default (only your machine can connect). An
  **Allow Remote Connections** setting (off by default) opens it to your LAN —
  use only on a trusted network, since anyone who can reach the port can drive
  your game.
- **Single-player focused.** Multiplayer is largely untested.

### Links

- Source, docs, and Python client: <https://github.com/daten/SatisfactoryAIMod>
- License: GPLv3.

*Beta — feedback and issues welcome on GitHub.*
