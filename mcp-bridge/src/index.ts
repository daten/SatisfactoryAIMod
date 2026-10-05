#!/usr/bin/env node
import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { StdioServerTransport } from "@modelcontextprotocol/sdk/server/stdio.js";
import { GameClient } from "./game.js";
import { SERVER_INSTRUCTIONS } from "./instructions.js";
import { registerReadTools } from "./tools/reads.js";
import { registerActionTools } from "./tools/actions.js";
import { registerGenericTools } from "./generic.js";

const VERSION = "0.1.0";

function envFlag(name: string): boolean {
  const v = (process.env[name] ?? "").toLowerCase();
  return v === "1" || v === "true" || v === "yes" || v === "on";
}

export function buildServer(game: GameClient, readOnlyMode: boolean): McpServer {
  const server = new McpServer(
    { name: "satisfactory-aimod", version: VERSION },
    {
      instructions:
        SERVER_INSTRUCTIONS +
        (readOnlyMode ? "\n\n(Read-only mode is ON: only look-up tools are available; no building.)" : ""),
    },
  );

  registerReadTools(server, game);
  registerGenericTools(server, game, { readOnlyMode });
  if (!readOnlyMode) registerActionTools(server, game);

  // A few starter prompts the app can surface.
  server.registerPrompt(
    "survey_base",
    { title: "Survey my base", description: "Ask the agent to survey your factory and summarize its state." },
    () => ({
      messages: [
        {
          role: "user",
          content: {
            type: "text",
            text: "Check we're connected, then survey my base: what machines I have and their status, any unpowered buildings, and what I'm producing. Summarize in plain language.",
          },
        },
      ],
    }),
  );
  server.registerPrompt(
    "find_free_nodes",
    { title: "Find free resource nodes near me", description: "Find unoccupied resource nodes close to the pioneer." },
    () => ({
      messages: [
        { role: "user", content: { type: "text", text: "Find the resource nodes near me that don't have a miner yet, nearest first." } },
      ],
    }),
  );
  if (!readOnlyMode) {
    server.registerPrompt(
      "first_build",
      { title: "Build a small production line", description: "Save, then build and verify a simple line." },
      () => ({
        messages: [
          {
            role: "user",
            content: {
              type: "text",
              text: "Save my game first. Then place a miner on a nearby free iron node, add a smelter, belt them together, connect power, and confirm it's producing. Tell me before anything moves my view or teleports me.",
            },
          },
        ],
      }),
    );
    server.registerPrompt(
      "cinematic_photo",
      { title: "Take a cinematic photo", description: "Set a nice time of day and photograph the base." },
      () => ({
        messages: [
          { role: "user", content: { type: "text", text: "Set a nice golden-hour time of day and take a high-res photo of my base from above." } },
        ],
      }),
    );
  }

  return server;
}

async function main() {
  const readOnlyMode = envFlag("SATISFACTORY_MCP_READONLY");
  const game = new GameClient({ url: process.env.SATISFACTORY_RPC_URL });
  const server = buildServer(game, readOnlyMode);
  const transport = new StdioServerTransport();
  await server.connect(transport);
  // stdout is the MCP channel; logs go to stderr only.
  console.error(
    `[satisfactory-aimod-mcp ${VERSION}] connected via stdio; game endpoint ${game.url}` +
      (readOnlyMode ? " (read-only mode)" : ""),
  );
}

// Only run when executed directly (not when imported by tests).
const isMain = (() => {
  try {
    return process.argv[1] && import.meta.url === new URL(`file://${process.argv[1]}`).href;
  } catch {
    return false;
  }
})();
// Fallback: run if this is the entry module.
if (process.env.SATISFACTORY_MCP_NORUN !== "1" && (isMain || process.argv[1]?.endsWith("index.js"))) {
  main().catch((e) => {
    console.error("[satisfactory-aimod-mcp] fatal:", e);
    process.exit(1);
  });
}
