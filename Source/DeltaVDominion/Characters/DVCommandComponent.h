#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Characters/DVSquadTypes.h"
#include "Net/DVRateLimiter.h"
#include "DVCommandComponent.generated.h"

class ADVSoldierCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDVOnOrderIssued, EDVSquadOrder, Order, FVector, Location);

/**
 * Squad leader's command interface (move, hold, flank, suppress, regroup).
 * The local player aims at a spot or enemy and issues an order; the server validates
 * and rate-limits it, then hands it to each AI squadmate's controller.
 * Co-op players who took over a squadmate receive the order as a HUD/VO prompt instead.
 */
UCLASS(ClassGroup = (DeltaV), meta = (BlueprintSpawnableComponent))
class DELTAVDOMINION_API UDVCommandComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDVCommandComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Local: trace from the camera to pick the order location / target, then send to the server. */
	UFUNCTION(BlueprintCallable, Category = "Command")
	void IssueOrderFromView(EDVSquadOrder Order);

	/** Issue an order at an explicit spot / target (scripted sequences, showcase, future command wheel). */
	UFUNCTION(BlueprintCallable, Category = "Command")
	void IssueOrderAt(EDVSquadOrder Order, FVector Location, AActor* Target);

	/** Server: adds a squadmate under this leader. */
	void RegisterSquadMember(ADVSoldierCharacter* Member);

	UFUNCTION(BlueprintPure, Category = "Command")
	TArray<ADVSoldierCharacter*> GetLivingSquad() const;

	UPROPERTY(BlueprintAssignable) FDVOnOrderIssued OnOrderIssued;

	/** Max distance (cm) from the leader an order can target. */
	UPROPERTY(EditDefaultsOnly, Category = "Command") float MaxOrderDistance = 15000.f;

protected:
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerIssueOrder(EDVSquadOrder Order, FVector_NetQuantize Location, AActor* Target);

	UFUNCTION(Client, Reliable)
	void ClientOrderAcknowledged(EDVSquadOrder Order, FVector_NetQuantize Location);

	UPROPERTY(Replicated) TArray<TObjectPtr<ADVSoldierCharacter>> SquadMembers;

private:
	void DispatchOrder(const FDVSquadOrderData& Data);

	FDVRateLimiter OrderLimiter{ 3.f, 5.f };
};
