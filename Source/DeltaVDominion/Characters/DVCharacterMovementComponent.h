#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DVCharacterMovementComponent.generated.h"

/**
 * Sprint and aim-down-sights speed changes carried in the saved-move compressed flags,
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DV Movement") float SprintSpeed = 680.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DV Movement") float AimSpeedMultiplier = 0.55f;

	bool bWantsToSprint = false;
	bool bWantsToAim = false;

	bool IsSprinting() const;
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
};

class FDVNetworkPredictionData_Client : public FNetworkPredictionData_Client_Character
{
public:
	typedef FNetworkPredictionData_Client_Character Super;

	explicit FDVNetworkPredictionData_Client(const UCharacterMovementComponent& ClientMovement) : Super(ClientMovement) {}
	virtual FSavedMovePtr AllocateNewMove() override { return FSavedMovePtr(new FDVSavedMove()); }
};
