#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DVStarSystem.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;

UENUM(BlueprintType)
enum class EDVControl : uint8
{
	Accord,		// Terran Accord space
	Contested,	// Front line
	Cinder,		// Held by CINDER's machines
	Uncharted
};

/** One playable operation launched from a star system. */
USTRUCT(BlueprintType)
struct FDVOperation
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Name;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Location;
	/** Map to load, e.g. /Game/Maps/Outpost. Empty = briefing only (not yet playable). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Map;
};

/**
 * A named star system on the strategic galaxy map. Placed by Scripts/build_galaxy_map.py at its real
 * galactic position (Sol-centred galactic coordinates, 1 unit = 1 light-year; the local bubble around
 * Sol is drawn magnified so nearby stars stay selectable).
 */
UCLASS()
class DELTAVDOMINION_API ADVStarSystem : public AActor
{
	GENERATED_BODY()

public:
	ADVStarSystem();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Star System") FString SystemName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Star System") FString Region;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Star System") float DistanceLy = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Star System", meta = (MultiLine = true)) FString Description;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Star System") EDVControl Control = EDVControl::Uncharted;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Star System") FLinearColor StarColor = FLinearColor(1.f, 0.9f, 0.7f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Star System") float MarkerSize = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Star System") TArray<FDVOperation> Operations;
	/** Sol: draws the home world Gaia beside the star. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Star System") bool bHomeSystem = false;

	FLinearColor GetControlColor() const;
	FString GetControlName() const;

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Star;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Halo;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Gaia;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> GaiaAtmosphere;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> GaiaMoon;

private:
	void ApplyLook();
};
