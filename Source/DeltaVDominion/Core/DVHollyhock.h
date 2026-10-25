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

/** Hollyhock's festival: one lantern-firework burst, purely cosmetic and local (spawned by ADVGameState::MulticastFirework). */
UCLASS()
class DELTAVDOMINION_API ADVFirework : public AActor
{
	GENERATED_BODY()

public:
	ADVFirework();
	void Burst(const FLinearColor& Color);
	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Sparks;
	TArray<FVector> Velocities;
	FVector BaseScale = FVector::OneVector;
	float Age = 0.f;
	static constexpr float Life = 2.4f;
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
	bool Contains(const FVector& Location) const
	{
		// Inside the fence, or standing in the gateway (the gate faces +X): once it's through the gate it's home.
		const FVector D = Location - GetActorLocation();
		return D.Size2D() < Radius + 30.f || (D.X > 0.f && FMath::Abs(D.Y) < 230.f && D.Size2D() < Radius + 160.f);
	}

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
};

/**
 * A puffbot: a round little farm robot the storm scattered. Walk up to one gently and it follows you like a duckling;
 * come sprinting and it scatters. Lead it near the barn and it trots into the pen by itself, then stays and hops.
 * Never harmed; bullets pass it by.
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
	/** How close a soldier gets before it notices them. */
	static constexpr float Skittish = 650.f;
	/** Faster than this (a sprint) is frightening. Walking is 420. */
	static constexpr float Scary = 500.f;
	/** A gentle soldier this close becomes the one it follows. */
	static constexpr float Befriend = 320.f;

	bool IsPenned() const { return bPenned; }
	bool IsFollowing(const APawn* Who) const { return Who && Leader.Get() == Who; }

protected:
	UPROPERTY(Replicated) bool bPenned = false;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Visual;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Tip;
	FVector Home = FVector::ZeroVector;
	double NextThink = 0.0;
	int32 Seed = 0;
	TWeakObjectPtr<const APawn> Leader;
	/** Where it wants to go right now (fleeing or heading home), so it can still waddle there when pathfinding fails. */
	FVector WantDir = FVector::ZeroVector;
	double StuckSince = -1.0;
	int32 StuckTurns = 0;
	void Want(const FVector& Dir) { WantDir = Dir.GetSafeNormal2D(); }
};
