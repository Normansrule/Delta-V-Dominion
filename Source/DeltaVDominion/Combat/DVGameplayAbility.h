#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "DVGameplayAbility.generated.h"

class ADVSoldierCharacter;
class ADVWeaponBase;

/** Base for all DELTA-V abilities: instanced per actor, locally predicted, blocked while dead. */
UCLASS(Abstract)
class DELTAVDOMINION_API UDVGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UDVGameplayAbility();

protected:
	ADVSoldierCharacter* GetSoldier() const;
	ADVWeaponBase* GetWeapon() const;
	static ADVWeaponBase* GetWeaponFromActorInfo(const FGameplayAbilityActorInfo* ActorInfo);
	double GetWorldTime() const;
};
