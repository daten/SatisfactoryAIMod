# Explosive Rebar factory - build plan (2026-09-02)

From-scratch (raw ore -> Explosive Rebar), 100% final rate, machines on
platforms, compact multi-floor core, output into a Dimensional Depot.
User chose "faithful, at oil field" (raws are map-spread; oil isolated).
Units are cm (1 unit = 1 cm); "far" numbers are hundreds of meters.

## Recipe chain (live-verified from world.recipeCatalog)
Explosive Rebar (Manufacturer, 12s): 2 Iron Rebar + 2 Smokeless Powder +
2 Steel Pipe -> 1. **100% = 5/min.**

At 5/min, one machine each, UNDERCLOCKED to exact demand:
| Product | Machine | Recipe | demand/min | 100%/min | clock |
|---|---|---|---|---|---|
| Explosive Rebar | Manufacturer | 2 IronRebar+2 Smokeless+2 SteelPipe | 5 | 5 | 100% |
| Iron Rebar | Constructor | 1 Iron Rod -> 1 | 10 | 15 | 66.667% |
| Iron Rod | Constructor | 1 Iron Ingot -> 1 | 10 | 15 | 66.667% |
| Iron Ingot | Smelter | 1 Iron Ore -> 1 | 10 | 30 | 33.333% |
| Steel Pipe | Constructor | 3 Steel Ingot -> 2 | 10 | 20 | 50% |
| Steel Ingot | Foundry | 3 Iron Ore + 3 Coal -> 3 | 15 | 45 | 33.333% |
| Black Powder | Assembler | 1 Coal + 1 Sulfur -> 2 | 10 | 30 | 33.333% |
| Smokeless Powder | Refinery | 2 Black Powder + 1000mL HOR -> 2 | 10 | 20 | 50% |
| Heavy Oil Residue | Refinery (ALT, avail) | 3000mL Crude -> 4000mL HOR + 2 PolyResin | 5000mL | 40000mL | 12.5% |

Byproduct: Polymer Resin ~2.5/min (alt HOR) -> store in a container/Depot.

## Raw inputs /min (at 100%)
Iron Ore 25, Coal 20, Sulfur 5, Crude Oil 3.75 m3 (3750 mL/min).

## Nodes (all FREE, all Pure for the far solids)
- Oil Pure   (184783, 191090, -5732)  <- FACTORY SITE (extractor local)
- Coal Pure  (206003, 181237, 10347)  ~234m
- Iron Pure  (164518, 238318, -8589)  ~514m
- Sulfur Pure(242310, 152779,  8888)  ~691m

## Layout (compact multi-floor tower near the oil node)
Solids move up via lifts; fluids via vertical pipes. Floors (z steps):
- F1 smelting: Smelter (Iron Ingot), Foundry (Steel Ingot)
- F2 parts: Constructor Iron Rod -> Constructor Iron Rebar; Constructor
  Steel Pipe; Assembler Black Powder
- F3 fluids: Refinery HOR (crude in from extractor, HOR out + PolyResin
  byproduct), Refinery Smokeless (Black Powder + HOR in)
- F4 final: Manufacturer Explosive Rebar -> Dimensional Depot

## Build order
1. Factory platform + core machines at the oil site (multi-floor).
2. Oil extractor on node + crude pipe to HOR refinery (local).
3. Power backbone (pole spine) for the factory.
4. Internal belts + lifts + pipes; set recipes + clocks.
5. Remote miners (Mk3, underclocked) on coal/iron/sulfur + power + long
   belts back to the factory (batch + pole relays).
6. Manufacturer -> Depot; byproduct -> container. Verify 100% + save.

## Site + platform (BUILT)
Factory at oil node (184783,191090,-5732). F1 platform = 8x1 foundations
z=-5250 (top ~-5200), covering ~x[183400-186600] y[191900-195900].
GOTCHA: 8x1 lightweight foundations FAIL "identical buildable already
there" in this session (global instance saturation) even on empty spots
- 8x2/8x4 and machines place fine. Use 8x2 for upper floors.
Class naming: Recipe_SmelterBasicMk1 -> Build_SmelterMk1_C = the real
SMELTER; Recipe_SmelterMk1 -> Build_FoundryMk1_C = the Foundry.

## F1 machines (BUILT, recipes+clocks set) at z=-5200, yaw180
(yaw180 => inputs face +y/north, outputs face -y/south)
- smelter (Iron Ingot @33.334%)  Build_SmelterMk1_C_2147228457 (183800,192200)
- foundry (Steel Ingot @33.334%) Build_FoundryMk1_C_2147228180 (185200,192200)
- rod    (Iron Rod @66.667%)     Build_ConstructorMk1_C_2147227805 (183800,193600)
- rebar  (Iron Rebar @66.667%)   Build_ConstructorMk1_C_2147227410 (184700,193600)
- pipe   (Steel Pipe @50%)       Build_ConstructorMk1_C_2147226999 (185600,193600)
- blackpowder (Black Powder @33.334%) Build_AssemblerMk1_C_2147226588 (184600,194900)
Saved: erb-f1.

## PROGRESS UPDATE 3 (saved: erb-power-complete) 2026-09-02
POWER SOLVED + miners placed. The fuel-generator plan hit a BOOTSTRAP
DEADLOCK (gen needs fuel, fuel needs extractor+refinery power, those need
the gen) - unsolvable via RPC (no item-insert to hand-fuel). FIX: bridged
the factory grid to the existing powered Power-Tower network. Placed a
Build_PowerTower_C_2146840725 @(186400,196000,-4400); genpole->mytower
(short), mytower->Build_PowerTowerPlatform_C_2146839917 @(187716,212389,
5975) 22676u tower wire. Whole grid now hasPower=True; all 7 core machines
Standby (powered, awaiting raws). Fuel gen/refinery remain built as bonus
supply (harmless).
KEY FIX: connectPower "Must be hooked up to a connection!" (UFGCDWireSnap)
is a FLAKY disqualifier - pass ignoreWireSnap:True (RPC param) to bypass it
(real construct still validates). This unblocked every stuck power link.
Poles hold 4 wires; "NO_POWER_CONNECTION: No compatible free" = target
full, add a fresh pole. P4 got full and islanded P5/P6 - repowered via
genpole->P5.
Grid: mytower(main-grid bridge) -> genpole -> P2(manuf), P5(F1). P2->P3->P4
(Rod/Rebar/Pipe), P3 hub full. P5->Smelter,Foundry; P5->P6->BlackPowder.
Extra poles: P7=2146848415 (smokeless), genpole=2146855858, P2=2146892031,
P3=2146891685, P4=2146891334, P5=2146890986, P6=2146860739.
CLOCKS SET (were all 100): Smelter/Foundry/BlackPowder 33.334, Rod/Rebar
66.667, Pipe/Smokeless 50, HOR 12.5, Manuf 100.
MINERS PLACED (Mk3, clocked to exact demand, occupied nodes):
- COAL  Build_MinerMk3_C_2146835107 @(206003,181237,10347) 4.1667% =20/min
- IRON  Build_MinerMk3_C_2146836923 @(164518,238318,-8589) 5.2083% =25/min
- SULFUR Build_MinerMk3_C_2146836266 @(242310,152779,8888) 1.05%   =5/min
REMAINING: (a) power each miner - bridge to nearby powered towers (sulfur
~3300u to tower 239029,152247; coal ~12800u to 204542,193860; iron ~8200u
to 161614,246061). (b) LONG BELT HAULS to F1 via conveyor-pole relays
(Recipe_ConveyorPole, segments <=5600u): iron->manifold[Smelter in +
Foundry in], coal->manifold[Foundry in + BlackPowder in], sulfur->
BlackPowder in. Manifolds self-balance (supply=demand). (c) verify 100%
into depot + save.

## PROGRESS UPDATE 4 (saved: erb-coal-flowing) 2026-09-02
REMOTE BELT HAULS - major progress. Reusable haul pattern PROVEN: place
Recipe_ConveyorPole poles every <=4600u along the straight miner->factory
line (place base at z-100 so SnapOnly connector sits at z), read each
pole's SnapOnly connector via world.connections, chain belts pole-to-pole
(they relay belt-to-belt through the single SnapOnly connector - items
flow, confirmed). Factory-end: place a Recipe_ConveyorAttachmentSplitter
WELL AWAY (>1500u from every machine input - the belt "shape rule" binds
only <1500u) and feed machine inputs; manifolds self-balance (supply=demand).
- COAL: DONE + FLOWING. miner->7 poles->splitter(2146815703@184600,188500)
  ->Foundry coal-in + BlackPowder coal-in. Foundry & BlackPowder show
  IN[Coal=100].
- SULFUR: DONE (built). miner->20 poles->arrival(2146718829@185400,188000)
  ->BlackPowder sulfur-in(185400,193400). 3 mid-haul gaps were repaired by
  inserting a midpoint pole ~150u ABOVE the straight line (clears terrain
  bumps). Sulfur still traveling the 68000u belt (~6min) at last check.
- IRON: haul 11/12 built + miner + power + splitter(2146798816@183800,187500)
  ->Smelter in + Foundry iron-in all DONE. BLOCKED at the final hop into
  the factory. ROOT CAUSE: the FACTORY SITS UNDER A MOUNTAIN OVERHANG
  (groundHeight at 182200,193600 = -1207 surface ABOVE a -5362 cave floor).
  Coal/sulfur enter via the factory's OPEN SE mouth; iron approaches from
  the NW open canyon straight INTO the mountain/cave, where belts fail
  (PLACEMENT_INCOMPLETE / "too long"/"too steep" as the spline fights
  cave walls) and near-factory poles land buried (can't source belts).
  Last OPEN iron pole = pole10 Build_ConveyorPole_C_2146807691
  @(180600,197800,-5655) (t=0.83, groundHeight None = open canyon). FIX
  NEEDED: reroute from pole10 AROUND the mountain to approach the arrival
  from the open S/SE (like coal/sulfur), OR relocate the iron arrival to
  the SE mouth. Iron ore IS mining + riding 11 belts, stuck at pole11.
KEY LESSONS: (1) belts tolerate steep slopes (~34% ok) and long spans
(<5600u) but NOT routing through terrain/cave interiors. (2) place poles
ABOVE local ground (groundHeight); buried poles can't source belts.
(3) approach the factory only from its open SE mouth. (4) pure-cardinal
short belts hit the shape rule; keep segments >1500u.

## >>> PAUSE / RESUME HERE (saved: erb-pause-2026-09-02) <<<
Session paused 2026-09-02 for the day. Player parked SAFE on the factory
platform (184500,192926,-5101). Coal + sulfur DELIVERED and FLOWING
(BlackPowder was producing). Iron is ONE belt from done.
IRON PATH (NW Pure node, lift-skyway over the mountain):
  NW miner(2146836923,powered) -> ~10 canyon belts -> pole10
  (2146807691 @180600,197800,-5655) -> LIFT-UP 2146469257 (its OUTPUT
  dangles at 180600,197600,-855; conveyor lift max height ~4800u, so it
  tops out here, which still clears the route's ceiling ~-1184) -> [[BROKEN:
  I accidentally deleted sky1 with an over-broad cleanup box]] -> sky2
  2146526659(181700,192000,-560) -> sky3 2146526353(182600,189500,-640)
  -> skyend 2146526051(183700,187600,-720) -> belt -> liftpole 2146505800
  (183700,190200,-720) -> LIFT-DOWN 2146500636 -> botpole 2146501543
  (183700,190200,-5100) -> belt -> iron splitter 2146798816 input
  -> Smelter(2147206106) + Foundry(2147205041) iron inputs. Down half +
  splitter->machines all VERIFIED connected. Only the lift-out->sky1->sky2
  link is missing.
RESUME STEP 1: run scratch rebuild_sky1.py (or: place a conveyor pole sky1
  @~181000,195000,-850; belt LIFT 2146469257 output(180600,197600,-855)
  -> sky1; belt sky1 -> sky2 2146526659) using the player-near belt method
  (see [[reference_belt_haul_terrain_rules]]) - the lift-out->sky1 midpoint
  (~180800,196300) is over VOID so use the brief-hover-then-restore trick,
  monitor world.player alive.
RESUME STEP 2: wait for iron to traverse (~minutes), confirm Smelter and
  Foundry produce; then Rod/Rebar/Pipe, Manufacturer -> Depot at 100%.
RESUME STEP 3: RE-APPLY clocks AFTER machines are producing (setClockSpeed
  is pending; Standby reverts to 100) and verify. Fuel-gen/refinery are
  bonus (grid is powered by the tower bridge). Also: byproduct polymer
  resin (fuel refinery) currently unrouted - optional.
USER FEEDBACK this session: subterranean/void belts are risky+abnormal (use
  lift-skyways); don't teleport the player over void (killed them once ->
  manual save-reload); questioned the skyway height (it's lift-limited, as
  low as clears the cave ceiling).

## PROGRESS UPDATE 5 (saved: erb-iron-connected) 2026-09-02
ALL THREE RAWS DELIVERED. Coal+sulfur flowing (BlackPowder was PRODUCING
Black Powder). IRON solved via a LIFT-SKYWAY over the mountain (user's
suggested technique - subterranean/void belts are risky+abnormal, don't).
The factory sits under a mountain overhang open only to the SE; the NW iron
node's canyon belts reach pole10 (Build_ConveyorPole_C_2146807691 @
180600,197800,-5655, open air) but can't belt through the mountain. Route:
pole10 -> LIFT UP to a SPLITTER at (180600,197800,-500) above the crest ->
sky belt (poles at z~-500..-720, above the -947 crest) diagonally SE across
the mountain -> skyend (183700,187600,-720) -> belt N to liftpole
(183700,190200,-720) -> LIFT DOWN to botpole (183700,190200,-5100) -> belt
to the existing iron splitter (2146798816 @183800,187500) input -> Smelter
+ Foundry. Whole chain structurally verified (all connectors fed).

CRITICAL BELT/PLAYER LEARNINGS (this is why it was hard):
1. Floating conveyor poles at HIGH Z: belts between them FAIL ("too long"/
   "too steep"/"incomplete") UNLESS the player is teleported NEAR the belt
   (within the mod's 2000u no-re-teleport window) - the mod's inclined-belt
   validation is player-camera-dependent, not just the synthetic hit. Belts
   at low Z near their terrain worked because my old teleport put the player
   there. Parking the player far (factory) BREAKS high belts.
2. PLAYER SAFETY: teleport near a belt ONLY where world.groundHeight finds a
   floor below (a survivable fall onto it); over DEEP VOID (no floor) park at
   the factory instead (that belt fails; reroute/accept). Void teleports KILL
   the player (NO_PLAYER, needs manual save-reload). The SE iron nodes are
   void pinnacles - AVOID. hover = belt_z - 150 (just below the belt).
3. LIFT->belt transition: lift into a SPLITTER (real directional connectors),
   NOT a plain pole (its single connector gets vertically oriented by the
   lift -> a horizontal belt off it reads "too steep"). Conveyor LIFTS do
   arbitrary height (5000+ verified).
4. Flaky "too long/too steep" on a geometrically-fine belt: brute-force with
   varied player hover positions (dz, xy offset) - a slight offset fixes it.
5. setClockSpeed = SetPendingPotential, applies on the NEXT PRODUCTION CYCLE.
   Standby machines never apply it (revert to 100). Set/verify clocks AFTER
   the machine is producing. Clocks currently pending: Smelter/Foundry/BP
   33.334, Rod/Rebar 66.667, Pipe/Smokeless 50, HOR 12.5, Manuf 100.
IRON2 miner (SE Impure node attempt) was ABANDONED (void); using the NW Pure
node via the skyway. REMAINING: wait for iron to traverse the long path,
confirm Smelter->...->Manufacturer->Depot at 100%, re-verify clocks applied.

## PROGRESS UPDATE 2 (saved: erb-internal-complete)
INTERNAL MATERIAL FLOW COMPLETE. Manufacturer RELOCATED north (was
overlapping F1 columns): NEW manuf=Build_ManufacturerMk1_C_2147152776
@(184400,196200,-4400); NEW depot=Build_CentralStorage_C_2147152393
@(184400,198000,-4400). All working: F1 belts, 3 lifts (F1->F2 via
receiver splitters: rebar recv 2147176692, pipe recv 2147174365, bp recv
2147171831), rebar/pipe/smokeless->manuf, manuf->depot, bp->smokeless,
HOR polyresin->container, full fluid chain. UnlimitedResources IS ON
(belts free - earlier "missing materials" was truncated "(ignored)").
Platform extended north to y198300.
REMAINING: (1) POWER - no grid here; plan = raise oil extractor clock,
add Pipeline Junction on crude, add Fuel refinery (crude->fuel), add
Fuel Generator, pipe fuel, build power grid to all machines. (2) REMOTE
MINING x3: coal(206003,181237) iron(164518,238318) sulfur(242310,152779)
- Mk3 miners underclocked + power + long belts to F1 (iron->smelter in
& foundry in; coal->foundry in & blackpowder in; sulfur->blackpowder in).
(3) verify 100% into depot.

## PROGRESS (saved: erb-core-partial)
DONE: F1+F2 platforms; all 11 machines placed+recipes+clocks; F1 belts
(smelter->rod->rebar, foundry->pipe); OIL EXTRACTOR Build_OilPump_C_2147195583
on node BP_ResourceNode155 @1.5625% + crude pipe -> HOR refinery + HOR pipe
-> smokeless refinery (ALL fluid connected+verified); F2 belts done:
pipe->manuf, smokeless->manuf, HOR(polyresin)->container(Build_StorageContainerMk1
@183000,193400,-4400). New machine IDs (yaw0): smelter 2147206106, rod
2147205798, rebar 2147205439, foundry 2147205041, pipe 2147204620,
blackpowder 2147204177, manuf 2147203605; refineries HOR 2147211115 /
smokeless 2147210567; depot 2147209427.
BLOCKERS: (1) "Unlimited Resources for RPC Builds" mod setting is OFF ->
belts cost real materials, player out of Iron Plate -> belt builds fail
"Missing materials". USER must enable it in AIMod mod settings (RPC can't).
(2) F1->F2 lifts fail NO_FACTORY_CONNECTION (connectConveyorLift src pin)
- needs debugging; receiver splitters placed at each F1 output (x,y,-4400):
rebar recv 2147192965, pipe recv 2147191067, bp recv 2147189143.
STILL TODO: 3 lifts (rebar->manuf via 184200,194125; pipe->manuf 185000,
194125 [belt already OK from recv]; bp->smokeless 185100,191600); manuf->
depot belt (184400,195800 -> 186400,194710); POWER GENERATION (nothing
powered - build fuel gen off the abundant crude, or coal gen); REMOTE
MINING x3 (coal 206003,181237 / iron 164518,238318 / sulfur 242310,152779)
+ power + long belts (~234/514/691m); iron ore splits to smelter+foundry;
coal to foundry+blackpowder; sulfur to blackpowder; verify 100%.

## (old) TODO next: F2 (2 refineries) + F3 (manufacturer+depot) platforms (8x2)
+ machines; oil extractor on node + crude pipe up; internal belts/lifts/
pipes; power; then remote miners (coal 206003,181237 / iron 164518,238318
/ sulfur 242310,152779) + power + long belts; byproduct container; verify.
Recipe classes: iron_ingot Recipe_IngotIron; steel_ingot Recipe_IngotSteel;
iron_rod Recipe_IronRod; iron_rebar Recipe_SpikedRebar; steel_pipe
Recipe_SteelPipe; black_powder Recipe_Gunpowder; smokeless Recipe_GunpowderMK2
(OilRefinery); HOR Recipe_Alternate_HeavyOilResidue (OilRefinery);
explosive_rebar Recipe_Rebar_Explosive (Manufacturer). Extractor Recipe_OilPump.
Clocks: smokeless 50%, HOR 12.5%, manufacturer 100%.
