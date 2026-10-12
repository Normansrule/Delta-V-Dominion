#include "Characters/DVCharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include "Characters/DVSoldierCharacter.h"

UDVCharacterMovementComponent::UDVCharacterMovementComponent()
{
	MaxWalkSpeed = 420.f;
	MaxWalkSpeedCrouched = 200.f;
	JumpZVelocity = 420.f;
	AirControl = 0.25f;
	BrakingDecelerationWalking = 1600.f;
	GetNavAgentPropertiesRef().bCanCrouch = true;
	bCanWalkOffLedgesWhenCrouching = true;
	DefaultGroundFriction = GroundFriction;
}

void UDVCharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
	ADVSoldierCharacter* Soldier = Cast<ADVSoldierCharacter>(CharacterOwner);
	BoostCharges = FMath::Min(MaxBoostCharges, BoostCharges + BoostRechargePerSecond * DeltaSeconds);

	// Thrusters: a boost jump in the air, a dash on the ground. Each costs one charge.
	if (bWantsToBoost)
	{
		bWantsToBoost = false;
		if (BoostCharges >= 1.f && Soldier && !Soldier->IsOutOfAction())
		{
			const FVector Facing = UpdatedComponent->GetForwardVector().GetSafeNormal2D();
			const FVector Wish = GetCurrentAcceleration().IsNearlyZero() ? Facing : GetCurrentAcceleration().GetSafeNormal2D();
			if (IsFalling())
			{
				Velocity.Z = FMath::Max(Velocity.Z, 0.f) + BoostUp;
				Velocity += Wish * BoostAirPush;
			}
			else if (IsMovingOnGround())
			{
				Velocity = Wish * DashSpeed + FVector(0.f, 0.f, 160.f);
				SetMovementMode(MOVE_Falling);
			}
			BoostCharges -= 1.f;
			Soldier->OnThrusterBoost();
		}
	}

	// Slide: only out of a real sprint, on the ground.
	if (bWantsToSlide)
	{
		bWantsToSlide = false;
		if (!bSliding && IsMovingOnGround() && Velocity.Size2D() >= SlideMinSpeed && Soldier && !Soldier->IsOutOfAction())
		{
			bSliding = true;
			SlideTimeLeft = SlideDuration;
			Velocity += Velocity.GetSafeNormal2D() * SlideBoost;
			bWantsToSprint = false;
			CharacterOwner->Crouch(true);
			Soldier->OnSlideStarted();
		}
	}
	if (bSliding)
	{
		SlideTimeLeft -= DeltaSeconds;
		if (SlideTimeLeft <= 0.f || !IsMovingOnGround() || Velocity.Size2D() < 220.f)
		{
			bSliding = false;
		}
	}
	GroundFriction = bSliding ? 0.4f : DefaultGroundFriction;
}

float UDVCharacterMovementComponent::GetMaxAcceleration() const
{
	return bSliding ? 250.f : Super::GetMaxAcceleration();	// you steer a slide a little, you don't drive it
}

float UDVCharacterMovementComponent::GetMaxBrakingDeceleration() const
{
	return bSliding ? 220.f : Super::GetMaxBrakingDeceleration();
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
	if (bSliding)
	{
		return SprintSpeed + SlideBoost + 200.f;
	}
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
	bWantsToSlide |= (Flags & FSavedMove_Character::FLAG_Custom_2) != 0;
	bWantsToBoost |= (Flags & FSavedMove_Character::FLAG_Custom_3) != 0;
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
	bSavedWantsToSlide = false;
	bSavedWantsToBoost = false;
}

uint8 FDVSavedMove::GetCompressedFlags() const
{
	uint8 Result = Super::GetCompressedFlags();
	if (bSavedWantsToSprint) { Result |= FLAG_Custom_0; }
	if (bSavedWantsToAim) { Result |= FLAG_Custom_1; }
	if (bSavedWantsToSlide) { Result |= FLAG_Custom_2; }
	if (bSavedWantsToBoost) { Result |= FLAG_Custom_3; }
	return Result;
}

bool FDVSavedMove::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const
{
	const FDVSavedMove* Other = static_cast<const FDVSavedMove*>(NewMove.Get());
	if (bSavedWantsToSprint != Other->bSavedWantsToSprint || bSavedWantsToAim != Other->bSavedWantsToAim
		|| bSavedWantsToSlide || Other->bSavedWantsToSlide || bSavedWantsToBoost || Other->bSavedWantsToBoost)
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
		bSavedWantsToSlide = Move->bWantsToSlide;
		bSavedWantsToBoost = Move->bWantsToBoost;
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
