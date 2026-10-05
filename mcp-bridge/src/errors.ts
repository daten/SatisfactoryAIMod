// Translate the mod's error codes (and transport failures) into plain-language
// guidance a player can act on. The raw code/message is always preserved too.

export class GameError extends Error {
  constructor(
    public readonly code: string,
    message: string,
    public readonly method?: string,
  ) {
    super(message);
    this.name = "GameError";
  }
}

/** Thrown when we cannot reach the game at all. */
export class ConnectionError extends GameError {
  constructor(message: string, method?: string) {
    super("CONNECTION", message, method);
    this.name = "ConnectionError";
  }
}

const HELP: Record<string, string> = {
  CONNECTION:
    "Can't reach the game. Is Satisfactory running with the SatisfactoryAIMod installed, and is a save actually loaded (not sitting on the main menu)?",
  NO_PLAYER:
    "Your pioneer isn't available right now. If you're driving a vehicle, step out; if you died, respawn first.",
  NO_BUILD_GUN:
    "The build gun isn't available - this usually means photo mode is on (I can exit it for you) or you're in a vehicle.",
  NO_BUILD_STATE:
    "The build gun isn't ready - usually photo mode is on or you're in a vehicle.",
  HOLOGRAM_SPAWN_FAILED:
    "The game couldn't start placing that - most often photo mode is on (I can exit it) or your pioneer is in a vehicle / not alive.",
  HOLOGRAM_INVALIDATED:
    "The placement was rejected mid-build. Try again, or move/rotate the target slightly.",
  CANNOT_CONSTRUCT:
    "The game refused this placement (blocked by terrain, clearance, or an existing building). Try moving, rotating, or clearing the spot.",
  PLACEMENT_INCOMPLETE:
    "The build didn't finish cleanly. I'll re-check the world to see what actually happened.",
  CONSTRUCTION_UNCONFIRMED:
    "I couldn't confirm the build completed - I'll re-read the world to verify.",
  INSUFFICIENT_INGREDIENTS:
    "Not enough materials for this. I can tell you exactly what's missing.",
  CREATIVE_DISABLED:
    "That's a creative/cheat feature. Only you can turn it on, via 'Allow Creative Features' in the mod's settings menu - an agent can't enable it.",
  BUILD_DISTANCE_EXCEEDED:
    "Your 'Limit Build Distance' mod setting is blocking a build that far from your pioneer. Move closer or raise/disable that setting.",
  TARGET_NOT_FOUND:
    "That object no longer exists (it may have been removed). I'll refresh and look again.",
  INVALID_TARGET: "That target isn't valid for this action.",
  NODE_OCCUPIED: "That resource node already has a miner on it.",
  NODE_NOT_FOUND: "No resource node was found there.",
  NO_TARGET_NODE: "No resource node was found there.",
  INVALID_RECIPE: "That recipe isn't valid for this machine.",
  INVALID_ITEM_CLASS: "That item type isn't recognized - look it up in the catalog first.",
  INVENTORY_FULL: "There's no room for that (inventory or storage is full).",
  NO_CENTRAL_STORAGE: "The Dimensional Depot isn't built/available yet.",
  NO_RAILROAD_CONNECTION:
    "Those rail pieces don't share a usable connector - connect to a station's track or an open rail end.",
  NO_POWER_CONNECTION: "One of those buildings has no free power connection.",
  NO_FACTORY_CONNECTION: "No matching belt/pipe connector was found to attach to.",
  NO_PIPE_CONNECTION: "No matching pipe connector was found to attach to.",
  WRONG_TYPE: "That action doesn't apply to this kind of object.",
  OPERATION_NOT_PERMITTED: "The game didn't allow that operation here.",
  INVALID_REQUEST: "", // mod's own message is the useful part; shown verbatim.
  PENDING:
    "The build was scheduled - I'll re-read the world to confirm whether it actually happened.",
  INTERNAL_ERROR:
    "The mod hit an internal error. Check the in-game log (LogAIModAI) for details.",
  NO_WORLD: "No game world is loaded yet (you're probably on the main menu).",
};

/** Build a player-facing sentence for an error. */
export function explain(err: GameError): string {
  const help = HELP[err.code];
  const raw = err.message ? ` (game said: ${err.message})` : "";
  if (help === "") return `${err.message || err.code}`;
  if (help) return `${help}${err.code === "CONNECTION" ? "" : raw}`;
  return `${err.code}${raw}`;
}
