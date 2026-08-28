# Mod Rename Migration: DocMod → AIMod

Prep/research for renaming the mod ahead of matching the GitHub project name.
This is a checklist for when the fresh SML-generated template exists in the
editor — nothing here has been executed yet.

## Naming decision

Recommend **`AIMod`** as the plugin/module/ModReference identifier, not
`SatisfactoryAIMod`.

Evidence from the existing repo:

- `ExampleMod` (the SML starter's own sample mod) uses a bare `<Name>Mod`
  pattern with no `Satisfactory` prefix — the game name is redundant inside
  a Satisfactory-only modding ecosystem.
- SML's `ModReference` (used by `ModLoadingLibrary`, `WorldModuleManager`,
  `ChatCommandLibrary`, save-tagged mod data, and Alpakit's deployment
  folder name) is **not** a separate JSON field in `.uplugin` — it's derived
  from the plugin's own name (the `.uplugin` filename / folder name), which
  is exactly what SML's `FModInfo.Name` reads.
- `.uplugin` has a *separate* `FriendlyName` field, decoupled from that
  identifier — confirmed by `ExampleMod.uplugin`: identifier `ExampleMod`,
  `FriendlyName: "Example Mod"`. So the GitHub repo can stay named
  `SatisfactoryAIMod` (or whatever) without forcing that string into every
  C++ symbol; the in-engine `FriendlyName` can independently be something
  descriptive like `"Satisfactory AI Interface"`.

If you'd rather keep the longer name for the ModReference itself, everything
below still applies — just substitute `SatisfactoryAIMod` for `AIMod`
throughout.

## Why this has to be one atomic pass

Unreal Build Tool requires `Source/<ModuleName>/<ModuleName>.Build.cs` to
physically match the module name declared in `.uplugin`'s `Modules[].Name`.
The directory move, the `.uplugin` edit, the `Build.cs` rename, and the
`IMPLEMENT_MODULE` macro all have to land together — there's no working
intermediate state with old files + new identifiers or vice versa.

## Concrete file/symbol inventory (current DocMod state)

### Directory + plugin identity
- `Mods/GameFeatures/DocMod/` → `Mods/GameFeatures/AIMod/`
- `DocMod.uplugin` → `AIMod.uplugin`
  - `"Modules": [{"Name": "DocMod", ...}]` → `"Name": "AIMod"`
  - `"FriendlyName": "DocMod"` → something descriptive (decoupled, see above)
- `Config/DefaultEngine.ini` (mod-local) — just has comments referencing
  `DocMod`/`DocModHttpServerSubsystem` paths, update for accuracy.

### Source/DocMod/ → Source/AIMod/
- `DocMod.Build.cs` → `AIMod.Build.cs`, `public class DocMod : ModuleRules` → `public class AIMod`
- `Public/DocMod.h` → `Public/AIMod.h`
  - `DECLARE_LOG_CATEGORY_EXTERN(LogDocModAI, ...)` → `LogAIModAI` (also
    update CLAUDE.md's "Preferred conceptual name: LogDocModAI" line)
  - `class FDocModModule` → `FAIModModule`
- `Private/DocMod.cpp` → `Private/AIMod.cpp`
  - `DEFINE_LOG_CATEGORY(LogDocModAI)`, `#define LOCTEXT_NAMESPACE "FDocModModule"`
  - `IMPLEMENT_MODULE(FDocModModule, DocMod)` → `IMPLEMENT_MODULE(FAIModModule, AIMod)`
  - ~14 console command names as string literals: `DocMod.SelfTest`,
    `DocMod.ResourceNodes`, `DocMod.Buildables`, `DocMod.Manufacturers`,
    `DocMod.Connections`, `DocMod.Target`, `DocMod.TargetNode`,
    `DocMod.TestExtractorPlacement`, `DocMod.TestExtractorPlacementViaBuildGun`,
    `DocMod.ConstructExtractorOnTargetedNode`, `DocMod.PlaceBuildingNearPlayer`,
    `DocMod.TestPowerConnection` — rename to `AIMod.*` for consistency (these
    are just console-command strings, no compatibility constraint).
  - All log/output strings referencing "DocMod" by name (cosmetic, but ~30
    occurrences in this file alone).
- Remaining paired `.h`/`.cpp` files, each with a `DocMod`-prefixed class:
  - `DocModChatCommand.{h,cpp}` — chat command class
  - `DocModConfiguration.{h,cpp}` — `UDocModConfiguration : UModConfiguration`
  - `DocModDeveloperSettings.{h,cpp}`
  - `DocModFunctionLibrary.{h,cpp}` — `UDocModFunctionLibrary`, by far the
    largest file (all RPC-backing telemetry/construction functions)
  - `DocModHotkey.{h,cpp}`
  - `DocModHttpServerSubsystem.{h,cpp}` — `UDocModHttpServerSubsystem`
  - `DocModOperationTypes.h` — `FDocModOperationResult` and friends
  - `DocModSelfTest.{h,cpp}`
  - `DocModTelemetryTypes.h` — `FDocModResourceNodeTelemetry`,
    `FDocModManufacturerTelemetry`, etc.

  Each needs: filename rename, `#include "DocModX.h"` → `"AIModX.h"` at every
  call site, and the `U`/`F` class-prefix rename (`UDocMod*` → `UAIMod*`,
  `FDocMod*` → `FAIMod*`). This is the bulk of the work — grep for
  `\bDocMod` across `Source/DocMod/` to catch every symbol reference once the
  directory exists to work in.

### Documentation cleanup (string references only, no compile dependency)

User has confirmed commit history isn't a concern, so this is a full rewrite
of every `DocMod` mention to `AIMod` — not a "formerly DocMod" annotation or
a preserved historical note. Applies to prose, file paths, class names
mentioned in text, console-command names, and the log category name
(`LogDocModAI` → `LogAIModAI`) everywhere it's referenced in docs.

Complete list of files with confirmed `DocMod` references (from a repo-wide
grep, excluding `Intermediate`/`Binaries`/generated build output):

- `AGENTS.md`
- `CLAUDE.md` — includes the "Preferred conceptual name: LogDocModAI" line
  under Logging
- `PLAYBOOK.md`
- `RPC_REFERENCE.md`
- `README.md`
- `PLAN.md`
- `controller/README.md`
- `tests/README.md`
- `Config/DefaultEngine.ini` (root, comment lines only)
- `docs/blueprint-smoke-test.md`
- `docs/build.md`
- `docs/buildable-research.md`
- `docs/buildgun-driven-placement-research.md`
- `docs/building-placement-research.md`
- `docs/chat-and-console-commands.md`
- `docs/conveyor-attachment-research.md`
- `docs/conveyor-power-connection-research.md`
- `docs/current-environment.md`
- `docs/demo-production-chain.md`
- `docs/extractor-placement-research.md`
- `docs/factorygame-binary-provenance.md`
- `docs/hotkey.md`
- `docs/lightweight-buildable-research.md`
- `docs/manual-verification.md`
- `docs/networking-research.md`
- `docs/operations-protocol.md`
- `docs/placement-lessons.md`
- `docs/resource-well-research.md`
- `docs/self-test.md`
- `docs/telemetry-protocol.md`

Re-run `grep -ril DocMod` across the repo (excluding generated build output)
right before this step to catch anything added between now and the actual
rename — this list is a snapshot as of 2026-08-28, not guaranteed exhaustive
by then.

### Config/DefaultGame.ini and Config/DefaultEngine.ini (root)
Checked directly: `DefaultGame.ini` has **no** `DocMod` string references
(GameFeature activation isn't keyed by name there). `DefaultEngine.ini`
(root) only has comment-line references pointing at the mod's port-binding
config — update for accuracy, not load-bearing.

### Not code — things to decide about, not just rename
- **Alpakit deployment folder name** changes from `DocMod` to `AIMod` in the
  installed Satisfactory mods directory. For a solo dev setup this is a
  non-issue (just uninstall the old one), but worth remembering it's not a
  silent swap.
- **Existing save data**: if any world state ever got save-tagged under the
  `DocMod` ModReference (per CLAUDE.md's save-compatibility section), it
  would need explicit handling. Believed not applicable yet — this mod is
  read/telemetry + RPC-driven construction, not itself persisting SML mod
  config into saves beyond the standard `UConfigManager` settings, but worth
  a real check (`grep` the save-relevant SML save subsystem usage) before
  the actual rename if this becomes a concern.

## Suggested execution order (when you're in the editor)

1. Generate the fresh template with the new name via the SML tooling.
2. Copy `Source/DocMod/*` content into the new module's `Source/AIMod/`,
   renaming files and symbols together (this is where an editor-assisted
   rename or a scripted sed pass across the new directory helps).
3. Port `Config/DefaultEngine.ini`'s loopback-binding block into the new
   module's config.
4. Build, fix compile errors from any missed symbol.
5. Rewrite every file in the Documentation cleanup list above in the same
   commit — full replacement, not an annotated rename, per the user's
   confirmation that commit history preserves the old name if it's ever
   needed.
6. Verify Layer 2/3 per CLAUDE.md's testing strategy before considering it done.
