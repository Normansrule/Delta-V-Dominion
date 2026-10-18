#include "Vehicles/DVKestrel.h"
#include "Vehicles/DVVehicleKit.h"
#include "Core/DVFx.h"

#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"

ADVKestrel::ADVKestrel()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;	// identical, deterministic set dressing on every machine

	Hull = CreateDefaultSubobject<USceneComponent>(TEXT("Hull"));
	RootComponent = Hull;

	UMaterialInterface* Armour = DVKit::Material(TEXT("Navy"));
	UMaterialInterface* Metal = DVKit::Material(TEXT("Gunmetal"));
	UMaterialInterface* Dark = DVKit::Material(TEXT("Concrete"));
	UMaterialInterface* Glow = DVKit::Material(TEXT("Glow"));

	// Silhouette: long armoured spine, angled prow, flight deck slab, superstructure island, engine block.
	// Units are cm; the ship is ~1.4 km long, so parts are huge.
	// The spine is built around the hangar bay (x -43200..-36800): a 64 m slot in the starboard side opens onto the
	// hangar deck, which streams in as its own level when a Raven flies home (see GetHangarTransform).
	DVKit::Part(this, Hull, TEXT("SpineFore"), DVKit::Box(), Armour, FVector(9100.f, 0.f, 0.f), FRotator::ZeroRotator, FVector(91800.f, 22000.f, 16000.f));
	DVKit::Part(this, Hull, TEXT("SpineAft"), DVKit::Box(), Armour, FVector(-49100.f, 0.f, 0.f), FRotator::ZeroRotator, FVector(11800.f, 22000.f, 16000.f));
	DVKit::Part(this, Hull, TEXT("SpineBayTop"), DVKit::Box(), Armour, FVector(-40000.f, 0.f, 2300.f), FRotator::ZeroRotator, FVector(6400.f, 22000.f, 11400.f));
	DVKit::Part(this, Hull, TEXT("SpineBayKeel"), DVKit::Box(), Armour, FVector(-40000.f, 0.f, -7025.f), FRotator::ZeroRotator, FVector(6400.f, 22000.f, 1950.f));
	DVKit::Part(this, Hull, TEXT("SpineBayBack"), DVKit::Box(), Armour, FVector(-40000.f, -6000.f, -4725.f), FRotator::ZeroRotator, FVector(6400.f, 10000.f, 2650.f));
	// Glowing frame around the bay mouth so it reads from the ground as a landing bay.
	MouthGlows.Add(DVKit::Part(this, Hull, TEXT("BayFrameTop"), DVKit::Box(), Glow, FVector(-40000.f, 11060.f, -3330.f), FRotator::ZeroRotator, FVector(6500.f, 140.f, 140.f)));
	MouthGlows.Add(DVKit::Part(this, Hull, TEXT("BayFrameLow"), DVKit::Box(), Glow, FVector(-40000.f, 11060.f, -6100.f), FRotator::ZeroRotator, FVector(6500.f, 140.f, 140.f)));
	MouthGlows.Add(DVKit::Part(this, Hull, TEXT("BayFrameFore"), DVKit::Box(), Glow, FVector(-36780.f, 11060.f, -4715.f), FRotator::ZeroRotator, FVector(140.f, 140.f, 2900.f)));
	MouthGlows.Add(DVKit::Part(this, Hull, TEXT("BayFrameAft"), DVKit::Box(), Glow, FVector(-43220.f, 11060.f, -4715.f), FRotator::ZeroRotator, FVector(140.f, 140.f, 2900.f)));
	DVKit::Part(this, Hull, TEXT("Prow"), DVKit::Box(), Armour, FVector(62000.f, 0.f, -2000.f), FRotator(-12.f, 0.f, 0.f), FVector(30000.f, 17000.f, 11000.f));
	DVKit::Part(this, Hull, TEXT("ProwBlade"), DVKit::Box(), Metal, FVector(76000.f, 0.f, -4000.f), FRotator(-20.f, 0.f, 0.f), FVector(16000.f, 6000.f, 5000.f));
	DVKit::Part(this, Hull, TEXT("Keel"), DVKit::Box(), Dark, FVector(-5000.f, 0.f, -10000.f), FRotator::ZeroRotator, FVector(80000.f, 12000.f, 6000.f));
	DVKit::Part(this, Hull, TEXT("FlightDeck"), DVKit::Box(), Dark, FVector(-8000.f, 0.f, 9000.f), FRotator::ZeroRotator, FVector(90000.f, 30000.f, 2500.f));
	DVKit::Part(this, Hull, TEXT("SponsonL"), DVKit::Box(), Armour, FVector(5000.f, -15500.f, -1000.f), FRotator::ZeroRotator, FVector(60000.f, 9000.f, 9000.f));
	DVKit::Part(this, Hull, TEXT("SponsonR"), DVKit::Box(), Armour, FVector(5000.f, 15500.f, -1000.f), FRotator::ZeroRotator, FVector(60000.f, 9000.f, 9000.f));
	DVKit::Part(this, Hull, TEXT("Island"), DVKit::Box(), Armour, FVector(-22000.f, 11000.f, 17000.f), FRotator::ZeroRotator, FVector(18000.f, 6000.f, 14000.f));
	DVKit::Part(this, Hull, TEXT("Bridge"), DVKit::Box(), Metal, FVector(-19000.f, 11000.f, 25000.f), FRotator::ZeroRotator, FVector(10000.f, 7000.f, 3000.f));
	DVKit::Part(this, Hull, TEXT("BridgeGlass"), DVKit::Box(), Glow, FVector(-14200.f, 11000.f, 25000.f), FRotator::ZeroRotator, FVector(300.f, 6500.f, 900.f));
	DVKit::Part(this, Hull, TEXT("Mast"), DVKit::Cylinder(), Metal, FVector(-22000.f, 11000.f, 31000.f), FRotator::ZeroRotator, FVector(800.f, 800.f, 9000.f));
	DVKit::Part(this, Hull, TEXT("EngineBlock"), DVKit::Box(), Metal, FVector(-62000.f, 0.f, 0.f), FRotator::ZeroRotator, FVector(16000.f, 30000.f, 18000.f));
	for (int32 i = 0; i < 4; ++i)
	{
		const float Y = -10500.f + i * 7000.f;
		DVKit::Part(this, Hull, *FString::Printf(TEXT("Nozzle%d"), i), DVKit::Cylinder(), Metal, FVector(-71000.f, Y, 0.f), FRotator(90.f, 0.f, 0.f), FVector(6000.f, 6000.f, 5000.f));
		EngineGlows.Add(DVKit::Part(this, Hull, *FString::Printf(TEXT("Burn%d"), i), DVKit::Cylinder(), Glow, FVector(-73700.f, Y, 0.f), FRotator(90.f, 0.f, 0.f), FVector(5200.f, 5200.f, 300.f)));
	}
	// Armour plating rhythm along the spine (reads as scale from the ground).
	for (int32 i = 0; i < 9; ++i)
	{
		const float X = -45000.f + i * 11000.f;
		DVKit::Part(this, Hull, *FString::Printf(TEXT("RibL%d"), i), DVKit::Box(), Metal, FVector(X, -11300.f, 0.f), FRotator::ZeroRotator, FVector(2500.f, 900.f, 15000.f));
		DVKit::Part(this, Hull, *FString::Printf(TEXT("RibR%d"), i), DVKit::Box(), Metal, FVector(X, 11300.f, 0.f), FRotator::ZeroRotator, FVector(2500.f, 900.f, 15000.f));
		RunningLights.Add(DVKit::Part(this, Hull, *FString::Printf(TEXT("RunL%d"), i), DVKit::Sphere(), Glow, FVector(X, -11900.f, -6000.f), FRotator::ZeroRotator, FVector(600.f, 600.f, 600.f)));
		RunningLights.Add(DVKit::Part(this, Hull, *FString::Printf(TEXT("RunR%d"), i), DVKit::Sphere(), Glow, FVector(X, 11900.f, -6000.f), FRotator::ZeroRotator, FVector(600.f, 600.f, 600.f)));
		DeckLights.Add(DVKit::Part(this, Hull, *FString::Printf(TEXT("Deck%d"), i), DVKit::Box(), Glow, FVector(X, 0.f, 10300.f), FRotator::ZeroRotator, FVector(3000.f, 250.f, 80.f)));
	}

	// Main battery: twin spinal guns under the prow.
	Battery = CreateDefaultSubobject<USceneComponent>(TEXT("Battery"));
	Battery->SetupAttachment(Hull);
	Battery->SetRelativeLocation(FVector(58000.f, 0.f, -9000.f));
	DVKit::Part(this, Battery, TEXT("BatteryHousing"), DVKit::Box(), Metal, FVector(-6000.f, 0.f, 0.f), FRotator::ZeroRotator, FVector(14000.f, 9000.f, 5000.f));
	DVKit::Part(this, Battery, TEXT("BarrelA"), DVKit::Cylinder(), Metal, FVector(4000.f, -2200.f, -1500.f), FRotator(90.f, 0.f, 0.f), FVector(1600.f, 1600.f, 16000.f));
	DVKit::Part(this, Battery, TEXT("BarrelB"), DVKit::Cylinder(), Metal, FVector(4000.f, 2200.f, -1500.f), FRotator(90.f, 0.f, 0.f), FVector(1600.f, 1600.f, 16000.f));

	// Point-defence turrets along the sponsons.
	for (int32 i = 0; i < 6; ++i)
	{
		const float X = -30000.f + i * 12000.f;
		const float Y = (i % 2 ? 1.f : -1.f) * 19500.f;
		USceneComponent* Mount = CreateDefaultSubobject<USceneComponent>(*FString::Printf(TEXT("Flak%d"), i));
		Mount->SetupAttachment(Hull);
		Mount->SetRelativeLocation(FVector(X, Y, -3500.f));
		DVKit::Part(this, Mount, *FString::Printf(TEXT("FlakBase%d"), i), DVKit::Cylinder(), Metal, FVector::ZeroVector, FRotator::ZeroRotator, FVector(2200.f, 2200.f, 900.f));
		FlakGuns.Add(Mount);
	}
}

ADVKestrel* ADVKestrel::Find(const UWorld* World)
{
	if (World)
	{
		for (TActorIterator<ADVKestrel> It(const_cast<UWorld*>(World)); It; ++It)
		{
			return *It;
		}
	}
	return nullptr;
}

void ADVKestrel::BeginPlay()
{
	Super::BeginPlay();
	BaseLocation = GetActorLocation();
	for (UStaticMeshComponent* Burn : EngineGlows)
	{
		DVKit::SetGlow(Burn, FLinearColor(0.35f, 0.65f, 1.f), 30.f);
	}
	for (UStaticMeshComponent* Frame : MouthGlows)
	{
		DVKit::SetGlow(Frame, FLinearColor(0.1f, 0.7f, 1.f), 25.f);
	}
	for (UStaticMeshComponent* Deck : DeckLights)
	{
		DVKit::SetGlow(Deck, FLinearColor(1.f, 0.75f, 0.35f), 12.f);
	}
	TArray<UStaticMeshComponent*> Parts;
	GetComponents(Parts);
	for (UStaticMeshComponent* P : Parts)
	{
		if (P->GetName() == TEXT("BridgeGlass"))
		{
			DVKit::SetGlow(P, FLinearColor(0.6f, 0.85f, 1.f), 8.f);
		}
		P->CastShadow = true;
	}
}

void ADVKestrel::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float T = GetWorld()->GetTimeSeconds();

	// Holding station: a slow heave and drift so 1.4 km of ship never looks painted on.
	SetActorLocation(bHolding ? BaseLocation : BaseLocation + FVector(FMath::Sin(T * 0.05f) * 1500.f, FMath::Cos(T * 0.04f) * 900.f, FMath::Sin(T * 0.13f) * 600.f));

	// Running lights blink in sequence, red to port and green to starboard.
	for (int32 i = 0; i < RunningLights.Num(); ++i)
	{
		const bool bOn = FMath::Fmod(T * 1.5f - (i / 2) * 0.15f, 1.f) < 0.18f;
		DVKit::SetGlow(RunningLights[i], i % 2 ? FLinearColor(0.1f, 1.f, 0.2f) : FLinearColor(1.f, 0.08f, 0.04f), bOn ? 60.f : 1.f);
	}

	// Point defence: short flak bursts at the sky (cosmetic: the Kestrel is fighting off CINDER drones overhead).
	if (T >= NextFlak && FlakGuns.Num() > 0)
	{
		NextFlak = T + FMath::FRandRange(1.5f, 4.f);
		USceneComponent* Gun = FlakGuns[FMath::RandRange(0, FlakGuns.Num() - 1)];
		const FVector From = Gun->GetComponentLocation();
		const FVector To = From + FMath::VRandCone(FVector(0.f, 0.f, 1.f), FMath::DegreesToRadians(70.f)) * FMath::FRandRange(30000.f, 60000.f);
		DVFx::Beam(GetWorld(), From, To, FLinearColor(120.f, 60.f, 10.f), 250.f, 0.08f);
		DVFx::Explosion(GetWorld(), To, 1.6f);
	}
}

FTransform ADVKestrel::GetHangarTransform() const
{
	const FTransform Ship(GetActorRotation(), BaseLocation);
	return FTransform(FRotator(0.f, GetActorRotation().Yaw + HangarYaw, 0.f), Ship.TransformPosition(FVector(HangarX, HangarY, HangarZ)));
}

void ADVKestrel::HoldStation()
{
	bHolding = true;
	SetActorLocation(BaseLocation);
}

FVector ADVKestrel::GetBatteryLocation() const
{
	return Battery->GetComponentLocation() + Battery->GetForwardVector() * 12000.f;
}

void ADVKestrel::FireMainBattery(const FVector& Target)
{
	const FVector Muzzle = GetBatteryLocation();
	DVFx::Flash(GetWorld(), Muzzle, FLinearColor(1.f, 0.7f, 0.4f), 3000000.f, 60000.f, 0.15f);
	DVFx::Beam(GetWorld(), Muzzle, Target, FLinearColor(200.f, 120.f, 40.f), 160.f, 0.35f);
}
