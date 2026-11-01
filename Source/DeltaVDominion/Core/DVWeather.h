#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DVWeather.generated.h"

class UExponentialHeightFogComponent;

/**
 * Weather that changes the fight: every couple of minutes a front rolls in (dust, ash, snow, fog), the air thickens and
 * every sentry's eyes shorten to a few tens of metres, then it clears. Move in the storm, hold in the clear.
 * The server runs the cycle and replicates how strong the front is; every machine thickens its own fog to match.
 */
UCLASS()
class DELTAVDOMINION_API ADVWeather : public AActor
{
	GENERATED_BODY()

public:
	ADVWeather();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	/** What the front is called on the HUD ("Dust storm"). */
	UPROPERTY(EditAnywhere, Replicated, Category = "Weather") FString FrontName = TEXT("Dust storm");
	UPROPERTY(EditAnywhere, Category = "Weather") float CalmMin = 75.f;
	UPROPERTY(EditAnywhere, Category = "Weather") float CalmMax = 120.f;
	UPROPERTY(EditAnywhere, Category = "Weather") float StormSeconds = 38.f;
	UPROPERTY(EditAnywhere, Category = "Weather") float RampSeconds = 6.f;
	/** Fog density multiplier at the height of the front. */
	UPROPERTY(EditAnywhere, Category = "Weather") float FogScale = 9.f;
	/** Fog colour the front tints toward. */
	UPROPERTY(EditAnywhere, Category = "Weather") FLinearColor FrontColor = FLinearColor(0.5f, 0.3f, 0.18f, 1.f);
	/** Sight radius every AI keeps at the height of the front, as a fraction of its normal sight. */
	UPROPERTY(EditAnywhere, Category = "Weather") float SightFraction = 0.33f;

	/** 0 clear, 1 the front at full strength. */
	UFUNCTION(BlueprintPure, Category = "Weather") float GetStrength() const { return Strength; }
	/** Seconds until the next front arrives (0 while one is blowing). Server only. */
	float GetCalmLeft() const { return FMath::Max(0.f, float(NextFront - GetWorld()->GetTimeSeconds())); }
	/** True on the server for a few seconds before a front: Halloran calls it in. */
	bool IsWarning() const { return Strength <= 0.f && GetCalmLeft() < 10.f; }

	static ADVWeather* Find(const UWorld* World);

	/** Tests: bring the next front in after Delay seconds. */
	void ForceFront(float Delay);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(Replicated) float Strength = 0.f;

	double NextFront = 0.0;
	double FrontStart = -1.0;
	float AppliedSight = 1.f;
	bool bWarned = false;

	TWeakObjectPtr<UExponentialHeightFogComponent> Fog;
	float BaseDensity = 0.f;
	FLinearColor BaseColor = FLinearColor::Black;

	void ApplySight(float Fraction);
};
