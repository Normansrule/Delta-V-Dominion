#pragma once

#include "CoreMinimal.h"

class UDVWeaponData;

/**
 * The Kestrel armoury's weapon racks: built-in weapon profiles, so loadouts work before any
 * DVWeaponData assets exist. Index 0 is the M9 Bastion (the class defaults).
 */
namespace DVLoadouts
{
	int32 Num();
	UDVWeaponData* Get(int32 Index);
	/** One line of stats for the armoury readout. */
	FString Describe(const UDVWeaponData* Data);
}
