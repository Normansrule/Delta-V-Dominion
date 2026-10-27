#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DVScatter.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;

/**
 * Ground clutter scattered when the level starts: thousands of pebbles, stones or shrubs as one instanced mesh,
 * each dropped onto the real ground with a trace and tilted to sit on it. Cosmetic, so every machine builds its own
 * from the same seed and nothing replicates.
 */
UCLASS()
class DELTAVDOMINION_API ADVScatter : public AActor
{
	GENERATED_BODY()

public:
	ADVScatter();

	UPROPERTY(EditAnywhere, Category = "Scatter") TObjectPtr<UStaticMesh> Mesh;
	UPROPERTY(EditAnywhere, Category = "Scatter") TObjectPtr<UMaterialInterface> Material;
	UPROPERTY(EditAnywhere, Category = "Scatter") int32 Count = 3000;
	/** Half-extents of the oval scattered over, centred on the actor (cm). */
	UPROPERTY(EditAnywhere, Category = "Scatter") FVector2D Extent = FVector2D(6500.f, 5000.f);
	UPROPERTY(EditAnywhere, Category = "Scatter") float MinScale = 0.05f;
	UPROPERTY(EditAnywhere, Category = "Scatter") float MaxScale = 0.25f;
	/** Squash the height (stones lie flat); 1 keeps proportions. */
	UPROPERTY(EditAnywhere, Category = "Scatter") float Flatten = 0.6f;
	/** How far into the ground each piece is pushed, as a fraction of its height. */
	UPROPERTY(EditAnywhere, Category = "Scatter") float Sink = 0.3f;
	/** Bias toward small pieces: higher means most are near MinScale. */
	UPROPERTY(EditAnywhere, Category = "Scatter") float SizeBias = 2.5f;
	/** Clumping: 0 even, 1 tight clusters. */
	UPROPERTY(EditAnywhere, Category = "Scatter") float Clump = 0.5f;
	UPROPERTY(EditAnywhere, Category = "Scatter") float MinNormalZ = 0.6f;
	UPROPERTY(EditAnywhere, Category = "Scatter") bool bCastShadow = true;
	UPROPERTY(EditAnywhere, Category = "Scatter") int32 Seed = 1;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Instances;
};
