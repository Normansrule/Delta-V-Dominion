#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameFramework/SaveGame.h"
#include "Core/DVRosterTypes.h"
#include "DVRosterSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDVOnRosterChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDVOnSoldierKilled, const FDVSoldierRecord&, Soldier);

/** What happened on a mission, handed to the roster when the mission ends. */
USTRUCT(BlueprintType)
struct FDVMissionReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite) FString MissionName;
	UPROPERTY(BlueprintReadWrite) bool bSuccess = true;
	/** Soldiers who deployed and survived. KIA soldiers were already reported via ReportKIA. */
	UPROPERTY(BlueprintReadWrite) TArray<FGuid> Survivors;
	/** Survivors who took heavy damage and go to the medbay. */
	UPROPERTY(BlueprintReadWrite) TArray<FGuid> Wounded;
	UPROPERTY(BlueprintReadWrite) FDVWarResources Reward;
};

/**
 * Persistent attrition: named soldiers, permanent death, greener replacements,
 * war resources and save/load. Lives on the GameInstance so it survives map travel.
 *
 * Only the authority (single player, or the co-op host) mutates the roster.
 */
/**
 * The operations log, saved beside the roster (its own slot, tagged properties) so the checksummed roster format
 * doesn't change. New campaign, new log.
 */
UCLASS()
class DELTAVDOMINION_API UDVOpLogSave : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY() TArray<FDVOpRecord> Ops;
};

UCLASS(Config = Game)
class DELTAVDOMINION_API UDVRosterSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// --- Campaign lifecycle -------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "Roster")
	void NewCampaign(bool bIronman, int32 Seed = 0);

	UFUNCTION(BlueprintCallable, Category = "Roster")
	bool SaveRoster();

	UFUNCTION(BlueprintCallable, Category = "Roster")
	bool LoadRoster();

	// --- Queries -------------------------------------------------------------
	UFUNCTION(BlueprintPure, Category = "Roster")
	FDVRosterState GetState() const { return State; }

	UFUNCTION(BlueprintCallable, Category = "Roster")
	bool GetSoldier(const FGuid& Id, FDVSoldierRecord& OutRecord) const;

	UFUNCTION(BlueprintCallable, Category = "Roster")
	TArray<FDVSoldierRecord> GetAvailableSoldiers() const;

	/** Every soldier ever lost, in order of death. Feeds the Memorial Wall. */
	UFUNCTION(BlueprintCallable, Category = "Roster")
	TArray<FDVSoldierRecord> GetMemorial() const;

	/** Picks the most experienced available soldiers for a deployment. */
	UFUNCTION(BlueprintCallable, Category = "Roster")
	TArray<FGuid> SelectSquad(int32 MaxSize) const;

	// --- Combat events (authority only) -------------------------------------
	UFUNCTION(BlueprintCallable, Category = "Roster")
	void ReportKIA(const FGuid& Id, const FString& Cause, const FString& Where);

	UFUNCTION(BlueprintCallable, Category = "Roster")
	void ReportKill(const FGuid& Id);

	/** Applies XP, promotions, medbay recovery and rewards, refills the roster, then saves. */
	UFUNCTION(BlueprintCallable, Category = "Roster")
	void CompleteMission(const FDVMissionReport& Report);

	/** Recruits until there are at least TargetAvailable soldiers, spending manpower. */
	UFUNCTION(BlueprintCallable, Category = "Roster")
	int32 Reinforce(int32 TargetAvailable);

	UFUNCTION(BlueprintCallable, Category = "Roster")
	void AddResources(const FDVWarResources& Delta);

	/** Every operation flown this campaign, oldest first. The Kestrel's hangar rebuilds itself from this. */
	const TArray<FDVOpRecord>& GetOpLog() const { return OpLog; }
	void RecordOperation(const FDVOpRecord& Record);
	/** Soldiers currently in the medbay. */
	TArray<FDVSoldierRecord> GetWounded() const;

	UPROPERTY(BlueprintAssignable) FDVOnRosterChanged OnRosterChanged;
	UPROPERTY(BlueprintAssignable) FDVOnSoldierKilled OnSoldierKilled;

	// --- Tuning (Config/DefaultGame.ini, [/Script/DeltaVDominion.DVRosterSubsystem]) ---
	UPROPERTY(Config) FString SaveSlot = TEXT("Campaign0");
	UPROPERTY(Config) int32 StartingRosterSize = 8;
	UPROPERTY(Config) int32 ManpowerPerRecruit = 2;
	UPROPERTY(Config) int32 WoundedRecoveryMissions = 2;
	/** Soldiers the campaign guarantees (via emergency conscripts) so it stays winnable. */
	UPROPERTY(Config) int32 DireStraitsMinimum = 4;
	UPROPERTY(Config) int32 XPPerMission = 100;
	UPROPERTY(Config) int32 XPPerKill = 25;

	static FString RankToString(EDVRank Rank);

	/** The weapon picked at the Kestrel armoury (DVLoadouts index); carried into every mission this session. */
	int32 PlayerLoadout = 0;

private:
	FDVSoldierRecord* FindSoldier(const FGuid& Id);
	FDVSoldierRecord MakeRecruit(bool bConscript);
	void ApplyExperience(FDVSoldierRecord& Soldier, int32 XP);
	int32 CountAvailable() const;
	void Broadcast();

	UPROPERTY() FDVRosterState State;
	UPROPERTY() TArray<FDVOpRecord> OpLog;
	void SaveOpLog();
	FRandomStream Rng;
};
