// RaceManager.cpp
#include "RaceManager.h"
#include "RaceRunner.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

static const TCHAR* KRNames[] = {
	TEXT("김춘수"), TEXT("박막례"), TEXT("이순자"), TEXT("최봉팔"),
	TEXT("정갑수"), TEXT("오만득"), TEXT("한상철"), TEXT("서말자"),
	TEXT("류판석"), TEXT("강달수"), TEXT("문영자"), TEXT("고만수")
};

// 레시피로 완주 시각을 결정론적으로 계산 (고정 스텝). 승자 판정용.
static float SimulateFinishTime(const FRunnerRaceScript& S)
{
	const float Dt = 1.f / 120.f;
	float T = 0.f, Pos = 0.f, StumbleUntil = 0.f;
	bool bAwake = false, bStumbled = false;

	for (int32 i = 0; i < 200000 && Pos < S.TrackLength; ++i)
	{
		if (S.bWillAwaken && !bAwake && Pos >= S.AwakenAtPos) { bAwake = true; }
		if (!bAwake && S.bWillStumble && !bStumbled && Pos >= S.StumbleAtPos) { bStumbled = true; StumbleUntil = T + 0.7f; }

		float Mult = bAwake ? 2.3f : 1.f;
		if (T < StumbleUntil) Mult *= 0.3f;

		Pos += S.Speed * Mult * Dt;
		T += Dt;
	}
	return T;
}

ARaceManager::ARaceManager()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
}

void ARaceManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARaceManager, Phase);
	DOREPLIFETIME(ARaceManager, WinnerIndex);
}

void ARaceManager::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority()) StartNewRound();
}

void ARaceManager::ShuffleBuckets()
{
	LaneBuckets = { 0, 1, 2, 3 };   // 구간 4개

	// Fisher–Yates 셔플
	for (int32 i = LaneBuckets.Num() - 1; i > 0; --i)
	{
		const int32 j = FMath::RandRange(0, i);
		LaneBuckets.Swap(i, j);
	}
}

FRaceRunnerStats ARaceManager::RollStats(int32 LaneIndex) const
{
	static const int32 Lo[4] = { 65, 75, 85, 95 };
	static const int32 Hi[4] = { 74, 84, 94, 104 };

	// LaneIndex는 유지 → 그 레인에 배정된 구간만 셔플된 값에서 가져옴
	const int32 Bucket = LaneBuckets.IsValidIndex(LaneIndex)
		? LaneBuckets[LaneIndex]
		: LaneIndex;   // 준비 안 됐으면 예전처럼 순서대로 폴백

	FRaceRunnerStats S;
	S.Age = FMath::RandRange(Lo[Bucket], Hi[Bucket]);
	S.Name = KRNames[FMath::RandRange(0, UE_ARRAY_COUNT(KRNames) - 1)];
	S.BaseSpeed = 225.f - (S.Age - 60) * 2.6f;
	S.AwakenChance = FMath::Max(0.f, (S.Age - 68) / 27.f) * 0.32f;
	S.StumbleChance = FMath::Max(0.f, (S.Age - 68) / 27.f) * 0.30f;
	const float Raw = 1.8f + FMath::Pow((S.Age - 60) / 35.f, 1.4f) * 7.f;
	S.Odds = FMath::RoundToFloat(Raw * 10.f) / 10.f;
	return S;
}

FRunnerRaceScript ARaceManager::RollScript(const FRaceRunnerStats& S, const FVector& StartLoc, const FVector& Dir) const
{
	FRunnerRaceScript R;
	R.StartLoc     = StartLoc;
	R.Dir          = Dir.GetSafeNormal();
	R.TrackLength  = TrackLength;
	R.Speed        = S.BaseSpeed * FMath::FRandRange(0.85f, 1.15f);   // 운 반영, 레이스 내내 고정
	R.bWillAwaken  = FMath::FRand() < S.AwakenChance;
	R.AwakenAtPos  = TrackLength * FMath::FRandRange(0.35f, 0.55f);
	R.bWillStumble = FMath::FRand() < S.StumbleChance;
	R.StumbleAtPos = TrackLength * FMath::FRandRange(0.25f, 0.70f);
	return R;
}

void ARaceManager::StartNewRound()
{
	if (!HasAuthority()) return;

	for (ARaceRunner* R : Runners) { if (R) R->Destroy(); }
	Runners.Reset();
	WinnerIndex = -1;
	RaceElapsed = 0.f;
	RaceDuration = 0.f;
	bResultBroadcast = false;

	if (!RunnerClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[RaceManager] RunnerClass 미지정 - 러너 BP를 지정해줘"));
		return;
	}

	const FVector DirN = RaceDirection.GetSafeNormal();
	const FVector Side = FVector::CrossProduct(DirN, FVector::UpVector).GetSafeNormal();

	ShuffleBuckets();
	for (int32 i = 0; i < NumRunners; ++i)
	{
		const FVector Loc = StartLocation + Side * (LaneSpacing * i);

		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ARaceRunner* R = GetWorld()->SpawnActor<ARaceRunner>(RunnerClass, Loc, DirN.Rotation(), SP);
		if (!R) continue;

		R->InitStats(RollStats(i));
		R->ServerSetRacing(false);
		Runners.Add(R);
	}

	Phase = ERacePhase::Betting;
	OnRep_Phase();
	OnLineupReady.Broadcast();
}

void ARaceManager::StartRace()
{
	if (!HasAuthority() || Phase != ERacePhase::Betting) return;

	const FVector DirN = RaceDirection.GetSafeNormal();
	float BestTime = 1.0e9f, MaxTime = 0.f;
	int32 Best = -1;

	for (int32 i = 0; i < Runners.Num(); ++i)
	{
		ARaceRunner* Rn = Runners[i];
		if (!Rn) continue;

		// 이번 판 레시피 롤 (랜덤은 여기서 한 번에 다 굴림)
		const FRunnerRaceScript Script = RollScript(Rn->Stats, Rn->GetActorLocation(), DirN);
		Rn->ServerSetupScript(Script);

		// 승자 판정: 레시피로 완주 시각 결정론 계산
		const float FinishT = SimulateFinishTime(Script);
		if (FinishT < BestTime) { BestTime = FinishT; Best = i; }
		MaxTime = FMath::Max(MaxTime, FinishT);
	}

	WinnerIndex = Best;                 // 결과 확정 (복제됨) — 결승 전에 이미 정해짐
	RaceDuration = MaxTime + 0.3f;      // 전원 완주 시간 + 버퍼
	RaceElapsed = 0.f;
	bResultBroadcast = false;

	for (ARaceRunner* Rn : Runners) { if (Rn) Rn->ServerSetRacing(true); }   // 출발 신호(복제)

	Phase = ERacePhase::Racing;
	OnRep_Phase();
	OnRaceStarted.Broadcast();
}

void ARaceManager::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!HasAuthority() || Phase != ERacePhase::Racing) return;

	RaceElapsed += Dt;
	if (!bResultBroadcast && RaceElapsed >= RaceDuration)
	{
		bResultBroadcast = true;
		Phase = ERacePhase::Finished;
		OnRep_Phase();
		OnRaceFinished.Broadcast(GetWinner(), WinnerIndex);

		// TODO(정산): PlayerState 순회 → 우승마(GetWinner())에 건 플레이어에게 bet * Odds 지급
	}
}

void ARaceManager::ResetRace()
{
	if (!HasAuthority()) return;
	Phase = ERacePhase::Idle;
	WinnerIndex = -1;
	bResultBroadcast = false;
	for (ARaceRunner* R : Runners) { if (R) R->ServerSetRacing(false); }
	OnRep_Phase();
}

ARaceRunner* ARaceManager::GetWinner() const
{
	return Runners.IsValidIndex(WinnerIndex) ? Runners[WinnerIndex] : nullptr;
}

void ARaceManager::OnRep_Phase()
{
	// 클라: 페이즈 바뀔 때 UI 전환 등 (BP에서 OnRep 후크로 처리)
}
