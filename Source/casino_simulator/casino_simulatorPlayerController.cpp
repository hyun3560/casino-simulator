// Copyright Epic Games, Inc. All Rights Reserved.


#include "casino_simulatorPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "casino_simulatorCameraManager.h"
#include "Blueprint/UserWidget.h"
#include "casino_simulator.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "UI/casino_simulatorPlayerHUD.h"
#include "casino_simulatorPlayerState.h"
#include "casino_simulatorAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"

Acasino_simulatorPlayerController::Acasino_simulatorPlayerController()
{
	// set the player camera manager class
	PlayerCameraManagerClass = Acasino_simulatorCameraManager::StaticClass();
}

void Acasino_simulatorPlayerController::BeginPlay()
{
	Super::BeginPlay();

	
	// only spawn touch controls on local player controllers
	if (ShouldUseTouchControls() && IsLocalPlayerController())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(Logcasino_simulator, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}

	// only spawn the player HUD on local player controllers
	if (IsLocalPlayerController() && PlayerHUDWidgetClass)
	{
		PlayerHUDWidget = CreateWidget<Ucasino_simulatorPlayerHUD>(this, PlayerHUDWidgetClass);

		if (PlayerHUDWidget)
		{
			PlayerHUDWidget->AddToPlayerScreen(100);

			BindToPlayerState(GetPlayerState<Acasino_simulatorPlayerState>());
		}
		else
		{
			UE_LOG(Logcasino_simulator, Error, TEXT("Could not spawn player HUD widget."));
		}

		// keep the HUD bound to whichever pawn we currently/next possess
		OnPossessedPawnChanged.AddDynamic(this, &Acasino_simulatorPlayerController::HandlePossessedPawnChanged);

		if (APawn* CurrentPawn = GetPawn())
		{
			HandlePossessedPawnChanged(nullptr, CurrentPawn);
		}
	}
}

void Acasino_simulatorPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindFromAbilitySystem();
	BindToPlayerState(nullptr);

	Super::EndPlay(EndPlayReason);
}

void Acasino_simulatorPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// PlayerState may have been null at BeginPlay on remote clients; this fires once it actually arrives.
	if (PlayerHUDWidget)
	{
		BindToPlayerState(GetPlayerState<Acasino_simulatorPlayerState>());
	}
}

void Acasino_simulatorPlayerController::BindToPlayerState(Acasino_simulatorPlayerState* NewPlayerState)
{
	if (BoundPlayerState == NewPlayerState)
	{
		return;
	}

	if (BoundPlayerState)
	{
		BoundPlayerState->OnInventoryChanged.RemoveDynamic(this, &Acasino_simulatorPlayerController::RefreshInventorySlotCounts);
	}

	BoundPlayerState = NewPlayerState;

	if (BoundPlayerState)
	{
		BoundPlayerState->OnInventoryChanged.AddDynamic(this, &Acasino_simulatorPlayerController::RefreshInventorySlotCounts);
	}

	RefreshInventorySlotCounts();
}

void Acasino_simulatorPlayerController::RefreshInventorySlotCounts()
{
	if (!PlayerHUDWidget)
	{
		return;
	}

	if (BoundPlayerState && BoundPlayerState->NumberSlots.Num() >= 2)
	{
		PlayerHUDWidget->BP_Slot_1Count(BoundPlayerState->GetItemQuantity(BoundPlayerState->NumberSlots[0]));
		PlayerHUDWidget->BP_Slot_2Count(BoundPlayerState->GetItemQuantity(BoundPlayerState->NumberSlots[1]));
	}
	else
	{
		PlayerHUDWidget->BP_Slot_1Count(0);
		PlayerHUDWidget->BP_Slot_2Count(0);
	}
}

void Acasino_simulatorPlayerController::HandlePossessedPawnChanged(APawn* PreviousPawn, APawn* NewPawn)
{
	UnbindFromAbilitySystem();

	if (const IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(NewPawn))
	{
		BindToAbilitySystem(AbilitySystemInterface->GetAbilitySystemComponent());
	}
}

void Acasino_simulatorPlayerController::BindToAbilitySystem(UAbilitySystemComponent* AbilitySystemComponent)
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	BoundAbilitySystemComponent = AbilitySystemComponent;

	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(Ucasino_simulatorAttributeSet::GetNicotineAttribute())
		.AddUObject(this, &Acasino_simulatorPlayerController::OnNicotineChanged);
	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(Ucasino_simulatorAttributeSet::GetAlcoholAttribute())
		.AddUObject(this, &Acasino_simulatorPlayerController::OnAlcoholChanged);
	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(Ucasino_simulatorAttributeSet::GetCurrencyAttribute())
		.AddUObject(this, &Acasino_simulatorPlayerController::OnCurrencyChanged);

	// push the current values immediately so the bars/text aren't left stale until the next change
	if (const Ucasino_simulatorAttributeSet* Attributes = AbilitySystemComponent->GetSet<Ucasino_simulatorAttributeSet>())
	{
		if (PlayerHUDWidget)
		{
			PlayerHUDWidget->BP_NicotineUpdated(Attributes->GetNicotine(), Attributes->GetMaxNicotine());
			PlayerHUDWidget->BP_AlcoholUpdated(Attributes->GetAlcohol(), Attributes->GetMaxAlcohol());
			PlayerHUDWidget->BP_CurrencyUpdated(Attributes->GetCurrency());
		}
	}
}

void Acasino_simulatorPlayerController::UnbindFromAbilitySystem()
{
	if (BoundAbilitySystemComponent)
	{
		BoundAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(Ucasino_simulatorAttributeSet::GetNicotineAttribute()).RemoveAll(this);
		BoundAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(Ucasino_simulatorAttributeSet::GetAlcoholAttribute()).RemoveAll(this);
		BoundAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(Ucasino_simulatorAttributeSet::GetCurrencyAttribute()).RemoveAll(this);
		BoundAbilitySystemComponent = nullptr;
	}
}

void Acasino_simulatorPlayerController::OnNicotineChanged(const FOnAttributeChangeData& Data)
{
	if (PlayerHUDWidget && BoundAbilitySystemComponent)
	{
		if (const Ucasino_simulatorAttributeSet* Attributes = BoundAbilitySystemComponent->GetSet<Ucasino_simulatorAttributeSet>())
		{
			PlayerHUDWidget->BP_NicotineUpdated(Data.NewValue, Attributes->GetMaxNicotine());
		}
	}
}

void Acasino_simulatorPlayerController::OnAlcoholChanged(const FOnAttributeChangeData& Data)
{
	if (PlayerHUDWidget && BoundAbilitySystemComponent)
	{
		if (const Ucasino_simulatorAttributeSet* Attributes = BoundAbilitySystemComponent->GetSet<Ucasino_simulatorAttributeSet>())
		{
			PlayerHUDWidget->BP_AlcoholUpdated(Data.NewValue, Attributes->GetMaxAlcohol());
		}
	}
}

void Acasino_simulatorPlayerController::OnCurrencyChanged(const FOnAttributeChangeData& Data)
{
	if (PlayerHUDWidget)
	{
		PlayerHUDWidget->BP_CurrencyUpdated(Data.NewValue);
	}
}

void Acasino_simulatorPlayerController::OpenInteraction()
{
	PlayerHUDWidget->BP_OpenInterection();
}

void Acasino_simulatorPlayerController::CloseInteraction()
{
	PlayerHUDWidget->BP_CloseInterection();
}

void Acasino_simulatorPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Context
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}
	
}

bool Acasino_simulatorPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}
