#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DVAscent.generated.h"

class ADVGunship;

/**
 * Flying home without a loading screen. Server-only director: a Raven carrying the commander lifts off the
 * battlefield, cruises up to the TAS Kestrel holding station overhead, flies in through the starboard landing
 * bay and settles on the hangar deck, which has been streamed into the hull (ADVGameState::MulticastStreamHangar).
 * The Raven is moved on a scripted path; its replicated movement carries it to clients.
 */
UCLASS()
class DELTAVDOMINION_API ADVAscent : public AActor
{
	GENERATED_BODY()

public:
	ADVAscent();

	virtual void Tick(float DeltaSeconds) override;

	void Begin(ADVGunship* InRaven, APlayerController* InPilot, const FTransform& Hangar);

	/** Hangar-space points (cm, the Kestrel map's own coordinates): outside the mouth, just inside it, the landing spot. */
	static inline const FVector Approach = FVector(-16000.f, 0.f, 900.f);
	static inline const FVector Entry = FVector(-6800.f, 0.f, 900.f);
	static inline const FVector Hover = FVector(-3000.f, 0.f, 700.f);
	static inline const FVector Touchdown = FVector(-3000.f, 0.f, 200.f);

protected:
	void Land();

	TWeakObjectPtr<ADVGunship> Raven;
	TWeakObjectPtr<APlayerController> Pilot;
	FTransform HangarXf;
	FVector Start, Lifted, ApproachW, EntryW, HoverW, TouchW;
	double StartTime = 0.0;
	float CruiseSeconds = 12.f;
	int32 Phase = 0;
	FVector LastPos;
};
