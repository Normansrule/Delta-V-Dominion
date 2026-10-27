#include "Core/DVScatter.h"
#include "DeltaVDominion.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

ADVScatter::ADVScatter()
{
	PrimaryActorTick.bCanEverTick = false;
	Instances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Instances"));
	SetRootComponent(Instances);
	Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Instances->SetCanEverAffectNavigation(false);
	Instances->SetMobility(EComponentMobility::Movable);	// instances are added at runtime
}

void ADVScatter::BeginPlay()
{
	Super::BeginPlay();
	if (!Mesh || Count <= 0)
	{
		return;
	}
	Instances->SetStaticMesh(Mesh);
	if (Material)
	{
		for (int32 i = 0; i < Mesh->GetStaticMaterials().Num(); ++i)
		{
			Instances->SetMaterial(i, Material);
		}
	}
	Instances->SetCastShadow(bCastShadow);
	Instances->SetCullDistances(0, bCastShadow ? 18000 : 9000);

	FRandomStream Rng(Seed);
	const FVector Centre = GetActorLocation();
	const float MeshHeight = Mesh->GetBounds().BoxExtent.Z * 2.f;
	// Clusters: a set of centres that pieces gather around, so the ground reads as scree and gravel patches, not salt.
	TArray<FVector2D> Clusters;
	for (int32 i = 0; i < FMath::Max(8, Count / 60); ++i)
	{
		const float A = Rng.FRandRange(0.f, 2.f * PI), Q = FMath::Sqrt(Rng.FRand());
		Clusters.Add(FVector2D(FMath::Cos(A) * Extent.X * Q, FMath::Sin(A) * Extent.Y * Q));
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DVScatter), false, this);
	TArray<FTransform> Placed;
	Placed.Reserve(Count);
	for (int32 i = 0; i < Count; ++i)
	{
		FVector2D P;
		if (Rng.FRand() < Clump)
		{
			const FVector2D& C = Clusters[Rng.RandRange(0, Clusters.Num() - 1)];
			const float R = 700.f * FMath::Square(Rng.FRand());
			const float A = Rng.FRandRange(0.f, 2.f * PI);
			P = C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R;
		}
		else
		{
			const float A = Rng.FRandRange(0.f, 2.f * PI), Q = FMath::Sqrt(Rng.FRand());
			P = FVector2D(FMath::Cos(A) * Extent.X * Q, FMath::Sin(A) * Extent.Y * Q);
		}
		const FVector Top = Centre + FVector(P, 40000.f);
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByChannel(Hit, Top, Top - FVector(0.f, 0.f, 80000.f), ECC_Visibility, Params) || Hit.ImpactNormal.Z < MinNormalZ)
		{
			continue;
		}
		const UStaticMeshComponent* Ground = Cast<UStaticMeshComponent>(Hit.GetComponent());
		if (!Ground || !Ground->GetStaticMesh() || !Ground->GetStaticMesh()->GetName().Contains(TEXT("Terrain")))
		{
			continue;	// only the ground itself: no gravel on roofs, crates or rocks
		}
		const float Scale = FMath::Lerp(MinScale, MaxScale, FMath::Pow(Rng.FRand(), SizeBias));
		const FVector S(Scale * Rng.FRandRange(0.8f, 1.25f), Scale * Rng.FRandRange(0.8f, 1.25f), Scale * Flatten * Rng.FRandRange(0.7f, 1.2f));
		// Sit on the slope: up follows the ground normal, then a random spin about it.
		const FQuat Align = FQuat::FindBetweenNormals(FVector::UpVector, Hit.ImpactNormal);
		const FQuat Spin(FVector::UpVector, Rng.FRandRange(0.f, 2.f * PI));
		const FVector Loc = Hit.ImpactPoint - Hit.ImpactNormal * MeshHeight * S.Z * Sink;
		Placed.Add(FTransform(Align * Spin, Loc - Centre, S));
	}
	Instances->AddInstances(Placed, false, false);
	UE_LOG(LogDV, Log, TEXT("DVScatter: %s placed %d of %d"), *GetName(), Placed.Num(), Count);
}
