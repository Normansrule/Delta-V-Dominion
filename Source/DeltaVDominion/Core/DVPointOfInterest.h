#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DVPointOfInterest.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;

UENUM(BlueprintType)
enum class EDVPointKind : uint8
{
	Intel,		// a field case with a data slate: walk over it to recover it (three per operation: a challenge)
	Survivor,	// someone stranded in a crashed escape pod: stay beside the pod while they get out (a challenge)
	Cache,		// an Accord supply crate: refills the ammo, health and armour of whoever opens it
	Wreck,		// a downed craft to strip: requisition for the quartermaster
};

/**
 * Something worth leaving the main path for. Placed on every operation map by Scripts/place_points_of_interest.py
 * (settings, then a Poisson-disk generator, then world data, then this actor as the renderer). Server-authoritative:
 * the server decides when it's taken and tells the game mode, which counts it toward the operation's challenges.
 */
UCLASS()
class DELTAVDOMINION_API ADVPointOfInterest : public AActor
{
	GENERATED_BODY()

public:
	ADVPointOfInterest();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, Category = "Point") EDVPointKind Kind = EDVPointKind::Intel;

	bool IsTaken() const { return bTaken; }
	/** 0..1 while a survivor is getting out. */
	float GetProgress() const { return Progress; }
	FString Label() const;
	static constexpr float Reach = 260.f;
	static constexpr float RescueSeconds = 4.f;

protected:
	UFUNCTION() void OnRep_Taken();
	UPROPERTY(ReplicatedUsing = OnRep_Taken) bool bTaken = false;
	UPROPERTY(Replicated) float Progress = 0.f;
	void Take(class ADVSoldierCharacter* By);

	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Parts;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Beacon;
	UPROPERTY() TObjectPtr<UPointLightComponent> Light;
	UStaticMeshComponent* Part(class UStaticMesh* Mesh, const FVector& Loc, const FVector& Size, const FLinearColor& Color, float Glow, const FRotator& Rot = FRotator::ZeroRotator);
};
