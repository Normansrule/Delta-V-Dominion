#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DVCharacterMovementComponent.generated.h"

/**
 * Sprint, aim, slide and thruster-boost requests carried in the saved-move compressed flags,
 * so they are predicted on the client and replayed/corrected by the server like any
 * other movement input (no client-authoritative speed).
 */
UCLASS()
class DELTAVDOMINION_API UDVCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UDVCharacterMovementComponent();

	virtual float GetMaxSpeed() const override;
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual float GetMaxAcceleration() const override;
	virtual float GetMaxBrakingDeceleration() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DV Movement") float SprintSpeed = 680.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DV Movement") float AimSpeedMultiplier = 0.55f;

	/** Slide: sprint + crouch. A burst of speed that bleeds off with low friction. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DV Movement") float SlideBoost = 380.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DV Movement") float SlideDuration = 0.9f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DV Movement") float SlideMinSpeed = 450.f;
	/** Suit thrusters: Space in the air for a boost jump, Left Alt on the ground for a dash. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DV Movement") float BoostUp = 620.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DV Movement") float BoostAirPush = 420.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DV Movement") float DashSpeed = 1150.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DV Movement") float MaxBoostCharges = 2.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DV Movement") float BoostRechargePerSecond = 0.45f;

	bool bWantsToSprint = false;
	bool bWantsToAim = false;
	/** One-shot requests from input; consumed by the next movement update (and replayed by the server). */
	bool bWantsToSlide = false;
	bool bWantsToBoost = false;

	bool IsSprinting() const;
	bool IsSliding() const { return bSliding; }
	float GetBoostCharges() const { return BoostCharges; }

private:
	bool bSliding = false;
	float SlideTimeLeft = 0.f;
	float BoostCharges = 2.f;
	float DefaultGroundFriction = 8.f;
};

class FDVSavedMove : public FSavedMove_Character
{
public:
	typedef FSavedMove_Character Super;

	virtual void Clear() override;
	virtual uint8 GetCompressedFlags() const override;
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override;
	virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData) override;
	virtual void PrepMoveFor(ACharacter* C) override;

	bool bSavedWantsToSprint = false;
	bool bSavedWantsToAim = false;
	bool bSavedWantsToSlide = false;
	bool bSavedWantsToBoost = false;
};

class FDVNetworkPredictionData_Client : public FNetworkPredictionData_Client_Character
{
public:
	typedef FNetworkPredictionData_Client_Character Super;

	explicit FDVNetworkPredictionData_Client(const UCharacterMovementComponent& ClientMovement) : Super(ClientMovement) {}
	virtual FSavedMovePtr AllocateNewMove() override { return FSavedMovePtr(new FDVSavedMove()); }
};
