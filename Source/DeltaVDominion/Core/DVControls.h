#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

/**
 * The controls, in one place: the standard PC shooter layout by default (WASD, Space, Shift, Ctrl, E to use, R to
 * reload, F for takedowns, Tab for the map, Esc for the menu), every one rebindable from the controls screen and saved
 * to GameUserSettings.ini. Gameplay code asks DVControls::Key(...) instead of naming keys, and prompts draw
 * DVControls::Label(...) so the HUD always shows the key you actually bound.
 */
enum class EDVInput : uint8
{
	MoveForward, MoveBack, MoveLeft, MoveRight,
	Jump, Sprint, Crouch, Dash,
	Fire, Aim, Reload, Interact, Melee, Grenade,
	LeanLeft, LeanRight,
	OrderMove, OrderHold, OrderRegroup, OrderSuppress, OrderFlank,
	Orbital, Supply, Strafe, FlyHome, Map, Buy,
	VehicleUp, VehicleDown, VehicleMode, VehicleCamera, Flares, Boost,
	Num
};

namespace DVControls
{
	DELTAVDOMINION_API FKey Key(EDVInput Control);
	DELTAVDOMINION_API FKey DefaultKey(EDVInput Control);
	DELTAVDOMINION_API void SetKey(EDVInput Control, const FKey& NewKey);
	DELTAVDOMINION_API void ResetAll();
	/** Short keycap text for prompts ("E", "Space", "MMB"). */
	DELTAVDOMINION_API FString Label(EDVInput Control);
	DELTAVDOMINION_API FString KeyLabel(const FKey& Key);
	/** "[E]" for building prompt strings the HUD draws as keycaps. */
	DELTAVDOMINION_API FString Cap(EDVInput Control);
	DELTAVDOMINION_API FString Name(EDVInput Control);
	/** Group heading the controls screen files it under. */
	DELTAVDOMINION_API FString Group(EDVInput Control);
	/** Bumped on every change, so input setups know to rebuild. */
	DELTAVDOMINION_API int32 Revision();
}
