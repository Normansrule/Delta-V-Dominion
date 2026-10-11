#include "Characters/DVCoverQuery.h"
#include "NavigationSystem.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"

namespace DVCover
{
	namespace
	{
		constexpr float CrouchedEyeHeight = 70.f;	// above the floor
		constexpr float StandingEyeHeight = 150.f;
		constexpr float MaxCoverDistance = 250.f;	// blocker must be this close to the point to count as cover
		constexpr float MinThreatDistance = 500.f;
		constexpr float ClaimRadius = 120.f;

		bool BlockedNearPoint(UWorld* World, const FVector& From, const FVector& To)
		{
			FCollisionObjectQueryParams Objects;
			Objects.AddObjectTypesToQuery(ECC_WorldStatic);
			Objects.AddObjectTypesToQuery(ECC_WorldDynamic);

			FHitResult Hit;
			if (!World->LineTraceSingleByObjectType(Hit, From, To, Objects, FCollisionQueryParams(SCENE_QUERY_STAT(DVCover), false)))
			{
				return false;
			}
			return FVector::Dist(Hit.ImpactPoint, To) <= MaxCoverDistance;
		}
	}

	bool IsCovered(UWorld* World, const FVector& FloorPoint, const FVector& ThreatEyes, bool& bOutLowCover)
	{
		const bool bCrouchCovered = BlockedNearPoint(World, ThreatEyes, FloorPoint + FVector(0, 0, CrouchedEyeHeight));
		if (!bCrouchCovered)
		{
			return false;
		}
		const bool bStandCovered = BlockedNearPoint(World, ThreatEyes, FloorPoint + FVector(0, 0, StandingEyeHeight));
		bOutLowCover = !bStandCovered;
		return true;
	}

	bool FindCover(UWorld* World, const APawn* Querier, const FVector& Anchor, float Radius,
		const FVector& ThreatEyes, const TArray<FVector>& ClaimedPoints, FResult& OutResult)
	{
		UNavigationSystemV1* NavSys = World ? UNavigationSystemV1::GetCurrent(World) : nullptr;
		if (!NavSys || !Querier)
		{
			return false;
		}

		const FVector QuerierLoc = Querier->GetActorLocation();
		float BestScore = TNumericLimits<float>::Max();
		bool bFound = false;

		constexpr int32 Directions = 12;
		for (float Ring = 150.f; Ring <= Radius; Ring += 200.f)
		{
			for (int32 i = 0; i < Directions; ++i)
			{
				const float Angle = (2.f * PI * i) / Directions + Ring * 0.01f;	// stagger rings
				const FVector Candidate = Anchor + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Ring;

				FNavLocation NavLoc;
				if (!NavSys->ProjectPointToNavigation(Candidate, NavLoc, FVector(80.f, 80.f, 250.f)))
				{
					continue;
				}
				const FVector Floor = NavLoc.Location;

				if (FVector::Dist2D(Floor, ThreatEyes) < MinThreatDistance)
				{
					continue;
				}
				bool bClaimed = false;
				for (const FVector& Claimed : ClaimedPoints)
				{
					if (FVector::Dist2D(Claimed, Floor) < ClaimRadius)
					{
						bClaimed = true;
						break;
					}
				}
				if (bClaimed)
				{
					continue;
				}

				bool bLow = false;
				if (!IsCovered(World, Floor, ThreatEyes, bLow))
				{
					continue;
				}

				// Cheap to reach, close to where we were told to be, and able to shoot back.
				float Score = FVector::Dist(QuerierLoc, Floor) + 0.5f * FVector::Dist(Anchor, Floor);
				if (bLow)
				{
					Score -= 300.f;
				}
				if (Score < BestScore)
				{
					BestScore = Score;
					OutResult.Location = Floor;
					OutResult.bLowCover = bLow;
					bFound = true;
				}
			}
		}
		return bFound;
	}
}
