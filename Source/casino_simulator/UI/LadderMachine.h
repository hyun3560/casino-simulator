// LadderMachine.h  (프로젝트 반영됨: Source/casino_simulator/UI/)
//
// 사다리 게임의 "머신" 액터 = Model.
//  - 돈/규칙/상태/판 생성/정산을 전부 소유한다. (그리기는 전혀 안 함)
//  - 상태 변화는 dispatcher 로 방송하고, 뷰(위젯)들이 구독해서 표시만 한다.
//  - BP_LadderMachine 을 이 클래스로 재부모화(Reparent)해서 사용.
//
// 흐름:
//   입력(BP) → 머신 Nav* → (RailSelect+Select) → Play(): Generator로 Plan 확정 + 정산까지 완료(돈 확정)
//            → OnPlayStarted(Plan) 방송 → 위젯이 트레이스 연출
//   위젯 트레이스 끝 → 머신 RevealFinished(): 최종 잔액 표시 + OnResult 방송 → 결과창 표시
//   (정산은 Play에서 끝. RevealFinished는 "보여주기"만 — 멀티에서 클라 타이머가 돈을 못 몰게.)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LadderGenerator.h"   // FLadderPlan / FLadderRungRow / ULadderGenerator
#include "LadderMachine.generated.h"

/** 전환 버튼으로 순환하는 조작 모드 (기존 위젯에서 이동). */
UENUM(BlueprintType)
enum class ELadderMode : uint8
{
	RailSelect  UMETA(DisplayName = "레일선택"),
	RailCount   UMETA(DisplayName = "줄개수"),
	Bet         UMETA(DisplayName = "배팅"),
	Refresh     UMETA(DisplayName = "새로고침"),
};

/** 배당 테이블 한 항목: 특정 줄 개수에서 나올 수 있는 배당 후보 하나.
 *  같은 Rails 값으로 여러 개 넣으면, 새로고침(리롤) 시 그 중 하나가 랜덤으로 뽑힌다.
 *  Multipliers 길이는 Rails 와 같게 (모자라면 0으로 채우고 넘치면 잘림). 0 = 꽝.
 *  예) {4, [0,0,2,2]}, {4, [0,0,0,4]}, {4, [0,0,1,3]} → 4줄일 때 셋 중 랜덤. */
USTRUCT(BlueprintType)
struct FPayoutOption
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder")
	int32 Rails = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder")
	TArray<int32> Multipliers;
};

// ── Dispatcher 선언 ──
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLadderIntEvent, int32, Value);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FLadderSimpleEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLadderModeEvent, ELadderMode, NewMode);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLadderPlayEvent, FLadderPlan, Plan);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FLadderResultEvent, bool, bWin, int32, DestRail, int32, Multiplier, int32, Winnings);

UCLASS()
class CASINO_SIMULATOR_API ALadderMachine : public AActor
{
	GENERATED_BODY()

public:
	ALadderMachine();

	// ─────────── Config (디자이너 편집) ───────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder")
	int32 Rails = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder")
	int32 RowsPerRail = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder")
	int32 MinRails = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder")
	int32 MaxRails = 5;

	/** 배당 테이블. 줄 개수별 배당 후보들. 같은 Rails로 여러 항목 → 리롤 시 랜덤 선택.
	 *  BP 디폴트에서 편집. 현재 Rails에 항목이 없으면 아래 BasePayouts로 폴백. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder|Result")
	TArray<FPayoutOption> PayoutTable;

	/** 폴백용 기본 배당 셋. PayoutTable에 현재 Rails 항목이 없을 때만 앞에서 Rails개 사용. 0 = 꽝. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder|Result")
	TArray<int32> BasePayouts = { 0, 0, 2, 3, 5 };

	/** 위장 가로줄 배치 확률. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder")
	float FakeRungChance = 0.4f;

	// ─────────── Money ───────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ladder|Money")
	int32 Balance = 1000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder|Money")
	int32 BetAmount = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder|Money")
	int32 BetStep = 100;

	// ─────────── Runtime state (읽기 전용) ───────────
	/** 행 수 = Rails * RowsPerRail. SetRailCount 시 재계산. */
	UPROPERTY(BlueprintReadOnly, Category = "Ladder")
	int32 Rows = 12;

	/** 현재 각 줄(맨 아래)의 배당. 길이 = Rails. */
	UPROPERTY(BlueprintReadOnly, Category = "Ladder|Result")
	TArray<int32> Payouts;

	UPROPERTY(BlueprintReadOnly, Category = "Ladder|Nav")
	ELadderMode Mode = ELadderMode::RailSelect;

	UPROPERTY(BlueprintReadOnly, Category = "Ladder|Nav")
	int32 RailCursor = 0;

	/** 트레이스(연출) 진행 중이라 새 판 시작 불가한 상태. */
	UPROPERTY(BlueprintReadOnly, Category = "Ladder")
	bool bIsPlaying = false;

	/** 결과창이 열려 있는 동안 true. 이 동안 Nav 입력(전환/좌/우/선택)은 전부 무시된다.
	 *  RevealFinished 에서 true, ResetRound(결과창 닫힘)에서 false. */
	UPROPERTY(BlueprintReadOnly, Category = "Ladder")
	bool bResultOpen = false;

	/** 이번 판의 확정된 결과 + 그리기 데이터. */
	UPROPERTY(BlueprintReadOnly, Category = "Ladder")
	FLadderPlan CurrentPlan;

	// ─────────── Dispatchers (뷰가 구독) ───────────
	UPROPERTY(BlueprintAssignable, Category = "Ladder|Events")
	FLadderIntEvent OnBalanceChanged;

	UPROPERTY(BlueprintAssignable, Category = "Ladder|Events")
	FLadderIntEvent OnBetChanged;

	/** 세로줄 개수 변경 — BP에서 선택버튼/결과칸 재생성. */
	UPROPERTY(BlueprintAssignable, Category = "Ladder|Events")
	FLadderIntEvent OnRailCountChanged;

	UPROPERTY(BlueprintAssignable, Category = "Ladder|Events")
	FLadderModeEvent OnModeChanged;

	UPROPERTY(BlueprintAssignable, Category = "Ladder|Events")
	FLadderIntEvent OnCursorChanged;

	/** 배당 셔플됨 — BP에서 결과 슬롯 텍스트 갱신. */
	UPROPERTY(BlueprintAssignable, Category = "Ladder|Events")
	FLadderSimpleEvent OnResultsChanged;

	/** 잔액 부족 — BP에서 "잔액 부족" 피드백. */
	UPROPERTY(BlueprintAssignable, Category = "Ladder|Events")
	FLadderSimpleEvent OnInsufficientFunds;

	/** 판 비움(세로줄만) — 위젯이 그리기 판 초기화. */
	UPROPERTY(BlueprintAssignable, Category = "Ladder|Events")
	FLadderSimpleEvent OnBoardCleared;

	/** 판 시작 — 위젯이 이 Plan으로 트레이스 연출 시작. */
	UPROPERTY(BlueprintAssignable, Category = "Ladder|Events")
	FLadderPlayEvent OnPlayStarted;

	/** 정산까지 끝난 최종 결과 — MachineScreen/ResultWidget이 결과창 표시. */
	UPROPERTY(BlueprintAssignable, Category = "Ladder|Events")
	FLadderResultEvent OnResult;

	/** 결과창을 닫아야 함 — MachineScreen이 결과창 숨기기 + 배경 딤 해제. (OnResult의 짝) */
	UPROPERTY(BlueprintAssignable, Category = "Ladder|Events")
	FLadderSimpleEvent OnResultClosed;

	// ─────────── Config ops ───────────
	UFUNCTION(BlueprintCallable, Category = "Ladder")
	void SetRailCount(int32 NewRails);

	UFUNCTION(BlueprintCallable, Category = "Ladder")
	void AddRail() { SetRailCount(Rails + 1); }

	UFUNCTION(BlueprintCallable, Category = "Ladder")
	void RemoveRail() { SetRailCount(Rails - 1); }

	UFUNCTION(BlueprintCallable, Category = "Ladder|Result")
	void ShuffleResults();

	UFUNCTION(BlueprintPure, Category = "Ladder|Result")
	int32 GetPayout(int32 Rail) const;

	// ─────────── Money ops ───────────
	UFUNCTION(BlueprintCallable, Category = "Ladder|Money")
	void ChangeBet(int32 Steps);

	UFUNCTION(BlueprintCallable, Category = "Ladder|Money")
	void ClampBet();

	// ─────────── Nav (입력 진입점 — BP 입력/상호작용에서 호출) ───────────
	UFUNCTION(BlueprintCallable, Category = "Ladder|Nav")
	void SetRailCursor(int32 NewCursor);

	UFUNCTION(BlueprintCallable, Category = "Ladder|Nav")
	void NavCycle();

	UFUNCTION(BlueprintCallable, Category = "Ladder|Nav")
	void NavLeft();

	UFUNCTION(BlueprintCallable, Category = "Ladder|Nav")
	void NavRight();

	UFUNCTION(BlueprintCallable, Category = "Ladder|Nav")
	void NavSelect();

	/** 새 판 초기화(결과창 닫을 때). 판 비움 + 모드=레일선택 + 커서 0. */
	UFUNCTION(BlueprintCallable, Category = "Ladder|Nav")
	void ResetRound();

	// ─────────── View callback ───────────
	/** 위젯이 트레이스 연출을 끝냈을 때 호출 → 결과창 표시 + 최종 잔액 "표시".
	 *  돈 정산(차감+지급)은 Play 시점에 이미 끝나 있음. 여기선 보여주기만 한다.
	 *  (멀티: 클라 애니 타이머가 돈을 정산하면 안 되므로, 정산은 서버 Play에서 끝냄.) */
	UFUNCTION(BlueprintCallable, Category = "Ladder")
	void RevealFinished();

	/** 뷰가 dispatcher 바인딩을 마친 뒤 1회 호출 → 현재 상태를 재방송해 초기 UI를 채운다.
	 *  (BeginPlay 방송을 놓쳐도 되도록, 바인딩 순서에 의존하지 않게 하는 용도.) */
	UFUNCTION(BlueprintCallable, Category = "Ladder")
	void BroadcastInitialState();

	// ─────────── Getters (뷰 레이아웃용) ───────────
	UFUNCTION(BlueprintPure, Category = "Ladder")
	int32 GetRails() const { return Rails; }

	UFUNCTION(BlueprintPure, Category = "Ladder")
	int32 GetRows() const { return Rows; }

protected:
	virtual void BeginPlay() override;

private:
	void BuildPayouts();
	TArray<int32> RollPayouts();   // 현재 Rails용 후보 랜덤 선택 + 위치 셔플
	void ClearBoard();
	void Play(int32 StartRail);   // NavSelect(RailSelect) 내부 호출

	/** 이번 판에 실제로 건 금액. */
	int32 StakedThisRound = 0;
};
