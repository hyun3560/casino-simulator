#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SeatedMachineBase.generated.h"

class Acasino_simulatorCharacter;
class USceneComponent;
class USphereComponent;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class ESeatedMachineUseResult : uint8
{
	Accepted UMETA(DisplayName = "Accepted"),
	AlreadyOccupied UMETA(DisplayName = "Already Occupied"),
	InvalidUser UMETA(DisplayName = "Invalid User")
};

/**
 * Base actor for machines that a single player operates while seated.
 * Child classes own the actual game rules; this class only owns occupancy,
 * seat/camera points, and the server-authoritative use/release flow.
 */
UCLASS(Abstract, Blueprintable)
class CASINO_SIMULATOR_API ASeatedMachineBase : public AActor
{
	GENERATED_BODY()

public:
	ASeatedMachineBase();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 머신 사용 요청
	UFUNCTION(BlueprintCallable, Category = "Machine|Interaction")
	void Interact(Acasino_simulatorCharacter* RequestingCharacter);

	// 이 머신에서 나가기 요청
	UFUNCTION(BlueprintCallable, Category = "Machine|Interaction")
	void RequestReleaseMachine(Acasino_simulatorCharacter* RequestingCharacter);

	//머신 사용여부 조회
	UFUNCTION(BlueprintPure, Category = "Machine|State")
	bool IsOccupied() const { return CurrentUser != nullptr; }

	// 플레이어가 지금 조작 가능한지 확인
	UFUNCTION(BlueprintPure, Category = "Machine|State")
	bool CanOperate(Acasino_simulatorCharacter* RequestingCharacter) const;

	UFUNCTION(BlueprintPure, Category = "Machine|State")
	Acasino_simulatorCharacter* GetCurrentUser() const { return CurrentUser; }

	UFUNCTION(BlueprintPure, Category = "Machine|Seat")
	USceneComponent* GetSeatPoint() const { return SeatPoint; }

	UFUNCTION(BlueprintPure, Category = "Machine|Seat")
	USceneComponent* GetCameraPoint() const { return CameraPoint; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine|Components")
	TObjectPtr<USceneComponent> SceneRoot;

	// 공통 의자 메쉬
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine|Components")
	TObjectPtr<UStaticMeshComponent> ChairMesh;

	// 플레이어가 앉을 위치
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine|Components")
	TObjectPtr<USceneComponent> SeatPoint;

	// 머신 조작 시 카메라 위치
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine|Components")
	TObjectPtr<USceneComponent> CameraPoint;

	// 상호작용 범위 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine|Components")
	TObjectPtr<USphereComponent> InteractionSphere;

	// 현재 이 머신을 쓰는 플레이어
	UPROPERTY(ReplicatedUsing = OnRep_CurrentUser, BlueprintReadOnly, Category = "Machine|State")
	TObjectPtr<Acasino_simulatorCharacter> CurrentUser;

	// 조작 가능한 상태인지
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Machine|State")
	bool bCanOperate = false;

	// InteractionSphere의 반지름 값
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Machine|Interaction")
	float InteractionRadius = 150.0f;

	//ㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡRPCㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡ
	// RPC : Remote Procedure Call - 멀리 있는 컴퓨터에서 함수를 실행시키는 것
	
	// 클라이언트가 서버에게 해당 머신 사용 요청함수
	UFUNCTION(Server, Reliable)
	void Server_RequestUseMachine(Acasino_simulatorCharacter* RequestingCharacter);

	// 클라이언트가 서버에게 해당 머신 나가기 요청 함수 *현재 사용자가 맞는지 검사
	UFUNCTION(Server, Reliable)
	void Server_ReleaseMachine(Acasino_simulatorCharacter* RequestingCharacter);

	// 서버가 클라이언트에게 플레이어의 머신 사용 시작을 알리는 함수
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_MachineUseStarted(Acasino_simulatorCharacter* RequestingCharacter);

	// 서버가 클라이언트에게 플레이어의 머신 사용 종료를 알리는 함수
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_MachineReleased(Acasino_simulatorCharacter* ReleasingCharacter);
	//ㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡ

	UFUNCTION()
	void OnRep_CurrentUser(); // OnRep는 RepNotify 함수

	// 머신 사용이 승인되어 조작 가능한 상태가 되었을 때 호출
	UFUNCTION(BlueprintNativeEvent, Category = "Machine|State")
	void OnMachineReady(Acasino_simulatorCharacter* RequestingCharacter);
	virtual void OnMachineReady_Implementation(Acasino_simulatorCharacter* RequestingCharacter);


	// 현재 사용자가 머신에서 나갔을 때 호출
	UFUNCTION(BlueprintNativeEvent, Category = "Machine|State")
	void OnMachineReleased(Acasino_simulatorCharacter* ReleasingCharacter);
	virtual void OnMachineReleased_Implementation(Acasino_simulatorCharacter* ReleasingCharacter);

	// 사용 요청이 거절됐을 때 호출
	UFUNCTION(BlueprintNativeEvent, Category = "Machine|State")
	void OnMachineUseRejected(Acasino_simulatorCharacter* RequestingCharacter, ESeatedMachineUseResult Result);
	virtual void OnMachineUseRejected_Implementation(Acasino_simulatorCharacter* RequestingCharacter, ESeatedMachineUseResult Result);

private:
	// 서버 내부에서 사용 가능 여부 판단
	ESeatedMachineUseResult CanAcceptUser(Acasino_simulatorCharacter* RequestingCharacter) const;
};
