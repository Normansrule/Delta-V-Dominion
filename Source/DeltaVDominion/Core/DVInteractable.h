#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DVInteractable.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;

UENUM(BlueprintType)
enum class EDVInteraction : uint8
{
	WarTable,	// opens the galaxy map to pick the next operation
	Memorial,	// shows the fallen and the living roster
	Armoury,	// cycle weapon loadouts
	Locker,		// commander customisation (look, optics, callsign)
};

/**
 * Something on the Kestrel you can walk up to and use with Z: the war table, the memorial wall, the armoury.
 * Visuals are kit-bashed per type; the HUD shows the prompt when you're within reach.
 */
UCLASS()
class DELTAVDOMINION_API ADVInteractable : public AActor
{
	GENERATED_BODY()

public:
	ADVInteractable();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact") EDVInteraction Interaction = EDVInteraction::WarTable;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact") FString Prompt = TEXT("Use");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact") float Reach = 380.f;

	static ADVInteractable* FindNear(const AActor* User);

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Hologram;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Glows;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> Light;
};
