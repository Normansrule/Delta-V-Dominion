#include "Core/DVRosterSubsystem.h"
#include "Core/DVSaveGame.h"
#include "DeltaVDominion.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	const TCHAR* const FirstNames[] = {
		TEXT("Amara"), TEXT("Tomas"), TEXT("Yusuf"), TEXT("Ines"), TEXT("Kenji"), TEXT("Freya"), TEXT("Mateo"),
		TEXT("Nadia"), TEXT("Oskar"), TEXT("Leila"), TEXT("Rafael"), TEXT("Sana"), TEXT("Dmitri"), TEXT("Chiara"),
		TEXT("Kwame"), TEXT("Elena"), TEXT("Arjun"), TEXT("Maeve"), TEXT("Jonah"), TEXT("Ximena"), TEXT("Bao"),
		TEXT("Ingrid"), TEXT("Tariq"), TEXT("Wren")
	};
	const TCHAR* const LastNames[] = {
		TEXT("Adeyemi"), TEXT("Brandt"), TEXT("Castellanos"), TEXT("Dubois"), TEXT("Eriksen"), TEXT("Fujita"),
		TEXT("Grieve"), TEXT("Haddad"), TEXT("Ivanova"), TEXT("Jaramillo"), TEXT("Kowalski"), TEXT("Lindqvist"),
		TEXT("Mbeki"), TEXT("Novak"), TEXT("Okafor"), TEXT("Petrov"), TEXT("Quist"), TEXT("Rourke"), TEXT("Sato"),
		TEXT("Tremblay"), TEXT("Varga"), TEXT("Whitlock"), TEXT("Yilmaz"), TEXT("Zhou")
	};
	const TCHAR* const Callsigns[] = {
		TEXT("Torque"), TEXT("Lamplight"), TEXT("Shale"), TEXT("Rook"), TEXT("Kettle"), TEXT("Vesper"),
		TEXT("Static"), TEXT("Crow"), TEXT("Ballast"), TEXT("Tinder")
	};

	struct FTraitDef { const TCHAR* Name; float Accuracy; float Morale; };
	const FTraitDef Traits[] = {
		{ TEXT("Steady"),      0.00f,  0.10f },
		{ TEXT("EagleEye"),    0.12f,  0.00f },
		{ TEXT("Nervous"),    -0.05f, -0.12f },
		{ TEXT("IronLungs"),   0.03f,  0.05f },
		{ TEXT("MedicTrained"),0.00f,  0.05f },
		{ TEXT("Reckless"),   -0.08f,  0.10f },
	};

	constexpr int32 RankXP[] = { 0, 150, 300, 600, 1100, 1900 };	// indexed by EDVRank
}

FString FDVSoldierRecord::GetDisplayName() const
{
	return Callsign.IsEmpty()
		? FString::Printf(TEXT("%s %s"), *FirstName, *LastName)
		: FString::Printf(TEXT("%s \"%s\" %s"), *FirstName, *Callsign, *LastName);
}

FString UDVRosterSubsystem::RankToString(EDVRank Rank)
{
	switch (Rank)
	{
	case EDVRank::Conscript:         return TEXT("Conscript");
	case EDVRank::Private:           return TEXT("Private");
	case EDVRank::PrivateFirstClass: return TEXT("Private First Class");
	case EDVRank::Corporal:          return TEXT("Corporal");
	case EDVRank::Sergeant:          return TEXT("Sergeant");
	case EDVRank::StaffSergeant:     return TEXT("Staff Sergeant");
	default:                         return TEXT("Unknown");
	}
}

void UDVRosterSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (!LoadRoster())
	{
		NewCampaign(/*bIronman*/ false);
	}
}

void UDVRosterSubsystem::NewCampaign(bool bIronman, int32 Seed)
{
	State = FDVRosterState();
	State.bIronman = bIronman;
	State.RecruitSeed = Seed != 0 ? Seed : FMath::Rand();
	Rng.Initialize(State.RecruitSeed);

	for (int32 i = 0; i < StartingRosterSize; ++i)
	{
		FDVSoldierRecord Recruit = MakeRecruit(false);
		// The starting platoon has some seasoning.
		ApplyExperience(Recruit, Rng.RandRange(0, 600));
		State.Soldiers.Add(MoveTemp(Recruit));
	}

	UE_LOG(LogDVRoster, Log, TEXT("New campaign (seed %d, ironman %d) with %d soldiers"), State.RecruitSeed, bIronman, State.Soldiers.Num());
	SaveRoster();
	Broadcast();
}

bool UDVRosterSubsystem::SaveRoster()
{
	UDVSaveGame* Save = Cast<UDVSaveGame>(UGameplayStatics::CreateSaveGameObject(UDVSaveGame::StaticClass()));
	if (!Save)
	{
		return false;
	}
	Save->WriteRoster(State);
	const bool bOk = UGameplayStatics::SaveGameToSlot(Save, SaveSlot, 0);
	UE_CLOG(!bOk, LogDVRoster, Error, TEXT("Failed to write save slot %s"), *SaveSlot);
	return bOk;
}

bool UDVRosterSubsystem::LoadRoster()
{
	if (!UGameplayStatics::DoesSaveGameExist(SaveSlot, 0))
	{
		return false;
	}

	const UDVSaveGame* Save = Cast<UDVSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlot, 0));
	FDVRosterState Loaded;
	if (!Save || !Save->ReadRoster(Loaded))
	{
		UE_LOG(LogDVRoster, Error, TEXT("Save slot %s failed validation"), *SaveSlot);
		return false;
	}

	State = MoveTemp(Loaded);
	// Advance the stream so new recruits differ from earlier ones after reload.
	Rng.Initialize(State.RecruitSeed + State.MissionsCompleted * 7919 + State.Soldiers.Num());
	UE_LOG(LogDVRoster, Log, TEXT("Loaded roster: %d soldiers, %d missions"), State.Soldiers.Num(), State.MissionsCompleted);
	Broadcast();
	return true;
}

bool UDVRosterSubsystem::GetSoldier(const FGuid& Id, FDVSoldierRecord& OutRecord) const
{
	const FDVSoldierRecord* Found = State.Soldiers.FindByPredicate([&Id](const FDVSoldierRecord& S) { return S.Id == Id; });
	if (Found)
	{
		OutRecord = *Found;
	}
	return Found != nullptr;
}

TArray<FDVSoldierRecord> UDVRosterSubsystem::GetAvailableSoldiers() const
{
	return State.Soldiers.FilterByPredicate([](const FDVSoldierRecord& S) { return S.IsAvailable(); });
}

TArray<FDVSoldierRecord> UDVRosterSubsystem::GetMemorial() const
{
	TArray<FDVSoldierRecord> Fallen = State.Soldiers.FilterByPredicate([](const FDVSoldierRecord& S) { return S.Status == EDVSoldierStatus::KIA; });
	Fallen.StableSort([](const FDVSoldierRecord& A, const FDVSoldierRecord& B) { return A.DiedOnMission < B.DiedOnMission; });
	return Fallen;
}

TArray<FGuid> UDVRosterSubsystem::SelectSquad(int32 MaxSize) const
{
	TArray<FDVSoldierRecord> Available = GetAvailableSoldiers();
	Available.Sort([](const FDVSoldierRecord& A, const FDVSoldierRecord& B) { return A.Experience > B.Experience; });

	TArray<FGuid> Result;
	for (int32 i = 0; i < Available.Num() && Result.Num() < MaxSize; ++i)
	{
		Result.Add(Available[i].Id);
	}
	return Result;
}

void UDVRosterSubsystem::ReportKIA(const FGuid& Id, const FString& Cause, const FString& Where)
{
	FDVSoldierRecord* Soldier = FindSoldier(Id);
	if (!Soldier || Soldier->Status == EDVSoldierStatus::KIA)
	{
		return;
	}

	Soldier->Status = EDVSoldierStatus::KIA;
	Soldier->DiedOnMission = State.MissionsCompleted;
	Soldier->CauseOfDeath = Cause;
	Soldier->DiedAt = Where;
	UE_LOG(LogDVRoster, Log, TEXT("KIA: %s %s (%s)"), *RankToString(Soldier->Rank), *Soldier->GetDisplayName(), *Cause);

	// Persist immediately so a death can't be undone by quitting before the mission ends.
	SaveRoster();
	OnSoldierKilled.Broadcast(*Soldier);
	Broadcast();
}

void UDVRosterSubsystem::ReportKill(const FGuid& Id)
{
	if (FDVSoldierRecord* Soldier = FindSoldier(Id))
	{
		++Soldier->Kills;
		ApplyExperience(*Soldier, XPPerKill);
	}
}

void UDVRosterSubsystem::CompleteMission(const FDVMissionReport& Report)
{
	// Medbay recovery for soldiers who sat this one out.
	for (FDVSoldierRecord& Soldier : State.Soldiers)
	{
		if (Soldier.Status == EDVSoldierStatus::Wounded && --Soldier.WoundedMissionsRemaining <= 0)
		{
			Soldier.Status = EDVSoldierStatus::Active;
			Soldier.WoundedMissionsRemaining = 0;
		}
	}

	for (const FGuid& Id : Report.Survivors)
	{
		FDVSoldierRecord* Soldier = FindSoldier(Id);
		if (!Soldier || Soldier->Status == EDVSoldierStatus::KIA)
		{
			continue;
		}
		++Soldier->MissionsFlown;
		ApplyExperience(*Soldier, Report.bSuccess ? XPPerMission : XPPerMission / 2);

		// Surviving combat steadies people; a failed op shakes them.
		Soldier->Morale = FMath::Clamp(Soldier->Morale + (Report.bSuccess ? 0.03f : -0.05f), 0.f, 1.f);

		if (Report.Wounded.Contains(Id))
		{
			Soldier->Status = EDVSoldierStatus::Wounded;
			Soldier->WoundedMissionsRemaining = WoundedRecoveryMissions;
		}
	}

	++State.MissionsCompleted;
	AddResources(Report.Reward);
	Reinforce(DireStraitsMinimum);

	SaveRoster();
	Broadcast();
}

int32 UDVRosterSubsystem::Reinforce(int32 TargetAvailable)
{
	int32 Added = 0;
	State.bDireStraits = false;

	while (CountAvailable() < TargetAvailable)
	{
		const bool bCanAfford = State.Resources.Manpower >= ManpowerPerRecruit;
		if (bCanAfford)
		{
			State.Resources.Manpower -= ManpowerPerRecruit;
		}
		else
		{
			// Dire straits: emergency conscripts keep the campaign winnable, but they are poor soldiers.
			State.bDireStraits = true;
		}
		State.Soldiers.Add(MakeRecruit(!bCanAfford));
		++Added;
	}

	UE_CLOG(Added > 0, LogDVRoster, Log, TEXT("Reinforced with %d soldiers (dire straits: %d, manpower left: %d)"),
		Added, State.bDireStraits, State.Resources.Manpower);
	if (Added > 0)
	{
		Broadcast();
	}
	return Added;
}

void UDVRosterSubsystem::AddResources(const FDVWarResources& Delta)
{
	State.Resources.Manpower = FMath::Max(0, State.Resources.Manpower + Delta.Manpower);
	State.Resources.Materiel = FMath::Max(0, State.Resources.Materiel + Delta.Materiel);
	State.Resources.Fuel = FMath::Max(0, State.Resources.Fuel + Delta.Fuel);
	Broadcast();
}

FDVSoldierRecord* UDVRosterSubsystem::FindSoldier(const FGuid& Id)
{
	return State.Soldiers.FindByPredicate([&Id](const FDVSoldierRecord& S) { return S.Id == Id; });
}

FDVSoldierRecord UDVRosterSubsystem::MakeRecruit(bool bConscript)
{
	FDVSoldierRecord R;
	R.Id = FGuid::NewGuid();
	R.FirstName = FirstNames[Rng.RandHelper(static_cast<int32>(UE_ARRAY_COUNT(FirstNames)))];
	R.LastName = LastNames[Rng.RandHelper(static_cast<int32>(UE_ARRAY_COUNT(LastNames)))];
	if (Rng.FRand() < 0.25f)
	{
		R.Callsign = Callsigns[Rng.RandHelper(static_cast<int32>(UE_ARRAY_COUNT(Callsigns)))];
	}
	R.PortraitIndex = Rng.RandHelper(64);

	if (bConscript)
	{
		R.Rank = EDVRank::Conscript;
		R.Accuracy = Rng.FRandRange(0.20f, 0.35f);
		R.Morale = Rng.FRandRange(0.20f, 0.35f);
		R.Traits.Add(TEXT("Conscript"));
	}
	else
	{
		R.Rank = EDVRank::Private;
		R.Accuracy = Rng.FRandRange(0.35f, 0.55f);
		R.Morale = Rng.FRandRange(0.45f, 0.65f);
	}

	const FTraitDef& Trait = Traits[Rng.RandHelper(static_cast<int32>(UE_ARRAY_COUNT(Traits)))];
	R.Traits.Add(Trait.Name);
	R.Accuracy = FMath::Clamp(R.Accuracy + Trait.Accuracy, 0.05f, 1.f);
	R.Morale = FMath::Clamp(R.Morale + Trait.Morale, 0.05f, 1.f);
	return R;
}

void UDVRosterSubsystem::ApplyExperience(FDVSoldierRecord& Soldier, int32 XP)
{
	Soldier.Experience += XP;
	// Experience slowly sharpens aim, with diminishing returns.
	Soldier.Accuracy = FMath::Clamp(Soldier.Accuracy + XP * 0.0002f * (1.f - Soldier.Accuracy), 0.f, 0.95f);

	const int32 RankCount = static_cast<int32>(EDVRank::MAX);
	int32 RankIndex = static_cast<int32>(Soldier.Rank);
	while (RankIndex + 1 < RankCount && Soldier.Experience >= RankXP[RankIndex + 1])
	{
		++RankIndex;
	}
	// Conscripts who survive long enough become regular soldiers.
	Soldier.Rank = static_cast<EDVRank>(FMath::Max(RankIndex, static_cast<int32>(Soldier.Rank)));
}

int32 UDVRosterSubsystem::CountAvailable() const
{
	int32 Count = 0;
	for (const FDVSoldierRecord& S : State.Soldiers)
	{
		Count += S.IsAvailable() ? 1 : 0;
	}
	return Count;
}

void UDVRosterSubsystem::Broadcast()
{
	OnRosterChanged.Broadcast();
}
