#include "Core/DVWorldGen.h"
#include "Core/DVPointOfInterest.h"
#include "Core/DVInteractable.h"
#include "Characters/DVSoldierCharacter.h"
#include "Characters/DVTownsfolk.h"
#include "Vehicles/DVVehicleKit.h"
#include "DeltaVDominion.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"

namespace DVWorldGenSteps	// named: unity builds merge anonymous namespaces
{
	/** 2. Bridson's Poisson-disk sampling over the arena oval. */
	TArray<FVector2D> PoissonDisk(FRandomStream& Rng, const DVWorldGen::FSettings& S)
	{
		const FVector2D R = S.Arena * S.ArenaFill;
		const float Radius = S.Spacing;
		const float Cell = Radius / UE_SQRT_2;
		TMap<FIntPoint, FVector2D> Grid;
		TArray<FVector2D> Points, Active;
		auto Inside = [&](const FVector2D& P) { return FMath::Square(P.X / R.X) + FMath::Square(P.Y / R.Y) <= 1.f; };
		auto CellOf = [&](const FVector2D& P) { return FIntPoint(FMath::FloorToInt(P.X / Cell), FMath::FloorToInt(P.Y / Cell)); };
		auto Fits = [&](const FVector2D& P)
		{
			const FIntPoint C = CellOf(P);
			for (int32 X = C.X - 2; X <= C.X + 2; ++X)
			{
				for (int32 Y = C.Y - 2; Y <= C.Y + 2; ++Y)
				{
					if (const FVector2D* Q = Grid.Find(FIntPoint(X, Y)); Q && FVector2D::DistSquared(*Q, P) < Radius * Radius)
					{
						return false;
					}
				}
			}
			return true;
		};
		const FVector2D First(Rng.FRandRange(-R.X * 0.5f, R.X * 0.5f), Rng.FRandRange(-R.Y * 0.5f, R.Y * 0.5f));
		Points.Add(First);
		Active.Add(First);
		Grid.Add(CellOf(First), First);
		while (Active.Num() > 0)
		{
			const int32 Pick = Rng.RandRange(0, Active.Num() - 1);
			const FVector2D Base = Active[Pick];
			bool bFound = false;
			for (int32 Try = 0; Try < 30 && !bFound; ++Try)
			{
				const float A = Rng.FRandRange(0.f, 2.f * PI);
				const float D = Rng.FRandRange(Radius, 2.f * Radius);
				const FVector2D P = Base + FVector2D(FMath::Cos(A), FMath::Sin(A)) * D;
				if (Inside(P) && Fits(P))
				{
					Points.Add(P);
					Active.Add(P);
					Grid.Add(CellOf(P), P);
					bFound = true;
				}
			}
			if (!bFound)
			{
				Active.RemoveAtSwap(Pick);
			}
		}
		return Points;
	}
}

int32 DVWorldGen::PlacePointsOfInterest(UWorld* World, const FString& MapKey, const FSettings& S)
{
	if (!World)
	{
		return 0;
	}
	for (TActorIterator<ADVPointOfInterest> It(World); It; ++It)
	{
		return 0;	// a map that already has hand-placed points keeps them
	}
	UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(World);
	FVector Start = FVector(-4300.f, 0.f, 100.f);
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		Start = It->GetActorLocation();
		break;
	}
	FNavLocation StartNav;
	if (!Nav || !Nav->ProjectPointToNavigation(Start, StartNav, FVector(400.f, 400.f, 600.f)))
	{
		return 0;	// the navmesh isn't built yet: try again shortly
	}

	// 1. Settings: what has to stay clear.
	TArray<FVector> Near, Far;
	Far.Add(Start);
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (Cast<ADVSoldierCharacter>(*It) || Cast<ADVTownsfolk>(*It) || Cast<ADVInteractable>(*It) || It->GetClass()->GetName().StartsWith(TEXT("DVPressure"))
			|| It->GetClass()->GetName().StartsWith(TEXT("DVRepair")) || It->GetClass()->GetName().StartsWith(TEXT("DVPen")) || It->GetClass()->GetName().StartsWith(TEXT("DVCourier"))
			|| It->GetClass()->GetName().StartsWith(TEXT("DVWalker")) || It->ActorHasTag(TEXT("DV_Obj_1")) || It->ActorHasTag(TEXT("DV_Obj_2")))
		{
			Near.Add(It->GetActorLocation());
		}
		else if (UDVSeatComponent::FindOn(*It))
		{
			Far.Add(It->GetActorLocation());	// vehicles: the landing zone and the motor pool
		}
	}

	// 2. Generator: Poisson-disk samples, kept only where a soldier could actually walk to them. A cramped map (a
	// harbour, a dense village) gets up to two tighter passes if the first doesn't find room for everything.
	FRandomStream Rng(GetTypeHash(MapKey));
	TArray<FVector> Valid;
	const int32 Wanted = S.Intel + S.Survivors + S.Caches + S.Wrecks;
	FSettings Pass = S;
	for (int32 Attempt = 0; Attempt < 3 && Valid.Num() < Wanted; ++Attempt)
	{
		Valid.Reset();
		for (const FVector2D& P : DVWorldGenSteps::PoissonDisk(Rng, Pass))
		{
			FHitResult Hit;
			FCollisionQueryParams Params(TEXT("DVWorldGen"), true);
			if (!World->LineTraceSingleByChannel(Hit, FVector(P, 40000.f), FVector(P, -40000.f), ECC_Visibility, Params) || Hit.ImpactNormal.Z < S.MinNormalZ)
			{
				continue;
			}
			const FVector G = Hit.ImpactPoint;
			if (Near.ContainsByPredicate([&](const FVector& Q) { return FVector::DistSquared2D(Q, G) < FMath::Square(S.KeepClear); })
				|| Far.ContainsByPredicate([&](const FVector& Q) { return FVector::DistSquared2D(Q, G) < FMath::Square(Pass.KeepClearStart); }))
			{
				continue;
			}
			FNavLocation OnNav;
			if (!Nav->ProjectPointToNavigation(G, OnNav, FVector(80.f, 80.f, 120.f)) || FMath::Abs(OnNav.Location.Z - G.Z) > 100.f)
			{
				continue;	// a roof, a rock top, water: somewhere the navmesh doesn't reach
			}
			const UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, StartNav.Location, OnNav.Location);
			if (!Path || !Path->IsValid() || Path->IsPartial())
			{
				continue;	// walkable, but not from where you land
			}
			Valid.Add(G);
		}
		Pass.Spacing *= 0.72f;
		Pass.KeepClearStart *= 0.8f;
	}

	// 3. World data.
	TArray<TPair<EDVPointKind, FVector>> Chosen;
	auto Dist = [](const FVector& A, const FVector& B) { return FVector::Dist2D(A, B); };
	TArray<FVector> Intel;
	for (int32 i = 0; i < S.Intel && Valid.Num() > 0; ++i)
	{
		// Farthest-point selection: each cache as far as possible from the start and from the others.
		int32 Best = 0;
		float BestScore = -1.f;
		for (int32 k = 0; k < Valid.Num(); ++k)
		{
			float Score = Dist(Valid[k], Start);
			for (const FVector& Q : Intel)
			{
				Score = FMath::Min(Score, Dist(Valid[k], Q));
			}
			if (Score > BestScore)
			{
				BestScore = Score;
				Best = k;
			}
		}
		Intel.Add(Valid[Best]);
		Chosen.Emplace(EDVPointKind::Intel, Valid[Best]);
		Valid.RemoveAtSwap(Best);
	}
	for (int32 i = 0; i < S.Survivors && Valid.Num() > 0; ++i)
	{
		int32 Best = 0;
		for (int32 k = 1; k < Valid.Num(); ++k)
		{
			if (Dist(Valid[k], Start) > Dist(Valid[Best], Start))
			{
				Best = k;
			}
		}
		Chosen.Emplace(EDVPointKind::Survivor, Valid[Best]);
		Valid.RemoveAtSwap(Best);
	}
	Valid.Sort([&](const FVector& A, const FVector& B) { return FMath::Abs(Dist(A, Start) - 4500.f) < FMath::Abs(Dist(B, Start) - 4500.f); });
	for (int32 i = 0; i < S.Caches && Valid.Num() > 0; ++i)
	{
		Chosen.Emplace(EDVPointKind::Cache, Valid[0]);
		Valid.RemoveAt(0);
	}
	for (int32 i = 0; i < S.Wrecks && Valid.Num() > 0; ++i)
	{
		const int32 Pick = Rng.RandRange(0, Valid.Num() - 1);
		Chosen.Emplace(EDVPointKind::Wreck, Valid[Pick]);
		Valid.RemoveAtSwap(Pick);
	}

	// 4. Renderer.
	for (const TPair<EDVPointKind, FVector>& Entry : Chosen)
	{
		const FTransform Xf(FRotator(0.f, Rng.FRandRange(0.f, 360.f), 0.f), Entry.Value);
		if (ADVPointOfInterest* Point = World->SpawnActorDeferred<ADVPointOfInterest>(ADVPointOfInterest::StaticClass(), Xf))
		{
			Point->Kind = Entry.Key;
			Point->FinishSpawning(Xf);
		}
	}
	UE_LOG(LogDV, Log, TEXT("DVWorldGen: %s placed %d points of interest"), *MapKey, Chosen.Num());
	return Chosen.Num();
}
