# Mod Rename Migration: DocMod → AIMod (complete, 2026-08-28)

The mod formerly known as `DocMod` is now `AIMod` — plugin, module,
`ModReference`, every `U`/`F` class prefix, the `LogAIModAI` log category,
every `AIMod.*` console command, the `/aimod` chat command, and every doc
reference. This was executed as one atomic pass once the user generated a
fresh `AIMod` template via the SML editor tooling
(`Mods/GameFeatures/AIMod/`), per the naming decision below.

## Naming decision (unchanged from the original plan)

`AIMod`, not `SatisfactoryAIMod` — the plugin/module/`ModReference` stays
bare, matching the `ExampleMod` convention already used in this SML starter
(no redundant "Satisfactory" prefix inside a Satisfactory-only modding
ecosystem). `.uplugin`'s `FriendlyName` is decoupled from that identifier
(confirmed via `ExampleMod.uplugin`), so the GitHub repo name
(`SatisfactoryAIMod`) and the in-engine `FriendlyName` ("AIMod", set by the
user in the fresh template) can differ from the C++ identifier without
forcing a longer string into every symbol.

## What actually happened

1. All 19 files under `Source/DocMod/{Public,Private}` + `DocMod.Build.cs`
   copied into the user's fresh `Source/AIMod/` template, filenames and
   every `DocMod`/`docmod`/`DOCMOD_API` occurrence replaced with the
   `AIMod`/`aimod`/`AIMOD_API` equivalent (case-sensitive three-pass sed -
   catches the mixed-case class prefixes, the lowercase `/aimod` chat
   command, and the all-caps module export macro in one pass each).
2. `AIMod.Build.cs` ported from `DocMod.Build.cs` wholesale (not the fresh
   template's placeholder version) - preserves the real accumulated
   dependencies (`HTTPServer`, `Sockets`, active `EnhancedInput`) the fresh
   template didn't have yet.
3. `Config/DefaultEngine.ini`'s loopback-binding block ported into the new
   module's `Config/`, with the `AIModHttpServerSubsystem` path/port-match
   comment updated. The root-level `Config/DefaultEngine.ini`'s mirrored
   comment updated too. `AccessTransformers.ini`/`PluginSettings.ini` were
   already identical between the fresh template and the old mod, so those
   needed no changes.
4. Compiled clean (`FactoryEditor Win64 Development`) with both `DocMod`
   and `AIMod` present side by side first, confirming the port didn't
   silently depend on anything only the old module provided, then again
   after deleting `Mods/GameFeatures/DocMod/` entirely, confirming nothing
   else in the build depended on the old module either.
5. Every doc/prose reference to `DocMod` rewritten to `AIMod` (full
   replacement, not an annotated rename, per the user's confirmation that
   commit history preserves the old name if ever needed): `AGENTS.md`,
   `CLAUDE.md`, `PLAN.md`, `PLAYBOOK.md`, `README.md`, `RPC_REFERENCE.md`,
   every file under `docs/`, `controller/` (including its Python source,
   not just docs), `tests/`, `tools/build-editor.ps1`, and the root
   `.gitignore`'s `!Mods/GameFeatures/AIMod/` allowlist entry.

## What was deliberately NOT ported

`Mods/GameFeatures/DocMod/Content/` had leftover SML-template demo assets
never used by the mod's real functionality - `Recipe_DocRecipe.uasset`,
`Schematic_DocSchem.uasset`/`Icon_SchemDoc.uasset`, and
`RootGameWorld_DocMod.uasset`. Confirmed via source grep that nothing in
`DocMod`'s C++ referenced any of them by name. These were left behind when
the old directory was deleted, not carried into `AIMod`'s own fresh
`Content/` - the fresh template's own `Content/AIMod.uasset`
(`GameFeatureData`) is the user's own creation from the editor tooling, not
something this migration touched or should have touched (Content/asset
work belongs in the editor per `CLAUDE.md`'s Blueprint Boundary section,
not a scripted text migration). If anything in the old `DocMod`'s
`GameFeatureData` asset referenced `RootGameWorld_DocMod` as a per-mod
level (not detectable via text search - `.uasset` is binary), verify in
the editor that `AIMod`'s own `GameFeatureData` doesn't need an equivalent;
not verified either way here.

## Environment note unrelated to the rename itself

Mid-migration, the project's local `Engine\` convenience junction (used for
`Engine\Build\BatchFiles\Build.bat`-relative CLI builds) had disappeared -
unrelated to any file this migration touched. Builds during this migration
used the real engine directly:
`tools/build-editor.ps1` (already in the repo, resolves the engine from
the `.uproject`'s `EngineAssociation` via the registry - the failure-proof
way to find it) or the literal path `F:\Claude\Unreal Engine - CSS` for a
manual invocation. Worth using `tools/build-editor.ps1` first next time
rather than guessing a path by hand - it would have avoided one failed
attempt against a different, incomplete `F:\Claude\UnrealEngine` copy this
session hit before finding the right one.
