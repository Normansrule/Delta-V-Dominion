#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Core/DVRosterTypes.h"
#include "DVGameMode.generated.h"

class ADVSoldierCharacter;
class UDVRosterSubsystem;

/**
 * Mission game mode (server only).
 *
 * - The first player becomes squad leader and gets a squad drawn from the persistent roster.
 * - Co-op: players who join later take over a living AI squadmate (that soldier's fate is
 *   still tracked on the roster).
 * - Deaths of roster soldiers are reported KIA immediately; EndMission() applies XP,
 *   wounds, rewards and reinforcements.
 */
UCLASS()
class DELTAVDOMINION_API ADVGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADVGameMode();

	virtual void StartPlay() override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

	void NotifySoldierKilled(ADVSoldierCharacter* Victim, ADVSoldierCharacter* Killer, AController* KillerController);
	void NotifySoldierDowned(ADVSoldierCharacter* Soldier);
	void NotifySoldierRevived(ADVSoldierCharacter* Soldier);

	UFUNCTION(BlueprintCallable, Category = "Mission")
	void SpawnSquadFor(ADVSoldierCharacter* Leader);

	UFUNCTION(BlueprintCallable, Category = "Mission")
	void EndMission(bool bSuccess);

	/** After the mission ends: reload the level with the updated roster (Enter on the after-action screen). */
	void RequestRedeploy();

	/** Kestrel orbital strike on Target, called in by a player. Shared cooldown for the whole squad. */
	void RequestOrbitalStrike(APlayerController* Caller, const FVector& Target);
	void RequestSupplyDrop(APlayerController* Caller, const FVector& Target);
	/** Dev builds: jump the operation to its next stage (tests and trailer captures). */
	void DevAdvanceStage();
	void RequestStrafeRun(APlayerController* Caller, const FVector& Target);

	/**
	 * Fly home: after an operation (or from a hub town), a Raven lifts the commander off and flies up into the
	 * Kestrel overhead, whose hangar deck streams into the hull. No loading screen. See ADVAscent.
	 */
	void BeginAscent(APlayerController* PC);
	void OnAscentLanded(APlayerController* PC, class ADVGunship* Raven);

	/** Ironstride: a power coupling blew (Count of 3), and the walker hit the ground. */
	void OnWalkerCoupling(int32 Count);
	void OnWalkerDown();

	/** Brasshaven: a valve on the Steamheart's manifold turned; bSolved when all three gauges sit in the green. */
	void OnManifoldTurned(int32 Valve, bool bSolved);
	/** A townsperson was shot at or caught in a blast by Delta. Costs standing with whoever's town it is. */
	void NotifyCivilianHurt(class ADVTownsfolk* Civilian, AActor* Shooter);

	UPROPERTY(EditDefaultsOnly, Category = "Mission|Operation") float PlantSeconds = 4.f;
	UPROPERTY(EditDefaultsOnly, Category = "Mission|Operation") float HoldSeconds = 45.f;
	UPROPERTY(EditDefaultsOnly, Category = "Mission|Operation") float RavenInboundSeconds = 12.f;

	UPROPERTY(EditDefaultsOnly, Category = "Mission|CallIns") float SupplyCooldown = 45.f;
	UPROPERTY(EditDefaultsOnly, Category = "Mission|CallIns") float StrafeCooldown = 60.f;

	UPROPERTY(EditDefaultsOnly, Category = "Mission|Orbital") float OrbitalCooldown = 40.f;
	UPROPERTY(EditDefaultsOnly, Category = "Mission|Orbital") float OrbitalDelay = 3.f;
	UPROPERTY(EditDefaultsOnly, Category = "Mission|Orbital") float OrbitalRadius = 900.f;
	UPROPERTY(EditDefaultsOnly, Category = "Mission|Orbital") float OrbitalDamage = 400.f;

	UPROPERTY(EditDefaultsOnly, Category = "Mission") TSubclassOf<ADVSoldierCharacter> SquadmateClass;
	UPROPERTY(EditDefaultsOnly, Category = "Mission") int32 SquadSize = 3;
	UPROPERTY(EditDefaultsOnly, Category = "Mission") FString MissionName = TEXT("Training");
	UPROPERTY(EditDefaultsOnly, Category = "Mission") FDVWarResources SuccessReward;
	UPROPERTY(EditDefaultsOnly, Category = "Mission") float PlayerRespawnDelay = 6.f;
	UPROPERTY(EditDefaultsOnly, Category = "Mission") bool bCoopPlayersTakeOverSquadmates = true;
	/** Team of the players and their squad; every other team counts toward the objective. */
	UPROPERTY(EditDefaultsOnly, Category = "Mission") uint8 PlayerTeamId = 1;
	/** Fraction of max health lost in a mission that sends a survivor to the medbay. */
	UPROPERTY(EditDefaultsOnly, Category = "Mission") float WoundedThreshold = 0.5f;

private:
	UDVRosterSubsystem* GetRoster() const;
	ADVSoldierCharacter* FindFreeSquadmate() const;
	void RespawnPlayer(TWeakObjectPtr<APlayerController> WeakPC);
	void RelinkSquadTo(ADVSoldierCharacter* NewLeader);
	bool AnySquadmateAlive() const;
	void SetupMissionForMap();
	void SpawnBulwarks();
	void SetupOperation();
	void TickStage();
	void StartSabotage();
	void StartDefend();
	void StartCapture();
	/** Breakwater-style operations: objectives come from actors tagged DV_Obj_1 / DV_Obj_2 / DV_LZ in the map. */
	bool bScriptedObjectives = false;
	bool bWalkerOp = false;
	bool bManifoldOp = false;
	bool bValeOp = false;
	void StartHerd();
	/** Hollyhock: count fixed pumps / penned puffbots and point the marker at the nearest one still to do. */
	void TickVale();
	FTimerHandle FestivalTimer;
	int32 FestivalBursts = 0;
	/** The faction whose people live on this map (who minds if they get hurt). */
	int32 LocalFaction = -1;
	bool bWarnedRoE = false;
	FVector SecondObjective = FVector::ZeroVector;
	void StartExtract();
	void SpawnWave(int32 Count);
	bool AnyPlayerNear(const FVector& Location, float Radius) const;
	FString TargetNoun;
	FTimerHandle StageTimer;
	double StageStartTime = 0.0;
	int32 WavesSpawned = 0;
	TWeakObjectPtr<class ADVGunship> ExtractRaven;
	bool CanCallIn(APlayerController* Caller, const FVector& Target, float ReadyTime) const;
	void Radio(const FString& Speaker, const FString& Text, float Delay = 0.f);
	/** Queued radio lines, so a new scene (the flight home) can cut off chatter left over from the last one. */
	TArray<FTimerHandle> RadioTimers;
	void ResolveOrbitalStrike(TWeakObjectPtr<APawn> Caller, FVector Target);
	int32 HostileKills = 0;
	int32 LostThisOp = 0;
	int32 BulwarksThisOp = 0;
	/** What the deck crew says when you come aboard, from the operations log. */
	void KestrelGreeting(float Delay);
	bool bIsTrainingMap = false;
	bool bIsHub = false;
	class ADVGameState* GetDVGameState() const;

	TWeakObjectPtr<ADVSoldierCharacter> SquadLeader;
	TWeakObjectPtr<APlayerController> LeaderController;
	TArray<TWeakObjectPtr<ADVSoldierCharacter>> DeployedSquad;
	TArray<FGuid> DeployedIds;
	bool bMissionEnded = false;
};
