// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "AbilitySystemInterface.h"
#include "casino_simulatorCharacter.generated.h"

class UInputComponent;
class USkeletalMeshComponent;
class UCameraComponent;
class UInputAction;
class UAbilitySystemComponent;
class Ucasino_simulatorAttributeSet;
class UGameplayEffect;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A basic first person character
 */
UCLASS(abstract)
class Acasino_simulatorCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

	/** Pawn mesh: first person view (arms; seen only by self) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* FirstPersonMesh;

	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCameraComponent;

protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* MouseLookAction;

	/** Ability system component driving this character's abilities/attributes/effects */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Abilities", meta = (AllowPrivateAccess = "true"))
	UAbilitySystemComponent* AbilitySystemComponent;

	/** Attribute set holding this character's nicotine/alcohol intoxication levels */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Abilities", meta = (AllowPrivateAccess = "true"))
	Ucasino_simulatorAttributeSet* AttributeSet;

	/** GameplayEffect (typically a Blueprint) applied once, server-side, to set starting Nicotine/Alcohol values */
	UPROPERTY(EditDefaultsOnly, Category="Abilities", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> InitialAttributesEffectClass;

public:
	Acasino_simulatorCharacter();

	//~ Begin IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	//~ End IAbilitySystemInterface

	/** Returns the attribute set holding nicotine/alcohol levels **/
	Ucasino_simulatorAttributeSet* GetAttributeSet() const { return AttributeSet; }

protected:

	//~ Begin AActor interface
	virtual void PossessedBy(AController* NewController) override;
	//~ End AActor interface

	//~ Begin APawn interface
	virtual void OnRep_PlayerState() override;
	//~ End APawn interface

	/** Applies InitialAttributesEffectClass to this character's own ability system. Server-only; call after InitAbilityActorInfo. */
	void InitializeDefaultAttributes() const;

	/** Called from Input Actions for movement input */
	void MoveInput(const FInputActionValue& Value);

	/** Called from Input Actions for looking input */
	void LookInput(const FInputActionValue& Value);

	/** Handles aim inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoAim(float Yaw, float Pitch);

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles jump start inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump end inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

protected:

	/** Set up input action bindings */
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
	

public:

	/** Returns the first person mesh **/
	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }

	/** Returns first person camera component **/
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }

};

