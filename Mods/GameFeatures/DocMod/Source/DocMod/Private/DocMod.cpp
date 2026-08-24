// Copyright Epic Games, Inc. All Rights Reserved.

#include "DocMod.h"

#if !UE_BUILD_SHIPPING
#include "DocModSelfTest.h"
#include "Engine/World.h"
#endif

DEFINE_LOG_CATEGORY(LogDocModAI);

#define LOCTEXT_NAMESPACE "FDocModModule"

void FDocModModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	UE_LOG(LogDocModAI, Display, TEXT("DocMod AI interface module initialized"));

#if !UE_BUILD_SHIPPING
	// Development-time convenience only (see DocModSelfTest.h) - compiled
	// out of Shipping entirely, not just disabled at runtime, so it can
	// never run for players of a released mod.
	WorldInitializedActorsHandle = FWorldDelegates::OnWorldInitializedActors.AddRaw(this, &FDocModModule::OnWorldInitializedActors);
#endif
}

void FDocModModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
#if !UE_BUILD_SHIPPING
	FWorldDelegates::OnWorldInitializedActors.Remove(WorldInitializedActorsHandle);
#endif

	UE_LOG(LogDocModAI, Display, TEXT("DocMod AI interface module shutting down"));
}

#if !UE_BUILD_SHIPPING
void FDocModModule::OnWorldInitializedActors(const FActorsInitializedParams& Params)
{
	// Fires for every world load, including menu/editor-preview worlds -
	// skip anything that isn't an actual game world. DocModSelfTest's
	// checks all tolerate "0 found" gracefully, so this is safe to run
	// even on a bare level with no resource nodes/buildings.
	if (Params.World && Params.World->IsGameWorld())
	{
		DocModSelfTest::RunAll(Params.World);
	}
}
#endif

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FDocModModule, DocMod)