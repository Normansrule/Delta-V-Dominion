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

	UPROPERTY(EditDefaultsOnly, Category = "Mission") TSubclassOf<ADVSoldierCharacter> SquadmateClass;
	UPROPERTY(EditDefaultsOnly, Category = "Mission") int32 SquadSize = 3;
	UPROPERTY(EditDefaultsOnly, Category = "Mission") FString MissionName = TEXT("Training");
	UPROPERTY(EditDefaultsOnly, Category = "Mission") FDVWarResources SuccessReward;
	UPROPERTY(EditDefaultsOnly, Category = "Mission") float PlayerRespawnDelay = 6.f;
	UPROPERTY(EditDefaultsOnly, Category = "Mission") bool bCoopPlayersTakeOverSquadmates = true;
	/** Fraction of max health lost in a mission that sends a survivor to the medbay. */
	UPROPERTY(EditDefaultsOnly, Category = "Mission") float WoundedThreshold = 0.5f;

private:
	UDVRosterSubsystem* GetRoster() const;
	ADVSoldierCharacter* FindFreeSquadmate() const;
	void RespawnPlayer(TWeakObjectPtr<APlayerController> WeakPC);
	void RelinkSquadTo(ADVSoldierCharacter* NewLeader);

	TWeakObjectPtr<ADVSoldierCharacter> SquadLeader;
	TWeakObjectPtr<APlayerController> LeaderController;
	TArray<TWeakObjectPtr<ADVSoldierCharacter>> DeployedSquad;
	TArray<FGuid> DeployedIds;
	bool bMissionEnded = false;
};
