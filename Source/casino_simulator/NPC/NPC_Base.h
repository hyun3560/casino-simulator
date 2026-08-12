// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Engine/HitResult.h"
#include "NPC_Base.generated.h"

class USphereComponent;
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

public:

	ANPC_Base();

	/** Returns the interaction/detection sphere component **/
	USphereComponent* GetInteractionSphere() const { return InteractionSphere; }

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
};
