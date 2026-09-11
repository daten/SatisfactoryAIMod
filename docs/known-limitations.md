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
| KL-4 | `world.connectConveyorLift` doesn't attach the DEST end across a large vertical span | P1 | no restart; blocks tall lifts | investigating |

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

## KL-4 — `connectConveyorLift` dest end doesn't attach over a large vertical span

**Symptom.** Building a coal lift up a 34m (~3436u) gap for the steel factory
(2026-09-11): `world.connectConveyorLift` from a coal-deck splitter (S1, z-1525)
to a foundry-level splitter (S2, z1911) reports **OK** and attaches the SOURCE
(S1 output gets the lift), but S2's input stays `connected=False` every time -
even with the source output and dest input pinned at the **same X,Y** (a pure
vertical run) and the dest connector position pinned. Repeated attempts each
leave a dangling lift on S1 (source attached, top floating). Not a connector-
alignment problem (verified co-located X,Y); the dest simply never binds.

**Likely cause (unconfirmed).** Either a single conveyor lift has a max height
below 3436u (so it builds a partial lift that never reaches the dest), or
`ConstructConveyorLift` has a dangling-dest bug analogous to the belt one
(source click completes, dest click/registration doesn't) with no
execute_and_verify-style repair for lifts. The executor's `_lift` does NOT
verify the dest attached.

**Categorized 2026-09-11 (short-lift test).** A SHORT 800u lift between two
fresh **splitters** AND between two fresh **storage containers** BOTH left the
dest `connected=False`. So it is **NOT a height cap and not splitter-specific** —
`ConstructConveyorLift`'s dest end does not bind at all in these tests (pinned
or unpinned). This is a C++ dest-attach bug (the source click completes, the
dest click/registration doesn't), the lift analogue of the belt two-click path.
Stacking lifts will NOT help (each dest still dangles). NOTE: memory says lifts
worked in the 2026-09-01 copper build ("2 arbitrary-height lifts") — so either
that used a different path/params or something regressed; check git history of
`ConstructConveyorLift` / the copper build scripts before rewriting.
**Fix path:** make `ConstructConveyorLift` drive the dest attachment like
`ConstructConveyorBelt`'s RealCharacter two-click flow (needs a rebuild), OR
finish the steel coal-vertical with a short DRONE hop (drones ignore vgap).

**Steel factory status (save `steel-wip2`).** ~90% built at the compact Pure-
iron + coal site (user extended power to it): iron miner → foundry (steel recipe)
✓, coal miner → S1 ✓, S3 → foundry coal input ✓, foundry → output container ✓,
foundry + coal miner + iron miner powered. The ONE missing link is S1→S2 (the
34m coal lift, KL-4). Once coal reaches S2, the chain S2→S3→foundry is already
built and steel will flow.
