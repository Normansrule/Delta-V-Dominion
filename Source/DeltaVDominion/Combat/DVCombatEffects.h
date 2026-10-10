#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GameplayEffectExecutionCalculation.h"
#include "DVCombatEffects.generated.h"

/**
 * Damage = SetByCaller.Damage, x2.5 on headshots, reduced by target Armor (percent).
 * Writes to the IncomingDamage meta attribute.
 */
UCLASS()
class DELTAVDOMINION_API UDVDamageExecution : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	UDVDamageExecution();
	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
		FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
};

/** Instant damage effect, configured in C++ so it works without editor assets. */
UCLASS()
class DELTAVDOMINION_API UDVDamageEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UDVDamageEffect();
};

/** Instant suppression from near misses (SetByCaller.Suppression -> IncomingSuppression). */
UCLASS()
class DELTAVDOMINION_API UDVSuppressionEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UDVSuppressionEffect();
};
