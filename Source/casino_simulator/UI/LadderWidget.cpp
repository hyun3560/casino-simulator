// Copyright Epic Games, Inc. All Rights Reserved.

#include "LadderWidget.h"
#include "Rendering/DrawElements.h"

void ULadderWidget::WalkStep(int32& Cur, int32 Dest, int32& Moves, int32& Slack) const
{
	if (Slack == 0)
	{
		// 여유분 0 → 최단루트(homing): 도착 쪽으로 한 칸
		if (Cur > Dest)      Cur -= 1;
		else if (Cur < Dest) Cur += 1;
	}
	else if (Cur == 0)          Cur += 1;   // 왼쪽 벽
	else if (Cur == Rails - 1)  Cur -= 1;   // 오른쪽 벽
	else                        Cur += (FMath::RandRange(0, 1) == 0) ? 1 : -1;

	Moves -= 1;
	Slack = (Moves - FMath::Abs(Dest - Cur)) / 2;
}

TArray<int32> ULadderWidget::PickSortedRows(int32 Count) const
{
	TArray<int32> Pool;
	for (int32 i = 1; i < Rows; ++i)
	{
		Pool.Add(i);
	}

	// Fisher-Yates: 뒤에서부터 [0..i] 무작위 위치와 스왑
	for (int32 i = Pool.Num() - 1; i > 0; --i)
	{
		const int32 j = FMath::RandRange(0, i);
		Pool.Swap(i, j);
	}

	Count = FMath::Clamp(Count, 0, Pool.Num());
	Pool.SetNum(Count);
	Pool.Sort();
	return Pool;
}

void ULadderWidget::GenerateLadder(int32 StartRail)
{
	if (Rails < 2 || Rows < 2)
	{
		return;
	}

	StartRail = FMath::Clamp(StartRail, 0, Rails - 1);
	StartRailResult = StartRail;

	// ── 도착 줄(결과) 먼저 결정 ──
	const int32 Dest = FMath::RandRange(0, Rails - 1);
	DestRail = Dest;
	bWin = (Dest >= WinRailThreshold);

	const int32 MinMove = FMath::Abs(Dest - StartRail);

	// 크로싱 수: 약 절반에서 시작해 최소이동과 패리티를 맞추고, 도달 가능/행 수 범위로 보정
	int32 Crossings = Rows / 2;
	if ((Crossings % 2) != (MinMove % 2)) Crossings -= 1;   // 패리티 맞추기
	Crossings = FMath::Max(Crossings, MinMove);              // 최소 도달 보장
	if ((Crossings % 2) != (MinMove % 2)) Crossings += 1;   // 보정 후 패리티 재확인
	Crossings = FMath::Min(Crossings, Rows - 1);            // 행 수 캡

	int32 Slack = (Crossings - MinMove) / 2;

	// ── walk: 줄 시퀀스(위치저장소) ──
	TArray<int32> Path;
	Path.Add(StartRail);
	int32 Cur = StartRail;
	int32 MovesLeft = Crossings;
	while (MovesLeft > 0)
	{
		WalkStep(Cur, Dest, MovesLeft, Slack);
		Path.Add(Cur);
	}

	// ── 크로싱 행(랜덤행) ──
	const TArray<int32> CrossRows = PickSortedRows(Crossings);

	// ── 구슬저장소 [행][줄] ──
	TArray<TArray<int32>> Ball;
	Ball.SetNum(Rows);
	for (TArray<int32>& Row : Ball) { Row.Init(0, Rails); }

	int32 idx = 0, ci = 0;
	for (int32 r = 0; r < Rows; ++r)
	{
		Ball[r][Path[idx]] = 1;                                  // 들어온 줄
		if (ci < CrossRows.Num() && r == CrossRows[ci])          // 이 행이 크로싱?
		{
			idx++;
			Ball[r][Path[idx]] = 1;                              // 건너간 줄도 같은 행에
			ci++;
		}
	}

	// ── 행저장소 판별 → DrawRung ──
	const int32 Gaps = Rails - 1;
	DrawRung.SetNum(Rows);
	for (TArray<bool>& Row : DrawRung) { Row.Init(false, Gaps); }

	// 1) 실제 가로줄: 칸 g 양옆 줄(g, g+1)이 둘 다 구슬
	for (int32 r = 0; r < Rows; ++r)
	{
		for (int32 g = 0; g < Gaps; ++g)
		{
			const bool L = Ball[r][g] != 0;
			const bool R = Ball[r][g + 1] != 0;
			if (L && R)
			{
				DrawRung[r][g] = true;   // 실제 경로 가로줄
			}
		}
	}

	// 2) 위장: "양옆 둘 다 구슬 아님" 후보 중, 인접 칸에 이미 줄이 없으면 배치 (좌→우 순차)
	for (int32 r = 0; r < Rows; ++r)
	{
		for (int32 g = 0; g < Gaps; ++g)
		{
			if (DrawRung[r][g]) continue;                        // 이미 실제 줄
			if (Ball[r][g] != 0 || Ball[r][g + 1] != 0) continue; // 구슬 옆 금지
			const bool LeftAdj  = (g > 0)        && DrawRung[r][g - 1];
			const bool RightAdj = (g < Gaps - 1) && DrawRung[r][g + 1];
			if (LeftAdj || RightAdj) continue;                   // 같은 행 인접 금지
			DrawRung[r][g] = true;                               // 위장 배치
		}
	}

	OnLadderResult(bWin, DestRail);

	// 기본적으로 UserWidget 은 매 프레임 다시 그려지므로 별도 무효화 불필요.
	// (Invalidation Panel 안에 넣는 경우에만 Invalidate 를 호출하면 됨)
}

int32 ULadderWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	if (Rails < 2 || Rows < 2)
	{
		return LayerId;
	}

	const FVector2D Full = AllottedGeometry.GetLocalSize();

	// 그릴 영역 크기 결정: LadderSize 지정 시 그 크기(위젯보다 크면 위젯에 맞춤), 아니면 전체
	FVector2D DrawSize = Full;
	if (LadderSize.X > 0.0f && LadderSize.Y > 0.0f)
	{
		DrawSize.X = FMath::Min(LadderSize.X, Full.X);
		DrawSize.Y = FMath::Min(LadderSize.Y, Full.Y);
	}
	const FVector2D Origin = (Full - DrawSize) * 0.5f;   // 가운데 정렬

	const float dx = DrawSize.X / (Rails - 1);   // 세로줄 간격
	const float dy = DrawSize.Y / (Rows - 1);    // 행 간격
	const FPaintGeometry PaintGeo = AllottedGeometry.ToPaintGeometry();

	// 세로줄
	for (int32 g = 0; g < Rails; ++g)
	{
		const float x = Origin.X + g * dx;
		TArray<FVector2D> Pts;
		Pts.Add(FVector2D(x, Origin.Y));
		Pts.Add(FVector2D(x, Origin.Y + DrawSize.Y));
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, PaintGeo, Pts,
			ESlateDrawEffect::None, LineColor, true, LineThickness * 0.7f);
	}

	// 가로줄 (실제 + 위장 통합, 같은 색)
	for (int32 r = 0; r < DrawRung.Num(); ++r)
	{
		const float y = Origin.Y + r * dy;
		for (int32 g = 0; g < DrawRung[r].Num(); ++g)
		{
			if (!DrawRung[r][g]) continue;
			TArray<FVector2D> Pts;
			Pts.Add(FVector2D(Origin.X + g * dx, y));
			Pts.Add(FVector2D(Origin.X + (g + 1) * dx, y));
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId, PaintGeo, Pts,
				ESlateDrawEffect::None, LineColor, true, LineThickness);
		}
	}

	return LayerId + 1;
}
