#include "Vehicles/DVBoat.h"
#include "Vehicles/DVVehicleKit.h"
#include "Characters/DVSoldierCharacter.h"
#include "Core/DVFx.h"
#include "Core/DVGameState.h"
#include "DeltaVDominion.h"

#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"

namespace
{
	constexpr float GunInterval = 0.16f;
	constexpr float MissileInterval = 2.5f;
}

ADVBoat::ADVBoat()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);

	Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
	Collision->SetBoxExtent(FVector(560.f, 170.f, 90.f));
	Collision->SetCollisionProfileName(TEXT("Pawn"));
	Collision->SetCollisionResponseToChannel(DV_TRACE_WEAPON, ECR_Block);
	RootComponent = Collision;
	Body = CreateDefaultSubobject<USceneComponent>(TEXT("Body"));
	Body->SetupAttachment(Collision);

	Seat = CreateDefaultSubobject<UDVSeatComponent>(TEXT("Seat"));
	Seat->VehicleName = TEXT("PB-9 Marlin missile boat");
	Seat->WeaponName = TEXT("30 mm deck gun  |  RMB missiles");
	Seat->EnterRadius = 750.f;
	Seat->ExitOffset = FVector(-100.f, 0.f, 180.f);	// step out onto the deck
	Seat->MaxHealth = 1300.f;
	Seat->ArmourFraction = 0.35f;

	CameraArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraArm"));
	CameraArm->SetupAttachment(Collision);
	CameraArm->SetRelativeLocation(FVector(0.f, 0.f, 300.f));
	CameraArm->TargetArmLength = 1400.f;
	CameraArm->bUsePawnControlRotation = true;
	CameraArm->bEnableCameraLag = true;
	CameraArm->CameraLagSpeed = 6.f;
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraArm);

	// A sleek, angular missile boat: knife bow, low cabin, a hinged launcher block amidships, deck gun forward.
	UMaterialInterface* Hull = DVKit::Material(TEXT("Concrete"));
	UMaterialInterface* Dark = DVKit::Material(TEXT("Gunmetal"));
	UMaterialInterface* Navy = DVKit::Material(TEXT("Navy"));
	UMaterialInterface* Glow = DVKit::Material(TEXT("Glow"));
	DVKit::Part(this, Body, TEXT("Hull"), DVKit::Box(), Hull, FVector(-60.f, 0.f, -20.f), FRotator::ZeroRotator, FVector(900.f, 320.f, 120.f));
	DVKit::Part(this, Body, TEXT("Bow"), DVKit::Box(), Hull, FVector(470.f, 0.f, -10.f), FRotator(0.f, 45.f, 0.f), FVector(230.f, 230.f, 110.f));
	DVKit::Part(this, Body, TEXT("Waterline"), DVKit::Box(), Navy, FVector(0.f, 0.f, -70.f), FRotator::ZeroRotator, FVector(980.f, 330.f, 30.f));
	DVKit::Part(this, Body, TEXT("Cabin"), DVKit::Box(), Dark, FVector(-80.f, 0.f, 90.f), FRotator(0.f, 0.f, 0.f), FVector(300.f, 220.f, 110.f));
	DVKit::Part(this, Body, TEXT("Glass"), DVKit::Box(), Glow, FVector(75.f, 0.f, 110.f), FRotator(-35.f, 0.f, 0.f), FVector(10.f, 200.f, 60.f));
	DVKit::Part(this, Body, TEXT("Mast"), DVKit::Box(), Dark, FVector(-160.f, 0.f, 210.f), FRotator::ZeroRotator, FVector(20.f, 20.f, 160.f));
	DVKit::Part(this, Body, TEXT("Radar"), DVKit::Box(), Dark, FVector(-160.f, 0.f, 290.f), FRotator::ZeroRotator, FVector(30.f, 120.f, 16.f));
	DVKit::Part(this, Body, TEXT("GunMount"), DVKit::Cylinder(), Dark, FVector(300.f, 0.f, 70.f), FRotator::ZeroRotator, FVector(90.f, 90.f, 50.f));
	DVKit::Part(this, Body, TEXT("GunBarrel"), DVKit::Cylinder(), Dark, FVector(400.f, 0.f, 90.f), FRotator(90.f, 0.f, 0.f), FVector(14.f, 14.f, 180.f));
	LauncherDoors = CreateDefaultSubobject<USceneComponent>(TEXT("LauncherDoors"));
	LauncherDoors->SetupAttachment(Body);
	LauncherDoors->SetRelativeLocation(FVector(-330.f, 0.f, 60.f));
	DVKit::Part(this, LauncherDoors, TEXT("Launcher"), DVKit::Box(), Navy, FVector(0.f, 0.f, 20.f), FRotator::ZeroRotator, FVector(200.f, 240.f, 70.f));
	for (int32 i = 0; i < 4; ++i)
	{
		DVKit::Part(this, LauncherDoors, *FString::Printf(TEXT("Cell%d"), i), DVKit::Box(), Glow, FVector(-50.f + (i / 2) * 100.f, (i % 2 ? 55.f : -55.f), 57.f), FRotator::ZeroRotator, FVector(70.f, 70.f, 4.f));
	}
	for (int32 i = 0; i < 3; ++i)
	{
		Wake.Add(DVKit::Part(this, Collision, *FString::Printf(TEXT("Wake%d"), i), DVKit::Box(), Glow, FVector(-520.f - i * 220.f, 0.f, -85.f), FRotator::ZeroRotator, FVector(200.f, 260.f - i * 40.f, 4.f)));
	}
}

void ADVBoat::BeginPlay()
{
	Super::BeginPlay();
	TargetYaw = GetActorRotation().Yaw;
	WaterZ = GetActorLocation().Z - 60.f;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(TEXT("DV_Water")))
		{
			WaterZ = It->GetActorLocation().Z;
			break;
		}
	}
	EngineSound = DVSound::Loop(GetWorld(), GetRootComponent(), TEXT("SW_EngineGround"), 0.5f);
	TArray<UStaticMeshComponent*> Parts;
	GetComponents(Parts);
	for (UStaticMeshComponent* P : Parts)
	{
		if (P->GetName() == TEXT("Glass"))
		{
			DVKit::SetGlow(P, FLinearColor(0.2f, 0.5f, 0.7f), 1.5f);
		}
		else if (P->GetName().StartsWith(TEXT("Cell")))
		{
			DVKit::SetGlow(P, FLinearColor(1.f, 0.3f, 0.1f), 6.f);
		}
	}
}

float ADVBoat::GetMissileReadyFraction() const
{
	return FMath::Clamp(static_cast<float>((GetWorld()->GetTimeSeconds() - LastMissile) / MissileInterval), 0.f, 1.f);
}

void ADVBoat::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double Now = GetWorld()->GetTimeSeconds();

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC && IsLocallyControlled())
	{
		float Throttle = (PC->IsInputKeyDown(EKeys::W) ? 1.f : 0.f) - (PC->IsInputKeyDown(EKeys::S) ? 0.5f : 0.f);
		if (FMath::IsNearlyZero(Throttle) && !FMath::IsNearlyZero(AutoThrottle))
		{
			Throttle = AutoThrottle;
			FRotator Steer = PC->GetControlRotation();
			Steer.Yaw += AutoYawRate * DeltaSeconds;
			PC->SetControlRotation(Steer);
		}
		ServerInput(Throttle, PC->GetControlRotation().Yaw);
		const FVector Start = PC->PlayerCameraManager->GetCameraLocation();
		const FVector End = Start + PC->PlayerCameraManager->GetActorForwardVector() * 30000.f;
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(DVBoatAim), false, this);
		const FVector Aim = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, DV_TRACE_WEAPON, Q) ? FVector(Hit.ImpactPoint) : End;
		if ((PC->IsInputKeyDown(EKeys::LeftMouseButton) || bAutoFire) && Now - LastGun >= GunInterval)
		{
			LastGun = Now;
			ServerFire(false, Aim);
		}
		if (PC->IsInputKeyDown(EKeys::RightMouseButton) && Now - LastMissile >= MissileInterval)
		{
			LastMissile = Now;
			ServerFire(true, Aim);
		}
	}

	if (HasAuthority())
	{
		const FRotator Heading(0.f, GetActorRotation().Yaw, 0.f);
		const FVector Fwd = Heading.Vector();
		FVector Planar(Velocity.X, Velocity.Y, 0.f);
		const float Along = FVector::DotProduct(Planar, Fwd);
		const FVector Side = Planar - Fwd * Along;
		Planar = Fwd * FMath::FInterpTo(Along, ThrottleInput * TopSpeed, DeltaSeconds, 0.7f) + Side * FMath::Max(0.f, 1.f - 2.5f * DeltaSeconds);
		Velocity = FVector(Planar.X, Planar.Y, 0.f);
		// Ride the swell: a little heave, more at speed.
		const float Bob = FMath::Sin(Now * 1.3f) * 12.f + FMath::Sin(Now * 2.1f + 1.f) * 6.f;
		const FVector Target(GetActorLocation().X, GetActorLocation().Y, WaterZ + 60.f + Bob);
		SetActorLocation(Target, false);
		FHitResult Hit;
		AddActorWorldOffset(Velocity * DeltaSeconds, true, &Hit);
		if (Hit.bBlockingHit)
		{
			Velocity = FVector::VectorPlaneProject(Velocity, Hit.ImpactNormal) * 0.3f;	// ran aground or hit a pier
		}
		// Turning needs way on: a stationary hull barely swings.
		const float TurnRate = 18.f + 40.f * FMath::Clamp(FMath::Abs(Along) / TopSpeed, 0.f, 1.f);
		SetActorRotation(FRotator(0.f, FMath::FixedTurn(GetActorRotation().Yaw, TargetYaw, TurnRate * DeltaSeconds), 0.f));
	}

	// Bow rises with speed, hull rolls into turns, the wake glows with throttle.
	const float Speed01 = FMath::Clamp(Velocity.Size() / TopSpeed, 0.f, 1.f);
	const float YawDelta = FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, TargetYaw);
	const FRotator Pose(4.f * Speed01 + FMath::Sin(Now * 1.7f) * 1.5f, 0.f, FMath::Clamp(-YawDelta / 30.f, -1.f, 1.f) * 10.f * Speed01 + FMath::Sin(Now * 1.1f) * 2.f);
	Body->SetRelativeRotation(FMath::RInterpTo(Body->GetRelativeRotation(), Pose, DeltaSeconds, 3.f));
	for (int32 i = 0; i < Wake.Num(); ++i)
	{
		Wake[i]->SetVisibility(Speed01 > 0.08f);
		DVKit::SetGlow(Wake[i], FLinearColor(0.7f, 0.85f, 0.9f), 1.2f * Speed01 / (i + 1));
	}
	// Launcher doors swing open for a second after each missile.
	const float Open = Now - LastMissile < 1.2 ? 1.f : 0.f;
	LauncherDoors->SetRelativeRotation(FMath::RInterpTo(LauncherDoors->GetRelativeRotation(), FRotator(Open * 35.f, 0.f, 0.f), DeltaSeconds, 6.f));
	if (EngineSound)
	{
		EngineSound->SetPitchMultiplier(0.7f + 0.6f * Speed01);
		EngineSound->SetVolumeMultiplier(GetController() ? 0.6f : 0.15f);
	}
}

void ADVBoat::ServerInput_Implementation(float Throttle, float Yaw)
{
	ThrottleInput = FMath::Clamp(Throttle, -1.f, 1.f);
	TargetYaw = Yaw;
}

void ADVBoat::ServerFire_Implementation(bool bMissile, FVector_NetQuantize AimPoint)
{
	const double Now = GetWorld()->GetTimeSeconds();
	double& Last = bMissile ? LastMissile : LastGun;
	if (!IsLocallyControlled() && Now - Last < (bMissile ? MissileInterval : GunInterval) * 0.9)
	{
		return;
	}
	Last = Now;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(DVBoatShot), false, this);
	if (Seat->Driver)
	{
		Q.AddIgnoredActor(Seat->Driver);
	}
	const int32 Shots = bMissile ? 2 : 1;
	for (int32 i = 0; i < Shots; ++i)
	{
		const FVector Muzzle = bMissile ? GetActorTransform().TransformPosition(FVector(-330.f, MissileSide * 60.f, 160.f))
			: GetActorTransform().TransformPosition(FVector(500.f, 0.f, 90.f));
		MissileSide = -MissileSide;
		const FVector Dir = FMath::VRandCone((FVector(AimPoint) - Muzzle).GetSafeNormal(), FMath::DegreesToRadians(bMissile ? 0.6f : 1.4f));
		FHitResult Hit;
		const FVector End = Muzzle + Dir * 30000.f;
		const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Muzzle, End, DV_TRACE_WEAPON, Q);
		const FVector Impact = bHit ? FVector(Hit.ImpactPoint) : End;
		if (bHit && Seat->Driver)
		{
			DVCombat::RadialDamage(Seat->Driver, Impact, bMissile ? 650.f : 180.f, bMissile ? 380.f : 75.f, false);
		}
		MulticastShot(Muzzle, Impact, bMissile);
	}
}

void ADVBoat::MulticastShot_Implementation(FVector_NetQuantize Muzzle, FVector_NetQuantize Impact, bool bMissile)
{
	UWorld* World = GetWorld();
	DVFx::Flash(World, Muzzle, FLinearColor(1.f, 0.6f, 0.25f), bMissile ? 8000.f : 3000.f, 1200.f, 0.06f);
	DVFx::Beam(World, Muzzle, Impact, bMissile ? FLinearColor(60.f, 40.f, 30.f) : FLinearColor(90.f, 40.f, 8.f), bMissile ? 14.f : 6.f, bMissile ? 0.25f : 0.05f);
	DVSound::At(World, bMissile ? TEXT("SW_Thruster") : TEXT("SW_Cannon30"), Muzzle, 0.9f, 1);
	DVFx::Explosion(World, Impact, bMissile ? 0.45f : 0.12f);
}

// ---------------------------------------------------------------------------
// Transport ship
// ---------------------------------------------------------------------------

ADVTransportShip::ADVTransportShip()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	// A blunt civilian evacuation transport: white hull, a tall bridge aft, rows of lit portholes.
	UMaterialInterface* White = DVKit::Material(TEXT("Concrete"));
	UMaterialInterface* Dark = DVKit::Material(TEXT("Navy"));
	UMaterialInterface* Glow = DVKit::Material(TEXT("Glow"));
	DVKit::Part(this, Root, TEXT("Hull"), DVKit::Box(), White, FVector(0.f, 0.f, 200.f), FRotator::ZeroRotator, FVector(4200.f, 900.f, 700.f));
	DVKit::Part(this, Root, TEXT("Bow"), DVKit::Box(), White, FVector(2300.f, 0.f, 240.f), FRotator(0.f, 45.f, 0.f), FVector(640.f, 640.f, 620.f));
	DVKit::Part(this, Root, TEXT("Keel"), DVKit::Box(), Dark, FVector(0.f, 0.f, -110.f), FRotator::ZeroRotator, FVector(4300.f, 920.f, 120.f));
	DVKit::Part(this, Root, TEXT("Bridge"), DVKit::Box(), White, FVector(-1400.f, 0.f, 900.f), FRotator::ZeroRotator, FVector(800.f, 820.f, 700.f));
	DVKit::Part(this, Root, TEXT("Funnel"), DVKit::Cylinder(), Dark, FVector(-1700.f, 0.f, 1450.f), FRotator::ZeroRotator, FVector(260.f, 260.f, 500.f));
	for (int32 Side = -1; Side <= 1; Side += 2)
	{
		for (int32 Row = 0; Row < 2; ++Row)
		{
			Lights.Add(DVKit::Part(this, Root, *FString::Printf(TEXT("Ports%d%d"), Side + 1, Row), DVKit::Box(), Glow,
				FVector(200.f, Side * 455.f, 300.f + Row * 180.f), FRotator::ZeroRotator, FVector(3200.f, 6.f, 40.f)));
		}
	}
}

void ADVTransportShip::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	for (UStaticMeshComponent* L : Lights)
	{
		DVKit::SetGlow(L, FLinearColor(1.f, 0.85f, 0.6f), 6.f);
	}
	const ADVGameState* GS = GetWorld()->GetGameState<ADVGameState>();
	const bool bGo = GS && GS->bMultiStage && (GS->Stage == EDVMissionStage::Defend || GS->Stage == EDVMissionStage::Extract);
	// Once the docks are held the transports get under way and head for open water.
	Speed = FMath::FInterpTo(Speed, bGo ? 900.f : 0.f, DeltaSeconds, 0.15f);
	if (HasAuthority() && Speed > 1.f)
	{
		AddActorWorldOffset(GetActorForwardVector() * Speed * DeltaSeconds);
	}
	SetActorTickInterval(0.f);
}
