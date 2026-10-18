#pragma once

#include "CoreMinimal.h"
#include "DVRosterTypes.generated.h"

UENUM(BlueprintType)
enum class EDVRank : uint8
{
	Conscript,
	Private,
	PrivateFirstClass,
	Corporal,
	Sergeant,
	StaffSergeant,
	MAX UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EDVSoldierStatus : uint8
{
	Active,
	Wounded,	// Recovering in the medbay, unavailable for a number of missions
	KIA,		// Permanent
	Discharged
};

/** One persistent soldier. Death is permanent: KIA records stay forever for the Memorial Wall. */
USTRUCT(BlueprintType)
struct FDVSoldierRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, SaveGame) FGuid Id;
	UPROPERTY(BlueprintReadOnly, SaveGame) FString FirstName;
	UPROPERTY(BlueprintReadOnly, SaveGame) FString LastName;
	UPROPERTY(BlueprintReadOnly, SaveGame) FString Callsign;
	UPROPERTY(BlueprintReadOnly, SaveGame) EDVRank Rank = EDVRank::Private;
	UPROPERTY(BlueprintReadOnly, SaveGame) EDVSoldierStatus Status = EDVSoldierStatus::Active;
	UPROPERTY(BlueprintReadOnly, SaveGame) TArray<FName> Traits;

	/** 0..1. Drives AI aim error. Recruits start low; veterans improve with XP. */
	UPROPERTY(BlueprintReadOnly, SaveGame) float Accuracy = 0.5f;
	/** 0..1. Drives suppression resistance and retreat thresholds. */
	UPROPERTY(BlueprintReadOnly, SaveGame) float Morale = 0.6f;

	UPROPERTY(BlueprintReadOnly, SaveGame) int32 Experience = 0;
	UPROPERTY(BlueprintReadOnly, SaveGame) int32 MissionsFlown = 0;
	UPROPERTY(BlueprintReadOnly, SaveGame) int32 Kills = 0;
	UPROPERTY(BlueprintReadOnly, SaveGame) int32 WoundedMissionsRemaining = 0;

	/** Index into a portrait table (DataTable/asset list wired up in content). */
	UPROPERTY(BlueprintReadOnly, SaveGame) int32 PortraitIndex = 0;

	// Memorial data
	UPROPERTY(BlueprintReadOnly, SaveGame) int32 DiedOnMission = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly, SaveGame) FString CauseOfDeath;
	UPROPERTY(BlueprintReadOnly, SaveGame) FString DiedAt;	// mission / location name

	bool IsAvailable() const { return Status == EDVSoldierStatus::Active; }
	FString GetDisplayName() const;
};

/** Strategic currency for reinforcements, repairs and upgrades. */
USTRUCT(BlueprintType)
struct FDVWarResources
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, SaveGame) int32 Manpower = 12;
	UPROPERTY(BlueprintReadOnly, SaveGame) int32 Materiel = 100;
	UPROPERTY(BlueprintReadOnly, SaveGame) int32 Fuel = 100;
};

/** One operation as the Kestrel remembers it (feeds the hangar's memory: plaques, repairs, trophies, evacuees). */
USTRUCT(BlueprintType)
struct FDVOpRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, SaveGame) FString Name;
	UPROPERTY(BlueprintReadOnly, SaveGame) FString MapKey;
	UPROPERTY(BlueprintReadOnly, SaveGame) bool bSuccess = false;
	UPROPERTY(BlueprintReadOnly, SaveGame) int32 Lost = 0;
	UPROPERTY(BlueprintReadOnly, SaveGame) int32 Bulwarks = 0;
};

/** Everything the roster persists. Serialized to bytes and checksummed by UDVSaveGame. */
USTRUCT(BlueprintType)
struct FDVRosterState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, SaveGame) TArray<FDVSoldierRecord> Soldiers;
	UPROPERTY(BlueprintReadOnly, SaveGame) FDVWarResources Resources;
	UPROPERTY(BlueprintReadOnly, SaveGame) int32 MissionsCompleted = 0;
	UPROPERTY(BlueprintReadOnly, SaveGame) int32 RecruitSeed = 0;

	/** Set when the roster could not be refilled normally; unlocks the salvage mission fallback. */
	UPROPERTY(BlueprintReadOnly, SaveGame) bool bDireStraits = false;
	UPROPERTY(BlueprintReadOnly, SaveGame) bool bIronman = false;
};
