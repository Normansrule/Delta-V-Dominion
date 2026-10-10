#pragma once

#include "CoreMinimal.h"
#include "Combat/DVGameplayAbility.h"
#include "DVGA_FireWeapon.generated.h"

/**
 * One trigger pull of the equipped weapon (all pellets).
 *
 * Network flow:
 *  - The shooter's machine traces (camera for players, AI aim point for bots), plays
 *    effects immediately, and sends the hits to the server as target data.
 *  - The server validates every hit (fire rate, ammo, shot origin, range, aim angle,
 *    line of sight and a ping-scaled target position tolerance) before applying damage.
 *  - AI and the listen-server host trace on the server and skip the client checks.
 *
 * Full rewind-based lag compensation is a later milestone; the tolerance check is the
 * placeholder for it (see ValidateHit).
 */
UCLASS()
class DELTAVDOMINION_API UDVGA_FireWeapon : public UDVGameplayAbility
{
	GENERATED_BODY()

public:
	UDVGA_FireWeapon();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** Max distance (cm) between the client's reported shot origin and the server's view of the shooter's eyes. */
	UPROPERTY(EditDefaultsOnly, Category = "Validation") float MaxOriginError = 150.f;
	/** Extra degrees of aim deviation tolerated beyond the weapon's spread. */
	UPROPERTY(EditDefaultsOnly, Category = "Validation") float AimToleranceDegrees = 12.f;
	/** Base tolerance (cm) for where a hit target can be relative to the server; ping-scaled velocity is added. */
	UPROPERTY(EditDefaultsOnly, Category = "Validation") float HitPositionTolerance = 75.f;

private:
	void TraceAndFireLocally();
	void OnTargetDataReady(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ApplicationTag);
	void ProcessShotOnServer(const FGameplayAbilityTargetDataHandle& Data, bool bTraceWasServerSide);
	bool ValidateHit(const FHitResult& Hit, float AllowedSpreadDeg) const;
	void ApplySuppressionAlongShot(const FVector& Start, const FVector& End, AActor* DirectHitActor) const;
	void OnServerWaitTimedOut();

	FTimerHandle ServerWaitTimer;
	FDelegateHandle TargetDataDelegateHandle;
};
