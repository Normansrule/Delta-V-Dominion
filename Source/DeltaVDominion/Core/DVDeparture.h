#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DVDeparture.generated.h"

class ADVGunship;

/**
 * Leaving the Kestrel without a loading screen: the reverse of ADVAscent. Server-only director, used in the hangar hub.
 * The commander's Raven lifts off its pad, turns for the open hangar mouth, slides out over the launch lip, then lights
 * its engines and burns away from the carrier toward Mars. As the burn peaks the screen washes white (the Lattice jump),
 * and the galaxy map opens already pulling back from Sol (ADVGalaxyPlayerController reads the "launch" option).
 */
UCLASS()
class DELTAVDOMINION_API ADVDeparture : public AActor
{
	GENERATED_BODY()

public:
	ADVDeparture();

	virtual void Tick(float DeltaSeconds) override;

	void Begin(ADVGunship* InRaven, APlayerController* InPilot);

	/** Hangar coordinates (the Kestrel map, cm): just inside the mouth, just outside it. The mouth is at -X. */
	static inline const FVector InsideMouth = FVector(-4600.f, 0.f, 900.f);
	static inline const FVector OutsideMouth = FVector(-8200.f, 0.f, 900.f);
	/** How long the white-out lasts before the galaxy map loads. */
	static constexpr float JumpSeconds = 1.3f;

protected:
	void Jump();

	TWeakObjectPtr<ADVGunship> Raven;
	TWeakObjectPtr<APlayerController> Pilot;
	FVector Start, Lifted, Burn;
	double StartTime = 0.0;
	int32 Phase = 0;
};
