// Teleport safety pre-check. A bad teleport (into a hazard volume, below the kill
// plane, out of bounds, or onto thin terrain over water) can kill the pioneer,
// and there is no respawn RPC. So we probe the destination before teleporting.

import { GameClient } from "./game.js";

export interface TeleportSafety {
  safe: boolean;
  reasons: string[];
  groundZ?: number;
}

// Heuristics from the project's terrain notes: the ocean plane is ~ -24400 and
// dry land reads groundHeight above roughly sea level; very low ground is
// usually thin-over-water that the pawn falls through.
const OCEAN_Z = -20000;

export async function checkTeleportSafe(
  game: GameClient,
  x: number,
  y: number,
  z?: number,
): Promise<TeleportSafety> {
  const reasons: string[] = [];
  let groundZ: number | undefined;

  // Hazard / bounds / kill-Z probe (use given z, else a high sample point).
  try {
    const probe = await game.call("world.probeHazard", {
      x,
      y,
      z: z ?? 10000,
    });
    if (probe?.insideDamageVolume) reasons.push("inside a damage (gas/kill) volume");
    if (probe?.belowKillZ) reasons.push("below the kill plane");
    if (probe?.insideWorldBounds2D === false) reasons.push("outside the playable world bounds");
    if (typeof probe?.nearestOtherVolumeDistance === "number" && probe.nearestOtherVolumeDistance < 5000)
      reasons.push("very close to a hazard volume");
  } catch {
    reasons.push("couldn't probe hazards at the destination");
  }

  // Ground height sanity (skip if caller insists on an explicit z they trust).
  try {
    const g = await game.call("world.groundHeight", { x, y });
    groundZ = typeof g?.z === "number" ? g.z : undefined;
    if (groundZ !== undefined && groundZ <= OCEAN_Z)
      reasons.push("the ground there reads as ocean/void (the pioneer would fall through)");
  } catch {
    // non-fatal
  }

  return { safe: reasons.length === 0, reasons, groundZ };
}
