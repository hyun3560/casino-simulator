// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "casino_simulatorPlayerHUD.generated.h"

/**
 *  Player HUD widget: displays the player's nicotine/alcohol levels.
 *  Kept passive - it doesn't source its own data. Acasino_simulatorPlayerController pushes
 *  Current/Max updates into it whenever the underlying attributes change; Blueprint is
 *  responsible for turning that into a ProgressBar Percent (CurrentValue / MaxValue).
 */
UCLASS(abstract)
class CASINO_SIMULATOR_API Ucasino_simulatorPlayerHUD : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Passes control to Blueprint to update the nicotine progress bar */
	UFUNCTION(BlueprintImplementableEvent, Category="HUD", meta = (DisplayName = "Nicotine Updated"))
	void BP_NicotineUpdated(float CurrentValue, float MaxValue);

	/** Passes control to Blueprint to update the alcohol progress bar */
	UFUNCTION(BlueprintImplementableEvent, Category="HUD", meta = (DisplayName = "Alcohol Updated"))
	void BP_AlcoholUpdated(float CurrentValue, float MaxValue);
};
