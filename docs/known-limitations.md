# Known limitations & shippability bugs

Authoritative tracker for anything that currently needs a **game restart, a
manual save/reload, or an out-of-band manual step** to recover. These are
shippability blockers: the mod's RPC surface must be self-recovering so an
external controller (or another user) never has to touch the game process by
hand. Fix them in C++ (or the controller) — never document a restart as a
"step."

Severity:
- **P0** — blocks normal operation; recovery needs a restart/reload today.
- **P1** — recoverable without a restart via a workaround, but the workaround
  is fragile / non-obvious and should be made unnecessary.
- **P2** — quality-of-life; no restart needed, but a manual step remains.

| id | title | severity | needs restart/reload today? | status |
|----|-------|----------|-----------------------------|--------|
| KL-1 | `world.connectConveyor` default strategy (`PlayerController`) is broken — permanent `Initializing` | P0 (shippability) | no restart; but raw callers get permanently-failing belts | fix ready: change default to `RealCharacter` |
| KL-2 | `connectPower` global stuck state ("Must be hooked up") | P1 | no (auto-reset + `ignoreWireSnap`) | likely resolved — verify |
| KL-3 | Void/deep teleport kills the player (`NO_PLAYER`) | P1 | manual menu load to respawn | mitigated by avoidance; respawn RPC not built |
| KL-4 | ~~`world.connectConveyorLift` doesn't attach the DEST end~~ | — | — | **WITHDRAWN — usage error, not a bug** |

> **Disproven (2026-09-10): "deleting a belt wedges the source connector."**
> Feared to be an `AbstractInstanceManager` delete-corpse needing a reload. A
> clean control on the solid HMF platform (RealCharacter strategy, routeMode
> cycling) built a belt, deleted it, and **rebuilt it in the same spot with no
> failure** — so delete→rebuild is fine. The "can't rebuild" symptom was two
> unrelated things: (a) KL-1 below — raw calls used the broken default strategy
> and hung on `Initializing`; and (b) one specific belt (an iron outpost's
> merger→drone-station) failing `UFGCDConveyorInvalidShape` because the merger
> sits ~300u from the 2400²-clearance drone-station body (a tight-layout issue,
> not a bug). **No reload was needed.** Lesson still stands: verify flow before
> deleting a working belt.

---

## KL-1 — `world.connectConveyor` default strategy is broken

**Symptom.** A raw `world.connectConveyor` / `world.testConveyorBelt` call that
does **not** pass `instigatorStrategy` uses the RPC default `"PlayerController"`,
a decoy-instigator path that can **never clear `UFGCDInitializing`** — the poll
runs the full 120-tick safety cap and returns `CANNOT_CONSTRUCT: Initializing`
every time, at every location, forever. Our own controller is unaffected
because `Executor._belt` always passes `"RealCharacter"`; only external/raw
callers (i.e. anyone shipping against this RPC) hit the broken default. The
decoy `PlayerController`/`AIController`/`LocalPlayer` strategies are documented
in `ConstructConveyorBelt`'s own comments as "conclusively failed identically"
on `UFGCDInitializing`; only `"RealCharacter"` (the real build gun) works.

**Fix.** Change the RPC default in `AIModHttpServerSubsystem` for
`world.connectConveyor`/`world.testConveyorBelt` from `"PlayerController"` to
`"RealCharacter"` (the sole known-working strategy). Optionally drop the dead
decoy strategies once RealCharacter is the confirmed sole path. Needs a
rebuild+redeploy. Verify: a raw `connectConveyor` with no `instigatorStrategy`
builds a belt between two facing connectors on solid ground.

**Note on the iron merger→drone belt (not a mod bug).** With `RealCharacter`
the only blocker is `UFGCDConveyorInvalidShape (hard)` + a soft clearance
overlap, because the drone station's 2400² body is right against the merger.
Resolve by layout (more gap between merger and station / an intermediate hop),
not code.

---

## KL-2 — `connectPower` global stuck state ("Must be hooked up")

**Symptom (historical).** After some failed power connections, every subsequent
`connectPower` returned "Must be hooked up to a connection!"
(`UFGCDWireSnap`) until a dummy pole was placed+dismantled or the game was
restarted.

**Status — likely resolved, verify.** Two independent fixes now cover it:
(1) the C++ stale-wire-hologram detector resets the build-gun state, and
(2) `ignoreWireSnap` (now default-on in `Executor.connect_power`) ignores the
disqualifier outright and still builds a real wire (verified 2026-09-10).
**Action:** confirm across a session of many power connects that the global
wedge never recurs and the dummy-pole reset (`Executor._reset_wire_state`) is
dead code that can be removed.

---

## KL-3 — Void/deep teleport kills the player (`NO_PLAYER`)

**Symptom.** Teleporting the player to a void/underground/too-high Z kills the
character; all subsequent build ops fail `NO_PLAYER` until the user manually
loads a save from the menu to respawn.

**Status — mitigated by avoidance; real fix not built.** The controller
groundHeight-gates teleports and never targets void/deep Z. The durable fix is
a `world.respawnPlayer` (and/or `world.loadSave`) RPC so recovery needs no
manual menu step. Tracked in memory `project_rpc_respawn_load_support`.

## KL-4 — WITHDRAWN (2026-09-11): `connectConveyorLift` is fine; this was a usage error

I reported that `connectConveyorLift` never attaches its DEST end. **That was
wrong — the lift works; I called it incorrectly.** `connectConveyorLift`'s
top stub **inherits the BOTTOM (source) connector's facing** (a documented
quirk, live-found 2026-09-02, see the `elevated_crossing` composite in
`controller/satisfactory_ai/composites.py`). So the destination relay's INPUT
must be oriented to face **the same direction as the source output's facing**.
My hand-rolled coal lift placed the top splitter with its input facing the wrong
way (−X while the source output faced +Y), so the inherited-facing top stub never
docked → "dest connected=False". My "short-lift test" repeated the same mistake
(both endpoints at default yaw0, dest input not oriented to the source facing),
so it "confirmed" a non-existent bug. I also used Mk1 instead of the proven Mk4.

**Correct usage (the proven pattern — use it, don't hand-roll):**
`elevated_crossing(db, source, dest, lane_z)` and the `"lift"` RouteOp. Manually:
place the top relay at (source_output.x, source_output.y, top_z), yaw =
`db.yaw_for_connector_facing(SPLITTER, "Input", source_output_facing)`, pin the
lift `sourceConnectorPosition`=source output and `destConnectorPosition`=that
relay's Input pin, recipe = Mk4. Then the traverse belt leaves from the RELAY
(not the lift top). Lesson: check the composite/history before declaring a lift
"bug" (feedback_check_history_before_diagnosing).

**Steel factory status (save `steel-wip2`).** ~90% built at the compact Pure-
iron + coal site (user extended power to it): iron miner → foundry (steel recipe)
✓, coal miner → S1 ✓, S3 → foundry coal input ✓, foundry → output container ✓,
foundry + coal miner + iron miner powered. The ONE missing link is the coal
S1→S2 lift up 34m — buildable now via the correct pattern above. Once coal
reaches S2, the built chain S2→S3→foundry carries it and steel flows.
