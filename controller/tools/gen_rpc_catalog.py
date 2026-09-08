#!/usr/bin/env python3
"""Generate the AIMod RPC catalog from the dispatcher (single source of truth).

Parses Mods/GameFeatures/AIMod/Source/AIMod/Private/AIModHttpServerSubsystem.cpp
for every `world.*` method and its params, merges in the hand-authored SUMMARIES
below, and emits:
  1. Mods/.../Private/AIModRpcCatalog.gen.cpp - an embedded JSON catalog string
     that the `world.help` RPC returns live (so a source-less agent can discover
     the whole interface at runtime).
  2. docs/rpc-reference.md - a rendered snapshot of the same catalog.

Re-run this whenever methods/params change, then rebuild the mod. Param TYPES a
caller can also confirm at runtime from structured errors ("params.X must be a
non-empty string"); this catalog gives the METHOD LIST + params + summaries,
which those errors never reveal.
"""
from __future__ import annotations
import json, re, sys, pathlib

ROOT = pathlib.Path(__file__).resolve().parents[2]
DISP = ROOT / "Mods/GameFeatures/AIMod/Source/AIMod/Private/AIModHttpServerSubsystem.cpp"
GEN_CPP = ROOT / "Mods/GameFeatures/AIMod/Source/AIMod/Private/AIModRpcCatalog.gen.cpp"
REF_MD = ROOT / "docs/rpc-catalog.md"

# Concise one-line summaries (hand-authored). Keep terse; deep detail lives in
# docs/factory-placement-guide.md, docs/vehicle-placement-guide.md, docs/telemetry-protocol.md.
SUMMARIES = {
 "world.resourceNodes":"List resource nodes/deposits (type, purity, position, occupied).",
 "world.buildables":"List placed buildables (id, class, position, bounds); optional id/box filter.",
 "world.connections":"List factory (belt/pipe) connection components and their connected state.",
 "world.connectorLayout":"Per-class connector geometry (offsets/normals) + walkway data for one buildableClass.",
 "world.manufacturers":"List production machines with recipe, clock, and inventories.",
 "world.targetedManufacturer":"Telemetry for the machine the player is currently aiming at.",
 "world.pipeConnections":"List pipe/fluid connection components.",
 "world.player":"Local player position and rotation.",
 "world.playerInventory":"Contents of the local player's inventory.",
 "world.centralStorage":"Dimensional Depot (central storage) contents and built state.",
 "world.withdrawFromCentralStorage":"Withdraw items from the Dimensional Depot to the player inventory (clamped to what the Depot holds + player room).",
 "world.uploadToCentralStorage":"Upload items from the player inventory into the Dimensional Depot (stack-granular; clamped to Depot capacity).",
 "world.chatHistory":"Recent in-game chat messages.",
 "world.sendChatMessage":"Post a chat message (local).",
 "world.waterVolumes":"List water volumes (for water extractor placement).",
 "world.groundHeight":"Ground Z at an (x,y) via trace; may hit the mod's own buildables.",
 "world.terrainHeightGrid":"Batched terrain-height survey over a grid (minX/minY/maxX/maxY/stepSize).",
 "world.recipeCatalog":"All recipes (recipeClass, ingredients, products).",
 "world.itemCatalog":"All item descriptors.",
 "world.buildableCatalog":"All buildable classes and their recipes.",
 "world.constructionCost":"Real total build cost for a recipe (incl. customization).",
 "world.simulatedCraft":"Simulate crafting a HANDHELD/equipment recipe (ingredient check); not factory recipes.",
 "world.conveyorBeltTiers":"Belt tiers and their speeds.",
 "world.conveyorLiftTiers":"Lift tiers and their speeds.",
 "world.conveyorAttachments":"Conveyor attachment (splitter/merger) catalog; supportsSortRules flag.",
 "world.pipelineTiers":"Pipeline tiers.",
 "world.pipelinePumpTiers":"Pipeline pump tiers/head-lift.",
 "world.pipeReservoirTiers":"Fluid buffer/reservoir tiers.",
 "world.pipeFluidBoxes":"Per-pipe fluid contents/flow.",
 "world.powerLineLimits":"Power line max length + power-tower length + cost.",
 "world.powerPoles":"List power poles (type, hasPower, free connections); positions via world.buildables.",
 "world.priorityPowerSwitches":"List priority power switches and their priorities.",
 "world.setPowerSwitchOn":"Turn a power switch on/off.",
 "world.setPriorityPowerSwitchPriority":"Set a priority power switch's priority group.",
 "world.activeEvents":"Active world/seasonal events.",
 "world.milestoneProgress":"HUB milestone/tech-tier progress.",
 "world.payMilestone":"Pay a milestone (pure bookkeeping; HUB has no inventory).",
 "world.mamStatus":"M.A.M. research status (ongoing/completed/hard drives).",
 "world.startMamResearch":"Start a M.A.M. research node.",
 "world.claimMamResearch":"Claim a completed M.A.M. research.",
 "world.claimMamHardDriveReward":"Claim a hard-drive alternate-recipe reward.",
 "world.rerollMamHardDrive":"Reroll a hard-drive's offered rewards.",
 "world.timeOfDay":"Current in-game time (hour/minute/isDay).",
 "world.setTimeOfDay":"Set the in-game time of day.",
 "world.mapMarkerIcons":"Available map-marker icon ids.",
 "world.mapMarkers":"List placed map markers.",
 "world.placeMapMarker":"Place a map marker at a position.",
 "world.removeMapMarker":"Remove a map marker by id.",
 "world.saveGame":"Save the game (name optional).",
 "world.placeBuilding":"Place a buildable (machine/foundation/pole/etc.). Always pass explicit yaw; gridSnapSize 0 for precision.",
 "world.placeExtractor":"Place a resource extractor (miner) on a node - NOT placeBuilding.",
 "world.placePortableMiner":"Place a portable miner on a node (needs a portable-miner ITEM in inventory).",
 "world.retrievePortableMinerInventory":"Empty a portable miner's output inventory.",
 "world.movePortableMinerToInventory":"Pick a portable miner back up into the player inventory.",
 "world.portableMiners":"List placed portable miners.",
 "world.setRecipe":"Set a machine's recipe (+optional clock).",
 "world.setClockSpeed":"Set a machine's clock % (range dynamic; install shards for >100).",
 "world.installPowerShard":"Install power shard(s) into a machine to raise its clock cap.",
 "world.setBuildableRotation":"Rotate a placed buildable (fails on lightweight/instanced foundations).",
 "world.setBuildableColor":"Recolor a buildable (machines only; fails on lightweight foundations).",
 "world.setSplitterSortRules":"Set a programmable splitter's per-output sort rules.",
 "world.splitterSortRules":"Read programmable/smart splitter sort rules.",
 "world.setBeamLength":"Set a placed beam's length.",
 "world.constructBeam":"Build a beam between two points.",
 "world.constructStackableSupport":"Build a stackable support at a position.",
 "world.constructStackableSupportOnTop":"Stack a support on top of an existing one.",
 "world.connectConveyor":"Connect a belt source->dest (pin connector positions; verify, may report success while unattached).",
 "world.testConveyorBelt":"Dry-run a belt connection (validation only).",
 "world.connectConveyorLift":"Connect a vertical lift source->dest (dest directly above; bridge residual with a belt).",
 "world.testConveyorLift":"Dry-run a lift connection.",
 "world.testConveyorSnap":"Dry-run a conveyor snap check.",
 "world.connectPipe":"Connect a pipe source->dest (can silently connect wrong buildables; verify).",
 "world.testPipe":"Dry-run a pipe connection.",
 "world.connectHypertube":"Connect a hypertube between two hypertube buildables.",
 "world.testHypertube":"Dry-run a hypertube connection.",
 "world.connectPower":"Connect a power line A<->B (pin connectorPositionA/B; no length limit).",
 "world.testPowerConnection":"Dry-run a power connection.",
 "world.constructRailroadTrack":"Build rail track between two rail buildables (pin connectors; drivable joint pending).",
 "world.testRailroadTrack":"Dry-run a rail track build.",
 "world.constructWaterPumpAtPosition":"Build a water extractor at a water-volume position.",
 "world.constructWaterPumpNearReference":"Build a water extractor near an existing reference pump.",
 "world.constructVehicle":"Spawn a vehicle (truck/tractor/explorer/loco/wagon; +droneStationId for a drone).",
 "world.constructVehiclePathSegment":"Build a directed vehicle-path segment (auto-creates path nodes).",
 "world.vehiclePathNodes":"List vehicle path nodes (guid, network id, connection counts).",
 "world.mergeVehiclePathNodes":"Fold one path node's connections into another (wire a docking node into a loop).",
 "world.vehicles":"List vehicles (id, class, position).",
 "world.setTruckAutopilot":"Arm a truck's autopilot with a station route (+fuel); returns rich diagnostics.",
 "world.truckStations":"List truck/docking stations and docked-vehicle state.",
 "world.trainStations":"List train stations (+trackGraphId).",
 "world.trains":"List trains and self-driving state.",
 "world.trainCargoPlatforms":"List train cargo/freight platforms.",
 "world.setTrainTimetable":"Set a train's timetable (stops keyed by stationBuildableId).",
 "world.setTrainSelfDriving":"Enable/disable a train's self-driving.",
 "world.droneStations":"List drone stations (paired id, drone status, fuel, inventories).",
 "world.pairDroneStations":"Pair two drone stations (call BOTH ways for a working route).",
 "world.addItemsToInventory":"Inject items into a buildable inventory (storage/chest, drone input/output/fuel [arms drone fuel], truck-station fuel).",
 "world.removeItemsFromInventory":"Remove/delete items from a buildable inventory (storage/chest, drone, truck-station); items are destroyed, not moved.",
 "world.addItemsToPlayerInventory":"Inject items into the local player's inventory (creative; e.g. a portable-miner item, fuel). Respects slot/stack limits.",
 "world.spawnCreature":"Spawn a creature (gated by the AllowCreatureSpawning mod setting).",
 "world.despawnCreature":"Despawn a creature by id.",
 "world.creatures":"List creatures (state, controller, anim instance) - tells animated vs frozen.",
 "world.deleteBuilding":"Dismantle a buildable or vehicle by id.",
 "world.cleanupOrphanedFlowIndicators":"Remove orphaned pipe flow indicators near a position.",
 "world.batch":"Run up to 100 ops in one call (per-op results; proximity still applies).",
 "world.help":"This catalog: every RPC method with params + a one-line summary (runtime self-description).",
 "world.splineGeometry":"Spline geometry (points/length) for a belt/pipe/hypertube/track buildable.",
 "world.teleportPlayer":"Teleport the local player to (x,y,z) (+optional yaw). Use to satisfy camera-distance-sensitive connect/place calls.",
}

def parse():
    src = DISP.read_text(encoding="utf-8", errors="ignore")
    # Split into dispatch blocks at each `Method == TEXT("world.X")`.
    marks = list(re.finditer(r'Method == TEXT\(\s*"(world\.[A-Za-z]+)"\s*\)', src))
    methods = {}
    for i, m in enumerate(marks):
        name = m.group(1)
        end = marks[i+1].start() if i+1 < len(marks) else min(len(src), m.end()+3500)
        block = src[m.start():end]
        # A handler may guard several methods with `|| Method == TEXT("...")` on
        # the same line; group them so aliases (e.g. test*/connect*) share params.
        line_end = src.find("\n", m.start())
        alias = re.findall(r'Method == TEXT\(\s*"(world\.[A-Za-z]+)"\s*\)', src[m.start():line_end+1])
        names = alias if alias else [name]
        # params: TryGet<Type>Field(TEXT("x")); required if negated in an if.
        params = {}
        for pm in re.finditer(r'(!?)\s*\w+->TryGet(String|Number|Bool|Array|Object)Field\(\s*TEXT\("(\w+)"\)', block):
            neg, typ, pname = pm.group(1), pm.group(2), pm.group(3)
            if pname == "params":
                continue
            jtype = {"String":"string","Number":"number","Bool":"bool","Array":"array","Object":"object"}[typ]
            if pname not in params:
                params[pname] = {"name": pname, "type": jtype, "required": bool(neg)}
            elif neg:
                params[pname]["required"] = True
        is_read = "MethodResultJson =" in block
        if name.rsplit(".",1)[-1].startswith(("construct","place","connect","test","merge")):
            cat = "build"
        elif is_read:
            cat = "telemetry"
        else:
            cat = "command"
        for nm in names:
            if nm not in methods:
                methods[nm] = {"method": nm, "category": cat, "params": list(params.values())}
    return dict(sorted(methods.items()))

def build_catalog(methods):
    entries = []
    for nm, info in methods.items():
        entries.append({
            "method": nm,
            "category": info["category"],
            "summary": SUMMARIES.get(nm, ""),
            "params": info["params"],
        })
    return {"protocolVersion": 1, "methodCount": len(entries), "methods": entries}

def emit_cpp(cat):
    body = json.dumps(cat, separators=(",", ":"))
    delim = "RPCCATALOG"
    assert delim not in body
    # MSVC caps a single string literal at ~16380 bytes (C2026); emit the JSON
    # as adjacent raw-string literals (the compiler concatenates them). Split at
    # fixed offsets - the reassembled bytes are identical regardless of where we
    # cut.
    chunk = 8000
    parts = [body[i:i + chunk] for i in range(0, len(body), chunk)]
    literals = "\n".join(f'\tTEXT(R"{delim}({p}){delim}")' for p in parts)
    txt = (
        "// GENERATED by controller/tools/gen_rpc_catalog.py - DO NOT EDIT BY HAND.\n"
        "// Re-run that script after changing RPC methods/params, then rebuild.\n"
        "// Served verbatim by the world.help RPC so source-less agents can\n"
        "// discover the whole interface at runtime.\n"
        '#include "CoreMinimal.h"\n\n'
        "const TCHAR* GAIModRpcCatalogJson =\n" + literals + ";\n"
    )
    GEN_CPP.write_text(txt, encoding="utf-8")

def emit_md(cat):
    lines = ["# AIMod RPC catalog (generated)", "",
             f"Auto-generated from the dispatcher by `controller/tools/gen_rpc_catalog.py` — the "
             f"always-current, complete list of **{cat['methodCount']} methods** with params + one-line "
             "summaries. The running mod serves this same catalog live via the `world.help` RPC. Param "
             "*types* can also be confirmed at runtime from structured errors like "
             "`params.buildableId must be a non-empty string`.", "",
             "For richer, hand-written detail (examples, a Connecting section, per-method notes) see "
             "`RPC_REFERENCE.md` in the repo root — but it is maintained by hand and can lag; trust "
             "`world.help` / this file on any conflict. Deep placement guidance: "
             "`docs/factory-placement-guide.md`, `docs/vehicle-placement-guide.md`.", ""]
    bycat = {}
    for e in cat["methods"]:
        bycat.setdefault(e["category"], []).append(e)
    for c in ("telemetry", "build", "command"):
        if c not in bycat:
            continue
        lines.append(f"## {c}")
        lines.append("")
        for e in sorted(bycat[c], key=lambda x: x["method"]):
            ps = ", ".join(f"{p['name']}:{p['type']}{'' if p['required'] else '?'}" for p in e["params"]) or "(none)"
            lines.append(f"- **`{e['method']}`** — {e['summary'] or '(see guides)'}  \n  params: `{ps}`")
        lines.append("")
    lines.append("_`name:type` = required, `name:type?` = optional. Nested object params "
                 "(e.g. connector positions {x,y,z}) are summarized; see the guides for shapes._")
    REF_MD.write_text("\n".join(lines), encoding="utf-8")

def _render(cat):
    """Return (cpp_text, md_text) without writing, for --check."""
    import io
    global GEN_CPP, REF_MD
    real_cpp, real_md = GEN_CPP, REF_MD
    tmp = pathlib.Path
    buf = {}
    class _P:
        def __init__(self, key): self.key = key
        def write_text(self, s, encoding=None): buf[self.key] = s
        def relative_to(self, *_): return self.key
    GEN_CPP, REF_MD = _P("cpp"), _P("md")
    try:
        emit_cpp(cat); emit_md(cat)
    finally:
        GEN_CPP, REF_MD = real_cpp, real_md
    return buf["cpp"], buf["md"]


if __name__ == "__main__":
    methods = parse()
    cat = build_catalog(methods)
    missing = [e["method"] for e in cat["methods"] if not e["summary"]]

    if "--check" in sys.argv:
        # CI/self-test: fail if the committed catalog/reference are stale vs the
        # current dispatcher. See CLAUDE.md Definition-of-Done item 9.
        want_cpp, want_md = _render(cat)
        have_cpp = GEN_CPP.read_text(encoding="utf-8") if GEN_CPP.exists() else ""
        have_md = REF_MD.read_text(encoding="utf-8") if REF_MD.exists() else ""
        stale = []
        if want_cpp != have_cpp: stale.append(str(GEN_CPP.relative_to(ROOT)))
        if want_md != have_md: stale.append(str(REF_MD.relative_to(ROOT)))
        if missing:
            print(f"FAIL: {len(missing)} methods have no summary: {missing}")
        if stale:
            print(f"FAIL: RPC catalog is stale - re-run gen_rpc_catalog.py: {stale}")
        if stale or missing:
            sys.exit(1)
        print(f"OK: RPC catalog current ({cat['methodCount']} methods).")
        sys.exit(0)

    emit_cpp(cat)
    emit_md(cat)
    print(f"methods: {cat['methodCount']}")
    print(f"wrote {GEN_CPP.relative_to(ROOT)} and {REF_MD.relative_to(ROOT)}")
    if missing:
        print(f"WARNING: {len(missing)} methods have no summary: {missing}")
