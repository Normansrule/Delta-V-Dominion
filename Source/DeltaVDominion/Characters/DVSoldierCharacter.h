#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GenericTeamAgentInterface.h"
#include "Net/DVRateLimiter.h"
#include "Core/DVProfileSubsystem.h"
#include "DVSoldierCharacter.generated.h"

class UCameraComponent;
class USkeletalMeshComponent;
class USkeletalMesh;
class UStaticMeshComponent;
class UAbilitySystemComponent;
class UGameplayAbility;
class UDVAttributeSet;
class UDVCommandComponent;
class UDVCharacterMovementComponent;
class UDVWeaponData;
class ADVWeaponBase;
class UInputMappingContext;
class UInputAction;
class UAnimMontage;
struct FInputActionValue;
struct FDVSoldierRecord;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDVOnSoldierDied, ADVSoldierCharacter*, Soldier);

/**
 * The soldier used by the player, AI squadmates, co-op players and enemies alike.
 *
 * - First-person camera + arms mesh for the local player, full body for everyone else.
 * - GAS: owns its AbilitySystemComponent (so AI and co-op takeover keep state with the pawn).
 * - Weighty movement: sprint/ADS speeds predicted through the movement component, crouch, lean.
 * - Optional link to a persistent roster record (RosterId). When a linked soldier dies, the
 *   game mode reports them KIA to the roster.
 */
UCLASS()
class DELTAVDOMINION_API ADVSoldierCharacter : public ACharacter, public IAbilitySystemInterface, public IGenericTeamAgentInterface
{
	GENERATED_BODY()

public:
	ADVSoldierCharacter(const FObjectInitializer& ObjectInitializer);

	// AActor / APawn
	virtual void BeginPlay() override;
	virtual void Destroyed() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void OnRep_Controller() override;
	virtual void OnRep_PlayerState() override;
	virtual void NotifyControllerChanged() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual FVector GetPawnViewLocation() const override;

	// IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// IGenericTeamAgentInterface
	virtual void SetGenericTeamId(const FGenericTeamId& NewTeamId) override;
	virtual FGenericTeamId GetGenericTeamId() const override { return FGenericTeamId(TeamId); }
	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;

	// --- Roster -------------------------------------------------------------
	/** Server: bind this pawn to a persistent soldier and apply their stats. */
	void ApplyRosterRecord(const FDVSoldierRecord& Record);

	UFUNCTION(BlueprintPure, Category = "Soldier") FGuid GetRosterId() const { return RosterId; }
	UFUNCTION(BlueprintPure, Category = "Soldier") FString GetSoldierName() const { return SoldierName; }

	// --- Combat -------------------------------------------------------------
	UFUNCTION(BlueprintPure, Category = "Soldier") ADVWeaponBase* GetCurrentWeapon() const { return CurrentWeapon; }
	/** Server: swap the carried weapon's profile (armoury), with a full magazine and reserve. */
	void EquipWeaponData(UDVWeaponData* Data);
	UFUNCTION(BlueprintPure, Category = "Soldier") bool IsAiming() const { return bIsAiming; }
	UFUNCTION(BlueprintPure, Category = "Soldier") bool IsDead() const { return bIsDead; }
	/** Down, not dead: bleeding out until a squadmate revives them (Accord soldiers only). */
	UFUNCTION(BlueprintPure, Category = "Soldier") bool IsDowned() const { return bDowned; }
	bool IsOutOfAction() const { return bIsDead || bDowned; }

	/** Cosmetics for movement abilities (called by the movement component on the owning client and server). */
	void OnThrusterBoost();
	void OnSlideStarted();
	float GetBoostCharges() const;
	float GetMaxBoostCharges() const;

	/** The commander's customised look (replicated; applied over the team paint). */
	UFUNCTION(Server, Reliable) void ServerSetLook(const FDVLook& NewLook);
	/** The commander's level (perks are applied on the server from it). */
	UFUNCTION(Server, Reliable) void ServerSetCommanderLevel(int32 Level);
	int32 GetCommanderLevel() const { return CommanderLevel; }
	/** Show the whole body (head included) for the locker mirror view. */
	void SetShowFullBody(bool bShow);
	float GetBleedOutRemaining() const;
	float GetReviveProgress() const { return ReviveProgress; }
	/** Server: a player holding Z next to this soldier (AI squadmates revive automatically). */
	void SetReviveHeld(bool bHeld) { bReviveHeld = bHeld; }
	bool IsReviveHeld() const { return bReviveHeld; }

	static constexpr float BleedOutSeconds = 30.f;
	static constexpr float ReviveSeconds = 4.f;
	static constexpr float ReviveRange = 190.f;
	/** Extra damage a downed soldier can take before they are finished off. */
	static constexpr float DownedHealth = 60.f;
	UFUNCTION(BlueprintPure, Category = "Soldier") float GetLean() const { return LeanAmount; }
	UFUNCTION(BlueprintPure, Category = "Soldier") float GetHealth() const;
	UFUNCTION(BlueprintPure, Category = "Soldier") float GetSuppression() const;
	UDVAttributeSet* GetAttributeSet() const { return AttributeSet; }
	UDVCommandComponent* GetCommandComponent() const { return CommandComponent; }
	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }
	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }

	/** Attempts one activation of the fire ability (also used by AI). Auto-reloads on empty. */
	UFUNCTION(BlueprintCallable, Category = "Soldier") bool TryFire();
	UFUNCTION(BlueprintCallable, Category = "Soldier") bool TryReload();

	/** Local: AI and input both route aiming through here; replicates via movement flags. */
	UFUNCTION(BlueprintCallable, Category = "Soldier") void SetAiming(bool bAim);
	UFUNCTION(BlueprintCallable, Category = "Soldier") void SetSprinting(bool bSprint);
	/** -1 (left) .. 1 (right). */
	UFUNCTION(BlueprintCallable, Category = "Soldier") void SetLean(float Amount);

	void ApplyRecoil(const FVector2D& Kick);
	void PlayWeaponMontage(UAnimMontage* Montage);

	UPROPERTY(BlueprintAssignable) FDVOnSoldierDied OnDied;

	// --- Tuning ---------------------------------------------------------------
	UPROPERTY(EditDefaultsOnly, Category = "Soldier|Weapon") TObjectPtr<UDVWeaponData> DefaultWeapon;
	UPROPERTY(EditDefaultsOnly, Category = "Soldier|Weapon") TSubclassOf<ADVWeaponBase> WeaponClass;
	UPROPERTY(EditDefaultsOnly, Category = "Soldier|Abilities") TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;
	UPROPERTY(EditAnywhere, Replicated, Category = "Soldier|Team") uint8 TeamId = 1;
	/** Body used for non-player teams (the Hearthbound). Falls back to Mesh when unset. */
	UPROPERTY(EditDefaultsOnly, Category = "Soldier|Appearance") TObjectPtr<USkeletalMesh> HostileBodyMesh;
	/** Armour paint and visor glow per team (mannequin "Paint Tint" / emissive). */
	UPROPERTY(EditDefaultsOnly, Category = "Soldier|Appearance") FLinearColor FriendlyPaint = FLinearColor(0.16f, 0.2f, 0.13f);
	UPROPERTY(EditDefaultsOnly, Category = "Soldier|Appearance") FLinearColor HostilePaint = FLinearColor(0.03f, 0.03f, 0.035f);
	UPROPERTY(EditDefaultsOnly, Category = "Soldier|Camera") float DefaultFOV = 90.f;
	UPROPERTY(EditDefaultsOnly, Category = "Soldier|Camera") float LookSensitivity = 0.4f;
	UPROPERTY(EditDefaultsOnly, Category = "Soldier|Camera") float LeanAngle = 12.f;
	UPROPERTY(EditDefaultsOnly, Category = "Soldier|Camera") float LeanOffset = 35.f;
	/** Suppression lost per second when not under fire. */
	UPROPERTY(EditDefaultsOnly, Category = "Soldier|Combat") float SuppressionDecayPerSecond = 15.f;

	// --- Input (assign assets in a Blueprint child, or leave empty to use built-in defaults) ---
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputMappingContext> DefaultMappingContext;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> MoveAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> LookAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> JumpAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> FireAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> AimAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> ReloadAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> SprintAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> CrouchAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> DashAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> LeanLeftAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> LeanRightAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> OrderMoveAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> OrderHoldAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> OrderRegroupAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> OrderSuppressAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> OrderFlankAction;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UCameraComponent> FirstPersonCamera;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<USkeletalMeshComponent> FirstPersonMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UDVCommandComponent> CommandComponent;
	/** Grey-box body shown until a skeletal Mesh is assigned, tinted by team (blue = friendly, red = Hearthbound). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UStaticMeshComponent> PlaceholderBody;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UStaticMeshComponent> PlaceholderVisor;
	UPROPERTY() TObjectPtr<UDVAttributeSet> AttributeSet;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentWeapon) TObjectPtr<ADVWeaponBase> CurrentWeapon;
	UPROPERTY(Replicated) FGuid RosterId;
	UPROPERTY(Replicated) FString SoldierName;
	UPROPERTY(Replicated) bool bIsAiming = false;
	UPROPERTY(Replicated) float LeanAmount = 0.f;
	UPROPERTY(ReplicatedUsing = OnRep_IsDead) bool bIsDead = false;
	UPROPERTY(ReplicatedUsing = OnRep_Downed) bool bDowned = false;
	UPROPERTY(ReplicatedUsing = OnRep_Look) FDVLook Look;
	UPROPERTY(ReplicatedUsing = OnRep_CommanderLevel) int32 CommanderLevel = 0;
	UPROPERTY() TObjectPtr<USkeletalMesh> DefaultBodyMesh;
	bool bShowFullBody = false;
	UPROPERTY(Replicated) float BleedOutEndTime = 0.f;
	UPROPERTY(Replicated) float ReviveProgress = 0.f;
	bool bReviveHeld = false;
	TWeakObjectPtr<AActor> DownedBy;

	UFUNCTION() void OnRep_CurrentWeapon();
	UFUNCTION() void OnRep_IsDead();
	UFUNCTION() void OnRep_Downed();
	UFUNCTION() void OnRep_Look();
	UFUNCTION() void OnRep_CommanderLevel();

	UFUNCTION(Server, Unreliable) void ServerSetLean(float Amount);

private:
	void InitAbilitySystem();
	void GrantDefaultAbilities();
	void SpawnDefaultWeapon();
	void HandleOutOfHealth(AActor* DamageInstigator, AActor* DamageCauser);
	void Die(AActor* DamageInstigator);
	void EnterDowned(AActor* DamageInstigator);
	void Revive();
	void TickDowned(float DeltaSeconds);
	void EnterRagdoll();
	void SetupPlaceholderBody();
	void ApplyTeamAppearance();
	/** Local player sees their own body (head hidden) instead of floating arms. */
	void UpdateFirstPersonBody();
	void CreateFallbackInput();
	UDVCharacterMovementComponent* GetDVMovement() const;

	// Input handlers
	void Input_Move(const FInputActionValue& Value);
	void Input_Look(const FInputActionValue& Value);
	void Input_Jump();
	void Input_Dash();
	void Input_FireStarted();
	void Input_FireCompleted();
	void Input_AimStarted() { SetAiming(true); }
	void Input_AimCompleted() { SetAiming(false); }
	void Input_SprintStarted() { SetSprinting(true); }
	void Input_SprintCompleted() { SetSprinting(false); }
	void Input_Crouch();
	void Input_Reload() { TryReload(); }
	void Input_LeanLeft() { SetLean(LeanAmount < -0.5f ? 0.f : -1.f); }
	void Input_LeanRight() { SetLean(LeanAmount > 0.5f ? 0.f : 1.f); }
	void Input_OrderMove();
	void Input_OrderHold();
	void Input_OrderRegroup();
	void Input_OrderSuppress();
	void Input_OrderFlank();

	bool bAbilitiesGranted = false;
	bool bFireHeld = false;
	int32 BurstShotsRemaining = 0;
	float CurrentLeanVisual = 0.f;
	FVector CameraBaseOffset = FVector::ZeroVector;
	FDVRateLimiter LeanRpcLimiter{ 10.f, 10.f };
};
