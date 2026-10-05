import { test } from "node:test";
import assert from "node:assert/strict";
import { Client } from "@modelcontextprotocol/sdk/client/index.js";
import { StdioClientTransport } from "@modelcontextprotocol/sdk/client/stdio.js";
import { fileURLToPath } from "node:url";
import * as path from "node:path";

// Spawns the real built entry point (dist/src/index.js) over stdio - the same way
// Claude Desktop / Codex launch it - and confirms it serves tools without a game.
test("built server launches over stdio and lists tools", async () => {
  const entry = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../src/index.js");
  const transport = new StdioClientTransport({
    command: process.execPath,
    args: [entry],
    env: { ...process.env, SATISFACTORY_RPC_URL: "http://127.0.0.1:59998/rpc" },
    stderr: "ignore",
  });
  const client = new Client({ name: "smoke", version: "1.0.0" });
  await client.connect(transport);
  try {
    const tools = await client.listTools();
    const names = tools.tools.map((t) => t.name);
    assert.ok(names.includes("game_status"));
    assert.ok(names.includes("place_building"));
    // and the server advertised instructions
    const caps = client.getServerCapabilities();
    assert.ok(caps?.tools);
  } finally {
    await client.close();
  }
});
