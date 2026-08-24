# F11 hotkey — targeted manufacturer + live clock-speed test

**This performs a real game-state mutation when pressed.** Only active
while testing against a disposable save — see the Shipping-gating note
below.

## What it does

Press **F11** in-game:

1. Looks up the manufacturer the local player is currently aiming
   at/can interact with (`GetTargetedManufacturer` —
   `AFGCharacterPlayer::GetBestUsableActor()`, the same state driving the
   "Press E to interact" prompt).
2. If nothing/non-manufacturer is targeted: logs it and sends a yellow
   chat message ("not looking at a manufacturer"), stops there.
3. If a manufacturer is targeted: logs its recipe/clock speed/status and
   sends a white chat message announcing what's targeted.
4. Attempts to raise its clock speed by 10 percentage points via
   `SetManufacturerClockSpeed` — a real call into the Phase 12 write
   path, with all its existing validation (target exists, operation
   permitted, value in range).
5. Reports the result: **green** chat message + `Display` log on
   success, **red** chat message + `Warning` log with the actual error
   code/message on failure (e.g. `INVALID_CLOCK_SPEED` if already near
   the building's max potential).

This exists specifically to make the Phase 12 positive-path write test
(`docs/manual-verification.md` item 8) something you can trigger by
walking up to a machine and pressing a key, rather than hand-typing
`Invoke-RestMethod` calls with a copy-pasted buildable id.

## How it's built (no Content/Blueprint asset needed)

Unlike the chat command (`docs/chat-and-console-commands.md`), this
needed no Editor step at all — the whole input action + mapping context +
key binding is constructed in pure C++ at runtime
(`Mods/GameFeatures/DocMod/Source/DocMod/Private/DocModHotkey.cpp`):

- `AFGPlayerController::AddMappingContextImmediately(UInputMappingContext*)`
  — a real, public FactoryGame function (`FGPlayerController.h:674`) for
  registering an Enhanced Input mapping context at runtime, found by
  reading the header rather than assumed.
- `UInputMappingContext::MapKey(const UInputAction*, FKey)` — builds the
  key mapping programmatically.
- Both the `UInputAction` and `UInputMappingContext` are constructed via
  `NewObject<>()` and held for the module's lifetime via
  `TStrongObjectPtr` (since the owning code isn't itself a `UObject`, so
  can't hold them via a `UPROPERTY`).
- Bound once per world load from the same
  `FWorldDelegates::OnWorldInitializedActors` hook the self-test uses
  (`DocMod.cpp`), via `UEnhancedInputComponent::BindActionValueLambda`.

Chat delivery: `USMLRemoteCallObject::SendChatMessage` — the exact same
underlying call SML's own `UPlayerCommandSender::SendChatMessage` uses
for chat commands (`Mods/SML/Source/SML/Private/Player/PlayerCommandSender.cpp`),
reached directly via
`PlayerController->GetRemoteCallObjectOfClass<USMLRemoteCallObject>()`
since a keypress has no `UCommandSender` to go through.

## Why it's gated out of Shipping

The binding call (`DocModHotkey::SetupForWorld`) sits behind the same
`#if !UE_BUILD_SHIPPING` guard as the automatic self-test
(`DocMod.cpp`'s `OnWorldInitializedActors`). Unlike the read-only
console/chat commands (which stay available in all configs since they
only run on explicit invocation with no side effects), this one
*mutates real game state* on a keypress — a real player of a released
mod pressing F11 by accident and having a random machine's clock speed
changed would be a bad, confusing experience for what's explicitly a
developer testing aid, not a feature. The underlying
`DocModHotkey.cpp`/`GetTargetedManufacturer`/`SetManufacturerClockSpeed`
code all still compiles and exists in Shipping — only the automatic
binding is skipped.

## Configurability

Currently hardcoded to F11 (`EKeys::F11` in `DocModHotkey.cpp`), matching
how the HTTP server's port is hardcoded rather than config-driven.
Straightforward to move to a `UDeveloperSettings`-derived config class
(DocMod already depends on the `DeveloperSettings` module) if a
per-install-configurable key becomes worth the extra complexity — not
done here since nobody's asked for it yet.

## Status

**Not yet runtime-verified at all** — entirely new. Needs, at minimum:
confirming F11 actually triggers it (proving the Enhanced Input wiring
worked), confirming the chat messages actually appear on-screen (not
just in the log), and confirming the clock speed change is real and
visible in-game afterward. See
[manual-verification.md](manual-verification.md) item 8, which this
directly helps exercise.
