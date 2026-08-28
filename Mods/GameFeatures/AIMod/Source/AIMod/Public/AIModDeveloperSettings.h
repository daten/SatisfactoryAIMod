// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "InputCoreTypes.h"
#include "AIModDeveloperSettings.generated.h"

/**
 * AIMod dev-time settings (PLAN.md - this is a development convenience,
 * see AIModHotkey.h). Editable via Project Settings > Plugins > AIMod
 * in the Editor, or by hand-editing Config/DefaultAIMod.ini
 * ([/Script/AIMod.AIModDeveloperSettings], HotkeyKey=(KeyName="F6")) -
 * no rebuild needed either way, unlike changing the old hardcoded
 * EKeys::F11 constant.
 *
 * Default changed from F11 to F6 after a real conflict was found:
 * Config/DefaultInput.ini has bF11TogglesFullscreen=True, an
 * engine-level shortcut that intercepts F11 before Enhanced Input
 * gameplay bindings ever see it - not something bindable around, only
 * avoidable by picking a different key.
 */
UCLASS(config = AIMod, defaultconfig, meta = (DisplayName = "AIMod"))
class AIMOD_API UAIModDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UAIModDeveloperSettings();

	/**
	 * Key that triggers AIModHotkey (print targeted manufacturer + test
	 * clock speed change). Change here if it collides with another
	 * binding on your system - F11 clashed with the engine's built-in
	 * fullscreen toggle, which is why the default is F6 instead.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Hotkey")
	FKey HotkeyKey = EKeys::F6;
};
