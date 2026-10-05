# Satisfactory AI Mod — MCP bridge

A small local **MCP server** that lets AI apps (Claude Desktop, OpenAI Codex, and
other MCP-capable tools) read and act in a running Satisfactory game through the
[SatisfactoryAIMod](https://github.com/daten/SatisfactoryAIMod) loopback API.

It turns the mod's ~133 low-level methods into ~23 friendly, game-language tools
(find resource nodes, place/connect buildings, power, logistics, save…) plus
read/action passthroughs for anything not covered. It adds guardrails the raw API
doesn't have: read-only vs. action classification, plain-language errors, verified
saves, safe teleports, serialized calls, and output limits.

> **What it is NOT:** it does not add new game abilities, and it is not the AI.
> It's an adapter between the AI app and the mod. See
> [`docs/mcp-bridge-plan.md`](../docs/mcp-bridge-plan.md) for the design.

## Requirements

- **Satisfactory running with the SatisfactoryAIMod**, and a **save loaded** (not
  the main menu). The bridge talks to the mod at `http://127.0.0.1:51902/rpc`.
- **Node.js ≥ 18** for the Codex / command-line path (the Claude Desktop extension
  bundles its own Node).

## Install

### Claude Desktop (extension) — the simplest path

1. Build the extension bundle (see **Packaging** below) to get `satisfactory-aimod.mcpb`.
2. In Claude Desktop → **Settings → Extensions**, install the `.mcpb`.
3. (Optional) In the extension's settings, set **Read-only mode** to `true` to let
   the AI only look, not build.
4. Start Satisfactory with the mod, load a save, then chat: *"Are we connected?
   Survey my base."*

> Status: the manifest validates against `mcpb` 2.x and packs to a ~3 MB bundle
> (verified to contain the entry point + runtime deps, no source/tests). The one
> step not yet done is a **test-install in a live Claude Desktop** - do that before
> release. The Codex / command-line path below is fully working.

### OpenAI Codex (local) — CLI, IDE, or desktop app

Codex must run **on the same PC as the game** (Codex *cloud* tasks can't reach it).

Add an MCP server to your Codex config (`~/.codex/config.toml`):

```toml
[mcp_servers.satisfactory]
command = "node"
args = ["F:/path/to/SatisfactoryAIMod/mcp-bridge/dist/src/index.js"]
# optional:
# env = { SATISFACTORY_MCP_READONLY = "true" }
```

(Or, once published to npm: `command = "npx"`, `args = ["-y", "satisfactory-aimod-mcp"]`.)

> **Windows/WSL note:** the mod listens on Windows `127.0.0.1`. Run Codex on
> Windows directly, or in WSL with mirrored networking — default NAT WSL can't
> reach the game.

### Any other MCP app

Point it at the command `node dist/src/index.js` (stdio transport). Chat-only
websites/mobile apps that can't run a local program can't connect.

## Settings (environment variables)

| Variable | Default | Meaning |
|---|---|---|
| `SATISFACTORY_RPC_URL` | `http://127.0.0.1:51902/rpc` | The mod's endpoint. Change only if you changed the mod's port. |
| `SATISFACTORY_MCP_READONLY` | _(off)_ | `true`/`1` to expose only look-up tools (no building, placing, deleting). |

## Tools

Reads (safe to auto-approve): `game_status`, `get_player`, `find_resource_nodes`,
`list_buildings`, `get_machines`, `get_power`, `get_logistics`, `get_progression`,
`lookup_catalog`, `get_build_cost`, `check_location`, `list_game_methods`,
`read_game_data`.

Actions (ask first): `save_game`, `place_building`, `place_miner`, `connect_belt`,
`connect_power`, `configure_machine`, `remove_building` (destructive),
`teleport_player` (safety-checked), `take_photo`, `set_train_route`,
`perform_game_action`.

In read-only mode, only the reads are registered.

## Develop

```bash
cd mcp-bridge
npm install
npm run gen     # regenerate src/catalog.ts from the mod's catalog (needs Python)
npm run build   # tsc -> dist/
npm test        # build + run the mock-game and stdio tests (no game needed)
npm start       # run the server over stdio (expects the game for tool calls)
```

- `src/catalog.ts` / `src/catalog.json` are **generated** from
  `Mods/.../AIModRpcCatalog.gen.cpp` by `scripts/extract_catalog.py`. Re-run `npm run gen`
  whenever the mod's methods change; the test suite fails if any method is left
  unclassified (`src/classify.ts`).
- Tests use an in-process mock of the game (`test/mock-game.ts`) and an in-memory
  MCP client, plus a real stdio launch — so they run without Satisfactory.

## Packaging

- **npm:** `npm run build` then `npm pack` (or publish) for the Codex `npx` path.
- **Claude Desktop `.mcpb`:** the `mcpb` CLI is a devDependency. Validate and pack:

  ```bash
  npm run validate        # mcpb validate manifest.json
  # lean release bundle (runtime deps only):
  npm ci                  # clean install (incl. dev, needed to build)
  npm run build           # tsc -> dist/
  npm prune --omit=dev    # drop typescript/mcpb/@types from node_modules
  npx mcpb pack . satisfactory-aimod.mcpb
  npm install             # restore dev deps for development
  ```

  `.mcpbignore` keeps the bundle lean (ships `dist/` minus tests/maps, `manifest.json`,
  `package.json`, `README.md`, and production `node_modules`). **Note:** patterns are
  gitignore-style - anchor top-level excludes with a leading `/` (e.g. `/src/`), or a
  bare `src/` will also drop `dist/src/` (the entry point). Produced bundle: ~3 MB.
  **Remaining before release:** test-install the `.mcpb` in a live Claude Desktop.

## Safety

- Local only (connects to loopback; opens no network listener of its own).
- Every action tool is annotated so apps can prompt before changes; reads are marked
  safe. Destructive tools (`remove_building`, `teleport_player`) are flagged.
- Creative/cheat features stay gated **in the mod** — the bridge can't enable them.
- In-game text (chat, station/marker names) is treated as data, never instructions.
- See [`docs/rpc-player-interaction.md`](../docs/rpc-player-interaction.md) for the
  camera/teleport/photo-mode/vehicle effects the tools warn about.
