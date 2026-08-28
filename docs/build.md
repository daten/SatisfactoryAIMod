# Building the Development Editor target from the CLI

Phase 1 deliverable. This documents how to compile `FactoryEditor`
(Win64, Development) — which includes `FactoryGame` and every enabled
Game Feature plugin under `Mods/GameFeatures/`, including `AIMod` — without
opening Visual Studio.

## Prerequisites

- The custom Coffee Stain Unreal Engine build registered under the engine
  association named in `FactoryGame.uproject`'s `EngineAssociation`
  (currently `5.6.1-CSS`). This must be resolvable via
  `HKCU:\SOFTWARE\Epic Games\Unreal Engine\Builds\<EngineAssociation>`, which
  is normally set up automatically the first time the project is opened
  with that engine (e.g. via "Generate Visual Studio project files" or
  launching the .uproject once). In this environment it currently resolves
  to `F:\Claude\Unreal Engine - CSS`.
- .NET SDK (the engine's `Build.bat` bundles/uses its own — confirmed
  working with the bundled DotNet SDK 8.0.300 win-x64 in this environment;
  no separate install needed).
- No Visual Studio GUI interaction is required. `Build.bat` invokes
  `UnrealBuildTool.dll` directly via `dotnet`.

## Command

```powershell
.\tools\build-editor.ps1
```

This resolves the engine path from the registry and runs, verbatim:

```
"F:\Claude\Unreal Engine - CSS\Engine\Build\BatchFiles\Build.bat" FactoryEditor Win64 Development -Project="F:\Claude\SatisfactoryModLoader\FactoryGame.uproject" -WaitMutex -architecture=x64
```

This is the exact same invocation Visual Studio's generated project files
use for the "Development Editor" build configuration (verified against
`Intermediate\ProjectFiles\AIMod.vcxproj` / `UECommon.props`, which define
`NMakeBuildCommandLine` as `$(BuildBatchScript) FactoryEditor Win64
Development -Project="$(SolutionDir)FactoryGame.uproject" -WaitMutex
-FromMsBuild -architecture=x64`). The script omits `-FromMsBuild` since
that flag only changes UBT's error-message formatting for IDE parsing and
isn't relevant to a console workflow.

### Parameters

`tools/build-editor.ps1` accepts overrides if ever needed:

```powershell
.\tools\build-editor.ps1 -Target FactoryEditor -Platform Win64 -Configuration Development -Architecture x64 -EnginePath "F:\Claude\Unreal Engine - CSS"
```

All default to the values above; `-EnginePath` bypasses registry
resolution entirely (useful if the engine registration ever breaks or
points elsewhere).

## What the script does

1. Reads `EngineAssociation` out of `FactoryGame.uproject`.
2. Resolves that association to an engine install path via
   `HKCU:\SOFTWARE\Epic Games\Unreal Engine\Builds`.
3. Locates `Engine\Build\BatchFiles\Build.bat` under that path.
4. Invokes it with the target/platform/configuration/project args above,
   letting UBT's own stdout/stderr stream straight to the console.
5. Exits with UBT's actual exit code (`$LASTEXITCODE`) — build failures are
   never swallowed or converted into a generic "done" message.

The script is a thin wrapper around UnrealBuildTool. It does not replace,
reimplement, or bypass it, and it does not compile anything itself — Unreal
Build Tool remains authoritative, per CLAUDE.md.

## Validation performed

Ran `.\tools\build-editor.ps1` with no arguments in this environment:

```
Engine:        F:/Claude/Unreal Engine - CSS
Target:        FactoryEditor
Platform:      Win64
Configuration: Development
Architecture:  x64
Project:       F:\Claude\SatisfactoryModLoader\FactoryGame.uproject

Running UnrealBuildTool: dotnet "..\..\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll" FactoryEditor Win64 Development -Project="F:\Claude\SatisfactoryModLoader\FactoryGame.uproject" -WaitMutex -architecture=x64
...
Target is up to date

Result: Succeeded
Total execution time: 1.13 seconds

Build succeeded.
```

Exit code was `0`. `FactoryEditor`/`AIMod` were already built from a
previous Editor session (see [current-environment.md](current-environment.md)),
so UBT correctly reported "Target is up to date" rather than recompiling —
this still proves the CLI path resolves the engine, invokes UBT with the
correct target, and returns UBT's real result rather than a false positive.
The script has not yet been exercised on a path that forces actual
recompilation (e.g. after touching a AIMod `.cpp`/`.h` file); that will
happen naturally once Phase 2+ starts editing `AIMod.cpp`.

### Known benign warning

During the run, one of UBT's pre-build patch-check steps printed:

```
Set-ExecutionPolicy : The 'Set-ExecutionPolicy' command was found in the module 'Microsoft.PowerShell.Security', but
the module could not be loaded. For more information, run 'Import-Module Microsoft.PowerShell.Security'.
```

This comes from an internal UBT/engine patch-application script invoking
`powershell.exe` in a context where that module fails to autoload (observed
when the outer script itself runs under PowerShell 7/`pwsh`). It did not
affect the build result (all five engine patches reported "already been
applied. Skipping.", and the build still returned `Result: Succeeded`).
Noted here so a future session doesn't mistake it for a real failure.

## Not yet done

- No log category exists yet, so this build doesn't exercise any AIMod
  code changes (Phase 2).
- The script has not been validated on a *failing* build (e.g. deliberately
  broken source) to confirm error output surfaces cleanly and the exit code
  is non-zero — recommend doing this once real source changes start
  landing in Phase 2/3, rather than manufacturing an artificial failure now.
