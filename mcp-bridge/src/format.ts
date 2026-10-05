// Output shaping: keep tool responses small and readable for the model.

export const DEFAULT_LIMIT = 50;
export const MAX_LIMIT = 500;
export const MAX_PAYLOAD_BYTES = 50_000;

/** Shorten a FactoryGame class path to its leaf name (Build_SmelterMk1_C). */
export function shortClass(path: unknown): string {
  if (typeof path !== "string") return String(path ?? "");
  const afterDot = path.includes(".") ? path.split(".").pop()! : path;
  return afterDot.replace(/'$/, "");
}

/** Round a coordinate-ish number to whole units. */
export function roundCoord(n: unknown): number | undefined {
  if (typeof n !== "number" || !isFinite(n)) return undefined;
  return Math.round(n);
}

export function xyz(p: any): { x?: number; y?: number; z?: number } | undefined {
  if (!p || typeof p !== "object") return undefined;
  const x = roundCoord(p.x), y = roundCoord(p.y), z = roundCoord(p.z);
  if (x === undefined && y === undefined && z === undefined) return undefined;
  return { x: x!, y: y!, z: z! };
}

export function dist2d(a: any, b: any): number | undefined {
  if (!a || !b) return undefined;
  const dx = (a.x ?? 0) - (b.x ?? 0);
  const dy = (a.y ?? 0) - (b.y ?? 0);
  return Math.round(Math.hypot(dx, dy));
}

export interface Limited<T> {
  items: T[];
  total: number;
  shown: number;
  truncated: boolean;
  note?: string;
}

/** Cap a list; attach a human note when truncated. */
export function limit<T>(all: T[], max = DEFAULT_LIMIT): Limited<T> {
  const capped = Math.min(Math.max(1, max), MAX_LIMIT);
  const items = all.slice(0, capped);
  const truncated = all.length > items.length;
  return {
    items,
    total: all.length,
    shown: items.length,
    truncated,
    note: truncated
      ? `Showing ${items.length} of ${all.length}. Narrow the filter (area/type) to see the rest.`
      : undefined,
  };
}

/** Enforce a hard payload cap on structured content as a last resort. */
export function capPayload<T>(obj: T): T | { error: string; hint: string } {
  const json = JSON.stringify(obj);
  if (json.length <= MAX_PAYLOAD_BYTES) return obj;
  return {
    error: "Result too large to return.",
    hint: "Add a tighter filter (a smaller area, a type, or a lower limit).",
  } as any;
}

/** A standard MCP tool result: a one-line text summary + structured data. */
export function result(summary: string, data?: unknown) {
  const structured = data === undefined ? undefined : capPayload({ summary, ...(data as object) });
  return {
    content: [{ type: "text" as const, text: summary }],
    ...(structured ? { structuredContent: structured as Record<string, unknown> } : {}),
  };
}

/** A tool error result (isError: true) with a friendly message. */
export function errorResult(message: string) {
  return {
    isError: true as const,
    content: [{ type: "text" as const, text: message }],
  };
}
