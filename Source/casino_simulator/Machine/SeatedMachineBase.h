#pragma once

#include "CoreMinimal.h"
#include "Interaction/WorldInteractableBase.h"
#include "SeatedMachineBase.generated.h"

class Acasino_simulatorCharacter;
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
class CASINO_SIMULATOR_API ASeatedMachineBase : public AWorldInteractableBase
{
	GENERATED_BODY()

public:
	ASeatedMachineBase();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// �ӽ� ��� ��û
	virtual void Interact(Acasino_simulatorCharacter* RequestingCharacter) override;

	// �� �ӽſ��� ������ ��û
	UFUNCTION(BlueprintCallable, Category = "Machine|Interaction")
	void RequestReleaseMachine(Acasino_simulatorCharacter* RequestingCharacter);

	//�ӽ� ��뿩�� ��ȸ
	UFUNCTION(BlueprintPure, Category = "Machine|State")
	bool IsOccupied() const { return CurrentUser != nullptr; }

	// �÷��̾ ���� ���� �������� Ȯ��
	UFUNCTION(BlueprintPure, Category = "Machine|State")
	bool CanOperate(Acasino_simulatorCharacter* RequestingCharacter) const;

	virtual bool CanInteract(Acasino_simulatorCharacter* RequestingCharacter) const override;

	UFUNCTION(BlueprintPure, Category = "Machine|State")
	Acasino_simulatorCharacter* GetCurrentUser() const { return CurrentUser; }

	UFUNCTION(BlueprintPure, Category = "Machine|Seat")
	USceneComponent* GetSeatPoint() const { return SeatPoint; }

	UFUNCTION(BlueprintPure, Category = "Machine|Seat")
	USceneComponent* GetCameraPoint() const { return CameraPoint; }

protected:
	// ���� ���� �޽�
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine|Components")
	TObjectPtr<UStaticMeshComponent> ChairMesh;

	// �÷��̾ ���� ��ġ
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine|Components")
	TObjectPtr<USceneComponent> SeatPoint;

	// �ӽ� ���� �� ī�޶� ��ġ
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine|Components")
	TObjectPtr<USceneComponent> CameraPoint;

	// ���� �� �ӽ��� ���� �÷��̾�
	UPROPERTY(ReplicatedUsing = OnRep_CurrentUser, BlueprintReadOnly, Category = "Machine|State")
	TObjectPtr<Acasino_simulatorCharacter> CurrentUser;

	// ���� ������ ��������
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Machine|State")
	bool bCanOperate = false;

	//�ѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤ�RPC�ѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤ�
	// RPC : Remote Procedure Call - �ָ� �ִ� ��ǻ�Ϳ��� �Լ��� �����Ű�� ��
	
	// Ŭ���̾�Ʈ�� �������� �ش� �ӽ� ��� ��û�Լ�
	UFUNCTION(Server, Reliable)
	void Server_RequestUseMachine(Acasino_simulatorCharacter* RequestingCharacter);

	// Ŭ���̾�Ʈ�� �������� �ش� �ӽ� ������ ��û �Լ� *���� ����ڰ� �´��� �˻�
	UFUNCTION(Server, Reliable)
	void Server_ReleaseMachine(Acasino_simulatorCharacter* RequestingCharacter);

	// ������ Ŭ���̾�Ʈ���� �÷��̾��� �ӽ� ��� ������ �˸��� �Լ�
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_MachineUseStarted(Acasino_simulatorCharacter* RequestingCharacter);

	// ������ Ŭ���̾�Ʈ���� �÷��̾��� �ӽ� ��� ���Ḧ �˸��� �Լ�
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_MachineReleased(Acasino_simulatorCharacter* ReleasingCharacter);
	//�ѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤѤ�

	UFUNCTION()
	void OnRep_CurrentUser(); // OnRep�� RepNotify �Լ�

	// �ӽ� ����� ���εǾ� ���� ������ ���°� �Ǿ��� �� ȣ��
	UFUNCTION(BlueprintNativeEvent, Category = "Machine|State")
	void OnMachineReady(Acasino_simulatorCharacter* RequestingCharacter);
	virtual void OnMachineReady_Implementation(Acasino_simulatorCharacter* RequestingCharacter);


	// ���� ����ڰ� �ӽſ��� ������ �� ȣ��
	UFUNCTION(BlueprintNativeEvent, Category = "Machine|State")
	void OnMachineReleased(Acasino_simulatorCharacter* ReleasingCharacter);
	virtual void OnMachineReleased_Implementation(Acasino_simulatorCharacter* ReleasingCharacter);

	// ��� ��û�� �������� �� ȣ��
	UFUNCTION(BlueprintNativeEvent, Category = "Machine|State")
	void OnMachineUseRejected(Acasino_simulatorCharacter* RequestingCharacter, ESeatedMachineUseResult Result);
	virtual void OnMachineUseRejected_Implementation(Acasino_simulatorCharacter* RequestingCharacter, ESeatedMachineUseResult Result);

private:
	// ���� ���ο��� ��� ���� ���� �Ǵ�
	ESeatedMachineUseResult CanAcceptUser(Acasino_simulatorCharacter* RequestingCharacter) const;
};
