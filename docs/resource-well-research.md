# Resource Well Pressurizer/Extractor research — 2026-08-27

Recorded in response to the user asking whether Resource Well
Pressurizers/Extractors could be placed, and (they could not, at the
time) asking for the capability to be added. Investigated via a dedicated
research pass over the installed FactoryGame headers/uassets before
extending `ConstructExtractorOnNode` - same posture as
`docs/hypertube-research.md` earlier the same day.

**Caveat inherited from every other research doc in this project**: the
`Source/FactoryGame/Private/**/*.cpp` files in this workspace are
UHT-stub-generated (empty bodies) — declarations, class hierarchy,
enums, and public API are real and citable; internal *logic* is not
readable from source and is inferred from behavior/localized strings,
same posture as belts/pipes/hypertube elsewhere in this project.

## Naming: "Resource Well" is UI-only, the code says "Fracking"

| Building | Recipe | Buildable class | Header |
|---|---|---|---|
| Pressurizer | `Recipe_FrackingSmasher` | `AFGBuildableFrackingActivator` | `Source/FactoryGame/Public/Buildables/FGBuildableFrackingActivator.h:15` |
| Extractor (satellite) | `Recipe_FrackingExtractor` | `AFGBuildableFrackingExtractor` | `Source/FactoryGame/Public/Buildables/FGBuildableFrackingExtractor.h:14` |

No `AFGBuildableResourceWellPressurizer`/`Extractor` class exists — "Resource
Well" only appears as the player-facing localized name (e.g.
`Content/FactoryGame/Buildable/Factory/FrackingExtractor/Build_FrackingExtractor.uasset`'s
`Production/ResourceWellExtractor/Description`).

Hierarchy:
- `AFGBuildableFrackingActivator : AFGBuildableResourceExtractorBase`
  (`FGBuildableFrackingActivator.h:15`) — **not** an
  `AFGBuildableResourceExtractor`; no output inventory/pipe of its own.
- `AFGBuildableFrackingExtractor : AFGBuildableResourceExtractor : AFGBuildableResourceExtractorBase`
  (`FGBuildableFrackingExtractor.h:14`, `FGBuildableResourceExtractor.h:13`).

Both use the plain `AFGResourceExtractorHologram` — same class Miners
use, set as the default in `AFGBuildableResourceExtractorBase`'s
constructor (`Private/Buildables/FGBuildableResourceExtractorBase.cpp:29`),
not overridden by either fracking class or their Blueprint CDOs (checked
both `.uasset`s for an `mHologramClass` property - none present). **This
is why `ConstructExtractorOnNode`'s existing hologram-driving flow
(HotKeyRecipe + synthetic FHitResult + UpdateHologramPlacement/
TrySnapToActor + InternalConstructHologram, including the `Distance` fix
documented in that function) applies unchanged to both buildings** -
confirmed live, no new construction logic needed, only a wider node
lookup and recipe parameterization (see "What changed" below).

## The real target: two distinct node types, NOT the same `AFGResourceNode` a Miner uses

`Source/FactoryGame/Public/Resources/FGResourceNodeBase.h:23-33`:

```cpp
UENUM( BlueprintType )
enum class EResourceNodeType : uint8
{
    Node,
    FrackingSatellite,
    FrackingCore,
    Geyser,
    Deposit,
    Invalid
};
```

- **`AFGResourceNodeFrackingCore : AFGResourceNodeBase`** directly
  (`Public/Resources/FGResourceNodeFrackingCore.h:14`) — class comment:
  *"A node whose purpose is to let a AFGBuildableFrackingActivator
  activate a group of AFGResourceNodeFrackingSatellite nodes."* **This is
  the Pressurizer's real target, and it is NOT an `AFGResourceNode`** -
  `ConstructExtractorOnNode`'s old `TActorIterator<AFGResourceNode>` scan
  could never find it. World instances are named `BP_FrackingCore<N>`.
- **`AFGResourceNodeFrackingSatellite : AFGResourceNode`**
  (`Public/Resources/FGResourceNodeFrackingSatellite.h:23`) — class
  comment: *"A node that must be activated through an
  AFGResourceNodeFrackingCore node before it can be mined by an
  AFGBuildableFrackingExtractor."* This one WAS already reachable (it's a
  real `AFGResourceNode`), which is why `world.resourceNodes` already
  showed `BP_FrackingSatellite*` entries with `resource: "Water"` before
  any of this work.

**Fix**: widened `ConstructExtractorOnNode`'s lookup from
`TActorIterator<AFGResourceNode>` to `TActorIterator<AFGResourceNodeBase>`
— strictly wider, not a behavior change for existing callers (normal
nodes and satellites were already `AFGResourceNode`; cores are the new
reachable case).

Placement API lives on the shared base, so the synthetic-hit trick works
identically for a core:
- `GetPlacementLocation(const FVector&)` / `GetPlacementRotation(const FVector&)`
  — `FGResourceNodeBase.h:157,159`
- `IsOccupied()` / `GetResourceClass()` / `GetResourceNodeType()` —
  `:140, :144, :176`

**One real API asymmetry to know about**: `GetResourcePurity()` is
declared on `AFGResourceNode` only (`FGResourceNode.h:113`), **not** on
`AFGResourceNodeBase` — a Fracking Core has no purity of its own.
`MakeResourceNodeTelemetry` now `Cast<AFGResourceNode>` before calling
it, reporting `"N/A"` for cores instead.

Navigation between core and satellites (all public):
- satellite → core: `AFGResourceNodeFrackingSatellite::GetCore()` →
  `TWeakObjectPtr<AFGResourceNodeFrackingCore>` (`FGResourceNodeFrackingSatellite.h:61`)
- core → satellites: `GetSatellites(TArray<AFGResourceNodeFrackingSatellite*>&)`
  (`FGResourceNodeFrackingCore.h:44-45`)
- core → existing pressurizer: `GetActivator()` →
  `TWeakObjectPtr<AFGBuildableFrackingActivator>`, null if none built yet
  (`FGResourceNodeFrackingCore.h:39`)

There is no separate "whole cluster" actor beyond the core - the core
node **is** the cluster's representation.

Snap targeting is real, engine-enforced restriction, not a soft
suggestion - confirmed from both classes' `.uasset` CDOs:
- `Build_FrackingSmasher.uasset` → `mRestrictToNodeType` = `BP_FrackingCore_C`,
  `mMustPlaceOnResourceDisqualifier` = `UFGCDNeedsFrackingCoreNode`
  (`Public/FGConstructDisqualifier.h:293`).
- `Build_FrackingExtractor.uasset` → `mRestrictToNodeType` = `BP_FrackingSatellite_C`,
  `mMustPlaceOnResourceDisqualifier` = `UFGCDNeedsFrackingSatelliteNode`
  (`FGConstructDisqualifier.h:304`).

`mRestrictToNodeType` itself: `Public/Buildables/FGBuildableResourceExtractorBase.h:140-142`,
*"If set, this extractor type can only be placed on this node type."*
This - not a manual resource-form check in our own code - is what should
gate a mismatched recipe/node pairing (see "What changed" below).

## The real, hard sequencing requirement: satellites must be activated first

This is the piece that matches the user's own description of the
required order and is a genuine construction-time gate, not just
gameplay advice:

- `AFGResourceNodeFrackingSatellite::CanPlaceResourceExtractor()` is
  **overridden** (`FGResourceNodeFrackingSatellite.h:40`) — unlike a
  plain `AFGResourceNode`, which just returns `mCanPlaceResourceExtractor`
  (default `true`, `Private/Resources/FGResourceNode.cpp:19`).
- Localized disqualifier text confirms it requires *activation*, not just
  "is a satellite": `Content/Localization/StringTables/Messages_UI.csv:24`
  → `"Must be placed on an activated Fracking Satellite Node!"` (compare
  the core's own text at line 23, which has no activation wording: *"Must
  be placed on a Fracking Core Node!"*).
- Satellite state: `EFrackingSatelliteState { FSS_Untouched, FSS_Active,
  FSS_Inactive }` (`FGResourceNodeFrackingSatellite.h:11-17`), starts
  `FSS_Untouched`. Real accessor: `GetState()`, `BlueprintPure`
  (`:52`).
- Activation is driven by the Pressurizer producing:
  `AFGBuildableFrackingActivator::Factory_TickProducing` +
  `mActivationStartupTime`/`mActivationStartupTimer`
  (`FGBuildableFrackingActivator.h:86-96`), which eventually calls
  `AFGResourceNodeFrackingCore::Factory_SetSatellitesActive(bool)`
  (`FGResourceNodeFrackingCore.h:34`). `Build_FrackingSmasher.uasset`
  does override `mActivationStartupTime` (a real, non-zero delay - exact
  seconds not decodable from the asset string table, verify live).
- The satellite extractor itself has **no power connector of its own** -
  `AFGBuildableFrackingExtractor::Factory_HasPower()` is overridden
  (`FGBuildableFrackingExtractor.h:26`) to derive power state from its
  core's activator instead. **Only the Pressurizer needs a real power
  connection** (`Build_FrackingSmasher.uasset` has exactly one ordinary
  `FGPowerConnectionComponent`, `Build_FrackingExtractor.uasset` has
  none) - don't try to wire power to a satellite extractor.

**Confirmed sequencing, matching exactly what the user described**:
build the Pressurizer on the core → connect power to the Pressurizer →
wait for the satellites' `GetState()` to leave `FSS_Untouched` → only
then can `Recipe_FrackingExtractor` be constructed on each satellite.
This is now exposed as `satelliteState` in `world.resourceNodes` (new
`FAIModResourceNodeTelemetry::SatelliteState`/`NodeType`/`CoreId`
fields) specifically so this can be polled for real instead of guessed
at with a fixed delay.

**Not resolved from source** (stub `.cpp`, `Private/Resources/FGResourceNodeFrackingSatellite.cpp:21`
- `CanPlaceResourceExtractor()`'s body is `return bool();`): whether the
real predicate is `mState != FSS_Untouched` or specifically
`mState == FSS_Active` - i.e. whether a satellite that later goes
`FSS_Inactive` (pressurizer loses power after having activated it once)
can still receive an extractor, or whether it reverts to blocked. Treat
`!= FSS_Untouched` as the poll condition per the strong circumstantial
evidence (class comment, disqualifier text), but don't assume an
extractor is buildable on an `Inactive` satellite without checking live
first if that state is ever observed.

## Other construction disqualifiers vs. a normal Miner

Nothing exotic beyond the node-type/activation gates above:
- No distance-from-core check, no minimum-satellite-count check, no
  special terrain/floor requirement -
  `AFGResourceExtractorHologram` sets `mNeedsValidFloor = false` and
  `mUseBuildClearanceOverlapSnapp = false`
  (`Private/Hologram/FGResourceExtractorHologram.cpp:10-11`) for this
  whole hologram class, Miners included.
- Both `.uasset`s carry `mAllowedResourceForms` including
  `EResourceForm::RF_LIQUID` and `RF_GAS`
  (`FGBuildableResourceExtractorBase.h:145-146`) - this (plus
  `mRestrictToNodeType` above) is the real engine-side gate our own code
  used to redundantly (and too narrowly) re-implement as a manual
  `Form != EResourceForm::RF_SOLID` check.
- Ordinary building clearance only (`EClearanceType::CT_Default`) - the
  Pressurizer's box is physically large, so a real-world failure mode is
  an ordinary clearance overlap with nearby terrain/buildings, same
  disqualifier family as any other big building, nothing fracking-specific.

## What changed in `ConstructExtractorOnNode` (2026-08-27)

1. Node lookup: `TActorIterator<AFGResourceNode>` → `TActorIterator<AFGResourceNodeBase>`
   (reaches Fracking Cores; strictly additive for existing callers).
2. Removed the manual `Form != EResourceForm::RF_SOLID` early-return -
   this only ever existed from the function being written/tested against
   Miners first. The real engine-side gating
   (`mAllowedResourceForms`/`mRestrictToNodeType` and their
   disqualifiers) already handles this correctly and generally; trust
   `CanConstruct()`/the poll loop's disqualifier walk for it, the same
   way this function already trusts that machinery for everything else.
3. `RecipeClassPath` is now a caller-supplied parameter
   (`world.placeExtractor`'s new optional `recipeClass`, default
   `Recipe_MinerMk1` for backward compatibility) instead of a hardcoded
   `Recipe_MinerMk1` load - any extractor recipe now works:
   `Recipe_MinerMk1`/`Mk2`/`Mk3`, `Recipe_WaterPump`, `Recipe_OilPump`,
   `Recipe_FrackingSmasher`, `Recipe_FrackingExtractor`.
4. `world.resourceNodes` gained `nodeType` (`"Node"`/`"FrackingCore"`/
   `"FrackingSatellite"`/`"Geyser"`/`"Deposit"`/`"Invalid"`), `coreId`
   (satellites only - the id of their `AFGResourceNodeFrackingCore`), and
   `satelliteState` (satellites only - `"Untouched"`/`"Active"`/
   `"Inactive"`) - the data needed to actually plan and poll the required
   build order instead of guessing.

## Live-tested 2026-08-27 — full workflow confirmed end-to-end

Same day as the research above, on a real Water Resource Well cluster
(`BP_FrackingCore10`, 7 satellites, all `resource: "Water"`):

- **`ConstructExtractorOnNode` predictably hit the same untreated
  camera-dependency bug** documented for `ConstructPipe` earlier the same
  session - failed with `"Invalid aim location!"` on the very first
  attempt at the Pressurizer, even targeting a genuinely valid,
  unoccupied core. **Fixed** with the identical pattern (deterministic
  look computed from the real placement location, reasserted every poll
  tick, plus the manual disqualifier-ignore-list replacing the raw
  `CanConstruct()` call) - this function predates that whole fix pass and
  had never been updated until now.
- **Missing materials was real, not a bug** - `Recipe_FrackingSmasher`'s
  actual cost (10 Radio Control Units, 25 Heavy Modular Frames, 50
  Motors, 50 Alclad Aluminum Sheets, 100 Rubber) genuinely wasn't in the
  player's inventory on the first attempt; resolved once gathered.
- **No power grid existed anywhere near the site** (nearest `PowerPole`
  131,676 units away, nearest generator 518,589 units away) - resolved by
  discovering **`Build_PowerTowerPlatform_C`** (a real, distinct
  buildable class from `PowerPoleMk1/2/3` - "Power Tower", built
  specifically for very-long-distance transmission) only 6,962 units
  away, well within normal wire range. A direct `world.connectPower`
  from the Pressurizer straight to the tower worked with no intermediate
  pole needed, confirmed via a genuine new `Build_PowerLine_C` actor.
  **Worth remembering for any future remote-site power problem**: search
  for `"PowerTower"` in `buildableClass`, not just `"PowerPole"` - a
  tower search can turn up a much closer real connection point than a
  pole-only search would suggest.
- **The Pressurizer→power→satellite-activation sequence is real and
  observable**, not just a documented gate: all 7 satellites' `satelliteState`
  genuinely transitioned `"Untouched"` → `"Active"` in `world.resourceNodes`
  within moments of the Pressurizer's power connection landing -
  confirmed by polling, not assumed from a fixed delay.
- **The satellite-extractor construction disqualifier flickered once**:
  the very first `Recipe_FrackingExtractor` attempt on an already-`Active`
  satellite failed with `"Must be placed on an activated Fracking
  Satellite Node!"` (plus an `"Encroaching another object's clearance!"`
  soft disqualifier) despite `world.resourceNodes` already showing
  `satelliteState: "Active"` at the time - an immediate retry with zero
  other changes succeeded. Matches this project's established
  "transient disqualifier flakiness, retry once" pattern (see
  `docs/placement-lessons.md`) rather than indicating the activation
  state itself was wrong. Two subsequent extractors on different
  satellites both succeeded on the first attempt.
- Final state: Pressurizer built and powered, 3 of 7 satellites given
  real `Build_FrackingExtractor` buildings, all confirmed via
  `world.resourceNodes` showing `occupied: true` at each satellite's
  exact position, not just `success: true`.

See `docs/placement-lessons.md` for the consolidated writeup of the
camera-independence fix and the Power Tower discovery.
