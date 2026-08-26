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

	Track = CreateDefaultSubobject<UStaticMeshComponent>("Track");
	NPCSpawnPoints = CreateDefaultSubobject<USceneComponent>("NPCSpawnPoints");
	NPCSpawnPoints->SetupAttachment(Track);

}

void ARaceManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARaceManager, Phase);
	DOREPLIFETIME(ARaceManager, WinnerIndex);
	DOREPLIFETIME(ARaceManager, FinishOrder);
}

void ARaceManager::BeginPlay()
{
	Super::BeginPlay();
	FActorSpawnParameters Params;
	Params.Owner = this;
	RaceNPC = GetWorld()->SpawnActor<ANPC_Base>(NPCClass, NPCSpawnPoints->GetComponentTransform(), Params);

	if (HasAuthority()) StartNewRound();
}

FRaceRunnerStats ARaceManager::RollStats(int32 LaneIndex) const
{
	static const int32 Lo[4] = { 65, 75, 85, 95 };
	static const int32 Hi[4] = { 74, 84, 94, 104 };
	const int32 Bucket = (LaneIndex < 4) ? LaneIndex : FMath::RandRange(0, 3);

	FRaceRunnerStats S;
	S.Age           = FMath::RandRange(Lo[Bucket], Hi[Bucket]);
	S.Name          = KRNames[FMath::RandRange(0, UE_ARRAY_COUNT(KRNames) - 1)];
	S.BaseSpeed     = 225.f - (S.Age - 60) * 2.6f;
	S.AwakenChance  = FMath::Max(0.f, (S.Age - 68) / 27.f) * 0.32f;
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
	FinishOrder.Reset();
	RaceElapsed = 0.f;
	RaceDuration = 0.f;
	bResultBroadcast = false;

	if (!RunnerClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[RaceManager] RunnerClass 미지정 - 러너 BP를 지정해줘"));
		return;
	}

	const bool bUseSpawnPoints = RunnerSpawnPoints.Num() > 0;
	const int32 Count = bUseSpawnPoints ? RunnerSpawnPoints.Num() : NumRunners;

	const FVector DirN = RaceDirection.GetSafeNormal();
	const FVector Side = FVector::CrossProduct(DirN, FVector::UpVector).GetSafeNormal();

	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	for (int32 i = 0; i < Count; ++i)
	{
		FVector  Loc;
		FRotator Rot;
		if (bUseSpawnPoints)
		{
			if (!RunnerSpawnPoints[i]) continue;                    // 순서대로: i번 지점 = i번 러너
			Loc = RunnerSpawnPoints[i]->GetActorLocation();
			Rot = RunnerSpawnPoints[i]->GetActorRotation();
		}
		else
		{
			Loc = StartLocation + Side * (LaneSpacing * i);   // 폴백: 하드코딩 계산
			Rot = DirN.Rotation();
		}

		ARaceRunner* R = GetWorld()->SpawnActor<ARaceRunner>(RunnerClass, Loc, Rot, SP);
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

	float MaxTime = 0.f;
	TArray<TPair<int32, float>> Times;   // {러너 인덱스, 완주 시각}

	for (int32 i = 0; i < Runners.Num(); ++i)
	{
		ARaceRunner* Rn = Runners[i];
		if (!Rn) continue;

		// 이번 판 레시피 롤 (랜덤은 여기서 한 번에 다 굴림). 방향 = 러너가 바라보는 쪽(스폰지점 방향).
		const FRunnerRaceScript Script = RollScript(Rn->Stats, Rn->GetActorLocation(), Rn->GetActorForwardVector());
		Rn->ServerSetupScript(Script);

		// 완주 시각 결정론 계산 → 순위 판정용
		const float FinishT = SimulateFinishTime(Script);
		Times.Add({ i, FinishT });
		MaxTime = FMath::Max(MaxTime, FinishT);
	}

	// 완주 시각 오름차순 = 완주 순위
	Times.Sort([](const TPair<int32, float>& A, const TPair<int32, float>& B) { return A.Value < B.Value; });
	FinishOrder.Reset();
	for (const TPair<int32, float>& T : Times) FinishOrder.Add(T.Key);

	WinnerIndex = FinishOrder.Num() > 0 ? FinishOrder[0] : -1;   // 1등 (복제됨) — 결승 전에 이미 정해짐
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
	FinishOrder.Reset();
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
