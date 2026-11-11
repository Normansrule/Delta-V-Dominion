#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DVGrenade.generated.h"

class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class ADVSoldierCharacter;

/**
 * A frag grenade: thrown along an arc, bounces and rolls, blinks while the fuse burns, then blows. Server-authoritative
 * (the throw is a server RPC on the soldier); movement replicates and every machine plays the blast.
 */
UCLASS()
class DELTAVDOMINION_API ADVGrenade : public AActor
{
	GENERATED_BODY()

public:
	ADVGrenade();

	UPROPERTY(EditDefaultsOnly, Category = "Grenade") float Fuse = 2.6f;
	UPROPERTY(EditDefaultsOnly, Category = "Grenade") float Radius = 650.f;
	UPROPERTY(EditDefaultsOnly, Category = "Grenade") float Damage = 150.f;

	/** Server: send it on its way from Start along Velocity, thrown by Thrower. */
	void Launch(ADVSoldierCharacter* Thrower, const FVector& Velocity);

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	void Detonate();
	UFUNCTION(NetMulticast, Reliable) void MulticastBlast(FVector_NetQuantize Location);

	UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> Collision;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> Blink;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UProjectileMovementComponent> Movement;

	TWeakObjectPtr<ADVSoldierCharacter> ThrownBy;
	double ArmedAt = 0.0;
	bool bBlown = false;
};
