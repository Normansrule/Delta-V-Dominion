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

/** Operation stages: clear the garrison, plant charges on the target, hold until they blow, extract. */
UENUM(BlueprintType)
enum class EDVMissionStage : uint8
{
	Clear,
	Sabotage,
	Capture,	// hold a point for a few seconds (Breakwater: the dock control key)
	Defend,
	Extract
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

/** Local-only radio subtitle (filled by a multicast). */
struct FDVRadioLine
{
	FString Speaker;
	FString Text;
	double Time = 0.0;
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
	ADVGameState();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Mission") EDVMissionPhase Phase = EDVMissionPhase::InProgress;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Mission") FString MissionName;
	/** Flying home: 0 on the ground, 1 a Raven is carrying the squad up to the Kestrel, 2 landed aboard. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Mission") uint8 Ascent = 0;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Mission") FString ObjectiveText;
	/** Title card lines, e.g. "Tharsis Ridge, Mars" / "Day 3 of the Uprising". */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Mission") FString MissionLocation;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Mission") FString MissionSubtitle;
	/** Server time when the Kestrel's orbital strike is charged again. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Mission") float OrbitalReadyTime = 0.f;
	/** Server times when the supply drop and the Raven strafing run can be called again. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Mission") float SupplyReadyTime = 0.f;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Mission") float StrafeReadyTime = 0.f;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Mission") bool bMultiStage = false;
	/** Ironstride: the objective is the walker's power couplings (see ADVWalker). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Mission") bool bWalkerOp = false;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Mission") EDVMissionStage Stage = EDVMissionStage::Clear;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Mission") FVector_NetQuantize ObjectiveLocation;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Mission") FVector_NetQuantize ExtractLocation;
	/** 0..1 for the current stage (charges planted, hold timer, Raven inbound). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Mission") float StageProgress = 0.f;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Mission") float StageEndTime = 0.f;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Mission") int32 EnemiesTotal = 0;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Mission") int32 EnemiesRemaining = 0;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Mission") TArray<FDVAfterActionLine> AfterAction;
	/** Server time the mission started / ended, for the mission clock. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Mission") float MissionStartTime = 0.f;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Mission") float MissionEndTime = 0.f;

	UFUNCTION(NetMulticast, Reliable)
	void MulticastKillFeed(const FString& Killer, const FString& Victim, bool bFriendlyLoss);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastRadio(const FString& Speaker, const FString& Text);

	/** Server -> everyone: play a DVSound by name (Location zero = 2D). */
	/** Server -> everyone: Delta's standing with a faction changes (each player's profile records it). */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastReputation(uint8 Faction, int32 Delta, const FString& Reason);

	/** Something a Rook's Rest contract counts happened (DVContracts kind); every player's own contract hears it. */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastContractEvent(uint8 Kind);

	/** Every machine streams the Kestrel's hangar deck into the carrier hanging over this battlefield. */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastStreamHangar(FVector_NetQuantize Location, float Yaw);
	bool IsHangarReady() const;

	UFUNCTION() void OnHangarShown();
	UPROPERTY() TObjectPtr<class ULevelStreamingDynamic> HangarLevel;

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastSound(FName Sound, FVector_NetQuantize Location, float Volume, uint8 Range);

	virtual void BeginPlay() override;

	/** Cosmetics for an incoming orbital strike: target beam now, impact after Delay. */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastOrbitalStrike(FVector_NetQuantize Location, float Delay);

	const TArray<FDVKillFeedEntry>& GetKillFeed() const { return KillFeed; }
	const TArray<FDVRadioLine>& GetRadio() const { return Radio; }
	bool GetPendingStrike(FVector& OutLocation, double& OutImpactTime) const;

private:
	void PlayOrbitalImpact(FVector Location);

	// Hard references so the packaged game cooks these.
	UPROPERTY() TObjectPtr<class UStaticMesh> BeamMesh;
	UPROPERTY() TObjectPtr<class UStaticMesh> SphereMesh;
	UPROPERTY() TObjectPtr<class UMaterialInterface> BeamMaterial;
	UPROPERTY() TObjectPtr<class UNiagaraSystem> ExplosionFX;

	TArray<FDVKillFeedEntry> KillFeed;
	TArray<FDVRadioLine> Radio;
	FVector PendingStrikeLocation = FVector::ZeroVector;
	double PendingStrikeImpactTime = -1.0;
};
