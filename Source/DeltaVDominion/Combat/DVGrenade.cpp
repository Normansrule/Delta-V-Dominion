#include "Combat/DVGrenade.h"
#include "Characters/DVSoldierCharacter.h"
#include "Characters/DVTownsfolk.h"
#include "Core/DVFx.h"

#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ADVGrenade::ADVGrenade()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(6.f);
	Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);	// it rolls past feet instead of stopping dead on them
	SetRootComponent(Collision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(TEXT("/Game/Environment/Outpost/Materials/MI_Container_Olive.MI_Container_Olive"));
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Collision);
	Body->SetStaticMesh(Sphere.Object);
	Body->SetMaterial(0, Mat.Object);
	Body->SetRelativeScale3D(FVector(0.1f, 0.1f, 0.13f));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Blink = CreateDefaultSubobject<UPointLightComponent>(TEXT("Blink"));
	Blink->SetupAttachment(Collision);
	Blink->SetLightColor(FLinearColor(1.f, 0.15f, 0.05f));
	Blink->SetAttenuationRadius(220.f);
	Blink->SetIntensity(0.f);
	Blink->SetCastShadows(false);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->bShouldBounce = true;
	Movement->Bounciness = 0.35f;
	Movement->Friction = 0.4f;
	Movement->BounceVelocityStopSimulatingThreshold = 40.f;
	Movement->ProjectileGravityScale = 1.f;
	Movement->bAutoActivate = false;
}

void ADVGrenade::BeginPlay()
{
	Super::BeginPlay();
	ArmedAt = GetWorld()->GetTimeSeconds();
}

void ADVGrenade::Launch(ADVSoldierCharacter* Thrower, const FVector& Velocity)
{
	ThrownBy = Thrower;
	if (Thrower)
	{
		Collision->IgnoreActorWhenMoving(Thrower, true);
	}
	Movement->Velocity = Velocity;
	Movement->Activate(true);
	ArmedAt = GetWorld()->GetTimeSeconds();
}

void ADVGrenade::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Age = float(GetWorld()->GetTimeSeconds() - ArmedAt);
	// The blink speeds up as the fuse burns down: a readable "get away from it".
	const float Rate = FMath::Lerp(2.f, 10.f, FMath::Clamp(Age / Fuse, 0.f, 1.f));
	Blink->SetIntensity(FMath::Frac(Age * Rate) < 0.3f ? 1500.f : 0.f);
	if (HasAuthority() && !bBlown && Age >= Fuse)
	{
		Detonate();
	}
}

void ADVGrenade::Detonate()
{
	bBlown = true;
	const FVector Where = GetActorLocation() + FVector(0.f, 0.f, 20.f);
	DVCombat::RadialDamage(ThrownBy.Get(), Where, Radius, Damage, false);
	ADVTownsfolk::BlastNear(GetWorld(), Where, Radius * 1.5f, ThrownBy.Get());
	MulticastBlast(Where);
	SetLifeSpan(0.2f);
}

void ADVGrenade::MulticastBlast_Implementation(FVector_NetQuantize Location)
{
	DVFx::Explosion(GetWorld(), Location, 0.6f);
	DVSound::At(GetWorld(), TEXT("SW_ExplosionSmall"), Location, 1.f, 1);
	Body->SetVisibility(false);
	Blink->SetVisibility(false);
}
