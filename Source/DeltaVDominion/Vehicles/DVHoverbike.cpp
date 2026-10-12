#include "Vehicles/DVHoverbike.h"
#include "Vehicles/DVVehicleKit.h"
#include "Characters/DVSoldierCharacter.h"
#include "Core/DVFx.h"
#include "DeltaVDominion.h"

#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"

namespace
{
	constexpr float ShotInterval = 0.09f;
}

ADVHoverbike::ADVHoverbike()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);

	Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
	Collision->SetBoxExtent(FVector(150.f, 70.f, 45.f));
	Collision->SetCollisionProfileName(TEXT("Pawn"));
	Collision->SetCollisionResponseToChannel(DV_TRACE_WEAPON, ECR_Block);
	RootComponent = Collision;

	Body = CreateDefaultSubobject<USceneComponent>(TEXT("Body"));
	Body->SetupAttachment(Collision);

	Seat = CreateDefaultSubobject<UDVSeatComponent>(TEXT("Seat"));
	Seat->VehicleName = TEXT("VX-3 Skiff hoverbike");
	Seat->WeaponName = TEXT("Twin 12 mm  |  Shift boost  |  Space hop");
	Seat->EnterRadius = 400.f;
	Seat->ExitOffset = FVector(0.f, -220.f, 60.f);
	Seat->MaxHealth = 320.f;
	Seat->ArmourFraction = 0.15f;

	CameraArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraArm"));
	CameraArm->SetupAttachment(Collision);
	CameraArm->SetRelativeLocation(FVector(0.f, 0.f, 110.f));
	CameraArm->TargetArmLength = 560.f;
	CameraArm->bUsePawnControlRotation = true;
	CameraArm->bEnableCameraLag = true;
	CameraArm->CameraLagSpeed = 10.f;
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraArm);
	Camera->SetFieldOfView(95.f);

	// Black composite frame, a saddle and bars, four ducted fans with glowing rotor rings.
	UMaterialInterface* Hull = DVKit::Material(TEXT("Navy"));
	UMaterialInterface* Metal = DVKit::Material(TEXT("Gunmetal"));
	UMaterialInterface* Glow = DVKit::Material(TEXT("Glow"));
	DVKit::Part(this, Body, TEXT("Spine"), DVKit::Box(), Hull, FVector(0.f, 0.f, 10.f), FRotator::ZeroRotator, FVector(260.f, 46.f, 40.f));
	DVKit::Part(this, Body, TEXT("Tank"), DVKit::Box(), Hull, FVector(40.f, 0.f, 40.f), FRotator(-8.f, 0.f, 0.f), FVector(110.f, 56.f, 36.f));
	DVKit::Part(this, Body, TEXT("Saddle"), DVKit::Box(), Metal, FVector(-50.f, 0.f, 46.f), FRotator::ZeroRotator, FVector(90.f, 40.f, 14.f));
	DVKit::Part(this, Body, TEXT("Bars"), DVKit::Box(), Metal, FVector(95.f, 0.f, 72.f), FRotator::ZeroRotator, FVector(12.f, 90.f, 8.f));
	DVKit::Part(this, Body, TEXT("Screen"), DVKit::Box(), Glow, FVector(110.f, 0.f, 70.f), FRotator(-40.f, 0.f, 0.f), FVector(6.f, 46.f, 30.f));
	DVKit::Part(this, Body, TEXT("GunL"), DVKit::Cylinder(), Metal, FVector(110.f, -30.f, -12.f), FRotator(90.f, 0.f, 0.f), FVector(10.f, 10.f, 90.f));
	DVKit::Part(this, Body, TEXT("GunR"), DVKit::Cylinder(), Metal, FVector(110.f, 30.f, -12.f), FRotator(90.f, 0.f, 0.f), FVector(10.f, 10.f, 90.f));
	for (int32 i = 0; i < 4; ++i)
	{
		const FVector At((i < 2 ? 125.f : -125.f), (i % 2 ? 78.f : -78.f), 0.f);
		DVKit::Part(this, Body, *FString::Printf(TEXT("Duct%d"), i), DVKit::Cylinder(), Hull, At, FRotator::ZeroRotator, FVector(96.f, 96.f, 30.f));
		DVKit::Part(this, Body, *FString::Printf(TEXT("Hub%d"), i), DVKit::Cylinder(), Metal, At + FVector(0.f, 0.f, 4.f), FRotator::ZeroRotator, FVector(26.f, 26.f, 32.f));
		FanGlows.Add(DVKit::Part(this, Body, *FString::Printf(TEXT("Rotor%d"), i), DVKit::Cylinder(), Glow, At - FVector(0.f, 0.f, 12.f), FRotator::ZeroRotator, FVector(84.f, 84.f, 4.f)));
	}
}

void ADVHoverbike::BeginPlay()
{
	Super::BeginPlay();
	TargetYaw = GetActorRotation().Yaw;
	EngineSound = DVSound::Loop(GetWorld(), GetRootComponent(), TEXT("SW_EngineGunship"), 0.35f);
	TArray<UStaticMeshComponent*> Parts;
	GetComponents(Parts);
	for (UStaticMeshComponent* P : Parts)
	{
		if (P->GetName() == TEXT("Screen"))
		{
			DVKit::SetGlow(P, FLinearColor(0.2f, 0.6f, 1.f), 4.f);
		}
	}
}

void ADVHoverbike::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC && IsLocallyControlled())
	{
		float Throttle = (PC->IsInputKeyDown(EKeys::W) ? 1.f : 0.f) - (PC->IsInputKeyDown(EKeys::S) ? 0.6f : 0.f);
		const bool bSpace = PC->IsInputKeyDown(EKeys::SpaceBar);
		if (FMath::IsNearlyZero(Throttle) && !FMath::IsNearlyZero(AutoThrottle))
		{
			Throttle = AutoThrottle;
			FRotator Steer = PC->GetControlRotation();
			Steer.Yaw += AutoYawRate * DeltaSeconds;
			PC->SetControlRotation(Steer);
		}
		ServerInput(Throttle, PC->GetControlRotation().Yaw, PC->IsInputKeyDown(EKeys::LeftShift), bSpace && !bWasSpace);
		bWasSpace = bSpace;
		if ((PC->IsInputKeyDown(EKeys::LeftMouseButton) || bAutoFire) && GetWorld()->GetTimeSeconds() - LastShot >= ShotInterval)
		{
			LastShot = GetWorld()->GetTimeSeconds();
			const FVector Start = PC->PlayerCameraManager->GetCameraLocation();
			const FVector End = Start + PC->PlayerCameraManager->GetActorForwardVector() * 20000.f;
			FHitResult Hit;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(DVBikeAim), false, this);
			ServerFire(GetWorld()->LineTraceSingleByChannel(Hit, Start, End, DV_TRACE_WEAPON, Q) ? Hit.ImpactPoint : End);
		}
	}

	if (HasAuthority())
	{
		// Boost tank drains while held, refills when released.
		const bool bBoosting = bBoostHeld && Boost > 0.02f && ThrottleInput > 0.f;
		Boost = FMath::Clamp(Boost + (bBoosting ? -0.45f : 0.25f) * DeltaSeconds, 0.f, 1.f);

		// Hover: spring toward a fixed height over whatever is below; gravity when there is nothing.
		FHitResult Ground;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(DVBikeHover), false, this);
		if (Seat->Driver)
		{
			Q.AddIgnoredActor(Seat->Driver);
		}
		const bool bGround = GetWorld()->LineTraceSingleByChannel(Ground, GetActorLocation(), GetActorLocation() - FVector(0.f, 0.f, 1200.f), ECC_Visibility, Q);
		const float Height = bGround ? Ground.Distance : 1200.f;
		float VZ = Velocity.Z;
		if (Height < HoverHeight * 2.5f)
		{
			VZ += ((HoverHeight - Height) * 9.f - VZ * 2.2f) * DeltaSeconds;
		}
		else
		{
			VZ -= 980.f * DeltaSeconds;
		}
		if (bHopRequested && Height < HoverHeight * 1.6f)
		{
			VZ = 900.f;
		}
		bHopRequested = false;

		const FRotator Heading(0.f, GetActorRotation().Yaw, 0.f);
		const float MaxSpeed = bBoosting ? BoostSpeed : TopSpeed;
		FVector Planar = FVector(Velocity.X, Velocity.Y, 0.f);
		const FVector Wish = Heading.Vector() * ThrottleInput * MaxSpeed;
		// Grippy: most of the sideways slip bleeds off, so it carves rather than drifts.
		const FVector Fwd = Heading.Vector();
		const float Along = FVector::DotProduct(Planar, Fwd);
		const FVector Side = Planar - Fwd * Along;
		Planar = Fwd * FMath::FInterpTo(Along, FVector::DotProduct(Wish, Fwd), DeltaSeconds, bBoosting ? 1.8f : 1.2f) + Side * FMath::Max(0.f, 1.f - 4.f * DeltaSeconds);
		Velocity = FVector(Planar.X, Planar.Y, VZ);

		// Move vertically and horizontally in separate sweeps, so brushing the ground never stalls the bike.
		FHitResult Hit;
		AddActorWorldOffset(FVector(0.f, 0.f, Velocity.Z * DeltaSeconds), true, &Hit);
		if (Hit.bBlockingHit)
		{
			Velocity.Z = FMath::Max(Velocity.Z, 0.f);
		}
		AddActorWorldOffset(FVector(Velocity.X, Velocity.Y, 0.f) * DeltaSeconds, true, &Hit);
		if (Hit.bBlockingHit)
		{
			if (Hit.ImpactNormal.Z > 0.5f)
			{
				// A slope: ride up it.
				AddActorWorldOffset(FVector(0.f, 0.f, 40.f), false);
			}
			else
			{
				const FVector Slid = FVector::VectorPlaneProject(FVector(Velocity.X, Velocity.Y, 0.f), Hit.ImpactNormal) * 0.7f;
				Velocity = FVector(Slid.X, Slid.Y, Velocity.Z);
			}
		}
		SetActorRotation(FRotator(0.f, FMath::FixedTurn(GetActorRotation().Yaw, TargetYaw, 140.f * DeltaSeconds), 0.f));
	}

	// Lean into turns, pitch with acceleration; rotor rings glow with throttle.
	const float YawDelta = FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, TargetYaw);
	const FVector Local = GetActorRotation().UnrotateVector(Velocity);
	const FRotator Tilt(-FMath::Clamp(ThrottleInput, -1.f, 1.f) * 6.f, 0.f, FMath::Clamp(YawDelta / 25.f, -1.f, 1.f) * 28.f * FMath::Clamp(Local.X / 1500.f, 0.f, 1.f));
	Body->SetRelativeRotation(FMath::RInterpTo(Body->GetRelativeRotation(), Tilt, DeltaSeconds, 5.f));
	const float Spin = 8.f + 30.f * FMath::Clamp(Velocity.Size() / BoostSpeed, 0.f, 1.f);
	for (UStaticMeshComponent* G : FanGlows)
	{
		DVKit::SetGlow(G, Boost > 0.f && bBoostHeld ? FLinearColor(1.f, 0.5f, 0.15f) : FLinearColor(0.3f, 0.75f, 1.f), Spin);
	}
	if (EngineSound)
	{
		EngineSound->SetPitchMultiplier(1.2f + 0.8f * FMath::Clamp(Velocity.Size() / BoostSpeed, 0.f, 1.f));
		EngineSound->SetVolumeMultiplier(GetController() ? 0.7f : 0.2f);
	}
}

void ADVHoverbike::ServerInput_Implementation(float Throttle, float Yaw, bool bBoost, bool bHop)
{
	ThrottleInput = FMath::Clamp(Throttle, -1.f, 1.f);
	TargetYaw = Yaw;
	bBoostHeld = bBoost;
	bHopRequested |= bHop;
}

void ADVHoverbike::ServerFire_Implementation(FVector_NetQuantize AimPoint)
{
	const double Now = GetWorld()->GetTimeSeconds();
	if (!IsLocallyControlled() && Now - LastShot < ShotInterval * 0.9)
	{
		return;
	}
	LastShot = Now;
	const FVector Muzzle = GetActorTransform().TransformPosition(FVector(160.f, GunSide * 30.f, -12.f));
	GunSide = -GunSide;
	const FVector Dir = FMath::VRandCone((FVector(AimPoint) - Muzzle).GetSafeNormal(), FMath::DegreesToRadians(1.4f));
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(DVBikeShot), false, this);
	if (Seat->Driver)
	{
		Q.AddIgnoredActor(Seat->Driver);
	}
	const FVector End = Muzzle + Dir * 15000.f;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Muzzle, End, DV_TRACE_WEAPON, Q);
	if (bHit && Seat->Driver)
	{
		DVCombat::ApplyDamage(Seat->Driver, Hit.GetActor(), 22.f);
	}
	MulticastShot(Muzzle, bHit ? FVector(Hit.ImpactPoint) : End, bHit);
}

void ADVHoverbike::MulticastShot_Implementation(FVector_NetQuantize Muzzle, FVector_NetQuantize Impact, bool bHit)
{
	DVFx::Flash(GetWorld(), Muzzle, FLinearColor(1.f, 0.7f, 0.3f), 1500.f, 600.f, 0.04f);
	DVFx::Beam(GetWorld(), Muzzle, Impact, FLinearColor(70.f, 30.f, 6.f), 3.f, 0.04f);
	DVSound::At(GetWorld(), TEXT("SW_SMG"), Muzzle, 0.7f, 0);
	if (bHit)
	{
		DVSound::At(GetWorld(), TEXT("SW_Impact"), Impact, 0.4f, 0, 0.2f);
	}
}
