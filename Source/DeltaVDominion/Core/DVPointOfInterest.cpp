#include "Core/DVPointOfInterest.h"
#include "Core/DVGameMode.h"
#include "Core/DVFx.h"
#include "Characters/DVSoldierCharacter.h"
#include "Combat/DVAttributeSet.h"
#include "Combat/DVWeaponBase.h"
#include "Vehicles/DVVehicleKit.h"

#include "AbilitySystemComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

ADVPointOfInterest::ADVPointOfInterest()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

void ADVPointOfInterest::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADVPointOfInterest, bTaken);
	DOREPLIFETIME(ADVPointOfInterest, Progress);
}

FString ADVPointOfInterest::Label() const
{
	switch (Kind)
	{
	case EDVPointKind::Intel: return TEXT("INTEL");
	case EDVPointKind::Survivor: return TEXT("SURVIVOR");
	case EDVPointKind::Cache: return TEXT("SUPPLY CACHE");
	default: return TEXT("WRECK");
	}
}

UStaticMeshComponent* ADVPointOfInterest::Part(UStaticMesh* Mesh, const FVector& Loc, const FVector& Size, const FLinearColor& Color, float Glow, const FRotator& Rot)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetupAttachment(Root);
	C->SetStaticMesh(Mesh);
	C->SetMaterial(0, DVKit::Material(TEXT("Glow")));
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetRelativeLocationAndRotation(Loc, Rot);
	if (Mesh)
	{
		const FVector Extent = Mesh->GetBounds().BoxExtent * 2.f;
		C->SetRelativeScale3D(FVector(Size.X / FMath::Max(Extent.X, 1.f), Size.Y / FMath::Max(Extent.Y, 1.f), Size.Z / FMath::Max(Extent.Z, 1.f)));
	}
	C->RegisterComponent();
	DVKit::SetGlow(C, Color, Glow);
	Parts.Add(C);
	return C;
}

void ADVPointOfInterest::BeginPlay()
{
	Super::BeginPlay();
	const FLinearColor Steel(0.05f, 0.055f, 0.06f), Olive(0.12f, 0.15f, 0.08f), Orange(0.85f, 0.3f, 0.05f);
	FLinearColor BeaconColor;
	switch (Kind)
	{
	case EDVPointKind::Intel:
		// A hard case left open on a rock, a data slate glowing inside.
		Part(DVKit::Box(), FVector(0.f, 0.f, 18.f), FVector(70.f, 50.f, 36.f), Steel, 0.05f, FRotator(0.f, 20.f, 0.f));
		Part(DVKit::Box(), FVector(-14.f, 0.f, 48.f), FVector(8.f, 50.f, 40.f), Steel, 0.05f, FRotator(-35.f, 20.f, 0.f));
		Part(DVKit::Box(), FVector(4.f, 0.f, 38.f), FVector(40.f, 30.f, 3.f), FLinearColor(0.2f, 0.8f, 1.f), 12.f, FRotator(0.f, 20.f, 0.f));
		BeaconColor = FLinearColor(0.2f, 0.8f, 1.f);
		break;
	case EDVPointKind::Survivor:
		// A crashed escape pod, nose in the ground, hatch lights blinking for help.
		Part(DVKit::Cylinder(), FVector(0.f, 0.f, 90.f), FVector(160.f, 160.f, 300.f), FLinearColor(0.75f, 0.72f, 0.66f), 0.15f, FRotator(62.f, 30.f, 0.f));
		Part(DVKit::Cylinder(), FVector(60.f, 30.f, 150.f), FVector(170.f, 170.f, 30.f), Orange, 0.6f, FRotator(62.f, 30.f, 0.f));
		Part(DVKit::Box(), FVector(-90.f, -40.f, 10.f), FVector(240.f, 160.f, 20.f), FLinearColor(0.08f, 0.06f, 0.05f), 0.f, FRotator(0.f, 30.f, 6.f));
		BeaconColor = FLinearColor(1.f, 0.35f, 0.05f);
		break;
	case EDVPointKind::Cache:
		// A stack of Accord crates with a white cross.
		Part(DVKit::Box(), FVector(0.f, 0.f, 40.f), FVector(110.f, 80.f, 80.f), Olive, 0.05f);
		Part(DVKit::Box(), FVector(10.f, 5.f, 110.f), FVector(80.f, 70.f, 60.f), Olive, 0.05f, FRotator(0.f, 12.f, 0.f));
		Part(DVKit::Box(), FVector(-56.f, 0.f, 40.f), FVector(2.f, 40.f, 12.f), FLinearColor(1.f, 1.f, 1.f), 2.f);
		Part(DVKit::Box(), FVector(-56.f, 0.f, 40.f), FVector(2.f, 12.f, 40.f), FLinearColor(1.f, 1.f, 1.f), 2.f);
		BeaconColor = FLinearColor(0.3f, 1.f, 0.4f);
		break;
	default:
		// A downed scout craft: broken fuselage, a snapped wing, scorch.
		Part(DVKit::Box(), FVector(0.f, 0.f, 60.f), FVector(420.f, 140.f, 110.f), Steel, 0.05f, FRotator(8.f, 15.f, -12.f));
		Part(DVKit::Box(), FVector(-140.f, 220.f, 30.f), FVector(260.f, 30.f, 160.f), Steel, 0.05f, FRotator(20.f, 70.f, 60.f));
		Part(DVKit::Box(), FVector(160.f, -60.f, 20.f), FVector(120.f, 90.f, 50.f), Steel, 0.05f, FRotator(-15.f, 40.f, 10.f));
		Part(DVKit::Cylinder(), FVector(0.f, 0.f, 2.f), FVector(700.f, 520.f, 4.f), FLinearColor(0.02f, 0.02f, 0.02f), 0.f);
		BeaconColor = FLinearColor(1.f, 0.6f, 0.1f);
		break;
	}
	Beacon = Part(DVKit::Sphere(), FVector(0.f, 0.f, Kind == EDVPointKind::Survivor ? 260.f : 150.f), FVector(16.f, 16.f, 16.f), BeaconColor, 25.f);
	Light = NewObject<UPointLightComponent>(this);
	Light->SetupAttachment(Root);
	Light->SetRelativeLocation(FVector(0.f, 0.f, 160.f));
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(140.f);
	Light->SetAttenuationRadius(600.f);
	Light->SetLightColor(BeaconColor);
	Light->SetCastShadows(false);
	Light->RegisterComponent();
	OnRep_Taken();
}

void ADVPointOfInterest::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Beacon && !bTaken)
	{
		// A slow pulse you can spot from a distance at night.
		const float P = 0.5f + 0.5f * FMath::Sin(GetWorld()->GetTimeSeconds() * 3.f + GetActorLocation().X * 0.01f);
		Light->SetIntensity(60.f + 160.f * P);
	}
	if (!HasAuthority() || bTaken)
	{
		return;
	}
	ADVSoldierCharacter* Near = nullptr;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		ADVSoldierCharacter* S = It->Get() ? Cast<ADVSoldierCharacter>(It->Get()->GetPawn()) : nullptr;
		if (S && !S->IsOutOfAction() && FVector::Dist2D(S->GetActorLocation(), GetActorLocation()) < Reach
			&& FMath::Abs(S->GetActorLocation().Z - GetActorLocation().Z) < 300.f)
		{
			Near = S;
			break;
		}
	}
	if (Kind == EDVPointKind::Survivor)
	{
		// They need a moment to get the hatch open; walk away and they wait for you to come back.
		Progress = FMath::Clamp(Progress + DeltaSeconds * (Near ? 1.f / RescueSeconds : -0.1f), 0.f, 1.f);
		if (Progress >= 1.f)
		{
			Take(Near);
		}
		return;
	}
	if (Near)
	{
		Take(Near);
	}
}

void ADVPointOfInterest::Take(ADVSoldierCharacter* By)
{
	bTaken = true;
	if (Kind == EDVPointKind::Cache && By)
	{
		// Full ammo, full health, a plate of armour: what a supply pod gives, from a crate someone left behind.
		if (ADVWeaponBase* W = By->GetCurrentWeapon())
		{
			W->ServerResupply();
		}
		if (UAbilitySystemComponent* ASC = By->GetAbilitySystemComponent())
		{
			ASC->SetNumericAttributeBase(UDVAttributeSet::GetHealthAttribute(), ASC->GetNumericAttribute(UDVAttributeSet::GetMaxHealthAttribute()));
			ASC->SetNumericAttributeBase(UDVAttributeSet::GetArmorAttribute(), FMath::Max(40.f, ASC->GetNumericAttribute(UDVAttributeSet::GetArmorAttribute())));
		}
	}
	OnRep_Taken();
	if (ADVGameMode* GM = GetWorld()->GetAuthGameMode<ADVGameMode>())
	{
		GM->OnPointTaken(this, By);
	}
}

void ADVPointOfInterest::OnRep_Taken()
{
	if (!bTaken)
	{
		return;
	}
	if (Beacon)
	{
		Beacon->SetVisibility(false);
		Light->SetIntensity(0.f);
	}
	DVFx::Flash(GetWorld(), GetActorLocation() + FVector(0.f, 0.f, 120.f), FLinearColor(0.6f, 0.9f, 1.f), 900.f, 500.f, 0.3f);
	DVSound::At(GetWorld(), TEXT("SW_UIClick"), GetActorLocation(), 0.8f, 0, 0.1f);
	if (Kind == EDVPointKind::Intel || Kind == EDVPointKind::Cache)
	{
		for (UStaticMeshComponent* C : Parts)
		{
			C->SetVisibility(false);	// picked up and carried off
		}
	}
}
