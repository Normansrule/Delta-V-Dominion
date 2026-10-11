#include "Combat/DVWeaponData.h"
#include "Animation/AnimMontage.h"
#include "Engine/SkeletalMesh.h"
#include "NiagaraSystem.h"
#include "UObject/ConstructorHelpers.h"

UDVWeaponData::UDVWeaponData()
{
	// Class defaults double as the M9 Bastion when no data asset is assigned: give it a real rifle.
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> RifleMesh(TEXT("/Game/Weapons/Rifle/Meshes/SKM_Rifle.SKM_Rifle"));
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> Sparks(TEXT("/Niagara/DefaultAssets/Templates/Systems/DirectionalBurst.DirectionalBurst"));
	DisplayName = NSLOCTEXT("DeltaV", "M9Bastion", "M9 Bastion");
	Mesh = RifleMesh.Object;
	ImpactFX = Sparks.Object;
	FirstPersonAttachSocket = TEXT("HandGrip_R");
	ThirdPersonAttachSocket = TEXT("HandGrip_R");
	MuzzleSocket = TEXT("Muzzle");
}
