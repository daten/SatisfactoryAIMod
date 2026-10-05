import { z } from "zod";
import type { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { GameClient } from "../game.js";
import { GameError, explain } from "../errors.js";
import { result, errorResult, limit, shortClass, xyz } from "../format.js";
import { asList, pick, pos, dist2d, resolveCenter, Vec } from "./common.js";

const READ = { readOnlyHint: true, destructiveHint: false, idempotentHint: true, openWorldHint: false };

const nearSchema = z
  .union([z.literal("player"), z.object({ x: z.number(), y: z.number(), z: z.number().optional() })])
  .optional()
  .describe("Center for distance filtering: 'player' (default) or {x,y}.");

/** Attach distance + sort when a center is known. */
function withDistance<T extends { _p?: Vec }>(items: T[], center: Vec | undefined) {
  if (!center) return items;
  for (const it of items) (it as any)._dist = dist2d(center, it._p);
  items.sort((a, b) => ((a as any)._dist ?? 1e12) - ((b as any)._dist ?? 1e12));
  return items;
}

export function registerReadTools(server: McpServer, game: GameClient) {
  // --- game_status -------------------------------------------------------
  server.registerTool(
    "game_status",
    {
      title: "Game status / are we connected?",
      description:
        "Check that the game is running with the mod and a save is loaded. Returns the mod version and the pioneer's position. Call this first. If the pioneer position is (0,0,0) the player is probably driving a vehicle (building and position reads won't work until they step out).",
      inputSchema: {},
      annotations: READ,
    },
    async () => {
      try {
        const v = await game.version();
        let playerLine = "";
        let driving = false;
        try {
          const p = await game.call("world.player", undefined, { timeoutMs: 8000 });
          const vp = pos(p);
          if (vp && vp.x === 0 && vp.y === 0 && vp.z === 0) {
            driving = true;
            playerLine = " Pioneer reads (0,0,0) - you're likely in a vehicle; step out to build.";
          } else if (vp) {
            playerLine = ` Pioneer at (${Math.round(vp.x)}, ${Math.round(vp.y)}, ${Math.round(vp.z)}).`;
          }
        } catch {
          /* non-fatal */
        }
        return result(
          `Connected. Mod ${v.modVersion ?? "?"} (${v.buildConfig ?? "?"}).${playerLine}`,
          { connected: true, version: v, driving },
        );
      } catch (e) {
        if (e instanceof GameError) return errorResult(explain(e));
        throw e;
      }
    },
  );

  // --- get_player --------------------------------------------------------
  server.registerTool(
    "get_player",
    {
      title: "Get pioneer position & inventory",
      description: "The pioneer's current position plus a summary of carried items.",
      inputSchema: {},
      annotations: READ,
    },
    async () => {
      try {
        const p = await game.call("world.player");
        const v = pos(p);
        let inv: any[] = [];
        try {
          inv = asList(await game.call("world.playerInventory"), "items", "inventory", "contents");
        } catch {
          /* ignore */
        }
        const items = inv
          .map((it) => ({ item: shortClass(pick(it, "itemClass", "class", "item")), amount: pick<number>(it, "amount", "count", "quantity") }))
          .filter((it) => it.amount);
        const driving = v && v.x === 0 && v.y === 0 && v.z === 0;
        return result(
          driving
            ? "Pioneer reads (0,0,0) - you're likely in a vehicle."
            : `Pioneer at ${v ? `(${Math.round(v.x)}, ${Math.round(v.y)}, ${Math.round(v.z)})` : "unknown"}, carrying ${items.length} item type(s).`,
          { position: xyz(v), driving: !!driving, inventory: items },
        );
      } catch (e) {
        if (e instanceof GameError) return errorResult(explain(e));
        throw e;
      }
    },
  );

  // --- find_resource_nodes ----------------------------------------------
  server.registerTool(
    "find_resource_nodes",
    {
      title: "Find resource nodes",
      description:
        "List resource nodes/deposits, optionally filtered by resource, purity, whether they're free (no miner yet), and distance from a center. Sorted nearest-first when a center is known. Use the returned node id with place_miner.",
      inputSchema: {
        resource: z.string().optional().describe("Filter by resource name, e.g. 'iron', 'copper', 'coal', 'oil'."),
        purity: z.enum(["impure", "normal", "pure"]).optional(),
        freeOnly: z.boolean().optional().describe("Only nodes with no miner on them."),
        near: nearSchema,
        radius: z.number().positive().optional().describe("Only within this distance (units) of the center."),
        limit: z.number().int().positive().optional(),
      },
      annotations: READ,
    },
    async ({ resource, purity, freeOnly, near, radius, limit: lim }) => {
      try {
        const center = await resolveCenter(game, near);
        let nodes = asList(await game.call("world.resourceNodes"), "nodes", "resourceNodes").map((n) => ({
          id: pick(n, "id", "nodeId"),
          resource: shortClass(pick(n, "resource", "type", "resourceClass", "itemClass")),
          purity: String(pick(n, "purity") ?? "").toLowerCase(),
          occupied: !!pick(n, "occupied", "isOccupied"),
          _p: pos(n),
          position: xyz(pos(n)),
        }));
        if (resource) {
          const q = resource.toLowerCase();
          nodes = nodes.filter((n) => n.resource.toLowerCase().includes(q));
        }
        if (purity) nodes = nodes.filter((n) => n.purity === purity);
        if (freeOnly) nodes = nodes.filter((n) => !n.occupied);
        withDistance(nodes, center);
        if (radius && center) nodes = nodes.filter((n) => ((n as any)._dist ?? 1e12) <= radius);
        const out = nodes.map(({ _p, ...rest }) => ({ ...rest, distance: (rest as any)._dist }));
        const { items, total, truncated, note } = limit(out, lim);
        return result(
          `${total} node(s)${resource ? " of " + resource : ""}${freeOnly ? " free" : ""}${note ? " - " + note : ""}.`,
          { nodes: items, total, truncated },
        );
      } catch (e) {
        if (e instanceof GameError) return errorResult(explain(e));
        throw e;
      }
    },
  );

  // --- list_buildings ----------------------------------------------------
  server.registerTool(
    "list_buildings",
    {
      title: "List placed buildings",
      description:
        "List buildings in an area (required unless count_only), optionally filtered by type. Provide a center (near/radius) or a box. Returns id, type, and position; use count_only for just totals-by-type.",
      inputSchema: {
        near: nearSchema,
        radius: z.number().positive().optional().describe("Area radius around the center (units)."),
        box: z
          .object({ minX: z.number(), minY: z.number(), maxX: z.number(), maxY: z.number() })
          .optional()
          .describe("Explicit 2D box instead of near/radius."),
        type: z.string().optional().describe("Filter by building type substring, e.g. 'Smelter', 'Foundation'."),
        countOnly: z.boolean().optional().describe("Return only counts grouped by type."),
        limit: z.number().int().positive().optional(),
      },
      annotations: READ,
    },
    async ({ near, radius, box, type, countOnly, limit: lim }) => {
      try {
        const center = await resolveCenter(game, near);
        let params: Record<string, unknown> | undefined;
        if (box) params = { minX: box.minX, minY: box.minY, maxX: box.maxX, maxY: box.maxY };
        else if (center && radius)
          params = { minX: center.x - radius, minY: center.y - radius, maxX: center.x + radius, maxY: center.y + radius };
        if (!params && !countOnly)
          return errorResult("Give an area (near + radius, or a box), or set countOnly to list every building's counts.");
        let blds = asList(await game.call("world.buildables", params), "buildables").map((b) => ({
          id: pick(b, "id"),
          type: shortClass(pick(b, "buildableClass", "class")),
          _p: pos(b),
          position: xyz(pos(b)),
        }));
        if (type) {
          const q = type.toLowerCase();
          blds = blds.filter((b) => b.type.toLowerCase().includes(q));
        }
        const counts: Record<string, number> = {};
        for (const b of blds) counts[b.type] = (counts[b.type] ?? 0) + 1;
        if (countOnly) {
          const sorted = Object.entries(counts).sort((a, b) => b[1] - a[1]).map(([t, c]) => ({ type: t, count: c }));
          return result(`${blds.length} building(s), ${sorted.length} type(s).`, { total: blds.length, byType: sorted });
        }
        withDistance(blds, center);
        const out = blds.map(({ _p, ...rest }) => ({ ...rest, distance: (rest as any)._dist }));
        const { items, total, truncated, note } = limit(out, lim);
        return result(`${total} building(s)${note ? " - " + note : ""}.`, { buildings: items, total, truncated, byType: counts });
      } catch (e) {
        if (e instanceof GameError) return errorResult(explain(e));
        throw e;
      }
    },
  );

  // --- get_machines ------------------------------------------------------
  server.registerTool(
    "get_machines",
    {
      title: "Get production machines",
      description:
        "List production machines with recipe, clock speed, and status. Status meaning: 'Producing' = running, 'Standby' = powered but starved of inputs, 'Error' = unpowered or no recipe. Filter by type or status.",
      inputSchema: {
        type: z.string().optional().describe("Filter by machine type, e.g. 'Smelter', 'Constructor'."),
        status: z.enum(["producing", "standby", "error"]).optional(),
        near: nearSchema,
        radius: z.number().positive().optional(),
        limit: z.number().int().positive().optional(),
      },
      annotations: READ,
    },
    async ({ type, status, near, radius, limit: lim }) => {
      try {
        const center = await resolveCenter(game, near);
        let m = asList(await game.call("world.manufacturers"), "manufacturers", "machines").map((x) => ({
          id: pick(x, "id"),
          type: shortClass(pick(x, "buildableClass", "class")),
          recipe: shortClass(pick(x, "recipe", "recipeClass") ?? ""),
          clock: pick<number>(x, "clock", "clockSpeed", "potential"),
          status: String(pick(x, "productionStatus", "status") ?? ""),
          _p: pos(x),
          position: xyz(pos(x)),
        }));
        if (type) {
          const q = type.toLowerCase();
          m = m.filter((x) => x.type.toLowerCase().includes(q));
        }
        if (status) m = m.filter((x) => x.status.toLowerCase() === status);
        withDistance(m, center);
        if (radius && center) m = m.filter((x) => ((x as any)._dist ?? 1e12) <= radius);
        const byStatus: Record<string, number> = {};
        for (const x of m) byStatus[x.status || "?"] = (byStatus[x.status || "?"] ?? 0) + 1;
        const out = m.map(({ _p, ...rest }) => ({ ...rest, distance: (rest as any)._dist }));
        const { items, total, truncated, note } = limit(out, lim);
        return result(`${total} machine(s)${note ? " - " + note : ""}. Status: ${JSON.stringify(byStatus)}.`, {
          machines: items,
          total,
          truncated,
          byStatus,
        });
      } catch (e) {
        if (e instanceof GameError) return errorResult(explain(e));
        throw e;
      }
    },
  );

  // --- get_power ---------------------------------------------------------
  server.registerTool(
    "get_power",
    {
      title: "Get power status",
      description: "Summary of power poles and whether they have power (powered vs unpowered).",
      inputSchema: { limit: z.number().int().positive().optional() },
      annotations: READ,
    },
    async ({ limit: lim }) => {
      try {
        const poles = asList(await game.call("world.powerPoles"), "powerPoles", "poles");
        const powered = poles.filter((p) => pick(p, "hasPower", "powered")).length;
        const rows = poles.map((p) => ({
          id: pick(p, "id", "buildableId"),
          type: shortClass(pick(p, "type", "buildableClass") ?? ""),
          hasPower: !!pick(p, "hasPower", "powered"),
        }));
        const { items, total, truncated } = limit(rows, lim);
        return result(`${powered}/${total} power poles have power.`, { powered, total, poles: items, truncated });
      } catch (e) {
        if (e instanceof GameError) return errorResult(explain(e));
        throw e;
      }
    },
  );

  // --- get_logistics -----------------------------------------------------
  server.registerTool(
    "get_logistics",
    {
      title: "Get trains / trucks / drones",
      description: "Summarize logistics: trains (and self-driving state), train stations, road vehicles, drone stations, truck stations.",
      inputSchema: {
        kind: z.enum(["trains", "stations", "vehicles", "drones", "trucks", "all"]).optional().describe("Default 'all'."),
      },
      annotations: READ,
    },
    async ({ kind }) => {
      const want = kind ?? "all";
      const out: Record<string, unknown> = {};
      const summary: string[] = [];
      const grab = async (key: string, method: string, listKeys: string[]) => {
        if (want !== "all" && want !== key) return;
        try {
          const list = asList(await game.call(method), ...listKeys);
          out[key] = limit(list, 100).items;
          summary.push(`${list.length} ${key}`);
        } catch (e) {
          if (e instanceof GameError) out[key] = { error: explain(e) };
          else throw e;
        }
      };
      await grab("trains", "world.trains", ["trains"]);
      await grab("stations", "world.trainStations", ["stations", "trainStations"]);
      await grab("vehicles", "world.vehicles", ["vehicles"]);
      await grab("drones", "world.droneStations", ["droneStations", "stations"]);
      await grab("trucks", "world.truckStations", ["truckStations", "stations"]);
      return result(summary.join(", ") || "no logistics data", out);
    },
  );

  // --- get_progression ---------------------------------------------------
  server.registerTool(
    "get_progression",
    {
      title: "Get progression (milestones / research / Depot)",
      description: "What's unlocked and what's next: HUB milestones & tech tier, M.A.M. research, and Dimensional Depot contents.",
      inputSchema: {},
      annotations: READ,
    },
    async () => {
      const out: Record<string, unknown> = {};
      for (const [key, method] of [
        ["milestones", "world.milestoneProgress"],
        ["mam", "world.mamStatus"],
        ["depot", "world.centralStorage"],
      ] as const) {
        try {
          out[key] = await game.call(method);
        } catch (e) {
          if (e instanceof GameError) out[key] = { error: explain(e) };
          else throw e;
        }
      }
      return result("Progression snapshot (milestones, M.A.M., Depot).", out);
    },
  );

  // --- lookup_catalog ----------------------------------------------------
  server.registerTool(
    "lookup_catalog",
    {
      title: "Look up recipe / item / building classes",
      description:
        "Search the game's recipes, items, or buildings by name to get the exact class id that other tools need (e.g. search 'smelter' -> Build_SmelterMk1_C, 'copper wire' -> its recipe class). Always resolve names to ids here before building.",
      inputSchema: {
        search: z.string().describe("Name to search for, e.g. 'constructor', 'iron rod', 'copper ingot'."),
        kind: z.enum(["recipe", "item", "building"]).optional().describe("Which catalog (default: all)."),
        limit: z.number().int().positive().optional(),
      },
      annotations: READ,
    },
    async ({ search, kind, limit: lim }) => {
      const q = search.toLowerCase();
      const out: Record<string, unknown> = {};
      const matchList = (list: any[]) =>
        list
          .map((x) => ({
            class: pick(x, "class", "recipeClass", "itemClass", "buildableClass"),
            name: pick(x, "name", "displayName"),
          }))
          .filter((x) => `${x.name ?? ""} ${shortClass(x.class)}`.toLowerCase().includes(q));
      const sources: Array<[string, string, string[]]> = [];
      if (!kind || kind === "building") sources.push(["buildings", "world.buildableCatalog", ["buildables", "catalog"]]);
      if (!kind || kind === "recipe") sources.push(["recipes", "world.recipeCatalog", ["recipes", "catalog"]]);
      if (!kind || kind === "item") sources.push(["items", "world.itemCatalog", ["items", "catalog"]]);
      let totalMatches = 0;
      for (const [key, method, keys] of sources) {
        try {
          const matched = matchList(asList(await game.call(method), ...keys));
          const { items } = limit(matched, lim ?? 20);
          out[key] = items;
          totalMatches += matched.length;
        } catch (e) {
          if (e instanceof GameError) out[key] = { error: explain(e) };
          else throw e;
        }
      }
      return result(`${totalMatches} match(es) for "${search}".`, out);
    },
  );

  // --- get_build_cost ----------------------------------------------------
  server.registerTool(
    "get_build_cost",
    {
      title: "Get build cost",
      description: "The real material cost to build a given recipe/building, before building it.",
      inputSchema: {
        recipeClass: z.string().describe("The recipe/building class id (from lookup_catalog)."),
      },
      annotations: READ,
    },
    async ({ recipeClass }) => {
      try {
        const data = await game.call("world.constructionCost", { recipeClass });
        return result(`Build cost for ${shortClass(recipeClass)}.`, { recipeClass, cost: data });
      } catch (e) {
        if (e instanceof GameError) return errorResult(explain(e));
        throw e;
      }
    },
  );

  // --- check_location ----------------------------------------------------
  server.registerTool(
    "check_location",
    {
      title: "Check a location (ground / hazard)",
      description: "Is a spot solid ground, water/void, a hazard zone, or out of bounds? Returns ground height and hazard flags.",
      inputSchema: { x: z.number(), y: z.number(), z: z.number().optional() },
      annotations: READ,
    },
    async ({ x, y, z }) => {
      try {
        const ground = await game.call("world.groundHeight", { x, y });
        let hazard: any = {};
        try {
          hazard = await game.call("world.probeHazard", { x, y, z: z ?? pick(ground, "z") ?? 10000 });
        } catch {
          /* ignore */
        }
        const gz = pick<number>(ground, "z");
        const problems: string[] = [];
        if (hazard?.insideDamageVolume) problems.push("in a hazard volume");
        if (hazard?.belowKillZ) problems.push("below kill plane");
        if (hazard?.insideWorldBounds2D === false) problems.push("out of bounds");
        if (typeof gz === "number" && gz <= -20000) problems.push("ocean/void");
        return result(
          problems.length ? `Caution: ${problems.join(", ")}.` : `Looks OK. Ground z ~ ${gz ?? "?"}.`,
          { groundZ: gz, hazard, problems },
        );
      } catch (e) {
        if (e instanceof GameError) return errorResult(explain(e));
        throw e;
      }
    },
  );
}
