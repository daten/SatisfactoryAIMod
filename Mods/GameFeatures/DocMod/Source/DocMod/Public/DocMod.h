// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogDocModAI, Log, All);

struct FActorsInitializedParams;

class FDocModModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
#if !UE_BUILD_SHIPPING
	/** Runs DocModSelfTest::RunAll for every loaded game world. See DocModSelfTest.h. */
	void OnWorldInitializedActors(const FActorsInitializedParams& Params);

	FDelegateHandle WorldInitializedActorsHandle;
#endif
};
