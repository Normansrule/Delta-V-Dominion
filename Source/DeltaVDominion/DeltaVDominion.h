#pragma once

#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(LogDV, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogDVRoster, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogDVAI, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogDVNet, Log, All);

// Trace channel configured in DefaultEngine.ini as "Weapon".
#define DV_TRACE_WEAPON ECC_GameTraceChannel1

// Team ids used by IGenericTeamAgentInterface.
namespace DVTeam
{
	constexpr uint8 Directorate = 1;
	constexpr uint8 Hearthbound = 2;
}
