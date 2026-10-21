#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "DVHollyhock.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;

/**
 * Hollyhock Vale: a storm-broken windpump. Stand beside it and it gets fixed (RepairSeconds of someone standing there;
 * it slowly loses progress when nobody is). Once fixed, the rotor turns and the lamp goes green. Server-authoritative.
 */
UCLASS()
class DELTAVDOMINION_API ADVRepairPoint : public AActor
{
	GENERATED_BODY()

public:
	ADVRepairPoint();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, Category = "Repair") FString DisplayName = TEXT("Windpump");
	UPROPERTY(EditAnywhere, Category = "Repair") float RepairSeconds = 5.f;
	static constexpr float Reach = 320.f;

	bool IsFixed() const { return bFixed; }
	float GetProgress() const { return Progress; }

protected:
	UFUNCTION() void OnRep_Fixed();
	UPROPERTY(Replicated) float Progress = 0.f;
	UPROPERTY(ReplicatedUsing = OnRep_Fixed) bool bFixed = false;

	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Rotor;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Lamp;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Spark;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> LooseBlade;
	UPROPERTY() TObjectPtr<UPointLightComponent> Light;
	float RotorAngle = 0.f;
	float RotorSpeed = 0.f;
};

/** Hollyhock Vale: the barn pen the puffbots are herded into. Placed by the map builder, which also builds its fence. */
UCLASS()
class DELTAVDOMINION_API ADVPen : public AActor
{
	GENERATED_BODY()

public:
	ADVPen();
	UPROPERTY(EditAnywhere, Category = "Pen") float Radius = 520.f;
	static ADVPen* Find(const UWorld* World);
	bool Contains(const FVector& Location) const { return FVector::Dist2D(Location, GetActorLocation()) < Radius; }

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
};

/**
 * A puffbot: a round little farm robot the storm scattered. It runs from anyone who comes close, so you herd it: come
 * at it from the far side and walk it into the pen. Once inside it stays and hops. Never harmed; bullets pass it by.
 */
UCLASS()
class DELTAVDOMINION_API ADVPuffbot : public ACharacter
{
	GENERATED_BODY()

public:
	ADVPuffbot();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, Category = "Puffbot") FString DisplayName = TEXT("Puffbot");
	/** How close a soldier gets before it bolts. */
	static constexpr float Skittish = 650.f;

	bool IsPenned() const { return bPenned; }

protected:
	UPROPERTY(Replicated) bool bPenned = false;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Visual;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Tip;
	FVector Home = FVector::ZeroVector;
	double NextThink = 0.0;
	int32 Seed = 0;
};
