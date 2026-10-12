#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "DVAttributeSet.generated.h"

#define DV_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

DECLARE_MULTICAST_DELEGATE_TwoParams(FDVOnOutOfHealth, AActor* /*Instigator*/, AActor* /*DamageCauser*/);

/**
 * Soldier attributes. Damage and suppression arrive through the meta attributes
 * IncomingDamage / IncomingSuppression (server only) and are folded into the
 * replicated values in PostGameplayEffectExecute.
 */
UCLASS()
class DELTAVDOMINION_API UDVAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UDVAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Vitals")
	FGameplayAttributeData Health;
	DV_ATTRIBUTE_ACCESSORS(UDVAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Vitals")
	FGameplayAttributeData MaxHealth;
	DV_ATTRIBUTE_ACCESSORS(UDVAttributeSet, MaxHealth)

	/** Flat damage reduction percentage 0..80 from body armour. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Armor, Category = "Vitals")
	FGameplayAttributeData Armor;
	DV_ATTRIBUTE_ACCESSORS(UDVAttributeSet, Armor)

	/** 0..100. Raised by near misses, decays over time. Widens aim, blurs HUD, pins AI. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Suppression, Category = "Combat")
	FGameplayAttributeData Suppression;
	DV_ATTRIBUTE_ACCESSORS(UDVAttributeSet, Suppression)

	/** 0..1, seeded from the roster record. Resists suppression. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Morale, Category = "Combat")
	FGameplayAttributeData Morale;
	DV_ATTRIBUTE_ACCESSORS(UDVAttributeSet, Morale)

	/** 0..1, seeded from the roster record. Drives AI aim error. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Accuracy, Category = "Combat")
	FGameplayAttributeData Accuracy;
	DV_ATTRIBUTE_ACCESSORS(UDVAttributeSet, Accuracy)

	// Meta attributes (not replicated)
	UPROPERTY(BlueprintReadOnly, Category = "Meta")
	FGameplayAttributeData IncomingDamage;
	DV_ATTRIBUTE_ACCESSORS(UDVAttributeSet, IncomingDamage)

	UPROPERTY(BlueprintReadOnly, Category = "Meta")
	FGameplayAttributeData IncomingSuppression;
	DV_ATTRIBUTE_ACCESSORS(UDVAttributeSet, IncomingSuppression)

	/** Server only. Fired once when Health reaches zero. */
	FDVOnOutOfHealth OnOutOfHealth;

	/** Server only. Damage taken this life as a fraction of MaxHealth; used to mark soldiers wounded. */
	float DamageTakenFraction = 0.f;

protected:
	UFUNCTION() void OnRep_Health(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Armor(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Suppression(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Morale(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Accuracy(const FGameplayAttributeData& OldValue);

public:
	/** Server: re-arm the out-of-health event (a downed soldier can still be finished off). */
	void ResetOutOfHealth() { bOutOfHealth = false; }

private:
	bool bOutOfHealth = false;
};
