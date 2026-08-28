// Copyright Epic Games, Inc. All Rights Reserved.

#include "DocModConfiguration.h"
#include "Configuration/Properties/ConfigPropertyBool.h"
#include "Configuration/Properties/ConfigPropertyFloat.h"
#include "UObject/ConstructorHelpers.h"

UDocModConfiguration::UDocModConfiguration()
{
	ConfigId.ModReference = TEXT("DocMod");
	ConfigId.ConfigCategory = TEXT("");

	DisplayName = FText::FromString(TEXT("DocMod AI Interface"));
	Description = FText::FromString(TEXT("Safety/capability trade-offs for the DocMod RPC interface. All default off - the RPC behaves exactly as before unless you opt in here."));

	// SML's C++ config property classes (UConfigPropertySection,
	// UConfigPropertyBool, UConfigPropertyFloat) all inherit
	// CreateEditorWidget_Implementation's unconditional `return NULL`
	// (SML's ConfigProperty.cpp) - real widget creation only exists on
	// their Blueprint subclasses under
	// Mods/SML/Content/Interface/UI/Menu/Mods/ConfigProperties/
	// (BP_ConfigPropertySection, BP_ConfigPropertyBool,
	// BP_ConfigPropertyFloat). Constructing the raw C++ classes (as this
	// file originally did) produces properties that are structurally
	// correct - HasResettableChildProperty() still works, RPC reads via
	// GetDocModConfigBool still work - but render as a completely blank
	// page in the Mods settings UI: no checkboxes, no descriptions, since
	// CreateEditorWidget always returns NULL. Confirmed live (2026-08-28):
	// the DocMod config page showed a "Reset to Default" button (pure
	// C++ data logic, unrelated to widget creation) but zero visible
	// property rows. Fix: resolve the real Blueprint classes here via
	// ConstructorHelpers::FClassFinder and construct instances of THOSE
	// instead, via the raw (non-template) UObject::CreateDefaultSubobject
	// overload that accepts an explicit runtime UClass (Object.h) - falls
	// back to the C++ base class if the Blueprint asset can't be found,
	// so a missing/renamed asset degrades to the old (data-only) behavior
	// instead of failing to construct at all.
	UClass* SectionClass = UConfigPropertySection::StaticClass();
	if (ConstructorHelpers::FClassFinder<UConfigPropertySection> SectionFinder(TEXT("/SML/Interface/UI/Menu/Mods/ConfigProperties/BP_ConfigPropertySection")); SectionFinder.Succeeded())
	{
		SectionClass = SectionFinder.Class;
	}
	UClass* BoolClass = UConfigPropertyBool::StaticClass();
	if (ConstructorHelpers::FClassFinder<UConfigPropertyBool> BoolFinder(TEXT("/SML/Interface/UI/Menu/Mods/ConfigProperties/BP_ConfigPropertyBool")); BoolFinder.Succeeded())
	{
		BoolClass = BoolFinder.Class;
	}
	UClass* FloatClass = UConfigPropertyFloat::StaticClass();
	if (ConstructorHelpers::FClassFinder<UConfigPropertyFloat> FloatFinder(TEXT("/SML/Interface/UI/Menu/Mods/ConfigProperties/BP_ConfigPropertyFloat")); FloatFinder.Succeeded())
	{
		FloatClass = FloatFinder.Class;
	}

	UConfigPropertySection* Section = CastChecked<UConfigPropertySection>(CreateDefaultSubobject(TEXT("RootSection"), UConfigPropertySection::StaticClass(), SectionClass, true, false));
	RootSection = Section;

	UConfigPropertyBool* AllowRemoteConnections = CastChecked<UConfigPropertyBool>(CreateDefaultSubobject(TEXT("AllowRemoteConnections"), UConfigPropertyBool::StaticClass(), BoolClass, true, false));
	AllowRemoteConnections->DisplayName = FText::FromString(TEXT("Allow Remote Connections"));
	AllowRemoteConnections->Tooltip = FText::FromString(TEXT(
		"SECURITY RISK. By default the DocMod RPC server only accepts connections from this machine (loopback). "
		"Enabling this lets other devices on your network send RPC commands too - anyone who can reach this "
		"machine's port can then control building/telemetry through DocMod. Only enable this on a network you trust."));
	AllowRemoteConnections->DefaultValue = false;
	AllowRemoteConnections->Value = false;
	Section->SectionProperties.Add(TEXT("AllowRemoteConnections"), AllowRemoteConnections);

	UConfigPropertyBool* UnlimitedResources = CastChecked<UConfigPropertyBool>(CreateDefaultSubobject(TEXT("UnlimitedResources"), UConfigPropertyBool::StaticClass(), BoolClass, true, false));
	UnlimitedResources->DisplayName = FText::FromString(TEXT("Unlimited Resources for RPC Builds"));
	UnlimitedResources->Tooltip = FText::FromString(TEXT(
		"By default, RPC-driven construction requires real materials in your inventory, exactly like placing it "
		"yourself. Enabling this lets the RPC build without consuming/requiring materials - a game advantage, "
		"not a safety concern. All other placement rules (clearance, valid floor, etc.) still apply."));
	UnlimitedResources->DefaultValue = false;
	UnlimitedResources->Value = false;
	Section->SectionProperties.Add(TEXT("UnlimitedResources"), UnlimitedResources);

	UConfigPropertyBool* LimitBuildDistance = CastChecked<UConfigPropertyBool>(CreateDefaultSubobject(TEXT("LimitBuildDistance"), UConfigPropertyBool::StaticClass(), BoolClass, true, false));
	LimitBuildDistance->DisplayName = FText::FromString(TEXT("Limit RPC Build Distance From Player"));
	LimitBuildDistance->Tooltip = FText::FromString(TEXT(
		"By default, RPC-driven construction has no distance limit at all - it can build anywhere on the map, "
		"unlike your own Build Gun. Enabling this restricts RPC construction to within Max Build Distance of "
		"you, so an external controller can't build somewhere you can't see. Off by default."));
	LimitBuildDistance->DefaultValue = false;
	LimitBuildDistance->Value = false;
	Section->SectionProperties.Add(TEXT("LimitBuildDistance"), LimitBuildDistance);

	// Default 8000 units ~= 10 standard 8m foundation tiles (800 units
	// each), per the user's own "10 foundations away" framing when this
	// setting was requested.
	UConfigPropertyFloat* MaxBuildDistance = CastChecked<UConfigPropertyFloat>(CreateDefaultSubobject(TEXT("MaxBuildDistance"), UConfigPropertyFloat::StaticClass(), FloatClass, true, false));
	MaxBuildDistance->DisplayName = FText::FromString(TEXT("Max Build Distance (cm)"));
	MaxBuildDistance->Tooltip = FText::FromString(TEXT(
		"Only used when Limit RPC Build Distance From Player is on. Maximum distance, in centimeters, RPC-driven "
		"construction is allowed from you. 800 = one 8m foundation tile; default 8000 is roughly 10 tiles."));
	MaxBuildDistance->DefaultValue = 8000.0f;
	MaxBuildDistance->Value = 8000.0f;
	Section->SectionProperties.Add(TEXT("MaxBuildDistance"), MaxBuildDistance);

	// Default ON (unlike the four safety/capability toggles above, which
	// default off to preserve prior behavior) - this is a pure UX
	// nicety, not a security or gameplay-balance trade-off, and is the
	// literal feature being requested when this was added. See
	// UDocModHttpServerSubsystem::HandlePlayerChatMessageAdded.
	UConfigPropertyBool* AutoAcknowledgeChatMessages = CastChecked<UConfigPropertyBool>(CreateDefaultSubobject(TEXT("AutoAcknowledgeChatMessages"), UConfigPropertyBool::StaticClass(), BoolClass, true, false));
	AutoAcknowledgeChatMessages->DisplayName = FText::FromString(TEXT("Auto-Acknowledge Chat Messages"));
	AutoAcknowledgeChatMessages->Tooltip = FText::FromString(TEXT(
		"When you type a message in chat, DocMod immediately posts a brief \"seen\" reply - independent of "
		"whether an external AI controller is actually watching yet, so you know your message registered. "
		"On by default; turn off if you find it noisy."));
	AutoAcknowledgeChatMessages->DefaultValue = true;
	AutoAcknowledgeChatMessages->Value = true;
	Section->SectionProperties.Add(TEXT("AutoAcknowledgeChatMessages"), AutoAcknowledgeChatMessages);

	// A sixth property, added 2026-08-28 - back to the "off by default,
	// player-opt-in-only" character of the original four. There is no
	// existing in-game equivalent of "a player spawns a creature", so
	// this is treated the same as bUnlimitedResources: a capability an
	// external AI controller can never enable itself, only the player
	// from this settings menu. See UDocModFunctionLibrary::SpawnCreatureNearPlayer.
	UConfigPropertyBool* AllowCreatureSpawning = CastChecked<UConfigPropertyBool>(CreateDefaultSubobject(TEXT("AllowCreatureSpawning"), UConfigPropertyBool::StaticClass(), BoolClass, true, false));
	AllowCreatureSpawning->DisplayName = FText::FromString(TEXT("Allow Creature Spawning"));
	AllowCreatureSpawning->Tooltip = FText::FromString(TEXT(
		"Off by default. When enabled, an external AI controller can spawn real creatures near you on request "
		"via DocMod's RPC interface. Off means every such request is rejected regardless of what's asked."));
	AllowCreatureSpawning->DefaultValue = false;
	AllowCreatureSpawning->Value = false;
	Section->SectionProperties.Add(TEXT("AllowCreatureSpawning"), AllowCreatureSpawning);
}
