#pragma once

#include "NativeGameplayTags.h"

// Native gameplay tags shared by abilities, effects and AI.
namespace DVTags
{
	// Ability identity / activation
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Weapon_Fire);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Weapon_Reload);

	// Owned state
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dead);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Reloading);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Firing);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Sprinting);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Suppressed);

	// SetByCaller magnitudes
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Damage);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Suppression);

	// Damage types (used in effect context / resistances later)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Ballistic);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Energy);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Headshot);
}
