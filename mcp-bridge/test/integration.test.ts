import { test } from "node:test";
import assert from "node:assert/strict";
import { Client } from "@modelcontextprotocol/sdk/client/index.js";
import { InMemoryTransport } from "@modelcontextprotocol/sdk/inMemory.js";
import { GameClient } from "../src/game.js";
import { buildServer } from "../src/index.js";
import { startMockGame, defaultHandlers } from "./mock-game.js";

async function connect(readOnly: boolean) {
  const mock = await startMockGame(defaultHandlers());
  const game = new GameClient({ url: mock.url });
  const server = buildServer(game, readOnly);
  const client = new Client({ name: "test", version: "1.0.0" });
  const [ct, st] = InMemoryTransport.createLinkedPair();
  await Promise.all([client.connect(ct), server.connect(st)]);
  return { mock, client, close: async () => { await client.close(); await mock.close(); } };
}

test("tools are exposed; read-only mode hides actions", async () => {
  const std = await connect(false);
  try {
    const names = (await std.client.listTools()).tools.map((t) => t.name);
    for (const n of ["game_status", "find_resource_nodes", "list_buildings", "get_machines", "lookup_catalog",
      "save_game", "place_building", "place_miner", "connect_belt", "connect_power", "configure_machine",
      "remove_building", "teleport_player", "take_photo", "set_train_route",
      "list_game_methods", "read_game_data", "perform_game_action"]) {
      assert.ok(names.includes(n), `missing tool ${n}`);
    }
    // destructive annotation present
    const del = (await std.client.listTools()).tools.find((t) => t.name === "remove_building")!;
    assert.equal(del.annotations?.destructiveHint, true);
  } finally {
    await std.close();
  }

  const ro = await connect(true);
  try {
    const names = (await ro.client.listTools()).tools.map((t) => t.name);
    assert.ok(names.includes("find_resource_nodes"));
    assert.ok(names.includes("read_game_data"));
    for (const n of ["place_building", "remove_building", "perform_game_action", "teleport_player"]) {
      assert.ok(!names.includes(n), `${n} must be hidden in read-only mode`);
    }
  } finally {
    await ro.close();
  }
});

test("game_status reports connected", async () => {
  const h = await connect(false);
  try {
    const r: any = await h.client.callTool({ name: "game_status", arguments: {} });
    assert.ok(!r.isError);
    assert.match(r.content[0].text, /Connected/);
    assert.equal(r.structuredContent.connected, true);
  } finally {
    await h.close();
  }
});

test("find_resource_nodes filters free + sorts by distance", async () => {
  const h = await connect(false);
  try {
    const r: any = await h.client.callTool({ name: "find_resource_nodes", arguments: { freeOnly: true } });
    assert.ok(!r.isError, r.content?.[0]?.text);
    const nodes = r.structuredContent.nodes as any[];
    assert.equal(nodes.length, 2); // iron_1 and cu_1 are free; iron_2 occupied
    assert.ok(nodes.every((n) => !n.occupied));
    // nearest-first from player (100,200): copper at (100,900) dist 700 vs iron at (600,200) dist 500
    assert.equal(nodes[0].id, "node_iron_1");
  } finally {
    await h.close();
  }
});

test("teleport to a hazard is refused", async () => {
  const h = await connect(false);
  try {
    const r: any = await h.client.callTool({ name: "teleport_player", arguments: { x: 66666, y: 0, z: 300 } });
    assert.equal(r.isError, true);
    assert.match(r.content[0].text, /unsafe|Refusing/i);
  } finally {
    await h.close();
  }
});

test("teleport to a good spot works", async () => {
  const h = await connect(false);
  try {
    const r: any = await h.client.callTool({ name: "teleport_player", arguments: { x: 100, y: 200, z: 300 } });
    assert.ok(!r.isError, r.content?.[0]?.text);
    assert.equal(r.structuredContent.teleported, true);
  } finally {
    await h.close();
  }
});

test("read_game_data refuses a non-read method", async () => {
  const h = await connect(false);
  try {
    const r: any = await h.client.callTool({ name: "read_game_data", arguments: { method: "world.placeBuilding" } });
    assert.equal(r.isError, true);
    assert.match(r.content[0].text, /not allowed|action/i);
  } finally {
    await h.close();
  }
});

test("list_game_methods search works", async () => {
  const h = await connect(false);
  try {
    const r: any = await h.client.callTool({ name: "list_game_methods", arguments: { search: "train" } });
    assert.ok(!r.isError);
    assert.ok(r.structuredContent.methods.length > 0);
    assert.ok(r.structuredContent.methods.every((m: any) => /train/i.test(m.method + m.summary)));
  } finally {
    await h.close();
  }
});
