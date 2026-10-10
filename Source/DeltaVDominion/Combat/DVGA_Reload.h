#pragma once

#include "CoreMinimal.h"
#include "Combat/DVGameplayAbility.h"
#include "DVGA_Reload.generated.h"

/** Reloads the equipped weapon after its ReloadTime. Blocks firing while active. */
UCLASS()
class DELTAVDOMINION_API UDVGA_Reload : public UDVGameplayAbility
{
	GENERATED_BODY()

public:
	UDVGA_Reload();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

private:
	UFUNCTION() void OnReloadFinished();
};
