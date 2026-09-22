// Copyright Epic Games, Inc. All Rights Reserved.

#include "AIModFunctionLibraryInternal.h"
#include "FGPhotoModeComponent.h"
#include "FGPlayerState.h"           // AFGPlayerState::GetPhotoModeComponent (canonical owner)
#include "UnrealClient.h"            // FScreenshotRequest
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"

using namespace AIModInternal;

// Photo mode / screenshot RPCs. These run on the game thread (the HTTP handler
// dispatches there), so direct game-object access is safe. FScreenshotRequest
// renders on the NEXT frame, so captureScreenshot returns the path immediately
// and the file lands a moment later - the caller reads it after a short delay.

namespace
{
	// The FactoryGame screenshots directory (same place F12 / Photo Mode use):
	// <ProjectSaved>/Screenshots/Windows. Forward slashes are JSON- and
	// Windows-safe, avoiding backslash-escaping bugs in ResultDetailJson.
	FString AIModScreenshotDir()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots"), TEXT("Windows"))
			.Replace(TEXT("\\"), TEXT("/"));
	}

	// The photo-mode component is OWNED BY THE PLAYER STATE (AFGPlayerState::
	// mPhotoModeComponent); the character's GetCachedPhotoModeComponent() is a
	// lazily-populated cache that is null until photo mode is first opened, so we
	// prefer the player state, then the static getter, then the cache.
	UFGPhotoModeComponent* AIModGetPhotoComp(UWorld* World, FString& OutError)
	{
		APlayerController* PC = World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
		AFGCharacterPlayer* Character = World ? Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(World, 0)) : nullptr;
		if (!PC && !Character)
		{
			OutError = TEXT("NO_PLAYER");
			return nullptr;
		}
		AFGPlayerState* PS = PC ? PC->GetPlayerState<AFGPlayerState>() : nullptr;
		if (!PS && Character) { PS = Character->GetPlayerState<AFGPlayerState>(); }
		if (PS && PS->GetPhotoModeComponent()) { return PS->GetPhotoModeComponent(); }
		if (PC) { if (UFGPhotoModeComponent* Comp = UFGPhotoModeComponent::GetUFGPhotoModeComponent(PC)) { return Comp; } }
		if (Character && Character->GetCachedPhotoModeComponent()) { return Character->GetCachedPhotoModeComponent(); }
		OutError = TEXT("NO_PHOTO_COMPONENT");
		return nullptr;
	}
}

FAIModOperationResult UAIModFunctionLibrary::CaptureScreenshot(UObject* WorldContextObject, bool bShowUI)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}
	const FString Dir = AIModScreenshotDir();
	IFileManager::Get().MakeDirectory(*Dir, true);
	const FString Path = FString::Printf(TEXT("%s/AIMod_%s.png"), *Dir, *FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));

	// bAddFilenameSuffix=false: use exactly this filename so we can report it.
	FScreenshotRequest::RequestScreenshot(Path, bShowUI, /*bAddFilenameSuffix*/ false);

	UE_LOG(LogAIModAI, Display, TEXT("CaptureScreenshot: requested -> %s (showUI=%s)"), *Path, bShowUI ? TEXT("true") : TEXT("false"));
	FAIModOperationResult Result = FAIModOperationResult::Success();
	Result.ResultDetailJson = FString::Printf(
		TEXT("{\"path\":\"%s\",\"note\":\"rendered next frame; read the file after a short delay\"}"), *Path);
	return Result;
}

FAIModOperationResult UAIModFunctionLibrary::SetPhotoModeEnabled(UObject* WorldContextObject, bool bEnabled)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}
	FString Err;
	UFGPhotoModeComponent* Comp = AIModGetPhotoComp(World, Err);
	if (!Comp)
	{
		return FAIModOperationResult::Failure(Err, Err == TEXT("NO_PLAYER")
			? TEXT("No local AFGCharacterPlayer (player index 0)")
			: TEXT("AFGCharacterPlayer::GetCachedPhotoModeComponent() returned null"));
	}
	if (bEnabled)
	{
		Comp->EnterPhotoMode();
	}
	else
	{
		Comp->ExitPhotoMode();
	}
	UE_LOG(LogAIModAI, Display, TEXT("SetPhotoModeEnabled(%s): photoModeOn=%s"),
		bEnabled ? TEXT("true") : TEXT("false"), Comp->GetIsPhotoModeOn() ? TEXT("true") : TEXT("false"));
	FAIModOperationResult Result = FAIModOperationResult::Success();
	Result.ResultDetailJson = FString::Printf(TEXT("{\"photoModeOn\":%s}"),
		Comp->GetIsPhotoModeOn() ? TEXT("true") : TEXT("false"));
	return Result;
}

FAIModOperationResult UAIModFunctionLibrary::SetPhotoCamera(UObject* WorldContextObject, float X, float Y, float Z, float Pitch, float Yaw)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}
	FString Err;
	UFGPhotoModeComponent* Comp = AIModGetPhotoComp(World, Err);
	if (!Comp)
	{
		return FAIModOperationResult::Failure(Err, Err == TEXT("NO_PLAYER")
			? TEXT("No local AFGCharacterPlayer (player index 0)")
			: TEXT("AFGCharacterPlayer::GetCachedPhotoModeComponent() returned null"));
	}
	if (!Comp->GetIsPhotoModeOn())
	{
		Comp->EnterPhotoMode();
	}
	Comp->SetPhotoCameraMode(EPhotoCameraMode::PCM_Decoupled);
	// Raise the decoupled-camera distance clamps so the camera can frame a large
	// build well away from the player (defaults are only ~3000/4000 units).
	Comp->mDCMoveDistanceLimit = 1.0e9f;
	Comp->mDCCutoffDistanceLimit = 1.0e9f;

	AFGPhotoModeCamera* Cam = Comp->GetPhotoModeCameraCharacter();
	if (!Cam)
	{
		return FAIModOperationResult::Failure(TEXT("NO_PHOTO_CAMERA"),
			TEXT("Decoupled photo camera not available yet - call world.enterPhotoMode first, then retry."));
	}
	const bool bMoved = Cam->SetActorLocationAndRotation(
		FVector(X, Y, Z), FRotator(Pitch, Yaw, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
	Cam->mPreviousCameraLocation = FVector(X, Y, Z);
	Cam->mPreviousCameraRotation = FRotator(Pitch, Yaw, 0.0f);

	UE_LOG(LogAIModAI, Display, TEXT("SetPhotoCamera: (%.0f,%.0f,%.0f) pitch=%.1f yaw=%.1f moved=%s"),
		X, Y, Z, Pitch, Yaw, bMoved ? TEXT("true") : TEXT("false"));
	FAIModOperationResult Result = FAIModOperationResult::Success();
	Result.ResultDetailJson = FString::Printf(TEXT("{\"decoupled\":true,\"moved\":%s}"), bMoved ? TEXT("true") : TEXT("false"));
	return Result;
}

FAIModOperationResult UAIModFunctionLibrary::TakePhoto(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return FAIModOperationResult::Failure(TEXT("INTERNAL_ERROR"), TEXT("No valid world context"));
	}
	FString Err;
	UFGPhotoModeComponent* Comp = AIModGetPhotoComp(World, Err);
	if (!Comp)
	{
		return FAIModOperationResult::Failure(Err, Err == TEXT("NO_PLAYER")
			? TEXT("No local AFGCharacterPlayer (player index 0)")
			: TEXT("AFGCharacterPlayer::GetCachedPhotoModeComponent() returned null"));
	}
	Comp->TakePhoto();
	const FString Dir = AIModScreenshotDir();
	UE_LOG(LogAIModAI, Display, TEXT("TakePhoto: requested -> %s"), *Dir);
	FAIModOperationResult Result = FAIModOperationResult::Success();
	Result.ResultDetailJson = FString::Printf(
		TEXT("{\"dir\":\"%s\",\"note\":\"high-res capture rendered next frame; newest file in dir is the photo\"}"), *Dir);
	return Result;
}
