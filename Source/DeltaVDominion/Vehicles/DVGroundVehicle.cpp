#include "Vehicles/DVGroundVehicle.h"
#include "Vehicles/DVVehicleKit.h"
#include "Characters/DVSoldierCharacter.h"
#include "Core/DVFx.h"
#include "DeltaVDominion.h"

#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/InputComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/CollisionProfile.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

UDVWheelFront::UDVWheelFront()
{
	WheelRadius = 39.f;
	WheelWidth = 35.f;
	FrictionForceMultiplier = 3.f;
	bAffectedByEngine = true;
	bAffectedBySteering = true;
	bAffectedByHandbrake = false;
	MaxSteerAngle = 40.f;
	SuspensionMaxRaise = 8.f;
	SuspensionMaxDrop = 12.f;
}

UDVWheelRear::UDVWheelRear()
{
	WheelRadius = 39.f;
	WheelWidth = 35.f;
	FrictionForceMultiplier = 3.f;
	bAffectedByEngine = true;
	bAffectedBySteering = false;
	bAffectedByHandbrake = true;
	MaxSteerAngle = 0.f;
	SuspensionMaxRaise = 8.f;
	SuspensionMaxDrop = 12.f;
}

// ---------------------------------------------------------------------------

ADVGroundVehicle::ADVGroundVehicle()
{
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Offroad(TEXT("/Game/Vehicles/OffroadCar/SKM_Offroad.SKM_Offroad"));
	static ConstructorHelpers::FClassFinder<UAnimInstance> OffroadAnim(TEXT("/Game/Vehicles/OffroadCar/Offroad_AnimBP"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> OffroadBody(TEXT("/Game/Vehicles/OffroadCar/SM_Offroad_Body.SM_Offroad_Body"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> OffroadTire(TEXT("/Game/Vehicles/OffroadCar/SM_Offroad_Tire.SM_Offroad_Tire"));
	GetMesh()->SetSkeletalMeshAsset(Offroad.Object);
	if (OffroadAnim.Succeeded())
	{
		GetMesh()->SetAnimInstanceClass(OffroadAnim.Class);
	}
	GetMesh()->SetCollisionProfileName(UCollisionProfile::Vehicle_ProfileName);
	GetMesh()->BodyInstance.bSimulatePhysics = true;
	GetMesh()->BodyInstance.COMNudge = FVector(0.f, 0.f, -45.f);	// low centre of mass: no rollovers on the dunes
	GetMesh()->BodyInstance.bNotifyRigidBodyCollision = true;
	GetMesh()->SetGenerateOverlapEvents(true);
	GetMesh()->SetCanEverAffectNavigation(false);
	GetMesh()->SetCollisionResponseToChannel(DV_TRACE_WEAPON, ECR_Block);

	UChaosWheeledVehicleMovementComponent* Move = CastChecked<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement());
	Move->WheelSetups.SetNum(4);
	const TCHAR* Bones[4] = { TEXT("PhysWheel_FL"), TEXT("PhysWheel_FR"), TEXT("PhysWheel_BL"), TEXT("PhysWheel_BR") };
	for (int32 i = 0; i < 4; ++i)
	{
		Move->WheelSetups[i].WheelClass = i < 2 ? UDVWheelFront::StaticClass() : UDVWheelRear::StaticClass();
		Move->WheelSetups[i].BoneName = Bones[i];
	}
	Move->ChassisHeight = 160.f;
	Move->DragCoefficient = 0.3f;
	Move->EngineSetup.MaxTorque = 750.f;
	Move->EngineSetup.MaxRPM = 6500.f;
	Move->TransmissionSetup.bUseAutomaticGears = true;
	// Curves are empty when a vehicle is built in C++ (Blueprints get defaults from the editor): without them
	// the engine makes no torque and the wheels never steer.
	FRichCurve* Torque = Move->EngineSetup.TorqueCurve.GetRichCurve();
	Torque->Reset();
	Torque->AddKey(0.f, 0.45f);
	Torque->AddKey(1500.f, 0.8f);
	Torque->AddKey(3500.f, 1.f);
	Torque->AddKey(6500.f, 0.75f);
	FRichCurve* Steering = Move->SteeringSetup.SteeringCurve.GetRichCurve();
	Steering->Reset();
	Steering->AddKey(0.f, 1.f);
	Steering->AddKey(40.f, 0.75f);
	Steering->AddKey(120.f, 0.55f);
	Move->bLegacyWheelFrictionPosition = true;

	// The skeletal mesh is the chassis, suspension and wheels; the body shell is a separate static mesh.
	BodyShell = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyShell"));
	BodyShell->SetupAttachment(GetMesh());
	BodyShell->SetStaticMesh(OffroadBody.Object);
	BodyShell->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BodyShell->SetCollisionResponseToAllChannels(ECR_Ignore);
	BodyShell->SetCollisionResponseToChannel(DV_TRACE_WEAPON, ECR_Block);

	// Tyres ride on the animated wheel bones.
	const TCHAR* WheelBones[4] = { TEXT("VisWheel_FL"), TEXT("VisWheel_FR"), TEXT("VisWheel_BL"), TEXT("VisWheel_BR") };
	for (int32 i = 0; i < 4; ++i)
	{
		UStaticMeshComponent* Tire = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Tire%d"), i));
		Tire->SetupAttachment(GetMesh(), WheelBones[i]);
		Tire->SetStaticMesh(OffroadTire.Object);
		Tire->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Tire->SetRelativeRotation(FRotator(0.f, (i % 2) ? 180.f : 0.f, 0.f));
		Tires.Add(Tire);
	}

	Seat = CreateDefaultSubobject<UDVSeatComponent>(TEXT("Seat"));
	Seat->VehicleName = TEXT("M-14 Jackal LTV");
	Seat->WeaponName = TEXT("Roof HMG");

	CameraArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraArm"));
	CameraArm->SetupAttachment(GetMesh());
	CameraArm->SetRelativeLocation(FVector(0.f, 0.f, 220.f));
	CameraArm->TargetArmLength = 820.f;
	CameraArm->bUsePawnControlRotation = true;
	CameraArm->bEnableCameraLag = true;
	CameraArm->CameraLagSpeed = 8.f;
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraArm);

	// Roof gun ring and heavy machine gun.
	Turret = CreateDefaultSubobject<USceneComponent>(TEXT("Turret"));
	Turret->SetupAttachment(GetMesh());
	Turret->SetRelativeLocation(FVector(-60.f, 0.f, 150.f));
	Gun = CreateDefaultSubobject<USceneComponent>(TEXT("Gun"));
	Gun->SetupAttachment(Turret);
	Gun->SetRelativeLocation(FVector(0.f, 0.f, 30.f));
	UMaterialInterface* Metal = DVKit::Material(TEXT("Gunmetal"));
	HmgParts.Add(DVKit::Part(this, Turret, TEXT("GunRing"), DVKit::Cylinder(), Metal, FVector::ZeroVector, FRotator::ZeroRotator, FVector(90.f, 90.f, 14.f)));
	HmgParts.Add(DVKit::Part(this, Turret, TEXT("GunShield"), DVKit::Box(), Metal, FVector(30.f, 0.f, 35.f), FRotator::ZeroRotator, FVector(8.f, 70.f, 45.f)));
	HmgParts.Add(DVKit::Part(this, Gun, TEXT("GunBody"), DVKit::Box(), Metal, FVector(10.f, 0.f, 0.f), FRotator::ZeroRotator, FVector(70.f, 16.f, 18.f)));
	HmgParts.Add(DVKit::Part(this, Gun, TEXT("GunBarrel"), DVKit::Cylinder(), Metal, FVector(75.f, 0.f, 0.f), FRotator(90.f, 0.f, 0.f), FVector(6.f, 6.f, 90.f)));

	HeadlightL = DVKit::Part(this, GetMesh(), TEXT("HeadlightL"), DVKit::Box(), DVKit::Material(TEXT("Glow")), FVector(200.f, -60.f, 95.f), FRotator::ZeroRotator, FVector(4.f, 22.f, 8.f));
	HeadlightR = DVKit::Part(this, GetMesh(), TEXT("HeadlightR"), DVKit::Box(), DVKit::Material(TEXT("Glow")), FVector(200.f, 60.f, 95.f), FRotator::ZeroRotator, FVector(4.f, 22.f, 8.f));
}

ADVTank::ADVTank()
{
	Seat->VehicleName = TEXT("M9 Sabre MBT");
	Seat->WeaponName = TEXT("120 mm main gun");
	Seat->ExitOffset = FVector(0.f, -420.f, 160.f);
	Seat->EnterRadius = 700.f;
	FireInterval = 3.f;
	Damage = 450.f;
	SplashRadius = 650.f;
	MuzzleLength = 520.f;
	MaxPitch = 18.f;
	MinPitch = -8.f;
	bExplosiveRounds = true;

	UChaosWheeledVehicleMovementComponent* Move = CastChecked<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement());
	Move->EngineSetup.MaxRPM = 4000.f;
	Move->EngineSetup.MaxTorque = 650.f;
	GetMesh()->BodyInstance.COMNudge = FVector(0.f, 0.f, -70.f);

	CameraArm->TargetArmLength = 1150.f;
	CameraArm->SetRelativeLocation(FVector(0.f, 0.f, 320.f));

	// Armoured hull over the chassis (the car body is hidden in BeginPlay; its wheels still do the physics).
	UMaterialInterface* Armour = DVKit::Material(TEXT("Olive"));
	UMaterialInterface* Metal = DVKit::Material(TEXT("Gunmetal"));
	USceneComponent* Body = GetMesh();
	DVKit::Part(this, Body, TEXT("Hull"), DVKit::Box(), Armour, FVector(0.f, 0.f, 105.f), FRotator::ZeroRotator, FVector(640.f, 330.f, 110.f));
	DVKit::Part(this, Body, TEXT("Glacis"), DVKit::Box(), Armour, FVector(330.f, 0.f, 85.f), FRotator(28.f, 0.f, 0.f), FVector(150.f, 320.f, 60.f));
	DVKit::Part(this, Body, TEXT("TrackL"), DVKit::Box(), Metal, FVector(0.f, -190.f, 60.f), FRotator::ZeroRotator, FVector(700.f, 85.f, 110.f));
	DVKit::Part(this, Body, TEXT("TrackR"), DVKit::Box(), Metal, FVector(0.f, 190.f, 60.f), FRotator::ZeroRotator, FVector(700.f, 85.f, 110.f));
	DVKit::Part(this, Body, TEXT("SkirtL"), DVKit::Box(), Armour, FVector(0.f, -235.f, 95.f), FRotator::ZeroRotator, FVector(640.f, 12.f, 70.f));
	DVKit::Part(this, Body, TEXT("SkirtR"), DVKit::Box(), Armour, FVector(0.f, 235.f, 95.f), FRotator::ZeroRotator, FVector(640.f, 12.f, 70.f));
	for (int32 i = 0; i < 6; ++i)
	{
		const float X = -260.f + i * 104.f;
		DVKit::Part(this, Body, *FString::Printf(TEXT("RoadWheelL%d"), i), DVKit::Cylinder(), Metal, FVector(X, -240.f, 40.f), FRotator(0.f, 0.f, 90.f), FVector(80.f, 80.f, 20.f));
		DVKit::Part(this, Body, *FString::Printf(TEXT("RoadWheelR%d"), i), DVKit::Cylinder(), Metal, FVector(X, 240.f, 40.f), FRotator(0.f, 0.f, 90.f), FVector(80.f, 80.f, 20.f));
	}

	for (UStaticMeshComponent* P : HmgParts)
	{
		P->SetVisibility(false);	// the LTV's roof gun; the tank has its own turret
	}
	Turret->SetRelativeLocation(FVector(-30.f, 0.f, 190.f));
	Gun->SetRelativeLocation(FVector(140.f, 0.f, 40.f));
	DVKit::Part(this, Turret, TEXT("TurretBody"), DVKit::Box(), Armour, FVector(0.f, 0.f, 45.f), FRotator::ZeroRotator, FVector(380.f, 300.f, 100.f));
	DVKit::Part(this, Turret, TEXT("TurretFront"), DVKit::Box(), Armour, FVector(170.f, 0.f, 40.f), FRotator(0.f, 0.f, 0.f), FVector(90.f, 240.f, 80.f));
	DVKit::Part(this, Turret, TEXT("Commander"), DVKit::Cylinder(), Metal, FVector(-60.f, 80.f, 110.f), FRotator::ZeroRotator, FVector(70.f, 70.f, 30.f));
	DVKit::Part(this, Turret, TEXT("Sensor"), DVKit::Box(), DVKit::Material(TEXT("Glow")), FVector(120.f, -110.f, 100.f), FRotator::ZeroRotator, FVector(40.f, 30.f, 26.f));
	DVKit::Part(this, Gun, TEXT("Mantlet"), DVKit::Box(), Metal, FVector(20.f, 0.f, 0.f), FRotator::ZeroRotator, FVector(60.f, 110.f, 60.f));
	DVKit::Part(this, Gun, TEXT("Barrel"), DVKit::Cylinder(), Metal, FVector(260.f, 0.f, 0.f), FRotator(90.f, 0.f, 0.f), FVector(22.f, 22.f, 460.f));
	DVKit::Part(this, Gun, TEXT("MuzzleBrake"), DVKit::Cylinder(), Metal, FVector(500.f, 0.f, 0.f), FRotator(90.f, 0.f, 0.f), FVector(34.f, 34.f, 45.f));
	HeadlightL->SetRelativeLocation(FVector(330.f, -130.f, 120.f));
	HeadlightR->SetRelativeLocation(FVector(330.f, 130.f, 120.f));
}

void ADVGroundVehicle::BeginPlay()
{
	Super::BeginPlay();
	GetMesh()->SetSimulatePhysics(true);
	DVKit::SetGlow(HeadlightL, FLinearColor(1.f, 0.95f, 0.8f), 30.f);
	DVKit::SetGlow(HeadlightR, FLinearColor(1.f, 0.95f, 0.8f), 30.f);
	if (bExplosiveRounds)
	{
		// Tank: hide the car body but keep its physics and wheels; the kit-bashed hull is what you see.
		GetMesh()->SetVisibility(false, false);
		BodyShell->SetVisibility(false);
		for (UStaticMeshComponent* Tire : Tires)
		{
			Tire->SetVisibility(false);
		}
		TArray<UStaticMeshComponent*> Parts;
		GetComponents(Parts);
		for (UStaticMeshComponent* P : Parts)
		{
			if (P->GetName() == TEXT("Sensor"))
			{
				DVKit::SetGlow(P, FLinearColor(0.2f, 0.7f, 1.f), 20.f);
			}
		}
	}
}

void ADVGroundVehicle::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ADVGroundVehicle, AimRotation, COND_SkipOwner);
}

void ADVGroundVehicle::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ADVGroundVehicle::FirePressed);
	PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &ADVGroundVehicle::FireReleased);
}

float ADVGroundVehicle::GetReloadFraction() const
{
	return FMath::Clamp(static_cast<float>((GetWorld()->GetTimeSeconds() - LastShotTime) / FireInterval), 0.f, 1.f);
}

float ADVGroundVehicle::GetSpeedKph() const
{
	return GetVelocity().Size() * 0.036f;
}

void ADVGroundVehicle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC && IsLocallyControlled())
	{
		UChaosWheeledVehicleMovementComponent* Move = CastChecked<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement());
		float Forward = (PC->IsInputKeyDown(EKeys::W) ? 1.f : 0.f) - (PC->IsInputKeyDown(EKeys::S) ? 1.f : 0.f);
		float Steer = (PC->IsInputKeyDown(EKeys::D) ? 1.f : 0.f) - (PC->IsInputKeyDown(EKeys::A) ? 1.f : 0.f);
		if (Forward == 0.f && Steer == 0.f)
		{
			Forward = AutoThrottle;
			Steer = AutoSteer;
		}
		Move->SetThrottleInput(FMath::Max(Forward, 0.f));
		Move->SetBrakeInput(FMath::Max(-Forward, 0.f));
		Move->SetSteeringInput(Steer);
		Move->SetHandbrakeInput(PC->IsInputKeyDown(EKeys::SpaceBar));

		// Weapon follows the camera.
		FRotator Rel = (GetControlRotation() - GetActorRotation()).GetNormalized();
		Rel.Pitch = FMath::Clamp(Rel.Pitch, MinPitch, MaxPitch);
		Rel.Roll = 0.f;
		AimRotation = Rel;
		ServerSetAim(Rel);

		if ((bFireHeld || bAutoFire) && GetWorld()->GetTimeSeconds() - LastShotTime >= FireInterval)
		{
			LastShotTime = GetWorld()->GetTimeSeconds();
			ServerFire(GetAimPoint());
		}
	}

	// Turrets turn at a finite speed so the tank feels heavy.
	const float TurnRate = bExplosiveRounds ? 55.f : 220.f;
	const FRotator CurrentYaw = Turret->GetRelativeRotation();
	const FRotator CurrentPitch = Gun->GetRelativeRotation();
	Turret->SetRelativeRotation(FMath::RInterpConstantTo(CurrentYaw, FRotator(0.f, AimRotation.Yaw, 0.f), DeltaSeconds, TurnRate));
	Gun->SetRelativeRotation(FMath::RInterpConstantTo(CurrentPitch, FRotator(AimRotation.Pitch, 0.f, 0.f), DeltaSeconds, TurnRate));
}

void ADVGroundVehicle::ServerSetAim_Implementation(FRotator NewAim)
{
	AimRotation = NewAim;
}

FVector ADVGroundVehicle::GetAimPoint() const
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !PC->PlayerCameraManager)
	{
		return GetMuzzleLocation() + Gun->GetForwardVector() * 10000.f;
	}
	const FVector Start = PC->PlayerCameraManager->GetCameraLocation();
	const FVector End = Start + PC->PlayerCameraManager->GetActorForwardVector() * 30000.f;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DVVehicleAim), false, this);
	return GetWorld()->LineTraceSingleByChannel(Hit, Start, End, DV_TRACE_WEAPON, Params) ? Hit.ImpactPoint : End;
}

FVector ADVGroundVehicle::GetMuzzleLocation() const
{
	return Gun->GetComponentLocation() + Gun->GetForwardVector() * MuzzleLength;
}

void ADVGroundVehicle::ServerFire_Implementation(FVector_NetQuantize AimPoint)
{
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - LastServerShotTime < FireInterval * 0.9)
	{
		return;	// server-side rate of fire
	}
	LastServerShotTime = Now;

	// The round leaves the real muzzle toward the crosshair point, with a little dispersion.
	const FVector Muzzle = GetMuzzleLocation();
	FVector Dir = (FVector(AimPoint) - Muzzle).GetSafeNormal();
	if (!bExplosiveRounds)
	{
		Dir = FMath::VRandCone(Dir, FMath::DegreesToRadians(0.8f));
	}
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DVVehicleShot), false, this);
	if (Seat->Driver)
	{
		Params.AddIgnoredActor(Seat->Driver);
	}
	const FVector End = Muzzle + Dir * 30000.f;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Muzzle, End, DV_TRACE_WEAPON, Params);
	const FVector Impact = bHit ? Hit.ImpactPoint : End;

	if (Seat->Driver)
	{
		if (bExplosiveRounds)
		{
			DVCombat::RadialDamage(Seat->Driver, Impact, SplashRadius, Damage, false);
		}
		else if (bHit)
		{
			DVCombat::ApplyDamage(Seat->Driver, Hit.GetActor(), Damage);
		}
	}
	MulticastShot(Muzzle, Impact, bExplosiveRounds && bHit);
}

void ADVGroundVehicle::MulticastShot_Implementation(FVector_NetQuantize Muzzle, FVector_NetQuantize Impact, bool bExplosive)
{
	UWorld* World = GetWorld();
	DVFx::Flash(World, Muzzle, FLinearColor(1.f, 0.65f, 0.3f), bExplosiveRounds ? 20000.f : 2500.f, bExplosiveRounds ? 2500.f : 900.f, 0.06f);
	DVFx::Beam(World, Muzzle, Impact, bExplosiveRounds ? FLinearColor(120.f, 60.f, 15.f) : FLinearColor(80.f, 35.f, 6.f), bExplosiveRounds ? 14.f : 4.f, bExplosiveRounds ? 0.12f : 0.05f);
	if (bExplosive)
	{
		DVFx::Explosion(World, Impact, 0.45f);
	}
}
