#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DVPressureManifold.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;

/**
 * Brasshaven: the Steamheart foundry's pressure manifold, an engineering puzzle.
 *
 * Three valve wheels feed three gauges through shared pipes. Each turn of a valve adds one step of pressure to every
 * gauge it feeds (A feeds 1 and 2, B feeds all three, C feeds 2 and 3). A gauge has five steps (0 empty .. 4 red); one
 * more past red trips its safety valve and it vents back to empty. The job: all three needles in the green (step 2).
 *
 * The coupling matrix is invertible mod 5, so every starting state has exactly one answer (counting turns mod 5).
 * Turning at random rarely gets there; reading which gauges a valve moves does.
 */
UCLASS()
class DELTAVDOMINION_API ADVPressureManifold : public AActor
{
	GENERATED_BODY()

public:
	ADVPressureManifold();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	static constexpr int32 NumValves = 3;
	static constexpr int32 Steps = 5;
	static constexpr int32 Green = 2;
	static constexpr float Reach = 230.f;

	static ADVPressureManifold* Find(const UWorld* World);

	/** The valve wheel User can reach right now, or -1. */
	int32 FindValveNear(const AActor* User) const;
	/** Server: one quarter-turn of valve Index. */
	void TurnValve(int32 Index);
	/** Which gauges valve Index feeds. */
	static bool Feeds(int32 Valve, int32 Gauge);

	int32 GetGauge(int32 Index) const { return Gauges.IsValidIndex(Index) ? Gauges[Index] : 0; }
	int32 GetTurns() const { return TotalTurns; }
	bool IsSolved() const { return bSolved; }
	/** Gauges in the green, 0..1. */
	float GetBalance() const;
	FVector GetValveLocation(int32 Index) const;
	static FString ValveName(int32 Index);

	/** Dev builds: the turns (0..4 each) that solve the current state. */
	void GetSolution(int32 OutTurns[NumValves]) const;

protected:
	UFUNCTION() void OnRep_Gauges();
	UFUNCTION(NetMulticast, Unreliable) void MulticastTurned(uint8 Index, uint8 VentMask);

	UPROPERTY(ReplicatedUsing = OnRep_Gauges) TArray<uint8> Gauges;
	UPROPERTY(Replicated) TArray<uint8> ValveTurns;
	UPROPERTY(Replicated) int32 TotalTurns = 0;
	UPROPERTY(Replicated) bool bSolved = false;

private:
	UStaticMeshComponent* Part(class UStaticMesh* Mesh, class UMaterialInterface* Material, const FVector& Loc, const FVector& SizeCm, const FRotator& Rot = FRotator::ZeroRotator, USceneComponent* Parent = nullptr);
	UStaticMeshComponent* Glow(const FVector& Loc, const FVector& SizeCm, const FLinearColor& Color, float Strength, USceneComponent* Parent = nullptr);
	void Scramble();

	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> Wheels;
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> Needles;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Lamps;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Beacon;
	UPROPERTY() TObjectPtr<UPointLightComponent> BeaconLight;
	float WheelAngle[NumValves] = { 0.f, 0.f, 0.f };
	float NeedleAngle[NumValves] = { 0.f, 0.f, 0.f };
	bool bBuilt = false;
};
