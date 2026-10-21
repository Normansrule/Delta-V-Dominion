#include "Core/DVHollyhock.h"
#include "Core/DVFx.h"
#include "Characters/DVSoldierCharacter.h"
#include "Vehicles/DVVehicleKit.h"
#include "DeltaVDominion.h"

#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "Net/UnrealNetwork.h"

namespace
{
	UStaticMeshComponent* RuntimePart(AActor* Owner, USceneComponent* Parent, UStaticMesh* Mesh, UMaterialInterface* Material,
		const FVector& Loc, const FVector& SizeCm, const FRotator& Rot = FRotator::ZeroRotator)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner);
		C->SetupAttachment(Parent);
		C->SetStaticMesh(Mesh);
		C->SetMaterial(0, Material);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetRelativeLocationAndRotation(Loc, Rot);
		if (Mesh)
		{
			const FVector Extent = Mesh->GetBounds().BoxExtent * 2.f;
			C->SetRelativeScale3D(FVector(SizeCm.X / FMath::Max(Extent.X, 1.f), SizeCm.Y / FMath::Max(Extent.Y, 1.f), SizeCm.Z / FMath::Max(Extent.Z, 1.f)));
		}
		C->RegisterComponent();
		return C;
	}

	UStaticMeshComponent* Painted(AActor* Owner, USceneComponent* Parent, UStaticMesh* Mesh, const FVector& Loc, const FVector& SizeCm,
		const FLinearColor& Color, float Glow = 0.25f, const FRotator& Rot = FRotator::ZeroRotator)
	{
		UStaticMeshComponent* C = RuntimePart(Owner, Parent, Mesh, DVKit::Material(TEXT("Glow")), Loc, SizeCm, Rot);
		DVKit::SetGlow(C, Color, Glow);
		return C;
	}

	// Hollyhock's palette: rose, sage, butter, sky, lilac. Nothing in the vale is olive or gunmetal.
	const FLinearColor Pastels[] = {
		FLinearColor(1.f, 0.62f, 0.72f), FLinearColor(0.62f, 0.86f, 0.66f), FLinearColor(1.f, 0.86f, 0.45f),
		FLinearColor(0.6f, 0.78f, 1.f), FLinearColor(0.86f, 0.7f, 1.f) };
}

// ---------------------------------------------------------------------------------------------
// Windpump
// ---------------------------------------------------------------------------------------------

ADVRepairPoint::ADVRepairPoint()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Rotor = CreateDefaultSubobject<USceneComponent>(TEXT("Rotor"));
	Rotor->SetupAttachment(Root);
	Rotor->SetRelativeLocation(FVector(-70.f, 0.f, 820.f));
}

void ADVRepairPoint::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADVRepairPoint, Progress);
	DOREPLIFETIME(ADVRepairPoint, bFixed);
}

void ADVRepairPoint::BeginPlay()
{
	Super::BeginPlay();
	const FLinearColor Cream(0.95f, 0.88f, 0.74f);
	const FLinearColor Wood(0.42f, 0.27f, 0.16f);
	// A stone footing, a tapering timber tower, a little roofed cap and a four-sail rotor facing -X.
	Painted(this, Root, DVKit::Cylinder(), FVector(0.f, 0.f, 40.f), FVector(260.f, 260.f, 80.f), FLinearColor(0.6f, 0.6f, 0.56f), 0.15f);
	for (int32 k = 0; k < 4; ++k)
	{
		const FVector Foot((k & 1) ? 80.f : -80.f, (k & 2) ? 80.f : -80.f, 0.f);
		Painted(this, Root, DVKit::Box(), Foot * 0.6f + FVector(0.f, 0.f, 440.f), FVector(16.f, 16.f, 760.f), Wood, 0.15f,
			FRotator(Foot.Y > 0.f ? -5.f : 5.f, 0.f, Foot.X > 0.f ? 5.f : -5.f));
	}
	for (int32 h = 1; h < 4; ++h)
	{
		Painted(this, Root, DVKit::Box(), FVector(0.f, 0.f, h * 200.f), FVector(150.f - h * 14.f, 150.f - h * 14.f, 10.f), Wood, 0.15f);
	}
	Painted(this, Root, DVKit::Box(), FVector(0.f, 0.f, 820.f), FVector(120.f, 110.f, 90.f), Cream, 0.3f);
	Painted(this, Root, DVKit::Box(), FVector(0.f, 0.f, 880.f), FVector(150.f, 140.f, 20.f), FLinearColor(0.85f, 0.35f, 0.4f), 0.3f);
	Painted(this, Rotor, DVKit::Cylinder(), FVector::ZeroVector, FVector(30.f, 30.f, 30.f), Wood, 0.15f, FRotator(90.f, 0.f, 0.f));
	for (int32 b = 0; b < 4; ++b)
	{
		const float A = b * 90.f;
		const FVector Out = FRotator(0.f, 0.f, A).RotateVector(FVector(0.f, 0.f, 1.f));
		UStaticMeshComponent* Sail = Painted(this, Rotor, DVKit::Box(), Out * 210.f, FVector(6.f, 60.f, 360.f), b % 2 ? Cream : FLinearColor(1.f, 0.7f, 0.76f), 0.35f, FRotator(0.f, 0.f, A));
		if (b == 3)
		{
			LooseBlade = Sail;	// the storm snapped this one; it hangs until the pump is fixed
		}
	}
	Lamp = Painted(this, Root, DVKit::Sphere(), FVector(-70.f, 0.f, 920.f), FVector(30.f, 30.f, 30.f), FLinearColor(1.f, 0.4f, 0.1f), 10.f);
	Spark = Painted(this, Root, DVKit::Sphere(), FVector(-60.f, 30.f, 760.f), FVector(16.f, 16.f, 16.f), FLinearColor(1.f, 0.7f, 0.3f), 30.f);
	Light = NewObject<UPointLightComponent>(this);
	Light->SetupAttachment(Root);
	Light->SetRelativeLocation(FVector(-120.f, 0.f, 900.f));
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(120.f);
	Light->SetAttenuationRadius(700.f);
	Light->SetCastShadows(false);
	Light->RegisterComponent();
	OnRep_Fixed();
}

void ADVRepairPoint::OnRep_Fixed()
{
	if (!Lamp)
	{
		return;
	}
	const FLinearColor C = bFixed ? FLinearColor(0.3f, 1.f, 0.45f) : FLinearColor(1.f, 0.4f, 0.1f);
	DVKit::SetGlow(Lamp, C, 10.f);
	Light->SetLightColor(C);
	Spark->SetVisibility(!bFixed);
	if (LooseBlade)
	{
		LooseBlade->SetRelativeLocationAndRotation(bFixed ? FVector(0.f, -210.f, 0.f) : FVector(0.f, -120.f, -150.f), FRotator(0.f, 0.f, bFixed ? 270.f : 200.f));
	}
	if (bFixed)
	{
		DVFx::Flash(GetWorld(), GetActorLocation() + FVector(0.f, 0.f, 820.f), FLinearColor(0.5f, 1.f, 0.6f), 1200.f, 900.f, 0.4f);
		DVSound::At(GetWorld(), TEXT("SW_PlantBeep"), GetActorLocation(), 0.9f, 1, 0.f);
	}
}

void ADVRepairPoint::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority() && !bFixed)
	{
		bool bWorking = false;
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			const ADVSoldierCharacter* S = It->Get() ? Cast<ADVSoldierCharacter>(It->Get()->GetPawn()) : nullptr;
			if (S && !S->IsOutOfAction() && FVector::Dist2D(S->GetActorLocation(), GetActorLocation()) < Reach
				&& FMath::Abs(S->GetActorLocation().Z - GetActorLocation().Z) < 400.f)
			{
				bWorking = true;
				break;
			}
		}
		Progress = FMath::Clamp(Progress + DeltaSeconds * (bWorking ? 1.f / RepairSeconds : -0.05f), 0.f, 1.f);
		if (Progress >= 1.f)
		{
			bFixed = true;
			OnRep_Fixed();
		}
	}
	// Broken: the rotor rocks in the wind and the loose wire sparks. Fixed: it spins up and turns steadily.
	const float Now = GetWorld()->GetTimeSeconds();
	RotorSpeed = FMath::FInterpTo(RotorSpeed, bFixed ? 110.f : 0.f, DeltaSeconds, 0.8f);
	RotorAngle += RotorSpeed * DeltaSeconds;
	Rotor->SetRelativeRotation(FRotator(0.f, 0.f, RotorAngle + (bFixed ? 0.f : FMath::Sin(Now * 1.3f) * 9.f)));
	if (Spark && !bFixed)
	{
		Spark->SetVisibility(FMath::Frac(Now * 3.7f + GetActorLocation().X) < 0.25f || Progress > 0.f);
	}
}

// ---------------------------------------------------------------------------------------------
// Pen
// ---------------------------------------------------------------------------------------------

ADVPen::ADVPen()
{
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

ADVPen* ADVPen::Find(const UWorld* World)
{
	if (World)
	{
		for (TActorIterator<ADVPen> It(const_cast<UWorld*>(World)); It; ++It)
		{
			return *It;
		}
	}
	return nullptr;
}

// ---------------------------------------------------------------------------------------------
// Puffbot
// ---------------------------------------------------------------------------------------------

ADVPuffbot::ADVPuffbot()
{
	PrimaryActorTick.bCanEverTick = true;
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	bUseControllerRotationYaw = false;
	GetCapsuleComponent()->InitCapsuleSize(48.f, 50.f);
	// Bullets and foam pass it by: it isn't part of any fight.
	GetCapsuleComponent()->SetCollisionResponseToChannel(DV_TRACE_WEAPON, ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetVisibility(false);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0.f, 540.f, 0.f);
	Move->MaxWalkSpeed = 140.f;
	Move->bUseRVOAvoidance = true;
	Visual = CreateDefaultSubobject<USceneComponent>(TEXT("Visual"));
	Visual->SetupAttachment(GetCapsuleComponent());
	Visual->SetRelativeLocation(FVector(0.f, 0.f, -50.f));
}

void ADVPuffbot::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADVPuffbot, bPenned);
}

void ADVPuffbot::BeginPlay()
{
	Super::BeginPlay();
	Home = GetActorLocation();
	Seed = GetTypeHash(GetName());
	FRandomStream Rng(Seed);
	const FLinearColor Shell = Pastels[Rng.RandRange(0, UE_ARRAY_COUNT(Pastels) - 1)];
	// A round body, a cream face band, two big eyes, a stubby antenna with a glowing tip, and three little feet.
	Body = Painted(this, Visual, DVKit::Sphere(), FVector(0.f, 0.f, 58.f), FVector(100.f, 100.f, 92.f), Shell, 0.35f);
	Painted(this, Visual, DVKit::Cylinder(), FVector(0.f, 0.f, 58.f), FVector(102.f, 102.f, 22.f), FLinearColor(0.97f, 0.92f, 0.8f), 0.3f);
	for (int32 Side = -1; Side <= 1; Side += 2)
	{
		Painted(this, Visual, DVKit::Sphere(), FVector(40.f, Side * 18.f, 72.f), FVector(22.f, 22.f, 26.f), FLinearColor(1.f, 1.f, 1.f), 1.5f);
		Painted(this, Visual, DVKit::Sphere(), FVector(50.f, Side * 18.f, 72.f), FVector(8.f, 11.f, 13.f), FLinearColor(0.02f, 0.02f, 0.03f), 0.f);
	}
	Painted(this, Visual, DVKit::Cylinder(), FVector(0.f, 0.f, 112.f), FVector(5.f, 5.f, 30.f), FLinearColor(0.3f, 0.3f, 0.32f), 0.1f);
	Tip = Painted(this, Visual, DVKit::Sphere(), FVector(0.f, 0.f, 130.f), FVector(14.f, 14.f, 14.f), Shell, 8.f);
	for (int32 k = 0; k < 3; ++k)
	{
		const FVector Foot = FRotator(0.f, k * 120.f, 0.f).RotateVector(FVector(26.f, 0.f, 8.f));
		Painted(this, Visual, DVKit::Sphere(), Foot, FVector(24.f, 24.f, 16.f), FLinearColor(0.35f, 0.3f, 0.3f), 0.1f);
	}
	NextThink = GetWorld()->GetTimeSeconds() + Rng.FRandRange(0.2f, 1.5f);
}

void ADVPuffbot::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Now = GetWorld()->GetTimeSeconds();
	const float Speed = GetVelocity().Size2D();
	// Penned: happy hops. Running: a quick waddle-bounce. Idle: a slow breathe.
	const float Hop = bPenned ? FMath::Abs(FMath::Sin(Now * 5.f + Seed % 7)) * 26.f
		: Speed > 200.f ? FMath::Abs(FMath::Sin(Now * 14.f)) * 12.f : FMath::Sin(Now * 2.f) * 2.f;
	Visual->SetRelativeLocation(FVector(0.f, 0.f, -50.f + Hop));
	if (!HasAuthority() || bPenned)
	{
		return;
	}
	if (const ADVPen* Pen = ADVPen::Find(GetWorld()); Pen && Pen->Contains(GetActorLocation()))
	{
		bPenned = true;
		if (AAIController* AI = Cast<AAIController>(GetController()))
		{
			AI->StopMovement();
		}
		DVFx::Flash(GetWorld(), GetActorLocation() + FVector(0.f, 0.f, 80.f), FLinearColor(1.f, 0.8f, 0.9f), 600.f, 400.f, 0.3f);
		DVSound::At(GetWorld(), TEXT("SW_UIClick"), GetActorLocation(), 0.9f, 0, 0.3f);
		return;
	}
	if (Now < NextThink)
	{
		return;
	}
	NextThink = Now + 0.38;
	// Who's closest? Anyone inside Skittish sends it running directly away from them.
	const APawn* Threat = nullptr;
	float ThreatDist = Skittish;
	for (TActorIterator<ADVSoldierCharacter> It(GetWorld()); It; ++It)
	{
		const float D = FVector::Dist2D(It->GetActorLocation(), GetActorLocation());
		if (It->IsPlayerControlled() && !It->IsOutOfAction() && D < ThreatDist)	// your squadmates know better than to chase them
		{
			ThreatDist = D;
			Threat = *It;
		}
	}
	AAIController* AI = Cast<AAIController>(GetController());
	UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!AI || !Nav)
	{
		return;
	}
	FNavLocation Spot;
	if (Threat)
	{
		FVector Away = (GetActorLocation() - Threat->GetActorLocation()).GetSafeNormal2D();
		// Homesick: when running roughly the right way anyway, it drifts toward the barn.
		if (const ADVPen* Barn = ADVPen::Find(GetWorld()))
		{
			const FVector ToPen = (Barn->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
			if (FVector::DotProduct(Away, ToPen) > -0.2f)
			{
				Away = (Away * 0.65f + ToPen * 0.35f).GetSafeNormal2D();
			}
		}
		GetCharacterMovement()->MaxWalkSpeed = 330.f;	// a little slower than a walking soldier, so you can keep up
		// Straight away if it can; otherwise veer left or right, like a sheep at a fence.
		for (const float Veer : { 0.f, 35.f, -35.f, 70.f, -70.f })
		{
			const FVector Dir = FRotator(0.f, Veer, 0.f).RotateVector(Away);
			if (Nav->ProjectPointToNavigation(GetActorLocation() + Dir * 450.f, Spot, FVector(120.f, 120.f, 400.f))
				&& FVector::Dist2D(Spot.Location, GetActorLocation()) > 250.f)
			{
				AI->MoveToLocation(Spot.Location, 30.f, false);
				break;
			}
		}
	}
	else if (AI->GetMoveStatus() == EPathFollowingStatus::Idle && FMath::FRand() < 0.12f)
	{
		GetCharacterMovement()->MaxWalkSpeed = 110.f;
		if (Nav->GetRandomReachablePointInRadius(GetActorLocation(), 400.f, Spot))
		{
			AI->MoveToLocation(Spot.Location, 30.f, false);
		}
	}
}
