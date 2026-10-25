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
	Move->bRunPhysicsWithNoController = true;
	Move->bUseRVOAvoidance = false;	// avoidance would make it politely stop for the very soldier herding it
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
	if (HasAuthority() && !GetController())
	{
		SpawnDefaultController();	// a placed puffbot can miss its automatic AI controller; without one it never moves
	}
	// Stand it on the walkable ground, in case it was placed on a ledge or a slab the navmesh doesn't cover.
	FNavLocation OnNav;
	UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(GetWorld());
	if (HasAuthority() && Nav && Nav->ProjectPointToNavigation(GetActorLocation(), OnNav, FVector(300.f, 300.f, 300.f)))
	{
		SetActorLocation(OnNav.Location + FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.f));
	}
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
		WantDir = FVector::ZeroVector;
		if (AAIController* AI = Cast<AAIController>(GetController()))
		{
			// Make room at the gate: settle somewhere toward the back of the pen.
			const float A = FMath::FRandRange(100.f, 260.f);
			AI->MoveToLocation(Pen->GetActorLocation() + FRotator(0.f, A, 0.f).RotateVector(FVector(FMath::FRandRange(80.f, 300.f), 0.f, 0.f)), 40.f, false);
		}
		DVFx::Flash(GetWorld(), GetActorLocation() + FVector(0.f, 0.f, 80.f), FLinearColor(1.f, 0.8f, 0.9f), 600.f, 400.f, 0.3f);
		DVSound::At(GetWorld(), TEXT("SW_UIClick"), GetActorLocation(), 0.9f, 0, 0.3f);
		return;
	}
	// Pathfinding can't always help (a puffbot standing on a ledge or a slab the navmesh missed): if it wants to go
	// somewhere and hasn't moved, it waddles there directly, turning a little more each time it bumps into something.
	if (!WantDir.IsNearlyZero())
	{
		if (Speed > 60.f)
		{
			StuckSince = -1.0;
			StuckTurns = 0;
		}
		else if (StuckSince < 0.0)
		{
			StuckSince = Now;
		}
		else if (Now - StuckSince > 0.5)
		{
			const float Turn = (StuckTurns % 2 ? -1.f : 1.f) * 30.f * ((StuckTurns + 1) / 2);
			AddMovementInput(FRotator(0.f, Turn, 0.f).RotateVector(WantDir), 1.f);
			if (Now - StuckSince > 1.6)
			{
				StuckSince = Now - 0.5;
				StuckTurns = (StuckTurns + 1) % 7;
			}
		}
	}
	if (Now < NextThink)
	{
		return;
	}
	NextThink = Now + 0.3;
	// Who's closest? A soldier who walks up gently is a friend to follow; one who comes sprinting is a fright.
	const APawn* Threat = nullptr;
	const APawn* Friend = nullptr;
	float ThreatDist = Skittish;
	for (TActorIterator<ADVSoldierCharacter> It(GetWorld()); It; ++It)
	{
		const float D = FVector::Dist2D(It->GetActorLocation(), GetActorLocation());
		if (!It->IsPlayerControlled() || It->IsOutOfAction() || D >= ThreatDist)	// your squadmates know better than to chase them
		{
			continue;
		}
		ThreatDist = D;
		const bool bRushing = It->GetVelocity().Size2D() > Scary;
		Threat = bRushing ? *It : nullptr;
		Friend = !bRushing && D < Befriend ? *It : nullptr;
	}
	AAIController* AI = Cast<AAIController>(GetController());
	UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!AI || !Nav)
	{
		return;
	}
	FNavLocation Spot;
	// Close enough to see the barn: it remembers where home is and trots in by itself.
	if (const ADVPen* Barn = ADVPen::Find(GetWorld()); Barn && FVector::Dist2D(Barn->GetActorLocation(), GetActorLocation()) < Barn->Radius + 1200.f)
	{
		Leader = nullptr;
		GetCharacterMovement()->MaxWalkSpeed = 260.f;
		AI->MoveToLocation(Barn->GetActorLocation(), 60.f, false);
		// The gate faces +X: aim for it while outside, then for the middle.
		const FVector Gate = Barn->GetActorLocation() + FVector(Barn->Radius + 120.f, 0.f, 0.f);
		const FVector FromMiddle = GetActorLocation() - Barn->GetActorLocation();
		const bool bAtGate = FromMiddle.X > 0.f && FMath::Abs(FromMiddle.Y) < 200.f;	// lined up with the gate: straight in
		// Behind the pen: round the fence on whichever side it's already on, then along to the gate.
		const FVector Round = Barn->GetActorLocation() + FVector(0.f, (FromMiddle.Y >= 0.f ? 1.f : -1.f) * (Barn->Radius + 260.f), 0.f);
		Want((bAtGate ? Barn->GetActorLocation() : FromMiddle.X < 0.f ? Round : Gate) - GetActorLocation());
		return;
	}
	if (Friend && !Leader.IsValid())
	{
		Leader = Friend;	// a gentle soldier: it waddles after them like a duckling
		DVSound::At(GetWorld(), TEXT("SW_UIClick"), GetActorLocation(), 0.7f, 0, 0.4f);
	}
	if (Threat)
	{
		Leader = nullptr;	// sprinting at it breaks the trust; it scatters
	}
	if (const APawn* Following = Leader.Get(); Following && !Threat)
	{
		if (FVector::Dist2D(Following->GetActorLocation(), GetActorLocation()) > 2600.f)
		{
			Leader = nullptr;	// lost them
		}
		else
		{
			GetCharacterMovement()->MaxWalkSpeed = 460.f;	// keeps up with a walking soldier
			AI->MoveToActor(const_cast<APawn*>(Following), 170.f, true, true, false);
			Want(FVector::Dist2D(Following->GetActorLocation(), GetActorLocation()) > 260.f ? Following->GetActorLocation() - GetActorLocation() : FVector::ZeroVector);
			return;
		}
	}
	if (Threat)
	{
		FVector Away = (GetActorLocation() - Threat->GetActorLocation()).GetSafeNormal2D();
		// Homesick: when running roughly the right way anyway, it drifts toward the barn.
		if (const ADVPen* Barn = ADVPen::Find(GetWorld()))
		{
			const FVector ToPen = (Barn->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
			const float FromPen = FVector::Dist2D(Barn->GetActorLocation(), GetActorLocation());
			if (FromPen > 3200.f)
			{
				// The hedges at the edge of the vale frighten it more than you do: it won't run off into the hills.
				Away = (Away * 0.2f + ToPen * 0.8f).GetSafeNormal2D();
			}
			else if (FVector::DotProduct(Away, ToPen) > -0.35f)
			{
				Away = (Away * 0.5f + ToPen * 0.5f).GetSafeNormal2D();
			}
		}
		GetCharacterMovement()->MaxWalkSpeed = 330.f;	// a little slower than a walking soldier, so you can keep up
		Want(Away);
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
	else
	{
		WantDir = FVector::ZeroVector;	// nobody near: calm again, and potter about now and then
		if (AI->GetMoveStatus() == EPathFollowingStatus::Idle && FMath::FRand() < 0.12f)
		{
			GetCharacterMovement()->MaxWalkSpeed = 110.f;
			if (Nav->GetRandomReachablePointInRadius(GetActorLocation(), 400.f, Spot))
			{
				AI->MoveToLocation(Spot.Location, 30.f, false);
			}
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Festival fireworks
// ---------------------------------------------------------------------------------------------

ADVFirework::ADVFirework()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ADVFirework::Burst(const FLinearColor& Color)
{
	// Two dozen glowing petals thrown out in a sphere; they fall a little and shrink away, like a chrysanthemum shell.
	for (int32 i = 0; i < 24; ++i)
	{
		UStaticMeshComponent* S = Painted(this, RootComponent, DVKit::Sphere(), FVector::ZeroVector, FVector(40.f, 40.f, 40.f), i % 5 ? Color : FLinearColor(1.f, 0.95f, 0.85f), 60.f);
		S->SetCastShadow(false);
		Sparks.Add(S);
		BaseScale = S->GetRelativeScale3D();
		Velocities.Add(FMath::VRand() * FMath::FRandRange(700.f, 1000.f));
	}
	SetLifeSpan(Life + 0.2f);
}

void ADVFirework::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	const float Fade = FMath::Clamp(1.f - Age / Life, 0.f, 1.f);
	for (int32 i = 0; i < Sparks.Num(); ++i)
	{
		Velocities[i] *= FMath::Exp(-DeltaSeconds * 1.6f);	// air drag
		Velocities[i].Z -= 300.f * DeltaSeconds;
		Sparks[i]->AddRelativeLocation(Velocities[i] * DeltaSeconds);
		Sparks[i]->SetRelativeScale3D(BaseScale * (0.25f + 0.75f * Fade));
	}
}
