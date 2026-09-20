# Hypertube tube-segment research — 2026-08-27

Recorded in response to the user asking to test longer pipe runs and add
Hypertube support "where missing." Investigated via a dedicated research
pass over the installed FactoryGame headers/uassets in this workspace
before writing `ConstructHypertube` (mirroring how
`docs/conveyor-attachment-research.md` preceded the splitter/merger work).

**Caveat inherited from every other research doc in this project**: the
`Source/FactoryGame/Private/**/*.cpp` files in this workspace are
UHT-stub-generated (empty bodies) — declarations, class hierarchy,
`UPROPERTY` defaults, and constructor CDO values are real and citable;
internal *logic* (e.g. what `AFGPipelineHologram::TrySnapToActor` actually
does) is not readable from source and is inferred from behavior, same
posture as belts/pipes/power elsewhere in this project.

## The naming trap: "Hypertube" only exists in asset/UI names

Every recipe you'd find by searching the live `world.buildableCatalog`
for `"HyperTube"` — `Recipe_HyperTubeJunction`, `Recipe_HyperTubeTJunction`,
`Recipe_HyperTubeWallSupport`, `Recipe_HyperTubeWallHole` — is an
**attachment**, not the tube. In C++ the whole family is named `PipeHyper`
instead. The actual connecting tube segment is:

- Recipe: `/Game/FactoryGame/Recipes/Buildings/Recipe_PipeHyper.Recipe_PipeHyper_C`
  (`Content/FactoryGame/Recipes/Buildings/Recipe_PipeHyper.uasset` — name
  table has product `Desc_PipeHyper_C`, ingredient `Desc_SteelPipe_C`).
- Buildable class: `AFGBuildablePipeHyper`,
  `Source/FactoryGame/Public/Buildables/FGBuildablePipeHyper.h:16` —
  `class FACTORYGAME_API AFGBuildablePipeHyper : public AFGBuildablePipeBase, public IFGPipeHyperInterface`.
  Sibling of `AFGBuildablePipeline` under the same `AFGBuildablePipeBase`
  (`FGBuildablePipeBase.h:22`), not a new hierarchy.

Other members of the family, for reference (not tube segments):

| Recipe | Buildable | Hologram |
|---|---|---|
| `Recipe_PipeHyper` (**the tube**) | `AFGBuildablePipeHyper` | `Holo_PipeHyper_C` → `AFGPipelineHologram` |
| `Recipe_HyperTubeJunction` | `AFGBuildablePipeHyperJunction` | `Holo_HyperTubeJunction_C` → `AFGPipeHyperAttachmentHologram` |
| `Recipe_HyperTubeTJunction` | `AFGBuildablePipeHyperJunction` | `Holo_HypertubeTJunction_C` |
| `Recipe_HyperTubeWallSupport`/`WallHole` | plain `AFGBuildable` | `Holo_PipelineSupportWall*_C` |
| `Recipe_PipeHyperSupport` (pole) | `AFGBuildablePolePipe` | `Holo_PipelineSupport_C` |
| entrance/exit: `Build_PipeHyperStart` | `AFGPipeHyperStart` | `Holo_PipeHyperStart_C` → `AFGPipePartHologram` |
| `Recipe_FoundationPassthrough_Hypertube` | `AFGBuildablePassthroughPipeHyper` | — |

## The tube's hologram is the SAME class `ConstructPipe` already drives

`Content/FactoryGame/Buildable/Factory/-Shared/Holo_PipeHyper.uasset`'s
name table resolves its native parent to
`/Script/FactoryGame.FGPipelineHologram` — `Holo_PipeHyper_C` is a
Blueprint child of `AFGPipelineHologram`, the identical class
`ConstructPipe` uses for `Recipe_Pipeline`/`Recipe_PipelineMK2`. No
`AFGHypertubeHologram` or `AFGPipeHyperHologram` class exists anywhere
under `Source/FactoryGame/Public/Hologram` — searched directly, the only
"Hyper" holograms present are `AFGPassthroughPipeHyperHologram`
(`FGPassthroughPipeBaseHologram.h:27`) and
`AFGPipeHyperAttachmentHologram` (`FGPipeHyperAttachmentHologram.h:8`),
neither of which builds the tube itself.

`AFGPipelineHologram` (`Source/FactoryGame/Public/Hologram/FGPipelineHologram.h`):
same spline-hologram lineage already used (`:16`
`class FACTORYGAME_API AFGPipelineHologram : public AFGSplineHologram`),
same overrides `ConstructPipe` already drives
(`DoMultiStepPlacement`/`TrySnapToActor`/`CanTakeNextBuildStep`/
`IsConnectionSnapped`, `:32/:41/:50/:60`); `GetCurrentBuildStep()` is
inherited unchanged from `FGSplineHologram.h:44`.

**Practical consequence**: `ConstructHypertube` is `ConstructPipe`'s exact
two-click `TrySnapToActor`+`DoMultiStepPlacement` flow and deferred-poll
pattern, hardcoded to `Recipe_PipeHyper` — no new hologram-driving logic
needed, only a different connector lookup (below).

## The one real difference: connector type and connection-type filtering

Hypertube connectors are `UFGPipeConnectionComponentHyper` — a trivial
type-tag subclass of `UFGPipeConnectionComponentBase` with **no added
members** (`Source/FactoryGame/Public/FGPipeConnectionComponentHyper.h:14`;
its `.cpp` ctor body is empty). `AFGBuildablePipeHyper`'s two connectors
are created as this type directly in C++:
`Source/FactoryGame/Private/Buildables/FGBuildablePipeHyper.cpp:8-11`.

Critically, hypertube connectors' `mPipeConnectionType` stays at the CDO
default `PCT_ANY` — confirmed both from the C++ default
(`Source/FactoryGame/Private/FGPipeConnectionComponent.cpp:7`:
`this->mPipeConnectionType = EPipeConnectionType::PCT_ANY;`) and by
scanning `Build_PipeHyper`/`Build_PipeHyperStart`/`Build_HyperTubeJunction`/
`Build_HypertubeTJunction`'s uassets for an `mPipeConnectionType` override
(none found), against a control group that *does* show the override
(`Build_OilRefinery`, `Build_Blender`, `Build_Packager`,
`Build_PipelinePump`/`PumpMK2` all contain real `PCT_PRODUCER`/
`PCT_CONSUMER` entries; `Build_WaterPump` only `PCT_PRODUCER`).

This means the existing `FindFreePipeConnection(Buildable, Type)` helper
— which filters by an *exact* `GetPipeConnectionType() == Type` match,
called with `PCT_PRODUCER`/`PCT_CONSUMER` — **finds nothing on any
hypertube part**. `ConstructHypertube` uses a new
`FindFreeHyperPipeConnection(Buildable)` instead: matches any
`UFGPipeConnectionComponentHyper` that isn't `PCT_SNAP_ONLY` (wall
supports/poles are explicitly non-endpoints — see
`FGPipeConnectionComponent.h:91`'s `IsConnected()` doc comment) and isn't
already connected. Hypertubes are bidirectional (no real producer/
consumer distinction), so the same finder is used for both ends —
"source"/"dest" in `ConstructHypertube` just mean which buildable's free
connector each end resolves to, not a flow direction.

Entrance/exit (`AFGPipeHyperStart`,
`Source/FactoryGame/Public/Buildables/FGPipeHyperStart.h:13`, doc comment:
*"Hypertube entrance part"*) has exactly one connector (`:31`
`mConnection0`, real component name `PipeHyperStartConnection`,
confirmed `FGPipeConnectionComponentHyper` class from
`Build_PipeHyperStart.uasset`). Note `:23` `mOpeningOffset = 350.0f` — a
real offset between the entrance actor's transform and where a tube
should actually terminate, not accounted for in `ConstructHypertube`'s
`MakeHitAt` (it uses the connector's own `GetConnectorLocation()`
directly, same as every other buildable this project drives — the
connector component should already reflect any such offset, but this is
worth re-checking if entrance/junction connections behave oddly in
practice).

## Real geometric limits (for a future pre-flight length/bend check)

No `GetFlowLimit()` analogue exists for hypertubes —
`AFGBuildablePipeHyper`'s entire public surface beyond the base pipe
class is the `IFGPipeHyperInterface` traversal API
(`FGBuildablePipeHyper.h:32-37`), no throughput member.

The geometric limits live on the shared `AFGPipelineHologram`
(`FGPipelineHologram.h:195/:203/:207` — `mBendRadius`/`mMinBendRadius`/
`mMaxSplineLength`, C++ defaults 199.0/75/5600.1 for the base class/fluid
pipes). Hand-parsed the `Holo_PipeHyper.uasset` export data for its own
CDO overrides (medium-high confidence — parsed via raw property tags in
the uasset, not read live off the class default object; treat as
"verify by reading the CDO at runtime" rather than gospel until then):

| Property | Fluid pipe default | Hypertube override |
|---|---|---|
| `mBendRadius` | 199.0 | **300.0** |
| `mMaxSplineLength` | 5600.1 | **10000.0** (100m) |
| `mMinBendRadius` | 75 | *not overridden* → inherits 75 |

If a real pre-flight distance check is added later (mirroring
`world.pipelineTiers`), it should read these off `Holo_PipeHyper_C`'s CDO
the same way `LogPipelineTiersAsJson` already does for
`Recipe_Pipeline`/`Recipe_PipelineMK2`, not hardcode the numbers above.

## Live-tested 2026-08-27 — the main uncertainty is resolved

`ConstructHypertube` (mirroring `ConstructPipe`'s flow exactly, as
predicted below) was implemented and live-tested the same day this
research was done:

- **`AFGPipelineHologram::TrySnapToActor` does accept an
  `AFGPipeHyperStart` hit the same way it accepts a fluid pump/tank** -
  confirmed, resolving the first open question below. A straight run
  between two entrances (connectors facing each other) constructed a real
  `Build_PipeHyper` segment, verified via `world.buildables` and directly
  by the user.
- **The `mOpeningOffset`/connector-location concern turned out to be a
  non-issue** - `UFGPipeConnectionComponentBase::GetConnectorLocation()`
  already reflects the real, usable connector position; no manual offset
  needed in `MakeHitAt`.
- **A real, separate rotation gotcha did surface**, unrelated to anything
  below: `AFGPipeHyperStart` has exactly one connector, so two entrances
  placed at the same default yaw both face the *same* world direction -
  a straight tube needs the downstream entrance rotated 180° so its
  normal points back at the upstream one, same "opposite normals dock"
  convention as every other connector pair in this project. See
  `docs/placement-lessons.md`'s "Pipes fixed, Hypertube support added"
  section for the full fix, and the new `world.pipeConnections` RPC that
  made it diagnosable instead of guessed.

## Genuinely open questions (not resolved by source alone, still open)

- `AFGBuildablePipeBase::GetConnectionType()` is a `BlueprintNativeEvent`
  (`FGBuildablePipeBase.h:121-122`) that `AFGBuildablePipeHyper` does not
  override (unlike `AFGBuildablePipeline`, `FGBuildablePipeline.h:62`) —
  only matters if something needs to construct connection components
  directly; the hologram handles this internally for normal placement.
- The `Holo_PipeHyper_C` Blueprint has its own `SimpleConstructionScript`/
  graph that wasn't decompiled — there could be BP-level placement
  behavior beyond the four CDO overrides above (not observed to matter in
  the live test, but not ruled out for other geometries either).
