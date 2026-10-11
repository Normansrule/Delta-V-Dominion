#include "Combat/DVWeaponBase.h"
#include "Combat/DVWeaponData.h"
#include "Characters/DVSoldierCharacter.h"
#include "DeltaVDominion.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "TimerManager.h"
#include "Core/DVFx.h"
#include "Components/PointLightComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Net/UnrealNetwork.h"

ADVWeaponBase::ADVWeaponBase()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(false);	// attached; transform follows the owner

	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->CastShadow = true;
	RootComponent = Mesh;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	PlaceholderTracerMesh = CylinderMesh.Object;
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GreyBoxMat(TEXT("/Game/Dev/M_GreyBox.M_GreyBox"));
	PlaceholderMaterial = GreyBoxMat.Object;

	PlaceholderMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderMesh"));
	PlaceholderMesh->SetupAttachment(Mesh);
	PlaceholderMesh->SetStaticMesh(CubeMesh.Object);
	PlaceholderMesh->SetRelativeLocation(FVector(25.f, 0.f, 0.f));
	PlaceholderMesh->SetRelativeScale3D(FVector(0.75f, 0.07f, 0.13f));
	PlaceholderMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlaceholderMesh->SetMaterial(0, PlaceholderMaterial);
}

void ADVWeaponBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADVWeaponBase, WeaponData);
	DOREPLIFETIME(ADVWeaponBase, AmmoInMag);
	DOREPLIFETIME(ADVWeaponBase, ReserveAmmo);
	DOREPLIFETIME(ADVWeaponBase, Heat);
	DOREPLIFETIME(ADVWeaponBase, bOverheated);
}

void ADVWeaponBase::InitializeWeapon(UDVWeaponData* InData)
{
	check(HasAuthority());
	WeaponData = InData;
	AmmoInMag = GetData()->MagazineSize;
	ReserveAmmo = GetData()->StartingReserve;
	OnRep_Data();
}

void ADVWeaponBase::OnRep_Data()
{
	if (GetData() && GetData()->Mesh)
	{
		Mesh->SetSkeletalMeshAsset(GetData()->Mesh);
	}
	PlaceholderMesh->SetVisibility(!HasRealMesh());
	if (ADVSoldierCharacter* Soldier = Cast<ADVSoldierCharacter>(GetOwner()))
	{
		AttachToSoldier(Soldier);
	}
}

void ADVWeaponBase::AttachToSoldier(ADVSoldierCharacter* Soldier)
{
	if (!Soldier || !GetData())
	{
		return;
	}

	// With a real body the local player sees that body too (true first person), so everyone attaches to it.
	const bool bHasBody = Soldier->GetMesh() && Soldier->GetMesh()->GetSkeletalMeshAsset();
	const bool bFirstPerson = !bHasBody && Soldier->IsLocallyControlled() && Soldier->IsPlayerControlled();
	USkeletalMeshComponent* Parent = bFirstPerson ? Soldier->GetFirstPersonMesh() : Soldier->GetMesh();
	const FName Socket = bFirstPerson ? GetData()->FirstPersonAttachSocket : GetData()->ThirdPersonAttachSocket;
	if (Parent && Parent->DoesSocketExist(Socket))
	{
		AttachToComponent(Parent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
	}
	else if (Parent)
	{
		// No skeleton yet: hold the weapon at a fixed offset (in view for first person, at the hip for third).
		AttachToComponent(Parent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, NAME_None);
		SetActorRelativeLocation(bFirstPerson ? FVector(62.f, 16.f, 128.f) : FVector(30.f, 22.f, 15.f));
	}
	Mesh->SetOnlyOwnerSee(false);
	Mesh->SetOwnerNoSee(false);
}

void ADVWeaponBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!GetData())
	{
		return;
	}

	Bloom = FMath::Max(0.f, Bloom - GetData()->BloomRecoveryPerSecond * DeltaSeconds);

	if (GetData()->bUsesHeat && HasAuthority())
	{
		Heat = FMath::Max(0.f, Heat - GetData()->CooldownPerSecond * DeltaSeconds);
		if (bOverheated && Heat <= GetData()->OverheatRecoveredAt)
		{
			bOverheated = false;
		}
	}
}

bool ADVWeaponBase::CanFire(double Now, bool bLenientTiming) const
{
	if (!GetData())
	{
		return false;
	}
	if (Now - LastShotTime < GetData()->GetFireInterval() * (bLenientTiming ? 0.75 : 1.0))
	{
		return false;
	}
	return GetData()->bUsesHeat ? !bOverheated : AmmoInMag > 0;
}

bool ADVWeaponBase::NeedsReload() const
{
	return GetData() && !GetData()->bUsesHeat && AmmoInMag <= 0 && ReserveAmmo > 0;
}

bool ADVWeaponBase::CanReload() const
{
	return GetData() && !GetData()->bUsesHeat && AmmoInMag < GetData()->MagazineSize && ReserveAmmo > 0;
}

void ADVWeaponBase::ConsumeShot(double Now)
{
	if (!GetData())
	{
		return;
	}
	LastShotTime = Now;
	Bloom = FMath::Min(GetData()->MaxBloom, Bloom + GetData()->SpreadPerShot);

	if (GetData()->bUsesHeat)
	{
		Heat = FMath::Min(1.f, Heat + GetData()->HeatPerShot);
		if (Heat >= 1.f)
		{
			bOverheated = true;
		}
	}
	else
	{
		AmmoInMag = FMath::Max(0, AmmoInMag - 1);
	}
}

bool ADVWeaponBase::ServerValidateShotTiming(double Now)
{
	if (!GetData())
	{
		return false;
	}

	// Allow 25% early to absorb network jitter, but never sustained faster-than-cyclic fire.
	const double MinInterval = GetData()->GetFireInterval() * 0.75;
	if (Now - LastServerShotTime < MinInterval)
	{
		if (++ServerEarlyShots > 3)
		{
			UE_LOG(LogDVNet, Warning, TEXT("%s: rejecting shot, fire rate exceeds weapon cycle (%s)"),
				*GetNameSafe(GetOwner()), *GetNameSafe(GetData()));
			return false;
		}
	}
	else
	{
		ServerEarlyShots = FMath::Max(0, ServerEarlyShots - 1);
	}

	LastServerShotTime = Now;
	return true;
}

void ADVWeaponBase::ServerFinishReload()
{
	if (!HasAuthority() || !CanReload())
	{
		return;
	}
	const int32 Needed = GetData()->MagazineSize - AmmoInMag;
	const int32 Moved = FMath::Min(Needed, ReserveAmmo);
	AmmoInMag += Moved;
	ReserveAmmo -= Moved;
}

float ADVWeaponBase::GetCurrentSpread(bool bAiming, float Suppression01) const
{
	if (!GetData())
	{
		return 0.f;
	}
	const float Base = bAiming ? GetData()->AdsSpread : GetData()->HipSpread;
	return Base + Bloom + GetData()->SuppressedSpread * FMath::Clamp(Suppression01, 0.f, 1.f);
}

FVector2D ADVWeaponBase::ConsumeRecoil(double Now, bool bAiming)
{
	if (!GetData() || GetData()->RecoilPattern.Num() == 0)
	{
		return FVector2D::ZeroVector;
	}
	if (Now - LastShotTime > GetData()->RecoilResetTime)
	{
		RecoilIndex = 0;
	}
	const FVector2D Kick = GetData()->RecoilPattern[RecoilIndex % GetData()->RecoilPattern.Num()];
	++RecoilIndex;
	return Kick * (bAiming ? GetData()->AdsRecoilMultiplier : 1.f);
}

FVector ADVWeaponBase::GetMuzzleLocation() const
{
	if (GetData() && Mesh->DoesSocketExist(GetData()->MuzzleSocket))
	{
		return Mesh->GetSocketLocation(GetData()->MuzzleSocket);
	}
	if (PlaceholderMesh && PlaceholderMesh->IsVisible())
	{
		return PlaceholderMesh->GetComponentLocation() + PlaceholderMesh->GetForwardVector() * 40.f;
	}
	return GetActorLocation();
}

void ADVWeaponBase::PlayLocalFireEffects(const TArray<FVector>& ImpactPoints)
{
	SpawnShotEffects(ImpactPoints);
}

void ADVWeaponBase::MulticastFireEffects_Implementation(const TArray<FVector_NetQuantize>& ImpactPoints)
{
	// The shooter already played its own predicted effects.
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (OwnerPawn && OwnerPawn->IsLocallyControlled())
	{
		return;
	}
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	TArray<FVector> Points;
	Points.Reserve(ImpactPoints.Num());
	for (const FVector_NetQuantize& P : ImpactPoints)
	{
		Points.Add(P);
	}
	SpawnShotEffects(Points);
}

void ADVWeaponBase::SpawnShotEffects(const TArray<FVector>& ImpactPoints) const
{
	if (!GetData())
	{
		return;
	}

	const FVector Muzzle = GetMuzzleLocation();
	if (GetData()->MuzzleFlash)
	{
		UNiagaraFunctionLibrary::SpawnSystemAttached(GetData()->MuzzleFlash, Mesh, GetData()->MuzzleSocket, FVector::ZeroVector,
			FRotator::ZeroRotator, EAttachLocation::SnapToTarget, /*bAutoDestroy*/ true);
	}
	else if (GetNetMode() != NM_DedicatedServer)
	{
		SpawnMuzzleFlashLight(Muzzle);
	}
	if (GetData()->FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, GetData()->FireSound, Muzzle);
	}

	for (const FVector& Impact : ImpactPoints)
	{
		if (GetData()->Tracer)
		{
			if (UNiagaraComponent* Tracer = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, GetData()->Tracer, Muzzle))
			{
				Tracer->SetVectorParameter(TEXT("BeamEnd"), Impact);
			}
		}
		else
		{
			SpawnPlaceholderTracer(Muzzle, Impact);
		}
		if (GetData()->ImpactFX)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, GetData()->ImpactFX, Impact);
		}
		else
		{
			DVFx::Flash(GetWorld(), Impact, FLinearColor(1.f, 0.7f, 0.4f), 300.f, 250.f, 0.04f);
		}
	}
}

bool ADVWeaponBase::HasRealMesh() const
{
	return GetData() && GetData()->Mesh;
}

void ADVWeaponBase::SpawnMuzzleFlashLight(const FVector& Muzzle) const
{
	// A brief, hot point light: lights the shooter, nearby walls and smoke the way a real muzzle flash does.
	ADVWeaponBase* MutableThis = const_cast<ADVWeaponBase*>(this);
	UPointLightComponent* Flash = NewObject<UPointLightComponent>(MutableThis);
	Flash->SetUsingAbsoluteLocation(true);
	Flash->RegisterComponent();
	Flash->SetWorldLocation(Muzzle);
	Flash->SetIntensityUnits(ELightUnits::Candelas);
	Flash->SetIntensity(FMath::FRandRange(900.f, 1400.f));
	Flash->SetLightColor(FLinearColor(1.f, 0.62f, 0.28f));
	Flash->SetAttenuationRadius(650.f);
	Flash->SetCastShadows(false);

	if (PlaceholderTracerMesh)
	{
		// Star-shaped flare: two crossed, stretched emissive discs.
		for (int32 i = 0; i < 2; ++i)
		{
			UStaticMeshComponent* Flare = NewObject<UStaticMeshComponent>(MutableThis);
			Flare->SetStaticMesh(PlaceholderTracerMesh);
			Flare->SetMaterial(0, PlaceholderMaterial);
			Flare->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Flare->CastShadow = false;
			Flare->SetUsingAbsoluteLocation(true);
			Flare->SetUsingAbsoluteRotation(true);
			Flare->SetUsingAbsoluteScale(true);
			Flare->RegisterComponent();
			const FRotator Facing = (GetMuzzleDirection()).Rotation();
			Flare->SetWorldLocationAndRotation(Muzzle + GetMuzzleDirection() * 12.f, FRotator(Facing.Pitch + 90.f, Facing.Yaw, i * 90.f + FMath::FRandRange(0.f, 45.f)));
			Flare->SetWorldScale3D(FVector(0.09f, FMath::FRandRange(0.18f, 0.3f), 0.25f));
			if (UMaterialInstanceDynamic* Mat = Flare->CreateAndSetMaterialInstanceDynamic(0))
			{
				Mat->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.8f, 0.4f));
				Mat->SetVectorParameterValue(TEXT("Emissive"), FLinearColor(60.f, 30.f, 8.f));
			}
			FTimerHandle FlareHandle;
			GetWorldTimerManager().SetTimer(FlareHandle, FTimerDelegate::CreateWeakLambda(Flare, [Flare]() { Flare->DestroyComponent(); }), 0.035f, false);
		}
	}

	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(Flash, [Flash]() { Flash->DestroyComponent(); }), 0.05f, false);
}

FVector ADVWeaponBase::GetMuzzleDirection() const
{
	if (GetData() && Mesh->DoesSocketExist(GetData()->MuzzleSocket))
	{
		return Mesh->GetSocketRotation(GetData()->MuzzleSocket).Vector();
	}
	if (const APawn* PawnOwner = Cast<APawn>(GetOwner()))
	{
		return PawnOwner->GetBaseAimRotation().Vector();
	}
	return GetActorForwardVector();
}

void ADVWeaponBase::SpawnPlaceholderTracer(const FVector& From, const FVector& To) const
{
	const FVector Delta = To - From;
	const float Length = Delta.Size();
	if (!PlaceholderTracerMesh || Length < 50.f || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// Cosmetic only; a transient component that removes itself after a couple of frames.
	ADVWeaponBase* MutableThis = const_cast<ADVWeaponBase*>(this);
	UStaticMeshComponent* Beam = NewObject<UStaticMeshComponent>(MutableThis);
	Beam->SetStaticMesh(PlaceholderTracerMesh);
	Beam->SetMaterial(0, PlaceholderMaterial);
	Beam->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Beam->CastShadow = false;
	Beam->SetUsingAbsoluteLocation(true);
	Beam->SetUsingAbsoluteRotation(true);
	Beam->SetUsingAbsoluteScale(true);
	Beam->RegisterComponent();
	Beam->SetWorldLocationAndRotation((From + To) * 0.5f, FRotationMatrix::MakeFromZ(Delta).Rotator());
	Beam->SetWorldScale3D(FVector(0.012f, 0.012f, Length / 100.f));
	if (UMaterialInstanceDynamic* Mat = Beam->CreateAndSetMaterialInstanceDynamic(0))
	{
		Mat->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.7f, 0.15f));
		Mat->SetVectorParameterValue(TEXT("Emissive"), FLinearColor(80.f, 32.f, 6.f));
	}

	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(Beam, [Beam]() { Beam->DestroyComponent(); }), 0.06f, false);
}

const UDVWeaponData* ADVWeaponBase::GetData() const
{
	return WeaponData ? WeaponData.Get() : GetDefault<UDVWeaponData>();
}
