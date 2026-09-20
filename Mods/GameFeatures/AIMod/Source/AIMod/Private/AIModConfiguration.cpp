// Copyright Epic Games, Inc. All Rights Reserved.

#include "AIModConfiguration.h"
#include "Configuration/Properties/ConfigPropertyBool.h"
#include "Configuration/Properties/ConfigPropertyFloat.h"
#include "UObject/ConstructorHelpers.h"
#include "Configuration/Properties/WidgetExtension/CP_Section.h"

UAIModConfiguration::UAIModConfiguration(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ConfigId.ModReference = TEXT("AIMod");
	ConfigId.ConfigCategory = TEXT("");

	DisplayName = FText::FromString(TEXT("AIMod AI Interface"));
	Description = FText::FromString(TEXT("Safety/capability trade-offs for the AIMod RPC interface. All default off - the RPC behaves exactly as before unless you opt in here."));

	// SML's C++ config property classes (UConfigPropertySection,
	// UConfigPropertyBool, UConfigPropertyFloat) all inherit
	// CreateEditorWidget_Implementation's unconditional `return NULL`
	// (SML's ConfigProperty.cpp) - real widget creation only exists on
	// their Blueprint subclasses under
	// Mods/SML/Content/Interface/UI/Menu/Mods/ConfigProperties/
	// (BP_ConfigPropertySection, BP_ConfigPropertyBool,
	// BP_ConfigPropertyFloat). Constructing the raw C++ classes produces
	// properties that are structurally correct - HasResettableChildProperty()
	// still works, RPC reads via GetAIModConfigBool still work - but render
	// as a completely blank page in the Mods settings UI: no checkboxes, no
	// descriptions, since CreateEditorWidget always returns NULL (the page
	// still shows a "Reset to Default" button, which is pure C++ data logic
	// unrelated to widget creation). Fix: resolve the real Blueprint classes
	// here via ConstructorHelpers::FClassFinder and construct instances of
	// THOSE instead, via the raw (non-template) UObject::CreateDefaultSubobject
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

	// The Section is a subobject of THIS configuration; each property below
	// is created as a subobject of the SECTION (via ObjectInitializer's
	// explicit-Outer overload) rather than of this configuration. See the
	// header's constructor doc comment for why that parenting is required
	// for config persistence to work at all - parenting the properties to
	// the configuration object instead breaks save-on-change.
	UConfigPropertySection* Section = CastChecked<UConfigPropertySection>(ObjectInitializer.CreateDefaultSubobject(this, TEXT("RootSection"), UConfigPropertySection::StaticClass(), SectionClass, true, false));
	RootSection = Section;

	// BP_ConfigPropertySection derives from UCP_Section (CP_Section.h),
	// which adds a WidgetType enum controlling Horizontal-with-scrollbar
	// vs Vertical layout for its child rows - defaults to Horizontal, which
	// renders these toggles in a horizontal scrolling row. Force Vertical
	// instead, the layout every other section in this settings menu uses.
	if (UCP_Section* SectionExtended = Cast<UCP_Section>(Section))
	{
		SectionExtended->WidgetType = ECP_SectionWidgetType::CPS_Vertical;
	}

	UConfigPropertyBool* AllowRemoteConnections = CastChecked<UConfigPropertyBool>(ObjectInitializer.CreateDefaultSubobject(Section, TEXT("AllowRemoteConnections"), UConfigPropertyBool::StaticClass(), BoolClass, true, false));
	AllowRemoteConnections->DisplayName = FText::FromString(TEXT("Allow Remote Connections"));
	AllowRemoteConnections->Tooltip = FText::FromString(TEXT(
		"SECURITY RISK. By default the AIMod RPC server only accepts connections from this machine (loopback). "
		"Enabling this lets other devices on your network send RPC commands too - anyone who can reach this "
		"machine's port can then control building/telemetry through AIMod. Only enable this on a network you trust."));
	AllowRemoteConnections->DefaultValue = false;
	AllowRemoteConnections->Value = false;
	Section->SectionProperties.Add(TEXT("AllowRemoteConnections"), AllowRemoteConnections);

	UConfigPropertyBool* UnlimitedResources = CastChecked<UConfigPropertyBool>(ObjectInitializer.CreateDefaultSubobject(Section, TEXT("UnlimitedResources"), UConfigPropertyBool::StaticClass(), BoolClass, true, false));
	UnlimitedResources->DisplayName = FText::FromString(TEXT("Unlimited Resources for RPC Builds"));
	UnlimitedResources->Tooltip = FText::FromString(TEXT(
		"By default, RPC-driven construction requires real materials in your inventory, exactly like placing it "
		"yourself. Enabling this lets the RPC build without consuming/requiring materials - a game advantage, "
		"not a safety concern. All other placement rules (clearance, valid floor, etc.) still apply."));
	UnlimitedResources->DefaultValue = false;
	UnlimitedResources->Value = false;
	Section->SectionProperties.Add(TEXT("UnlimitedResources"), UnlimitedResources);

	UConfigPropertyBool* LimitBuildDistance = CastChecked<UConfigPropertyBool>(ObjectInitializer.CreateDefaultSubobject(Section, TEXT("LimitBuildDistance"), UConfigPropertyBool::StaticClass(), BoolClass, true, false));
	LimitBuildDistance->DisplayName = FText::FromString(TEXT("Limit RPC Build Distance From Player"));
	LimitBuildDistance->Tooltip = FText::FromString(TEXT(
		"By default, RPC-driven construction has no distance limit at all - it can build anywhere on the map, "
		"unlike your own Build Gun. Enabling this restricts RPC construction to within Max Build Distance of "
		"you, so an external controller can't build somewhere you can't see. Off by default."));
	LimitBuildDistance->DefaultValue = false;
	LimitBuildDistance->Value = false;
	Section->SectionProperties.Add(TEXT("LimitBuildDistance"), LimitBuildDistance);

	// Default 8000 units ~= 10 standard 8m foundation tiles (800 units
	// each).
	UConfigPropertyFloat* MaxBuildDistance = CastChecked<UConfigPropertyFloat>(ObjectInitializer.CreateDefaultSubobject(Section, TEXT("MaxBuildDistance"), UConfigPropertyFloat::StaticClass(), FloatClass, true, false));
	MaxBuildDistance->DisplayName = FText::FromString(TEXT("Max Build Distance (cm)"));
	MaxBuildDistance->Tooltip = FText::FromString(TEXT(
		"Only used when Limit RPC Build Distance From Player is on. Maximum distance, in centimeters, RPC-driven "
		"construction is allowed from you. 800 = one 8m foundation tile; default 8000 is roughly 10 tiles."));
	MaxBuildDistance->DefaultValue = 8000.0f;
	MaxBuildDistance->Value = 8000.0f;
	Section->SectionProperties.Add(TEXT("MaxBuildDistance"), MaxBuildDistance);

	// Default ON (unlike the four safety/capability toggles above, which
	// default off to preserve prior behavior) - this is a pure UX
	// nicety, not a security or gameplay-balance trade-off. See
	// UAIModHttpServerSubsystem::HandlePlayerChatMessageAdded.
	UConfigPropertyBool* AutoAcknowledgeChatMessages = CastChecked<UConfigPropertyBool>(ObjectInitializer.CreateDefaultSubobject(Section, TEXT("AutoAcknowledgeChatMessages"), UConfigPropertyBool::StaticClass(), BoolClass, true, false));
	AutoAcknowledgeChatMessages->DisplayName = FText::FromString(TEXT("Auto-Acknowledge Chat Messages"));
	AutoAcknowledgeChatMessages->Tooltip = FText::FromString(TEXT(
		"When you type a message in chat, AIMod immediately posts a brief \"seen\" reply - independent of "
		"whether an external AI controller is actually watching yet, so you know your message registered. "
		"On by default; turn off if you find it noisy."));
	AutoAcknowledgeChatMessages->DefaultValue = true;
	AutoAcknowledgeChatMessages->Value = true;
	Section->SectionProperties.Add(TEXT("AutoAcknowledgeChatMessages"), AutoAcknowledgeChatMessages);

	// Off by default, player-opt-in only. There is no existing in-game
	// equivalent of "a player spawns a creature", so this is treated the
	// same as bUnlimitedResources: a capability an external AI controller
	// can never enable itself, only the player from this settings menu.
	// See UAIModFunctionLibrary::SpawnCreatureNearPlayer.
	UConfigPropertyBool* AllowCreatureSpawning = CastChecked<UConfigPropertyBool>(ObjectInitializer.CreateDefaultSubobject(Section, TEXT("AllowCreatureSpawning"), UConfigPropertyBool::StaticClass(), BoolClass, true, false));
	AllowCreatureSpawning->DisplayName = FText::FromString(TEXT("Allow Creature Spawning"));
	AllowCreatureSpawning->Tooltip = FText::FromString(TEXT(
		"Off by default. When enabled, an external AI controller can spawn real creatures near you on request "
		"via AIMod's RPC interface. Off means every such request is rejected regardless of what's asked."));
	AllowCreatureSpawning->DefaultValue = false;
	AllowCreatureSpawning->Value = false;
	Section->SectionProperties.Add(TEXT("AllowCreatureSpawning"), AllowCreatureSpawning);

	// Multiplayer chat safety, off-by-default player-opt-in, same character
	// as bUnlimitedResources / bAllowCreatureSpawning. By default,
	// world.chatHistory suppresses
	// chat messages typed by anyone OTHER than the host player (the game's
	// own FChatMessageStruct::bIsLocalPlayerMessage identifies the host's
	// messages in the host process, where AIMod runs), and the instant
	// auto-acknowledgment only reacts to the host. This means a guest in a
	// multiplayer session cannot issue instructions to an external AI
	// controller through in-game chat unless the HOST deliberately turns
	// this on from the settings menu - the AI controller can never enable
	// it via RPC. See UAIModFunctionLibrary::LogChatHistoryAsJson and
	// UAIModHttpServerSubsystem::HandlePlayerChatMessageAdded.
	UConfigPropertyBool* AllowNonHostChatMessages = CastChecked<UConfigPropertyBool>(ObjectInitializer.CreateDefaultSubobject(Section, TEXT("AllowNonHostChatMessages"), UConfigPropertyBool::StaticClass(), BoolClass, true, false));
	AllowNonHostChatMessages->DisplayName = FText::FromString(TEXT("Allow Other Players' Chat Messages"));
	AllowNonHostChatMessages->Tooltip = FText::FromString(TEXT(
		"Off by default. In a multiplayer session, only YOUR (the host's) chat messages are visible to an "
		"external AI controller through AIMod's chat interface, and only your messages get the instant "
		"\"seen\" acknowledgment. Enable this to let every connected player's chat messages through as well, "
		"so other players can also talk to the AI. Leave off if you don't want guests directing the AI."));
	AllowNonHostChatMessages->DefaultValue = false;
	AllowNonHostChatMessages->Value = false;
	Section->SectionProperties.Add(TEXT("AllowNonHostChatMessages"), AllowNonHostChatMessages);

	// Protects the deliberately-scarce alien artifacts (Somersloop =
	// Desc_WAT1, Mercer Sphere = Desc_WAT2, the
	// "/Prototype/WAT/" family). AIMod's item-injection RPCs create items from
	// nothing, so without a gate they could fabricate unlimited Somersloops /
	// Mercer Spheres, trivializing production amplification and the tech that
	// consumes spheres. This is INDEPENDENT of Unlimited Resources (that bypasses
	// build material COST; this protects specific unique items), so even with
	// Unlimited Resources on, adding these is rejected unless this is enabled.
	// Off by default (protected), player-opt-in only - an external AI controller
	// can never enable it. See UAIModFunctionLibrary::AddItemsToInventory /
	// AddItemsToPlayerInventory (IsProtectedAlienArtifactClass).
	UConfigPropertyBool* AllowSpawningAlienArtifacts = CastChecked<UConfigPropertyBool>(ObjectInitializer.CreateDefaultSubobject(Section, TEXT("AllowSpawningAlienArtifacts"), UConfigPropertyBool::StaticClass(), BoolClass, true, false));
	AllowSpawningAlienArtifacts->DisplayName = FText::FromString(TEXT("Allow Spawning Alien Artifacts (Somersloops / Mercer Spheres)"));
	AllowSpawningAlienArtifacts->Tooltip = FText::FromString(TEXT(
		"Off by default. Somersloops and Mercer Spheres are deliberately limited in the game. By default AIMod "
		"REFUSES to create them out of nothing via its item-injection RPCs, even when Unlimited Resources is on. "
		"Enable this only if you deliberately want the RPC to be able to fabricate these artifacts."));
	AllowSpawningAlienArtifacts->DefaultValue = false;
	AllowSpawningAlienArtifacts->Value = false;
	Section->SectionProperties.Add(TEXT("AllowSpawningAlienArtifacts"), AllowSpawningAlienArtifacts);

	// Creative-features master switch, public-release safety.
	// Off by default: gates the RPCs with no legitimate in-game equivalent
	// - free item injection, milestone-achievement re-fire, seasonal-event
	// forcing, and world/entity manipulation (space station height/phase,
	// Giant Flying Mantas, map hazard volumes, vehicle engine tuning). A
	// default install is telemetry + real-material-cost construction only,
	// so it doesn't hand players a cheat menu they didn't ask for. Enforced
	// centrally in UAIModHttpServerSubsystem::HandleRpcRequest (covers
	// batched sub-ops too). An external RPC caller can never enable this;
	// only the player, here. NOTE: this mod still affects achievement/save
	// integrity in general - disclose that on the mod page regardless.
	UConfigPropertyBool* AllowCreativeFeatures = CastChecked<UConfigPropertyBool>(ObjectInitializer.CreateDefaultSubobject(Section, TEXT("AllowCreativeFeatures"), UConfigPropertyBool::StaticClass(), BoolClass, true, false));
	AllowCreativeFeatures->DisplayName = FText::FromString(TEXT("Allow Creative Features (cheats)"));
	AllowCreativeFeatures->Tooltip = FText::FromString(TEXT(
		"Off by default. Enables the RPC commands that have no normal in-game equivalent and act as cheats: "
		"injecting items for free, re-firing milestone achievements, forcing seasonal (HUB party) events, and "
		"manipulating the world (moving/rebuilding the space station, spawning/controlling Giant Flying Mantas, "
		"disabling map boundary hazards, boosting vehicle speed). Using these can affect achievements and save "
		"integrity. Telemetry and normal, material-cost construction do NOT require this."));
	AllowCreativeFeatures->DefaultValue = false;
	AllowCreativeFeatures->Value = false;
	Section->SectionProperties.Add(TEXT("AllowCreativeFeatures"), AllowCreativeFeatures);
}
