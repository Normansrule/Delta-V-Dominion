#include "Combat/DVGameplayAbility.h"
#include "Characters/DVSoldierCharacter.h"
#include "DVGameplayTags.h"

UDVGameplayAbility::UDVGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationBlockedTags.AddTag(DVTags::State_Dead);
}

ADVSoldierCharacter* UDVGameplayAbility::GetSoldier() const
{
	return Cast<ADVSoldierCharacter>(GetAvatarActorFromActorInfo());
}

ADVWeaponBase* UDVGameplayAbility::GetWeapon() const
{
	const ADVSoldierCharacter* Soldier = GetSoldier();
	return Soldier ? Soldier->GetCurrentWeapon() : nullptr;
}

ADVWeaponBase* UDVGameplayAbility::GetWeaponFromActorInfo(const FGameplayAbilityActorInfo* ActorInfo)
{
	const ADVSoldierCharacter* Soldier = ActorInfo ? Cast<ADVSoldierCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	return Soldier ? Soldier->GetCurrentWeapon() : nullptr;
}

double UDVGameplayAbility::GetWorldTime() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.0;
}
