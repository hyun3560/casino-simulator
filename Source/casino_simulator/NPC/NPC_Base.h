// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Engine/HitResult.h"
#include "NPC_Base.generated.h"

class USphereComponent;
class UCameraComponent;
class UPrimitiveComponent;
class Acasino_simulatorCharacter;

/**
 *  Base class for all NPCs. Plain ACharacter (no GAS/player-specific setup) with a sphere
 *  collision for interaction/detection use.
 */
UCLASS(abstract)
class CASINO_SIMULATOR_API ANPC_Base : public ACharacter
{
	GENERATED_BODY()

	/** Sphere collision used for interaction/detection (e.g. player walking into range). Attached to the capsule root. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USphereComponent* InteractionSphere;

	/** Camera used by default while this NPC owns an interaction UI. Adjust it per NPC Blueprint. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* InteractionCameraComponent;

	/** Optional external view actor override. Leave empty to use this NPC's InteractionCameraComponent. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Interaction|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AActor> InteractionCameraTarget;

public:

	ANPC_Base();

	/** Returns the interaction/detection sphere component **/
	USphereComponent* GetInteractionSphere() const { return InteractionSphere; }

	/** Returns the camera component used by this NPC's interaction view **/
	UCameraComponent* GetInteractionCameraComponent() const { return InteractionCameraComponent; }

	UFUNCTION(BlueprintPure, Category="Interaction|Camera")
	AActor* GetInteractionCameraTarget() const;

	UFUNCTION(BlueprintCallable, Category="Interaction")
	virtual void Interact(Acasino_simulatorCharacter* InteractingCharacter);

protected:

	//~ Begin AActor interface
	virtual void BeginPlay() override;
	//~ End AActor interface

	/** Bound to InteractionSphere's begin-overlap event */
	UFUNCTION()
	void OnInteractionSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** Bound to InteractionSphere's end-overlap event */
	UFUNCTION()
	void OnInteractionSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UFUNCTION(BlueprintImplementableEvent, Category="Interaction", meta=(DisplayName="On Interact"))
	void BP_OnInteract(Acasino_simulatorCharacter* InteractingCharacter);
};
