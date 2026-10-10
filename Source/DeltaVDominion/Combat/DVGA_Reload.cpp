#include "Combat/DVGA_Reload.h"
#include "Combat/DVWeaponBase.h"
#include "Combat/DVWeaponData.h"
#include "Characters/DVSoldierCharacter.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "DVGameplayTags.h"

UDVGA_Reload::UDVGA_Reload()
{
	SetAssetTags(FGameplayTagContainer(DVTags::Ability_Weapon_Reload));
	ActivationOwnedTags.AddTag(DVTags::State_Reloading);
}

bool UDVGA_Reload::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	const ADVWeaponBase* Weapon = GetWeaponFromActorInfo(ActorInfo);
	return Weapon && Weapon->CanReload();
}

void UDVGA_Reload::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ADVWeaponBase* Weapon = GetWeapon();
	const UDVWeaponData* Data = Weapon ? Weapon->GetData() : nullptr;
	if (!Data)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (ADVSoldierCharacter* Soldier = GetSoldier())
	{
		Soldier->PlayWeaponMontage(Data->ReloadMontage);
	}

	UAbilityTask_WaitDelay* Wait = UAbilityTask_WaitDelay::WaitDelay(this, Data->ReloadTime);
	Wait->OnFinish.AddDynamic(this, &UDVGA_Reload::OnReloadFinished);
	Wait->ReadyForActivation();
}

void UDVGA_Reload::OnReloadFinished()
{
	if (ADVWeaponBase* Weapon = GetWeapon())
	{
		if (Weapon->HasAuthority())
		{
			Weapon->ServerFinishReload();
		}
	}
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}
