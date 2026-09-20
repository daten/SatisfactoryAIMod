# C++ -> Blueprint smoke test (Phase 3)

Phase 3 deliverable. Proves the chain `C++ -> UHT -> Blueprint -> Alpakit ->
Satisfactory` works before any real telemetry code is written.

## What was added

`Mods/GameFeatures/AIMod/Source/AIMod/Public/AIModFunctionLibrary.h` /
`Private/AIModFunctionLibrary.cpp`:

```cpp
UCLASS()
class AIMOD_API UAIModFunctionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintPure, Category = "AIMod|AI Interface", meta = (BlueprintThreadSafe))
    static FString GetInterfaceVersion();
};
```

`GetInterfaceVersion()` returns the literal string `0.1.0` and also logs at
`Verbose` level to `LogAIModAI` (see [current-environment.md](current-environment.md)
/ the Phase 2 log category work) so it's visible with `-LogCmds="LogAIModAI Verbose"`
or in the editor Output Log with the log category's verbosity raised.

This mirrors SML's own `UBlueprintVersionLibrary` pattern
(`Mods/SML/Source/SML/Public/Kismet/BlueprintVersionLibrary.h`) — a plain
`UBlueprintFunctionLibrary` with a `BlueprintPure` static function — which
is the smallest reasonable implementation for a stateless version query, per
PLAN.md Phase 3's "prefer the smallest implementation" guidance. It does not
touch a subsystem, service class, or any world/game state.

**Compiled and linked successfully** via `.\tools\build-editor.ps1`
(UnrealHeaderTool generated reflection code for the new `UCLASS`, `Result:
Succeeded`). This proves steps 1 (C++) and 2 (UHT) of the milestone chain.
Steps 3–7 below require Unreal Editor and Satisfactory and have **not** been
performed yet — they need to be run manually.

## Manual validation steps (steps 3–7 of the milestone)

1. Open `FactoryGame.uproject` in Unreal Editor (Development Editor
   configuration — the one just built).
2. Open the `AIMod` content, e.g.
   `Mods/GameFeatures/AIMod/Content/RootGameWorld_AIMod` level (or any
   other convenient Blueprint graph — a Level Blueprint works fine for a
   throwaway smoke test).
3. Open its Level Blueprint (Blueprints menu -> Open Level Blueprint).
4. On the `Event BeginPlay` node (or any convenient event), add a
   **Get Interface Version** node — search for it under the `AIMod | AI
   Interface` category to confirm it's actually exposed to Blueprint.
5. Wire it into a **Print String** node so the returned value is visible
   on-screen/in the log when the level runs.
6. Play-in-editor (or launch the level) and confirm `0.1.0` is printed.
7. Package with Alpakit and deploy to the local Satisfactory install; launch
   Satisfactory and confirm the same behavior in a real game session.

**Expected result:** the Blueprint graph shows a `Get Interface Version`
node callable exactly like any other Blueprint function library call, and
it returns the string `0.1.0` both in-editor and in the packaged/deployed
build. If the node does not appear in the palette, the most likely causes
are a stale Blueprint asset cache (recompile via Editor "Compile" button /
restart editor) or the module not being loaded because the Game Feature
isn't active — check `LogAIModAI` for the Phase 2 startup message first.

Do not claim this phase is runtime-verified until these manual steps have
actually been run — a successful compile only proves the code is
syntactically and semantically valid to Unreal, not that it behaves
correctly at runtime (per CLAUDE.md's Definition of Done).
