#pragma once

#include "CoreMinimal.h"

class APawn;
class UWorld;

/**
 * Lightweight runtime cover search (no pre-placed cover actors needed).
 * Samples rings of navmesh points around an anchor and keeps the ones where solid
 * geometry close to the point blocks the threat's line of fire.
 * EQS can replace this later; the scoring rules are the same.
 */
namespace DVCover
{
	struct FResult
	{
		FVector Location = FVector::ZeroVector;
		/** True when the cover only protects while crouched (can pop up to fire over it). */
		bool bLowCover = false;
	};

	/** Is a point covered from a threat? Fills bLowCover when it is. */
	bool IsCovered(UWorld* World, const FVector& FloorPoint, const FVector& ThreatEyes, bool& bOutLowCover);

	bool FindCover(UWorld* World, const APawn* Querier, const FVector& Anchor, float Radius,
		const FVector& ThreatEyes, const TArray<FVector>& ClaimedPoints, FResult& OutResult);
}
