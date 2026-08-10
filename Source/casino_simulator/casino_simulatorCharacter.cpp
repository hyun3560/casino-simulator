// Copyright Epic Games, Inc. All Rights Reserved.

#include "casino_simulatorCharacter.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AbilitySystemComponent.h"
#include "Economy/CasinoShopComponent.h"
#include "casino_simulatorAttributeSet.h"
#include "casino_simulator.h"

Acasino_simulatorCharacter::Acasino_simulatorCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);
	
	// Create the first person mesh that will be viewed only by this character's owner
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));

	FirstPersonMesh->SetupAttachment(GetMesh());
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));

	// Create the Camera Component	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("First Person Camera"));
	FirstPersonCameraComponent->SetupAttachment(FirstPersonMesh, FName("head"));
	FirstPersonCameraComponent->SetRelativeLocationAndRotation(FVector(-2.8f, 5.89f, 0.0f), FRotator(0.0f, 90.0f, -90.0f));
	FirstPersonCameraComponent->bUsePawnControlRotation = true;
	FirstPersonCameraComponent->bEnableFirstPersonFieldOfView = true;
	FirstPersonCameraComponent->bEnableFirstPersonScale = true;
	FirstPersonCameraComponent->FirstPersonFieldOfView = 70.0f;
	FirstPersonCameraComponent->FirstPersonScale = 0.6f;

	// configure the character comps
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;

	GetCapsuleComponent()->SetCapsuleSize(34.0f, 96.0f);

	// Configure character movement
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;
	GetCharacterMovement()->AirControl = 0.5f;

	// Create the ability system component. Attributes/abilities/effects are replicated
	// via the ASC itself, so the actor doesn't need to replicate it separately.
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	// Created as a subobject of this actor so the ASC (also owned by this actor) auto-discovers
	// it when InitAbilityActorInfo runs.
	AttributeSet = CreateDefaultSubobject<Ucasino_simulatorAttributeSet>(TEXT("AttributeSet"));

	ShopComponent = CreateDefaultSubobject<UCasinoShopComponent>(TEXT("ShopComponent"));
}

UAbilitySystemComponent* Acasino_simulatorCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void Acasino_simulatorCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// Server-side init. The ASC lives on this character, so it is both owner and avatar.
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);

		// Set starting attribute values. GameplayEffects should only ever be applied on the
		// authority - clients pick the result up through normal attribute replication.
		if (HasAuthority())
		{
			InitializeDefaultAttributes();
			ApplyAttributeDecayEffect();
		}

		// Every machine (server and each client) needs its own local MaxWalkSpeed to match, since
		// movement prediction/simulation runs locally - Nicotine itself replicates, so this just
		// needs to react to it wherever it's bound.
		BindMoveSpeedToNicotine();
	}
}

void Acasino_simulatorCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// Client-side init, mirrors PossessedBy for clients that receive the player state after possession.
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
		BindMoveSpeedToNicotine();
	}
}

void Acasino_simulatorCharacter::InitializeDefaultAttributes() const
{
	if (!AbilitySystemComponent || !InitialAttributesEffectClass)
	{
		return;
	}

	FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	const FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(InitialAttributesEffectClass, 1.0f, EffectContext);
	if (SpecHandle.IsValid())
	{
		AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}
}

void Acasino_simulatorCharacter::ApplyAttributeDecayEffect()
{
	if (!AbilitySystemComponent || !AttributeDecayEffectClass)
	{
		UE_LOG(Logcasino_simulator, Warning, TEXT("'%s' could not apply attribute decay effect: %s missing."), *GetNameSafe(this), !AbilitySystemComponent ? TEXT("AbilitySystemComponent") : TEXT("AttributeDecayEffectClass"));
		return;
	}

	FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	const FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(AttributeDecayEffectClass, 1.0f, EffectContext);
	if (SpecHandle.IsValid())
	{
		AttributeDecayEffectHandle = AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}

	UE_LOG(Logcasino_simulator, Log, TEXT("'%s' applied attribute decay effect (active: %s)."), *GetNameSafe(this), AttributeDecayEffectHandle.IsValid() ? TEXT("true") : TEXT("false"));
}

void Acasino_simulatorCharacter::BindMoveSpeedToNicotine()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(Ucasino_simulatorAttributeSet::GetNicotineAttribute())
		.AddLambda([this](const FOnAttributeChangeData&) { UpdateMoveSpeedFromNicotine(); });
	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(Ucasino_simulatorAttributeSet::GetMaxNicotineAttribute())
		.AddLambda([this](const FOnAttributeChangeData&) { UpdateMoveSpeedFromNicotine(); });

	// Apply immediately so movement speed matches the current ratio without waiting for the next change.
	UpdateMoveSpeedFromNicotine();
}

void Acasino_simulatorCharacter::UpdateMoveSpeedFromNicotine() const
{
	if (!AttributeSet || !GetCharacterMovement())
	{
		return;
	}

	const float MaxNicotineValue = AttributeSet->GetMaxNicotine();
	const float Ratio = (MaxNicotineValue > 0.0f) ? FMath::Clamp(AttributeSet->GetNicotine() / MaxNicotineValue, 0.0f, 1.0f) : 1.0f;

	GetCharacterMovement()->MaxWalkSpeed = MaxMoveSpeed * Ratio;
}

void Acasino_simulatorCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{	
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &Acasino_simulatorCharacter::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &Acasino_simulatorCharacter::DoJumpEnd);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &Acasino_simulatorCharacter::MoveInput);

		// Looking/Aiming
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &Acasino_simulatorCharacter::LookInput);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &Acasino_simulatorCharacter::LookInput);
	}
	else
	{
		UE_LOG(Logcasino_simulator, Error, TEXT("'%s' Failed to find an Enhanced Input Component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}


void Acasino_simulatorCharacter::MoveInput(const FInputActionValue& Value)
{
	// get the Vector2D move axis
	FVector2D MovementVector = Value.Get<FVector2D>();

	// pass the axis values to the move input
	DoMove(MovementVector.X, MovementVector.Y);

}

void Acasino_simulatorCharacter::LookInput(const FInputActionValue& Value)
{
	// get the Vector2D look axis
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// pass the axis values to the aim input
	DoAim(LookAxisVector.X, LookAxisVector.Y);

}

void Acasino_simulatorCharacter::DoAim(float Yaw, float Pitch)
{
	if (GetController())
	{
		// pass the rotation inputs
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void Acasino_simulatorCharacter::DoMove(float Right, float Forward)
{
	if (GetController())
	{
		// pass the move inputs
		AddMovementInput(GetActorRightVector(), Right);
		AddMovementInput(GetActorForwardVector(), Forward);
	}
}

void Acasino_simulatorCharacter::DoJumpStart()
{
	// pass Jump to the character
	Jump();
}

void Acasino_simulatorCharacter::DoJumpEnd()
{
	// pass StopJumping to the character
	StopJumping();
}
