#include "Characters/DVCharacterMovementComponent.h"
#include "GameFramework/Character.h"

UDVCharacterMovementComponent::UDVCharacterMovementComponent()
{
	MaxWalkSpeed = 420.f;
	MaxWalkSpeedCrouched = 200.f;
	JumpZVelocity = 420.f;
	AirControl = 0.25f;
	BrakingDecelerationWalking = 1600.f;
	GetNavAgentPropertiesRef().bCanCrouch = true;
	bCanWalkOffLedgesWhenCrouching = true;
}

bool UDVCharacterMovementComponent::IsSprinting() const
{
	if (!bWantsToSprint || bWantsToAim || !IsMovingOnGround() || IsCrouching())
	{
		return false;
	}
	// Sprint only when moving mostly forward.
	const FVector Forward = UpdatedComponent ? UpdatedComponent->GetForwardVector() : FVector::ForwardVector;
	const FVector Accel = GetCurrentAcceleration().GetSafeNormal2D();
	return FVector::DotProduct(Forward, Accel) > 0.6f;
}

float UDVCharacterMovementComponent::GetMaxSpeed() const
{
	float Speed = Super::GetMaxSpeed();
	if (MovementMode == MOVE_Walking || MovementMode == MOVE_NavWalking)
	{
		if (IsSprinting())
		{
			Speed = SprintSpeed;
		}
		else if (bWantsToAim)
		{
			Speed *= AimSpeedMultiplier;
		}
	}
	return Speed;
}

void UDVCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);
	bWantsToSprint = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
	bWantsToAim = (Flags & FSavedMove_Character::FLAG_Custom_1) != 0;
}

FNetworkPredictionData_Client* UDVCharacterMovementComponent::GetPredictionData_Client() const
{
	if (!ClientPredictionData)
	{
		UDVCharacterMovementComponent* MutableThis = const_cast<UDVCharacterMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FDVNetworkPredictionData_Client(*this);
	}
	return ClientPredictionData;
}

void FDVSavedMove::Clear()
{
	Super::Clear();
	bSavedWantsToSprint = false;
	bSavedWantsToAim = false;
}

uint8 FDVSavedMove::GetCompressedFlags() const
{
	uint8 Result = Super::GetCompressedFlags();
	if (bSavedWantsToSprint) { Result |= FLAG_Custom_0; }
	if (bSavedWantsToAim) { Result |= FLAG_Custom_1; }
	return Result;
}

bool FDVSavedMove::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const
{
	const FDVSavedMove* Other = static_cast<const FDVSavedMove*>(NewMove.Get());
	if (bSavedWantsToSprint != Other->bSavedWantsToSprint || bSavedWantsToAim != Other->bSavedWantsToAim)
	{
		return false;
	}
	return Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
}

void FDVSavedMove::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);
	if (const UDVCharacterMovementComponent* Move = Cast<UDVCharacterMovementComponent>(C->GetCharacterMovement()))
	{
		bSavedWantsToSprint = Move->bWantsToSprint;
		bSavedWantsToAim = Move->bWantsToAim;
	}
}

void FDVSavedMove::PrepMoveFor(ACharacter* C)
{
	Super::PrepMoveFor(C);
	if (UDVCharacterMovementComponent* Move = Cast<UDVCharacterMovementComponent>(C->GetCharacterMovement()))
	{
		Move->bWantsToSprint = bSavedWantsToSprint;
		Move->bWantsToAim = bSavedWantsToAim;
	}
}
