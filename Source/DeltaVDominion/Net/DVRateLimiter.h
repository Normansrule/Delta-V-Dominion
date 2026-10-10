#pragma once

#include "CoreMinimal.h"

/**
 * Token bucket used to rate-limit server RPCs per connection/pawn.
 * Consume() returns false when a client is sending faster than allowed,
 * which callers treat as a dropped (and logged) request.
 */
struct FDVRateLimiter
{
	FDVRateLimiter() = default;
	FDVRateLimiter(float InRatePerSecond, float InBurst)
		: RatePerSecond(InRatePerSecond), Burst(InBurst), Tokens(InBurst) {}

	bool Consume(double Now, float Cost = 1.f)
	{
		const double Elapsed = FMath::Max(0.0, Now - LastRefill);
		LastRefill = Now;
		Tokens = FMath::Min<float>(Burst, Tokens + static_cast<float>(Elapsed) * RatePerSecond);
		if (Tokens < Cost)
		{
			return false;
		}
		Tokens -= Cost;
		return true;
	}

	float RatePerSecond = 10.f;
	float Burst = 10.f;

private:
	float Tokens = 10.f;
	double LastRefill = 0.0;
};
