// Read vs write classification for every world.* method.
//
// IMPORTANT: the mod catalog's "category" field is NOT a safety signal
// (e.g. world.buildables/world.connections are read-only but tagged "command";
// world.cleanupOrphanedFlowIndicators removes things but is tagged "telemetry").
// So the bridge keeps its OWN reviewed allowlist here. Anything NOT in READ_ONLY
// is treated as a world/player-affecting action that needs approval.
//
// The build-time check (scripts/check_classified.ts) fails if any catalog method
// is missing from exactly one of READ_ONLY / known actions.

import { CATALOG } from "./catalog.js";

/**
 * Methods that only READ state and have no side effect on the world OR the
 * player (no build gun, no camera move, no teleport). Safe to auto-approve.
 *
 * Deliberately EXCLUDED even though they don't change the world:
 *  - test* dry-runs: they drive the real build gun and can repoint the camera /
 *    teleport the player for belts, so they are treated as actions.
 *  - simulatedCraft: harmless, but it's an equipment-craft simulation; kept out
 *    of the auto-approve read path out of caution (available via perform_game_action).
 */
export const READ_ONLY: ReadonlySet<string> = new Set([
  "world.help",
  "world.version",
  "world.activeEvents",
  "world.buildableCatalog",
  "world.buildables",
  "world.centralStorage",
  "world.chatHistory",
  "world.connections",
  "world.connectorLayout",
  "world.constructionCost",
  "world.conveyorAttachments",
  "world.conveyorBeltTiers",
  "world.conveyorLiftTiers",
  "world.creatures",
  "world.damageVolumes",
  "world.droneStations",
  "world.groundHeight",
  "world.itemCatalog",
  "world.mamStatus",
  "world.mantas",
  "world.manufacturers",
  "world.mapMarkerIcons",
  "world.mapMarkers",
  "world.milestoneProgress",
  "world.pipeConnections",
  "world.pipeFluidBoxes",
  "world.pipeReservoirTiers",
  "world.pipelinePumpTiers",
  "world.pipelineTiers",
  "world.player",
  "world.playerInventory",
  "world.portableMiners",
  "world.powerLineLimits",
  "world.powerPoles",
  "world.priorityPowerSwitches",
  "world.probeHazard",
  "world.projectAssembly",
  "world.recipeCatalog",
  "world.resourceNodes",
  "world.splineGeometry",
  "world.splitterSortRules",
  "world.targetedManufacturer",
  "world.terrainHeightGrid",
  "world.timeOfDay",
  "world.trainCargoPlatforms",
  "world.trainStations",
  "world.trains",
  "world.truckStations",
  "world.vehiclePathNodes",
  "world.vehicles",
  "world.waterVolumes",
]);

/** Creative-gated methods (mod refuses them unless the player enabled the setting). */
export const CREATIVE: ReadonlySet<string> = new Set(
  CATALOG.methods.filter((m) => m.creative).map((m) => m.method),
);

/** Every method the mod knows about. */
export const ALL_METHODS: ReadonlySet<string> = new Set(
  CATALOG.methods.map((m) => m.method),
);

export function isReadOnly(method: string): boolean {
  return READ_ONLY.has(method);
}

export function isCreative(method: string): boolean {
  return CREATIVE.has(method);
}

export function isKnownMethod(method: string): boolean {
  return ALL_METHODS.has(method);
}

/** Methods that change the world or affect the player - everything not read-only. */
export function actionMethods(): string[] {
  return CATALOG.methods.map((m) => m.method).filter((m) => !READ_ONLY.has(m));
}
