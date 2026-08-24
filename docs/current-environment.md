# Current Environment (Phase 0 findings)

Recorded 2026-08-24. This documents what actually exists in the workspace, as
opposed to what PLAN.md assumes. No functional code was changed to produce
this document.

## Repository layout

```
F:\Claude\SatisfactoryModLoader\        (game project root, "FactoryGame")
    FactoryGame.uproject
    FactoryGame.sln                     (already generated)
    Source\                             (FactoryGame's own C++ modules)
        FactoryGame\
        FactoryEditor\
        DummyHeaders\
        FactoryPreEarlyLoadingScreen\
        FactoryDedicatedServer\
        FactoryDedicatedClient\
        FactoryClient.Target.cs
        FactoryEditor.Target.cs
        FactoryGame.Target.cs
        FactoryGameEGS.Target.cs
        FactoryGameSteam.Target.cs
        FactoryServer.Target.cs
        FactoryShared.Target.cs
    Mods\
        SML\                             (Satisfactory Mod Loader plugin, tracked)
        SMLEditor\                       (tracked)
        Alpakit\                         (packaging plugin, tracked)
        AccessTransformers\              (tracked)
        WwisePatches\                    (tracked)
        GameFeatures\
            ExampleMod\                  (tracked, reference implementation)
            DocMod\                      (our mod — NOT tracked by git, see below)
    Config\
    Plugins\                             (engine/third-party plugins, e.g. Wwise)
```

The engine is a custom Coffee Stain build, resolved via the Windows registry
(`HKCU\SOFTWARE\Epic Games\Unreal Engine\Builds`) under the key
`5.6.1-CSS` → `F:/Claude/Unreal Engine - CSS`. This matches
`FactoryGame.uproject`'s `"EngineAssociation": "5.6.1-CSS"`. Standard UBT
batch files exist there (`Engine\Build\BatchFiles\Build.bat`, `RunUAT.bat`,
etc.), so no engine-path guessing is needed for Phase 1 — `Build.bat` is the
known entry point.

`FactoryGame.sln` already exists at the repo root, so Visual Studio project
files have already been generated at least once; regeneration is not
required to build from the command line via `Build.bat`.

## DocMod is currently untracked by git

Root `.gitignore` contains:

```
# Ignore all non-Starter Project mods
Mods/*
...
!Mods/Alpakit/
!Mods/AccessTransformers/
!Mods/WwisePatches/
!Mods/SML/
!Mods/SMLEditor/
!Mods/GameFeatures/ExampleMod/
```

`Mods/GameFeatures/DocMod/` is **not** in the allow-list, so every file under
it (including hand-written `Source/` C++) is currently untracked and
`git status` shows nothing for it. `git log` for any DocMod path returns no
history. This is a pre-existing condition, not something introduced this
session — worth flagging to the user before any future commit, since work
on DocMod is presently invisible to git and unprotected by version control.
No .gitignore change was made in Phase 0.

Other pre-existing uncommitted changes (unrelated to DocMod, not touched):
`Config/DefaultGame.ini` and `Mods/SML/SML.uplugin`/`PluginLocalization.ini`
show trivial UTF-8 BOM-removal diffs, plus `Mods/GameFeatures/ExampleMod` and
`Mods/SMLEditor` localization file changes. Left as-is.

## DocMod plugin identity

`Mods/GameFeatures/DocMod/DocMod.uplugin`:

- `FriendlyName`: `DocMod`
- `Version` / `VersionName`: `1.0.0`
- `GameVersion`: `>=502094`
- `CanContainContent`: `true`
- `BuiltInInitialFeatureState`: `Active` (Game Feature is active by default)
- Depends on plugin `SML` (`^3.12.0`, semver-compatible range)
- Declares one module:
  - `Name`: `DocMod`
  - `Type`: `Runtime`
  - `LoadingPhase`: `Default`

`Content/DocMod.uasset` is an `FGGameFeatureData` asset named `DocMod`
(package `/DocMod/DocMod`), referencing primary asset scan directories
`/DocMod/Schematics`, `/DocMod/Schematics/Research`, `/DocMod/Settings`.
Confirms the mod is wired up as a proper Satisfactory Game Feature, not a
bare Unreal plugin.

Installed `SML.uplugin` (`Mods/SML/SML.uplugin`) reports `SemVersion: 3.12.0`,
`GameVersion: >=502094` — matches the `>=502094` the DocMod project also
targets, and matches the latest commit message ("Update headers to
CL502094"). SML's own module (`SML`, `Runtime`, `PostDefault`) loads after
default-phase modules, i.e. after `DocMod`'s module.

Existing Content assets in DocMod: `Recipes/Recipe_DocRecipe.uasset`,
`Schematics/Schematic_DocSchem.uasset`, `Schematics/Icon_SchemDoc.uasset`,
plus the root `RootGameWorld_DocMod.uasset` level. These are the
template-generated Blueprint/content pieces referenced in CLAUDE.md as
"Blueprint example mod" — not modified.

`Config/AccessTransformers.ini` is present but empty (`[AccessTransformers]`
section with no entries) — no access transformers currently applied.
`Config/PluginSettings.ini` only sets `+AdditionalNonUSFDirectories=Resources`
for staging.

Unlike `ExampleMod` (which has `Config/Alpakit.ini`), **DocMod has no
`Config/Alpakit.ini`**. Alpakit packaging settings for DocMod are presumably
using defaults or are configured only inside the Unreal Editor UI (Alpakit
settings can be stored outside the ini if never customized) — this needs
confirming in the editor before Phase where packaging is exercised, but is
out of scope for Phase 0.

## C++ module: `DocMod`

Files (all present, all template-generated, none hand-modified yet):

- `Source/DocMod/DocMod.Build.cs`
- `Source/DocMod/Public/DocMod.h`
- `Source/DocMod/Private/DocMod.cpp`

### Module class and macro

`DocMod.h` declares:

```cpp
class FDocModModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
```

`DocMod.cpp` implements empty `StartupModule`/`ShutdownModule` bodies (just
comments, no logging yet — Phase 2 work) and registers the module with:

```cpp
IMPLEMENT_MODULE(FDocModModule, DocMod)
```

No custom log category exists yet (`LogDocModAI` from CLAUDE.md/PLAN.md is
not yet defined anywhere in the module). No Blueprint-callable functions
exist yet. This is a completely stock Epic-generated module skeleton —
matches the "initial generated C++ template" note in CLAUDE.md.

### Build.cs dependencies

`DocMod.Build.cs` (module rules class `DocMod : ModuleRules`):

- `PCHUsage`: `UseExplicitOrSharedPCHs`
- `CppStandard`: `Cpp20`
- Large `PublicDependencyModuleNames` block of "FactoryGame transitive
  dependencies" (mostly commented out, template boilerplate): active ones
  are `Core`, `CoreUObject`, `Engine`, `DeveloperSettings`, `PhysicsCore`,
  `InputCore`, `GeometryCollectionEngine`, `AnimGraphRuntime`,
  `AssetRegistry`, `NavigationSystem`, `AIModule`, `GameplayTasks`,
  `SlateCore`, `Slate`, `UMG`, `RenderCore`, `CinematicCamera`, `Foliage`,
  `NetCore`, `GameplayTags`, `Json`, `JsonUtilities`.
- `DummyHeaders` (project-level stub-header module at `Source/DummyHeaders/`)
  is a public dependency — this is what lets the mod compile against
  FactoryGame headers that are stubbed/unavailable in some contexts.
- If `Target.Type == Editor`: adds `AnimGraph` (commented-out
  `OnlineBlueprintSupport` alongside it).
- Unconditionally depends on `FactoryGame` and `SML` modules (added as a
  second `PublicDependencyModuleNames.AddRange` call after the editor-only
  block).
- `Json`/`JsonUtilities` are **already present** as dependencies — relevant
  to PLAN.md Phase 6 (JSON serialization): no Build.cs change will be needed
  to reach for `FJsonObject`/`FJsonSerializer` when that phase starts.
- All `PublicIncludePaths`/`PrivateIncludePaths`/extra
  `PublicDependencyModuleNames`/`PrivateDependencyModuleNames`/
  `DynamicallyLoadedModuleNames` blocks are empty template placeholders.

This is the standard SML "Starter Project" mod template — nothing here is
DocMod-specific beyond the name.

### Confirmed prior successful build

`Mods/GameFeatures/DocMod/Binaries/Win64/` already contains
`UnrealEditor-DocMod.dll` and `.pdb`, and
`Intermediate/Build/Win64/x64/UnrealEditor/Development/DocMod/` contains a
full set of UBT intermediates (`Module.DocMod.cpp.obj`, `.dep.json`,
`LiveCodingInfo.json`, etc.) for target `UnrealEditor`, platform `Win64`,
configuration `Development`. This corroborates CLAUDE.md's claim that a
successful `Development Editor` compile has already occurred for this exact
module — Phase 1 does not need to prove first-time buildability, only make
the existing build reproducible from the CLI.

## FactoryGame target configuration

`FactoryGame.uproject` declares five build targets via the standard
`*.Target.cs` files in `Source/`. The one Phase 1 cares about is
`FactoryEditor.Target.cs`:

```cpp
public class FactoryEditorTarget : TargetRules
{
    public FactoryEditorTarget( TargetInfo Target ) : base(Target)
    {
        Type = TargetType.Editor;
        bOverrideBuildEnvironment = true;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        WindowsPlatform.bEnableRayTracing = true;
        DefaultBuildSettings = BuildSettingsVersion.Latest;

        ExtraModuleNames.AddRange(new string[] {
            "FactoryPreEarlyLoadingScreen",
            "FactoryGame",
            "FactoryEditor",
            "FactoryDedicatedServer",
            "FactoryDedicatedClient"
        });
    }
}
```

So the actual UBT target name to build is **`FactoryEditor`**, platform
`Win64`, configuration `Development` (Unreal's "Development Editor" is
`-Target=FactoryEditor Win64 Development`). `DocMod` is not itself a UBT
target — it is a plugin module that gets pulled in automatically because
it's an enabled Game Feature plugin under `Mods/GameFeatures/`, discovered
by the project's plugin search (Mods-as-plugins is an SML Starter Project
convention, not something configured per-module in the Target.cs files).

`FactoryGame.uproject` module list separately (for reference, not itself a
build target) lists `FactoryGame`, `FactoryEditor`,
`FactoryPreEarlyLoadingScreen`, `FactoryDedicatedServer`,
`FactoryDedicatedClient`, `DummyHeaders`, with additional engine-module
dependencies declared per-module (`AdditionalDependencies`).

## Build tooling available

- `F:\Claude\Unreal Engine - CSS\Engine\Build\BatchFiles\Build.bat` — the
  standard UBT invocation entry point. No custom/wrapper build script exists
  in this repo yet (no `tools/` directory, no `*.bat`/`*.ps1` at repo root
  for building). Phase 1's `tools/build-editor.ps1` will be new, not a
  replacement for something already there.
- `RunUAT.bat`, `Rebuild.bat`, `Clean.bat`, `GetMSBuildPath.bat`,
  `GetDotnetPath.bat` also present alongside `Build.bat`.
- No `GenerateProjectFiles.bat`/`.sh` was found at the repo root (it lives
  under the engine's `Engine\Build\BatchFiles\` as usual) — not needed since
  `FactoryGame.sln` is already generated.
- No existing `docs/` or `tools/` directories prior to this task — both are
  created fresh as PLAN.md expects.

## Differences between the repository and PLAN.md/CLAUDE.md assumptions

1. **DocMod is not under version control.** PLAN.md Phase 0 asks to
   "determine whether the project is already under Git" — the project root
   is a git repo, but the DocMod mod directory specifically is excluded by
   `.gitignore`'s `Mods/*` blanket rule (only `ExampleMod`, `SML`,
   `SMLEditor`, `Alpakit`, `AccessTransformers`, `WwisePatches` are
   allow-listed). Nothing in CLAUDE.md or PLAN.md flags this. Recommend
   deciding explicitly (and asking the user) whether to add
   `!Mods/GameFeatures/DocMod/` before any further hand-written C++ is
   considered safely persisted.
2. **No log category exists yet.** CLAUDE.md/PLAN.md both assume
   `LogDocModAI` as a starting point; it doesn't exist in the current
   `DocMod.cpp`/`.h` — this is genuinely Phase 2 work, not something to
   discover, confirming PLAN.md's own sequencing is accurate here.
3. **JSON support is already wired into Build.cs** (`Json`, `JsonUtilities`
   public dependencies), ahead of where PLAN.md Phase 6 expects to add it.
   No Build.cs change needed when that phase arrives.
4. **No `Config/Alpakit.ini` for DocMod**, unlike `ExampleMod`. Packaging
   configuration should be verified in-editor before Alpakit is exercised
   again for DocMod specifically.
5. **A `Development Editor` build of DocMod already exists** in
   `Binaries/Win64/` and `Intermediate/`, consistent with CLAUDE.md's claim
   that compilation has already succeeded — Phase 1 is about making that
   *reproducible from the CLI*, not about achieving a first successful
   compile.
6. **The UBT target name is `FactoryEditor`, not `DocMod`.** PLAN.md
   correctly anticipates `Project: FactoryGame` with `Configuration:
   Development Editor`, but the concrete UBT `-Target=` value that
   `tools/build-editor.ps1` (Phase 1) will need is `FactoryEditor`, and
   `DocMod` is compiled as a dependency of that target via the plugin
   system, not as its own target.
7. **`FactoryGame.sln` and engine registry association already exist** — no
   project-file generation or engine-path discovery work is required before
   Phase 1's build script can be written; the engine path
   (`F:/Claude/Unreal Engine - CSS`) is resolvable directly from
   `HKCU\SOFTWARE\Epic Games\Unreal Engine\Builds\5.6.1-CSS`.

## Not yet touched (deliberately, per Phase 0 scope)

- No source files modified.
- No `.gitignore` changes.
- No build attempted.
- `tools/build-editor.ps1` not created (Phase 1).
- No log category added (Phase 2).
- No Blueprint-callable function added (Phase 3).
