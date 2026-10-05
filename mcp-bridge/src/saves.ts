// Verify a save actually hit disk. world.saveGame returns success even when NO
// file was written, so we confirm the .sav exists with a fresh timestamp.

import { promises as fs } from "node:fs";
import * as os from "node:os";
import * as path from "node:path";

export interface SaveVerification {
  confirmed: boolean;
  path?: string;
  ageSeconds?: number;
  note: string;
}

function saveGameRoots(): string[] {
  const roots: string[] = [];
  const local = process.env.LOCALAPPDATA;
  if (local) roots.push(path.join(local, "FactoryGame", "Saved", "SaveGames"));
  // Fallback for non-standard setups / WSL-mounted Windows homes.
  roots.push(path.join(os.homedir(), "AppData", "Local", "FactoryGame", "Saved", "SaveGames"));
  return [...new Set(roots)];
}

/**
 * Look for `<saveName>.sav` under any SaveGames/<steamid>/ folder, modified in
 * the last `withinSeconds`. Returns confirmed=false (not an error) if we can't
 * find it or can't read the folder (e.g. WSL without the Windows mount).
 */
export async function verifySaveOnDisk(
  saveName: string,
  withinSeconds = 120,
): Promise<SaveVerification> {
  const wanted = `${saveName}.sav`.toLowerCase();
  let best: { path: string; mtimeMs: number } | undefined;
  let sawRoot = false;

  for (const root of saveGameRoots()) {
    let subdirs: string[];
    try {
      subdirs = await fs.readdir(root);
      sawRoot = true;
    } catch {
      continue;
    }
    // search root itself plus one level of subfolders (per-steam-id dirs)
    for (const dir of [root, ...subdirs.map((d) => path.join(root, d))]) {
      let entries: string[];
      try {
        entries = await fs.readdir(dir);
      } catch {
        continue;
      }
      for (const name of entries) {
        if (name.toLowerCase() !== wanted) continue;
        const full = path.join(dir, name);
        try {
          const st = await fs.stat(full);
          if (!best || st.mtimeMs > best.mtimeMs) best = { path: full, mtimeMs: st.mtimeMs };
        } catch {
          /* ignore */
        }
      }
    }
  }

  if (!best) {
    return {
      confirmed: false,
      note: sawRoot
        ? `The game reported success, but no file named "${wanted}" was found on disk. The save may not have been written - verify in-game.`
        : `The game reported success, but I couldn't read the Windows save folder to confirm the file (this can happen under WSL). Verify the save in-game.`,
    };
  }

  const ageSeconds = Math.round((Date.now() - best.mtimeMs) / 1000);
  const fresh = ageSeconds <= withinSeconds;
  return {
    confirmed: fresh,
    path: best.path,
    ageSeconds,
    note: fresh
      ? `Confirmed on disk: ${best.path} (written ${ageSeconds}s ago).`
      : `A file exists (${best.path}) but it's ${ageSeconds}s old - the new save may NOT have been written. Verify in-game.`,
  };
}
