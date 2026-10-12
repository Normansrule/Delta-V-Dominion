#include "Core/DVCallIns.h"
#include "Core/DVFx.h"
#include "Core/DVGameState.h"
#include "Characters/DVSoldierCharacter.h"
#include "Combat/DVAttributeSet.h"
#include "Combat/DVWeaponBase.h"
#include "Vehicles/DVGunship.h"
#include "Vehicles/DVVehicleKit.h"

#include "AbilitySystemComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

// ---------------------------------------------------------------------------
// Supply pod
// ---------------------------------------------------------------------------

ADVSupplyPod::ADVSupplyPod()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);
	bAlwaysRelevant = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	UMaterialInterface* Olive = DVKit::Material(TEXT("Olive"));
	UMaterialInterface* Metal = DVKit::Material(TEXT("Gunmetal"));
	UMaterialInterface* Glow = DVKit::Material(TEXT("Glow"));
	DVKit::Part(this, Root, TEXT("Hull"), DVKit::Cylinder(), Olive, FVector(0.f, 0.f, 110.f), FRotator::ZeroRotator, FVector(150.f, 150.f, 220.f));
	DVKit::Part(this, Root, TEXT("Nose"), DVKit::Cylinder(), Metal, FVector(0.f, 0.f, 10.f), FRotator::ZeroRotator, FVector(170.f, 170.f, 20.f));
	DVKit::Part(this, Root, TEXT("Cap"), DVKit::Cylinder(), Metal, FVector(0.f, 0.f, 230.f), FRotator::ZeroRotator, FVector(120.f, 120.f, 24.f));
	for (int32 i = 0; i < 4; ++i)
	{
		const float A = i * 90.f;
		const FVector Out = FRotator(0.f, A, 0.f).RotateVector(FVector(95.f, 0.f, 0.f));
		DVKit::Part(this, Root, *FString::Printf(TEXT("Fin%d"), i), DVKit::Box(), Metal, Out + FVector(0.f, 0.f, 70.f), FRotator(0.f, A, 0.f), FVector(50.f, 10.f, 140.f));
		Strips.Add(DVKit::Part(this, Root, *FString::Printf(TEXT("Strip%d"), i), DVKit::Box(), Glow, FRotator(0.f, A + 45.f, 0.f).RotateVector(FVector(76.f, 0.f, 0.f)) + FVector(0.f, 0.f, 150.f),
			FRotator(0.f, A + 45.f, 0.f), FVector(4.f, 30.f, 110.f)));
	}
	Beacon = DVKit::Part(this, Root, TEXT("Beacon"), DVKit::Sphere(), Glow, FVector(0.f, 0.f, 255.f), FRotator::ZeroRotator, FVector(40.f, 40.f, 40.f));
	Trail = DVKit::Part(this, Root, TEXT("Trail"), DVKit::Cylinder(), Glow, FVector(0.f, 0.f, 1600.f), FRotator::ZeroRotator, FVector(60.f, 60.f, 3000.f));
	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Root);
	Light->SetRelativeLocation(FVector(0.f, 0.f, 300.f));
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(1500.f);
	Light->SetAttenuationRadius(1200.f);
	Light->SetLightColor(FLinearColor(0.3f, 1.f, 0.45f));
}

void ADVSupplyPod::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADVSupplyPod, Charges);
	DOREPLIFETIME(ADVSupplyPod, bLanded);
}

void ADVSupplyPod::Launch(const FVector& Ground)
{
	GroundPoint = Ground;
	SetActorLocation(Ground + FVector(0.f, 0.f, 9000.f));
	SetLifeSpan(120.f);	// two minutes on the ground, then the deck crew writes it off
}

void ADVSupplyPod::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Blink = FMath::Sin(GetWorld()->GetTimeSeconds() * 6.f) > 0.f ? 1.f : 0.25f;
	const FLinearColor Green(0.25f, 1.f, 0.4f);
	DVKit::SetGlow(Beacon, Green, Charges > 0 ? 40.f * Blink : 0.f);
	for (UStaticMeshComponent* S : Strips)
	{
		DVKit::SetGlow(S, Green, Charges > 0 ? 12.f : 0.5f);
	}
	if (!bLanded)
	{
		DVKit::SetGlow(Trail, FLinearColor(1.f, 0.55f, 0.2f), 18.f);
	}
	Trail->SetVisibility(!bLanded);
	Light->SetIntensity(Charges > 0 ? 1500.f * Blink : 0.f);

	if (!HasAuthority())
	{
		return;
	}
	if (!bLanded)
	{
		FVector Loc = GetActorLocation();
		Loc.Z = FMath::Max(GroundPoint.Z, Loc.Z - 5200.f * DeltaSeconds);
		SetActorLocation(Loc);
		if (Loc.Z <= GroundPoint.Z + 1.f)
		{
			bLanded = true;
			MulticastLanded(Loc);
		}
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (Charges > 0 && Now >= NextResupply)
	{
		NextResupply = Now + 0.5;
		Resupply();
	}
}

void ADVSupplyPod::MulticastLanded_Implementation(FVector_NetQuantize Location)
{
	DVFx::Explosion(GetWorld(), Location, 0.15f);
	DVSound::At(GetWorld(), TEXT("SW_PodLanding"), Location, 1.f, 1);
	DVFx::Flash(GetWorld(), Location + FVector(0.f, 0.f, 200.f), FLinearColor(0.4f, 1.f, 0.5f), 20000.f, 2500.f, 0.25f);
}

void ADVSupplyPod::Resupply()
{
	for (TActorIterator<ADVSoldierCharacter> It(GetWorld()); It && Charges > 0; ++It)
	{
		ADVSoldierCharacter* S = *It;
		if (S->IsDead() || S->TeamId != 1 || FVector::Dist2D(S->GetActorLocation(), GetActorLocation()) > Reach)
		{
			continue;
		}
		bool bUsed = false;
		if (ADVWeaponBase* W = S->GetCurrentWeapon())
		{
			bUsed |= W->ServerResupply();
		}
		if (UAbilitySystemComponent* ASC = S->GetAbilitySystemComponent())
		{
			const float Max = ASC->GetNumericAttribute(UDVAttributeSet::GetMaxHealthAttribute());
			if (ASC->GetNumericAttribute(UDVAttributeSet::GetHealthAttribute()) < Max - 1.f)
			{
				ASC->SetNumericAttributeBase(UDVAttributeSet::GetHealthAttribute(), Max);
				bUsed = true;
			}
			if (ASC->GetNumericAttribute(UDVAttributeSet::GetArmorAttribute()) < 40.f)
			{
				ASC->SetNumericAttributeBase(UDVAttributeSet::GetArmorAttribute(), 40.f);
				bUsed = true;
			}
		}
		Charges -= bUsed ? 1 : 0;
	}
}

// ---------------------------------------------------------------------------
// Strafing run
// ---------------------------------------------------------------------------

ADVStrafeRun::ADVStrafeRun()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ADVStrafeRun::Begin(ADVSoldierCharacter* Caller, const FVector& InTarget, const FVector& Direction)
{
	Shooter = Caller;
	Target = InTarget;
	Dir = FVector(Direction.X, Direction.Y, 0.f).GetSafeNormal();
	if (Dir.IsNearlyZero())
	{
		Dir = FVector::ForwardVector;
	}
	Along = -RunIn;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Aircraft = GetWorld()->SpawnActor<ADVGunship>(ADVGunship::StaticClass(), Target - Dir * RunIn + FVector(0.f, 0.f, Altitude), Dir.Rotation(), Params);
	if (Aircraft)
	{
		Aircraft->SetScriptedFlight(true);
	}
	SetLifeSpan(14.f);
}

void ADVStrafeRun::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || !Aircraft)
	{
		return;
	}
	Along += Speed * DeltaSeconds;
	// Dive a little toward the target and pull out after it.
	const float Dip = 900.f * FMath::Exp(-FMath::Square(Along / 4000.f));
	const FVector Pos = Target + Dir * Along + FVector(0.f, 0.f, Altitude - Dip);
	const float Slope = 2.f * Along / FMath::Square(4000.f) * Dip;	// d(height)/d(along): nose down on the way in
	Aircraft->SetActorLocationAndRotation(Pos, FRotator(FMath::RadiansToDegrees(FMath::Atan(Slope)), Dir.Rotation().Yaw, 0.f));

	// Guns from 75 m out until overhead: rounds walk through the beaten zone.
	const double Now = GetWorld()->GetTimeSeconds();
	if (Along > -7500.f && Along < -500.f && Now >= NextShot)
	{
		NextShot = Now + 0.07;
		const float T = FMath::GetMappedRangeValueClamped(FVector2D(-7500.f, -500.f), FVector2D(-BeatenZone, BeatenZone), Along);
		const FVector Side = FVector::CrossProduct(Dir, FVector::UpVector);
		FVector Aim = Target + Dir * T + Side * FMath::FRandRange(-220.f, 220.f);
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(DVStrafe), false, Aircraft);
		if (GetWorld()->LineTraceSingleByChannel(Hit, Aim + FVector(0.f, 0.f, 4000.f), Aim - FVector(0.f, 0.f, 4000.f), ECC_Visibility, Q))
		{
			Aim = Hit.ImpactPoint;
		}
		const bool bBig = (Shots++ % 3) == 0;
		MulticastBurst(Pos + Dir * 500.f - FVector(0.f, 0.f, 120.f), Aim, bBig);
		DVCombat::RadialDamage(Shooter.Get(), Aim, 280.f, 75.f, false);
	}
	if (Along > RunIn)
	{
		Destroy();
	}
}

void ADVStrafeRun::MulticastBurst_Implementation(FVector_NetQuantize From, FVector_NetQuantize To, bool bBig)
{
	DVFx::Beam(GetWorld(), From, To, FLinearColor(90.f, 40.f, 8.f), 8.f, 0.05f);
	DVFx::Flash(GetWorld(), To + FVector(0.f, 0.f, 60.f), FLinearColor(1.f, 0.6f, 0.25f), 6000.f, 1200.f, 0.08f);
	DVFx::Flash(GetWorld(), From, FLinearColor(1.f, 0.7f, 0.3f), 3000.f, 900.f, 0.04f);
	if (bBig)
	{
		DVSound::At(GetWorld(), TEXT("SW_Cannon30"), From, 1.f, 1);	// one report per three rounds keeps the burst readable
	}
	if (bBig)
	{
		DVFx::Explosion(GetWorld(), To, 0.12f);
	}
}

void ADVStrafeRun::Destroyed()
{
	if (HasAuthority() && Aircraft)
	{
		Aircraft->Destroy();
	}
	Super::Destroyed();
}

// ---------------------------------------------------------------------------
// Objective beacon
// ---------------------------------------------------------------------------

ADVObjectiveBeacon::ADVObjectiveBeacon()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	UMaterialInterface* Glow = DVKit::Material(TEXT("Glow"));
	Column = DVKit::Part(this, RootComponent, TEXT("Column"), DVKit::Cylinder(), Glow, FVector(0.f, 0.f, 4000.f), FRotator::ZeroRotator, FVector(70.f, 70.f, 8000.f));
	Column->CastShadow = false;
	Ring = DVKit::Part(this, RootComponent, TEXT("Ring"), DVKit::Cylinder(), Glow, FVector(0.f, 0.f, 6.f), FRotator::ZeroRotator, FVector(800.f, 800.f, 6.f));
	Ring->CastShadow = false;
	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(RootComponent);
	Light->SetRelativeLocation(FVector(0.f, 0.f, 250.f));
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetAttenuationRadius(1500.f);
}

void ADVObjectiveBeacon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADVObjectiveBeacon, bExtraction);
}

void ADVObjectiveBeacon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const ADVGameState* GS = GetWorld()->GetGameState<ADVGameState>();
	if (!GS)
	{
		return;
	}
	const bool bActive = GS->Phase == EDVMissionPhase::InProgress && (bExtraction
		? GS->Stage == EDVMissionStage::Extract
		: (GS->Stage == EDVMissionStage::Sabotage || GS->Stage == EDVMissionStage::Defend));
	SetActorHiddenInGame(!bActive);
	if (!bActive)
	{
		return;
	}
	// Amber target while planting, pulsing red while the charges count down, cyan landing zone.
	const float T = GetWorld()->GetTimeSeconds();
	FLinearColor Color(1.f, 0.6f, 0.15f);
	float Strength = 6.f;
	if (bExtraction)
	{
		Color = FLinearColor(0.2f, 0.85f, 1.f);
		Strength = 3.f;
	}
	else if (GS->Stage == EDVMissionStage::Defend)
	{
		Color = FLinearColor(1.f, 0.12f, 0.05f);
		Strength = 6.f + 5.f * (0.5f + 0.5f * FMath::Sin(T * (4.f + 10.f * GS->StageProgress)));
	}
	DVKit::SetGlow(Column, Color, Strength * 0.5f);
	DVKit::SetGlow(Ring, Color, Strength);
	Light->SetLightColor(Color);
	Light->SetIntensity(800.f * Strength);
}
