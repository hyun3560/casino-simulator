// Copyright Epic Games, Inc. All Rights Reserved.

#include "Economy/CasinoShopComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameplayEffect.h"
#include "Net/UnrealNetwork.h"
#include "casino_simulatorAttributeSet.h"

UCasinoShopComponent::UCasinoShopComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	FCasinoShopItemData CheapCigarette;
	CheapCigarette.ItemId = TEXT("CheapCigarette");
	CheapCigarette.DisplayName = FText::FromString(TEXT("Cheap Cigarette"));
	CheapCigarette.Category = ECasinoShopItemCategory::Cigarette;
	CheapCigarette.BasePrice = 100;
	CheapCigarette.RecoveryType = ECasinoShopRecoveryType::Nicotine;
	CheapCigarette.RestoreAmount = 15.0f;
	CheapCigarette.Description = FText::FromString(TEXT("Restores a small amount of nicotine."));

	FCasinoShopItemData RegularCigarette;
	RegularCigarette.ItemId = TEXT("RegularCigarette");
	RegularCigarette.DisplayName = FText::FromString(TEXT("Regular Cigarette"));
	RegularCigarette.Category = ECasinoShopItemCategory::Cigarette;
	RegularCigarette.BasePrice = 180;
	RegularCigarette.RecoveryType = ECasinoShopRecoveryType::Nicotine;
	RegularCigarette.RestoreAmount = 30.0f;
	RegularCigarette.Description = FText::FromString(TEXT("Restores a moderate amount of nicotine."));

	FCasinoShopItemData Beer;
	Beer.ItemId = TEXT("Beer");
	Beer.DisplayName = FText::FromString(TEXT("Beer"));
	Beer.Category = ECasinoShopItemCategory::Alcohol;
	Beer.BasePrice = 150;
	Beer.RecoveryType = ECasinoShopRecoveryType::Alcohol;
	Beer.RestoreAmount = 15.0f;
	Beer.Description = FText::FromString(TEXT("Restores a small amount of alcohol."));

	FCasinoShopItemData Whiskey;
	Whiskey.ItemId = TEXT("Whiskey");
	Whiskey.DisplayName = FText::FromString(TEXT("Whiskey"));
	Whiskey.Category = ECasinoShopItemCategory::Alcohol;
	Whiskey.BasePrice = 300;
	Whiskey.RecoveryType = ECasinoShopRecoveryType::Alcohol;
	Whiskey.RestoreAmount = 35.0f;
	Whiskey.Description = FText::FromString(TEXT("Restores a large amount of alcohol."));

	ShopItems = { CheapCigarette, RegularCigarette, Beer, Whiskey };
}

void UCasinoShopComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UCasinoShopComponent, CurrentDay);
}

TArray<FCasinoShopItemData> UCasinoShopComponent::GetShopItemsByCategory(ECasinoShopItemCategory Category) const
{
	TArray<FCasinoShopItemData> MatchingItems;
	for (const FCasinoShopItemData& Item : ShopItems)
	{
		if (Item.Category == Category)
		{
			MatchingItems.Add(Item);
		}
	}

	return MatchingItems;
}

bool UCasinoShopComponent::GetShopItem(FName ItemId, FCasinoShopItemData& OutItem) const
{
	if (const FCasinoShopItemData* Item = FindShopItem(ItemId))
	{
		OutItem = *Item;
		return true;
	}

	return false;
}

int32 UCasinoShopComponent::GetItemUnitPrice(FName ItemId) const
{
	if (const FCasinoShopItemData* Item = FindShopItem(ItemId))
	{
		return GetScaledPrice(Item->BasePrice);
	}

	return 0;
}

int32 UCasinoShopComponent::GetItemTotalPrice(FName ItemId, int32 Quantity) const
{
	return GetItemUnitPrice(ItemId) * FMath::Max(Quantity, 0);
}

bool UCasinoShopComponent::BuyShopItem(FName ItemId, int32 Quantity)
{
	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		ServerBuyShopItem(ItemId, Quantity);
		return true;
	}

	return ProcessPurchase(ItemId, Quantity);
}

bool UCasinoShopComponent::BuyCigarette(int32 Quantity)
{
	return BuyShopItem(TEXT("RegularCigarette"), Quantity);
}

bool UCasinoShopComponent::BuyAlcohol(int32 Quantity)
{
	return BuyShopItem(TEXT("Beer"), Quantity);
}

void UCasinoShopComponent::SetCurrentDay(int32 NewCurrentDay)
{
	CurrentDay = FMath::Max(NewCurrentDay, 1);
}

float UCasinoShopComponent::GetCurrentPriceMultiplier() const
{
	const int32 DayIndex = FMath::Max(CurrentDay - 1, 0);
	return 1.0f + (PriceIncreasePerDay * DayIndex);
}

void UCasinoShopComponent::ServerBuyShopItem_Implementation(FName ItemId, int32 Quantity)
{
	ProcessPurchase(ItemId, Quantity);
}

void UCasinoShopComponent::ClientPurchaseCompleted_Implementation(FName ItemId, int32 Quantity, int32 TotalPrice)
{
	OnPurchaseCompleted.Broadcast(ItemId, Quantity, TotalPrice);
}

void UCasinoShopComponent::ClientPurchaseFailed_Implementation(const FString& Reason)
{
	OnPurchaseFailed.Broadcast(Reason);
}

bool UCasinoShopComponent::ProcessPurchase(FName ItemId, int32 Quantity)
{
	FString Reason;
	if (!ValidateQuantity(Quantity, Reason))
	{
		FailPurchase(Reason);
		return false;
	}

	const FCasinoShopItemData* Item = FindShopItem(ItemId);
	if (!Item)
	{
		FailPurchase(TEXT("Shop item was not found."));
		return false;
	}

	const int32 TotalPrice = GetItemTotalPrice(ItemId, Quantity);
	if (!TrySpendForPurchase(TotalPrice))
	{
		FailPurchase(TEXT("Not enough personal money."));
		return false;
	}

	if (!ApplyItemEffects(*Item, Quantity))
	{
		FailPurchase(TEXT("Could not apply item effect."));
		return false;
	}

	ClientPurchaseCompleted(ItemId, Quantity, TotalPrice);
	OnPurchaseCompleted.Broadcast(ItemId, Quantity, TotalPrice);
	return true;
}

bool UCasinoShopComponent::TrySpendForPurchase(int32 Price)
{
	if (bAllowFreePurchasesUntilEconomyExists)
	{
		return true;
	}

	// TODO: Connect this to the private personal-money component once cash/chips exist.
	return Price <= 0;
}

bool UCasinoShopComponent::ApplyItemEffects(const FCasinoShopItemData& Item, int32 Quantity)
{
	bool bAppliedAnyEffect = false;
	const float EffectLevel = FMath::Max(static_cast<float>(Quantity), 1.0f);

	if (ApplyGameplayEffect(Item.RecoveryEffectClass, EffectLevel))
	{
		bAppliedAnyEffect = true;
	}

	for (const TSubclassOf<UGameplayEffect>& BonusEffectClass : Item.BonusEffectClasses)
	{
		if (ApplyGameplayEffect(BonusEffectClass, EffectLevel))
		{
			bAppliedAnyEffect = true;
		}
	}

	const float TotalRecovery = Item.RestoreAmount * Quantity;
	if (ApplyFallbackAttributeRecovery(Item, TotalRecovery))
	{
		bAppliedAnyEffect = true;
	}

	return bAppliedAnyEffect || Item.RecoveryType == ECasinoShopRecoveryType::None;
}

bool UCasinoShopComponent::ApplyGameplayEffect(TSubclassOf<UGameplayEffect> EffectClass, float Level)
{
	if (!EffectClass)
	{
		return false;
	}

	AActor* Owner = GetOwner();
	const IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(Owner);
	UAbilitySystemComponent* AbilitySystemComponent = AbilitySystemInterface ? AbilitySystemInterface->GetAbilitySystemComponent() : nullptr;
	if (!AbilitySystemComponent)
	{
		return false;
	}

	FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	const FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(EffectClass, Level, EffectContext);
	if (!SpecHandle.IsValid())
	{
		return false;
	}

	AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	return true;
}

bool UCasinoShopComponent::ApplyFallbackAttributeRecovery(const FCasinoShopItemData& Item, float TotalRecovery)
{
	if (Item.RecoveryType == ECasinoShopRecoveryType::None || TotalRecovery <= 0.0f)
	{
		return false;
	}

	AActor* Owner = GetOwner();
	const IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(Owner);
	UAbilitySystemComponent* AbilitySystemComponent = AbilitySystemInterface ? AbilitySystemInterface->GetAbilitySystemComponent() : nullptr;
	if (!AbilitySystemComponent)
	{
		return false;
	}

	FGameplayAttribute Attribute;
	if (Item.RecoveryType == ECasinoShopRecoveryType::Nicotine)
	{
		Attribute = Ucasino_simulatorAttributeSet::GetNicotineAttribute();
	}
	else if (Item.RecoveryType == ECasinoShopRecoveryType::Alcohol)
	{
		Attribute = Ucasino_simulatorAttributeSet::GetAlcoholAttribute();
	}
	else
	{
		return false;
	}

	AbilitySystemComponent->ApplyModToAttribute(Attribute, EGameplayModOp::Additive, TotalRecovery);
	return true;
}

bool UCasinoShopComponent::ValidateQuantity(int32 Quantity, FString& OutReason) const
{
	if (Quantity <= 0)
	{
		OutReason = TEXT("Quantity must be greater than zero.");
		return false;
	}

	return true;
}

int32 UCasinoShopComponent::GetScaledPrice(int32 BasePrice) const
{
	return FMath::Max(FMath::RoundToInt(BasePrice * GetCurrentPriceMultiplier()), 1);
}

const FCasinoShopItemData* UCasinoShopComponent::FindShopItem(FName ItemId) const
{
	return ShopItems.FindByPredicate([ItemId](const FCasinoShopItemData& Item)
	{
		return Item.ItemId == ItemId;
	});
}

void UCasinoShopComponent::FailPurchase(const FString& Reason)
{
	ClientPurchaseFailed(Reason);
	OnPurchaseFailed.Broadcast(Reason);
}
