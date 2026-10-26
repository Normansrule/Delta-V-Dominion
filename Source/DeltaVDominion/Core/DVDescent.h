#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DVDescent.generated.h"

class ADVGunship;

/**
 * Arriving at an operation without a loading screen: launched from the galaxy map with the "descent" option, the map
 * opens out of the Lattice jump's white-out with the commander already aboard the LZ's Raven, high and far out. The
 * Raven comes in on a long descent while the briefing plays on the radio, flares over the landing pad and sets down;
 * the squad climbs out and the operation begins. Server-only director, like ADVAscent and ADVDeparture.
 */
UCLASS()
class DELTAVDOMINION_API ADVDescent : public AActor
{
	GENERATED_BODY()

public:
	ADVDescent();

	virtual void Tick(float DeltaSeconds) override;

	/** Pad: where the Raven was parked in the map (where it lands). It is moved to the start of the approach. */
	void Begin(ADVGunship* InRaven, APlayerController* InPilot, const FTransform& Pad);

	/** Where the approach starts, relative to the pad: high overhead and a little off, so it drops steeply out of the sky
	 *  and never crosses a canyon wall or the enemy's guns on the way in. */
	static inline const FVector StartOffset = FVector(-3500.f, -2000.f, 9000.f);

protected:
	TWeakObjectPtr<ADVGunship> Raven;
	TWeakObjectPtr<APlayerController> Pilot;
	FVector Start, Flare, Hover, Touch;
	float PadYaw = 0.f;
	double StartTime = 0.0;
	int32 Phase = 0;
};
