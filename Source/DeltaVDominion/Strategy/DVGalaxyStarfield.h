#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DVGalaxyStarfield.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMeshComponent;

/**
 * Procedural Milky Way: a central bar and bulge, four logarithmic spiral arms (Perseus, Sagittarius,
 * Norma, Scutum-Centaurus, 12 degree pitch) and the Orion Spur through Sol, ~100,000 light-years across.
 * 1 unit = 1 light-year. Sol sits 26,000 ly from the core at (-26000, 0, 0).
 * Stars are instanced emissive spheres binned by colour; generated at runtime (and in the editor for preview).
 */
UCLASS()
class DELTAVDOMINION_API ADVGalaxyStarfield : public AActor
{
	GENERATED_BODY()

public:
	ADVGalaxyStarfield();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, Category = "Galaxy") int32 NumStars = 90000;
	UPROPERTY(EditAnywhere, Category = "Galaxy") int32 Seed = 2189;
	UPROPERTY(EditAnywhere, Category = "Galaxy") float StarScale = 4.f;

	/** Arm phase in degrees at r = 4000 ly (shared with Scripts/build_galaxy_map.py for labels). */
	static float ArmAngleRad(int32 Arm, float Radius);

protected:
	/** 0 blue-white, 1 white, 2 yellow, 3 orange, 4 pink (star-forming regions). */
	UPROPERTY(VisibleAnywhere) TArray<TObjectPtr<UInstancedStaticMeshComponent>> Bins;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> CoreGlow;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> CoreHaze;

private:
	void Generate();
};
