#include "Combat/DVWeaponBase.h"
#include "Combat/DVWeaponData.h"
#include "Characters/DVSoldierCharacter.h"
#include "DeltaVDominion.h"
#include "Components/SkeletalMeshComponent.h"
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

	const bool bFirstPerson = Soldier->IsLocallyControlled() && Soldier->IsPlayerControlled();
	USkeletalMeshComponent* Parent = bFirstPerson ? Soldier->GetFirstPersonMesh() : Soldier->GetMesh();
	const FName Socket = bFirstPerson ? GetData()->FirstPersonAttachSocket : GetData()->ThirdPersonAttachSocket;
	if (Parent)
	{
		AttachToComponent(Parent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
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
		if (GetData()->ImpactFX)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, GetData()->ImpactFX, Impact);
		}
	}
}

const UDVWeaponData* ADVWeaponBase::GetData() const
{
	return WeaponData ? WeaponData.Get() : GetDefault<UDVWeaponData>();
}
