#include "Vehicles/DVVehicleKit.h"
#include "Characters/DVSoldierCharacter.h"
#include "Core/DVFx.h"
#include "Vehicles/DVGunship.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "TimerManager.h"

#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

UDVSeatComponent::UDVSeatComponent()
{
	SetIsReplicatedByDefault(true);
}

void UDVSeatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UDVSeatComponent, Driver);
	DOREPLIFETIME(UDVSeatComponent, Health);
	DOREPLIFETIME(UDVSeatComponent, bWrecked);
}

bool UDVSeatComponent::Enter(ADVSoldierCharacter* Soldier, APlayerController* PC)
{
	APawn* Vehicle = Cast<APawn>(GetOwner());
	if (!Vehicle || !Soldier || !PC || Driver || Soldier->IsDead() || !Vehicle->HasAuthority())
	{
		return false;
	}
	if (FVector::Dist(Soldier->GetActorLocation(), Vehicle->GetActorLocation()) > EnterRadius * 1.5f)
	{
		return false;	// server-side reach check
	}
	Driver = Soldier;
	Soldier->SetAiming(false);
	Soldier->GetCharacterMovement()->DisableMovement();
	Soldier->SetActorEnableCollision(false);
	Soldier->SetActorHiddenInGame(true);	// hidden soldiers are ignored by enemy perception (see DVSquadAIController)
	Soldier->AttachToActor(Vehicle, FAttachmentTransformRules::KeepWorldTransform);
	PC->Possess(Vehicle);
	PC->SetControlRotation(Vehicle->GetActorRotation());
	return true;
}

void UDVSeatComponent::Exit(APlayerController* PC)
{
	APawn* Vehicle = Cast<APawn>(GetOwner());
	ADVSoldierCharacter* Soldier = Driver;
	if (!Vehicle || !Soldier || !PC || !Vehicle->HasAuthority())
	{
		return;
	}
	Driver = nullptr;
	Soldier->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	const FVector Out = Vehicle->GetActorTransform().TransformPosition(ExitOffset);
	Soldier->TeleportTo(Out, FRotator(0.f, Vehicle->GetActorRotation().Yaw, 0.f));
	Soldier->SetActorHiddenInGame(false);
	Soldier->SetActorEnableCollision(true);
	Soldier->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	PC->Possess(Soldier);
}

void UDVSeatComponent::BeginPlay()
{
	Super::BeginPlay();
	Health = MaxHealth;
}

uint8 UDVSeatComponent::GetTeam() const
{
	return Driver ? Driver->TeamId : 1;
}

UDVSeatComponent* UDVSeatComponent::FindOn(AActor* Actor)
{
	for (AActor* A = Actor; A; A = A->GetAttachParentActor())
	{
		if (UDVSeatComponent* Seat = A->FindComponentByClass<UDVSeatComponent>())
		{
			return Seat;
		}
	}
	return nullptr;
}

void UDVSeatComponent::ApplyHullDamage(float Amount, ADVSoldierCharacter* Instigator, bool bFriendlyFire)
{
	AActor* Vehicle = GetOwner();
	if (!Vehicle || !Vehicle->HasAuthority() || bWrecked || Amount <= 0.f)
	{
		return;
	}
	if (!bFriendlyFire && Instigator && Instigator->TeamId == GetTeam())
	{
		return;
	}
	Health = FMath::Max(0.f, Health - Amount * (1.f - ArmourFraction));
	if (Health <= 0.f)
	{
		Wreck(Instigator);
	}
}

void UDVSeatComponent::Wreck(ADVSoldierCharacter* Instigator)
{
	APawn* Vehicle = Cast<APawn>(GetOwner());
	bWrecked = true;
	EnterRadius = 0.f;
	// Throw the crew clear, singed.
	if (Driver)
	{
		ADVSoldierCharacter* Crew = Driver;
		if (APlayerController* PC = Cast<APlayerController>(Vehicle->GetController()))
		{
			Exit(PC);
		}
		if (Instigator)
		{
			DVCombat::ApplyDamage(Instigator, Crew, 35.f, true);
		}
	}
	if (ADVGunship* Air = Cast<ADVGunship>(Vehicle))
	{
		Air->SetScriptedFlight(true);	// no pilot input; gravity takes it from here
	}
	if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Vehicle->GetRootComponent()))
	{
		Root->SetSimulatePhysics(true);
		Root->AddImpulse(FVector(FMath::FRandRange(-300.f, 300.f), FMath::FRandRange(-300.f, 300.f), 500.f), NAME_None, true);
	}
	ApplyWreckVisuals();
}

void UDVSeatComponent::OnRep_Wrecked()
{
	if (bWrecked)
	{
		ApplyWreckVisuals();
	}
}

void UDVSeatComponent::ApplyWreckVisuals()
{
	AActor* Vehicle = GetOwner();
	UWorld* World = GetWorld();
	if (!Vehicle || !World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const FVector Where = Vehicle->GetActorLocation();
	DVFx::Explosion(World, Where, 0.6f);
	// Burnt-out hull: every part goes to scorched metal, lights die, the engine stops.
	UMaterialInstanceDynamic* Char = UMaterialInstanceDynamic::Create(DVKit::Material(TEXT("Rust")), Vehicle);
	if (Char)
	{
		Char->SetVectorParameterValue(TEXT("TintColor"), FLinearColor(0.018f, 0.014f, 0.012f));
	}
	TArray<UMeshComponent*> Meshes;
	Vehicle->GetComponents(Meshes);
	for (UMeshComponent* M : Meshes)
	{
		for (int32 i = 0; Char && i < M->GetNumMaterials(); ++i)
		{
			M->SetMaterial(i, Char);
		}
	}
	TArray<UAudioComponent*> Sounds;
	Vehicle->GetComponents(Sounds);
	for (UAudioComponent* A : Sounds)
	{
		A->Stop();
	}
	// Fire in the hull for a while: flickering light plus small cook-offs.
	for (int32 i = 0; i < 6; ++i)
	{
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(Vehicle, [Vehicle, i]()
		{
			const FVector At = Vehicle->GetActorLocation() + FVector(FMath::FRandRange(-150.f, 150.f), FMath::FRandRange(-150.f, 150.f), 120.f);
			DVFx::Flash(Vehicle->GetWorld(), At, FLinearColor(1.f, 0.45f, 0.12f), 8000.f, 1600.f, 1.6f);
			if (i % 2 == 1)
			{
				DVFx::Explosion(Vehicle->GetWorld(), At, 0.12f);
			}
		}), 0.6f + i * 1.4f, false);
	}
}

UDVSeatComponent* UDVSeatComponent::FindNear(const ADVSoldierCharacter* Soldier)
{
	if (!Soldier)
	{
		return nullptr;
	}
	UDVSeatComponent* Best = nullptr;
	float BestDist = TNumericLimits<float>::Max();
	for (TActorIterator<APawn> It(Soldier->GetWorld()); It; ++It)
	{
		if (UDVSeatComponent* Seat = It->FindComponentByClass<UDVSeatComponent>())
		{
			const float Dist = FVector::Dist(It->GetActorLocation(), Soldier->GetActorLocation());
			if (!Seat->Driver && !Seat->bWrecked && Dist < Seat->EnterRadius && Dist < BestDist)
			{
				Best = Seat;
				BestDist = Dist;
			}
		}
	}
	return Best;
}

// ---------------------------------------------------------------------------

UStaticMesh* DVKit::Box()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Game/LevelPrototyping/Meshes/SM_ChamferCube.SM_ChamferCube"));
	return Mesh.Object;
}

UStaticMesh* DVKit::Cylinder()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	return Mesh.Object;
}

UStaticMesh* DVKit::Sphere()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	return Mesh.Object;
}

UMaterialInterface* DVKit::Material(const TCHAR* Name)
{
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Gunmetal(TEXT("/Game/Environment/Outpost/Materials/MI_Gunmetal.MI_Gunmetal"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Olive(TEXT("/Game/Environment/Outpost/Materials/MI_Container_Olive.MI_Container_Olive"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Rust(TEXT("/Game/Environment/Outpost/Materials/MI_Container_Rust.MI_Container_Rust"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Navy(TEXT("/Game/Environment/Outpost/Materials/MI_Container_Navy.MI_Container_Navy"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Concrete(TEXT("/Game/Environment/Outpost/Materials/MI_Concrete_Dark.MI_Concrete_Dark"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Glow(TEXT("/Game/Dev/M_GreyBox.M_GreyBox"));
	const FString N(Name);
	if (N == TEXT("Olive")) { return Olive.Object; }
	if (N == TEXT("Rust")) { return Rust.Object; }
	if (N == TEXT("Navy")) { return Navy.Object; }
	if (N == TEXT("Concrete")) { return Concrete.Object; }
	if (N == TEXT("Glow")) { return Glow.Object; }
	return Gunmetal.Object;
}

UStaticMeshComponent* DVKit::Part(AActor* Owner, USceneComponent* Parent, const TCHAR* Name, UStaticMesh* Mesh,
	UMaterialInterface* Material, const FVector& Loc, const FRotator& Rot, const FVector& SizeCm)
{
	UStaticMeshComponent* C = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
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
	return C;
}

void DVKit::SetGlow(UStaticMeshComponent* Part, const FLinearColor& Color, float Strength)
{
	if (UMaterialInstanceDynamic* Mat = Part ? Part->CreateAndSetMaterialInstanceDynamic(0) : nullptr)
	{
		Mat->SetVectorParameterValue(TEXT("Color"), Color);
		Mat->SetVectorParameterValue(TEXT("Emissive"), Color * Strength);
	}
}
