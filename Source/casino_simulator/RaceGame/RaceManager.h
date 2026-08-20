// RaceManager.h  ─ 레이스 진행 관리(서버 권위). 레벨에 배치.
// [결정론 재생] 시작 때 각 러너 레시피 롤 → 복제. 승자는 서버가 시뮬로 확정 → WinnerIndex 복제.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RaceTypes.h"
#include "RaceManager.generated.h"

class ARaceRunner;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRaceLineupReady);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRaceStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRaceFinished, ARaceRunner*, Winner, int32, WinnerIndex);

UCLASS()
class CASINO_SIMULATOR_API ARaceManager : public AActor
{
	GENERATED_BODY()

public:
	ARaceManager();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race") TSubclassOf<ARaceRunner> RunnerClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race") int32 NumRunners = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Track") FVector StartLocation = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Track") FVector RaceDirection = FVector(1, 0, 0);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Track") float   TrackLength = 3000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Track") float   LaneSpacing = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Odds")  float   HouseMargin = 0.15f;

	UPROPERTY(ReplicatedUsing = OnRep_Phase, BlueprintReadOnly, Category = "Race") ERacePhase Phase = ERacePhase::Idle;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Race")                     int32 WinnerIndex = -1;

	UPROPERTY(BlueprintAssignable, Category = "Race") FOnRaceLineupReady OnLineupReady;
	UPROPERTY(BlueprintAssignable, Category = "Race") FOnRaceStarted     OnRaceStarted;
	UPROPERTY(BlueprintAssignable, Category = "Race") FOnRaceFinished    OnRaceFinished;

	// 새 라운드: 러너 스폰 + 스탯 롤 + 배당 공개 (서버)
	UFUNCTION(BlueprintCallable, Category = "Race") void StartNewRound();
	// 배팅 마감 → 레이스 시작: 레시피 롤 + 승자 확정 + 출발 (서버)
	UFUNCTION(BlueprintCallable, Category = "Race") void StartRace();
	UFUNCTION(BlueprintCallable, Category = "Race") void ResetRace();

	UFUNCTION(BlueprintCallable, Category = "Race") const TArray<ARaceRunner*>& GetRunners() const { return Runners; }
	UFUNCTION(BlueprintCallable, Category = "Race") ARaceRunner* GetWinner() const;

protected:
	virtual void BeginPlay() override;
	
	UFUNCTION() void OnRep_Phase();

	UPROPERTY() TArray<ARaceRunner*> Runners;
	UPROPERTY() TArray<int32> LaneBuckets;

	void ShuffleBuckets();
	FRaceRunnerStats  RollStats(int32 LaneIndex) const;
	FRunnerRaceScript RollScript(const FRaceRunnerStats& S, const FVector& StartLoc, const FVector& Dir) const;

	float RaceElapsed = 0.f;
	float RaceDuration = 0.f;
	bool  bResultBroadcast = false;
};
