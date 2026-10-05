// Generic passthrough tools: discover any method, read any read-only method, and
// (standard mode only) perform any action method. These are the escape hatch so
// the full 133-method API stays reachable without 133 tools.

import { z } from "zod";
import type { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { GameClient } from "./game.js";
import { CATALOG } from "./catalog.js";
import { isReadOnly, isCreative, isKnownMethod } from "./classify.js";
import { GameError } from "./errors.js";
import { explain } from "./errors.js";
import { result, errorResult, limit } from "./format.js";

export interface GenericOptions {
  readOnlyMode: boolean;
}

export function registerGenericTools(server: McpServer, game: GameClient, opts: GenericOptions) {
  server.registerTool(
    "list_game_methods",
    {
      title: "List game methods",
      description:
        "Search the full catalog of low-level game methods (there are ~133). Use this to discover a capability that the friendly tools don't cover, then call it with read_game_data (reads) or perform_game_action (changes). Returns each method's name, whether it only reads, whether it's a creative/cheat feature, its parameters, and a one-line summary.",
      inputSchema: {
        search: z
          .string()
          .optional()
          .describe("Filter by text in the method name or summary, e.g. 'train', 'pipe', 'milestone'."),
        limit: z.number().int().positive().optional().describe("Max rows (default 40)."),
      },
      annotations: { readOnlyHint: true, destructiveHint: false, idempotentHint: true, openWorldHint: false },
    },
    async ({ search, limit: lim }) => {
      const q = (search ?? "").toLowerCase();
      const matched = CATALOG.methods.filter(
        (m) => !q || m.method.toLowerCase().includes(q) || m.summary.toLowerCase().includes(q),
      );
      const { items, total, truncated, note } = limit(matched, lim ?? 40);
      const rows = items.map((m) => ({
        method: m.method,
        readOnly: isReadOnly(m.method),
        creative: !!m.creative,
        params: m.params.map((p) => `${p.name}${p.required ? "" : "?"}:${p.type}`),
        summary: m.summary,
      }));
      return result(`${total} method(s) match${total === 1 ? "es" : ""}${note ? " (" + note + ")" : ""}.`, {
        methods: rows,
        total,
        truncated,
      });
    },
  );

  server.registerTool(
    "read_game_data",
    {
      title: "Read game data (any read-only method)",
      description:
        "Call any read-only game method by name (from list_game_methods) and get its raw result. Only methods that cannot change the world or move the pioneer are allowed here; anything else is refused. Use the friendly read tools first - this is for cases they don't cover.",
      inputSchema: {
        method: z.string().describe("Full method name, e.g. 'world.pipeFluidBoxes'."),
        params: z.record(z.any()).optional().describe("Parameters object for the method, if any."),
      },
      annotations: { readOnlyHint: true, destructiveHint: false, idempotentHint: true, openWorldHint: false },
    },
    async ({ method, params }) => {
      if (!isKnownMethod(method))
        return errorResult(`Unknown method "${method}". Use list_game_methods to find the right name.`);
      if (!isReadOnly(method))
        return errorResult(
          `"${method}" can change the world or move the pioneer, so it's not allowed here. Use perform_game_action for actions.`,
        );
      try {
        const data = await game.call(method, params);
        return result(`Read ${method}.`, { method, result: data });
      } catch (e) {
        if (e instanceof GameError) return errorResult(explain(e));
        throw e;
      }
    },
  );

  if (!opts.readOnlyMode) {
    server.registerTool(
      "perform_game_action",
      {
        title: "Perform a game action (any method)",
        description:
          "Escape hatch: call any game method that the friendly action tools don't cover (e.g. pipes, hypertubes, rail, drones, milestones, M.A.M.). Prefer the specific tools when one exists. This can change the world or move the pioneer, so confirm with the player first for anything large or destructive. Creative/cheat methods will be refused by the mod unless the player enabled them in settings.",
        inputSchema: {
          method: z.string().describe("Full method name, e.g. 'world.connectPipe'."),
          params: z.record(z.any()).optional().describe("Parameters object for the method."),
        },
        annotations: { readOnlyHint: false, destructiveHint: true, idempotentHint: false, openWorldHint: false },
      },
      async ({ method, params }) => {
        if (!isKnownMethod(method))
          return errorResult(`Unknown method "${method}". Use list_game_methods to find the right name.`);
        try {
          const data = await game.call(method, params, { timeoutMs: 120_000 });
          const creativeNote = isCreative(method) ? " (creative feature)" : "";
          return result(`Ran ${method}${creativeNote}.`, { method, result: data });
        } catch (e) {
          if (e instanceof GameError) return errorResult(explain(e));
          throw e;
        }
      },
    );
  }
}
