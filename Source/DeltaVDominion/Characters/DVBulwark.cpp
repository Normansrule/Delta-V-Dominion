#include "Characters/DVBulwark.h"
#include "Combat/DVAttributeSet.h"
#include "Combat/DVLoadouts.h"
#include "Vehicles/DVVehicleKit.h"

#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

ADVBulwark::ADVBulwark(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	TeamId = 2;
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	GetCapsuleComponent()->SetRelativeScale3D(FVector(1.35f));
	GetCharacterMovement()->MaxWalkSpeed = 250.f;	// a deliberate stomp, slower than any marine
	HostilePaint = FLinearColor(0.05f, 0.04f, 0.035f);

	// Kit-bashed armour bolted to the war-frame's skeleton: slab pauldrons, a chest plate, shin guards,
	// a reactor pack on the back and a visor band of red optics.
	UMaterialInterface* Metal = DVKit::Material(TEXT("Gunmetal"));
	UMaterialInterface* Rust = DVKit::Material(TEXT("Rust"));
	UMaterialInterface* Glow = DVKit::Material(TEXT("Glow"));
	USkeletalMeshComponent* Body = GetMesh();
	struct FPlate { const TCHAR* Name; FName Bone; UStaticMesh* Mesh; UMaterialInterface* Mat; FVector Loc; FVector Size; bool bGlow; };
	const FPlate Plates[] = {
		{ TEXT("PauldronL"), TEXT("upperarm_l"), DVKit::Box(), Metal, FVector(4.f, 0.f, 0.f), FVector(34.f, 30.f, 30.f), false },
		{ TEXT("PauldronR"), TEXT("upperarm_r"), DVKit::Box(), Metal, FVector(-4.f, 0.f, 0.f), FVector(34.f, 30.f, 30.f), false },
		{ TEXT("PauldronTrimL"), TEXT("upperarm_l"), DVKit::Box(), Rust, FVector(4.f, 0.f, 0.f), FVector(36.f, 32.f, 6.f), false },
		{ TEXT("PauldronTrimR"), TEXT("upperarm_r"), DVKit::Box(), Rust, FVector(-4.f, 0.f, 0.f), FVector(36.f, 32.f, 6.f), false },
		{ TEXT("ChestPlate"), TEXT("spine_04"), DVKit::Box(), Metal, FVector(6.f, 0.f, 0.f), FVector(34.f, 30.f, 46.f), false },
		{ TEXT("Collar"), TEXT("spine_05"), DVKit::Box(), Rust, FVector(10.f, 0.f, 0.f), FVector(16.f, 34.f, 44.f), false },
		{ TEXT("ShinL"), TEXT("calf_l"), DVKit::Box(), Metal, FVector(-18.f, 0.f, 0.f), FVector(34.f, 16.f, 16.f), false },
		{ TEXT("ShinR"), TEXT("calf_r"), DVKit::Box(), Metal, FVector(18.f, 0.f, 0.f), FVector(34.f, 16.f, 16.f), false },
		{ TEXT("Reactor"), TEXT("spine_03"), DVKit::Cylinder(), Metal, FVector(0.f, -24.f, 0.f), FVector(26.f, 26.f, 40.f), false },
		{ TEXT("ReactorCore"), TEXT("spine_03"), DVKit::Cylinder(), Glow, FVector(0.f, -32.f, 0.f), FVector(16.f, 16.f, 36.f), true },
		{ TEXT("Visor"), TEXT("head"), DVKit::Box(), Glow, FVector(8.f, -12.f, 0.f), FVector(6.f, 6.f, 20.f), true },
	};
	for (const FPlate& P : Plates)
	{
		UStaticMeshComponent* C = DVKit::Part(this, Body, P.Name, P.Mesh, P.Mat, P.Loc, FRotator::ZeroRotator, P.Size);
		C->SetupAttachment(Body, P.Bone);
		C->CastShadow = true;
		(P.bGlow ? Optics : ArmourPlates).Add(C);
	}
}

void ADVBulwark::BeginPlay()
{
	Super::BeginPlay();
	for (UStaticMeshComponent* C : Optics)
	{
		DVKit::SetGlow(C, FLinearColor(1.f, 0.08f, 0.02f), 8.f);
	}
	if (HasAuthority())
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
		{
			ASC->SetNumericAttributeBase(UDVAttributeSet::GetMaxHealthAttribute(), BulwarkHealth);
			ASC->SetNumericAttributeBase(UDVAttributeSet::GetHealthAttribute(), BulwarkHealth);
			ASC->SetNumericAttributeBase(UDVAttributeSet::GetArmorAttribute(), 60.f);
			ASC->SetNumericAttributeBase(UDVAttributeSet::GetMoraleAttribute(), 1.f);	// it does not get suppressed
		}
		EquipWeaponData(DVLoadouts::BulwarkCannon());
		SoldierName = FString::Printf(TEXT("Bulwark-%02d"), FMath::RandRange(10, 99));
	}
}
