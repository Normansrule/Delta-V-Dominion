#include "Core/DVMissions.h"
#include "Core/DVProfileSubsystem.h"

bool DVMissions::bUnlockAll = false;

namespace DVMissionsData	// named: unity builds merge anonymous namespaces
{
	struct FMapEntry
	{
		const TCHAR* Key;
		const TCHAR* OpName;
		bool bSetPiece;	// the map's story is its own set piece (walker, manifold, herding, hacks, harbour): generated work is recon and rescue
	};

	// Campaign order, following docs/CAMPAIGN.md: Act I Gaia and the inner system, Act II the infrastructure war,
	// Act III the colonies and Morrow's trail, Act IV the Static and the cure worlds.
	const FMapEntry Maps[] = {
		{ TEXT("Breakwater"), TEXT("Breakwater"), true },
		{ TEXT("Outpost"), TEXT("Red Tide"), false },
		{ TEXT("Ironstride"), TEXT("Ironstride"), true },
		{ TEXT("Europa"), TEXT("Glass Ocean"), false },
		{ TEXT("Io"), TEXT("Forgefall"), false },
		{ TEXT("Ceres"), TEXT("Ghost Signal"), false },
		{ TEXT("Barnard"), TEXT("Iron Hive"), false },
		{ TEXT("Eridani"), TEXT("Dust Choir"), false },
		{ TEXT("Proxima"), TEXT("Cold Wake"), false },
		{ TEXT("Glacier"), TEXT("White Silence"), false },
		{ TEXT("Trappist"), TEXT("The Architect"), false },
		{ TEXT("Kepler"), TEXT("Ark Fall"), false },
		{ TEXT("Crystal"), TEXT("Prism Fall"), false },
		{ TEXT("Jungle"), TEXT("Emerald Wake"), false },
		{ TEXT("Ocean"), TEXT("Tidefall"), false },
		{ TEXT("Brasshaven"), TEXT("Steamheart"), true },
		{ TEXT("NeonVerge"), TEXT("Clean Signal"), true },
		{ TEXT("Hollyhock"), TEXT("Lantern Night"), true },
		{ TEXT("Gaia"), TEXT("Gaia Falls"), false },
	};

	const TCHAR* const Names[][4] = {
		{ TEXT(""), TEXT(""), TEXT(""), TEXT("") },
		{ TEXT("Hammerfall"), TEXT("Clean Sweep"), TEXT("Breach"), TEXT("Cold Steel") },
		{ TEXT("Long Fuse"), TEXT("Demolition"), TEXT("Cut the Wire"), TEXT("Lights Out") },
		{ TEXT("Deep Recon"), TEXT("Silent Eyes"), TEXT("Data Run"), TEXT("Ghost Walk") },
		{ TEXT("Lifeline"), TEXT("No One Left"), TEXT("Search and Recover"), TEXT("Homeward") },
		{ TEXT("Tide Breaker"), TEXT("Last Stand"), TEXT("Iron Rain"), TEXT("Meat Grinder") },
	};

	const FMapEntry* Find(const FString& Key)
	{
		for (const FMapEntry& E : Maps)
		{
			if (Key.Equals(E.Key))
			{
				return &E;
			}
		}
		return nullptr;
	}

	FString TwistText(int32 Mods)
	{
		TArray<FString> Parts;
		if (Mods & DVMissionMods::Heavy) { Parts.Add(TEXT("reinforced garrison")); }
		if (Mods & DVMissionMods::Blackout) { Parts.Add(TEXT("no fire support")); }
		if (Mods & DVMissionMods::Front) { Parts.Add(TEXT("weather fronts")); }
		if (Mods & DVMissionMods::Lightning) { Parts.Add(TEXT("eight-minute clock")); }
		FString Out = FString::Join(Parts, TEXT(", "));
		if (!Out.IsEmpty())
		{
			Out[0] = FChar::ToUpper(Out[0]);
		}
		return Out;
	}
}

const TArray<FString>& DVMissions::CampaignMaps()
{
	static TArray<FString> Keys;
	if (Keys.Num() == 0)
	{
		for (const DVMissionsData::FMapEntry& E : DVMissionsData::Maps)
		{
			Keys.Add(E.Key);
		}
	}
	return Keys;
}

FString DVMissions::KindName(EDVMissionKind Kind)
{
	switch (Kind)
	{
	case EDVMissionKind::Story: return TEXT("Story operation");
	case EDVMissionKind::Assault: return TEXT("Assault");
	case EDVMissionKind::Sabotage: return TEXT("Sabotage");
	case EDVMissionKind::Recon: return TEXT("Recon");
	case EDVMissionKind::Rescue: return TEXT("Rescue");
	case EDVMissionKind::Purge: return TEXT("Purge");
	default: return TEXT("Operation");
	}
}

FDVMission DVMissions::Make(const FString& MapKey, int32 Index)
{
	FDVMission M;
	M.MapKey = MapKey;
	M.Index = FMath::Clamp(Index, 0, MissionsPerMap - 1);
	const DVMissionsData::FMapEntry* Entry = DVMissionsData::Find(MapKey);
	const FString Op = Entry ? Entry->OpName : MapKey;
	if (M.Index == 0)
	{
		M.Title = Op;
		return M;	// the story operation keeps its authored objective
	}
	FRandomStream Rng(int32(GetTypeHash(MapKey)) * 31 + M.Index * 7919);
	if (Entry && Entry->bSetPiece)
	{
		M.Kind = M.Index % 2 ? EDVMissionKind::Recon : EDVMissionKind::Rescue;
	}
	else
	{
		// Every kind appears across a map's run; the order is shuffled per map.
		static const EDVMissionKind Cycle[] = { EDVMissionKind::Assault, EDVMissionKind::Recon, EDVMissionKind::Sabotage, EDVMissionKind::Rescue, EDVMissionKind::Purge };
		const int32 Offset = Rng.RandRange(0, 4);
		M.Kind = Cycle[(M.Index - 1 + Offset) % 5];
	}
	// Twists grow with the index: early missions are clean, late ones stack two.
	const int32 Wanted = M.Index <= 2 ? Rng.RandRange(0, 1) : M.Index <= 6 ? 1 : 2;
	TArray<int32> Pool = { DVMissionMods::Heavy, DVMissionMods::Blackout, DVMissionMods::Front, DVMissionMods::Lightning };
	for (int32 i = 0; i < Wanted && Pool.Num() > 0; ++i)
	{
		const int32 Pick = Rng.RandRange(0, Pool.Num() - 1);
		M.Mods |= Pool[Pick];
		Pool.RemoveAt(Pick);
	}
	const int32 KindIndex = int32(M.Kind);
	M.Title = FString::Printf(TEXT("%s %d: %s"), *Op, M.Index + 1, DVMissionsData::Names[KindIndex][Rng.RandRange(0, 3)]);
	switch (M.Kind)
	{
	case EDVMissionKind::Assault: M.Objective = TEXT("Clear the garrison"); break;
	case EDVMissionKind::Sabotage: M.Objective = TEXT("Clear the garrison, plant the charges, hold, extract"); break;
	case EDVMissionKind::Recon: M.Objective = TEXT("Recover all five intel caches. The garrison is optional"); break;
	case EDVMissionKind::Rescue: M.Objective = TEXT("Find the three stranded survivors and get them out"); break;
	case EDVMissionKind::Purge: M.Objective = TEXT("Clear the garrison. Reinforcements keep landing"); break;
	default: break;
	}
	M.Twists = DVMissionsData::TwistText(M.Mods);
	return M;
}

FString DVMissions::Id(const FString& MapKey, int32 Index)
{
	return FString::Printf(TEXT("Mission:%s:%d"), *MapKey, Index);
}

bool DVMissions::IsDone(const UDVProfileSubsystem* Profile, const FString& MapKey, int32 Index)
{
	if (!Profile)
	{
		return false;
	}
	// Story operations won before missions were recorded still count: a won operation pays its challenges.
	return Profile->HasChallenge(Id(MapKey, Index)) || (Index == 0 && Profile->CountChallenges(MapKey) > 0);
}

bool DVMissions::IsUnlocked(const UDVProfileSubsystem* Profile, const FString& MapKey, int32 Index)
{
	if (bUnlockAll)
	{
		return true;
	}
	const TArray<FString>& Order = CampaignMaps();
	const int32 MapIndex = Order.IndexOfByKey(MapKey);
	if (MapIndex == INDEX_NONE)
	{
		return true;	// hubs, training and anything off the campaign list are always open
	}
	if (Index > 0)
	{
		return IsDone(Profile, MapKey, Index - 1);
	}
	return MapIndex == 0 || IsDone(Profile, Order[MapIndex - 1], 0);
}

int32 DVMissions::NextFor(const UDVProfileSubsystem* Profile, const FString& MapKey)
{
	if (!CampaignMaps().Contains(MapKey))
	{
		return 0;
	}
	if (!IsUnlocked(Profile, MapKey, 0))
	{
		return -1;
	}
	for (int32 i = 0; i < MissionsPerMap; ++i)
	{
		if (!IsDone(Profile, MapKey, i))
		{
			return IsUnlocked(Profile, MapKey, i) ? i : FMath::Max(0, i - 1);
		}
	}
	return MissionsPerMap - 1;
}

int32 DVMissions::CountDone(const UDVProfileSubsystem* Profile, const FString& MapKey)
{
	int32 N = 0;
	for (int32 i = 0; i < MissionsPerMap; ++i)
	{
		N += IsDone(Profile, MapKey, i) ? 1 : 0;
	}
	return N;
}

int32 DVMissions::Total()
{
	return CampaignMaps().Num() * MissionsPerMap;
}
