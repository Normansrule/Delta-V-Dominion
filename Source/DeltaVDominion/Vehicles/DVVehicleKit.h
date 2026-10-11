#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "DVVehicleKit.generated.h"

class ADVSoldierCharacter;
class APlayerController;
class UMaterialInterface;

/**
 * The seat of a vehicle: who is driving, entering and exiting (server-authoritative).
 * Any pawn with this component can be boarded with Z by a soldier standing within EnterRadius.
 */
UCLASS(ClassGroup = (DeltaV), meta = (BlueprintSpawnableComponent))
class DELTAVDOMINION_API UDVSeatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDVSeatComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle") FString VehicleName = TEXT("Vehicle");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle") FString WeaponName = TEXT("");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle") float EnterRadius = 550.f;
	/** Where the driver steps out, in the vehicle's local space. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle") FVector ExitOffset = FVector(0.f, -320.f, 120.f);

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Vehicle") TObjectPtr<ADVSoldierCharacter> Driver;

	/** Server: Soldier (possessed by PC) climbs in and PC takes the controls. */
	bool Enter(ADVSoldierCharacter* Soldier, APlayerController* PC);
	/** Server: the driver climbs out and PC gets the soldier back. */
	void Exit(APlayerController* PC);

	/** Closest boardable vehicle within reach of Soldier, or null. */
	static UDVSeatComponent* FindNear(const ADVSoldierCharacter* Soldier);
};

/** Kit-bash helpers: build vehicles and ships out of primitive meshes in constructors. */
namespace DVKit
{
	/** Adds a static mesh part sized SizeCm (bounding box) at Loc/Rot relative to Parent. Call only in a constructor. */
	DELTAVDOMINION_API UStaticMeshComponent* Part(AActor* Owner, USceneComponent* Parent, const TCHAR* Name, UStaticMesh* Mesh,
		UMaterialInterface* Material, const FVector& Loc, const FRotator& Rot, const FVector& SizeCm);

	DELTAVDOMINION_API UStaticMesh* Box();
	DELTAVDOMINION_API UStaticMesh* Cylinder();
	DELTAVDOMINION_API UStaticMesh* Sphere();
	DELTAVDOMINION_API UMaterialInterface* Material(const TCHAR* Name);	// "Gunmetal", "Olive", "Rust", "Navy", "Concrete", "Glow"

	/** Tints a part made with the "Glow" material (emissive), at runtime. */
	DELTAVDOMINION_API void SetGlow(UStaticMeshComponent* Part, const FLinearColor& Color, float Strength);
}
