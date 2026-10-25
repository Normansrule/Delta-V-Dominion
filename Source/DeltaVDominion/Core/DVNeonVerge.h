#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "DVNeonVerge.generated.h"

/**
 * Neon Verge: the broker's courier carrying the clean sample of the Static. When the chase starts he runs a fixed route
 * through the district (TargetPoints tagged DV_Route_00, DV_Route_01, ...) toward the maglev. Catch him by getting
 * within CatchRange; reach the end of the route first and he's gone. Shots pass him by: the sample can't be risked.
 */
UCLASS()
class DELTAVDOMINION_API ADVCourier : public ACharacter
{
	GENERATED_BODY()

public:
	ADVCourier();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	static constexpr float CatchRange = 240.f;
	/** Slower than a sprinting soldier (680), faster than a walking one (420). */
	static constexpr float RunSpeed = 590.f;

	static ADVCourier* Find(const UWorld* World);
	/** Server: he bolts. */
	void StartRunning();
	/** Server: someone caught him. He stops, hands up. */
	void Catch();
	bool IsRunning() const { return bRunning; }
	bool IsCaught() const { return bCaught; }
	bool HasEscaped() const { return bEscaped; }
	/** 0..1 along the route. */
	float GetRouteProgress() const { return Route.Num() > 1 ? float(Next) / (Route.Num() - 1) : 0.f; }

protected:
	UFUNCTION() void OnRep_Caught();
	UPROPERTY(Replicated) bool bRunning = false;
	UPROPERTY(ReplicatedUsing = OnRep_Caught) bool bCaught = false;
	UPROPERTY(Replicated) bool bEscaped = false;
	TArray<FVector> Route;
	int32 Next = 0;
	double LastProgress = 0.0;
	FVector LastSpot = FVector::ZeroVector;
};
