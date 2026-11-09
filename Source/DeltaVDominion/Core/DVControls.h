#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

/**
 * The controls, in one place: the standard PC shooter layout by default (WASD, Space, Shift, Ctrl, E to use, R to
 * reload, F for takedowns, Tab for the map, Esc for the menu), every one rebindable from the controls screen and saved
 * to GameUserSettings.ini. Gameplay code asks DVControls::Key(...) instead of naming keys, and prompts draw
 * DVControls::Label(...) so the HUD always shows the key you actually bound.
 */
enum class EDVControl : uint8
{
	MoveForward, MoveBack, MoveLeft, MoveRight,
	Jump, Sprint, Crouch, Dash,
	Fire, Aim, Reload, Interact, Melee,
	LeanLeft, LeanRight,
	OrderMove, OrderHold, OrderRegroup, OrderSuppress, OrderFlank,
	Orbital, Supply, Strafe, FlyHome, Map, Buy,
	VehicleUp, VehicleDown, VehicleMode, VehicleCamera, Flares, Boost,
	Num
};

namespace DVControls
{
	DELTAVDOMINION_API FKey Key(EDVControl Control);
	DELTAVDOMINION_API FKey DefaultKey(EDVControl Control);
	DELTAVDOMINION_API void SetKey(EDVControl Control, const FKey& NewKey);
	DELTAVDOMINION_API void ResetAll();
	/** Short keycap text for prompts ("E", "Space", "MMB"). */
	DELTAVDOMINION_API FString Label(EDVControl Control);
	DELTAVDOMINION_API FString KeyLabel(const FKey& Key);
	/** "[E]" for building prompt strings the HUD draws as keycaps. */
	DELTAVDOMINION_API FString Cap(EDVControl Control);
	DELTAVDOMINION_API FString Name(EDVControl Control);
	/** Group heading the controls screen files it under. */
	DELTAVDOMINION_API FString Group(EDVControl Control);
	/** Bumped on every change, so input setups know to rebuild. */
	DELTAVDOMINION_API int32 Revision();
}
