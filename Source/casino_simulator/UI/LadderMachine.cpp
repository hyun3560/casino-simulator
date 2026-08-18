// LadderMachine.cpp

#include "LadderMachine.h"

ALadderMachine::ALadderMachine()
{
	// 로직 액터라 매 프레임 틱 불필요. (트레이스 연출은 위젯이 담당)
	PrimaryActorTick.bCanEverTick = false;
}

void ALadderMachine::BeginPlay()
{
	Super::BeginPlay();

	// 판 초기화: 기본 줄개수로 Rows/배당 세팅. (초기 UI 방송은 BP가 바인딩 후 BroadcastInitialState 호출)
	SetRailCount(Rails);
}

void ALadderMachine::BroadcastInitialState()
{
	OnRailCountChanged.Broadcast(Rails);
	OnResultsChanged.Broadcast();
	OnModeChanged.Broadcast(Mode);
	OnCursorChanged.Broadcast(RailCursor);
	OnBetChanged.Broadcast(BetAmount);
	OnBalanceChanged.Broadcast(Balance);
	OnBoardCleared.Broadcast();
}

// ─────────── Config ops ───────────

void ALadderMachine::SetRailCount(int32 NewRails)
{
	Rails = FMath::Clamp(NewRails, MinRails, MaxRails);
	Rows  = FMath::Max(Rails * RowsPerRail, 2);   // 세로줄 수에 비례해 행도 변함

	RailCursor = FMath::Clamp(RailCursor, 0, Rails - 1);

	ClearBoard();       // 개수 바뀌면 판 비우고 세로줄만 (OnBoardCleared 방송 포함)
	BuildPayouts();     // 배당 배열 개수 맞춰 재구성

	OnRailCountChanged.Broadcast(Rails);   // BP: 버튼/결과칸 재생성
	OnResultsChanged.Broadcast();          // BP: 결과 슬롯 텍스트 갱신
	OnCursorChanged.Broadcast(RailCursor); // 커서 하이라이트 갱신
}

void ALadderMachine::BuildPayouts()
{
	Payouts.Reset();
	for (int32 i = 0; i < Rails; ++i)
	{
		Payouts.Add(BasePayouts.IsValidIndex(i) ? BasePayouts[i] : 0);   // 모자라면 꽝(0)
	}
	// Fisher-Yates 셔플
	for (int32 i = Payouts.Num() - 1; i > 0; --i)
	{
		Payouts.Swap(i, FMath::RandRange(0, i));
	}
}

void ALadderMachine::ShuffleResults()
{
	for (int32 i = Payouts.Num() - 1; i > 0; --i)
	{
		Payouts.Swap(i, FMath::RandRange(0, i));
	}
	OnResultsChanged.Broadcast();
}

int32 ALadderMachine::GetPayout(int32 Rail) const
{
	return Payouts.IsValidIndex(Rail) ? Payouts[Rail] : 0;
}

// ─────────── Money ops ───────────

void ALadderMachine::ClampBet()
{
	const int32 MaxBet = FMath::Max(Balance, BetStep);   // 잔액까지만 (최소 BetStep)
	BetAmount = FMath::Clamp(BetAmount, BetStep, MaxBet);
	OnBetChanged.Broadcast(BetAmount);
}

void ALadderMachine::ChangeBet(int32 Steps)
{
	BetAmount += Steps * BetStep;
	ClampBet();
}

// ─────────── Nav ───────────

void ALadderMachine::SetRailCursor(int32 NewCursor)
{
	RailCursor = FMath::Clamp(NewCursor, 0, Rails - 1);
	OnCursorChanged.Broadcast(RailCursor);
}

void ALadderMachine::NavCycle()
{
	if (bResultOpen) return;   // 결과창 열림 중엔 입력 무시

	const uint8 Next = (static_cast<uint8>(Mode) + 1) % 4;
	Mode = static_cast<ELadderMode>(Next);
	OnModeChanged.Broadcast(Mode);
}

void ALadderMachine::NavLeft()
{
	if (bResultOpen) return;   // 결과창 열림 중엔 입력 무시

	switch (Mode)
	{
	case ELadderMode::RailSelect: SetRailCursor(RailCursor - 1); break;   // 왼쪽 줄
	case ELadderMode::RailCount:  RemoveRail();                  break;   // 줄 하나 줄임(커서 클램프/방송 포함)
	case ELadderMode::Bet:        ChangeBet(-1);                 break;
	default: break;                                                       // 새로고침: 좌우 없음
	}
}

void ALadderMachine::NavRight()
{
	if (bResultOpen) return;   // 결과창 열림 중엔 입력 무시

	switch (Mode)
	{
	case ELadderMode::RailSelect: SetRailCursor(RailCursor + 1); break;   // 오른쪽 줄
	case ELadderMode::RailCount:  AddRail();                     break;   // 줄 하나 늘림
	case ELadderMode::Bet:        ChangeBet(1);                  break;
	default: break;
	}
}

void ALadderMachine::NavSelect()
{
	if (bResultOpen) return;   // 결과창 열림 중엔 입력 무시

	switch (Mode)
	{
	case ELadderMode::RailSelect:
		if (!bIsPlaying)                          // 트레이스 중이면 무시
		{
			if (Balance >= BetAmount)             // 잔액 충분 → 시작
			{
				Play(RailCursor);
			}
			else
			{
				OnInsufficientFunds.Broadcast();  // 잔액 부족
			}
		}
		break;

	case ELadderMode::Refresh:
		ClearBoard();       // 판 비우기 → 세로줄만
		ShuffleResults();   // 배당 셔플
		break;

	default:
		break;              // 줄개수/배팅 모드: 선택 동작 없음
	}
}

void ALadderMachine::ResetRound()
{
	const bool bWasResultOpen = bResultOpen;

	bResultOpen = false;              // 결과창 닫힘 → Nav 입력 다시 허용
	ClearBoard();                     // 판 비움(+OnBoardCleared)
	Mode = ELadderMode::RailSelect;
	OnModeChanged.Broadcast(Mode);
	SetRailCursor(0);                 // 커서 0 (+OnCursorChanged)
	ClampBet();                       // 잔액 줄었으면 배팅액 맞춤 (+OnBetChanged)

	if (bWasResultOpen)
	{
		OnResultClosed.Broadcast();   // 스크린: 결과창 숨기기 + 배경 딤 해제
	}
}

// ─────────── Play / Settle ───────────

void ALadderMachine::Play(int32 StartRail)
{
	if (Rails < 2 || Rows < 2)
	{
		return;
	}

	// (1) 스테이크 차감 — 즉시 표시 (돈 내고 시작하는 거라 스포일러 아님)
	StakedThisRound = FMath::Min(BetAmount, Balance);
	Balance -= StakedThisRound;
	OnBalanceChanged.Broadcast(Balance);

	// (2) 결과 확정 (순수 생성기)
	CurrentPlan = ULadderGenerator::Generate(Rails, Rows, StartRail, Payouts, FakeRungChance);

	// (3) 정산까지 여기서 끝냄 — 당첨금을 내부 Balance에 바로 반영.
	//     단 잔액 "표시"는 아직 방송하지 않는다(공이 떨어지기 전 스포일러 방지).
	//     내부 Balance는 최종값(멀티에선 서버 권위값). 표시는 RevealFinished 에서.
	const int32 Winnings = StakedThisRound * CurrentPlan.Payout;
	Balance += Winnings;

	bIsPlaying = true;
	OnPlayStarted.Broadcast(CurrentPlan);   // 위젯이 트레이스 시작
}

void ALadderMachine::RevealFinished()
{
	if (!bIsPlaying)
	{
		return;   // 진행 중인 판이 없으면 무시 (중복 방지)
	}
	bIsPlaying = false;
	bResultOpen = true;   // 결과창 열림 → 이 동안 Nav 입력 차단

	// 돈은 Play에서 이미 정산됨. 여기선 최종 잔액 "표시" + 결과창만.
	const int32 Winnings = StakedThisRound * CurrentPlan.Payout;
	OnBalanceChanged.Broadcast(Balance);   // 이제 당첨금 반영된 최종 잔액 표시
	OnResult.Broadcast(CurrentPlan.bWin, CurrentPlan.DestRail, CurrentPlan.Payout, Winnings);
}

// ─────────── helpers ───────────

void ALadderMachine::ClearBoard()
{
	CurrentPlan = FLadderPlan();
	bIsPlaying = false;
	OnBoardCleared.Broadcast();   // 위젯: 가로줄/트레이스 제거 → 세로줄만
}
