# Buildable / recipe / inventory / production API research (Phase 10)

Recorded 2026-08-24. Same evidence-based approach and the same caveat as
[resource-node-research.md](resource-node-research.md): every `.cpp` under
`Source/FactoryGame/Private/` in this repo is an auto-generated stub with
fake bodies (`return nullptr;` etc.) — only header declarations are
trustworthy; nothing about actual runtime behavior is verifiable from
source. Everything below needs runtime confirmation (see
[manual-verification.md](manual-verification.md)).

Load-bearing claims were re-verified directly (not solely trusted from
delegated research) before being used in code — noted inline.

## 1. Base buildable actor and enumeration

- **`AFGBuildable`** — `Source/FactoryGame/Public/Buildables/FGBuildable.h:167`,
  base `AActor`, `Abstract`.
- **`AFGBuildableSubsystem`** — `Source/FactoryGame/Public/FGBuildableSubsystem.h:246`,
  base `AFGSubsystem`. Unlike `AFGResourceNodeManager`, this one has a
  genuinely public, non-stubbed-looking accessor shape:
  - `static AFGBuildableSubsystem* Get(UObject* worldContext)` (`:276`) —
    confirmed directly.
  - `const TArray<AFGBuildable*>& GetAllBuildablesRef() const` (`:360`,
    `FORCEINLINE`) — confirmed directly, returns `mBuildables` by const
    reference. `FORCEINLINE` on the getter is a good sign it isn't doing
    anything exotic at call time — but its *population* (whether
    `mBuildables` actually gets filled as buildings are placed) is
    server-side runtime logic this repo's stubbed `.cpp` can't confirm.
  - `void GetTypedBuildable(TSubclassOf<AFGBuildable>, TArray<AFGBuildable*>&) const`
    and `int32 GetBuildableCount(TSubclassOf<AFGBuildable>) const` also
    exist as public/BlueprintCallable/BlueprintPure.
  - `.cpp` confirmed stubbed: `Source/FactoryGame/Private/FGBuildableSubsystem.cpp:90-91`
    (`Get()` → `return nullptr;`).

  **Implementation choice:** try `AFGBuildableSubsystem::Get()` +
  `GetAllBuildablesRef()` first; if the subsystem is null, fall back to
  `TActorIterator<AFGBuildable>`. This is a real improvement over the
  resource-node case (which had no usable manager at all) *if* the
  subsystem is actually populated at runtime — that's flagged as a
  specific thing to check in manual-verification.md.

- **Stable ID:** none. No GUID/PersistentId/StableId property on
  `AFGBuildable`. `FNetConstructionID mNetConstructionID`
  (`FGBuildable.h:1080-1081`) exists but is explicitly `transient`
  (`UPROPERTY(transient, replicated)`, doc: "Has not been assigned a value
  by server if 0") — not `SaveGame`, so it will not survive a save/reload.
  Same conclusion and same stopgap as resource nodes: session-local
  `GetPathName()`, pending Phase 7's real design.

## 2. Manufacturing buildings and recipes

- **`AFGBuildableManufacturer`** — `Source/FactoryGame/Public/Buildables/FGBuildableManufacturer.h:85`,
  base `AFGBuildableFactory` (`FGBuildableFactory.h:27`, itself
  `AFGBuildable`). Covers Constructor/Assembler/Manufacturer/Smelter/
  Refinery etc.
- Current recipe: `TSubclassOf<UFGRecipe> GetCurrentRecipe() const`
  (`FGBuildableManufacturer.h:165`, `FORCEINLINE`, backed by `mCurrentRecipe`)
  — confirmed directly.
- Set recipe: `void SetRecipe(TSubclassOf<UFGRecipe> recipe)` (`:174`).
  Doc comment (`:169-171`, confirmed directly): *"It is up to the caller
  to make sure input and output inventories are empty before changing
  recipe... @param recipe - The new recipe."* **No stated validation** that
  the recipe is actually valid for this building class. Not used yet
  (Phase 12) — noted here because Phase 12's `SetRecipe` RPC method will
  need to validate this itself (against `UFGRecipe::IsProducedIn`, below)
  rather than trusting the game to reject a bad recipe.
- Enumerate recipes valid for a *specific instance*:
  `void GetAvailableRecipes(TArray<TSubclassOf<UFGRecipe>>&) const`
  (`:149`) — "expensive, cache the result."
- Enumerate/validate recipes for a producer *class* (no live instance
  needed): `static bool UFGRecipe::IsProducedIn(TSubclassOf<UFGRecipe>
  inClass, TSubclassOf<UObject> inProducer)` (`FGRecipe.h:136`, confirmed
  directly) and `static TArray<TSubclassOf<UObject>> GetProducedIn(
  TSubclassOf<UFGRecipe> inClass)` (`:83`). No inverse "all recipes for
  this producer class" static was found - only per-recipe queries.
- Clock speed ("potential" is FactoryGame's actual internal name) — on
  the **base** `AFGBuildableFactory`, not the manufacturer:
  - `float GetCurrentPotential() const` (`FGBuildableFactory.h:225`,
    `FORCEINLINE`) — confirmed directly. The applied multiplier (1.0 =
    100%).
  - `float GetPendingPotential() const` (`:229`) — queued value, takes
    effect at next production cycle, not instantaneous.
  - `virtual void SetPendingPotential(float)` (`:233`) — the setter
    (Phase 12 candidate, not used yet).
  - `GetCurrentMinPotential()`/`GetMaxPotential()`/`GetCurrentMaxPotential()`/
    `GetCanChangePotential()` (`:237-249`) — range/eligibility.
- **`UFGRecipe`** — `FGRecipe.h:40`, base `UObject`, `Blueprintable`.
  - Name: `static FText GetRecipeName(TSubclassOf<UFGRecipe> inClass)`
    (`:55`, confirmed directly).
  - Products/ingredients: `static TArray<FItemAmount> GetProducts(...)`
    (`:63`) / `GetIngredients(...)` (`:59`) — not consumed in this pass;
    `Recipe` telemetry field is currently just the display name string.
    Revisit if a controller needs full ingredient/product lists.

## 3. Production status

All on `AFGBuildableFactory`:

- `virtual EProductionStatus GetProductionIndicatorStatus() const`
  (`FGBuildableFactory.h:189`) — confirmed directly. `EProductionStatus`
  is a **plain C++ `enum class : uint8`** (`FGBuildable.h:50-58`,
  confirmed directly — `IS_NONE`, `IS_PRODUCING`,
  `IS_PRODUCING_WITH_CRYSTAL`, `IS_STANDBY`, `IS_ERROR`, `IS_MAX`), **not**
  a `UENUM`, so there's no `UEnum::GetDisplayValueAsText` reflection path
  available — `DocModFunctionLibrary.cpp` converts it with a manual
  `switch`.
- `virtual float GetProductionProgress() const` (`:193`, overridden on
  the manufacturer, `FGBuildableManufacturer.h:138`) — "[0,1]".
- `virtual float GetProductivity() const` (`FGBuildableFactory.h:213`) —
  FactoryGame's own "how productive" measure, distinct from clock
  speed/potential.

## 4. Inventories

- **`UFGInventoryComponent`** — `Source/FactoryGame/Public/FGInventoryComponent.h:158`,
  base `UActorComponent`. Not a member of `AFGBuildable` itself — added
  per-subclass. On the manufacturer:
  `GetInputInventory()`/`GetOutputInventory()` (`FGBuildableManufacturer.h:157,161`,
  both `FORCEINLINE`, public, `BlueprintPure`) return
  `UFGInventoryComponent*`.
- Reading contents: `void GetInventoryStacks(TArray<FInventoryStack>&
  out_stacks, bool getEmptyStacks = false) const`
  (`FGInventoryComponent.h:451`) — confirmed directly (public, not the
  `protected` `AFGBuildableManufacturer::GetInputInventoryItems`/
  `GetOutputInventoryItems`, which despite similar names are NOT usable
  externally — verified directly, they're declared under a `protected:`
  section at `FGBuildableManufacturer.h:176,209,215`).
- **`FInventoryStack`** (`FGInventoryComponent.h:95-119`, confirmed
  directly): `FInventoryItem Item` (use `Item.GetItemClass()` for the
  item descriptor class) and `int32 NumItems`.

## 5. Existing SML code

Confirmed (via delegated search): zero matches for "Buildable" or
"Manufacturer" anywhere in `Mods/SML/Source/SML`. The only "Recipe" hits
are `AFGModContentRegistry`'s mod-load-time recipe *class* bookkeeping
(`RegisterRecipe`/`GetRegisteredRecipes`/`IsRecipeVanilla` in
`Mods/SML/Source/SML/Public/Registry/ModContentRegistry.h`) — a
different concern (which recipe classes mods registered) from reading a
live machine's active recipe. Nothing to build on for buildables,
manufacturers, runtime recipe state, or inventories.

## 6. Factory connection components (conveyor topology facts)

Found and verified directly (not part of the original delegated research
pass, needed later for Phase 10's "conveyor connection components" /
Phase 11's world graph):

- **`UFGFactoryConnectionComponent`** —
  `Source/FactoryGame/Public/FGFactoryConnectionComponent.h:41`, base
  `UFGConnectionComponent` (itself a plain `UActorComponent`, so
  `GetOwner()` returns the owning `AActor`).
  - `GetDirection() const` → `EFactoryConnectionDirection` (`:122`).
    `EFactoryConnectionDirection` **is** a real `UENUM(BlueprintType)`
    (`:27-34`, confirmed directly) — unlike `EProductionStatus` — with
    values `FCD_INPUT`, `FCD_OUTPUT`, `FCD_ANY`, `FCD_SNAP_ONLY`
    ("Special case for conveyor poles"), `FCD_MAX` (hidden). Mapped to
    strings manually in code anyway, for consistency with
    `ProductionStatusToString` and to avoid `StaticEnum<>()` boilerplate
    for four values.
  - `GetConnection() const` → `UFGFactoryConnectionComponent*`, the
    connected peer component, or `nullptr` if unconnected (`:96`).
  - `IsConnected() const` (`:111`).
- **`AFGBuildableFactory::GetConnectionComponents() const`** →
  `TArray<UFGFactoryConnectionComponent*>` — `Source/FactoryGame/Public/Buildables/FGBuildableFactory.h:78`,
  public, `BlueprintCallable`. This is the entry point: every
  `AFGBuildableFactory` (belts, splitters, mergers, and all machines -
  anything that moves items) exposes its connection points this way.

**Fact exposed, not a graph:** one row per connection point
(`FDocModFactoryConnectionTelemetry` — owner buildable id, direction,
connected bool, connected-to buildable id). A single physical belt/pipe
link between two buildings therefore produces **two** rows (an Output row
on the source, an Input row on the destination), each naming the other.
Building the actual graph from these rows is Phase 11's job, and PLAN.md
is explicit that belongs on the **external controller**, not the mod —
see `controller/satisfactory_ai/graph.py`.

## Summary for Phase 10 implementation

| Need | API | Citation |
|---|---|---|
| Enumerate buildables | `AFGBuildableSubsystem::Get()->GetAllBuildablesRef()`, fallback `TActorIterator<AFGBuildable>` | §1 |
| Current recipe | `GetCurrentRecipe()` → `UFGRecipe::GetRecipeName()` | `FGBuildableManufacturer.h:165`, `FGRecipe.h:55` |
| Clock speed | `GetCurrentPotential()` | `FGBuildableFactory.h:225` |
| Production status | `GetProductionIndicatorStatus()` (manual enum→string, not a UENUM) | `FGBuildableFactory.h:189`, `FGBuildable.h:50-58` |
| Production progress | `GetProductionProgress()` | `FGBuildableManufacturer.h:138` |
| Productivity | `GetProductivity()` | `FGBuildableFactory.h:213` |
| Input/output inventory | `GetInputInventory()`/`GetOutputInventory()` → `GetInventoryStacks()` | `FGBuildableManufacturer.h:157,161`, `FGInventoryComponent.h:451` |
| Stable ID | **Not available** — same Phase 7 stopgap as resource nodes | §1 |

## Correction found live (2026-08-24): conveyor belts/lifts are a separate hierarchy

The original §6 enumeration (`TActorIterator<AFGBuildableFactory>` +
`GetConnectionComponents()`) missed conveyor belts and lifts entirely.
Confirmed directly: `AFGBuildableConveyorBase` (base of
`AFGBuildableConveyorBelt` and `AFGBuildableConveyorLift`,
`Source/FactoryGame/Public/Buildables/FGBuildableConveyorBase.h:97`)
derives straight from `AFGBuildable` — **not** `AFGBuildableFactory` — so
the `TActorIterator<AFGBuildableFactory>` scan could never find them.
They expose their two connection points via named accessors instead of
an array: `GetConnection0()`/`GetConnection1()`
(`FGBuildableConveyorBase.h:163-164`, both `UFGFactoryConnectionComponent*`
— same component type as the factory side).

Found via live data, not static reading: a real save's
`FactoryConnectionTelemetry.reciprocity` self-test check failed
(435/1265 connection points unmatched). Pulling and analyzing the live
`world.connections` RPC data showed every single unmatched `"Output"` row
pointed at a `ConveyorBeltMk5`/`ConveyorBeltMk2` peer that had **zero**
connection rows of its own in the enumeration — i.e. belts were never
being visited at all, not a subtler direction-matching issue. Fixed by
adding a second `TActorIterator<AFGBuildableConveyorBase>` pass. See the
matching fix commit and `docs/manual-verification.md`'s Confirmed
section for the full incident writeup.

## Second correction found live (2026-08-24): conveyor attachments are a THIRD hierarchy — switched to generic discovery

The two-pass fix above (`AFGBuildableFactory` + `AFGBuildableConveyorBase`)
was still insufficient. Re-running the reciprocity check against a real
save showed 920/6791 unmatched — worse in absolute terms than before,
though the connection count grew from 1265→6791, confirming the belt fix
was working. Live analysis of `world.connections` showed every unmatched
peer's `buildableClass` was `Build_ConveyorAttachmentMerger` /
`Build_ConveyorAttachmentSplitter` (and the Lift/Smart variants).

Confirmed directly: `AFGBuildableConveyorAttachment`
(`Source/FactoryGame/Public/Buildables/FGBuildableConveyorAttachment.h`)
is a **third** sibling hierarchy deriving straight from `AFGBuildable` —
not `AFGBuildableFactory`, not `AFGBuildableConveyorBase`. Its connection
points live in protected `mInputs`/`mOutputs` arrays with **no public
getter at all**, so neither existing enumeration pass could ever reach
them.

Rather than add a third per-class pass (and risk missing a fourth
sibling class later), `CollectFactoryConnectionTelemetry` was redesigned
around generic component discovery: a single
`TActorIterator<AFGBuildable>` pass calling
`Buildable->GetComponents<UFGFactoryConnectionComponent>(Connections)`
(`AActor::GetComponents<T>()`, a standard Unreal component-lookup
template — works for any actor regardless of which `AFGBuildable`
subclass it is, since `UFGFactoryConnectionComponent` is always a real
`UActorComponent` attached to the owning actor even when the owning
class exposes no public getter for it). This is now the sole enumeration
path — the `AFGBuildableFactory`/`AFGBuildableConveyorBase`-specific
passes and the `GetConnectionComponents()`/`GetConnection0()`/
`GetConnection1()` accessors are no longer used by this code, though
they remain valid APIs for other purposes. See the matching fix commit
and `docs/manual-verification.md`'s Confirmed section for the full
incident writeup.
