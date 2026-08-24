// Copyright Epic Games, Inc. All Rights Reserved.

#include "DocModFunctionLibrary.h"
#include "DocMod.h"

FString UDocModFunctionLibrary::GetInterfaceVersion()
{
	UE_LOG(LogDocModAI, Verbose, TEXT("GetInterfaceVersion called"));
	return TEXT("0.1.0");
}
