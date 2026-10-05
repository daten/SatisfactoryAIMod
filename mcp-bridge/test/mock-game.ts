// A tiny stand-in for the mod's loopback endpoint, for tests (no game needed).
import * as http from "node:http";

export type Handler = (params: any) => any; // return result object, or throw {code,message}

export interface MockGame {
  url: string;
  calls: Array<{ method: string; params: any }>;
  setHandler(method: string, h: Handler): void;
  close(): Promise<void>;
}

export async function startMockGame(initial: Record<string, Handler> = {}): Promise<MockGame> {
  const handlers = new Map<string, Handler>(Object.entries(initial));
  const calls: MockGame["calls"] = [];

  const server = http.createServer((req, res) => {
    if (req.method !== "POST" || !req.url?.startsWith("/rpc")) {
      res.writeHead(404).end();
      return;
    }
    let body = "";
    req.on("data", (c) => (body += c));
    req.on("end", () => {
      let msg: any = {};
      try {
        msg = JSON.parse(body);
      } catch {
        res.writeHead(400).end();
        return;
      }
      calls.push({ method: msg.method, params: msg.params });
      const h = handlers.get(msg.method);
      const send = (obj: any) => {
        res.writeHead(200, { "Content-Type": "application/json" });
        res.end(JSON.stringify(obj));
      };
      if (!h) {
        send({ success: false, error: { code: "INVALID_REQUEST", message: `no mock for ${msg.method}` } });
        return;
      }
      try {
        const result = h(msg.params ?? {});
        send({ success: true, result: result ?? {} });
      } catch (e: any) {
        send({ success: false, error: { code: e?.code ?? "INTERNAL_ERROR", message: e?.message ?? String(e) } });
      }
    });
  });

  await new Promise<void>((resolve) => server.listen(0, "127.0.0.1", resolve));
  const addr = server.address();
  const port = typeof addr === "object" && addr ? addr.port : 0;

  return {
    url: `http://127.0.0.1:${port}/rpc`,
    calls,
    setHandler: (m, h) => handlers.set(m, h),
    close: () => new Promise((resolve) => server.close(() => resolve())),
  };
}

/** A reasonable default fixture set. */
export function defaultHandlers(): Record<string, Handler> {
  return {
    "world.version": () => ({ modVersion: "0.3.3", buildStamp: "test", buildConfig: "Shipping" }),
    "world.player": () => ({ position: { x: 100, y: 200, z: 300 }, rotation: { yaw: 0 } }),
    "world.playerInventory": () => ({ items: [{ itemClass: "Desc_IronIngot_C", amount: 42 }] }),
    "world.resourceNodes": () => ({
      nodes: [
        { id: "node_iron_1", resource: "Desc_OreIron_C", purity: "normal", occupied: false, position: { x: 600, y: 200, z: 0 } },
        { id: "node_iron_2", resource: "Desc_OreIron_C", purity: "pure", occupied: true, position: { x: 5000, y: 200, z: 0 } },
        { id: "node_cu_1", resource: "Desc_OreCopper_C", purity: "impure", occupied: false, position: { x: 100, y: 900, z: 0 } },
      ],
    }),
    "world.manufacturers": () => ({
      manufacturers: [
        { id: "m1", buildableClass: "Build_SmelterMk1_C", recipe: "Recipe_IngotIron_C", clock: 100, productionStatus: "Producing", position: { x: 110, y: 210, z: 0 } },
        { id: "m2", buildableClass: "Build_ConstructorMk1_C", recipe: "", clock: 100, productionStatus: "Error", position: { x: 300, y: 210, z: 0 } },
      ],
    }),
    "world.powerPoles": () => ({ powerPoles: [{ id: "p1", type: "PowerPoleMk1", hasPower: true }, { id: "p2", hasPower: false }] }),
    "world.saveGame": () => ({}),
    "world.groundHeight": (p) => ({ z: p?.x === -99999 ? -24400 : 120, traceMethod: "WorldStatic" }),
    "world.probeHazard": (p) => ({
      insideDamageVolume: p?.x === 66666,
      belowKillZ: false,
      insideWorldBounds2D: p?.x !== 77777,
      nearestOtherVolumeDistance: 99999,
    }),
    "world.constructionCost": () => ({ items: [{ itemClass: "Desc_IronPlate_C", amount: 5 }] }),
    "world.placeBuilding": () => ({ buildableId: "new_building_1" }),
    "world.teleportPlayer": () => ({ ok: true }),
  };
}
