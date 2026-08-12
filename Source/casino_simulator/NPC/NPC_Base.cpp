// Copyright Epic Games, Inc. All Rights Reserved.

#include "NPC_Base.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "casino_simulatorCharacter.h"
#include "casino_simulatorPlayerController.h"

ANPC_Base::ANPC_Base()
{
	// Pure overlap detection (interaction range, aggro range, etc.) - doesn't block movement/physics.
	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(GetCapsuleComponent());
	InteractionSphere->InitSphereRadius(150.0f);
	InteractionSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
}

void ANPC_Base::BeginPlay()
{
	Super::BeginPlay();

	if (InteractionSphere)
	{
		InteractionSphere->OnComponentBeginOverlap.AddDynamic(this, &ANPC_Base::OnInteractionSphereBeginOverlap);
		InteractionSphere->OnComponentEndOverlap.AddDynamic(this, &ANPC_Base::OnInteractionSphereEndOverlap);
	}
}

void ANPC_Base::Interact(Acasino_simulatorCharacter* InteractingCharacter)
{
	if (!InteractingCharacter)
	{
		return;
	}

	BP_OnInteract(InteractingCharacter);
}

void ANPC_Base::OnInteractionSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// Only the player character (Acasino_simulatorCharacter) opens the interaction - other NPCs/objects overlapping the sphere are ignored.
	if (Acasino_simulatorCharacter* PlayerCharacter = Cast<Acasino_simulatorCharacter>(OtherActor))
	{
		if (Acasino_simulatorPlayerController* PlayerController = Cast<Acasino_simulatorPlayerController>(PlayerCharacter->GetController()))
		{
			PlayerController->SetInteractionTarget(this);
		}
	}
}

void ANPC_Base::OnInteractionSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (Acasino_simulatorCharacter* PlayerCharacter = Cast<Acasino_simulatorCharacter>(OtherActor))
	{
		if (Acasino_simulatorPlayerController* PlayerController = Cast<Acasino_simulatorPlayerController>(PlayerCharacter->GetController()))
		{
			PlayerController->ClearInteractionTarget(this);
		}
	}
}
