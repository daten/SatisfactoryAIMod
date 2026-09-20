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

- [x] **NEW `world.setActiveMilestone` + `world.launchShip`** — **DONE
  2026-09-20, ALL PASS after redeploy.** NOT_PAID_OFF guard fired
  correctly on the unpaid active milestone (with remaining cost in
  detail); setActiveMilestone switched Base Building → Logistics with
  verified previous/new detail; launchShip on pre-paid Logistics
  launched the freighter (timeUntilShipReturn 240, shipAtTradingPost
  false) and `purchased` flipped TRUE — full RPC milestone flow
  (setActive → pay → launch → purchased) closed end to end. Nuance:
  purchased flipped BEFORE the 240s ship return elapsed — completion
  appears to land at/shortly after launch; the return is cosmetic.
  Bonus: the pre-paid deposit SURVIVED save/reload (mPaidOffSchematic
  is SaveGame).

- [x] **Post-refactor smoke test** — **DONE 2026-09-20, PASS** on the
  redeployed build: `world.help` returned the full 124-method catalog and
  the session exercised RPCs across every domain file (reads, placeBuilding,
  deleteBuilding via batch, teleport, inventory injection, milestone/MAM
  writes, the two brand-new RPCs) with no missing-handler or wiring
  failures.
- [x] **`world.batch` delete fast-path** — **DONE 2026-09-20, PASS**:
  batch-deleted 3 disposable foundations in 0.78 s wall total — the old
  path settled 0.75 s PER delete (would have been ≥2.25 s), confirming
  one settle per delete-run. All refund paths returned success.
  (No belts involved — the dangling-belt repair path wasn't re-exercised,
  but it was unchanged by the fix.)
- [ ] **`world.setVehicleEngineParams`** — 2026-09-20 (creative ON):
  RPC-level PASS — `dragCoefficient: 0.05` applied to the real Explorer
  (`appliedDrag: true`, torque correctly left unchanged when omitted).
  Remaining: the physical confirmation — drive the Explorer and feel/
  measure whether top speed actually rose (Chaos setters are stub-source;
  only a drive proves them).
- [x] **Hazard unknown #3: damage stops for a player already inside** —
  **DONE, user-confirmed live 2026-09-20** ("we've live tested ...
  hazard removal"). Session note: the doc's east-border recipe is not
  reachable by ground-gated teleport from the base plateau (groundHeight
  returns found=false beyond terrain streaming range — hop-teleport
  required for any future border work).

## Tier 2 — M.A.M. / milestone / achievement writes (deliberately skipped in the 2026-09-07 sweep, at user request — get a fresh save checkpoint first)

All six are implemented with dry-run or verify-after-write discipline but
have NEVER been run live; the engine side (`FGSchematicManager.cpp`,
`FGResearchManager.cpp`) is stub source, so real contracts are unconfirmed.

- [x] **`world.payMilestone`** — **DONE 2026-09-20, mechanics PASS with
  two real findings.** Dry run exact (wouldSubmit 50 Iron Plate,
  shortfall empty); real call submitted 50, `remainingCost` dropped to 0,
  no PAYOFF_REJECTED. ANSWERED the header's open question: works on a
  NON-active schematic (activeSchematic was null). Findings: (1)
  `purchased` never flipped true for the paid-off (ExampleMod) milestone
  — paying deposits cost but completion apparently needs the HUB flow /
  active-schematic path — REPLICATED on vanilla in a fresh save
  2026-09-20 (Logistics/Schematic_1-2: full 150/150/300 cost deposited,
  remaining 0/0/0, purchased stays false, clean deduction with no
  inventory anomaly). ROOT CAUSE FOUND: paying ACTIVE Field Research
  (user-selected in HUB) also left purchased=false — completion needs
  the terminal's LAUNCH step: FGSchematicManager.h's
  LaunchShip(instigator) ("player pressed the launch button"), with the
  purchase completing at freighter RETURN. Also verified: the
  default-to-active path (empty schematicClass resolved the active
  Field Research). Fix implemented same session: world.setActiveMilestone
  + world.launchShip RPCs (see Tier 1) — pending redeploy + live test.
  (2) Player inventory NET +50 plates
  (211→261): −50 payment plus a suspected +100 ExampleMod demo unlock
  grant — harmless here, but re-observe on a vanilla milestone.
  `bFromDepot` variant still untried.
- [x] **`world.startMamResearch`** — **DONE 2026-09-20, PASS** (fresh
  save, ExampleMod "Example Complex Tree"): dry run reported
  canResearchBeInitiated + canAfford correctly; real call paid the full
  100-Copper-Ingot cost atomically (100→0) and the research showed up
  in `ongoingResearch`, later completing on its own — the atomic
  no-partial-payment model behaved exactly as the header describes.
- [x] **`world.claimMamResearch`** — **DONE 2026-09-20, BOTH PATHS
  PASS.** Hard-drive path (main save): claimed the completed
  `Research_HardDrive_0_C`, completedResearch 1→0, generated exactly one
  unclaimed hard drive with 2 pending choices (not a direct unlock).
  Normal path (fresh save): claimed a completed ExampleMod research —
  node flipped to Purchased, no hard drive generated.
- [x] **`world.claimMamHardDriveReward`** — **DONE 2026-09-20, PASS.**
  Claimed Alternate: Heavy Flexible Frame by schematic path; drive
  consumed (unclaimed 1→0) and `world.recipeCatalog` confirms
  `Recipe_Alternate_HeavyFlexibleFrame_C` isAvailable=true. (Single-drive
  case only — the multi-drive collision question stays theoretical.)
- [x] **`world.rerollMamHardDrive`** — **DONE 2026-09-20, PASS.** Reroll
  replaced both choices (Caterium Wire/Turbo Motor → Heavy Flexible
  Frame/Aluminum Rod) and `hasReroll` flipped false. Param note: the RPC
  takes `schematicClass` (any currently-offered reward), matching
  RPC_REFERENCE, not the C++ arg name.
- [ ] **`world.reprocessMilestone`** — 2026-09-20 (creative ON):
  mechanics PASS — tier 1 reprocessed 3 real schematics
  (Schematic_1-1/1-2/1-3) and every purchase stayed intact afterward
  (the "will not revoke unlocks" contract holds). STILL OPEN: whether a
  Steam achievement actually re-fired (user-observable only — awaiting
  confirmation; if no pop on tier 1, try a tier with a known milestone
  achievement before concluding the mechanism doesn't reach Steam).

## Tier 3 — portable miner end-to-end flow

- [x] **`world.placePortableMiner` → `world.retrievePortableMinerInventory`
  → `world.movePortableMinerToInventory`** — **DONE 2026-09-20 (creative
  ON), flow PASS.** Seeded via `world.addItemsToPlayerInventory` with
  descriptor class `BP_ItemDescriptorPortableMiner_C` (confirmed — the
  creative-ON positive path for that RPC too). Placed on real iron node
  `BP_ResourceNode577`: miner spawned, `isProducing: true`. Retrieve
  emptied the output into the player with an exact +12 Iron Ore delta.
  `movePortableMinerToInventory`: the header confirms it is the
  ARMS-equipment-slot→backpack transfer; its documented success-no-op
  (nothing equipped) verified. Its POSITIVE path needs a miner equipped
  in ARMS — trivial manual step, left open only as a footnote. Doc debt:
  the `world.help` summary ("Pick a portable miner back up") misdescribes
  it. Cleanup note: the placed miner is NOT reachable by
  `world.deleteBuilding` (TARGET_NOT_FOUND — not a buildable/vehicle);
  it was left mining on the node for manual pickup (which also feeds the
  ARMS positive-path test).

## Tier 4 — packaging / release verification (ficsit.app prep blockers)

- [x] **Loopback-only socket bind in the PACKAGED build** — **DONE,
  verified 2026-09-20** (release-prep session): netstat on the packaged
  Steam build shows `127.0.0.1:51902` (was `0.0.0.0`) via the
  Initialize()-time GConfig injection (commit `f559607dd6`). The
  plugin-ini comment's "NOT YET VERIFIED" is now stale. Optional
  leftover: a LAN socket-refused check from another machine.
- [ ] **Multiplayer: untested by design** — not a test to run, but the
  README/AGENTS "largely untested" disclosure must survive into the
  ficsit.app listing (several write RPCs assume the local player /
  listen-server context).

## Tier 5 — known partials and edge cases (carried findings + header caveats)

- [ ] `world.truckStations` — the `resourceForm: "Liquid"` case (needs a
  Fluid Truck Station; none reachable when first tested 2026-08-31).
- [x] `world.trainCargoPlatforms` — **DONE 2026-09-20, PASS.** A real
  non-zero rate observed live: fluid platform
  `Build_TrainDockingStationLiquid_C_2144894785` reported
  `outflowRate: 9.84` (unload mode) alongside working
  `freightCargoType: "Liquid"`/`isInLoadMode` fields. A self-driving
  freight dock was also produced on demand (train re-enabled →
  `dockingState: Docked`), but its standard platforms read 0 — nothing
  to transfer since the 09-18 teardown, not a telemetry gap. Train
  restored to parked afterward.
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
- [x] `world.spawnCreature` `scale` param — **DONE 2026-09-20,
  user-confirmed.** Spawned a scale-3.0 Space Rabbit near the player
  (alive, animated, wandering per telemetry); user visually confirmed
  the creature-spawn tests. Note: creature telemetry carries no scale
  field — visual confirmation is the only check. Despawned after.
- [ ] `world.constructStackableSupport` with `stackCount >= 2` (single Zoop
  call) — **FAILED 2026-09-20, real finding**: returns
  `UNEXPECTED_STEP_COMPLETE` ("DoMultiStepPlacement() reported complete
  after only the start click") — the hologram finishes as a single
  placement instead of entering Zoop mode; nothing was built (buildable
  count unchanged). Repeated-call stacking remains the working approach;
  fix or drop the `stackCount` param.
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
- [ ] `controller/satisfactory_ai/belt_route.py` — pole belt-connector
  offset is a "live-seed TODO" (poles placed at the span endpoint with no
  learned connector offset); seed it via `world.connectorLayout` /
  `connector_db.learn_and_store()` on a real placed pole.
- [x] `controller/satisfactory_ai/conveyors.py` — **DONE 2026-09-20,
  CONFIRMED: items/min = GetSpeed() / 2.** All six tiers returned
  exactly 2x the known real rates (120/240/540/960/1560/2400 vs
  60/120/270/480/780/1200) — a coincidence across the whole ladder is
  implausible. Update conveyors.py to expose the conversion (a timed
  physical throughput count remains optional belt-and-suspenders).
- [ ] `controller/satisfactory_ai/recipe_tree.py` — somersloop
  amplification assumes full 2x when a machine's sloop slot count is
  unknown ("slots unknown -> assume full 2x"); read the real per-machine
  production-amplifier slot counts live and replace the assumption.
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
