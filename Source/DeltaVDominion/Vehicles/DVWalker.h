#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DVWalker.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class ADVSoldierCharacter;

UENUM()
enum class EDVWalkerState : uint8
{
	Walking,
	Kneeling,	// lowering its belly toward the ground
	Knelt,		// down, rear ramp out: the way aboard
	Rising,
	Collapsing,
	Down
};

/**
 * The Ironstride: a 30 m Hearthbound walker that strides a loop across the battlefield. Four legs really step
 * (diagonal gait, feet planted on the terrain, two-bone knees). Its back is a deck you can stand on and ride; at two
 * points on its loop it kneels and drops a rear ramp, which is the way aboard. On the deck are three power couplings:
 * stand at one to overload it. When all three blow, the walker collapses. Meanwhile its cannon marks a spot on the
 * ground in red and fires two seconds later at whoever is down there.
 *
 * Server moves the body (replicated movement); every machine animates the legs from the body and the state.
 */
UCLASS()
class DELTAVDOMINION_API ADVWalker : public AActor
{
	GENERATED_BODY()

public:
	ADVWalker();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	static ADVWalker* Find(const UWorld* World);

	EDVWalkerState GetState() const { return State; }
	int32 GetCouplingsBlown() const { return CouplingsBlown; }
	float GetCouplingProgress() const { return CouplingProgress; }
	/** World position of the coupling to work on next (for the objective waypoint), or the walker if none left. */
	FVector GetActiveCouplingLocation() const;
	/** True if Actor is standing on (or inside the outline of) the deck. */
	bool IsAboard(const AActor* Actor) const;

	static constexpr int32 NumCouplings = 3;
	static constexpr float WalkHeight = 2200.f;	// hull bottom above the ground while striding
	static constexpr float KneelHeight = 350.f;
	static constexpr float WalkSpeed = 230.f;
	static constexpr float KneelSeconds = 18.f;

	/** Dev/tests: blow every coupling (collapse) or kneel right now. */
	void DevBlowAll();
	void DevKneel();
	void DevHoldFire() { bHoldFire = true; }

protected:
	void TickServer(float DeltaSeconds);
	void TickCouplings(float DeltaSeconds);
	void TickCannon();
	void TickLegs(float DeltaSeconds);
	float GroundZ(const FVector& At) const;
	void SetRamp(float Alpha);
	bool AnyoneOnRamp() const;
	void BlowCoupling(int32 Index);

	UFUNCTION(NetMulticast, Reliable) void MulticastCouplingBlown(int32 Index);
	UFUNCTION(NetMulticast, Reliable) void MulticastTelegraph(FVector_NetQuantize Target);
	UFUNCTION(NetMulticast, Reliable) void MulticastCannonFire(FVector_NetQuantize Target);
	UFUNCTION(NetMulticast, Reliable) void MulticastCollapse();
	UFUNCTION() void OnRep_State();

	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Body;	// bobs and tilts; the deck and everything on it
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Hull;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> RampHinge;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Ramp;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Muzzle;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Couplings;
	UPROPERTY() TArray<TObjectPtr<UPointLightComponent>> CouplingLights;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Upper;	// thigh per leg
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Lower;	// shin per leg
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Feet;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Knees;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Optic;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> TelegraphRing;
	UPROPERTY() TObjectPtr<UPointLightComponent> OpticLight;

	UPROPERTY(ReplicatedUsing = OnRep_State) EDVWalkerState State = EDVWalkerState::Walking;
	UPROPERTY(Replicated) int32 CouplingsBlown = 0;
	UPROPERTY(Replicated) float CouplingProgress = 0.f;	// on the coupling being worked
	UPROPERTY(Replicated) uint8 BlownMask = 0;

	// Server
	TArray<FVector> Waypoints;	// world, around the spawn point
	int32 NextWaypoint = 0;
	double StateTime = 0.0;
	float Height = WalkHeight;
	float Heading = 0.f;
	double NextShot = 8.0;
	FVector PendingTarget = FVector::ZeroVector;
	double PendingImpact = -1.0;
	TWeakObjectPtr<ADVSoldierCharacter> Gunner;
	bool bHoldFire = false;

	// Every machine: gait
	TArray<FVector> FootPlanted;	// world
	TArray<FVector> FootFrom;
	TArray<FVector> FootTo;
	TArray<float> FootSwing;	// 0..1 while stepping, -1 planted
	double GaitClock = 0.0;
	int32 GaitPair = 0;
	double CollapseStart = -1.0;
	double TelegraphUntil = -1.0;
};
