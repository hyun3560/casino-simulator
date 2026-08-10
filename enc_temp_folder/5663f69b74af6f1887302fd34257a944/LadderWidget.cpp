// Copyright Epic Games, Inc. All Rights Reserved.

#include "LadderWidget.h"
#include "Rendering/DrawElements.h"
#include "Brushes/SlateColorBrush.h"

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
	LastPayout = GetPayout(Dest);      // 도착줄 배당
	bWin = (LastPayout > 0);           // 0이면 꽝

	const int32 MinMove = FMath::Abs(Dest - StartRail);

	//크로싱 횟수 결정 -> 행 수와 최소 이동횟수로 결정
	int32 Crossings = Rows / 2;
	if ((Crossings % 2) != (MinMove % 2)) Crossings -= 1;

	int32 Slack = (Crossings - MinMove) / 2;

	// ── walk: 줄 시퀀스(위치저장소) ──
	TArray<int32> Path;
	Path.Add(StartRail);
	int32 Cur = StartRail;
	int32 MovesRemaining = Crossings;
	while (MovesRemaining > 0)
	{
		WalkStep(Cur, Dest, MovesRemaining, Slack);
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
			if (FMath::FRand() >= 0.4f) continue;
			DrawRung[r][g] = true;                               // 위장 배치
		}
	}

	// 트레이스용 실제 경로 저장 + 상태 초기화
	PathRails = Path;
	CrossRowsSaved = CrossRows;
	bTracing = false;
	TraceProgress = 0.0f;

	OnLadderResult(bWin, DestRail);

	// 기본적으로 UserWidget 은 매 프레임 다시 그려지므로 별도 무효화 불필요.
	// (Invalidation Panel 안에 넣는 경우에만 Invalidate 를 호출하면 됨)
}

bool ULadderWidget::GetLayoutForSize(const FVector2D& Full, FVector2D& OutOrigin, FVector2D& OutDrawSize, float& OutDx) const
{
	if (Rails < 2 || Full.X <= 0.0f || Full.Y <= 0.0f)
	{
		OutOrigin = FVector2D::ZeroVector;
		OutDrawSize = FVector2D::ZeroVector;
		OutDx = 0.0f;
		return false;
	}

	const FVector2D Avail(
		FMath::Max(Full.X - LadderPadding * 2.0f, 0.0f),
		FMath::Max(Full.Y - LadderPadding * 2.0f, 0.0f));

	OutDrawSize = Avail;
	if (LadderSize.X > 0.0f && LadderSize.Y > 0.0f)
	{
		OutDrawSize.X = FMath::Min(LadderSize.X, Avail.X);
		OutDrawSize.Y = FMath::Min(LadderSize.Y, Avail.Y);
	}
	OutOrigin = (Full - OutDrawSize) * 0.5f;
	OutDx = OutDrawSize.X / (Rails - 1);
	return true;
}

FVector2D ULadderWidget::GetRailTop(int32 Rail) const
{
	const FVector2D Cached = GetCachedGeometry().GetLocalSize();
	const FVector2D Size = (Cached.X > 0.0f && Cached.Y > 0.0f) ? Cached : LastKnownSize;   // 평소엔 캐시, 첫 빌드 순간만 폴백
	FVector2D Origin, DrawSize; float Dx;
	if (!GetLayoutForSize(Size, Origin, DrawSize, Dx))
	{
		return FVector2D::ZeroVector;
	}
	Rail = FMath::Clamp(Rail, 0, Rails - 1);
	return FVector2D(Origin.X + Rail * Dx, Origin.Y);                    // 세로줄 맨 위
}

FVector2D ULadderWidget::GetRailBottom(int32 Rail) const
{
	const FVector2D Cached = GetCachedGeometry().GetLocalSize();
	const FVector2D Size = (Cached.X > 0.0f && Cached.Y > 0.0f) ? Cached : LastKnownSize;   // 평소엔 캐시, 첫 빌드 순간만 폴백
	FVector2D Origin, DrawSize; float Dx;
	if (!GetLayoutForSize(Size, Origin, DrawSize, Dx))
	{
		return FVector2D::ZeroVector;
	}
	Rail = FMath::Clamp(Rail, 0, Rails - 1);
	return FVector2D(Origin.X + Rail * Dx, Origin.Y + DrawSize.Y);       // 세로줄 맨 아래
}

void ULadderWidget::SetRailCount(int32 NewRails)
{
	Rails = FMath::Clamp(NewRails, MinRails, MaxRails);
	Rows  = FMath::Max(Rails * RowsPerRail, 2);   // 세로줄 수에 비례해 행 수도 변함
	ClearBoard();                                 // 개수 바뀌면 판 비우고 세로줄만
	BuildPayouts();                               // 배당 배열 개수 맞춰 재구성
	OnRailCountChanged(Rails);                     // BP: 버튼/결과 칸 재생성
}

void ULadderWidget::BuildPayouts()
{
	Payouts.Reset();
	for (int32 i = 0; i < Rails; ++i)
	{
		Payouts.Add(BasePayouts.IsValidIndex(i) ? BasePayouts[i] : 0);   // 셋이 모자라면 꽝(0)
	}
	// 순서 섞기 (Fisher-Yates)
	for (int32 i = Payouts.Num() - 1; i > 0; --i)
	{
		Payouts.Swap(i, FMath::RandRange(0, i));
	}
}

void ULadderWidget::ShuffleResults()
{
	for (int32 i = Payouts.Num() - 1; i > 0; --i)
	{
		Payouts.Swap(i, FMath::RandRange(0, i));
	}
	OnResultsChanged();   // BP: 결과 슬롯 텍스트 갱신
}

int32 ULadderWidget::GetPayout(int32 Rail) const
{
	return Payouts.IsValidIndex(Rail) ? Payouts[Rail] : 0;
}

void ULadderWidget::ClearBoard()
{
	DrawRung.Reset();          // 가로줄 없음 → NativePaint 는 세로줄만 그림
	PathRails.Reset();
	CrossRowsSaved.Reset();
	bTracing = false;
	TraceProgress = 0.0f;
}

void ULadderWidget::StartTrace()
{
	// GenerateLadder 로 경로가 만들어져 있어야 함
	if (PathRails.Num() < 2)
	{
		return;
	}
	TraceProgress = 0.0f;
	bTracing = true;
}

void ULadderWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 크기가 유효해진 첫 프레임에 딱 한 번 초기 빌드. (Construct 시점엔 크기 0이라 버튼/결과가 어긋나던 문제 해결)
	if (!bInitialBuildDone && MyGeometry.GetLocalSize().X > 1.0f && MyGeometry.GetLocalSize().Y > 1.0f)
	{
		bInitialBuildDone = true;
		LastKnownSize = MyGeometry.GetLocalSize();   // 이 순간엔 GetCachedGeometry가 아직 0일 수 있어 폴백용으로 저장
		SetRailCount(Rails);                          // OnRailCountChanged → 버튼/결과 스폰
	}

	if (!bTracing)
	{
		return;
	}

	TraceProgress += (TraceDuration > 0.0f) ? (InDeltaTime / TraceDuration) : 1.0f;

	if (TraceProgress >= 1.0f)
	{
		TraceProgress = 1.0f;
		bTracing = false;

		// 정산: 결과값(배당)만큼 자금에 더함. (2단계에서 BetAmount * LastPayout 로 바뀔 예정)
		Balance += LastPayout;
		OnBalanceChanged(Balance);

		OnTraceFinished(bWin, DestRail);   // BP에서 당첨/꽝 연출, BGM 정지 등
	}
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

	FVector2D Origin, DrawSize;
	float dx;
	GetLayoutForSize(AllottedGeometry.GetLocalSize(), Origin, DrawSize, dx);   // 세로줄 좌표 함수와 공용

	// 가로줄이 차지하는 세로 영역은 레일보다 위아래로 RailOverhang 만큼 안쪽
	const float RungTop    = Origin.Y + RailOverhang;
	const float RungHeight = FMath::Max(DrawSize.Y - RailOverhang * 2.0f, 0.0f);
	const float dy = RungHeight / (Rows - 1);     // 행 간격
	const FPaintGeometry PaintGeo = AllottedGeometry.ToPaintGeometry();

	// 세로줄 (가로줄 영역보다 위아래로 RailOverhang 만큼 더 뻗음)
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
		const float y = RungTop + r * dy;
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

	// ── 실제 경로 트레이스(빨강) + 구슬 ──
	if (PathRails.Num() >= 2 && TraceProgress > 0.0f)
	{
		auto RailX = [&](int32 Rail) { return Origin.X + Rail * dx; };
		auto RowY  = [&](int32 Row)  { return RungTop + Row * dy; };

		// 픽셀 경로 폴리라인: 맨 위 → (내려가고 옆으로 건너기 반복) → 맨 아래
		TArray<FVector2D> Poly;
		Poly.Add(FVector2D(RailX(PathRails[0]), Origin.Y));
		for (int32 k = 0; k < CrossRowsSaved.Num(); ++k)
		{
			const float cy = RowY(CrossRowsSaved[k]);
			Poly.Add(FVector2D(RailX(PathRails[k]),     cy));   // 현재 줄로 내려옴
			Poly.Add(FVector2D(RailX(PathRails[k + 1]), cy));   // 옆 줄로 건넘
		}
		Poly.Add(FVector2D(RailX(PathRails.Last()), Origin.Y + DrawSize.Y));

		// 총 길이
		float Total = 0.0f;
		for (int32 i = 1; i < Poly.Num(); ++i)
		{
			Total += FVector2D::Distance(Poly[i - 1], Poly[i]);
		}
		const float Target = Total * FMath::Clamp(TraceProgress, 0.0f, 1.0f);

		// 진행 지점까지 폴리라인 잘라내기 + 구슬 위치(Head) 계산
		TArray<FVector2D> Trav;
		Trav.Add(Poly[0]);
		FVector2D Head = Poly[0];
		float Acc = 0.0f;
		for (int32 i = 1; i < Poly.Num(); ++i)
		{
			const float Seg = FVector2D::Distance(Poly[i - 1], Poly[i]);
			if (Seg <= KINDA_SMALL_NUMBER) continue;
			if (Acc + Seg <= Target)
			{
				Trav.Add(Poly[i]);
				Head = Poly[i];
				Acc += Seg;
			}
			else
			{
				const float t = (Target - Acc) / Seg;
				Head = FMath::Lerp(Poly[i - 1], Poly[i], t);
				Trav.Add(Head);
				break;
			}
		}

		// 지나온 경로 = 빨강 (기존 흰 선 위 레이어)
		if (Trav.Num() >= 2)
		{
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, PaintGeo, Trav,
				ESlateDrawEffect::None, TraceColor, true, LineThickness + 1.0f);
		}

		// 구슬 (MarbleBrush 지정 시 그 이미지, 없으면 흰 사각형에 틴트)
		static const FSlateColorBrush WhiteFill(FLinearColor::White);
		const FSlateBrush* Brush = (MarbleBrush.GetResourceObject() != nullptr) ? &MarbleBrush : &WhiteFill;
		const FVector2D MHalf(MarbleSize * 0.5f, MarbleSize * 0.5f);
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 2,
			AllottedGeometry.ToPaintGeometry(FVector2D(MarbleSize, MarbleSize),
				FSlateLayoutTransform(1.0f, Head - MHalf)),
			Brush, ESlateDrawEffect::None, MarbleColor);

		return LayerId + 3;
	}

	return LayerId + 1;
}
