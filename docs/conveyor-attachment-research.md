# Conveyor splitter/merger research — 2026-08-25

Recorded in response to the user asking, at the end of a long session, to
"research, verify or build support for using conveyor splitters and mergers
later." Investigated via source research (see citations below), then
verified the key finding by adding read-only telemetry and compiling
(construction itself needed no new code - see "Bottom line").

## Real classes

All conveyor attachments derive from `AFGBuildable` directly (same base as
belts), NOT from `AFGBuildableFactory`:

- `AFGBuildableConveyorAttachment` — abstract base,
  `Source/FactoryGame/Public/Buildables/FGBuildableConveyorAttachment.h:108`.
  Caches `TArray<UFGFactoryConnectionComponent*> mInputs` (`:154`) and
  `mOutputs` (`:157`), plus a buffer `UFGInventoryComponent* mBufferInventory`
  (`:147`).
- `AFGBuildableAttachmentSplitter : AFGBuildableConveyorAttachment` —
  `FGBuildableAttachmentSplitter.h:14`, doc comment (`:11`): *"Base class
  for splitters, one input and multiple outputs."*
- `AFGBuildableAttachmentMerger : AFGBuildableConveyorAttachment` —
  `FGBuildableAttachmentMerger.h:14`, doc comment (`:11`): *"Base class for
  mergers, it can have multiple inputs, but only one output."*
- `AFGBuildableMergerPriority : AFGBuildableConveyorAttachment` —
  `FGBuildableMergerPriority.h:14` (Priority Merger variant - inputs are
  drained in a configured priority order, not evenly).
- `AFGBuildableSplitterSmart : AFGBuildableConveyorAttachment` —
  `FGBuildableSplitterSmart.h:64` — used by BOTH the Smart Splitter and the
  Programmable Splitter (same native class, different recipe/Blueprint).
  Holds `TArray<FSplitterSortRule> mSortRules` (item→output-index rules,
  `:149`) and `mMaxNumSortRules` (`:153`), with public
  `AddSortRule`/`RemoveSortRuleAt`/`SetSortRuleAt`/`GetSortRules` UFUNCTIONs
  (`:104-119`) and an `OnSortRulesChangedDelegate`.

No fixed input/output *count* is declared as a literal constant in these
headers — populated generically into `mInputs`/`mOutputs` from real
`UFGFactoryConnectionComponent`s, same mechanism `world.connections`
telemetry itself already scans (see "Telemetry bugs found & fixed" in
project memory — conveyor attachments were the "third sibling hierarchy"
that motivated making that scan generic in the first place).

## Hologram — simple, NOT spline-based (the key finding)

All conveyor attachments share one Blueprint hologram,
`Holo_ConveyorAttachment_C`
(`Content/FactoryGame/Buildable/Factory/-Shared/Holo_ConveyorAttachment.uasset`),
whose native parent is `AFGConveyorAttachmentHologram`
(`Source/FactoryGame/Public/Hologram/FGConveyorAttachmentHologram.h:14`):

```
AFGConveyorAttachmentHologram : AFGFactoryHologram : AFGBuildableHologram : AFGHologram
```

(`FGFactoryHologram.h:13`, `FGBuildableHologram.h:125`).

This is the **same simple-hologram branch already proven for Miners,
Smelters, and Constructors** via `ConstructBuildingAtPosition`. It is
*not* the spline branch belts/pipes needed special multi-click
`TrySnapToActor`/`DoMultiStepPlacement` driving for:

```
AFGConveyorBeltHologram : AFGSplineHologram : AFGBuildableHologram   (FGConveyorBeltHologram.h:15)
AFGPipelineHologram     : AFGSplineHologram : AFGBuildableHologram   (FGPipelineHologram.h:16)
```

(`FGSplineHologram.h:23`). `AFGConveyorAttachmentHologram` does override
`TrySnapToActor`/`SnapToConnection` (`FGConveyorAttachmentHologram.h:28,53`)
— but that's a single-step placement refinement (snapping the attachment's
height/rotation to a nearby belt when the player is aiming at one, a UX
convenience), not multi-click spline placement. The `.cpp` bodies in this
SDK are stub/empty (headers-only SDK, same as every other engine class
this project has had to research from headers alone), so the exact snap
refinement can't be traced further — but the class hierarchy is
conclusive: single-step placement.

## Real recipe/buildable asset paths (confirmed on disk)

| Recipe (`Content/FactoryGame/Recipes/Buildings/`) | Buildable class |
|---|---|
| `Recipe_ConveyorAttachmentSplitter.uasset` | `AFGBuildableAttachmentSplitter` |
| `Recipe_ConveyorAttachmentSplitterSmart.uasset` | `AFGBuildableSplitterSmart` |
| `Recipe_ConveyorAttachmentSplitterProgrammable.uasset` | `AFGBuildableSplitterSmart` (same native class as Smart) |
| `Recipe_ConveyorAttachmentMerger.uasset` | `AFGBuildableAttachmentMerger` |
| `Recipe_ConveyorAttachmentMergerPriority.uasset` | `AFGBuildableMergerPriority` |

Lift variants also exist on disk (`Recipe_ConveyorAttachmentSplitterLift`,
`...SplitterProgrammableLift`, `...SplitterSmartLift`,
`Recipe_ConveyorAttachmentMergerLift`, `...MergerPriorityLift`) — not
included in `LogConveyorAttachmentCatalogAsJson`'s catalog for now (same
five ground-mounted variants only, mirroring how belt/pipe tier catalogs
started with the base set). No "Smart Merger" exists — only plain Merger
and Priority Merger.

## Connection counts and sort rules

`LogConveyorAttachmentCatalogAsJson` (`world.conveyorAttachments`) reports
real `inputCount`/`outputCount` per variant, read generically via
`GetDirection()` on each buildable class CDO's `UFGFactoryConnectionComponent`s
— not hardcoded to the commonly-known 1-in/3-out (splitter) / 3-in/1-out
(merger) figures, consistent with this project's "query real data, don't
assume" practice. It also reports `supportsSortRules` (true only for
Smart/Programmable splitters, both backed by `AFGBuildableSplitterSmart`).

## Bottom line: what needed new code, what didn't

**Placement and connection needed ZERO new construction code.** Splitters
and mergers use the same simple `AFGBuildableHologram` lineage already
proven for Miners/Smelters/Constructors, so:

- `world.placeBuilding` (`ConstructBuildingAtPosition`) already places
  them, given any of the five real recipe paths above.
- `world.connectConveyor` (`ConstructConveyorBelt`) already connects belts
  to/from them — its `FindFreeFactoryConnection` helper does a generic
  `GetComponents<UFGFactoryConnectionComponent>()` scan by direction, not
  hardcoded to any specific building class, so it will correctly find
  whichever of a splitter's 3 outputs (or a merger's 3 inputs) is still
  free on each successive `world.connectConveyor` call.

Deliberately did **not** add a `ConstructSplitter`/`ConstructMerger`
wrapper function — it would just be a thin, unnecessary duplicate of
`ConstructBuildingAtPosition`.

**What genuinely IS a gap, not yet built:** Smart/Programmable splitter
per-output item-type routing. `AFGBuildableSplitterSmart::AddSortRule()`
and friends are real, public, but AIMod has no RPC method calling them
yet — a placed Smart/Programmable splitter can be connected exactly like
a plain one, but cannot yet be configured to route by item type. This
would need its own future write operation (e.g. `world.setSplitterSortRule`)
if/when needed — flagged via `supportsSortRules` in the telemetry so an
agent doesn't assume otherwise.

## Live verification

NOT YET LIVE-TESTED as of this doc's initial commit — the C++ above
compiles clean; the next live session should place a real Splitter near
the existing demo site, connect two of its three outputs to two
temporary Constructors (or similar), and confirm via `world.connections`
that `FindFreeFactoryConnection` really does hand out a *different* free
output on each successive `world.connectConveyor` call rather than
re-using the same one. Low risk given the placement/connection mechanism
itself is identical to the already-proven Miner/Smelter/Constructor path.
