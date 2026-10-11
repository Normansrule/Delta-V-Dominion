#pragma once

#include "CoreMinimal.h"
#include "Characters/DVSoldierCharacter.h"
#include "DVBulwark.generated.h"

class UStaticMeshComponent;

/**
 * Hearthbound "Bulwark" heavy war-frame: a third bigger than a trooper, slow, heavily armoured
 * (700 HP, 60% damage reduction) and carrying a rotary cannon that pins a squad down.
 * Missions add one or two near the enemy's centre of mass (see ADVGameMode::SpawnBulwarks).
 * Flank it, focus fire, or paint it for the Kestrel.
 */
UCLASS()
class DELTAVDOMINION_API ADVBulwark : public ADVSoldierCharacter
{
	GENERATED_BODY()

public:
	ADVBulwark(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;

	static constexpr float BulwarkHealth = 700.f;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Bulwark") TArray<TObjectPtr<UStaticMeshComponent>> ArmourPlates;
	UPROPERTY(VisibleAnywhere, Category = "Bulwark") TArray<TObjectPtr<UStaticMeshComponent>> Optics;
};
