#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "DVBoat.generated.h"

class UBoxComponent;
class UCameraComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class UDVSeatComponent;
class UAudioComponent;

/**
 * PB-9 "Marlin" missile patrol boat. Rides the water plane of the map (the actor tagged DV_Water):
 * W/S throttle, mouse steers, LMB 30 mm deck gun, RMB a pair of missiles from the pop-up launcher.
 * Movement runs on the server from the helm's inputs and replicates.
 */
UCLASS()
class DELTAVDOMINION_API ADVBoat : public APawn
{
	GENERATED_BODY()

public:
	ADVBoat();

	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;

	float GetSpeedKph() const { return Velocity.Size() * 0.036f; }
	float GetMissileReadyFraction() const;

	void SetAutopilot(float Throttle, float YawRate, bool bFire) { AutoThrottle = Throttle; AutoYawRate = YawRate; bAutoFire = bFire; }

	static constexpr float TopSpeed = 2600.f;

protected:
	UFUNCTION(Server, Unreliable) void ServerInput(float Throttle, float Yaw);
	UFUNCTION(Server, Reliable) void ServerFire(bool bMissile, FVector_NetQuantize AimPoint);
	UFUNCTION(NetMulticast, Unreliable) void MulticastShot(FVector_NetQuantize Muzzle, FVector_NetQuantize Impact, bool bMissile);

	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Collision;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Body;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> LauncherDoors;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UDVSeatComponent> Seat;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USpringArmComponent> CameraArm;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Wake;
	UPROPERTY() TObjectPtr<UAudioComponent> EngineSound;

	float WaterZ = 0.f;
	float ThrottleInput = 0.f;
	float TargetYaw = 0.f;
	FVector Velocity = FVector::ZeroVector;
	double LastGun = -100.0, LastMissile = -100.0;
	float AutoThrottle = 0.f;
	float AutoYawRate = 0.f;
	bool bAutoFire = false;
	int32 MissileSide = 1;
};

/** A civilian transport in the harbour: it sails for open water once the squad has secured the docks. */
UCLASS()
class DELTAVDOMINION_API ADVTransportShip : public AActor
{
	GENERATED_BODY()

public:
	ADVTransportShip();
	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Lights;
	float Speed = 0.f;
};
