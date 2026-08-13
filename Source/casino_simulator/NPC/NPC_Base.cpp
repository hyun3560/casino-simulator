// Copyright Epic Games, Inc. All Rights Reserved.

#include "NPC_Base.h"
#include "Camera/CameraComponent.h"
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

	InteractionCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("InteractionCamera"));
	InteractionCameraComponent->SetupAttachment(GetCapsuleComponent());
	InteractionCameraComponent->SetRelativeLocation(FVector(180.0f, -180.0f, 90.0f));
	InteractionCameraComponent->SetRelativeRotation(FRotator(0.0f, 135.0f, 0.0f));
	InteractionCameraComponent->bAutoActivate = true;
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

AActor* ANPC_Base::GetInteractionCameraTarget() const
{
	return InteractionCameraTarget ? InteractionCameraTarget.Get() : const_cast<ANPC_Base*>(this);
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
