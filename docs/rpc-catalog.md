# AIMod RPC catalog (generated)

Auto-generated from the dispatcher by `controller/tools/gen_rpc_catalog.py` — the always-current, complete list of **105 methods** with params + one-line summaries. The running mod serves this same catalog live via the `world.help` RPC. Param *types* can also be confirmed at runtime from structured errors like `params.buildableId must be a non-empty string`.

For richer, hand-written detail (examples, a Connecting section, per-method notes) see `RPC_REFERENCE.md` in the repo root — but it is maintained by hand and can lag; trust `world.help` / this file on any conflict. Deep placement guidance: `docs/factory-placement-guide.md`, `docs/vehicle-placement-guide.md`.

## telemetry

- **`world.activeEvents`** — Active world/seasonal events.  
  params: `(none)`
- **`world.buildableCatalog`** — All buildable classes and their recipes.  
  params: `(none)`
- **`world.centralStorage`** — Dimensional Depot (central storage) contents and built state.  
  params: `(none)`
- **`world.chatHistory`** — Recent in-game chat messages.  
  params: `(none)`
- **`world.cleanupOrphanedFlowIndicators`** — Remove orphaned pipe flow indicators near a position.  
  params: `(none)`
- **`world.conveyorAttachments`** — Conveyor attachment (splitter/merger) catalog; supportsSortRules flag.  
  params: `(none)`
- **`world.conveyorBeltTiers`** — Belt tiers and their speeds.  
  params: `(none)`
- **`world.conveyorLiftTiers`** — Lift tiers and their speeds.  
  params: `(none)`
- **`world.creatures`** — List creatures (state, controller, anim instance) - tells animated vs frozen.  
  params: `(none)`
- **`world.droneStations`** — List drone stations (paired id, drone status, fuel, inventories).  
  params: `(none)`
- **`world.groundHeight`** — Ground Z at an (x,y) via trace; may hit the mod's own buildables.  
  params: `x:number, y:number, z:number?`
- **`world.help`** — This catalog: every RPC method with params + a one-line summary (runtime self-description).  
  params: `(none)`
- **`world.itemCatalog`** — All item descriptors.  
  params: `(none)`
- **`world.mamStatus`** — M.A.M. research status (ongoing/completed/hard drives).  
  params: `(none)`
- **`world.manufacturers`** — List production machines with recipe, clock, and inventories.  
  params: `(none)`
- **`world.mapMarkerIcons`** — Available map-marker icon ids.  
  params: `(none)`
- **`world.mapMarkers`** — List placed map markers.  
  params: `(none)`
- **`world.milestoneProgress`** — HUB milestone/tech-tier progress.  
  params: `(none)`
- **`world.pipeConnections`** — List pipe/fluid connection components.  
  params: `(none)`
- **`world.pipeFluidBoxes`** — Per-pipe fluid contents/flow.  
  params: `(none)`
- **`world.pipeReservoirTiers`** — Fluid buffer/reservoir tiers.  
  params: `(none)`
- **`world.pipelinePumpTiers`** — Pipeline pump tiers/head-lift.  
  params: `(none)`
- **`world.pipelineTiers`** — Pipeline tiers.  
  params: `(none)`
- **`world.player`** — Local player position and rotation.  
  params: `(none)`
- **`world.playerInventory`** — Contents of the local player's inventory.  
  params: `(none)`
- **`world.portableMiners`** — List placed portable miners.  
  params: `(none)`
- **`world.powerLineLimits`** — Power line max length + power-tower length + cost.  
  params: `(none)`
- **`world.powerPoles`** — List power poles (type, hasPower, free connections); positions via world.buildables.  
  params: `(none)`
- **`world.priorityPowerSwitches`** — List priority power switches and their priorities.  
  params: `(none)`
- **`world.recipeCatalog`** — All recipes (recipeClass, ingredients, products).  
  params: `(none)`
- **`world.resourceNodes`** — List resource nodes/deposits (type, purity, position, occupied).  
  params: `(none)`
- **`world.splineGeometry`** — Spline geometry (points/length) for a belt/pipe/hypertube/track buildable.  
  params: `buildableId:string`
- **`world.splitterSortRules`** — Read programmable/smart splitter sort rules.  
  params: `(none)`
- **`world.targetedManufacturer`** — Telemetry for the machine the player is currently aiming at.  
  params: `(none)`
- **`world.terrainHeightGrid`** — Batched terrain-height survey over a grid (minX/minY/maxX/maxY/stepSize).  
  params: `minX:number, minY:number, maxX:number, maxY:number, stepSize:number, z:number?`
- **`world.timeOfDay`** — Current in-game time (hour/minute/isDay).  
  params: `(none)`
- **`world.trainCargoPlatforms`** — List train cargo/freight platforms.  
  params: `(none)`
- **`world.trainStations`** — List train stations (+trackGraphId).  
  params: `(none)`
- **`world.trains`** — List trains and self-driving state.  
  params: `(none)`
- **`world.truckStations`** — List truck/docking stations and docked-vehicle state.  
  params: `(none)`
- **`world.vehiclePathNodes`** — List vehicle path nodes (guid, network id, connection counts).  
  params: `(none)`
- **`world.vehicles`** — List vehicles (id, class, position).  
  params: `(none)`
- **`world.waterVolumes`** — List water volumes (for water extractor placement).  
  params: `(none)`

## build

- **`world.connectConveyor`** — Connect a belt source->dest (pin connector positions; verify, may report success while unattached).  
  params: `(none)`
- **`world.connectConveyorLift`** — Connect a vertical lift source->dest (dest directly above; bridge residual with a belt).  
  params: `(none)`
- **`world.connectHypertube`** — Connect a hypertube between two hypertube buildables.  
  params: `(none)`
- **`world.connectPipe`** — Connect a pipe source->dest (can silently connect wrong buildables; verify).  
  params: `(none)`
- **`world.connectPower`** — Connect a power line A<->B (pin connectorPositionA/B; no length limit).  
  params: `(none)`
- **`world.connectorLayout`** — Per-class connector geometry (offsets/normals) + walkway data for one buildableClass.  
  params: `(none)`
- **`world.constructBeam`** — Build a beam between two points.  
  params: `recipeClass:string, startX:number, startY:number, endX:number, endY:number, startZ:number?, endZ:number?, ignoreGroundTrace:bool?, freeformMode:bool?, rotationScrollSteps:number?`
- **`world.constructRailroadTrack`** — Build rail track between two rail buildables (pin connectors; drivable joint pending).  
  params: `(none)`
- **`world.constructStackableSupport`** — Build a stackable support at a position.  
  params: `recipeClass:string, x:number, y:number, z:number?, ignoreGroundTrace:bool?, stackCount:number?`
- **`world.constructStackableSupportOnTop`** — Stack a support on top of an existing one.  
  params: `referenceBuildableId:string, recipeClass:string`
- **`world.constructVehicle`** — Spawn a vehicle (truck/tractor/explorer/loco/wagon; +droneStationId for a drone).  
  params: `recipeClass:string, droneStationId:string?, x:number?, y:number?, z:number?, ignoreGroundTrace:bool?, yaw:number?`
- **`world.constructVehiclePathSegment`** — Build a directed vehicle-path segment (auto-creates path nodes).  
  params: `recipeClass:string, startX:number, startY:number, endX:number, endY:number, startZ:number?, endZ:number?, ignoreGroundTrace:bool?`
- **`world.constructWaterPumpAtPosition`** — Build a water extractor at a water-volume position.  
  params: `x:number, y:number, z:number, recipeClass:string?`
- **`world.constructWaterPumpNearReference`** — Build a water extractor near an existing reference pump.  
  params: `referenceBuildableId:string, offsetX:number, offsetY:number, offsetZ:number?, recipeClass:string?`
- **`world.constructionCost`** — Real total build cost for a recipe (incl. customization).  
  params: `recipeClass:string`
- **`world.mergeVehiclePathNodes`** — Fold one path node's connections into another (wire a docking node into a loop).  
  params: `sourceNodeId:string, destNodeId:string`
- **`world.placeBuilding`** — Place a buildable (machine/foundation/pole/etc.). Always pass explicit yaw; gridSnapSize 0 for precision.  
  params: `recipeClass:string, x:number, y:number, rotationScrollDelta:number?, gridSnapSize:number?, z:number?, ignoreGroundTrace:bool?, ignoreAimLocation:bool?, ignorePlayerEncroachment:bool?, ignoreClearance:bool?, ignoreInvalidFloor:bool?, yaw:number?, faceBuildableId:string?`
- **`world.placeExtractor`** — Place a resource extractor (miner) on a node - NOT placeBuilding.  
  params: `nodeId:string, recipeClass:string`
- **`world.placeMapMarker`** — Place a map marker at a position.  
  params: `x:number, y:number, iconId:number, z:number?, ignoreGroundTrace:bool?, name:string?, colorR:number?, colorG:number?, colorB:number?, scale:number?, compassViewDistance:string?`
- **`world.placePortableMiner`** — Place a portable miner on a node (needs a portable-miner ITEM in inventory).  
  params: `nodeId:string, itemClass:string?`
- **`world.testConveyorBelt`** — Dry-run a belt connection (validation only).  
  params: `(none)`
- **`world.testConveyorLift`** — Dry-run a lift connection.  
  params: `(none)`
- **`world.testConveyorSnap`** — Dry-run a conveyor snap check.  
  params: `sourceBuildableId:string`
- **`world.testHypertube`** — Dry-run a hypertube connection.  
  params: `(none)`
- **`world.testPipe`** — Dry-run a pipe connection.  
  params: `(none)`
- **`world.testPowerConnection`** — Dry-run a power connection.  
  params: `(none)`
- **`world.testRailroadTrack`** — Dry-run a rail track build.  
  params: `(none)`

## command

- **`world.addItemsToInventory`** — Inject items into a buildable inventory (drone input/output/fuel, arms drone fuel).  
  params: `buildableId:string, itemClass:string, inventoryRole:string?, amount:number`
- **`world.addItemsToPlayerInventory`** — Inject items into the local player's inventory (creative; e.g. a portable-miner item, fuel). Respects slot/stack limits.  
  params: `itemClass:string, amount:number`
- **`world.batch`** — Run up to 100 ops in one call (per-op results; proximity still applies).  
  params: `(none)`
- **`world.buildables`** — List placed buildables (id, class, position, bounds); optional id/box filter.  
  params: `(none)`
- **`world.claimMamHardDriveReward`** — Claim a hard-drive alternate-recipe reward.  
  params: `schematicClass:string`
- **`world.claimMamResearch`** — Claim a completed M.A.M. research.  
  params: `schematicClass:string`
- **`world.connections`** — List factory (belt/pipe) connection components and their connected state.  
  params: `(none)`
- **`world.deleteBuilding`** — Dismantle a buildable or vehicle by id.  
  params: `buildableId:string`
- **`world.despawnCreature`** — Despawn a creature by id.  
  params: `creatureId:string`
- **`world.installPowerShard`** — Install power shard(s) into a machine to raise its clock cap.  
  params: `buildableId:string, count:number`
- **`world.movePortableMinerToInventory`** — Pick a portable miner back up into the player inventory.  
  params: `(none)`
- **`world.pairDroneStations`** — Pair two drone stations (call BOTH ways for a working route).  
  params: `stationBuildableId:string, targetStationBuildableId:string?`
- **`world.payMilestone`** — Pay a milestone (pure bookkeeping; HUB has no inventory).  
  params: `schematicClass:string?, dryRun:bool?`
- **`world.removeMapMarker`** — Remove a map marker by id.  
  params: `markerId:string`
- **`world.rerollMamHardDrive`** — Reroll a hard-drive's offered rewards.  
  params: `schematicClass:string`
- **`world.retrievePortableMinerInventory`** — Empty a portable miner's output inventory.  
  params: `portableMinerId:string`
- **`world.saveGame`** — Save the game (name optional).  
  params: `(none)`
- **`world.sendChatMessage`** — Post a chat message (local).  
  params: `message:string, sender:string?`
- **`world.setBeamLength`** — Set a placed beam's length.  
  params: `buildableId:string, newLength:number`
- **`world.setBuildableColor`** — Recolor a buildable (machines only; fails on lightweight foundations).  
  params: `buildableId:string, primaryR:number, primaryG:number, primaryB:number, secondaryR:number?, secondaryG:number?, secondaryB:number?`
- **`world.setBuildableRotation`** — Rotate a placed buildable (fails on lightweight/instanced foundations).  
  params: `buildableId:string, yaw:number`
- **`world.setClockSpeed`** — Set a machine's clock % (range dynamic; install shards for >100).  
  params: `(none)`
- **`world.setPowerSwitchOn`** — Turn a power switch on/off.  
  params: `buildableId:string, switchOn:bool`
- **`world.setPriorityPowerSwitchPriority`** — Set a priority power switch's priority group.  
  params: `buildableId:string, priority:number`
- **`world.setRecipe`** — Set a machine's recipe (+optional clock).  
  params: `(none)`
- **`world.setSplitterSortRules`** — Set a programmable splitter's per-output sort rules.  
  params: `buildableId:string, rules:array`
- **`world.setTimeOfDay`** — Set the in-game time of day.  
  params: `hour:number, minute:number?`
- **`world.setTrainSelfDriving`** — Enable/disable a train's self-driving.  
  params: `trainId:string, enabled:bool`
- **`world.setTrainTimetable`** — Set a train's timetable (stops keyed by stationBuildableId).  
  params: `trainId:string, stops:array`
- **`world.setTruckAutopilot`** — Arm a truck's autopilot with a station route (+fuel); returns rich diagnostics.  
  params: `vehicleId:string, enabled:bool, stationIds:array?, fuelItemClass:string?, fuelAmount:number?`
- **`world.simulatedCraft`** — Simulate crafting a HANDHELD/equipment recipe (ingredient check); not factory recipes.  
  params: `recipeClass:string`
- **`world.spawnCreature`** — Spawn a creature (gated by the AllowCreatureSpawning mod setting).  
  params: `creatureClass:string, distanceFromPlayer:number?, scale:number?`
- **`world.startMamResearch`** — Start a M.A.M. research node.  
  params: `schematicClass:string, researchTreeClass:string, dryRun:bool?`
- **`world.teleportPlayer`** — Teleport the local player to (x,y,z) (+optional yaw). Use to satisfy camera-distance-sensitive connect/place calls.  
  params: `x:number, y:number, z:number?, ignoreGroundTrace:bool?, yaw:number?`
- **`world.withdrawFromCentralStorage`** — Withdraw items from the Dimensional Depot to the player.  
  params: `itemClass:string, amount:number`

_`name:type` = required, `name:type?` = optional. Nested object params (e.g. connector positions {x,y,z}) are summarized; see the guides for shapes._