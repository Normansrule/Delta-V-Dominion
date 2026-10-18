#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DVKestrel.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;

/**
 * TAS Kestrel, the Accord strike carrier, holding station over the battlefield. 1.4 km long.
 * It is part of the fight, not a backdrop: orbital strikes are fired from its main battery (beam from the
 * ship to the target), its point-defence guns throw flak at the sky, and its running lights and engines live.
 */
UCLASS()
class DELTAVDOMINION_API ADVKestrel : public AActor
{
	GENERATED_BODY()

public:
	ADVKestrel();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** World position of the main battery muzzle (where orbital strikes come from). */
	FVector GetBatteryLocation() const;

	/** Cosmetic broadside from the main battery at Target (called on all machines by the game state). */
	void FireMainBattery(const FVector& Target);

	static ADVKestrel* Find(const UWorld* World);

	/**
	 * Where the hangar deck (the /Game/Maps/Kestrel level) sits inside this hull: its open mouth is cut into the
	 * starboard side, facing the battlefield. Uses the ship's resting position (see HoldStation), so every machine
	 * computes the same transform.
	 */
	FTransform GetHangarTransform() const;
	/** Stop the slow heave so a streamed-in hangar stays lined up with the hull. */
	void HoldStation();

	/** Hangar placement in hull space (cm): deck centre and yaw of the level's +X (deeper into the ship). */
	static constexpr float HangarX = -40000.f;
	static constexpr float HangarY = 5000.f;
	static constexpr float HangarZ = -6000.f;
	static constexpr float HangarYaw = -90.f;

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Hull;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Battery;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> EngineGlows;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> RunningLights;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> DeckLights;
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> FlakGuns;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> MouthGlows;
	bool bHolding = false;

private:
	FVector BaseLocation = FVector::ZeroVector;
	float NextFlak = 3.f;
};
