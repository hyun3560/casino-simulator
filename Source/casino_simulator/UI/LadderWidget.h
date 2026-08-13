// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LadderWidget.generated.h"

/** 전환 버튼으로 순환하는 조작 모드 */
UENUM(BlueprintType)
enum class ELadderMode : uint8
{
	RailSelect  UMETA(DisplayName = "레일선택"),
	RailCount   UMETA(DisplayName = "줄개수"),
	Bet         UMETA(DisplayName = "배팅"),
	Refresh     UMETA(DisplayName = "새로고침"),
};

/**
 *  사다리타기(위장 사다리 포함) 위젯.
 *  - GenerateLadder(StartRail) 로 시작 줄을 넣으면 도착 줄이 정해지고 사다리가 생성된다.
 *  - 실제 경로 가로줄과 위장 가로줄은 렌더링에서 구분되지 않는다(플레이어가 못 알아봄).
 *  - NativePaint 에서 세로줄 + 가로줄을 직접 그린다.
 */
UCLASS()
class CASINO_SIMULATOR_API ULadderWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 세로줄 개수 (줄 0 ~ Rails-1). 칸(가로줄 자리) 개수는 Rails-1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder")
	int32 Rails = 3;

	/** 행(가로 레벨) 개수. SetRailCount 시 Rails * RowsPerRail 로 자동 계산됨. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder")
	int32 Rows = 12;

	/** 행 수 = Rails * RowsPerRail. 세로줄이 늘면 행도 이 비율로 늘어난다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder")
	int32 RowsPerRail = 4;

	/** 세로줄 개수 조절 범위(인게임 +/- 버튼). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder")
	int32 MinRails = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder")
	int32 MaxRails = 5;

	/** 가로줄 두께(px). 세로줄은 이보다 살짝 얇게 그린다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder")
	float LineThickness = 3.0f;

	/** 줄 색. 실제/위장 모두 같은 색으로 그려야 구분이 안 됨. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder")
	FLinearColor LineColor = FLinearColor::White;

	/** 사다리 바깥 여백(px). 세로줄이 위젯 가장자리에 딱 붙지 않게 안쪽으로 띄운다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder")
	float LadderPadding = 20.0f;

	/** 세로줄(레일)이 맨 위/맨 아래 가로줄보다 얼마나 더 뻗어나올지(px). 시작·끝의 꺾임을 없앰. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder")
	float RailOverhang = 24.0f;

	/** 사다리 가로 크기 비율(0~1). 1=위젯 폭 꽉, 0.6=가운데 60%만. 줄이려면 이 값을 낮춰. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float LadderWidthRatio = 1.0f;

	/** 사다리 세로 크기 비율(0~1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float LadderHeightRatio = 1.0f;

	/** 사다리 픽셀 크기 강제 지정. (0,0)이면 위젯(=캔버스 슬롯) 크기를 그대로 채운다.
	 *  캔버스 패널 슬롯 크기로 조절하려면 반드시 (0,0)으로 둘 것. 값이 있으면 그 크기로 가운데 정렬. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder")
	FVector2D LadderSize = FVector2D(0.0f, 0.0f);

	/** 도착 줄이 이 값 이상이면 당첨 (원본 규칙: 3 이상). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder")
	int32 WinRailThreshold = 3;

	/** 시작 줄(플레이어 선택)로 사다리를 생성한다. 결과는 아래 ReadOnly 값들로. */
	UFUNCTION(BlueprintCallable, Category = "Ladder")
	void GenerateLadder(int32 StartRail);

	// ── 세로줄 개수 조절 ──

	/** 세로줄 개수 설정(Min~Max 클램프). Rows 재계산 + 판 비우기(세로줄만) + OnRailCountChanged 발생. */
	UFUNCTION(BlueprintCallable, Category = "Ladder")
	void SetRailCount(int32 NewRails);

	/** + 버튼용: 세로줄 하나 늘림 */
	UFUNCTION(BlueprintCallable, Category = "Ladder")
	void AddRail() { SetRailCount(Rails + 1); }

	/** - 버튼용: 세로줄 하나 줄임 */
	UFUNCTION(BlueprintCallable, Category = "Ladder")
	void RemoveRail() { SetRailCount(Rails - 1); }

	/** 가로줄/트레이스 제거 → 세로줄만 남김(버튼 누르기 전 상태). */
	UFUNCTION(BlueprintCallable, Category = "Ladder")
	void ClearBoard();

	/** 세로줄 개수가 바뀌면 호출 — BP에서 선택 버튼/결과 칸을 NewRails 개수로 다시 생성. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ladder")
	void OnRailCountChanged(int32 NewRails);

	// ── 세로줄 좌표 (버튼/결과 배치용, 위젯 로컬 좌표) ──

	/** 세로줄 Rail 의 맨 위 지점(로컬). 이 위쪽에 선택 버튼을 얹으면 됨. */
	UFUNCTION(BlueprintCallable, Category = "Ladder")
	FVector2D GetRailTop(int32 Rail) const;

	/** 세로줄 Rail 의 맨 아래 지점(로컬). 이 아래쪽에 결과 칸을 놓으면 됨. */
	UFUNCTION(BlueprintCallable, Category = "Ladder")
	FVector2D GetRailBottom(int32 Rail) const;

	/** 세로줄 Rail 의 가로 앵커 비율(0~1). BP에서 Canvas Slot Anchor.X 로 쓰면 크기·타이밍 상관없이 정렬됨. */
	UFUNCTION(BlueprintPure, Category = "Ladder")
	float GetRailAnchorX(int32 Rail) const;

	/** 사다리 맨 위 앵커 비율(0~1). 버튼 앵커 Y 로 쓰면 세로 축소해도 줄 위에 맞춰짐. */
	UFUNCTION(BlueprintPure, Category = "Ladder")
	float GetLadderTopAnchorY() const;

	/** 사다리 맨 아래 앵커 비율(0~1). 결과 앵커 Y 로. */
	UFUNCTION(BlueprintPure, Category = "Ladder")
	float GetLadderBottomAnchorY() const;

	/** 생성 결과 (읽기 전용) */
	UPROPERTY(BlueprintReadOnly, Category = "Ladder")
	int32 StartRailResult = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Ladder")
	int32 DestRail = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Ladder")
	bool bWin = false;

	/** 생성이 끝나면 호출 — 블루프린트에서 당첨/꽝 연출 붙이는 용도 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ladder")
	void OnLadderResult(bool bDidWin, int32 InDestRail);

	// ── 결과(배당) — 맨 아래 각 줄의 배당 금액 ──

	/** 기본 배당 셋(디자이너 편집). 예: [0,0,2,5,10]. Rails 개수만큼 앞에서 사용. 0 = 꽝. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder|Result")
	TArray<int32> BasePayouts = { 0, 0, 2, 3, 5 };

	/** 현재 각 줄(맨 아래)의 배당. 길이 = Rails. 셔플 대상. */
	UPROPERTY(BlueprintReadOnly, Category = "Ladder|Result")
	TArray<int32> Payouts;

	/** 방금 트레이스 도착줄의 배당(0이면 꽝). OnTraceFinished 때 정산에 쓰면 됨. */
	UPROPERTY(BlueprintReadOnly, Category = "Ladder|Result")
	int32 LastPayout = 0;

	/** 줄 Rail 의 현재 배당 (BP 결과 슬롯 텍스트 표시용). */
	UFUNCTION(BlueprintPure, Category = "Ladder|Result")
	int32 GetPayout(int32 Rail) const;

	/** 배당 셔플(새로고침용). Payouts 순서만 섞음 → OnResultsChanged 발생. 경로는 안 건드림. */
	UFUNCTION(BlueprintCallable, Category = "Ladder|Result")
	void ShuffleResults();

	/** 배당이 바뀌면 호출 — BP에서 결과 슬롯 텍스트 갱신(재스폰 등). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ladder|Result")
	void OnResultsChanged();

	// ── 자금(배팅) ──

	/** 보유 자금. 시작값 1000. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ladder|Money")
	int32 Balance = 1000;

	/** 이번 판 배팅액. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder|Money")
	int32 BetAmount = 100;

	/** 배팅액 +/- 단위. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder|Money")
	int32 BetStep = 100;

	/** 배팅액을 Steps*BetStep 만큼 변경(최소 BetStep, 최대 잔액). OnBetChanged 발생. */
	UFUNCTION(BlueprintCallable, Category = "Ladder|Money")
	void ChangeBet(int32 Steps);

	/** 배팅액을 현재 잔액 기준으로 재클램프(최소 BetStep, 최대 잔액) + OnBetChanged 발생.
	 *  잔액이 바뀐 뒤(정산/새 판) 배팅액·표시를 잔액에 맞추는 용도. */
	UFUNCTION(BlueprintCallable, Category = "Ladder|Money")
	void ClampBet();

	/** 잔액이 배팅액보다 적어서 시작 못 할 때 호출 — BP에서 "잔액 부족" 일시 연출(깜빡/사운드). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ladder|Money")
	void OnInsufficientFunds();

	/** 자금이 바뀌면 호출 — BP에서 잔액 표시 갱신. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ladder|Money")
	void OnBalanceChanged(int32 NewBalance);

	/** 배팅액이 바뀌면 호출 — BP에서 배팅액 표시 갱신. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ladder|Money")
	void OnBetChanged(int32 NewBet);

	// ── 조작 내비게이션 (물리 버튼 5개 중 4개: 전환/왼/오/선택) ──

	/** 현재 조작 모드 */
	UPROPERTY(BlueprintReadOnly, Category = "Ladder|Nav")
	ELadderMode Mode = ELadderMode::RailSelect;

	/** 레일선택 모드에서 지금 가리키는 줄(커서) */
	UPROPERTY(BlueprintReadOnly, Category = "Ladder|Nav")
	int32 RailCursor = 0;

	/** 레일 커서를 특정 줄로 직접 설정(클램프). 물리버튼/버튼클릭 공용 진입점. OnCursorChanged 발생. */
	UFUNCTION(BlueprintCallable, Category = "Ladder|Nav")
	void SetRailCursor(int32 NewCursor);

	/** 새 판 초기화(결과창 닫을 때 호출). 판 비우기 + 모드=레일선택 + 커서 0 으로 리셋. */
	UFUNCTION(BlueprintCallable, Category = "Ladder|Nav")
	void ResetRound();

	/** 전환 버튼: 모드 순환 (레일선택→줄개수→배팅→새로고침→…). */
	UFUNCTION(BlueprintCallable, Category = "Ladder|Nav")
	void NavCycle();

	/** 왼쪽 버튼: 현재 모드에서 왼쪽/감소. */
	UFUNCTION(BlueprintCallable, Category = "Ladder|Nav")
	void NavLeft();

	/** 오른쪽 버튼: 현재 모드에서 오른쪽/증가. */
	UFUNCTION(BlueprintCallable, Category = "Ladder|Nav")
	void NavRight();

	/** 선택(가운데) 버튼: 레일선택=발사, 새로고침=셔플. */
	UFUNCTION(BlueprintCallable, Category = "Ladder|Nav")
	void NavSelect();

	/** 모드가 바뀌면 호출 — BP에서 현재 모드 하이라이트 표시. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ladder|Nav")
	void OnModeChanged(ELadderMode NewMode);

	/** 레일 커서가 바뀌면 호출 — BP에서 선택 줄 하이라이트. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ladder|Nav")
	void OnCursorChanged(int32 RailIndex);

	// ── 실제 경로 트레이스(구슬 이동) 연출 ──

	/** 전체 경로를 훑는 시간(초). 작을수록 구슬이 빨리 내려감. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder|Trace")
	float TraceDuration = 2.5f;

	/** 지나온 경로 색(빨강). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder|Trace")
	FLinearColor TraceColor = FLinearColor::Red;

	/** 구슬 이미지. 원형 텍스처를 지정하면 동그란 구슬로, 비워두면 사각형으로 그림. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder|Trace")
	FSlateBrush MarbleBrush;

	/** 구슬 색(틴트). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder|Trace")
	FLinearColor MarbleColor = FLinearColor::Red;

	/** 구슬 크기(px). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder|Trace")
	float MarbleSize = 22.0f;

	/** 실제 경로를 따라 구슬 이동 연출 시작. GenerateLadder 이후, 버튼 OnClicked 에서 호출. */
	UFUNCTION(BlueprintCallable, Category = "Ladder")
	void StartTrace();

	/** 현재 트레이스 진행 중인지 */
	UFUNCTION(BlueprintPure, Category = "Ladder")
	bool IsTracing() const { return bTracing; }

	/** 트레이스가 끝나면 호출(당첨/꽝 연출, BGM 정지 등).
	 *  결과 위젯이 계산 없이 바로 쓰도록 배율·당첨금까지 완제품으로 넘김.
	 *  InMultiplier = 배율(LastPayout, 0=꽝), InWinnings = 획득 금액(건 돈 × 배율, 0=꽝). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ladder")
	void OnTraceFinished(bool bDidWin, int32 InDestRail, int32 InMultiplier, int32 InWinnings);

protected:
	/** 위젯 생성 시 초기 UI 상태 동기화(시작 모드 하이라이트/커서/배팅/잔액 등을 BP로 1회 방송). */
	virtual void NativeConstruct() override;

	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
		const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	/** 그릴 가로줄. DrawRung[행][칸] == true 면 그린다 (실제 + 위장 통합). */
	TArray<TArray<bool>> DrawRung;

	/** 실제 경로: 방문한 줄 시퀀스(Crossings+1개)와 크로싱이 일어난 행(Crossings개). 트레이스 재구성용. */
	TArray<int32> PathRails;
	TArray<int32> CrossRowsSaved;

	/** 트레이스 상태 */
	bool  bTracing = false;
	float TraceProgress = 0.0f;   // 0..1

	/** 이번 판에 실제로 건 금액(StartTrace 때 확정, 정산에 사용). */
	int32 StakedThisRound = 0;

	/** BasePayouts 에서 Rails 개수만큼 Payouts 구성 + 셔플. SetRailCount 시 호출. */
	void BuildPayouts();

	/** 주어진 위젯 크기로 그리기 영역(Origin/DrawSize)과 세로줄 간격(dx) 계산. NativePaint·좌표 함수 공용. */
	bool GetLayoutForSize(const FVector2D& Full, FVector2D& OutOrigin, FVector2D& OutDrawSize, float& OutDx) const;

	/** walk 한 스텝: 현재 줄을 갱신하고 이동 수/여유분을 갱신 */
	void WalkStep(int32& Cur, int32 Dest, int32& Moves, int32& Slack) const;

	/** [1, Rows-1] 에서 Count 개를 중복 없이 뽑아 오름차순 반환 (Fisher-Yates) */
	TArray<int32> PickSortedRows(int32 Count) const;
};
