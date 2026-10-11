#include "Strategy/DVGalaxyStarfield.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace DVGalaxy
{
	constexpr float R0 = 4000.f;					// spiral reference radius
	constexpr float PitchCot = 3.487f;				// 1 / tan(16 deg): a touch more open than the real ~12-14 deg so the arms read
	// Phases chosen so Perseus passes outside Sol and Sagittarius inside it, as in the real galaxy.
	constexpr float ArmPhaseDeg[5] = { 121.f, 211.f, 301.f, 31.f, 166.f };	// Perseus, Sagittarius, Norma, Scutum-Centaurus, Orion Spur
	constexpr float ArmWeight[5] = { 0.30f, 0.17f, 0.15f, 0.30f, 0.08f };

	// Saturated colours and modest glow so overlapping stars keep their hue instead of clipping to white.
	const FLinearColor BinColor[7] = {
		FLinearColor(0.22f, 0.45f, 1.f),	// hot young blue stars along the arms
		FLinearColor(0.6f, 0.75f, 1.f),		// blue-white
		FLinearColor(1.f, 0.72f, 0.32f),	// yellow
		FLinearColor(1.f, 0.42f, 0.12f),	// old orange bulge population
		FLinearColor(1.f, 0.18f, 0.45f),	// H-II star-forming knots (pink)
		FLinearColor(0.85f, 0.25f, 0.6f),	// nebular gas haze (magenta)
		FLinearColor(0.15f, 0.55f, 0.8f),	// nebular gas haze (teal)
	};
	const float BinGlow[7] = { 1.0f, 0.6f, 1.1f, 1.2f, 2.2f, 0.16f, 0.14f };
}

float ADVGalaxyStarfield::ArmAngleRad(int32 Arm, float Radius)
{
	return FMath::DegreesToRadians(DVGalaxy::ArmPhaseDeg[Arm]) + FMath::Loge(FMath::Max(Radius, 100.f) / DVGalaxy::R0) * DVGalaxy::PitchCot;
}

ADVGalaxyStarfield::ADVGalaxyStarfield()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	// Unlit additive glow material created by Scripts/build_galaxy_map.py.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GreyBox(TEXT("/Game/Environment/Galaxy/M_Star.M_Star"));

	for (int32 i = 0; i < 7; ++i)
	{
		UInstancedStaticMeshComponent* Bin = CreateDefaultSubobject<UInstancedStaticMeshComponent>(*FString::Printf(TEXT("Stars%d"), i));
		Bin->SetupAttachment(RootComponent);
		Bin->SetStaticMesh(Sphere.Object);
		Bin->SetMaterial(0, GreyBox.Object);
		Bin->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Bin->CastShadow = false;
		Bin->SetCullDistances(0, 0);
		Bins.Add(Bin);
	}

	// The bright bar/bulge glow at the centre (Sagittarius A* sits inside it).
	CoreGlow = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoreGlow"));
	CoreGlow->SetupAttachment(RootComponent);
	CoreGlow->SetStaticMesh(Sphere.Object);
	CoreGlow->SetMaterial(0, GreyBox.Object);
	CoreGlow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CoreGlow->CastShadow = false;
	CoreGlow->SetRelativeRotation(FRotator(0.f, 27.f, 0.f));
	CoreGlow->SetRelativeScale3D(FVector(70.f, 32.f, 14.f));

	CoreHaze = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoreHaze"));
	CoreHaze->SetupAttachment(RootComponent);
	CoreHaze->SetStaticMesh(Sphere.Object);
	CoreHaze->SetMaterial(0, GreyBox.Object);
	CoreHaze->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CoreHaze->CastShadow = false;
	CoreHaze->SetRelativeScale3D(FVector(14.f, 14.f, 9.f));
}

void ADVGalaxyStarfield::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Generate();
}

void ADVGalaxyStarfield::BeginPlay()
{
	Super::BeginPlay();
	Generate();	// instances are rebuilt from the seed rather than stored in the map
}

void ADVGalaxyStarfield::Generate()
{
	FRandomStream Rng(Seed);
	TArray<TArray<FTransform>> Instances;
	Instances.SetNum(7);

	auto Gauss = [&Rng]()
	{
		// Box-Muller
		const float U1 = FMath::Max(Rng.FRand(), 1e-6f);
		const float U2 = Rng.FRand();
		return FMath::Sqrt(-2.f * FMath::Loge(U1)) * FMath::Cos(2.f * PI * U2);
	};
	auto Add = [&](int32 Bin, const FVector& P, float Size)
	{
		Instances[Bin].Add(FTransform(FRotator::ZeroRotator, P, FVector(Size * StarScale)));
	};

	const int32 BulgeCount = NumStars * 22 / 100;
	const int32 ArmCount = NumStars * 58 / 100;
	const int32 FieldCount = NumStars - BulgeCount - ArmCount;

	// Bar + bulge: a rotated triaxial cloud of old, orange-yellow stars.
	const FQuat Bar(FVector::UpVector, FMath::DegreesToRadians(27.f));
	for (int32 i = 0; i < BulgeCount; ++i)
	{
		const bool bBar = Rng.FRand() < 0.55f;
		FVector P(Gauss() * (bBar ? 7000.f : 3200.f), Gauss() * (bBar ? 2200.f : 3200.f), Gauss() * (bBar ? 900.f : 2400.f));
		P = Bar.RotateVector(P);
		Add(Rng.FRand() < 0.55f ? 3 : 2, P, Rng.FRandRange(0.6f, 1.5f));
	}

	// Spiral arms: logarithmic spirals with scatter that widens outward; young blue stars and pink nebulae.
	float WeightSum = 0.f;
	for (float W : DVGalaxy::ArmWeight) { WeightSum += W; }
	for (int32 i = 0; i < ArmCount; ++i)
	{
		float Pick = Rng.FRand() * WeightSum;
		int32 Arm = 0;
		while (Arm < 4 && Pick > DVGalaxy::ArmWeight[Arm]) { Pick -= DVGalaxy::ArmWeight[Arm]; ++Arm; }
		const bool bSpur = Arm == 4;
		const float R = bSpur ? Rng.FRandRange(20500.f, 32000.f) : FMath::Clamp(5500.f + -FMath::Loge(FMath::Max(Rng.FRand(), 1e-4f)) * 12000.f, 5500.f, 52000.f);
		const float Theta = ArmAngleRad(Arm, R) + Gauss() * (bSpur ? 0.035f : 0.07f);
		const float Spread = 500.f + R * 0.035f;
		FVector P(FMath::Cos(Theta) * R, FMath::Sin(Theta) * R, 0.f);
		P += FVector(Gauss() * Spread, Gauss() * Spread, Gauss() * (220.f + R * 0.006f));
		const float Roll = Rng.FRand();
		const int32 Bin = Roll < 0.07f ? 4 : Roll < 0.72f ? 0 : Roll < 0.86f ? 1 : 2;
		Add(Bin, P, Bin == 4 ? Rng.FRandRange(0.9f, 1.8f) : Rng.FRandRange(0.5f, 1.4f));
	}

	// Field stars: an exponential disk filling the gaps between arms.
	for (int32 i = 0; i < FieldCount; ++i)
	{
		const float R = FMath::Clamp(-FMath::Loge(FMath::Max(Rng.FRand(), 1e-4f)) * 11500.f, 2500.f, 55000.f);
		const float Theta = Rng.FRand() * 2.f * PI;
		const FVector P(FMath::Cos(Theta) * R, FMath::Sin(Theta) * R, Gauss() * (350.f + R * 0.008f));
		const float Roll = Rng.FRand();
		Add(Roll < 0.25f ? 1 : Roll < 0.7f ? 2 : 3, P, Rng.FRandRange(0.4f, 1.f));
	}

	// Coloured gas: large, faint additive clouds strung along the arms and around the bulge.
	for (int32 i = 0; i < 1400; ++i)
	{
		const int32 Arm = Rng.RandRange(0, 3);
		const float R = Rng.FRandRange(6000.f, 42000.f);
		const float Theta = ArmAngleRad(Arm, R) + Gauss() * 0.06f;
		const FVector P(FMath::Cos(Theta) * R + Gauss() * 900.f, FMath::Sin(Theta) * R + Gauss() * 900.f, Gauss() * 250.f);
		Instances[Rng.FRand() < 0.55f ? 5 : 6].Add(FTransform(FRotator::ZeroRotator, P, FVector(Rng.FRandRange(10.f, 24.f), Rng.FRandRange(10.f, 24.f), Rng.FRandRange(2.f, 4.f))));
	}

	for (int32 i = 0; i < Bins.Num(); ++i)
	{
		UInstancedStaticMeshComponent* Bin = Bins[i];
		if (!Bin)
		{
			continue;
		}
		Bin->ClearInstances();
		Bin->AddInstances(Instances[i], false, false, false);
		if (UMaterialInstanceDynamic* Mat = Bin->CreateAndSetMaterialInstanceDynamic(0))
		{
			Mat->SetVectorParameterValue(TEXT("Color"), DVGalaxy::BinColor[i]);
			Mat->SetVectorParameterValue(TEXT("Emissive"), DVGalaxy::BinColor[i] * DVGalaxy::BinGlow[i]);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("Galaxy: %d/%d/%d/%d/%d stars, material %s, world %s"), Bins[0]->GetInstanceCount(), Bins[1]->GetInstanceCount(),
		Bins[2]->GetInstanceCount(), Bins[3]->GetInstanceCount(), Bins[4]->GetInstanceCount(), *GetNameSafe(Bins[0]->GetMaterial(0)), *GetNameSafe(GetWorld()));
	if (UMaterialInstanceDynamic* Mat = CoreGlow->CreateAndSetMaterialInstanceDynamic(0))
	{
		Mat->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.7f, 0.35f));
		Mat->SetVectorParameterValue(TEXT("Emissive"), FLinearColor(0.5f, 0.28f, 0.08f));
	}
	if (UMaterialInstanceDynamic* Mat = CoreHaze->CreateAndSetMaterialInstanceDynamic(0))
	{
		Mat->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.8f, 0.5f));
		Mat->SetVectorParameterValue(TEXT("Emissive"), FLinearColor(0.7f, 0.45f, 0.2f));
	}
}
