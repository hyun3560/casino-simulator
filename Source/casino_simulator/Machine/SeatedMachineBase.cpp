#include "Machine/SeatedMachineBase.h"

#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "casino_simulatorCharacter.h"

ASeatedMachineBase::ASeatedMachineBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true; // 이 액터가 네트워크 복제 대상이라는 뜻

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	// 의자용 StaticMeshComponent를 생성하는 줄
	ChairMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChairMesh"));
	// ChairMesh를 SceneRoot 밑에 붙이는 줄
	ChairMesh->SetupAttachment(SceneRoot);
	// 의자의 충돌 설정을 정하는 줄
	ChairMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	SeatPoint = CreateDefaultSubobject<USceneComponent>(TEXT("SeatPoint"));
	SeatPoint->SetupAttachment(ChairMesh);

	CameraPoint = CreateDefaultSubobject<USceneComponent>(TEXT("CameraPoint"));
	CameraPoint->SetupAttachment(SceneRoot);

	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(SceneRoot);
	InteractionSphere->InitSphereRadius(InteractionRadius);
	InteractionSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
}

void ASeatedMachineBase::BeginPlay()
{
	Super::BeginPlay();

	if (InteractionSphere)
	{
		InteractionSphere->SetSphereRadius(InteractionRadius);
	}
}

void ASeatedMachineBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASeatedMachineBase, CurrentUser);
	DOREPLIFETIME(ASeatedMachineBase, bCanOperate);
}

void ASeatedMachineBase::Interact(Acasino_simulatorCharacter* RequestingCharacter)
{
	if (HasAuthority())
	{
		Server_RequestUseMachine_Implementation(RequestingCharacter);
		return;
	}

	Server_RequestUseMachine(RequestingCharacter);
}

void ASeatedMachineBase::RequestReleaseMachine(Acasino_simulatorCharacter* RequestingCharacter)
{
	if (HasAuthority())
	{
		Server_ReleaseMachine_Implementation(RequestingCharacter);
		return;
	}

	Server_ReleaseMachine(RequestingCharacter);
}

bool ASeatedMachineBase::CanOperate(Acasino_simulatorCharacter* RequestingCharacter) const
{
	return bCanOperate && CurrentUser && CurrentUser == RequestingCharacter;
}

void ASeatedMachineBase::Server_RequestUseMachine_Implementation(Acasino_simulatorCharacter* RequestingCharacter)
{
	const ESeatedMachineUseResult Result = CanAcceptUser(RequestingCharacter);
	if (Result != ESeatedMachineUseResult::Accepted)
	{
		OnMachineUseRejected(RequestingCharacter, Result);
		return;
	}

	CurrentUser = RequestingCharacter;
	bCanOperate = true;

	Multicast_MachineUseStarted(RequestingCharacter);
}

void ASeatedMachineBase::Server_ReleaseMachine_Implementation(Acasino_simulatorCharacter* RequestingCharacter)
{
	if (!RequestingCharacter || CurrentUser != RequestingCharacter)
	{
		return;
	}

	Acasino_simulatorCharacter* ReleasingCharacter = CurrentUser;
	CurrentUser = nullptr;
	bCanOperate = false;

	Multicast_MachineReleased(ReleasingCharacter);
}

void ASeatedMachineBase::Multicast_MachineUseStarted_Implementation(Acasino_simulatorCharacter* RequestingCharacter)
{
	OnMachineReady(RequestingCharacter);
}

void ASeatedMachineBase::Multicast_MachineReleased_Implementation(Acasino_simulatorCharacter* ReleasingCharacter)
{
	OnMachineReleased(ReleasingCharacter);
}

void ASeatedMachineBase::OnRep_CurrentUser()
{
	bCanOperate = CurrentUser != nullptr;
}

void ASeatedMachineBase::OnMachineReady_Implementation(Acasino_simulatorCharacter* RequestingCharacter)
{
}

void ASeatedMachineBase::OnMachineReleased_Implementation(Acasino_simulatorCharacter* ReleasingCharacter)
{
}

void ASeatedMachineBase::OnMachineUseRejected_Implementation(
	Acasino_simulatorCharacter* RequestingCharacter,
	ESeatedMachineUseResult Result)
{
}

ESeatedMachineUseResult ASeatedMachineBase::CanAcceptUser(Acasino_simulatorCharacter* RequestingCharacter) const
{
	if (!RequestingCharacter)
	{
		return ESeatedMachineUseResult::InvalidUser;
	}

	if (CurrentUser && CurrentUser != RequestingCharacter)
	{
		return ESeatedMachineUseResult::AlreadyOccupied;
	}

	return ESeatedMachineUseResult::Accepted;
}
