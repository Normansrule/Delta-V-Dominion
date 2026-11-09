#pragma once

#include "CoreMinimal.h"

class UDVProfileSubsystem;

/** What a mission asks of you. Story is the hand-written operation for the map; the rest are generated. */
enum class EDVMissionKind : uint8
{
	Story,		// the authored operation (clear, sabotage, hold, extract, or the map's own set piece)
	Assault,	// clear the garrison, nothing else
	Sabotage,	// the full clear > plant > hold > extract run
	Recon,		// recover every intel cache; the garrison is optional
	Rescue,		// bring every stranded survivor home
	Purge,		// clear the garrison while reinforcements keep landing
	Num
};

/** Twists layered on a generated mission (bit flags). */
namespace DVMissionMods
{
	constexpr int32 Heavy = 1 << 0;		// a reinforced garrison: four more war-frames dug in
	constexpr int32 Blackout = 1 << 1;	// no fire support from the Kestrel
	constexpr int32 Front = 1 << 2;		// a weather front every minute
	constexpr int32 Lightning = 1 << 3;	// eight minutes on the clock or the op is scrubbed
}

struct FDVMission
{
	FString MapKey;
	int32 Index = 0;
	EDVMissionKind Kind = EDVMissionKind::Story;
	int32 Mods = 0;
	FString Title;		// "Iron Hive 4: Deep Recon"
	FString Objective;	// the HUD line
	FString Twists;		// "Weather front, no fire support"
};

/**
 * The campaign: an ordered list of operation maps, each with its story operation and a run of generated missions
 * built from templates x twists, deterministic per map and index so co-op and replays agree. Unlocks run in order:
 * a map's story operation opens when the previous map's story is done, and its generated missions open one at a
 * time after that. Content scales by adding maps to the list; every map brings MissionsPerMap missions with it.
 */
namespace DVMissions
{
	constexpr int32 MissionsPerMap = 10;

	/** Campaign order of the operation maps (map asset names). */
	DELTAVDOMINION_API const TArray<FString>& CampaignMaps();

	/** Build mission Index (0 = story) for MapKey. */
	DELTAVDOMINION_API FDVMission Make(const FString& MapKey, int32 Index);

	/** Profile id recorded when the mission is won. */
	DELTAVDOMINION_API FString Id(const FString& MapKey, int32 Index);

	DELTAVDOMINION_API bool IsDone(const UDVProfileSubsystem* Profile, const FString& MapKey, int32 Index);
	DELTAVDOMINION_API bool IsUnlocked(const UDVProfileSubsystem* Profile, const FString& MapKey, int32 Index);
	/** The next mission to play on MapKey: the first one not yet won (or the last one if all are won). -1 if the map is locked. */
	DELTAVDOMINION_API int32 NextFor(const UDVProfileSubsystem* Profile, const FString& MapKey);
	DELTAVDOMINION_API int32 CountDone(const UDVProfileSubsystem* Profile, const FString& MapKey);
	/** Total missions in the campaign. */
	DELTAVDOMINION_API int32 Total();

	DELTAVDOMINION_API FString KindName(EDVMissionKind Kind);

	/** Tests and the -DVUnlockAll switch: open everything. */
	DELTAVDOMINION_API extern bool bUnlockAll;
}
