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

	UFUNCTION(BlueprintCallable, Category = "Mission")
	void SpawnSquadFor(ADVSoldierCharacter* Leader);

	UFUNCTION(BlueprintCallable, Category = "Mission")
	void EndMission(bool bSuccess);

	/** After the mission ends: reload the level with the updated roster (Enter on the after-action screen). */
	void RequestRedeploy();

	/** Kestrel orbital strike on Target, called in by a player. Shared cooldown for the whole squad. */
	void RequestOrbitalStrike(APlayerController* Caller, const FVector& Target);

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
	void Radio(const FString& Speaker, const FString& Text, float Delay = 0.f);
	void ResolveOrbitalStrike(TWeakObjectPtr<APawn> Caller, FVector Target);
	int32 HostileKills = 0;
	bool bIsTrainingMap = false;
	class ADVGameState* GetDVGameState() const;

	TWeakObjectPtr<ADVSoldierCharacter> SquadLeader;
	TWeakObjectPtr<APlayerController> LeaderController;
	TArray<TWeakObjectPtr<ADVSoldierCharacter>> DeployedSquad;
	TArray<FGuid> DeployedIds;
	bool bMissionEnded = false;
};
