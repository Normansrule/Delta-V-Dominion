#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DVWeaponData.generated.h"

class USkeletalMesh;
class UNiagaraSystem;
class USoundBase;
class UAnimMontage;

UENUM(BlueprintType)
enum class EDVFireMode : uint8
{
	Semi,
	Burst,
	Auto
};

/**
 * Tuning for one weapon (M9 Bastion, Kite SMG, Halberd, Longreach, Pulse Carbine...).
 * Create one data asset per weapon in the editor: Content Browser > Miscellaneous > Data Asset > DVWeaponData.
 */
UCLASS(BlueprintType)
class DELTAVDOMINION_API UDVWeaponData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity") FText DisplayName;

	// --- Fire ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire") EDVFireMode FireMode = EDVFireMode::Auto;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire", meta = (ClampMin = 30)) float RoundsPerMinute = 600.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire", meta = (EditCondition = "FireMode == EDVFireMode::Burst")) int32 BurstCount = 3;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire") float Damage = 28.f;
	/** >1 for shotguns. Damage is per pellet. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire", meta = (ClampMin = 1)) int32 PelletsPerShot = 1;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire") float MaxRange = 15000.f;

	// --- Accuracy (degrees, half-angle of the spread cone) ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Accuracy") float HipSpread = 2.5f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Accuracy") float AdsSpread = 0.35f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Accuracy") float SpreadPerShot = 0.25f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Accuracy") float MaxBloom = 3.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Accuracy") float BloomRecoveryPerSecond = 6.f;
	/** Extra spread at full suppression (100). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Accuracy") float SuppressedSpread = 2.f;

	// --- Recoil: per-shot view kick (X = pitch up, Y = yaw), looped after the last entry ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recoil") TArray<FVector2D> RecoilPattern = {
		{0.45f, 0.05f}, {0.5f, -0.08f}, {0.55f, 0.12f}, {0.5f, -0.15f}, {0.45f, 0.2f}, {0.4f, -0.1f}
	};
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recoil") float AdsRecoilMultiplier = 0.6f;
	/** Seconds without firing before the pattern resets. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recoil") float RecoilResetTime = 0.35f;

	// --- Ammo ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ammo", meta = (ClampMin = 1)) int32 MagazineSize = 30;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ammo") int32 StartingReserve = 180;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ammo") float ReloadTime = 2.2f;

	// --- Heat (Pulse Carbine: overheats instead of reloading) ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heat") bool bUsesHeat = false;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heat", meta = (EditCondition = "bUsesHeat")) float HeatPerShot = 0.06f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heat", meta = (EditCondition = "bUsesHeat")) float CooldownPerSecond = 0.35f;
	/** Heat must fall below this before an overheated weapon fires again. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heat", meta = (EditCondition = "bUsesHeat")) float OverheatRecoveredAt = 0.3f;

	// --- Suppression applied to enemies near the bullet path ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Suppression") float SuppressionRadius = 300.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Suppression") float SuppressionPerShot = 8.f;

	// --- ADS ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ADS") float AdsFOV = 65.f;

	// --- Presentation ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation") TObjectPtr<USkeletalMesh> Mesh;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation") FName MuzzleSocket = TEXT("Muzzle");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation") FName FirstPersonAttachSocket = TEXT("GripPoint");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation") FName ThirdPersonAttachSocket = TEXT("weapon_r");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation") TObjectPtr<UNiagaraSystem> MuzzleFlash;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation") TObjectPtr<UNiagaraSystem> Tracer;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation") TObjectPtr<UNiagaraSystem> ImpactFX;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation") TObjectPtr<USoundBase> FireSound;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation") TObjectPtr<UAnimMontage> FireMontage;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation") TObjectPtr<UAnimMontage> ReloadMontage;

	float GetFireInterval() const { return 60.f / FMath::Max(RoundsPerMinute, 1.f); }
};
