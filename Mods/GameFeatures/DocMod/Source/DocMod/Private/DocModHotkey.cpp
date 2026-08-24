// Copyright Epic Games, Inc. All Rights Reserved.

#include "DocModHotkey.h"
#include "DocMod.h"
#include "DocModFunctionLibrary.h"
#include "DocModOperationTypes.h"
#include "DocModTelemetryTypes.h"
#include "FGPlayerController.h"
#include "Player/SMLRemoteCallObject.h"
#include "Kismet/GameplayStatics.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"

namespace DocModHotkey
{
	namespace
	{
		// Held for the lifetime of the module via TStrongObjectPtr, not a
		// UPROPERTY, since this namespace isn't a UObject - built once via
		// NewObject<>() at runtime and reused across world loads, no
		// Content/Blueprint asset needed (see this file's header comment).
		TStrongObjectPtr<UInputAction> GHotkeyAction;
		TStrongObjectPtr<UInputMappingContext> GHotkeyMappingContext;

		void SendChat(AFGPlayerController* PlayerController, const FString& Message, const FLinearColor& Color)
		{
			if (!PlayerController)
			{
				return;
			}
			// Same underlying call SML's own chat commands use
			// (UPlayerCommandSender::SendChatMessage), reached directly
			// since a keypress has no UCommandSender to go through.
			if (USMLRemoteCallObject* RemoteCallObject = PlayerController->GetRemoteCallObjectOfClass<USMLRemoteCallObject>())
			{
				RemoteCallObject->SendChatMessage(Message, Color);
			}
		}

		void OnHotkeyPressed(TWeakObjectPtr<AFGPlayerController> WeakPlayerController)
		{
			AFGPlayerController* PlayerController = WeakPlayerController.Get();
			if (!PlayerController)
			{
				return;
			}
			UWorld* World = PlayerController->GetWorld();
			if (!World)
			{
				return;
			}

			const FDocModManufacturerTelemetry Target = UDocModFunctionLibrary::GetTargetedManufacturer(World);
			if (Target.Id.IsEmpty())
			{
				UE_LOG(LogDocModAI, Display, TEXT("DocMod hotkey: not currently looking at a manufacturer"));
				SendChat(PlayerController, TEXT("DocMod: not looking at a manufacturer"), FLinearColor::Yellow);
				return;
			}

			UE_LOG(LogDocModAI, Display, TEXT("DocMod hotkey: targeted %s recipe=\"%s\" clock=%.0f%% status=%s id=%s"),
				*Target.BuildableClass, *Target.Recipe, Target.ClockSpeedPercent, *Target.ProductionStatus, *Target.Id);
			SendChat(PlayerController, FString::Printf(TEXT("DocMod: targeting %s (%.0f%%)"), *Target.BuildableClass, Target.ClockSpeedPercent), FLinearColor::White);

			// Test the write-operation path end-to-end: nudge clock speed
			// up by 10 percentage points. This IS a real mutation - see
			// this file's header comment on why the setup call site is
			// Shipping-gated.
			const float NewClockSpeedPercent = Target.ClockSpeedPercent + 10.0f;
			const FDocModOperationResult Result = UDocModFunctionLibrary::SetManufacturerClockSpeed(World, Target.Id, NewClockSpeedPercent);

			if (Result.bSuccess)
			{
				UE_LOG(LogDocModAI, Display, TEXT("DocMod hotkey: clock speed change succeeded, %.0f%% -> %.0f%% (takes effect next production cycle)"),
					Target.ClockSpeedPercent, NewClockSpeedPercent);
				SendChat(PlayerController, FString::Printf(TEXT("DocMod: clock speed set to %.0f%% - success"), NewClockSpeedPercent), FLinearColor::Green);
			}
			else
			{
				UE_LOG(LogDocModAI, Warning, TEXT("DocMod hotkey: clock speed change failed - %s: %s"), *Result.ErrorCode, *Result.ErrorMessage);
				SendChat(PlayerController, FString::Printf(TEXT("DocMod: clock speed change failed - %s"), *Result.ErrorMessage), FLinearColor::Red);
			}
		}
	}

	void SetupForWorld(UWorld* World)
	{
		AFGPlayerController* PlayerController = World ? Cast<AFGPlayerController>(UGameplayStatics::GetPlayerController(World, 0)) : nullptr;
		if (!PlayerController)
		{
			UE_LOG(LogDocModAI, Warning, TEXT("DocModHotkey::SetupForWorld: no local AFGPlayerController"));
			return;
		}

		UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerController->InputComponent);
		if (!EnhancedInputComponent)
		{
			UE_LOG(LogDocModAI, Warning, TEXT("DocModHotkey::SetupForWorld: no UEnhancedInputComponent on the local player controller yet"));
			return;
		}

		if (!GHotkeyAction.IsValid())
		{
			GHotkeyAction = TStrongObjectPtr<UInputAction>(NewObject<UInputAction>());
			GHotkeyMappingContext = TStrongObjectPtr<UInputMappingContext>(NewObject<UInputMappingContext>());
			GHotkeyMappingContext->MapKey(GHotkeyAction.Get(), EKeys::F11);
		}

		PlayerController->AddMappingContextImmediately(GHotkeyMappingContext.Get());

		TWeakObjectPtr<AFGPlayerController> WeakPlayerController = PlayerController;
		EnhancedInputComponent->BindActionValueLambda(GHotkeyAction.Get(), ETriggerEvent::Started,
			[WeakPlayerController](const FInputActionValue&)
			{
				OnHotkeyPressed(WeakPlayerController);
			});

		UE_LOG(LogDocModAI, Display, TEXT("DocMod hotkey bound: F11 -> print targeted manufacturer + test clock speed change"));
	}
}
