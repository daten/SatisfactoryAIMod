# In-game chat and console commands

Answers "is it feasible for the player to interact with the mod and
issue commands through the in-game chat or console?" — yes, both exist
and are implemented. Two independent mechanisms, both read-only (neither
exposes `SetManufacturerClockSpeed`/`SetManufacturerRecipe` — see the
"Why no write commands yet" note at the bottom).

## Console commands — works immediately, no Editor step needed

`FDocModModule::RegisterConsoleCommands()`
(`Mods/GameFeatures/DocMod/Source/DocMod/Private/DocMod.cpp`) registers,
via plain `IConsoleManager::RegisterConsoleCommand` (no SML/Blueprint
involvement at all):

- `DocMod.SelfTest` — runs `DocModSelfTest::RunAll` (see
  [self-test.md](self-test.md)) against the current world.
- `DocMod.ResourceNodes` / `DocMod.Buildables` / `DocMod.Manufacturers` /
  `DocMod.Connections` — prints how many of each DocMod currently finds.
- `DocMod.Target` — prints recipe/clock speed/status/id for whichever
  manufacturer the local player is currently looking at (via
  `AFGCharacterPlayer::GetBestUsableActor()` — the game's own "what can I
  press E on" state, not a reimplemented trace), or a "not looking at a
  manufacturer" message. Added to answer "can I target the machine I'm
  currently looking at instead of picking one from a list?" — yes, both
  as a standalone lookup here and as `GetTargetedManufacturer()`/
  `"world.targetedManufacturer"` for Blueprint/RPC callers (see
  [telemetry-protocol.md](telemetry-protocol.md)).

Available in **all build configurations**, not just non-Shipping — unlike
the automatic self-test hook, these only run when someone explicitly
types one, so there's no automatic-behavior-in-Shipping concern.

**How to invoke:** press `~` (or whatever key opens the console in
Satisfactory — commonly needs cheats/console enabled, same as any other
Satisfactory console command) and type e.g. `DocMod.SelfTest`, or pass
`-ExecCmds="DocMod.SelfTest"` on the command line, or type it into the
Editor's Output Log console input field while in PIE. Results print via
the command's own `FOutputDevice` (so they show wherever you invoked it
from) and, for `SelfTest`, also the full detail via `LogDocModAI`.

**Not yet runtime-verified** — needs the game/Editor open to actually
press `~` and try it.

## Chat command — needs one Editor step to wire up

`ADocModChatCommand`
(`Mods/GameFeatures/DocMod/Source/DocMod/Public/DocModChatCommand.h`)
implements SML's real in-game chat command framework
(`AChatCommandInstance`, the same system Satisfactory's own built-in
`/help`/`/info`/`/playerlist` commands use —
`Mods/SML/Source/SML/Public/Command/SMLCommands/`). Once registered:

```
/docmod selftest         - runs the self-test, replies in chat when done
/docmod resourcenodes    - replies with the current resource node count
/docmod buildables       - replies with the current buildable count
/docmod manufacturers    - replies with the current manufacturer count
/docmod connections      - replies with the current connection count
/docmod target           - replies with info about the manufacturer you're currently looking at
```

(alias: `/dm`)

**Unlike the console commands, this needs to be registered** — SML's
chat commands are data-driven via a `UGameWorldModule` Blueprint asset's
`mChatCommands` array
(`Mods/SML/Source/SML/Public/Module/GameWorldModule.h:32-33`), not
auto-discovered by scanning for `AChatCommandInstance` subclasses.
`ADocModChatCommand` is a plain `UCLASS()` C++ class (not
Blueprint-only), so it should appear directly by name in any
`TSubclassOf<AChatCommandInstance>` picker in the Editor — no Blueprint
wrapper needed, just a reference to it.

**DocMod currently has no `UGameWorldModule` asset** — Phase 0's content
inventory found only `DocMod.uasset` (the `FGGameFeatureData`),
`Recipe_DocRecipe`, `Schematic_DocSchem`/`Icon_SchemDoc`, and
`RootGameWorld_DocMod` (a level). This is an Editor/Content step, not
something scriptable from the CLI (creating and configuring a Blueprint
data asset needs the Editor UI).

**Concrete reference to copy from:** `ExampleMod` already has working
chat commands in this exact repo —
`Mods/GameFeatures/ExampleMod/Content/ChatCommands/CC_ExampleChatCommand.uasset`
(plus `CC_ExamplePerPlayerData`, `CC_ExampleReplication`). Open one of
those in the Editor, find what `UGameWorldModule` (or equivalent) asset
references it in an `mChatCommands`-style array, and add
`ADocModChatCommand` to DocMod's equivalent — creating one for DocMod if
it doesn't have one yet, following the same pattern ExampleMod uses. This
is more reliable than instructions written without being able to open
the Editor and confirm the exact asset graph directly.

**Not yet runtime-verified** — needs both the Editor wiring step above
and the game/Editor open to actually type `/docmod` in chat.

## Why no write commands yet

`SetManufacturerClockSpeed`/`SetManufacturerRecipe` aren't exposed via
chat or console. A mistyped buildable id or recipe class path in a chat
message is an easy, low-friction way to trigger CLAUDE.md's validation
rejections by accident (or, if the validation ever has a gap, to mutate
the wrong building) — that's a different risk profile than a read-only
reporting command, and per CLAUDE.md's Safety and Stability Boundary
deserves a deliberate, separate decision rather than being tacked onto
this pass. If wanted, the natural extension is `/docmod setclockspeed
<id> <percent>` etc., reusing the same
`UDocModFunctionLibrary::SetManufacturer*` functions and their existing
validation — but that's a future addition, not implemented here.

Now that `GetTargetedManufacturer()`/`DocMod.Target` exist, a nicer
version of that future command wouldn't need an id argument at all -
`/docmod setclockspeed <percent>` could operate on whatever the player is
currently looking at, exactly like the in-game build gun operates on
whatever's under the crosshair. Worth keeping in mind if/when write
commands get added, since typing a full buildable-id path in chat is
real friction `GetTargetedManufacturer` already solves for reads.
