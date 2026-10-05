import { z } from "zod";
import type { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { GameClient } from "../game.js";
import { GameError, explain } from "../errors.js";
import { result, errorResult, shortClass } from "../format.js";
import { asList, pick } from "./common.js";
import { verifySaveOnDisk } from "../saves.js";
import { checkTeleportSafe } from "../safety.js";

const ACTION = { readOnlyHint: false, destructiveHint: false, idempotentHint: false, openWorldHint: false };
const DESTRUCTIVE = { readOnlyHint: false, destructiveHint: true, idempotentHint: false, openWorldHint: false };
const BUILD_TIMEOUT = 120_000;

export function registerActionTools(server: McpServer, game: GameClient) {
  // --- save_game (verified) ---------------------------------------------
  server.registerTool(
    "save_game",
    {
      title: "Save the game (verified)",
      description:
        "Save the game and CONFIRM the file was actually written to disk (the game reports success even when it wasn't). Do this before any building session so the player can roll back.",
      inputSchema: { saveName: z.string().optional().describe("Save name (default auto-named by the game).") },
      annotations: { ...ACTION, idempotentHint: true },
    },
    async ({ saveName }) => {
      try {
        await game.call("world.saveGame", saveName ? { saveName } : undefined, { timeoutMs: 60_000 });
        if (!saveName)
          return result("Save requested. (No name given, so I can't verify the exact file - check in-game.)", {
            saved: true,
            verified: false,
          });
        const v = await verifySaveOnDisk(saveName);
        return result(v.note, { saved: true, verified: v.confirmed, path: v.path });
      } catch (e) {
        if (e instanceof GameError) return errorResult(explain(e));
        throw e;
      }
    },
  );

  // --- place_building ----------------------------------------------------
  server.registerTool(
    "place_building",
    {
      title: "Place a building",
      description:
        "Place a machine/foundation/pole/etc. at a position with an explicit facing (yaw degrees). Get the recipeClass from lookup_catalog. Note: placing turns the pioneer's view toward the spot.",
      inputSchema: {
        recipeClass: z.string().describe("Building/recipe class id (from lookup_catalog)."),
        x: z.number(),
        y: z.number(),
        z: z.number().optional().describe("Height; omit to drop onto the ground."),
        yaw: z.number().describe("Facing in degrees (0=+X/East, 90=+Y/North, 180=-X/West, 270=-Y/South)."),
      },
      annotations: ACTION,
    },
    async ({ recipeClass, x, y, z, yaw }) => {
      try {
        const r = await game.call(
          "world.placeBuilding",
          { recipeClass, x, y, yaw, gridSnapSize: 0, ...(z !== undefined ? { z } : {}) },
          { timeoutMs: BUILD_TIMEOUT },
        );
        return result(`Placed ${shortClass(recipeClass)}.`, { placed: true, result: r });
      } catch (e) {
        if (e instanceof GameError) return errorResult(explain(e));
        throw e;
      }
    },
  );

  // --- place_miner -------------------------------------------------------
  server.registerTool(
    "place_miner",
    {
      title: "Place a miner on a node",
      description: "Place a resource extractor (miner) on a resource node id from find_resource_nodes.",
      inputSchema: {
        nodeId: z.string().describe("Resource node id from find_resource_nodes."),
        recipeClass: z
          .string()
          .optional()
          .describe("Miner recipe (default Mk1). Look up with lookup_catalog if you need a different tier."),
      },
      annotations: ACTION,
    },
    async ({ nodeId, recipeClass }) => {
      try {
        const params: Record<string, unknown> = { nodeId };
        if (recipeClass) params.recipeClass = recipeClass;
        const r = await game.call("world.placeExtractor", params, { timeoutMs: BUILD_TIMEOUT });
        return result("Placed a miner on the node.", { placed: true, result: r });
      } catch (e) {
        if (e instanceof GameError) return errorResult(explain(e));
        throw e;
      }
    },
  );

  // --- connect_belt (with attach re-check) -------------------------------
  server.registerTool(
    "connect_belt",
    {
      title: "Connect a conveyor belt",
      description:
        "Connect a belt from a source building to a destination building. Note: this may briefly teleport the pioneer to the work site and back if they're far away. The connection is re-checked after building (the raw call can report success while unattached).",
      inputSchema: {
        sourceId: z.string(),
        destId: z.string(),
        sourceConnector: z.object({ x: z.number(), y: z.number(), z: z.number() }).optional().describe("Optional pin for which source connector to use."),
        destConnector: z.object({ x: z.number(), y: z.number(), z: z.number() }).optional(),
      },
      annotations: ACTION,
    },
    async ({ sourceId, destId, sourceConnector, destConnector }) => {
      try {
        const params: Record<string, unknown> = { sourceBuildableId: sourceId, destBuildableId: destId };
        if (sourceConnector) params.sourceConnectorPosition = sourceConnector;
        if (destConnector) params.destConnectorPosition = destConnector;
        const r = await game.call("world.connectConveyor", params, { timeoutMs: BUILD_TIMEOUT });
        // Re-check attachment.
        let attached: boolean | undefined;
        try {
          const conns = asList(
            await game.call("world.connections", { minX: -1e9, minY: -1e9, maxX: 1e9, maxY: 1e9 } as any),
            "connections",
          );
          const touching = conns.filter(
            (c) => pick(c, "buildableId", "ownerId") === sourceId || pick(c, "buildableId", "ownerId") === destId,
          );
          attached = touching.some((c) => pick(c, "isConnected", "connected"));
        } catch {
          /* best-effort */
        }
        return result(
          attached === false
            ? "Belt build reported success but I couldn't confirm it's attached - please check in-game."
            : "Belt connected.",
          { connected: true, verifiedAttached: attached, result: r },
        );
      } catch (e) {
        if (e instanceof GameError) return errorResult(explain(e));
        throw e;
      }
    },
  );

  // --- connect_power -----------------------------------------------------
  server.registerTool(
    "connect_power",
    {
      title: "Connect a power line",
      description: "Run a power line between two buildings (any distance).",
      inputSchema: { buildableIdA: z.string(), buildableIdB: z.string() },
      annotations: ACTION,
    },
    async ({ buildableIdA, buildableIdB }) => {
      try {
        const r = await game.call(
          "world.connectPower",
          { buildableIdA, buildableIdB, ignoreWireLength: true },
          { timeoutMs: BUILD_TIMEOUT },
        );
        return result("Power line connected.", { connected: true, result: r });
      } catch (e) {
        if (e instanceof GameError) return errorResult(explain(e));
        throw e;
      }
    },
  );

  // --- configure_machine -------------------------------------------------
  server.registerTool(
    "configure_machine",
    {
      title: "Configure a machine",
      description: "Set a machine's recipe and/or clock speed, and/or install power shards. Provide only the fields you want to change.",
      inputSchema: {
        buildableId: z.string(),
        recipeClass: z.string().optional().describe("New recipe (from lookup_catalog)."),
        clock: z.number().optional().describe("Clock speed % (needs shards above 100)."),
        shards: z.number().int().positive().optional().describe("Power shards to install."),
      },
      annotations: ACTION,
    },
    async ({ buildableId, recipeClass, clock, shards }) => {
      const done: string[] = [];
      try {
        if (recipeClass) {
          await game.call("world.setRecipe", { buildableId, recipeClass, ...(clock !== undefined ? { clock } : {}) });
          done.push(`recipe=${shortClass(recipeClass)}`);
        }
        if (shards) {
          await game.call("world.installPowerShard", { buildableId, count: shards });
          done.push(`+${shards} shard(s)`);
        }
        if (clock !== undefined && !recipeClass) {
          await game.call("world.setClockSpeed", { buildableId, clock });
          done.push(`clock=${clock}%`);
        } else if (clock !== undefined && recipeClass) {
          done.push(`clock=${clock}%`);
        }
        if (!done.length) return errorResult("Nothing to change - provide recipeClass, clock, and/or shards.");
        return result(`Configured machine: ${done.join(", ")}.`, { changed: done });
      } catch (e) {
        if (e instanceof GameError) return errorResult(explain(e) + (done.length ? ` (already applied: ${done.join(", ")})` : ""));
        throw e;
      }
    },
  );

  // --- remove_building ---------------------------------------------------
  server.registerTool(
    "remove_building",
    {
      title: "Remove a building",
      description: "Dismantle a building or vehicle by id. This is destructive - confirm with the player first.",
      inputSchema: { buildableId: z.string() },
      annotations: DESTRUCTIVE,
    },
    async ({ buildableId }) => {
      try {
        const r = await game.call("world.deleteBuilding", { buildableId }, { timeoutMs: 30_000 });
        return result("Removed.", { removed: true, result: r });
      } catch (e) {
        if (e instanceof GameError) return errorResult(explain(e));
        throw e;
      }
    },
  );

  // --- teleport_player (safety-checked) ----------------------------------
  server.registerTool(
    "teleport_player",
    {
      title: "Teleport the pioneer",
      description:
        "Teleport the pioneer to a location. The destination is safety-checked first (hazard volumes, kill plane, out of bounds, water/void); unsafe spots are refused unless force=true. Can be fatal if forced into a bad spot.",
      inputSchema: {
        x: z.number(),
        y: z.number(),
        z: z.number().optional(),
        yaw: z.number().optional(),
        force: z.boolean().optional().describe("Skip the safety refusal (use with care)."),
      },
      annotations: DESTRUCTIVE,
    },
    async ({ x, y, z, yaw, force }) => {
      try {
        if (!force) {
          const safety = await checkTeleportSafe(game, x, y, z);
          if (!safety.safe)
            return errorResult(
              `Refusing to teleport - the destination looks unsafe (${safety.reasons.join(", ")}). ` +
                `Pick a solid spot, or pass force=true if you're sure.`,
            );
        }
        const params: Record<string, unknown> = { x, y };
        if (z !== undefined) params.z = z;
        if (yaw !== undefined) params.yaw = yaw;
        const r = await game.call("world.teleportPlayer", params, { timeoutMs: 30_000 });
        return result("Teleported the pioneer.", { teleported: true, result: r });
      } catch (e) {
        if (e instanceof GameError) return errorResult(explain(e));
        throw e;
      }
    },
  );

  // --- take_photo (always exits photo mode) ------------------------------
  server.registerTool(
    "take_photo",
    {
      title: "Take a photo",
      description:
        "Frame and capture a high-res photo with the free photo camera, then leave photo mode (so later building still works). Optionally set the time of day first.",
      inputSchema: {
        x: z.number(),
        y: z.number(),
        z: z.number(),
        pitch: z.number().describe("Camera pitch (look up/down), degrees."),
        yaw: z.number().describe("Camera yaw (compass facing), degrees."),
        timeOfDay: z.number().optional().describe("Optional hour 0-24 to set before the shot."),
      },
      annotations: { ...ACTION, idempotentHint: true },
    },
    async ({ x, y, z, pitch, yaw, timeOfDay }) => {
      try {
        if (timeOfDay !== undefined) {
          try {
            await game.call("world.setTimeOfDay", { time: timeOfDay });
          } catch {
            /* non-fatal */
          }
        }
        await game.call("world.setPhotoCamera", { x, y, z, pitch, yaw }, { timeoutMs: 30_000 });
        let photo: any;
        try {
          photo = await game.call("world.takePhoto", undefined, { timeoutMs: 30_000 });
        } finally {
          // ALWAYS leave photo mode, or subsequent builds fail.
          try {
            await game.call("world.exitPhotoMode");
          } catch {
            /* ignore */
          }
        }
        const dir = pick(photo, "dir", "path");
        return result(`Photo taken${dir ? ` (saved in ${dir})` : ""}. Left photo mode.`, { photo });
      } catch (e) {
        // Make sure we're out of photo mode even on failure.
        try {
          await game.call("world.exitPhotoMode");
        } catch {
          /* ignore */
        }
        if (e instanceof GameError) return errorResult(explain(e));
        throw e;
      }
    },
  );

  // --- set_train_route ---------------------------------------------------
  server.registerTool(
    "set_train_route",
    {
      title: "Set a train's route",
      description: "Give a train a timetable (stations to visit, in order) and turn self-driving on or off.",
      inputSchema: {
        trainId: z.string().describe("Train id from get_logistics (the BP_Train_C, not the locomotive)."),
        stationIds: z.array(z.string()).describe("Station buildable ids to visit, in order."),
        selfDriving: z.boolean().optional().describe("Enable self-driving (default true)."),
      },
      annotations: ACTION,
    },
    async ({ trainId, stationIds, selfDriving }) => {
      try {
        const stops = stationIds.map((id) => ({ stationBuildableId: id, dockingDefinition: "LoadUnloadOnce" }));
        await game.call("world.setTrainTimetable", { trainId, stops });
        const enabled = selfDriving !== false;
        const sd = await game.call("world.setTrainSelfDriving", { trainId, enabled });
        return result(`Route set (${stationIds.length} stops); self-driving ${enabled ? "on" : "off"}.`, { result: sd });
      } catch (e) {
        if (e instanceof GameError) return errorResult(explain(e));
        throw e;
      }
    },
  );
}
