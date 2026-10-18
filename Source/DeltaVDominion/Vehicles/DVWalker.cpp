#include "Vehicles/DVWalker.h"
#include "Vehicles/DVVehicleKit.h"
#include "Characters/DVSoldierCharacter.h"
#include "Core/DVFx.h"
#include "Core/DVGameMode.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

namespace
{
	constexpr int32 Legs = 4;
	constexpr float Thigh = 1700.f;
	constexpr float Shin = 1700.f;
	constexpr float StepSeconds = 1.6f;	// per diagonal pair
	constexpr float StepLift = 380.f;
	constexpr float LowerSeconds = 5.f;
	constexpr float RampSeconds = 2.f;
	constexpr float CollapseSeconds = 3.5f;
	constexpr float CouplingSeconds = 4.f;
	constexpr float CouplingReach = 260.f;

	// Leg order: front-left, front-right, back-left, back-right. Diagonal pairs step together: {0,3} then {1,2}.
	const FVector HipLocal[Legs] = { FVector(1000.f, -760.f, 200.f), FVector(1000.f, 760.f, 200.f), FVector(-1000.f, -760.f, 200.f), FVector(-1000.f, 760.f, 200.f) };
	const FVector RestLocal[Legs] = { FVector(1450.f, -1500.f, 0.f), FVector(1450.f, 1500.f, 0.f), FVector(-1450.f, -1500.f, 0.f), FVector(-1450.f, 1500.f, 0.f) };
	const FVector CouplingLocal[ADVWalker::NumCouplings] = { FVector(-700.f, -330.f, 600.f), FVector(100.f, 380.f, 600.f), FVector(850.f, -250.f, 600.f) };

	float Ease(float T) { T = FMath::Clamp(T, 0.f, 1.f); return T * T * (3.f - 2.f * T); }

	UStaticMeshComponent* Solid(UStaticMeshComponent* C)
	{
		C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		C->SetCollisionResponseToAllChannels(ECR_Block);
		C->SetCanEverAffectNavigation(false);
		return C;
	}

	UStaticMeshComponent* Free(UStaticMeshComponent* C)
	{
		C->SetUsingAbsoluteLocation(true);
		C->SetUsingAbsoluteRotation(true);
		return C;
	}
}

ADVWalker::ADVWalker()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(true);
	bAlwaysRelevant = true;	// 30 m tall: everyone on the map can see it

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Body = CreateDefaultSubobject<USceneComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);

	UMaterialInterface* Metal = DVKit::Material(TEXT("Gunmetal"));
	UMaterialInterface* Armour = DVKit::Material(TEXT("Navy"));
	UMaterialInterface* Dark = DVKit::Material(TEXT("Concrete"));
	UMaterialInterface* Rust = DVKit::Material(TEXT("Rust"));
	UMaterialInterface* Glow = DVKit::Material(TEXT("Glow"));

	// The back is a deck: hull top at 500, railings, a gap at the rear for the ramp.
	Hull = Solid(DVKit::Part(this, Body, TEXT("Hull"), DVKit::Box(), Metal, FVector(0.f, 0.f, 250.f), FRotator::ZeroRotator, FVector(2600.f, 1400.f, 500.f)));
	DVKit::Part(this, Body, TEXT("DeckPlate"), DVKit::Box(), Dark, FVector(-50.f, 0.f, 503.f), FRotator::ZeroRotator, FVector(2400.f, 1250.f, 8.f));
	Solid(DVKit::Part(this, Body, TEXT("RailL"), DVKit::Box(), Armour, FVector(0.f, -690.f, 560.f), FRotator::ZeroRotator, FVector(2600.f, 40.f, 120.f)));
	Solid(DVKit::Part(this, Body, TEXT("RailR"), DVKit::Box(), Armour, FVector(0.f, 690.f, 560.f), FRotator::ZeroRotator, FVector(2600.f, 40.f, 120.f)));
	Solid(DVKit::Part(this, Body, TEXT("RailFront"), DVKit::Box(), Armour, FVector(1290.f, 0.f, 560.f), FRotator::ZeroRotator, FVector(40.f, 1400.f, 120.f)));
	Solid(DVKit::Part(this, Body, TEXT("RailBackL"), DVKit::Box(), Armour, FVector(-1290.f, -470.f, 560.f), FRotator::ZeroRotator, FVector(40.f, 460.f, 120.f)));
	Solid(DVKit::Part(this, Body, TEXT("RailBackR"), DVKit::Box(), Armour, FVector(-1290.f, 470.f, 560.f), FRotator::ZeroRotator, FVector(40.f, 460.f, 120.f)));
	// Armour skirts, belly machinery, vents.
	DVKit::Part(this, Body, TEXT("SkirtL"), DVKit::Box(), Armour, FVector(0.f, -730.f, 200.f), FRotator(0.f, 0.f, -12.f), FVector(2500.f, 60.f, 420.f));
	DVKit::Part(this, Body, TEXT("SkirtR"), DVKit::Box(), Armour, FVector(0.f, 730.f, 200.f), FRotator(0.f, 0.f, 12.f), FVector(2500.f, 60.f, 420.f));
	DVKit::Part(this, Body, TEXT("Belly"), DVKit::Box(), Rust, FVector(-100.f, 0.f, -90.f), FRotator::ZeroRotator, FVector(1800.f, 900.f, 180.f));
	DVKit::Part(this, Body, TEXT("Reactor"), DVKit::Cylinder(), Metal, FVector(-300.f, 0.f, -200.f), FRotator::ZeroRotator, FVector(500.f, 500.f, 160.f));
	for (int32 i = 0; i < 4; ++i)
	{
		DVKit::Part(this, Body, *FString::Printf(TEXT("Vent%d"), i), DVKit::Box(), Metal, FVector(-1000.f + i * 220.f, 0.f, 540.f), FRotator::ZeroRotator, FVector(140.f, 500.f, 70.f));
	}
	// Head and cannon at the front.
	DVKit::Part(this, Body, TEXT("Head"), DVKit::Box(), Armour, FVector(1650.f, 0.f, 260.f), FRotator(-6.f, 0.f, 0.f), FVector(800.f, 760.f, 520.f));
	DVKit::Part(this, Body, TEXT("Jaw"), DVKit::Box(), Metal, FVector(1900.f, 0.f, -40.f), FRotator(10.f, 0.f, 0.f), FVector(500.f, 560.f, 160.f));
	Optic = DVKit::Part(this, Body, TEXT("Optic"), DVKit::Box(), Glow, FVector(2060.f, 0.f, 330.f), FRotator(-6.f, 0.f, 0.f), FVector(30.f, 460.f, 60.f));
	DVKit::Part(this, Body, TEXT("Turret"), DVKit::Cylinder(), Metal, FVector(1500.f, 0.f, 600.f), FRotator::ZeroRotator, FVector(500.f, 500.f, 200.f));
	DVKit::Part(this, Body, TEXT("Barrel"), DVKit::Cylinder(), Metal, FVector(2150.f, 0.f, 650.f), FRotator(-90.f, 0.f, 0.f), FVector(130.f, 130.f, 1300.f));
	Muzzle = CreateDefaultSubobject<USceneComponent>(TEXT("Muzzle"));
	Muzzle->SetupAttachment(Body);
	Muzzle->SetRelativeLocation(FVector(2820.f, 0.f, 650.f));
	OpticLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("OpticLight"));
	OpticLight->SetupAttachment(Body);
	OpticLight->SetRelativeLocation(FVector(2200.f, 0.f, 330.f));
	OpticLight->SetIntensityUnits(ELightUnits::Candelas);
	OpticLight->SetIntensity(3000.f);
	OpticLight->SetLightColor(FLinearColor(1.f, 0.1f, 0.05f));
	OpticLight->SetAttenuationRadius(3000.f);
	OpticLight->SetCastShadows(false);

	// Three power couplings on the deck: the way to bring it down.
	for (int32 i = 0; i < NumCouplings; ++i)
	{
		DVKit::Part(this, Body, *FString::Printf(TEXT("CouplingBase%d"), i), DVKit::Box(), Metal, CouplingLocal[i] - FVector(0.f, 0.f, 80.f), FRotator::ZeroRotator, FVector(200.f, 200.f, 40.f));
		Couplings.Add(Solid(DVKit::Part(this, Body, *FString::Printf(TEXT("Coupling%d"), i), DVKit::Cylinder(), Glow, CouplingLocal[i] + FVector(0.f, 0.f, 20.f), FRotator::ZeroRotator, FVector(110.f, 110.f, 200.f))));
		UPointLightComponent* L = CreateDefaultSubobject<UPointLightComponent>(*FString::Printf(TEXT("CouplingLight%d"), i));
		L->SetupAttachment(Body);
		L->SetRelativeLocation(CouplingLocal[i] + FVector(0.f, 0.f, 200.f));
		L->SetIntensityUnits(ELightUnits::Candelas);
		L->SetIntensity(800.f);
		L->SetAttenuationRadius(900.f);
		L->SetCastShadows(false);
		CouplingLights.Add(L);
	}

	// The rear ramp: hinged at the deck edge, folded up while walking.
	RampHinge = CreateDefaultSubobject<USceneComponent>(TEXT("RampHinge"));
	RampHinge->SetupAttachment(Body);
	RampHinge->SetRelativeLocation(FVector(-1300.f, 0.f, 500.f));
	Ramp = DVKit::Part(this, RampHinge, TEXT("Ramp"), DVKit::Box(), Metal, FVector(-900.f, 0.f, -12.f), FRotator::ZeroRotator, FVector(1800.f, 480.f, 24.f));
	DVKit::Part(this, RampHinge, TEXT("RampStripe"), DVKit::Box(), Glow, FVector(-900.f, 0.f, 2.f), FRotator::ZeroRotator, FVector(1700.f, 30.f, 4.f));

	// Legs: positioned in world space every frame.
	for (int32 i = 0; i < Legs; ++i)
	{
		Upper.Add(Free(DVKit::Part(this, Root, *FString::Printf(TEXT("Thigh%d"), i), DVKit::Box(), Armour, FVector::ZeroVector, FRotator::ZeroRotator, FVector(Thigh, 260.f, 300.f))));
		Lower.Add(Free(DVKit::Part(this, Root, *FString::Printf(TEXT("Shin%d"), i), DVKit::Box(), Metal, FVector::ZeroVector, FRotator::ZeroRotator, FVector(Shin, 200.f, 220.f))));
		Knees.Add(Free(DVKit::Part(this, Root, *FString::Printf(TEXT("Knee%d"), i), DVKit::Sphere(), Rust, FVector::ZeroVector, FRotator::ZeroRotator, FVector(360.f, 360.f, 360.f))));
		Feet.Add(Free(DVKit::Part(this, Root, *FString::Printf(TEXT("Foot%d"), i), DVKit::Cylinder(), Metal, FVector::ZeroVector, FRotator::ZeroRotator, FVector(480.f, 480.f, 140.f))));
		DVKit::Part(this, Body, *FString::Printf(TEXT("Hip%d"), i), DVKit::Sphere(), Rust, HipLocal[i], FRotator::ZeroRotator, FVector(420.f, 420.f, 420.f));
	}
	// The cannon's aim mark on the ground.
	TelegraphRing = Free(DVKit::Part(this, Root, TEXT("Telegraph"), DVKit::Cylinder(), Glow, FVector::ZeroVector, FRotator::ZeroRotator, FVector(900.f, 900.f, 8.f)));
}

void ADVWalker::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADVWalker, State);
	DOREPLIFETIME(ADVWalker, CouplingsBlown);
	DOREPLIFETIME(ADVWalker, CouplingProgress);
	DOREPLIFETIME(ADVWalker, BlownMask);
}

ADVWalker* ADVWalker::Find(const UWorld* World)
{
	if (World)
	{
		for (TActorIterator<ADVWalker> It(const_cast<UWorld*>(World)); It; ++It)
		{
			return *It;
		}
	}
	return nullptr;
}

void ADVWalker::BeginPlay()
{
	Super::BeginPlay();
	DVKit::SetGlow(Optic, FLinearColor(1.f, 0.06f, 0.02f), 40.f);
	TelegraphRing->SetVisibility(false);
	SetRamp(0.f);

	const FVector Centre = GetActorLocation();
	Waypoints = { Centre + FVector(-1800.f, 0.f, 0.f), Centre + FVector(0.f, 2200.f, 0.f), Centre + FVector(1800.f, 0.f, 0.f), Centre + FVector(0.f, -2200.f, 0.f) };
	if (HasAuthority())
	{
		// Start on the far side of the loop, walking toward the first kneeling point (nearest the drop zone).
		const FVector Start = Waypoints[3];
		Heading = (Waypoints[0] - Start).Rotation().Yaw;
		SetActorLocationAndRotation(FVector(Start.X, Start.Y, GroundZ(Start) + WalkHeight), FRotator(0.f, Heading, 0.f));
		NextWaypoint = 0;
		StateTime = GetWorld()->GetTimeSeconds();
		NextShot = GetWorld()->GetTimeSeconds() + 12.0;

		// Two frames ride the deck as guards; one of them is the gunner the cannon's damage is credited to.
		for (int32 i = 0; i < 2; ++i)
		{
			const FTransform Xf(GetActorRotation(), GetActorTransform().TransformPosition(FVector(i == 0 ? 600.f : -300.f, i == 0 ? 200.f : -350.f, 600.f)));
			if (ADVSoldierCharacter* Guard = GetWorld()->SpawnActorDeferred<ADVSoldierCharacter>(ADVSoldierCharacter::StaticClass(), Xf, nullptr, nullptr,
				ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn))
			{
				Guard->TeamId = 2;
				UGameplayStatics::FinishSpawningActor(Guard, Xf);
				if (!Guard->GetController())
				{
					Guard->SpawnDefaultController();	// spawned, not placed: give it its AI brain (and its gravity)
				}
				if (!Gunner.IsValid())
				{
					Gunner = Guard;
				}
			}
		}
	}
	for (int32 i = 0; i < Legs; ++i)
	{
		FVector Rest = GetActorTransform().TransformPosition(RestLocal[i]);
		Rest.Z = GroundZ(Rest);
		FootPlanted.Add(Rest);
		FootFrom.Add(Rest);
		FootTo.Add(Rest);
		FootSwing.Add(-1.f);
	}
}

float ADVWalker::GroundZ(const FVector& At) const
{
	FHitResult Hit;
	FCollisionQueryParams Params(TEXT("DVWalkerGround"), false, this);
	FCollisionObjectQueryParams Objects(ECC_WorldStatic);
	if (GetWorld()->LineTraceSingleByObjectType(Hit, FVector(At.X, At.Y, At.Z + 8000.f), FVector(At.X, At.Y, At.Z - 20000.f), Objects, Params))
	{
		return Hit.ImpactPoint.Z;
	}
	return At.Z - Height;
}

void ADVWalker::SetRamp(float Alpha)
{
	// Folded flat against the stern at 0, down to the ground at 1.
	const float Drop = FMath::Max(Height + 500.f, 200.f);
	const float Deployed = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Drop / 1800.f, 0.f, 1.f)));
	RampHinge->SetRelativeRotation(FRotator(FMath::Lerp(-85.f, Deployed, Ease(Alpha)), 0.f, 0.f));
	const bool bDown = Alpha > 0.95f;
	Ramp->SetCollisionEnabled(bDown ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	Ramp->SetCollisionResponseToAllChannels(ECR_Block);
}

FVector ADVWalker::GetActiveCouplingLocation() const
{
	for (int32 i = 0; i < NumCouplings; ++i)
	{
		if (!(BlownMask & (1 << i)))
		{
			return Couplings[i]->GetComponentLocation();
		}
	}
	return GetActorLocation();
}

bool ADVWalker::IsAboard(const AActor* Actor) const
{
	if (!Actor)
	{
		return false;
	}
	const FVector Local = GetActorTransform().InverseTransformPosition(Actor->GetActorLocation());
	return Local.X > -1290.f && Local.X < 1350.f && FMath::Abs(Local.Y) < 720.f && Local.Z > 300.f && Local.Z < 900.f;	// the hull, not the ramp
}

bool ADVWalker::AnyoneOnRamp() const
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APawn* P = It->Get() ? It->Get()->GetPawn() : nullptr;
		if (P)
		{
			const FVector Local = GetActorTransform().InverseTransformPosition(P->GetActorLocation());
			if (Local.X < -1250.f && Local.X > -3000.f && FMath::Abs(Local.Y) < 320.f && Local.Z > -500.f && Local.Z < 900.f)
			{
				return true;
			}
		}
	}
	return false;
}

void ADVWalker::DevBlowAll()
{
	for (int32 i = 0; i < NumCouplings; ++i)
	{
		if (!(BlownMask & (1 << i)))
		{
			BlowCoupling(i);
		}
	}
}

void ADVWalker::DevKneel()
{
	if (State == EDVWalkerState::Walking)
	{
		State = EDVWalkerState::Kneeling;
		StateTime = GetWorld()->GetTimeSeconds();
	}
}

void ADVWalker::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority())
	{
		TickServer(DeltaSeconds);
	}
	TickLegs(DeltaSeconds);

	// Couplings glow cyan, flicker while being overloaded, and go dark when blown.
	const float T = GetWorld()->GetTimeSeconds();
	for (int32 i = 0; i < NumCouplings; ++i)
	{
		const bool bBlown = (BlownMask & (1 << i)) != 0;
		const bool bWorking = !bBlown && CouplingProgress > 0.01f && GetActiveCouplingLocation().Equals(Couplings[i]->GetComponentLocation(), 1.f);
		const float Flicker = bWorking ? 0.5f + 0.5f * FMath::Sin(T * (10.f + 30.f * CouplingProgress)) : 1.f;
		const FLinearColor Color = bBlown ? FLinearColor(1.f, 0.3f, 0.05f) : FLinearColor(0.1f, 0.75f, 1.f);
		DVKit::SetGlow(Couplings[i], Color, bBlown ? 1.5f : 7.f * Flicker + 2.f);
		CouplingLights[i]->SetLightColor(Color);
		CouplingLights[i]->SetIntensity(bBlown ? 300.f + 250.f * FMath::Sin(T * 13.f + i) : 800.f * Flicker);
	}
	// Telegraph ring pulses until the shell lands.
	if (TelegraphUntil > 0.0)
	{
		const float Left = TelegraphUntil - T;
		TelegraphRing->SetVisibility(Left > 0.f);
		if (Left > 0.f)
		{
			DVKit::SetGlow(TelegraphRing, FLinearColor(1.f, 0.05f, 0.02f), 6.f + 20.f * (0.5f + 0.5f * FMath::Sin(T * (8.f + 12.f * (2.2f - Left)))));
		}
	}
}

void ADVWalker::TickServer(float DeltaSeconds)
{
	const double Now = GetWorld()->GetTimeSeconds();
	const float InState = Now - StateTime;
	FVector Loc = GetActorLocation();

	switch (State)
	{
	case EDVWalkerState::Walking:
	{
		const FVector Target = Waypoints[NextWaypoint];
		const FVector To = (Target - Loc).GetSafeNormal2D();
		const float Want = To.Rotation().Yaw;
		Heading = FMath::FixedTurn(Heading, Want, 9.f * DeltaSeconds);
		const float Align = FMath::Clamp(FVector::DotProduct(FRotator(0.f, Heading, 0.f).Vector(), To), 0.15f, 1.f);
		Loc += FRotator(0.f, Heading, 0.f).Vector() * WalkSpeed * Align * DeltaSeconds;
		if (FVector::Dist2D(Loc, Target) < 150.f)
		{
			const bool bKneelHere = NextWaypoint == 0 || NextWaypoint == 2;
			NextWaypoint = (NextWaypoint + 1) % Waypoints.Num();
			if (bKneelHere)
			{
				State = EDVWalkerState::Kneeling;
				StateTime = Now;
				DVSound::At(GetWorld(), TEXT("SW_Explosion"), Loc, 0.5f, 2);
			}
		}
		break;
	}
	case EDVWalkerState::Kneeling:
		Height = FMath::Lerp(WalkHeight, KneelHeight, Ease(InState / LowerSeconds));
		if (InState >= LowerSeconds)
		{
			State = EDVWalkerState::Knelt;
			StateTime = Now;
		}
		break;
	case EDVWalkerState::Knelt:
		// It waits (up to 10 s more) while anyone is still on the ramp, rather than fling them off it.
		if (InState >= KneelSeconds && (InState >= KneelSeconds + 10.f || !AnyoneOnRamp()))
		{
			State = EDVWalkerState::Rising;
			StateTime = Now;
		}
		break;
	case EDVWalkerState::Rising:
		Height = FMath::Lerp(KneelHeight, WalkHeight, Ease((InState - RampSeconds) / LowerSeconds));
		if (InState >= RampSeconds + LowerSeconds)
		{
			State = EDVWalkerState::Walking;
			StateTime = Now;
		}
		break;
	case EDVWalkerState::Collapsing:
	{
		// It doesn't kneel this time: the legs give and nine hundred tonnes drop.
		const float A = FMath::Clamp(InState / CollapseSeconds, 0.f, 1.f);
		Height = FMath::Lerp(Height, 120.f, A * A);
		if (A >= 1.f)
		{
			State = EDVWalkerState::Down;
			StateTime = Now;
			DVSound::At(GetWorld(), TEXT("SW_Explosion"), Loc, 1.f, 2);
			if (ADVGameMode* GM = GetWorld()->GetAuthGameMode<ADVGameMode>())
			{
				GM->OnWalkerDown();
			}
		}
		break;
	}
	default:
		break;
	}

	const float Ground = GroundZ(Loc);
	Loc.Z = FMath::FInterpTo(Loc.Z, Ground + Height, DeltaSeconds, 6.f);
	if (State == EDVWalkerState::Kneeling || State == EDVWalkerState::Rising || State == EDVWalkerState::Collapsing)
	{
		Loc.Z = Ground + Height;
	}
	FRotator Rot(0.f, Heading, 0.f);
	if (State == EDVWalkerState::Collapsing || State == EDVWalkerState::Down)
	{
		const float A = FMath::Clamp((Now - StateTime) / CollapseSeconds, 0.f, 1.f);
		Rot += State == EDVWalkerState::Down ? FRotator(-5.f, 0.f, 7.f) : FRotator(-5.f * A, 0.f, 7.f * A);
	}
	SetActorLocationAndRotation(Loc, Rot);

	TickCouplings(DeltaSeconds);
	TickCannon();
}

void ADVWalker::TickCouplings(float DeltaSeconds)
{
	if (State == EDVWalkerState::Collapsing || State == EDVWalkerState::Down)
	{
		return;
	}
	int32 Active = -1;
	for (int32 i = 0; i < NumCouplings; ++i)
	{
		if (!(BlownMask & (1 << i)))
		{
			Active = i;
			break;
		}
	}
	if (Active < 0)
	{
		return;
	}
	// Anyone standing at the active coupling overloads it; walk away and it slowly re-stabilises.
	bool bWorked = false;
	const FVector At = Couplings[Active]->GetComponentLocation();
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const ADVSoldierCharacter* S = It->Get() ? Cast<ADVSoldierCharacter>(It->Get()->GetPawn()) : nullptr;
		if (S && !S->IsOutOfAction() && FVector::Dist(S->GetActorLocation(), At) < CouplingReach)
		{
			bWorked = true;
		}
	}
	CouplingProgress = FMath::Clamp(CouplingProgress + (bWorked ? DeltaSeconds / CouplingSeconds : -DeltaSeconds * 0.05f), 0.f, 1.f);
	if (CouplingProgress >= 1.f)
	{
		BlowCoupling(Active);
	}
}

void ADVWalker::BlowCoupling(int32 Index)
{
	BlownMask |= (1 << Index);
	++CouplingsBlown;
	CouplingProgress = 0.f;
	MulticastCouplingBlown(Index);
	if (ADVGameMode* GM = GetWorld()->GetAuthGameMode<ADVGameMode>())
	{
		GM->OnWalkerCoupling(CouplingsBlown);
	}
	if (CouplingsBlown >= NumCouplings)
	{
		State = EDVWalkerState::Collapsing;
		StateTime = GetWorld()->GetTimeSeconds();
		MulticastCollapse();
	}
}

void ADVWalker::TickCannon()
{
	const double Now = GetWorld()->GetTimeSeconds();
	if (PendingImpact > 0.0 && Now >= PendingImpact)
	{
		PendingImpact = -1.0;
		MulticastCannonFire(PendingTarget);
		ADVSoldierCharacter* Shooter = Gunner.Get();
		if (!Shooter || Shooter->IsDead())
		{
			for (TActorIterator<ADVSoldierCharacter> It(GetWorld()); It; ++It)
			{
				if (It->TeamId != 1 && !It->IsDead())
				{
					Shooter = *It;
					break;
				}
			}
		}
		if (Shooter)
		{
			DVCombat::RadialDamage(Shooter, PendingTarget, 450.f, 75.f, false);
		}
	}
	if (bHoldFire || State == EDVWalkerState::Collapsing || State == EDVWalkerState::Down || Now < NextShot || PendingImpact > 0.0)
	{
		return;
	}
	NextShot = Now + 7.0;
	// The nearest soldier on the ground (not on its back) within range gets a shell, two seconds after the mark.
	const APawn* Best = nullptr;
	float BestDist = 9000.f;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APawn* P = It->Get() ? It->Get()->GetPawn() : nullptr;
		const ADVSoldierCharacter* S = Cast<ADVSoldierCharacter>(P);
		if (P && !(S && S->IsOutOfAction()) && !IsAboard(P))
		{
			const float D = FVector::Dist2D(P->GetActorLocation(), GetActorLocation());
			if (D < BestDist && D > 1200.f)
			{
				BestDist = D;
				Best = P;
			}
		}
	}
	if (!Best)
	{
		return;
	}
	FVector Target = Best->GetActorLocation();
	Target.Z = GroundZ(Target);
	PendingTarget = Target;
	PendingImpact = Now + 2.2;
	MulticastTelegraph(Target);
}

void ADVWalker::MulticastTelegraph_Implementation(FVector_NetQuantize Target)
{
	TelegraphRing->SetWorldLocationAndRotation(FVector(Target) + FVector(0.f, 0.f, 6.f), FRotator::ZeroRotator);
	TelegraphUntil = GetWorld()->GetTimeSeconds() + 2.2;
	DVSound::At(GetWorld(), TEXT("SW_RadioSquelch"), Target, 0.8f, 1);
}

void ADVWalker::MulticastCannonFire_Implementation(FVector_NetQuantize Target)
{
	TelegraphUntil = -1.0;
	TelegraphRing->SetVisibility(false);
	DVFx::Beam(GetWorld(), Muzzle->GetComponentLocation(), Target, FLinearColor(60.f, 6.f, 2.f), 90.f, 0.15f);
	DVFx::Explosion(GetWorld(), Target, 2.2f);
	DVSound::At(GetWorld(), TEXT("SW_Explosion"), Target, 1.f, 2);
}

void ADVWalker::MulticastCouplingBlown_Implementation(int32 Index)
{
	if (Couplings.IsValidIndex(Index))
	{
		const FVector At = Couplings[Index]->GetComponentLocation();
		DVFx::Explosion(GetWorld(), At, 1.4f);
		DVSound::At(GetWorld(), TEXT("SW_Explosion"), At, 0.9f, 1);
	}
}

void ADVWalker::MulticastCollapse_Implementation()
{
	CollapseStart = GetWorld()->GetTimeSeconds();
	for (int32 i = 0; i < Legs; ++i)
	{
		DVFx::Explosion(GetWorld(), GetActorTransform().TransformPosition(HipLocal[i]), 2.f);
	}
	DVKit::SetGlow(Optic, FLinearColor(0.2f, 0.02f, 0.01f), 2.f);
	OpticLight->SetIntensity(0.f);
}

void ADVWalker::OnRep_State()
{
}

void ADVWalker::TickLegs(float DeltaSeconds)
{
	const FTransform Xf = GetActorTransform();
	const double Now = GetWorld()->GetTimeSeconds();
	const bool bWalking = State == EDVWalkerState::Walking;
	const bool bDown = State == EDVWalkerState::Collapsing || State == EDVWalkerState::Down;

	// Ramp follows the state on every machine.
	const float InState = HasAuthority() ? float(Now - StateTime) : 0.f;
	if (State == EDVWalkerState::Knelt)
	{
		SetRamp(HasAuthority() ? FMath::Clamp(InState / RampSeconds, 0.f, 1.f) : 1.f);
	}
	else if (State == EDVWalkerState::Rising)
	{
		SetRamp(HasAuthority() ? 1.f - FMath::Clamp(InState / RampSeconds, 0.f, 1.f) : 0.f);
	}
	else
	{
		SetRamp(bDown ? 1.f : 0.f);
	}

	// Gait: diagonal pairs swing in turn while it walks; planted otherwise.
	if (bWalking)
	{
		GaitClock += DeltaSeconds;
	}
	const int32 Pair = int32(GaitClock / StepSeconds) % 2;
	const float Phase = FMath::Fmod(GaitClock, StepSeconds) / StepSeconds;
	const FVector Fwd = Xf.GetRotation().GetForwardVector();
	for (int32 i = 0; i < Legs; ++i)
	{
		const bool bSwingLeg = bWalking && ((Pair == 0) == (i == 0 || i == 3));
		if (bSwingLeg)
		{
			if (FootSwing[i] < 0.f)
			{
				// Lift off: aim ahead of the rest pose by the distance the body covers in a step.
				FootFrom[i] = FootPlanted[i];
				FVector Aim = Xf.TransformPosition(RestLocal[i]) + Fwd * WalkSpeed * StepSeconds;
				Aim.Z = GroundZ(Aim);
				FootTo[i] = Aim;
			}
			FootSwing[i] = Phase;
			FootPlanted[i] = FMath::Lerp(FootFrom[i], FootTo[i], Ease(Phase)) + FVector(0.f, 0.f, FMath::Sin(Phase * PI) * StepLift);
		}
		else if (FootSwing[i] >= 0.f)
		{
			FootSwing[i] = -1.f;	// touch down
			FootPlanted[i] = FootTo[i];
			if (bWalking)
			{
				DVSound::At(GetWorld(), TEXT("SW_Explosion"), FootTo[i], 0.25f, 1, 0.3f);
			}
		}
	}

	// Body: a heavy heave with each step while striding.
	const float Bob = bWalking ? FMath::Sin(Phase * PI) : 0.f;
	Body->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -60.f * Bob), FRotator(0.f, 0.f, (Pair == 0 ? 1.f : -1.f) * 1.2f * Bob));

	// Two-bone legs, knees bending outward and up like a crab's.
	const float Spread = bDown ? FMath::Clamp((Now - CollapseStart) / CollapseSeconds, 0.f, 1.f) : 0.f;
	for (int32 i = 0; i < Legs; ++i)
	{
		const FVector Hip = Body->GetComponentTransform().TransformPosition(HipLocal[i]);
		const FVector Out = (Hip - GetActorLocation()).GetSafeNormal2D();
		FVector Foot = FootPlanted[i] + Out * 700.f * Spread;
		const FVector Ankle = Foot + FVector(0.f, 0.f, 70.f);
		FVector ToFoot = Ankle - Hip;
		const float D = FMath::Clamp(ToFoot.Size(), 200.f, Thigh + Shin - 10.f);
		const FVector Dir = ToFoot.GetSafeNormal();
		const float A = (Thigh * Thigh - Shin * Shin + D * D) / (2.f * D);
		const float H = FMath::Sqrt(FMath::Max(Thigh * Thigh - A * A, 0.f));
		FVector Bend = (Out + FVector(0.f, 0.f, 0.7f)) - Dir * FVector::DotProduct(Out + FVector(0.f, 0.f, 0.7f), Dir);
		Bend = Bend.GetSafeNormal();
		const FVector Knee = Hip + Dir * A + Bend * H;
		const FVector FootEnd = Knee + (Ankle - Knee).GetSafeNormal() * Shin;
		Upper[i]->SetWorldLocationAndRotation((Hip + Knee) * 0.5f, FRotationMatrix::MakeFromX(Knee - Hip).Rotator());
		Lower[i]->SetWorldLocationAndRotation((Knee + FootEnd) * 0.5f, FRotationMatrix::MakeFromX(FootEnd - Knee).Rotator());
		Knees[i]->SetWorldLocation(Knee);
		Feet[i]->SetWorldLocationAndRotation(FootEnd - FVector(0.f, 0.f, 10.f), FRotator(0.f, GetActorRotation().Yaw, 0.f));
	}
}
