#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "DVGameState.generated.h"

UENUM(BlueprintType)
enum class EDVMissionPhase : uint8
{
	InProgress,
	Success,
	Failure
};

/** One row of the after-action report. */
USTRUCT(BlueprintType)
struct FDVAfterActionLine
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) FString Rank;
	UPROPERTY(BlueprintReadOnly) int32 Kills = 0;
	UPROPERTY(BlueprintReadOnly) bool bKIA = false;
	UPROPERTY(BlueprintReadOnly) bool bWounded = false;
	UPROPERTY(BlueprintReadOnly) bool bPromoted = false;
};

/** Local-only kill feed entry (filled by a multicast, never replicated as state). */
struct FDVKillFeedEntry
{
	FString Killer;
	FString Victim;
	bool bFriendlyLoss = false;
	double Time = 0.0;
};

/**
 * Mission state everyone sees: objective, enemy count, phase and the after-action report.
 * Written only by ADVGameMode on the server; replicated to co-op clients for their HUDs.
 */
UCLASS()
class DELTAVDOMINION_API ADVGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Mission") EDVMissionPhase Phase = EDVMissionPhase::InProgress;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Mission") FString MissionName;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Mission") FString ObjectiveText;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Mission") int32 EnemiesTotal = 0;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Mission") int32 EnemiesRemaining = 0;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Mission") TArray<FDVAfterActionLine> AfterAction;
	/** Server time the mission started / ended, for the mission clock. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Mission") float MissionStartTime = 0.f;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Mission") float MissionEndTime = 0.f;

	UFUNCTION(NetMulticast, Reliable)
	void MulticastKillFeed(const FString& Killer, const FString& Victim, bool bFriendlyLoss);

	const TArray<FDVKillFeedEntry>& GetKillFeed() const { return KillFeed; }

private:
	TArray<FDVKillFeedEntry> KillFeed;
};
