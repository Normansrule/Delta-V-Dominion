#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DVCallIns.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;
class ADVGunship;
class ADVSoldierCharacter;

/**
 * Kestrel supply drop: a pod that falls from orbit onto the painted spot, then refills ammo, health
 * and armour for friendly soldiers who stand next to it (a limited number of refills).
 */
UCLASS()
class DELTAVDOMINION_API ADVSupplyPod : public AActor
{
	GENERATED_BODY()

public:
	ADVSupplyPod();

	virtual void Tick(float DeltaSeconds) override;

	/** Server: start the fall toward Ground (world point on the terrain). */
	void Launch(const FVector& Ground);

	int32 GetChargesLeft() const { return Charges; }
	bool HasLanded() const { return bLanded; }

	static constexpr float Reach = 380.f;

protected:
	UFUNCTION(NetMulticast, Reliable) void MulticastLanded(FVector_NetQuantize Location);
	void Resupply();

	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Beacon;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Trail;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> Light;
	UPROPERTY(VisibleAnywhere) TArray<TObjectPtr<UStaticMeshComponent>> Strips;

	UPROPERTY(Replicated) int32 Charges = 8;
	UPROPERTY(Replicated) bool bLanded = false;
	FVector GroundPoint = FVector::ZeroVector;
	double NextResupply = 0.0;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};

/**
 * Raven strafing run: a gunship sweeps in low along the caller's line of sight and walks 30 mm
 * cannon fire through the painted spot. Friendly fire is off: the pilot can see your IFF strobes.
 */
UCLASS()
class DELTAVDOMINION_API ADVStrafeRun : public AActor
{
	GENERATED_BODY()

public:
	ADVStrafeRun();

	virtual void Tick(float DeltaSeconds) override;
	virtual void Destroyed() override;

	/** Server: begin a pass over Target heading along Direction (horizontal). */
	void Begin(ADVSoldierCharacter* Caller, const FVector& Target, const FVector& Direction);

	static constexpr float Altitude = 2600.f;
	static constexpr float Speed = 4200.f;
	static constexpr float RunIn = 11000.f;
	static constexpr float BeatenZone = 1700.f;

protected:
	UFUNCTION(NetMulticast, Unreliable) void MulticastBurst(FVector_NetQuantize From, FVector_NetQuantize To, bool bBig);

	TWeakObjectPtr<ADVSoldierCharacter> Shooter;
	UPROPERTY() TObjectPtr<ADVGunship> Aircraft;
	FVector Target = FVector::ZeroVector;
	FVector Dir = FVector::ForwardVector;
	float Along = 0.f;
	double NextShot = 0.0;
	int32 Shots = 0;
};

/** A tall light column over the operation's target / extraction point; colour follows the mission stage. */
UCLASS()
class DELTAVDOMINION_API ADVObjectiveBeacon : public AActor
{
	GENERATED_BODY()

public:
	ADVObjectiveBeacon();
	virtual void Tick(float DeltaSeconds) override;

	/** Which stages this beacon shows in (target: Sabotage/Defend; landing zone: Extract). */
	UPROPERTY(Replicated) bool bExtraction = false;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Column;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Ring;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> Light;
};
