#include "Vehicles/DVGunship.h"
#include "Vehicles/DVVehicleKit.h"
#include "Components/AudioComponent.h"
#include "Characters/DVSoldierCharacter.h"
#include "Core/DVFx.h"
#include "DeltaVDominion.h"

#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Net/UnrealNetwork.h"

namespace
{
	constexpr float CannonInterval = 0.12f;
	constexpr float RocketInterval = 0.9f;
}

ADVGunship::ADVGunship()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);
	bUseControllerRotationYaw = false;

	Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
	Collision->SetBoxExtent(FVector(520.f, 420.f, 130.f));
	Collision->SetCollisionProfileName(TEXT("Pawn"));
	Collision->SetCollisionResponseToChannel(DV_TRACE_WEAPON, ECR_Block);
	RootComponent = Collision;

	Body = CreateDefaultSubobject<USceneComponent>(TEXT("Body"));
	Body->SetupAttachment(Collision);

	Seat = CreateDefaultSubobject<UDVSeatComponent>(TEXT("Seat"));
	Seat->VehicleName = TEXT("AV-41 Raven gunship");
	Seat->WeaponName = TEXT("30 mm cannon  |  RMB rockets");
	Seat->EnterRadius = 900.f;
	Seat->MaxHealth = 1100.f;
	Seat->ArmourFraction = 0.4f;
	Seat->ExitOffset = FVector(0.f, -650.f, 0.f);

	CameraArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraArm"));
	CameraArm->SetupAttachment(Collision);
	CameraArm->SetRelativeLocation(FVector(0.f, 0.f, 250.f));
	CameraArm->TargetArmLength = 1700.f;
	CameraArm->bUsePawnControlRotation = true;
	CameraArm->bEnableCameraLag = true;
	CameraArm->CameraLagSpeed = 6.f;
	CameraArm->bDoCollisionTest = false;
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraArm);
	CockpitCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("CockpitCamera"));
	CockpitCamera->SetupAttachment(Collision);
	CockpitCamera->SetRelativeLocation(FVector(420.f, 0.f, 150.f));
	CockpitCamera->bUsePawnControlRotation = true;
	CockpitCamera->SetFieldOfView(100.f);
	CockpitCamera->SetAutoActivate(false);

	// Kit-bashed airframe: armoured fuselage, cockpit glass, stub wings with tilting engine pods, tail booms.
	UMaterialInterface* Hull = DVKit::Material(TEXT("Navy"));
	UMaterialInterface* Metal = DVKit::Material(TEXT("Gunmetal"));
	UMaterialInterface* Glow = DVKit::Material(TEXT("Glow"));
	DVKit::Part(this, Body, TEXT("Fuselage"), DVKit::Box(), Hull, FVector(0.f, 0.f, 0.f), FRotator::ZeroRotator, FVector(900.f, 260.f, 210.f));
	DVKit::Part(this, Body, TEXT("Nose"), DVKit::Box(), Hull, FVector(520.f, 0.f, -20.f), FRotator(-14.f, 0.f, 0.f), FVector(260.f, 200.f, 140.f));
	DVKit::Part(this, Body, TEXT("Cockpit"), DVKit::Box(), Glow, FVector(380.f, 0.f, 95.f), FRotator(-18.f, 0.f, 0.f), FVector(230.f, 170.f, 60.f));
	DVKit::Part(this, Body, TEXT("Spine"), DVKit::Box(), Metal, FVector(-60.f, 0.f, 135.f), FRotator::ZeroRotator, FVector(520.f, 120.f, 70.f));
	DVKit::Part(this, Body, TEXT("Wings"), DVKit::Box(), Hull, FVector(-40.f, 0.f, 40.f), FRotator::ZeroRotator, FVector(260.f, 1100.f, 30.f));
	DVKit::Part(this, Body, TEXT("BoomL"), DVKit::Box(), Hull, FVector(-620.f, -150.f, 40.f), FRotator::ZeroRotator, FVector(520.f, 50.f, 50.f));
	DVKit::Part(this, Body, TEXT("BoomR"), DVKit::Box(), Hull, FVector(-620.f, 150.f, 40.f), FRotator::ZeroRotator, FVector(520.f, 50.f, 50.f));
	DVKit::Part(this, Body, TEXT("FinL"), DVKit::Box(), Hull, FVector(-860.f, -150.f, 130.f), FRotator(0.f, 0.f, -12.f), FVector(120.f, 14.f, 190.f));
	DVKit::Part(this, Body, TEXT("FinR"), DVKit::Box(), Hull, FVector(-860.f, 150.f, 130.f), FRotator(0.f, 0.f, 12.f), FVector(120.f, 14.f, 190.f));
	DVKit::Part(this, Body, TEXT("Stab"), DVKit::Box(), Hull, FVector(-870.f, 0.f, 60.f), FRotator::ZeroRotator, FVector(90.f, 420.f, 18.f));
	DVKit::Part(this, Body, TEXT("Chin"), DVKit::Cylinder(), Metal, FVector(560.f, 0.f, -110.f), FRotator(90.f, 0.f, 0.f), FVector(16.f, 16.f, 170.f));
	for (int32 Side = -1; Side <= 1; Side += 2)
	{
		const TCHAR* S = Side < 0 ? TEXT("L") : TEXT("R");
		EnginePods.Add(DVKit::Part(this, Body, *FString::Printf(TEXT("Pod%s"), S), DVKit::Cylinder(), Metal, FVector(-40.f, Side * 560.f, 40.f), FRotator(90.f, 0.f, 0.f), FVector(150.f, 150.f, 330.f)));
		EngineGlows.Add(DVKit::Part(this, Body, *FString::Printf(TEXT("Exhaust%s"), S), DVKit::Cylinder(), Glow, FVector(-40.f, Side * 560.f, -40.f), FRotator::ZeroRotator, FVector(120.f, 120.f, 12.f)));
		DVKit::Part(this, Body, *FString::Printf(TEXT("RocketPod%s"), S), DVKit::Cylinder(), Metal, FVector(60.f, Side * 330.f, -10.f), FRotator(90.f, 0.f, 0.f), FVector(55.f, 55.f, 180.f));
		NavLights.Add(DVKit::Part(this, Body, *FString::Printf(TEXT("Nav%s"), S), DVKit::Sphere(), Glow, FVector(-40.f, Side * 650.f, 50.f), FRotator::ZeroRotator, FVector(22.f, 22.f, 22.f)));
		DVKit::Part(this, Body, *FString::Printf(TEXT("Skid%s"), S), DVKit::Box(), Metal, FVector(40.f, Side * 140.f, -125.f), FRotator::ZeroRotator, FVector(520.f, 16.f, 16.f));
	}
}

void ADVGunship::BeginPlay()
{
	Super::BeginPlay();
	EngineSound = DVSound::Loop(GetWorld(), GetRootComponent(), TEXT("SW_EngineGunship"), 0.6f);
	BaseArmour = Seat->ArmourFraction;
	TargetYaw = GetActorRotation().Yaw;
	TArray<UStaticMeshComponent*> Parts;
	GetComponents(Parts);
	for (UStaticMeshComponent* P : Parts)
	{
		if (P->GetName() == TEXT("Cockpit"))
		{
			DVKit::SetGlow(P, FLinearColor(0.15f, 0.45f, 0.6f), 1.5f);
		}
	}
	for (int32 i = 0; i < EngineGlows.Num(); ++i)
	{
		DVKit::SetGlow(EngineGlows[i], FLinearColor(0.3f, 0.7f, 1.f), 25.f);
	}
	DVKit::SetGlow(NavLights.IsValidIndex(0) ? NavLights[0].Get() : nullptr, FLinearColor(1.f, 0.1f, 0.05f), 40.f);
	DVKit::SetGlow(NavLights.IsValidIndex(1) ? NavLights[1].Get() : nullptr, FLinearColor(0.1f, 1.f, 0.2f), 40.f);
}

void ADVGunship::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);	// flight and weapons are polled in Tick
}

float ADVGunship::GetAltitude() const
{
	FHitResult Hit;
	const FVector Start = GetActorLocation();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DVGunshipAlt), false, this);
	return GetWorld()->LineTraceSingleByChannel(Hit, Start, Start - FVector(0.f, 0.f, 100000.f), ECC_Visibility, Params) ? Hit.Distance / 100.f : 999.f;
}

void ADVGunship::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADVGunship, bJetMode);
	DOREPLIFETIME(ADVGunship, FlaresUntil);
	DOREPLIFETIME(ADVGunship, FlareReadyTime);
}

float ADVGunship::GetFlareReadyFraction() const
{
	const float Left = FlareReadyTime - GetWorld()->GetTimeSeconds();
	return Left <= 0.f ? 1.f : 1.f - Left / 12.f;
}

bool ADVGunship::AreFlaresActive() const
{
	return GetWorld()->GetTimeSeconds() < FlaresUntil;
}

void ADVGunship::SetCockpitView(bool bCockpit)
{
	bCockpitView = bCockpit;
	Camera->SetActive(!bCockpit);
	CockpitCamera->SetActive(bCockpit);
}

void ADVGunship::ServerSetJetMode_Implementation(bool bJet)
{
	bJetMode = bJet;
}

void ADVGunship::ServerFlares_Implementation()
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now < FlareReadyTime)
	{
		return;
	}
	FlareReadyTime = Now + 12.f;
	FlaresUntil = Now + 3.f;
	MulticastFlares();
}

void ADVGunship::MulticastFlares_Implementation()
{
	// Eight burning decoys kicked out behind and below, falling and fading.
	for (int32 i = 0; i < 8; ++i)
	{
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, i]()
		{
			const FVector At = GetActorTransform().TransformPosition(FVector(-500.f - i * 120.f, (i % 2 ? 1.f : -1.f) * (200.f + i * 60.f), -150.f - i * 90.f));
			DVFx::Flash(GetWorld(), At, FLinearColor(1.f, 0.6f, 0.25f), 30000.f, 2500.f, 0.9f);
			DVFx::Beam(GetWorld(), At, At - FVector(0.f, 0.f, 500.f), FLinearColor(60.f, 30.f, 8.f), 18.f, 0.6f);
		}), 0.05f + i * 0.09f, false);
	}
	DVSound::At(GetWorld(), TEXT("SW_Thruster"), GetActorLocation(), 1.f, 1);
}

float ADVGunship::GetRocketReadyFraction() const
{
	return FMath::Clamp(static_cast<float>((GetWorld()->GetTimeSeconds() - LastRocket) / RocketInterval), 0.f, 1.f);
}

void ADVGunship::SetScriptedFlight(bool bOn)
{
	bScriptedFlight = bOn;
	if (Seat)
	{
		Seat->EnterRadius = bOn ? 0.f : 900.f;	// AI-flown Ravens (strafing, extraction) can't be boarded
	}
}

void ADVGunship::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (EngineSound)
	{
		const float Load = bScriptedFlight ? 0.8f : FMath::Clamp(GetSpeedKph() / 150.f, 0.f, 1.f);
		EngineSound->SetPitchMultiplier(0.85f + 0.35f * Load);
		EngineSound->SetVolumeMultiplier(GetController() || bScriptedFlight ? 0.8f : 0.25f);
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC && IsLocallyControlled())
	{
		const FVector Input(
			(PC->IsInputKeyDown(EKeys::W) ? 1.f : 0.f) - (PC->IsInputKeyDown(EKeys::S) ? 1.f : 0.f),
			(PC->IsInputKeyDown(EKeys::D) ? 1.f : 0.f) - (PC->IsInputKeyDown(EKeys::A) ? 1.f : 0.f),
			(PC->IsInputKeyDown(EKeys::SpaceBar) ? 1.f : 0.f) - ((PC->IsInputKeyDown(EKeys::C) || PC->IsInputKeyDown(EKeys::LeftControl)) ? 1.f : 0.f));
		FVector Move = Input;
		if (Input.IsNearlyZero() && !AutoMove.IsNearlyZero())
		{
			Move = AutoMove;
			FRotator Steer = PC->GetControlRotation();
			Steer.Yaw += AutoYawRate * DeltaSeconds;
			PC->SetControlRotation(Steer);
		}
		const FRotator View = PC->GetControlRotation();
		ServerInput(Move, View.Yaw, View.Pitch);
		if (!HasAuthority())
		{
			MoveInput = Move;
			TargetYaw = View.Yaw;
		}

		// Toggles: F flight mode, V cockpit view, X flares.
		const bool bF = PC->IsInputKeyDown(EKeys::F), bV = PC->IsInputKeyDown(EKeys::V), bX = PC->IsInputKeyDown(EKeys::X);
		if (bF && !bWasF)
		{
			ServerSetJetMode(!bJetMode);
			DVSound::UI(GetWorld(), TEXT("SW_UIClick"), 0.6f);
		}
		if (bV && !bWasV)
		{
			SetCockpitView(!bCockpitView);
		}
		if (bX && !bWasX)
		{
			ServerFlares();
		}
		bWasF = bF;
		bWasV = bV;
		bWasX = bX;

		const double Now = GetWorld()->GetTimeSeconds();
		if ((PC->IsInputKeyDown(EKeys::LeftMouseButton) || bAutoCannon) && Now - LastCannon >= CannonInterval)
		{
			LastCannon = Now;
			ServerFire(false, GetAimPoint());
		}
		if ((PC->IsInputKeyDown(EKeys::RightMouseButton) || bAutoRockets) && Now - LastRocket >= RocketInterval)
		{
			LastRocket = Now;
			ServerFire(true, GetAimPoint());
		}
	}

	if (HasAuthority())
	{
		// Flares harden the hull briefly (incoming fire chases the decoys).
		Seat->ArmourFraction = AreFlaresActive() ? 0.85f : BaseArmour;
	}
	if (HasAuthority() && !bScriptedFlight)
	{
		const FRotator Heading(0.f, GetActorRotation().Yaw, 0.f);
		FVector Desired;
		if (bJetMode && GetController())
		{
			// Jet: always flying forward along where the mouse points; W is afterburner, S air-brakes.
			const FRotator Flight(FMath::Clamp(TargetPitch > 180.f ? TargetPitch - 360.f : TargetPitch, -45.f, 45.f), GetActorRotation().Yaw, 0.f);
			const float Speed = 3600.f + 5200.f * FMath::Max(MoveInput.X, 0.f) - 1800.f * FMath::Max(-MoveInput.X, 0.f);
			Desired = Flight.Vector() * Speed;
		}
		else
		{
			// Arcade VTOL: accelerate toward the commanded velocity in the heading frame; hover when idle.
			Desired = Heading.RotateVector(FVector(MoveInput.X * 4200.f, MoveInput.Y * 2400.f, 0.f)) + FVector(0.f, 0.f, MoveInput.Z * 1600.f);
		}
		Velocity = FMath::VInterpTo(Velocity, Desired, DeltaSeconds, bJetMode ? 0.9f : 1.6f);
		FHitResult Hit;
		AddActorWorldOffset(Velocity * DeltaSeconds, true, &Hit);
		if (Hit.bBlockingHit)
		{
			Velocity = FVector::VectorPlaneProject(Velocity, Hit.Normal) * 0.5f;	// slide along terrain instead of sticking
		}
		const float NewYaw = FMath::FixedTurn(GetActorRotation().Yaw, TargetYaw, (bJetMode ? 50.f : 70.f) * DeltaSeconds);
		SetActorRotation(FRotator(0.f, NewYaw, 0.f));
	}

	// Visual tilt: nose down when accelerating forward, bank into strafes and turns.
	const FVector LocalVel = GetActorRotation().UnrotateVector(Velocity);
	const float YawDelta = FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, TargetYaw);
	const FRotator Tilt(-FMath::Clamp(LocalVel.X / 4200.f, -1.f, 1.f) * 12.f, 0.f, FMath::Clamp(LocalVel.Y / 2400.f + YawDelta / 60.f, -1.f, 1.f) * 18.f);
	FRotator Want = Tilt;
	if (bJetMode)
	{
		// Nose follows the flight path; bank hard into turns.
		Want.Pitch = FMath::RadiansToDegrees(FMath::Atan2(Velocity.Z, FMath::Max(Velocity.Size2D(), 1.f)));
		Want.Roll = FMath::Clamp(YawDelta / 30.f, -1.f, 1.f) * 55.f;
	}
	Body->SetRelativeRotation(FMath::RInterpTo(Body->GetRelativeRotation(), Want, DeltaSeconds, 3.f));
	// Engines swing vertical for hover, aft for jet flight.
	for (UStaticMeshComponent* Pod : EnginePods)
	{
		const FRotator PodWant(bJetMode ? 90.f : 0.f, 0.f, 0.f);
		Pod->SetRelativeRotation(FMath::RInterpTo(Pod->GetRelativeRotation(), PodWant, DeltaSeconds, 2.5f));
	}

	// Engine glow pulses with thrust.
	const float Thrust = 15.f + 25.f * FMath::Clamp(Velocity.Size() / 3000.f + FMath::Abs(MoveInput.Z), 0.f, 1.5f);
	for (UStaticMeshComponent* Glow : EngineGlows)
	{
		DVKit::SetGlow(Glow, FLinearColor(0.3f, 0.7f, 1.f), Thrust);
	}
}

void ADVGunship::ServerInput_Implementation(FVector_NetQuantize10 Move, float Yaw, float Pitch)
{
	MoveInput = FVector(FMath::Clamp(Move.X, -1.f, 1.f), FMath::Clamp(Move.Y, -1.f, 1.f), FMath::Clamp(Move.Z, -1.f, 1.f));
	TargetYaw = Yaw;
	TargetPitch = Pitch;
}

FVector ADVGunship::GetAimPoint() const
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !PC->PlayerCameraManager)
	{
		return GetActorLocation() + GetActorForwardVector() * 10000.f;
	}
	const FVector Start = PC->PlayerCameraManager->GetCameraLocation();
	const FVector End = Start + PC->PlayerCameraManager->GetActorForwardVector() * 40000.f;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DVGunshipAim), false, this);
	return GetWorld()->LineTraceSingleByChannel(Hit, Start, End, DV_TRACE_WEAPON, Params) ? Hit.ImpactPoint : End;
}

void ADVGunship::ServerFire_Implementation(bool bRocket, FVector_NetQuantize AimPoint)
{
	const double Now = GetWorld()->GetTimeSeconds();
	double& Last = bRocket ? LastRocket : LastCannon;
	if (HasAuthority() && !IsLocallyControlled() && Now - Last < (bRocket ? RocketInterval : CannonInterval) * 0.9)
	{
		return;
	}
	Last = Now;

	const FVector Muzzle = bRocket
		? GetActorTransform().TransformPosition(FVector(160.f, RocketSide * 330.f, -10.f))
		: GetActorTransform().TransformPosition(FVector(650.f, 0.f, -110.f));
	RocketSide = -RocketSide;
	const FVector Dir = FMath::VRandCone((FVector(AimPoint) - Muzzle).GetSafeNormal(), FMath::DegreesToRadians(bRocket ? 0.5f : 1.2f));
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DVGunshipShot), false, this);
	if (Seat->Driver)
	{
		Params.AddIgnoredActor(Seat->Driver);
	}
	const FVector End = Muzzle + Dir * 40000.f;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Muzzle, End, DV_TRACE_WEAPON, Params);
	const FVector Impact = bHit ? Hit.ImpactPoint : End;
	if (Seat->Driver && bHit)
	{
		if (bRocket)
		{
			DVCombat::RadialDamage(Seat->Driver, Impact, 550.f, 320.f, false);
		}
		else
		{
			DVCombat::RadialDamage(Seat->Driver, Impact, 160.f, 70.f, false);	// 30 mm HE: small splash
		}
	}
	MulticastShot(Muzzle, Impact, bRocket, bHit);
}

void ADVGunship::MulticastShot_Implementation(FVector_NetQuantize Muzzle, FVector_NetQuantize Impact, bool bRocket, bool bHit)
{
	UWorld* World = GetWorld();
	DVFx::Flash(World, Muzzle, FLinearColor(1.f, 0.65f, 0.3f), bRocket ? 6000.f : 3000.f, 1200.f, 0.05f);
	DVSound::At(World, bRocket ? TEXT("SW_ExplosionSmall") : TEXT("SW_Cannon30"), Muzzle, bRocket ? 0.6f : 0.9f, 1);
	DVFx::Beam(World, Muzzle, Impact, bRocket ? FLinearColor(60.f, 40.f, 30.f) : FLinearColor(90.f, 40.f, 8.f), bRocket ? 12.f : 6.f, bRocket ? 0.2f : 0.05f);
	if (bHit)
	{
		DVFx::Explosion(World, Impact, bRocket ? 0.4f : 0.12f);
	}
}
