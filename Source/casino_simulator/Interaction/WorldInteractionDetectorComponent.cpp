#include "Interaction/WorldInteractionDetectorComponent.h"

#include "Interaction/WorldInteractableBase.h"
#include "casino_simulatorCharacter.h"
#include "casino_simulatorPlayerController.h"

UWorldInteractionDetectorComponent::UWorldInteractionDetectorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UWorldInteractionDetectorComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerCharacter = Cast<Acasino_simulatorCharacter>(GetOwner());
}

void UWorldInteractionDetectorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CloseWorldPrompt();
	NearbyTargets.Reset();
	FocusedTarget = nullptr;

	Super::EndPlay(EndPlayReason);
}

void UWorldInteractionDetectorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!OwnerCharacter || !OwnerCharacter->IsLocallyControlled())
	{
		return;
	}

	UpdateFocusedTarget();
}

void UWorldInteractionDetectorComponent::RegisterCandidate(AWorldInteractableBase* Candidate)
{
	if (!Candidate)
	{
		return;
	}

	NearbyTargets.AddUnique(Candidate);
}

void UWorldInteractionDetectorComponent::UnregisterCandidate(AWorldInteractableBase* Candidate)
{
	if (!Candidate)
	{
		return;
	}

	NearbyTargets.Remove(Candidate);

	if (FocusedTarget == Candidate)
	{
		SetFocusedTarget(nullptr);
	}
}

bool UWorldInteractionDetectorComponent::TryInteract()
{
	if (!OwnerCharacter || !FocusedTarget || !FocusedTarget->CanInteract(OwnerCharacter))
	{
		return false;
	}

	CloseWorldPrompt();
	FocusedTarget->Interact(OwnerCharacter);
	return true;
}

void UWorldInteractionDetectorComponent::UpdateFocusedTarget()
{
	if (!OwnerCharacter)
	{
		SetFocusedTarget(nullptr);
		return;
	}

	NearbyTargets.RemoveAll([](const AWorldInteractableBase* Candidate)
	{
		return !IsValid(Candidate);
	});

	AWorldInteractableBase* BestTarget = nullptr;
	float BestDot = -1.0f;

	const FVector OwnerLocation = OwnerCharacter->GetActorLocation();
	const FVector OwnerForward = OwnerCharacter->GetActorForwardVector();

	for (AWorldInteractableBase* Candidate : NearbyTargets)
	{
		if (!Candidate || !Candidate->CanInteract(OwnerCharacter))
		{
			continue;
		}

		const FVector DirectionToCandidate = (Candidate->GetActorLocation() - OwnerLocation).GetSafeNormal();
		const float CandidateDot = FVector::DotProduct(OwnerForward, DirectionToCandidate);
		if (CandidateDot > BestDot)
		{
			BestDot = CandidateDot;
			BestTarget = Candidate;
		}
	}

	SetFocusedTarget(BestTarget);
}

void UWorldInteractionDetectorComponent::SetFocusedTarget(AWorldInteractableBase* NewFocusedTarget)
{
	if (FocusedTarget == NewFocusedTarget)
	{
		if (FocusedTarget && !bWorldPromptOpen)
		{
			if (Acasino_simulatorPlayerController* PlayerController = OwnerCharacter
				? Cast<Acasino_simulatorPlayerController>(OwnerCharacter->GetController())
				: nullptr)
			{
				bWorldPromptOpen = PlayerController->OpenWorldInteraction(FocusedTarget->GetInteractionPromptText());
			}
		}
		return;
	}

	FocusedTarget = NewFocusedTarget;

	Acasino_simulatorPlayerController* PlayerController = OwnerCharacter
		? Cast<Acasino_simulatorPlayerController>(OwnerCharacter->GetController())
		: nullptr;
	if (!PlayerController)
	{
		return;
	}

	if (FocusedTarget)
	{
		bWorldPromptOpen = PlayerController->OpenWorldInteraction(FocusedTarget->GetInteractionPromptText());
	}
	else
	{
		CloseWorldPrompt();
	}
}

void UWorldInteractionDetectorComponent::CloseWorldPrompt()
{
	if (!bWorldPromptOpen || !OwnerCharacter)
	{
		return;
	}

	if (Acasino_simulatorPlayerController* PlayerController = Cast<Acasino_simulatorPlayerController>(OwnerCharacter->GetController()))
	{
		PlayerController->CloseWorldInteraction();
	}

	bWorldPromptOpen = false;
}
