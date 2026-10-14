#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Characters/DVSquadTypes.h"
#include "DVSquadAIController.generated.h"

class ADVSoldierCharacter;
class UAISenseConfig_Sight;

UENUM(BlueprintType)
enum class EDVSquadAIState : uint8
{
	Idle,
	Moving,
	InCover,
	Engaging,
	Flanking,
	Suppressing,
	Pinned
};

/**
 * Squadmate (and enemy rifleman) brain.
 *
 * Orders from the squad leader select a behaviour (follow, move, hold, flank, suppress,
 * regroup); within it the bot perceives enemies, finds cover with DVCover, pops up from
 * low cover to fire bursts, and gets pinned when suppressed. Accuracy and morale come from
 * the soldier's roster record via GAS attributes, so green replacements really are worse.
 *
 * Written as a compact C++ state machine so it runs without editor assets. A StateTree
 * version can wrap the same Behave* functions as tasks once designers need to edit it.
 */
UCLASS()
class DELTAVDOMINION_API ADVSquadAIController : public AAIController
{
	GENERATED_BODY()

public:
	ADVSquadAIController(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaSeconds) override;
	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;

	void SetLeader(ADVSoldierCharacter* InLeader, int32 InFormationSlot);
	void ReceiveOrder(const FDVSquadOrderData& Order);

	/** Where the next round should go, including this soldier's aim error. Used by the fire ability. */
	FVector GetAimPoint() const;

	UFUNCTION(BlueprintPure, Category = "Squad AI") EDVSquadAIState GetAIState() const { return State; }
	UFUNCTION(BlueprintPure, Category = "Squad AI") EDVSquadOrder GetCurrentOrder() const { return CurrentOrder.Order; }
	UFUNCTION(BlueprintPure, Category = "Squad AI") ADVSoldierCharacter* GetLeader() const { return Leader.Get(); }

	// --- Tuning ---
	UPROPERTY(EditDefaultsOnly, Category = "Squad AI") float ThinkInterval = 0.25f;
	UPROPERTY(EditDefaultsOnly, Category = "Squad AI") float SightRadius = 6000.f;
	UPROPERTY(EditDefaultsOnly, Category = "Squad AI") float CoverSearchRadius = 900.f;
	UPROPERTY(EditDefaultsOnly, Category = "Squad AI") float FlankDistance = 1400.f;
	UPROPERTY(EditDefaultsOnly, Category = "Squad AI") float SuppressDuration = 10.f;
	/** Aim error in degrees for accuracy 0 and accuracy 1. */
	UPROPERTY(EditDefaultsOnly, Category = "Squad AI") FVector2D AimErrorRange = FVector2D(7.f, 0.8f);
	/** Seconds a target is remembered after losing sight. */
	UPROPERTY(EditDefaultsOnly, Category = "Squad AI") float TargetMemory = 6.f;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	UPROPERTY(VisibleAnywhere, Category = "Squad AI") TObjectPtr<UAISenseConfig_Sight> SightConfig;

private:
	void Think();
	void UpdateTarget();
	bool CanNotice(const AActor* Actor) const;
	void RaiseAlarm(const FVector& Where);

	void BehaveFollow(bool bRegroup);
	void BehaveMoveTo();
	void BehaveHold(const FVector& Anchor);
	void BehaveFlank();
	void BehaveSuppress();

	/** Fight the current target from cover near Anchor. */
	void Engage(const FVector& Anchor, float CoverRadius);
	bool SeekCover(const FVector& Anchor, float Radius, const FVector& Threat);
	void LeaveCover();
	void MoveIfFar(const FVector& Dest, float Tolerance, bool bSprint = false);
	void StartBurst(int32 MinShots, int32 MaxShots);
	bool HasShotAt(const FVector& Point, const AActor* IgnoreTarget) const;
	bool IsPinned() const;
	FVector GetFormationSlotLocation() const;
	FVector TargetChest() const;
	TArray<FVector> GetClaimedCoverPoints() const;
	ADVSoldierCharacter* GetSoldier() const;

	TWeakObjectPtr<ADVSoldierCharacter> Leader;
	int32 FormationSlot = 0;
	FDVSquadOrderData CurrentOrder;
	FVector HoldPoint = FVector::ZeroVector;

	TWeakObjectPtr<AActor> Target;
	FVector LastKnownTargetLocation = FVector::ZeroVector;
	double LastSeenTime = -1000.0;

	bool bInCover = false;
	bool bLowCover = false;
	FVector CoverLocation = FVector::ZeroVector;
	double NextCoverCheckTime = 0.0;

	FVector FlankPoint = FVector::ZeroVector;
	bool bFlankReached = false;

	FVector SuppressPoint = FVector::ZeroVector;
	bool bSuppressFire = false;
	double SuppressUntil = 0.0;

	int32 BurstRemaining = 0;
	double NextBurstTime = 0.0;

	FVector CurrentMoveDest = FVector(FLT_MAX);
	EDVSquadAIState State = EDVSquadAIState::Idle;
	FTimerHandle ThinkTimer;
};
