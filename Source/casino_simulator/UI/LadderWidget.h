// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LadderWidget.generated.h"

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
	int32 Rails = 5;

	/** 행(가로 레벨) 개수. 행 0 = 시작 전용. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder")
	int32 Rows = 20;

	/** 가로줄 두께(px). 세로줄은 이보다 살짝 얇게 그린다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder")
	float LineThickness = 3.0f;

	/** 줄 색. 실제/위장 모두 같은 색으로 그려야 구분이 안 됨. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder")
	FLinearColor LineColor = FLinearColor::White;

	/** 사다리를 그릴 픽셀 크기. X나 Y가 0 이하면 위젯 전체를 채운다. 값이 있으면 그 크기로 가운데 정렬해서 그림. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder")
	FVector2D LadderSize = FVector2D(400.0f, 760.0f);

	/** 도착 줄이 이 값 이상이면 당첨 (원본 규칙: 3 이상). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder")
	int32 WinRailThreshold = 3;

	/** 시작 줄(플레이어 선택)로 사다리를 생성한다. 결과는 아래 ReadOnly 값들로. */
	UFUNCTION(BlueprintCallable, Category = "Ladder")
	void GenerateLadder(int32 StartRail);

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

protected:
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
		const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	/** 그릴 가로줄. DrawRung[행][칸] == true 면 그린다 (실제 + 위장 통합). */
	TArray<TArray<bool>> DrawRung;

	/** walk 한 스텝: 현재 줄을 갱신하고 이동 수/여유분을 갱신 */
	void WalkStep(int32& Cur, int32 Dest, int32& Moves, int32& Slack) const;

	/** [1, Rows-1] 에서 Count 개를 중복 없이 뽑아 오름차순 반환 (Fisher-Yates) */
	TArray<int32> PickSortedRows(int32 Count) const;
};
