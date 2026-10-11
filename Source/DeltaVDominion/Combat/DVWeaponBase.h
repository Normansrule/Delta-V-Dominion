#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Net/DVRateLimiter.h"
#include "DVWeaponBase.generated.h"

class UDVWeaponData;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class ADVSoldierCharacter;

/**
 * A carried weapon. Ammo, heat and fire timing are server-authoritative; the owning
 * client predicts them for responsiveness and is corrected by replication.
 * The firing logic itself lives in UDVGA_FireWeapon.
 */
UCLASS()
class DELTAVDOMINION_API ADVWeaponBase : public AActor
{
	GENERATED_BODY()

public:
	ADVWeaponBase();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	/** Server: set data (nullptr = class defaults) and fill ammo. */
	void InitializeWeapon(UDVWeaponData* InData);

	/** Attach to the right mesh for this machine (first-person arms for the local player, body otherwise). */
	void AttachToSoldier(ADVSoldierCharacter* Soldier);

	/** Assigned data asset, or the class defaults (an M9 Bastion profile) when none is set. */
	const UDVWeaponData* GetData() const;
	UFUNCTION(BlueprintPure, Category = "Weapon") int32 GetAmmoInMag() const { return AmmoInMag; }
	UFUNCTION(BlueprintPure, Category = "Weapon") int32 GetReserveAmmo() const { return ReserveAmmo; }
	UFUNCTION(BlueprintPure, Category = "Weapon") float GetHeat() const { return Heat; }
	UFUNCTION(BlueprintPure, Category = "Weapon") bool IsOverheated() const { return bOverheated; }

	/** Local check used for activation: ammo/heat and the fire-rate cooldown. */
	bool CanFire(double Now, bool bLenientTiming = false) const;
	bool NeedsReload() const;
	bool CanReload() const;

	/** Called once per trigger pull (all pellets) on both the predicting client and the server. */
	void ConsumeShot(double Now);

	/** Server: rejects shots arriving faster than the weapon can cycle (with jitter tolerance). */
	bool ServerValidateShotTiming(double Now);

	/** Server: moves rounds from reserve into the magazine. */
	void ServerFinishReload();

	/** Current cone half-angle in degrees, including bloom and suppression. */
	float GetCurrentSpread(bool bAiming, float Suppression01) const;

	/** Next recoil kick, advancing the pattern. Local only. */
	FVector2D ConsumeRecoil(double Now, bool bAiming);

	FVector GetMuzzleLocation() const;

	/** Cosmetics for the shooter's own machine. */
	void PlayLocalFireEffects(const TArray<FVector>& ImpactPoints);

	/** Cosmetics for everyone else (server -> simulated proxies). */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastFireEffects(const TArray<FVector_NetQuantize>& ImpactPoints);

	/** Rate limiter shared by fire/reload server paths for this weapon. */
	FDVRateLimiter ServerRpcLimiter{ 30.f, 30.f };

protected:
	UFUNCTION() void OnRep_Data();

	UPROPERTY(VisibleAnywhere, Category = "Weapon") TObjectPtr<USkeletalMeshComponent> Mesh;
	/** Grey-box rifle shown until the data asset supplies a Mesh. */
	UPROPERTY(VisibleAnywhere, Category = "Weapon") TObjectPtr<UStaticMeshComponent> PlaceholderMesh;
	/** Used for grey-box tracers when the data asset has no Niagara tracer. */
	UPROPERTY() TObjectPtr<UStaticMesh> PlaceholderTracerMesh;
	UPROPERTY() TObjectPtr<UMaterialInterface> PlaceholderMaterial;

	UPROPERTY(ReplicatedUsing = OnRep_Data) TObjectPtr<UDVWeaponData> WeaponData;
	UPROPERTY(Replicated) int32 AmmoInMag = 0;
	UPROPERTY(Replicated) int32 ReserveAmmo = 0;
	UPROPERTY(Replicated) float Heat = 0.f;
	UPROPERTY(Replicated) bool bOverheated = false;

private:
	void SpawnShotEffects(const TArray<FVector>& ImpactPoints) const;
	void SpawnPlaceholderTracer(const FVector& From, const FVector& To) const;
	bool HasRealMesh() const;

	double LastShotTime = -1000.0;		// local / predicted
	double LastServerShotTime = -1000.0;
	int32 ServerEarlyShots = 0;
	float Bloom = 0.f;
	int32 RecoilIndex = 0;
};
