#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DVKestrelMemory.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class ADVTownsfolk;

/**
 * The Kestrel remembers. Placed at the hangar's origin (build_kestrel_hub.py); on BeginPlay it reads the roster and
 * the operations log and dresses the deck to match what actually happened:
 *  - a name plate on the memorial wall and a candle for every soldier lost;
 *  - a sealed locker (red seal, folded flag) for each of the fallen;
 *  - medbay beds with the wounded lying in them, and the doctor;
 *  - a repair scaffold and deck crew around the Raven if the last operation went badly or cost lives;
 *  - a Bulwark head on the trophy shelf for every one destroyed;
 *  - Port Halden families on the deck after Breakwater;
 *  - an operations board with a green or red light per operation.
 * Everything is rebuilt from saved data, so it survives quitting and reloading. Works inside the streamed hangar too.
 */
UCLASS()
class DELTAVDOMINION_API ADVKestrelMemory : public AActor
{
	GENERATED_BODY()

public:
	ADVKestrelMemory();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	/** Runtime kit part in hangar space (cm): a box/cylinder sized SizeCm, centred at Loc. */
	UStaticMeshComponent* Part(UStaticMesh* Mesh, const TCHAR* Material, const FVector& Loc, const FVector& SizeCm, const FRotator& Rot = FRotator::ZeroRotator);
	UStaticMeshComponent* Glow(const FVector& Loc, const FVector& SizeCm, const FLinearColor& Color, float Strength);
	UPointLightComponent* Lamp(const FVector& Loc, const FLinearColor& Color, float Candelas, float Radius);
	ADVTownsfolk* Person(const FVector& Loc, float Yaw, const FString& Name, const FString& Line, int32 Trade, bool bLying = false);

	void BuildMemorial(const TArray<struct FDVSoldierRecord>& Fallen);
	void BuildMedbay(const TArray<struct FDVSoldierRecord>& Wounded);
	void BuildRepairs(const struct FDVOpRecord& Last);
	void BuildTrophies(int32 Count);
	void BuildEvacuees();
	void BuildOpsBoard(const TArray<struct FDVOpRecord>& Log);

	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Candles;
	UPROPERTY() TObjectPtr<UPointLightComponent> WeldLight;
};
