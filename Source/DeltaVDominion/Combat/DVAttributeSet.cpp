#include "Combat/DVAttributeSet.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

UDVAttributeSet::UDVAttributeSet()
{
	InitHealth(100.f);
	InitMaxHealth(100.f);
	InitArmor(20.f);
	InitSuppression(0.f);
	InitMorale(0.6f);
	InitAccuracy(0.5f);
	InitIncomingDamage(0.f);
	InitIncomingSuppression(0.f);
}

void UDVAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UDVAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDVAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDVAttributeSet, Armor, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDVAttributeSet, Suppression, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDVAttributeSet, Morale, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDVAttributeSet, Accuracy, COND_None, REPNOTIFY_Always);
}

void UDVAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
	else if (Attribute == GetSuppressionAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, 100.f);
	}
	else if (Attribute == GetArmorAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, 80.f);
	}
	else if (Attribute == GetMoraleAttribute() || Attribute == GetAccuracyAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, 1.f);
	}
}

void UDVAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		const float Damage = GetIncomingDamage();
		SetIncomingDamage(0.f);
		if (Damage <= 0.f || bOutOfHealth)
		{
			return;
		}

		SetHealth(FMath::Clamp(GetHealth() - Damage, 0.f, GetMaxHealth()));
		DamageTakenFraction += Damage / FMath::Max(1.f, GetMaxHealth());

		if (GetHealth() <= 0.f)
		{
			bOutOfHealth = true;
			const FGameplayEffectContextHandle& Context = Data.EffectSpec.GetEffectContext();
			OnOutOfHealth.Broadcast(Context.GetOriginalInstigator(), Context.GetEffectCauser());
		}
	}
	else if (Data.EvaluatedData.Attribute == GetIncomingSuppressionAttribute())
	{
		const float Amount = GetIncomingSuppression();
		SetIncomingSuppression(0.f);
		// High morale soldiers shrug off a good part of incoming fire.
		const float Scaled = Amount * (1.f - 0.6f * GetMorale());
		SetSuppression(FMath::Clamp(GetSuppression() + Scaled, 0.f, 100.f));
	}
	else if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
	}
}

void UDVAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UDVAttributeSet, Health, OldValue); }
void UDVAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UDVAttributeSet, MaxHealth, OldValue); }
void UDVAttributeSet::OnRep_Armor(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UDVAttributeSet, Armor, OldValue); }
void UDVAttributeSet::OnRep_Suppression(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UDVAttributeSet, Suppression, OldValue); }
void UDVAttributeSet::OnRep_Morale(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UDVAttributeSet, Morale, OldValue); }
void UDVAttributeSet::OnRep_Accuracy(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UDVAttributeSet, Accuracy, OldValue); }
