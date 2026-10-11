#include "Core/DVInteractable.h"
#include "Vehicles/DVVehicleKit.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"

ADVInteractable::ADVInteractable()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Hologram = CreateDefaultSubobject<USceneComponent>(TEXT("Hologram"));
	Hologram->SetupAttachment(Root);
	Hologram->SetRelativeLocation(FVector(0.f, 0.f, 160.f));

	UMaterialInterface* Metal = DVKit::Material(TEXT("Gunmetal"));
	UMaterialInterface* Glow = DVKit::Material(TEXT("Glow"));

	// One kit covers all three: a console base with a glowing top; the war table adds a spinning star map.
	DVKit::Part(this, Root, TEXT("Base"), DVKit::Cylinder(), Metal, FVector(0.f, 0.f, 45.f), FRotator::ZeroRotator, FVector(260.f, 260.f, 90.f));
	DVKit::Part(this, Root, TEXT("Rim"), DVKit::Cylinder(), Metal, FVector(0.f, 0.f, 95.f), FRotator::ZeroRotator, FVector(300.f, 300.f, 12.f));
	Glows.Add(DVKit::Part(this, Root, TEXT("Surface"), DVKit::Cylinder(), Glow, FVector(0.f, 0.f, 103.f), FRotator::ZeroRotator, FVector(270.f, 270.f, 4.f)));
	Glows.Add(DVKit::Part(this, Hologram, TEXT("HoloDisc"), DVKit::Cylinder(), Glow, FVector::ZeroVector, FRotator::ZeroRotator, FVector(200.f, 200.f, 2.f)));
	Glows.Add(DVKit::Part(this, Hologram, TEXT("HoloCore"), DVKit::Sphere(), Glow, FVector(0.f, 0.f, 6.f), FRotator::ZeroRotator, FVector(26.f, 26.f, 16.f)));
	for (int32 i = 0; i < 4; ++i)
	{
		const float A = i * PI * 0.5f + 0.6f;
		Glows.Add(DVKit::Part(this, Hologram, *FString::Printf(TEXT("HoloArm%d"), i), DVKit::Box(), Glow,
			FVector(FMath::Cos(A) * 45.f, FMath::Sin(A) * 45.f, 2.f), FRotator(0.f, FMath::RadiansToDegrees(A) + 60.f, 0.f), FVector(90.f, 10.f, 3.f)));
	}

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Root);
	Light->SetRelativeLocation(FVector(0.f, 0.f, 200.f));
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(300.f);
	Light->SetAttenuationRadius(700.f);
	Light->SetCastShadows(false);
}

void ADVInteractable::BeginPlay()
{
	Super::BeginPlay();
	FLinearColor Color(0.2f, 0.7f, 1.f);
	switch (Interaction)
	{
	case EDVInteraction::Memorial: Color = FLinearColor(1.f, 0.7f, 0.35f); break;
	case EDVInteraction::Armoury:  Color = FLinearColor(0.4f, 1.f, 0.5f); break;
	default: break;
	}
	for (UStaticMeshComponent* G : Glows)
	{
		DVKit::SetGlow(G, Color, 8.f);
	}
	Light->SetLightColor(Color);
	Hologram->SetVisibility(Interaction == EDVInteraction::WarTable, true);
}

void ADVInteractable::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Hologram->AddRelativeRotation(FRotator(0.f, 12.f * DeltaSeconds, 0.f));
}

ADVInteractable* ADVInteractable::FindNear(const AActor* User)
{
	if (!User)
	{
		return nullptr;
	}
	for (TActorIterator<ADVInteractable> It(User->GetWorld()); It; ++It)
	{
		if (FVector::Dist2D(It->GetActorLocation(), User->GetActorLocation()) <= It->Reach)
		{
			return *It;
		}
	}
	return nullptr;
}
