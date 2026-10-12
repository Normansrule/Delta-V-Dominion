#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "DVHoverbike.generated.h"

class UBoxComponent;
class UCameraComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class UDVSeatComponent;
class UAudioComponent;

/**
 * VX-3 "Skiff" hoverbike: a quad-ducted-fan flying motorcycle that skims a metre and a half above the
 * ground. W/S throttle, mouse steers, Space hops, Left Shift boosts, LMB twin cannons. Fast, fragile,
 * the scout's ride. Movement runs on the server from the rider's inputs and replicates.
 */
UCLASS()
class DELTAVDOMINION_API ADVHoverbike : public APawn
{
	GENERATED_BODY()

public:
	ADVHoverbike();

	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;

	float GetSpeedKph() const { return Velocity.Size() * 0.036f; }
	float GetBoostFraction() const { return Boost; }

	/** Scripted input for trailers and tests. */
	void SetAutopilot(float Throttle, float YawRate, bool bFire) { AutoThrottle = Throttle; AutoYawRate = YawRate; bAutoFire = bFire; }

	static constexpr float HoverHeight = 150.f;
	static constexpr float TopSpeed = 3400.f;
	static constexpr float BoostSpeed = 5200.f;

protected:
	UFUNCTION(Server, Unreliable) void ServerInput(float Throttle, float Yaw, bool bBoost, bool bHop);
	UFUNCTION(Server, Reliable) void ServerFire(FVector_NetQuantize AimPoint);
	UFUNCTION(NetMulticast, Unreliable) void MulticastShot(FVector_NetQuantize Muzzle, FVector_NetQuantize Impact, bool bHit);

	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Collision;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Body;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UDVSeatComponent> Seat;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USpringArmComponent> CameraArm;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> FanGlows;
	UPROPERTY() TObjectPtr<UAudioComponent> EngineSound;

	float ThrottleInput = 0.f;
	float TargetYaw = 0.f;
	bool bBoostHeld = false;
	bool bHopRequested = false;
	float Boost = 1.f;	// 0..1 boost tank
	FVector Velocity = FVector::ZeroVector;
	double LastShot = -100.0;
	int32 GunSide = 1;
	bool bWasSpace = false;
	float AutoThrottle = 0.f;
	float AutoYawRate = 0.f;
	bool bAutoFire = false;
};
