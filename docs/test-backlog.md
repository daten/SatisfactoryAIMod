# Test backlog: everything currently untested

Rebuilt 2026-09-20 from a full audit of source doc-comments
(`AIModFunctionLibrary.h`, `AIModFunctionLibraryInternal.h`, the domain
`.cpp` files, `AIModHttpServerSubsystem.cpp`), `Config/DefaultEngine.ini`,
`RPC_REFERENCE.md`, `controller/`, and `docs/` — cross-checked against the
live-verification record (2026-08-31 backlog session, 2026-09-07 sweep of
~42 previously-untested RPCs, 2026-09-08 item-movement/creature work,
2026-09-18 train loop, 2026-09-19 hazard/manta/project-assembly work).

This is a checklist, not a reference — for what a call actually does, see
`RPC_REFERENCE.md`. Same rules as before: work top to bottom, check a box
and add a one-line result note; a failed test stays unchecked with the
failure noted; a fully-checked tier gets deleted (git history keeps it).

**Prior backlog (started 2026-08-31): fully checked off and deleted per its
own instructions** — outcomes are recorded in the session memory and in
`docs/placement-lessons.md` / `RPC_REFERENCE.md` git history. The two items
it left genuinely open are carried into Tier 5 below (truck station Liquid
form, cargo platform flow rates).

---

## Tier 1 — pending-redeploy items and quick, high-value checks

- [ ] **Post-refactor smoke test** — the 3-stage file split
  (`864eb66b7d`..`d39ed45f51`, include-hub extraction + shared helpers +
  domain `.cpp` split) has compiled but never run live. After the next
  Alpakit redeploy: `world.help` (expect full method catalog), then one
  cheap RPC per domain file (a read, a construction dry run, a
  connect dry run, an inventory op) to prove the split didn't drop or
  break any handler wiring.
- [ ] **`world.batch` delete fast-path** — the settle-once-per-delete-run
  fix is committed but PENDING REDEPLOY (never run live). Batch-delete a
  disposable row of foundations and confirm: one settle wait for the whole
  run (fast), refunds correct, no dangling-belt regressions.
- [ ] **`world.setVehicleEngineParams`** — added 2026-09-19; the doc
  comment's own caveat stands: FG's movement `.cpp` is stub source, so
  whether `SetMaxEngineTorque`/`SetDragCoefficient` actually move top
  speed is unproven. Test: read a truck's baseline top speed on a straight
  run, lower drag, re-run, confirm a real speed change. Tune gradually
  (Chaos instability warning in the header).
- [ ] **Hazard unknown #3: damage stops for a player already inside** —
  the ONE remaining item from `docs/world-boundary-hazards.md` (all four
  RPCs otherwise live-verified 2026-09-19). Recipe is written out in that
  doc's "Remaining live test" section: save, stand in the east warning
  band, `world.setDamageVolumeEnabled` off, confirm ticking stops while
  standing inside, re-enable, confirm it resumes.

## Tier 2 — M.A.M. / milestone / achievement writes (deliberately skipped in the 2026-09-07 sweep, at user request — get a fresh save checkpoint first)

All six are implemented with dry-run or verify-after-write discipline but
have NEVER been run live; the engine side (`FGSchematicManager.cpp`,
`FGResearchManager.cpp`) is stub source, so real contracts are unconfirmed.

- [ ] **`world.payMilestone`** — dry run first (reports would-be
  submission/shortfall, touches nothing), then a real partial payment on a
  cheap milestone. Open question flagged in the header: whether
  `PayOffOnSchematic` requires the target to already be the ACTIVE
  schematic. Also test the `bFromDepot` variant.
- [ ] **`world.startMamResearch`** — dry run, then a real cheap research
  (atomic full-cost payment; verify `IsResearchBeingConducted` flips).
- [ ] **`world.claimMamResearch`** — claim a completed research; for a
  hard-drive schematic confirm it generates an unclaimed drive instead of
  a direct unlock.
- [ ] **`world.claimMamHardDriveReward`** — pick one offered alternate
  recipe by schematic path; verify the lookup-by-reward-content design
  holds (no id collisions across unclaimed drives).
- [ ] **`world.rerollMamHardDrive`** — reroll once, re-query
  `world.mamStatus` for the new choices; check the
  no-rerolls-left vs no-alternates-available detail split.
- [ ] **`world.reprocessMilestone`** — the achievement re-fire mechanism.
  UNCONFIRMED that `GiveAccessToSchematics` re-invokes
  `CheckSchematicAchievement`. Test ONE tier first and watch for the Steam
  pop before sweeping all tiers (per the header's own instruction).

## Tier 3 — portable miner end-to-end flow

- [ ] **`world.placePortableMiner` → `world.retrievePortableMinerInventory`
  → `world.movePortableMinerToInventory`** — input validation passed
  2026-09-07 but the full flow is UNVERIFIED: it was blocked on (a) no
  portable-miner item in inventory — now unblocked by
  `world.addItemsToPlayerInventory` (live-verified 2026-09-08; verify the
  item class, likely `Desc_PortableMiner`), and (b) needing a real
  infinite ore node (`BP_ResourceNode`), not a depletable
  `BP_ResourceDeposit` — known-good nodes exist at the remote-factory
  sites (see `reference_remote_ore_factories` memory / world.resourceNodes).

## Tier 4 — packaging / release verification (ficsit.app prep blockers)

- [ ] **Loopback-only socket bind in the PACKAGED build** — the plugin-level
  `Config/DefaultEngine.ini` listener override (+ commit `f559607dd6`) is
  explicitly "NOT YET VERIFIED against a real Steam session". After the
  next packaged deploy: `netstat -ano | findstr 51902` must show
  `127.0.0.1:51902` (or `[::1]`), NOT `0.0.0.0`. This is release blocker #1.
- [ ] **Multiplayer: untested by design** — not a test to run, but the
  README/AGENTS "largely untested" disclosure must survive into the
  ficsit.app listing (several write RPCs assume the local player /
  listen-server context).

## Tier 5 — known partials and edge cases (carried findings + header caveats)

- [ ] `world.truckStations` — the `resourceForm: "Liquid"` case (needs a
  Fluid Truck Station; none reachable when first tested 2026-08-31).
- [ ] `world.trainCargoPlatforms` — `inflowRate`/`outflowRate` on an
  ACTIVELY-loading platform (read itself verified; rates never observed
  non-idle). The freight loop save (`train-loop-freight`) should provide one.
- [ ] `world.setBeamLength` — save/reload persistence of the changed length.
- [ ] Priority power switches under a REAL shortage — actual load-shedding
  behavior on an overloaded circuit (config round-trips all verified).
- [ ] `freeEndRotationSteps` on a genuinely FREE lift end — its independent
  effect was never isolated (verified lift landed on a docking connector,
  which forces orientation). Related unverified assumption in the same
  code path: `SetScrollRotateValue(0)` being the true unrotated baseline
  (`AIModFunctionLibrary_Connections.cpp`, scroll-reset comment).
- [ ] `world.setBuildableRotation` on a lift with one CONNECTED end — the
  header's SAFETY caveat: logical connection should survive, visual
  alignment at the connected end may not. (Plain machines verified.)
- [ ] `routeMode: "Curve"` on a real belt bend failure — forcing the build
  mode is a well-evidenced hypothesis, "not yet verified at runtime that
  forcing Curve actually resolves the bend failures"
  (`AIModFunctionLibrary_Connections.cpp`).
- [ ] Belt player-distance fix sufficiency — writing the synthetic hit into
  `BuildGun->GetHitResult()` every poll tick is "not yet verified to
  remove the distance dependence alone - keep teleporting near belt
  connections" (`AIModFunctionLibraryInternal.h`). Test: one long INCLINED
  belt with the player deliberately far away.
- [ ] `world.connectPower` joint connector-type selection — the
  two-pass exact-type-match/PCT_Any-fallback logic is source-grounded but
  unverified at runtime for the mixed cases: Tower→ordinary pole
  (short range, the case flagged "not yet tried" in 2026-08-31 notes) and
  a machine against a tower's short-range side.
- [ ] `world.spawnCreature` `scale` param — spawn itself verified
  2026-09-08; whether non-1.0 scale actually applies on FactoryGame
  creature BPs (collision/AI ranges often hardcoded) never checked.
- [ ] `world.constructStackableSupport` with `stackCount >= 2` (single Zoop
  call) — repeated-call stacking verified instead; the Zoop path never was.
- [ ] `instigatorStrategy: "LocalPlayer"` for `world.connectConveyor` —
  experimental second-`ULocalPlayer` hypothesis for the UFGCDInitializing
  gate, never run (`AIModFunctionLibrary.h`, InstigatorStrategy comment).
  Only matters if the decoy-instigator problem resurfaces.
- [ ] `controller/satisfactory_ai/connector_db.py` — wall-mount and
  ceiling-mount conveyor-pole profiles are UNVERIFIED (class
  path/geometry); smelter + miner seed profiles are UNVERIFIED-yaw
  (observed at one yaw only) — `learn_and_store()` them at a second yaw.
- [ ] `controller/satisfactory_ai/pipes.py` — steep vertical pipe run
  (flagged unverified pending a live `world.testPipe` dry run at real
  vertical geometry; ordinary pump→tank runs are verified).
- [ ] `world.projectAssembly` before the Space Elevator exists — whether
  the station actor spawns pre-elevator (`found=false` path). Trivial;
  only testable on a fresh save.

---

## Documentation debt found by this audit (not tests — cleanup)

A large number of "NOT YET LIVE-TESTED" / "not yet verified at runtime"
markers are STALE — the features were verified in the 2026-08-31 backlog
session, the 2026-09-07 sweep, or the 2026-09-18/19 train/hazard/manta
sessions, but the markers were never updated:

- **`RPC_REFERENCE.md`** — ~25 stale markers (teleportPlayer, map markers,
  powerPoles, power switches, pipe tiers/fluid boxes, splitter sort rules,
  activeEvents, waterVolumes, beams, stackable supports, setBuildableColor,
  installPowerShard, truck autopilot, vehicle path segments, …).
- **Header doc-comments** (`AIModFunctionLibrary.h`) — stale on:
  removeItemsFromInventory, addItemsToPlayerInventory,
  uploadToCentralStorage (all verified 2026-09-08);
  constructRailroadTrack, setTrainTimetable, setTrainSelfDriving
  (2026-09-18); setTruckAutopilot, spawnManta, projectAssembly phase/height
  FLAGGED-UNKNOWNs, probeHazard's EncompassesPoint caveat (2026-09-19);
  multi-segment belt chaining and pole-relay power chaining (proven by the
  copper/HMF factory builds).
- **`AIModHttpServerSubsystem.cpp`** — stale "not yet verified" on the
  conveyor-lift, pipe, railroad, and vehicle-path dispatch comments.
- **`controller/`** — stale NOT-YET-LIVE-TESTED in `models.py`,
  `protocol.py`, `conveyors.py`, `pipes.py` module headers, `README.md`.
- **`docs/`** — stale markers in `buildable-coverage.md`,
  `placement-lessons.md` (trains/MAM/milestone entries say untested; the
  MAM/milestone WRITES genuinely are — see Tier 2 — but the reads are not),
  `pipe-network-research.md`, `conveyor-attachment-research.md`,
  `splitter-port-control-test.md`.

Header/`.cpp` comment cleanup requires a rebuild — batch it with the next
functional redeploy rather than redeploying for comments alone.
