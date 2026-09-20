# In-game chat and console commands

Answers "is it feasible for the player to interact with the mod and
issue commands through the in-game chat or console?" — yes, both exist
and are implemented. Two independent mechanisms, both read-only (neither
exposes `SetManufacturerClockSpeed`/`SetManufacturerRecipe` — see the
"Why no write commands yet" note at the bottom).

## Console commands — works immediately, no Editor step needed

`FAIModModule::RegisterConsoleCommands()`
(`Mods/GameFeatures/AIMod/Source/AIMod/Private/AIMod.cpp`) registers,
via plain `IConsoleManager::RegisterConsoleCommand` (no SML/Blueprint
involvement at all):

- `AIMod.SelfTest` — runs `AIModSelfTest::RunAll` (see
  [self-test.md](self-test.md)) against the current world.
- `AIMod.ResourceNodes` / `AIMod.Buildables` / `AIMod.Manufacturers` /
  `AIMod.Connections` — prints how many of each AIMod currently finds.
- `AIMod.Target` — prints recipe/clock speed/status/id for whichever
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
Satisfactory console command) and type e.g. `AIMod.SelfTest`, or pass
`-ExecCmds="AIMod.SelfTest"` on the command line, or type it into the
Editor's Output Log console input field while in PIE. Results print via
the command's own `FOutputDevice` (so they show wherever you invoked it
from) and, for `SelfTest`, also the full detail via `LogAIModAI`.

**Not yet runtime-verified** — needs the game/Editor open to actually
press `~` and try it.

## Chat command — needs one Editor step to wire up

`AAIModChatCommand`
(`Mods/GameFeatures/AIMod/Source/AIMod/Public/AIModChatCommand.h`)
implements SML's real in-game chat command framework
(`AChatCommandInstance`, the same system Satisfactory's own built-in
`/help`/`/info`/`/playerlist` commands use —
`Mods/SML/Source/SML/Public/Command/SMLCommands/`). Once registered:

```
/aimod selftest         - runs the self-test, replies in chat when done
/aimod resourcenodes    - replies with the current resource node count
/aimod buildables       - replies with the current buildable count
/aimod manufacturers    - replies with the current manufacturer count
/aimod connections      - replies with the current connection count
/aimod target           - replies with info about the manufacturer you're currently looking at
```

(alias: `/dm`)

**Unlike the console commands, this needs to be registered** — SML's
chat commands are data-driven via a `UGameWorldModule` Blueprint asset's
`mChatCommands` array
(`Mods/SML/Source/SML/Public/Module/GameWorldModule.h:32-33`), not
auto-discovered by scanning for `AChatCommandInstance` subclasses.
`AAIModChatCommand` is a plain `UCLASS()` C++ class (not
Blueprint-only), so it should appear directly by name in any
`TSubclassOf<AChatCommandInstance>` picker in the Editor — no Blueprint
wrapper needed, just a reference to it.

**AIMod currently has no `UGameWorldModule` asset** — Phase 0's content
inventory found only `AIMod.uasset` (the `FGGameFeatureData`),
`Recipe_DocRecipe`, `Schematic_DocSchem`/`Icon_SchemDoc`, and
`RootGameWorld_AIMod` (a level). This is an Editor/Content step, not
something scriptable from the CLI (creating and configuring a Blueprint
data asset needs the Editor UI).

**Concrete reference to copy from:** `ExampleMod` already has working
chat commands in this exact repo —
`Mods/GameFeatures/ExampleMod/Content/ChatCommands/CC_ExampleChatCommand.uasset`
(plus `CC_ExamplePerPlayerData`, `CC_ExampleReplication`). Open one of
those in the Editor, find what `UGameWorldModule` (or equivalent) asset
references it in an `mChatCommands`-style array, and add
`AAIModChatCommand` to AIMod's equivalent — creating one for AIMod if
it doesn't have one yet, following the same pattern ExampleMod uses. This
is more reliable than instructions written without being able to open
the Editor and confirm the exact asset graph directly.

**Not yet runtime-verified** — needs both the Editor wiring step above
and the game/Editor open to actually type `/aimod` in chat.

## Why no write commands yet

`SetManufacturerClockSpeed`/`SetManufacturerRecipe` aren't exposed via
chat or console. A mistyped buildable id or recipe class path in a chat
message is an easy, low-friction way to trigger CLAUDE.md's validation
rejections by accident (or, if the validation ever has a gap, to mutate
the wrong building) — that's a different risk profile than a read-only
reporting command, and per CLAUDE.md's Safety and Stability Boundary
deserves a deliberate, separate decision rather than being tacked onto
this pass. If wanted, the natural extension is `/aimod setclockspeed
<id> <percent>` etc., reusing the same
`UAIModFunctionLibrary::SetManufacturer*` functions and their existing
validation — but that's a future addition, not implemented here.

Now that `GetTargetedManufacturer()`/`AIMod.Target` exist, a nicer
version of that future command wouldn't need an id argument at all -
`/aimod setclockspeed <percent>` could operate on whatever the player is
currently looking at, exactly like the in-game build gun operates on
whatever's under the crosshair. Worth keeping in mind if/when write
commands get added, since typing a full buildable-id path in chat is
real friction `GetTargetedManufacturer` already solves for reads.
