#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "ChaosVehicleWheel.h"
#include "DVGroundVehicle.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class UDVSeatComponent;

UCLASS()
class DELTAVDOMINION_API UDVWheelFront : public UChaosVehicleWheel
{
	GENERATED_BODY()
public:
	UDVWheelFront();
};

UCLASS()
class DELTAVDOMINION_API UDVWheelRear : public UChaosVehicleWheel
{
	GENERATED_BODY()
public:
	UDVWheelRear();
};

/**
 * Drivable ground vehicle on Chaos vehicle physics. Base class is the Accord's M-14 "Jackal" light
 * tactical vehicle with a roof heavy machine gun; ADVTank swaps the body for an armoured hull and turret.
 * Driver: W/S throttle, A/D steer, Space handbrake, mouse aims the weapon, LMB fires, Z exits.
 */
UCLASS()
class DELTAVDOMINION_API ADVGroundVehicle : public AWheeledVehiclePawn
{
	GENERATED_BODY()

public:
	ADVGroundVehicle();

	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UDVSeatComponent* GetSeat() const { return Seat; }
	float GetReloadFraction() const;
	float GetSpeedKph() const;

	/** Scripted input for trailers and tests (used when no driving keys are held). */
	void SetAutopilot(float Throttle, float Steer, bool bFire) { AutoThrottle = Throttle; AutoSteer = Steer; bAutoFire = bFire; }

protected:
	UFUNCTION(Server, Unreliable) void ServerSetAim(FRotator NewAim);
	UFUNCTION(Server, Reliable) void ServerFire(FVector_NetQuantize AimPoint);
	UFUNCTION(NetMulticast, Unreliable) void MulticastShot(FVector_NetQuantize Muzzle, FVector_NetQuantize Impact, bool bExplosive);

	void FirePressed() { bFireHeld = true; }
	void FireReleased() { bFireHeld = false; }
	FVector GetAimPoint() const;
	FVector GetMuzzleLocation() const;

	UPROPERTY(VisibleAnywhere) TObjectPtr<UDVSeatComponent> Seat;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> BodyShell;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Tires;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USpringArmComponent> CameraArm;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
	/** Yaws with the aim. */
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Turret;
	/** Pitches with the aim; its +X is the barrel. */
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Gun;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> HmgParts;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> HeadlightL;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> HeadlightR;

	/** Weapon tuning (the tank overrides these). */
	float FireInterval = 0.09f;
	float Damage = 45.f;
	float SplashRadius = 0.f;
	float MuzzleLength = 120.f;
	float MaxPitch = 35.f;
	float MinPitch = -12.f;
	bool bExplosiveRounds = false;

	UPROPERTY(Replicated) FRotator AimRotation = FRotator::ZeroRotator;	// relative to the vehicle
	double LastShotTime = -100.0;
	double LastServerShotTime = -100.0;
	bool bFireHeld = false;
	float AutoThrottle = 0.f;
	float AutoSteer = 0.f;
	bool bAutoFire = false;
};

/** M9 "Sabre" main battle tank: armoured hull and tracks over the same Chaos chassis, 120 mm main gun with splash. */
UCLASS()
class DELTAVDOMINION_API ADVTank : public ADVGroundVehicle
{
	GENERATED_BODY()

public:
	ADVTank();
};
