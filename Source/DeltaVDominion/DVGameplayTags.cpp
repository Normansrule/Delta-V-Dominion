#include "DVGameplayTags.h"

namespace DVTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Weapon_Fire, "Ability.Weapon.Fire", "Fire the equipped weapon");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Weapon_Reload, "Ability.Weapon.Reload", "Reload the equipped weapon");

	UE_DEFINE_GAMEPLAY_TAG(State_Dead, "State.Dead");
	UE_DEFINE_GAMEPLAY_TAG(State_Reloading, "State.Reloading");
	UE_DEFINE_GAMEPLAY_TAG(State_Firing, "State.Firing");
	UE_DEFINE_GAMEPLAY_TAG(State_Sprinting, "State.Sprinting");
	UE_DEFINE_GAMEPLAY_TAG(State_Suppressed, "State.Suppressed");

	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_Damage, "SetByCaller.Damage");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_Suppression, "SetByCaller.Suppression");

	UE_DEFINE_GAMEPLAY_TAG(Damage_Ballistic, "Damage.Ballistic");
	UE_DEFINE_GAMEPLAY_TAG(Damage_Energy, "Damage.Energy");
	UE_DEFINE_GAMEPLAY_TAG(Damage_Headshot, "Damage.Headshot");
}
