// Small shared helpers for the curated tools.
import { GameClient } from "../game.js";

/** Pull the first present key from an object (tolerates API field-name variation). */
export function pick<T = any>(obj: any, ...keys: string[]): T | undefined {
  if (!obj || typeof obj !== "object") return undefined;
  for (const k of keys) if (obj[k] !== undefined && obj[k] !== null) return obj[k] as T;
  return undefined;
}

/** Many world.* methods wrap their list under a key; accept a few shapes. */
export function asList(resp: any, ...keys: string[]): any[] {
  if (Array.isArray(resp)) return resp;
  for (const k of keys) if (Array.isArray(resp?.[k])) return resp[k];
  // last resort: first array-valued property
  if (resp && typeof resp === "object") {
    for (const v of Object.values(resp)) if (Array.isArray(v)) return v as any[];
  }
  return [];
}

export interface Vec {
  x: number;
  y: number;
  z: number;
}

export function pos(obj: any): Vec | undefined {
  const p = pick<any>(obj, "position", "pos", "location");
  if (p && typeof p === "object" && typeof p.x === "number") return { x: p.x, y: p.y, z: p.z };
  if (typeof obj?.x === "number") return { x: obj.x, y: obj.y, z: obj.z };
  return undefined;
}

export async function playerPos(game: GameClient): Promise<Vec | undefined> {
  try {
    const p = await game.call("world.player", undefined, { timeoutMs: 8000 });
    const v = pos(p);
    // (0,0,0) means "in a vehicle" per the mod - treat as unknown.
    if (v && v.x === 0 && v.y === 0 && v.z === 0) return undefined;
    return v;
  } catch {
    return undefined;
  }
}

export function dist2d(a: Vec | undefined, b: Vec | undefined): number | undefined {
  if (!a || !b) return undefined;
  return Math.round(Math.hypot(a.x - b.x, a.y - b.y));
}

/** Resolve a "near" spec ("player" or {x,y}) plus radius into a filter center. */
export async function resolveCenter(
  game: GameClient,
  near: unknown,
): Promise<Vec | undefined> {
  if (near === "player" || near === undefined) return playerPos(game);
  if (near && typeof near === "object" && typeof (near as any).x === "number")
    return { x: (near as any).x, y: (near as any).y, z: (near as any).z ?? 0 };
  return undefined;
}
