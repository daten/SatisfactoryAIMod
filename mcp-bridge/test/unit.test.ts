import { test } from "node:test";
import assert from "node:assert/strict";
import { promises as fs } from "node:fs";
import * as os from "node:os";
import * as path from "node:path";

import { CATALOG } from "../src/catalog.js";
import { READ_ONLY, CREATIVE, isReadOnly, actionMethods, ALL_METHODS } from "../src/classify.js";
import { GameClient } from "../src/game.js";
import { GameError, ConnectionError, explain } from "../src/errors.js";
import { shortClass, limit, capPayload } from "../src/format.js";
import { verifySaveOnDisk } from "../src/saves.js";
import { checkTeleportSafe } from "../src/safety.js";
import { startMockGame, defaultHandlers } from "./mock-game.js";

test("catalog loaded with all methods", () => {
  assert.ok(CATALOG.methods.length >= 130);
  assert.equal(ALL_METHODS.size, CATALOG.methods.length);
});

test("every catalog method is classified read or action, never both", () => {
  const actions = new Set(actionMethods());
  for (const m of CATALOG.methods) {
    const r = READ_ONLY.has(m.method);
    const a = actions.has(m.method);
    assert.ok(r !== a, `${m.method} must be exactly one of read/action (read=${r} action=${a})`);
  }
  // READ_ONLY must not contain anything unknown.
  for (const m of READ_ONLY) assert.ok(ALL_METHODS.has(m), `READ_ONLY has unknown method ${m}`);
});

test("read-only set excludes obvious writers and dry-runs", () => {
  for (const m of ["world.placeBuilding", "world.deleteBuilding", "world.teleportPlayer", "world.connectConveyor",
    "world.cleanupOrphanedFlowIndicators", "world.batch", "world.testRailroadTrack", "world.saveGame"]) {
    assert.equal(isReadOnly(m), false, `${m} should NOT be read-only`);
  }
  for (const m of ["world.buildables", "world.connections", "world.resourceNodes", "world.player", "world.version"]) {
    assert.equal(isReadOnly(m), true, `${m} should be read-only`);
  }
});

test("creative set matches catalog flags (11)", () => {
  assert.equal(CREATIVE.size, CATALOG.methods.filter((m) => m.creative).length);
  assert.ok(CREATIVE.has("world.addItemsToPlayerInventory"));
});

test("format helpers", () => {
  assert.equal(shortClass("/Game/X.Build_SmelterMk1_C"), "Build_SmelterMk1_C");
  const l = limit([1, 2, 3, 4, 5], 2);
  assert.equal(l.shown, 2);
  assert.equal(l.truncated, true);
  const big = capPayload({ s: "x".repeat(60000) }) as any;
  assert.ok(big.error, "oversized payload is capped");
});

test("GameClient: success, error mapping, serialized queue", async () => {
  const mock = await startMockGame(defaultHandlers());
  try {
    const game = new GameClient({ url: mock.url });
    const v = await game.call("world.version");
    assert.equal(v.modVersion, "0.3.3");

    await assert.rejects(
      () => game.call("world.nope"),
      (e: any) => e instanceof GameError && e.code === "INVALID_REQUEST",
    );

    // serialization: fire 5 concurrently, mock records order; all resolve
    const results = await Promise.all([1, 2, 3, 4, 5].map(() => game.call("world.version")));
    assert.equal(results.length, 5);
  } finally {
    await mock.close();
  }
});

test("GameClient: connection refused -> ConnectionError", async () => {
  const game = new GameClient({ url: "http://127.0.0.1:59999/rpc", defaultTimeoutMs: 2000 });
  await assert.rejects(
    () => game.call("world.version"),
    (e: any) => e instanceof ConnectionError,
  );
});

test("error explain gives friendly text", () => {
  assert.match(explain(new GameError("CREATIVE_DISABLED", "x")), /Allow Creative Features/);
  assert.match(explain(new ConnectionError("down")), /reach the game/i);
});

test("safe teleport refuses hazard / ocean, allows good spot", async () => {
  const mock = await startMockGame(defaultHandlers());
  try {
    const game = new GameClient({ url: mock.url });
    const good = await checkTeleportSafe(game, 100, 200, 300);
    assert.equal(good.safe, true, JSON.stringify(good));
    const hazard = await checkTeleportSafe(game, 66666, 0, 300);
    assert.equal(hazard.safe, false);
    const ocean = await checkTeleportSafe(game, -99999, 0);
    assert.equal(ocean.safe, false);
  } finally {
    await mock.close();
  }
});

test("save verification finds a fresh file", async () => {
  // Build a fake SaveGames tree under a temp LOCALAPPDATA.
  const tmp = await fs.mkdtemp(path.join(os.tmpdir(), "aimod-save-"));
  const saveDir = path.join(tmp, "FactoryGame", "Saved", "SaveGames", "76561190000000000");
  await fs.mkdir(saveDir, { recursive: true });
  await fs.writeFile(path.join(saveDir, "my-test-save.sav"), "x");
  const prev = process.env.LOCALAPPDATA;
  process.env.LOCALAPPDATA = tmp;
  try {
    const ok = await verifySaveOnDisk("my-test-save");
    assert.equal(ok.confirmed, true, ok.note);
    const miss = await verifySaveOnDisk("does-not-exist");
    assert.equal(miss.confirmed, false);
  } finally {
    if (prev === undefined) delete process.env.LOCALAPPDATA;
    else process.env.LOCALAPPDATA = prev;
    await fs.rm(tmp, { recursive: true, force: true });
  }
});
