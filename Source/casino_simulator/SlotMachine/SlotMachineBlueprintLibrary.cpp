#include "SlotMachine/SlotMachineBlueprintLibrary.h"
#include "SlotMachine/SlotMachinePaylines.h"
#include "SlotMachine/SlotMachineEvaluator.h"
#include "SlotMachine/SlotMachinePayoutCalculator.h"


TArray<int32> USlotMachineBlueprintLibrary::GetPaylineIndexes(int32 LineIndex)
{
	return FSlotMachinePaylines::GetPaylineIndexes(LineIndex);
}

ESlotLineType USlotMachineBlueprintLibrary::GetPaylineType(int32 LineIndex)
{
	return FSlotMachinePaylines::GetPaylineType(LineIndex);
}

int32 USlotMachineBlueprintLibrary::GetPaylineCount()
{
	return FSlotMachinePaylines::GetPaylineCount();
}


bool USlotMachineBlueprintLibrary::IsFullScreenMatch(const TArray<ESlotSymbol>& ResultSymbols, ESlotSymbol& OutSymbol)
{
	return FSlotMachineEvaluator::IsFullScreenMatch(ResultSymbols, OutSymbol);
}


ESlotResultType USlotMachineBlueprintLibrary::GetResultTypeByMatchCount(int32 MatchCount)
{
	return FSlotMachineEvaluator::GetResultTypeByMatchCount(MatchCount);
}

bool USlotMachineBlueprintLibrary::IsLineEligibleForMatchCount(ESlotLineType LineType, int32 MatchCount)
{
	return FSlotMachineEvaluator::IsLineEligibleForMatchCount(LineType, MatchCount);
}


bool USlotMachineBlueprintLibrary::EvaluateSingleLine(
	const TArray<ESlotSymbol>& ResultSymbols,
	int32 LineIndex,
	FSlotLineWin& OutLineWin)
{
	return FSlotMachineEvaluator::EvaluateSingleLine(ResultSymbols, LineIndex, OutLineWin);
}

FSlotSpinResult USlotMachineBlueprintLibrary::EvaluateSlotResult(const TArray<ESlotSymbol>& ResultSymbols)
{
	return FSlotMachineEvaluator::EvaluateSlotResult(ResultSymbols);
}

FSlotSpinResult USlotMachineBlueprintLibrary::EvaluateSlotResultWithPayout(
	const TArray<ESlotSymbol>& ResultSymbols,
	UDataTable* SymbolDataTable,
	UDataTable* PayoutDataTable,
	int32 BetAmount
)
{
	const FSlotSpinResult SpinResult = FSlotMachineEvaluator::EvaluateSlotResult(ResultSymbols);

	return FSlotMachinePayoutCalculator::ApplyPayoutToSpinResult(
		SpinResult,
		SymbolDataTable,
		PayoutDataTable,
		BetAmount
	);
}

FName USlotMachineBlueprintLibrary::GetSlotSymbolRowName(ESlotSymbol Symbol)
{
	switch (Symbol)
	{
	case ESlotSymbol::Cherry:
		return TEXT("Cherry");

	case ESlotSymbol::Bell:
		return TEXT("Bell");

	case ESlotSymbol::Game:
		return TEXT("Game");

	case ESlotSymbol::Diamond:
		return TEXT("Diamond");

	case ESlotSymbol::Watermelon:
		return TEXT("Watermelon");

	case ESlotSymbol::Star:
		return TEXT("Star");

	case ESlotSymbol::Grape:
		return TEXT("Grape");

	case ESlotSymbol::None:
	default:
		return NAME_None;
	}
}