#include "Interaction/WorldInteractableBase.h"

#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Interaction/WorldInteractionDetectorComponent.h"
#include "casino_simulatorCharacter.h"

AWorldInteractableBase::AWorldInteractableBase()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(SceneRoot);
	InteractionSphere->InitSphereRadius(500.0f);
	InteractionSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));

	InteractionPromptText = FText::FromString(TEXT("E Use"));
}

void AWorldInteractableBase::BeginPlay()
{
	Super::BeginPlay();

	if (InteractionSphere)
	{
		InteractionSphere->OnComponentBeginOverlap.AddDynamic(this, &AWorldInteractableBase::OnInteractionSphereBeginOverlap);
		InteractionSphere->OnComponentEndOverlap.AddDynamic(this, &AWorldInteractableBase::OnInteractionSphereEndOverlap);
	}
}

void AWorldInteractableBase::Interact(Acasino_simulatorCharacter* InteractingCharacter)
{
}

bool AWorldInteractableBase::CanInteract(Acasino_simulatorCharacter* InteractingCharacter) const
{
	if (!InteractingCharacter)
	{
		return false;
	}

	const FVector ToTarget = GetActorLocation() - InteractingCharacter->GetActorLocation();
	const float MaxDistance = InteractionSphere ? InteractionSphere->GetScaledSphereRadius() + 50.0f : 0.0f;
	if (ToTarget.SizeSquared() > FMath::Square(MaxDistance))
	{
		return false;
	}

	const FVector DirectionToTarget = ToTarget.GetSafeNormal();
	const float FacingDot = FVector::DotProduct(InteractingCharacter->GetActorForwardVector(), DirectionToTarget);
	return FacingDot >= InteractionFacingDotThreshold;
}

void AWorldInteractableBase::OnInteractionSphereBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (Acasino_simulatorCharacter* PlayerCharacter = Cast<Acasino_simulatorCharacter>(OtherActor))
	{
		if (UWorldInteractionDetectorComponent* Detector = PlayerCharacter->GetWorldInteractionDetector())
		{
			Detector->RegisterCandidate(this);
		}
	}
}

void AWorldInteractableBase::OnInteractionSphereEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	if (Acasino_simulatorCharacter* PlayerCharacter = Cast<Acasino_simulatorCharacter>(OtherActor))
	{
		if (UWorldInteractionDetectorComponent* Detector = PlayerCharacter->GetWorldInteractionDetector())
		{
			Detector->UnregisterCandidate(this);
		}
	}
}
