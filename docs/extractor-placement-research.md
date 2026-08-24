# Extractor-on-node placement research — narrowing Phase 13

Recorded 2026-08-24. Read-only research, no code changes. Narrows PLAN.md
Phase 13 ("Building Placement") to a specific, much smaller case: placing
a resource extractor (Miner Mk1-3, Water Extractor, Oil Extractor)
directly onto a specific, already-known `AFGResourceNodeBase` actor — not
general freeform building placement. Builds on
[building-placement-research.md](building-placement-research.md) and
[resource-node-research.md](resource-node-research.md); the same
auto-generated-stub-`.cpp` caveat applies (every claim below is a header
declaration fact unless marked as inference — none of this is verifiable
against real runtime logic from source alone). Load-bearing claims were
re-verified directly, not just trusted from delegated research — noted
where relevant.

## 1. Extractor buildable and hologram classes

Hierarchy: `AFGBuildableResourceExtractor`
(`Source/FactoryGame/Public/Buildables/FGBuildableResourceExtractor.h:13`,
`Abstract`) → `AFGBuildableResourceExtractorBase`
(`FGBuildableResourceExtractorBase.h:65`, `Abstract`, doc: *"The base
class for all resource extractors, i.e. miners and pumps"*) →
`AFGBuildableFactory` → ... → `AFGBuildable`. Miner Mk1-3/Water
Extractor/Oil Extractor all instantiate the non-abstract
`AFGBuildableResourceExtractor` (form-gated via `mAllowedResourceForms`).
`AFGBuildableFrackingExtractor` is a separate class for fracking
satellites — out of scope here.

**Dedicated hologram, confirmed directly:**
`AFGResourceExtractorHologram`
(`Source/FactoryGame/Public/Hologram/FGResourceExtractorHologram.h:14`),
doc comment: *"Hologram for buildings that can only be placed (snapped)
on resource nodes."* Hierarchy: `AFGResourceExtractorHologram` →
`AFGFactoryHologram` → `AFGBuildableHologram` (the general hierarchy from
`building-placement-research.md` §1). No separate `GetHologramClass`
override was found on the extractor buildable — linkage is the standard
descriptor/`AFGBuildable::mHologramClass` mechanism already documented.

## 2. Associating the hologram with a specific, already-known node

**No direct "set target node" setter exists on the hologram — everything
flows through `FHitResult`.** Confirmed directly:

```cpp
// FGResourceExtractorHologram.h
virtual bool TrySnapToActor( const FHitResult& hitResult ) override;              // :27
virtual void SetHologramLocationAndRotation( const FHitResult& hitResult ) override; // :29
virtual bool IsValidHitResult( const FHitResult& hitResult ) const override;      // :31
virtual void TrySnapToExtractableResource( const FHitResult& hitResult, FVector& newHitLocation ); // :50
TScriptInterface<class IFGExtractableResourceInterface> mSnappedExtractableResource; // :64, protected
```

`mSnappedExtractableResource` — the field holding "the node we snapped
to" — is `protected`, with no public getter/setter. Every entry point
that populates it takes an `FHitResult`, none take an
`AFGResourceNodeBase*`/`IFGExtractableResourceInterface` directly.

**Base `AFGHologram` confirmed to have no per-tick camera dependency**:
`void UpdateHologramPlacement(const FHitResult& hitResult)`
(`FGHologram.h:144`) is the single external trigger point — no `Tick`
override exists on the class. This means placement updates are
event-driven (something calls `UpdateHologramPlacement` with a hit
result), not continuously polled from a possessed pawn's camera every
frame.

**Practical implication:** a caller with a known `AFGResourceNodeBase*`
(no live player camera trace) would need to construct a *synthetic*
`FHitResult` — its `Location`, `HitObjectHandle`/`Component` referencing
the known node actor — and feed it through
`UpdateHologramPlacement`/`SetHologramLocationAndRotation`. This is
architecturally plausible from the header shapes (nothing requires a
*real* trace, just a well-formed `FHitResult`), but **unverified against
actual `.cpp` snapping logic** — whether the resulting hologram state is
fully correct this way needs runtime confirmation, not just header
reading.

The buildable side, by contrast, has direct setters usable *after*
construction — see §4.

## 3. Extractor-specific disqualifiers

Confirmed directly in `Source/FactoryGame/Public/FGConstructDisqualifier.h`:

- `UFGCDNeedsResourceNode` (`:249`) — must be placed on a resource node.
- `UFGCDResourceNodeIsOccuped` (`:315`) — node already occupied (matches
  `AFGResourceNodeBase::IsOccupied()` from prior research).
- `UFGCDResourceIsTooShallow` (`:326`) — liquid/fracking-oriented depth
  check.
- `UFGCDResourceDeposit` (`:348`).

Additionally,
`AFGBuildableResourceExtractorBase::mMustPlaceOnResourceDisqualifier`
(`FGBuildableResourceExtractorBase.h:159`, confirmed directly, doc:
*"used when this resource extractor is not placed on a matching resource
node (used in the hologram)"*) is a **per-buildable-class configurable**
disqualifier slot, separate from the fixed list above. Combined with
`CanOccupyResource`/`IsAllowedOnResource` (declared on both the hologram,
`:53,56`, and the buildable, `FGBuildableResourceExtractorBase.h:123,126`),
this is architecturally where purity/resource-type/tier mismatch
(e.g. a Miner on an oil node) is most likely enforced — **no dedicated
"wrong purity" or "wrong tier" disqualifier class name was found**; not
found, don't assume one exists.

## 4. Post-`Construct()` wiring to the node

`AFGResourceExtractorHologram::ConfigureActor(AFGBuildable* inBuildable) const`
is overridden (`:42`) — the standard `AFGBuildableHologram` configure
step already documented in `building-placement-research.md` §4. Given
`mSnappedExtractableResource` (§2) plus the buildable's own setters,
confirmed directly:

```cpp
// FGBuildableResourceExtractorBase.h
void SetExtractableResource( TScriptInterface<IFGExtractableResourceInterface> extractableInterface ); // :106
void SetResourceNode( class AFGResourceNode* resourceNode ); // :109
friend class AFGResourceExtractorHologram; // :138 - confirms the hologram needs private/protected access
```

This `ConfigureActor` override is architecturally where wiring the
constructed extractor to the snapped node would happen — **inferred from
header shape, not confirmed** (stub `.cpp`). No evidence that plain
`AFGHologram::Construct()` alone (without a working `ConfigureActor`
override) wires occupancy automatically — `SetIsOccupied`/
`CanBecomeOccupied` live on `IFGExtractableResourceInterface`
(`FGExtractableResourceInterface.h:25,33`) and aren't called from any
base hologram code found.

## 5. Is a real, camera-tracing pawn required?

**Confirmed not required at the API level.** Directly verified:

```cpp
// FGHologram.h
void SetConstructionInstigator( APawn* instigator ) { mConstructionInstigator = instigator; } // :113, public setter
FORCEINLINE APawn* GetConstructionInstigator() const { return mConstructionInstigator; }        // :114
```

No per-tick camera polling was found inside `AFGHologram` (no `Tick`
override); placement only updates via the explicit
`UpdateHologramPlacement(FHitResult)` call (§2). This supports the
server-side-driven flow this research was scoped to answer: spawn the
hologram, `SetConstructionInstigator(pawn)`, feed a synthetic
`FHitResult`/placement (§2), call `CanConstruct()`, then `Construct()` —
bypassing `AFGBuildGun`/`UFGBuildGunStateBuild` entirely. Plausible from
header shapes; **still unverified against actual `.cpp` logic** per the
standing caveat — this is the single most important thing to confirm
before writing real construction code, since it's the crux of whether
"place an extractor from a chat prompt" is even the right shape of
solution.

## 6. No simpler existing shortcut

`Source/FactoryGame/Public/FGCheatManager.h:864,866` (confirmed directly):

```cpp
void SpawnBuildableBlockAtPlayerLocation( TSubclassOf<class UFGRecipe> buildableRecipe, int32 blockSize );
void Server_SpawnBuildableBlockAtPlayerLocation( TSubclassOf<class UFGRecipe> buildableRecipe, int32 blockSize );
```

Doc comment: *"Spawns a cube of building of the same type centered at the
player location."* Recipe-driven and player-location-centered, **not**
resource-node-aware and can't target a specific known actor — not usable
as a shortcut for this case. No other instant-build/give-extractor/
node-targeted spawn function exists in this header.

## 7. SML

Confirmed: zero matches searching `Mods/SML/Source/SML` for
`Extractor|Miner|Hologram`. Same pattern as every prior research pass —
nothing to build on there.

## What this means for implementation

The path is architecturally coherent (spawn hologram → set instigator →
synthetic hit result → validate → construct → rely on `ConfigureActor`
for node wiring), and every step is backed by a real, verified header
declaration. But **the two riskiest unknowns — whether a synthetic
`FHitResult` actually produces correct snapping behavior (§2), and
whether hologram construction genuinely works without a real
`AFGBuildGun` in the loop (§5) — are both explicitly unverified against
runtime behavior**, because the `.cpp` bodies that would prove it are
stubs in this repo. Before writing real construction code, the
responsible next step is a small, isolated runtime experiment (e.g. a
debug console command that tries exactly this flow and logs each step's
result) rather than committing to the full validated RPC method first —
this is exactly the kind of case CLAUDE.md's "make a small test, compile,
verify" guidance is for, rather than building the complete feature in one
pass.

## Implementation status (2026-08-24)

The recommended small experiment above is now written:
`UDocModFunctionLibrary::GetTargetedResourceNode` and
`DebugCheckExtractorPlacementOnTargetedNode`
(`Mods/GameFeatures/DocMod/Source/DocMod/{Public,Private}/DocModFunctionLibrary.{h,cpp}`,
commit `796a74818a`), console commands `DocMod.TargetNode` /
`DocMod.TestExtractorPlacement`.

**`GetTargetedResourceNode` correction, found live (2026-08-24):** the
first version used a hand-rolled view-angle-cone + distance heuristic,
based on a research gap - this document's original research (before this
correction) searched for `IFGUsableInterface` and found zero matches,
concluding resource nodes have no usable-interface support the way
manufacturers do. First live test proved this wrong in the most direct
way possible: while clearly aiming at a Limestone node with the game's
own "Press E to start mining Limestone (Pure)" prompt on screen, the
heuristic reported nothing targeted. Real cause: FactoryGame spells it
`IFGUseableInterface`, and `AFGResourceNodeBase`
(`FGResourceNodeBase.h:93`) does implement it - confirmed live, the same
`AFGCharacterPlayer::GetBestUsableActor()` `GetTargetedManufacturer`
already trusts also correctly finds resource nodes. The heuristic was
deleted entirely and replaced with the same `GetBestUsableActor()`
pattern (commit `be67de68d4`). **Lesson for future header searches in
this repo: FactoryGame is not consistent about "Usable" vs "Useable"
spelling - grep both when a class doesn't turn up an expected interface.**

One correction to §2 above found while implementing: `AFGHologram` does
expose one real "spawn correctly" API this research pass missed —
`static AFGHologram* SpawnHologramFromRecipe(TSubclassOf<UFGRecipe>
inRecipe, AActor* hologramOwner, const FVector& spawnLocation, APawn*
hologramInstigator, ...)` (`FGHologram.h:95`) — it resolves the
descriptor's hologram class and spawns it correctly, removing one
smaller unknown (§2's "how do I even construct the right hologram
instance" question is answered; the two bigger unknowns in this
section's opening paragraph are unchanged and still what the experiment
exists to test). Takes a **recipe**, not a buildable/descriptor class
directly — used `Recipe_MinerMk1` (verified as a real asset in
`Content/FactoryGame/Recipes/Buildings/`, the building's *build-cost*
recipe, not a production recipe — Miner Mk1 has none).

`GetConstructDisqualifiers`/`UFGConstructDisqualifier::GetDisqualifyingText`/
`GetIsSoftDisqualifier` (`FGConstructDisqualifier.h:31,35`) turn
`CanConstruct()`'s bool into the actual class-based disqualifier list
with human-readable text — used to log *why* placement would fail, not
just whether.

**Deliberately stops at `CanConstruct()`.** `Construct()` is never
called — the function destroys the scratch hologram actor on every exit
path (`ON_SCOPE_EXIT`). This can't place a real building or touch the
save, so it carries a read-only risk profile despite spawning a real
(temporary) `AFGHologram` actor. First live run is what actually answers
whether the synthetic-hit-result-snapping and buildgun-free-construction
assumptions hold — see docs/manual-verification.md's new pending item.
