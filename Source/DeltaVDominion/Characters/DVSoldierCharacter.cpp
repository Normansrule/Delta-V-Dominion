#include "Characters/DVSoldierCharacter.h"
#include "Characters/DVCharacterMovementComponent.h"
#include "Characters/DVCommandComponent.h"
#include "Characters/DVSquadAIController.h"
#include "Combat/DVAttributeSet.h"
#include "Combat/DVWeaponBase.h"
#include "Combat/DVWeaponData.h"
#include "Core/DVPlayerController.h"
#include "Core/DVFx.h"
#include "AIController.h"
#include "EngineUtils.h"
#include "Combat/DVGA_FireWeapon.h"
#include "Combat/DVGA_Reload.h"
#include "Core/DVGameMode.h"
#include "Core/DVRosterTypes.h"
#include "Core/DVRosterSubsystem.h"
#include "DeltaVDominion.h"
#include "DVGameplayTags.h"

#include "AbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Net/UnrealNetwork.h"

ADVSoldierCharacter::ADVSoldierCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UDVCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(34.f, 92.f);
	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	// Eye height of the mannequin body, slightly forward so the hidden head never clips the view.
	FirstPersonCamera->SetRelativeLocation(FVector(14.f, 0.f, 70.f));
	FirstPersonCamera->bUsePawnControlRotation = true;
	FirstPersonCamera->SetFieldOfView(DefaultFOV);

	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonMesh"));
	FirstPersonMesh->SetupAttachment(FirstPersonCamera);
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->bCastDynamicShadow = false;
	FirstPersonMesh->CastShadow = false;
	FirstPersonMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FirstPersonMesh->SetRelativeLocation(FVector(-30.f, 0.f, -150.f));

	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->SetCollisionResponseToChannel(DV_TRACE_WEAPON, ECR_Block);

	// Default soldier body: the UE5 mannequin with rifle locomotion and aim offset. Blueprints can swap in real art.
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MannyMesh(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> QuinnMesh(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple"));
	static ConstructorHelpers::FClassFinder<UAnimInstance> RifleAnim(TEXT("/Game/Variant_Shooter/Anims/ABP_TP_Rifle"));
	if (MannyMesh.Succeeded())
	{
		GetMesh()->SetSkeletalMeshAsset(MannyMesh.Object);
		GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -92.f), FRotator(0.f, -90.f, 0.f));
	}
	if (RifleAnim.Succeeded())
	{
		GetMesh()->SetAnimInstanceClass(RifleAnim.Class);
	}
	HostileBodyMesh = QuinnMesh.Object;
	DefaultBodyMesh = MannyMesh.Object;
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;

	// Grey-box stand-in so the game is playable before character art exists. Hidden in BeginPlay once a Mesh is set.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	// Created by Scripts/build_bootcamp_map.py; exposes Color / Emissive parameters.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GreyBoxMat(TEXT("/Game/Dev/M_GreyBox.M_GreyBox"));

	PlaceholderBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderBody"));
	PlaceholderBody->SetupAttachment(GetCapsuleComponent());
	PlaceholderBody->SetStaticMesh(CylinderMesh.Object);
	PlaceholderBody->SetMaterial(0, GreyBoxMat.Object);
	PlaceholderBody->SetRelativeScale3D(FVector(0.62f, 0.62f, 1.8f));
	PlaceholderBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlaceholderBody->SetOwnerNoSee(true);

	PlaceholderVisor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderVisor"));
	PlaceholderVisor->SetupAttachment(GetCapsuleComponent());
	PlaceholderVisor->SetStaticMesh(CubeMesh.Object);
	PlaceholderVisor->SetMaterial(0, GreyBoxMat.Object);
	PlaceholderVisor->SetRelativeLocation(FVector(26.f, 0.f, 62.f));
	PlaceholderVisor->SetRelativeScale3D(FVector(0.15f, 0.4f, 0.12f));
	PlaceholderVisor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlaceholderVisor->SetOwnerNoSee(true);

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UDVAttributeSet>(TEXT("AttributeSet"));
	CommandComponent = CreateDefaultSubobject<UDVCommandComponent>(TEXT("Command"));

	DefaultAbilities = { UDVGA_FireWeapon::StaticClass(), UDVGA_Reload::StaticClass() };
	WeaponClass = ADVWeaponBase::StaticClass();

	AIControllerClass = ADVSquadAIController::StaticClass();
	// Placed enemies get an AI brain automatically; the game mode spawns brains for squadmates.
	AutoPossessAI = EAutoPossessAI::PlacedInWorld;
}

void ADVSoldierCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADVSoldierCharacter, CurrentWeapon);
	DOREPLIFETIME(ADVSoldierCharacter, RosterId);
	DOREPLIFETIME(ADVSoldierCharacter, SoldierName);
	DOREPLIFETIME(ADVSoldierCharacter, TeamId);
	DOREPLIFETIME(ADVSoldierCharacter, bClockwork);
	DOREPLIFETIME(ADVSoldierCharacter, bIsDead);
	DOREPLIFETIME(ADVSoldierCharacter, bDowned);
	DOREPLIFETIME(ADVSoldierCharacter, Look);
	DOREPLIFETIME(ADVSoldierCharacter, bAlerted);
	DOREPLIFETIME(ADVSoldierCharacter, CommanderLevel);
	DOREPLIFETIME(ADVSoldierCharacter, BleedOutEndTime);
	DOREPLIFETIME(ADVSoldierCharacter, ReviveProgress);
	DOREPLIFETIME_CONDITION(ADVSoldierCharacter, bIsAiming, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(ADVSoldierCharacter, LeanAmount, COND_SkipOwner);
}

void ADVSoldierCharacter::BeginPlay()
{
	Super::BeginPlay();

	CameraBaseOffset = FirstPersonCamera->GetRelativeLocation();

	// With a physics asset, bullets resolve against per-bone bodies (headshots); otherwise the capsule takes hits.
	const bool bHasPhysicsBodies = GetMesh()->GetSkeletalMeshAsset() && GetMesh()->GetPhysicsAsset();
	GetCapsuleComponent()->SetCollisionResponseToChannel(DV_TRACE_WEAPON, bHasPhysicsBodies ? ECR_Ignore : ECR_Block);
	FirstPersonCamera->SetFieldOfView(DefaultFOV);
	SetupPlaceholderBody();
	InitAbilitySystem();

	if (HasAuthority())
	{
		GrantDefaultAbilities();
		SpawnDefaultWeapon();
		AttributeSet->OnOutOfHealth.AddUObject(this, &ADVSoldierCharacter::HandleOutOfHealth);
	}
}

void ADVSoldierCharacter::ApplyTeamAppearance()
{
	if (TeamId != 1 && HostileBodyMesh && GetMesh()->GetSkeletalMeshAsset() != HostileBodyMesh)
	{
		GetMesh()->SetSkeletalMeshAsset(HostileBodyMesh);
	}
	const bool bFriendly = TeamId == 1;
	if (bFriendly && Look.bSet)
	{
		USkeletalMesh* Wanted = Look.Body == 1 ? HostileBodyMesh.Get() : DefaultBodyMesh.Get();
		if (Wanted && GetMesh()->GetSkeletalMeshAsset() != Wanted)
		{
			GetMesh()->SetSkeletalMeshAsset(Wanted);
		}
	}
	for (int32 i = 0; i < GetMesh()->GetNumMaterials(); ++i)
	{
		if (UMaterialInstanceDynamic* Mat = GetMesh()->CreateAndSetMaterialInstanceDynamic(i))
		{
			if (bFriendly && Look.bSet)
			{
				Mat->SetVectorParameterValue(TEXT("Paint Tint"), UDVProfileSubsystem::PaintColor(Look.Paint));
				Mat->SetVectorParameterValue(TEXT("LogoTint"), UDVProfileSubsystem::OpticsColor(Look.Optics));
				Mat->SetScalarParameterValue(TEXT("MetalPaintMetallic"), 0.35f);
				Mat->SetScalarParameterValue(TEXT("MetalPaintRoughness"), 0.45f);
				Mat->SetScalarParameterValue(TEXT("EmissivePower"), 14.f);
				continue;
			}
			if (!bFriendly && bClockwork)
			{
				// Guild constables: polished brass plate, warm amber optics behind glass.
				Mat->SetVectorParameterValue(TEXT("Paint Tint"), FLinearColor(0.5f, 0.32f, 0.1f));
				Mat->SetVectorParameterValue(TEXT("LogoTint"), FLinearColor(1.f, 0.55f, 0.12f));
				Mat->SetScalarParameterValue(TEXT("MetalPaintMetallic"), 1.f);
				Mat->SetScalarParameterValue(TEXT("MetalPaintRoughness"), 0.3f);
				Mat->SetScalarParameterValue(TEXT("EmissivePower"), bIsDead ? 0.f : 16.f);
				continue;
			}
			// Accord marines: matte olive armour, cyan unit markings.
			// Hearthbound war-frames: black chrome chassis, hot red optics and seams.
			Mat->SetVectorParameterValue(TEXT("Paint Tint"), bFriendly ? FriendlyPaint : HostilePaint);
			Mat->SetVectorParameterValue(TEXT("LogoTint"), bFriendly ? FLinearColor(0.2f, 0.7f, 1.f) : FLinearColor(1.f, 0.08f, 0.02f));
			Mat->SetScalarParameterValue(TEXT("MetalPaintMetallic"), bFriendly ? 0.2f : 1.f);
			Mat->SetScalarParameterValue(TEXT("MetalPaintRoughness"), bFriendly ? 0.6f : 0.22f);
			Mat->SetScalarParameterValue(TEXT("EmissivePower"), bFriendly ? 4.f : 40.f);
		}
	}
	if (!bFriendly && HasAuthority() && SoldierName.IsEmpty())
	{
		SoldierName = bClockwork ? FString::Printf(TEXT("Constable No. %d"), FMath::RandRange(100, 999))
			: FString::Printf(TEXT("Warden-%03d"), FMath::RandRange(100, 999));
	}
}

void ADVSoldierCharacter::UpdateFirstPersonBody()
{
	const bool bFirstPerson = IsLocallyControlled() && IsPlayerControlled() && !bIsDead;
	if (!GetMesh()->GetSkeletalMeshAsset())
	{
		return;
	}
	GetMesh()->SetOwnerNoSee(false);
	FirstPersonMesh->SetVisibility(false);
	if (bFirstPerson && !bShowFullBody)
	{
		GetMesh()->HideBoneByName(TEXT("neck_01"), EPhysBodyOp::PBO_None);
	}
	else
	{
		GetMesh()->UnHideBoneByName(TEXT("neck_01"));
	}
}

void ADVSoldierCharacter::SetupPlaceholderBody()
{
	if (GetMesh()->GetSkeletalMeshAsset())
	{
		ApplyTeamAppearance();
		UpdateFirstPersonBody();
		PlaceholderBody->DestroyComponent();
		PlaceholderVisor->DestroyComponent();
		PlaceholderBody = nullptr;
		PlaceholderVisor = nullptr;
		return;
	}

	const FLinearColor TeamColor = TeamId == 1 ? FLinearColor(0.15f, 0.35f, 0.9f) : FLinearColor(0.85f, 0.12f, 0.08f);
	if (UMaterialInstanceDynamic* BodyMat = PlaceholderBody->CreateAndSetMaterialInstanceDynamic(0))
	{
		BodyMat->SetVectorParameterValue(TEXT("Color"), TeamColor);
	}
	if (UMaterialInstanceDynamic* VisorMat = PlaceholderVisor->CreateAndSetMaterialInstanceDynamic(0))
	{
		VisorMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.9f, 0.75f, 0.1f));
		VisorMat->SetVectorParameterValue(TEXT("Emissive"), TeamColor * 4.f);
	}
}

void ADVSoldierCharacter::Destroyed()
{
	if (HasAuthority() && CurrentWeapon)
	{
		CurrentWeapon->Destroy();
	}
	Super::Destroyed();
}

// ---------------------------------------------------------------------------
// Possession / GAS
// ---------------------------------------------------------------------------

void ADVSoldierCharacter::InitAbilitySystem()
{
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
}

void ADVSoldierCharacter::GrantDefaultAbilities()
{
	if (bAbilitiesGranted || !HasAuthority())
	{
		return;
	}
	for (const TSubclassOf<UGameplayAbility>& Ability : DefaultAbilities)
	{
		if (Ability)
		{
			AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(Ability, 1, INDEX_NONE, this));
		}
	}
	bAbilitiesGranted = true;
}

void ADVSoldierCharacter::SpawnDefaultWeapon()
{
	if (!WeaponClass || CurrentWeapon)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	CurrentWeapon = GetWorld()->SpawnActor<ADVWeaponBase>(WeaponClass, GetActorTransform(), Params);
	if (CurrentWeapon)
	{
		// With no asset assigned the weapon uses UDVWeaponData's defaults (an M9 Bastion profile).
		CurrentWeapon->InitializeWeapon(DefaultWeapon);
	}
}

void ADVSoldierCharacter::EquipWeaponData(UDVWeaponData* Data)
{
	if (!HasAuthority())
	{
		return;
	}
	if (!CurrentWeapon)
	{
		SpawnDefaultWeapon();
	}
	if (CurrentWeapon)
	{
		CurrentWeapon->InitializeWeapon(Data);
	}
}

void ADVSoldierCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	AbilitySystemComponent->RefreshAbilityActorInfo();
	GrantDefaultAbilities();

	if (ADVSquadAIController* AI = Cast<ADVSquadAIController>(NewController))
	{
		AI->SetGenericTeamId(FGenericTeamId(TeamId));
	}
	if (CurrentWeapon)
	{
		CurrentWeapon->AttachToSoldier(this);
	}
}

void ADVSoldierCharacter::UnPossessed()
{
	Super::UnPossessed();
	SetAiming(false);
	SetSprinting(false);
	bFireHeld = false;
	BurstShotsRemaining = 0;
}

void ADVSoldierCharacter::OnRep_Controller()
{
	Super::OnRep_Controller();
	AbilitySystemComponent->RefreshAbilityActorInfo();
	if (CurrentWeapon)
	{
		CurrentWeapon->AttachToSoldier(this);
	}
}

void ADVSoldierCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	AbilitySystemComponent->RefreshAbilityActorInfo();
}

void ADVSoldierCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();
	UpdateFirstPersonBody();

	if (const APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (!DefaultMappingContext)
			{
				CreateFallbackInput();
			}
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

UAbilitySystemComponent* ADVSoldierCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ADVSoldierCharacter::OnRep_CurrentWeapon()
{
	if (CurrentWeapon)
	{
		CurrentWeapon->AttachToSoldier(this);
	}
}

FVector ADVSoldierCharacter::GetPawnViewLocation() const
{
	return FirstPersonCamera ? FirstPersonCamera->GetComponentLocation() : Super::GetPawnViewLocation();
}

UDVCharacterMovementComponent* ADVSoldierCharacter::GetDVMovement() const
{
	return Cast<UDVCharacterMovementComponent>(GetCharacterMovement());
}

// ---------------------------------------------------------------------------
// Teams
// ---------------------------------------------------------------------------

void ADVSoldierCharacter::SetGenericTeamId(const FGenericTeamId& NewTeamId)
{
	TeamId = NewTeamId.GetId();
	if (ADVSquadAIController* AI = Cast<ADVSquadAIController>(Controller))
	{
		AI->SetGenericTeamId(NewTeamId);
	}
}

ETeamAttitude::Type ADVSoldierCharacter::GetTeamAttitudeTowards(const AActor& Other) const
{
	if (const ADVSoldierCharacter* OtherSoldier = Cast<ADVSoldierCharacter>(&Other))
	{
		if (OtherSoldier->IsDead())
		{
			return ETeamAttitude::Neutral;
		}
	}

	const IGenericTeamAgentInterface* OtherAgent = Cast<const IGenericTeamAgentInterface>(&Other);
	if (!OtherAgent)
	{
		return ETeamAttitude::Neutral;
	}
	const FGenericTeamId OtherTeam = OtherAgent->GetGenericTeamId();
	if (OtherTeam == FGenericTeamId::NoTeam || TeamId == FGenericTeamId::NoTeam.GetId())
	{
		return ETeamAttitude::Neutral;
	}
	return OtherTeam.GetId() == TeamId ? ETeamAttitude::Friendly : ETeamAttitude::Hostile;
}

// ---------------------------------------------------------------------------
// Roster
// ---------------------------------------------------------------------------

void ADVSoldierCharacter::ApplyRosterRecord(const FDVSoldierRecord& Record)
{
	check(HasAuthority());
	RosterId = Record.Id;
	SoldierName = FString::Printf(TEXT("%s %s"), *UDVRosterSubsystem::RankToString(Record.Rank), *Record.GetDisplayName());
	AbilitySystemComponent->SetNumericAttributeBase(UDVAttributeSet::GetAccuracyAttribute(), Record.Accuracy);
	AbilitySystemComponent->SetNumericAttributeBase(UDVAttributeSet::GetMoraleAttribute(), Record.Morale);
}

// ---------------------------------------------------------------------------
// Combat
// ---------------------------------------------------------------------------

float ADVSoldierCharacter::GetHealth() const
{
	return AttributeSet ? AttributeSet->GetHealth() : 0.f;
}

float ADVSoldierCharacter::GetSuppression() const
{
	return AttributeSet ? AttributeSet->GetSuppression() : 0.f;
}

bool ADVSoldierCharacter::TryFire()
{
	if (bIsDead || bDowned || !CurrentWeapon)
	{
		return false;
	}
	if (CurrentWeapon->NeedsReload())
	{
		TryReload();
		return false;
	}
	if (UDVCharacterMovementComponent* Move = GetDVMovement(); Move && Move->bWantsToSprint)
	{
		SetSprinting(false);
	}
	return AbilitySystemComponent->TryActivateAbilitiesByTag(FGameplayTagContainer(DVTags::Ability_Weapon_Fire));
}

bool ADVSoldierCharacter::TryReload()
{
	if (bIsDead || bDowned)
	{
		return false;
	}
	return AbilitySystemComponent->TryActivateAbilitiesByTag(FGameplayTagContainer(DVTags::Ability_Weapon_Reload));
}

void ADVSoldierCharacter::SetAiming(bool bAim)
{
	bIsAiming = bAim && !bIsDead;
	if (UDVCharacterMovementComponent* Move = GetDVMovement())
	{
		Move->bWantsToAim = bIsAiming;
		if (bIsAiming)
		{
			Move->bWantsToSprint = false;
		}
	}
}

void ADVSoldierCharacter::SetSprinting(bool bSprint)
{
	if (UDVCharacterMovementComponent* Move = GetDVMovement())
	{
		Move->bWantsToSprint = bSprint && !bIsAiming;
	}
}

void ADVSoldierCharacter::SetLean(float Amount)
{
	LeanAmount = FMath::Clamp(Amount, -1.f, 1.f);
	if (!HasAuthority())
	{
		ServerSetLean(LeanAmount);
	}
}

void ADVSoldierCharacter::ServerSetLean_Implementation(float Amount)
{
	if (!LeanRpcLimiter.Consume(GetWorld()->GetTimeSeconds()))
	{
		return;
	}
	LeanAmount = FMath::Clamp(Amount, -1.f, 1.f);
}

void ADVSoldierCharacter::ApplyRecoil(const FVector2D& Kick)
{
	if (Controller)
	{
		FRotator Rot = Controller->GetControlRotation();
		Rot.Pitch = FMath::Clamp(FRotator::NormalizeAxis(Rot.Pitch + Kick.X), -89.f, 89.f);
		Rot.Yaw += Kick.Y;
		Controller->SetControlRotation(Rot);
	}
}

void ADVSoldierCharacter::PlayWeaponMontage(UAnimMontage* Montage)
{
	if (!Montage)
	{
		return;
	}
	// The first-person view is the real body (see UpdateFirstPersonBody), so montages always play on it.
	USkeletalMeshComponent* TargetMesh = GetMesh()->GetSkeletalMeshAsset() ? GetMesh() : FirstPersonMesh.Get();
	if (UAnimInstance* Anim = TargetMesh ? TargetMesh->GetAnimInstance() : nullptr)
	{
		Anim->Montage_Play(Montage);
	}
}

void ADVSoldierCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority())
	{
		// Mirror predicted movement flags into replicated state for other clients' animation.
		if (const UDVCharacterMovementComponent* Move = GetDVMovement(); Move && !IsLocallyControlled())
		{
			bIsAiming = Move->bWantsToAim;
		}

		if (bDowned)
		{
			TickDowned(DeltaSeconds);
		}

		const float Suppression = AttributeSet->GetSuppression();
		if (Suppression > 0.f && !bIsDead)
		{
			AbilitySystemComponent->SetNumericAttributeBase(UDVAttributeSet::GetSuppressionAttribute(),
				FMath::Max(0.f, Suppression - SuppressionDecayPerSecond * DeltaSeconds));
		}
	}

	if (IsLocallyControlled() && IsPlayerControlled() && !bIsDead)
	{
		// Automatic and burst fire are driven from tick while the trigger is held.
		const UDVWeaponData* Data = CurrentWeapon ? CurrentWeapon->GetData() : nullptr;
		if (Data && ((bFireHeld && Data->FireMode == EDVFireMode::Auto) || BurstShotsRemaining > 0))
		{
			if (TryFire() && BurstShotsRemaining > 0)
			{
				--BurstShotsRemaining;
			}
		}

		// ADS field of view.
		const float TargetFOV = (bIsAiming && Data) ? Data->AdsFOV : DefaultFOV;
		FirstPersonCamera->SetFieldOfView(FMath::FInterpTo(FirstPersonCamera->FieldOfView, TargetFOV, DeltaSeconds, 14.f));
	}

	TickWater(DeltaSeconds);

	// Lean: side-step the camera (and arms) for peeking out of cover.
	CurrentLeanVisual = FMath::FInterpTo(CurrentLeanVisual, LeanAmount, DeltaSeconds, 10.f);
	FirstPersonCamera->SetRelativeLocation(CameraBaseOffset + FVector(0.f, CurrentLeanVisual * LeanOffset, -FMath::Abs(CurrentLeanVisual) * 4.f - (bDowned ? 45.f : 0.f)));
}

// ---------------------------------------------------------------------------
// Death
// ---------------------------------------------------------------------------

float ADVSoldierCharacter::GetBleedOutRemaining() const
{
	return bDowned ? FMath::Max(0.f, BleedOutEndTime - GetWorld()->GetTimeSeconds()) : 0.f;
}

void ADVSoldierCharacter::HandleOutOfHealth(AActor* DamageInstigator, AActor* DamageCauser)
{
	if (bIsDead || !HasAuthority())
	{
		return;
	}
	// Accord soldiers go down first; a downed soldier hit again (or a frame) dies.
	if (TeamId == 1 && !bDowned)
	{
		EnterDowned(DamageInstigator);
		return;
	}
	Die(DamageInstigator);
}

void ADVSoldierCharacter::EnterDowned(AActor* DamageInstigator)
{
	bDowned = true;
	DownedBy = DamageInstigator;
	BleedOutEndTime = GetWorld()->GetTimeSeconds() + BleedOutSeconds;
	ReviveProgress = 0.f;
	bReviveHeld = false;
	AbilitySystemComponent->CancelAllAbilities();
	AbilitySystemComponent->SetNumericAttributeBase(UDVAttributeSet::GetHealthAttribute(), DownedHealth);
	AttributeSet->ResetOutOfHealth();
	SetAiming(false);
	SetSprinting(false);
	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		AI->StopMovement();
	}
	if (ADVGameMode* GM = GetWorld()->GetAuthGameMode<ADVGameMode>())
	{
		GM->NotifySoldierDowned(this);
	}
	OnRep_Downed();
}

void ADVSoldierCharacter::TickDowned(float DeltaSeconds)
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now >= BleedOutEndTime)
	{
		Die(DownedBy.Get());
		return;
	}
	// Anyone on our side kneeling next to us patches us up: AI squadmates automatically, players by holding Z.
	bool bBeingRevived = false;
	float ReviveRate = 1.f;
	for (TActorIterator<ADVSoldierCharacter> It(GetWorld()); It; ++It)
	{
		const ADVSoldierCharacter* Medic = *It;
		if (Medic == this || Medic->IsOutOfAction() || Medic->TeamId != TeamId || Medic->IsHidden()
			|| FVector::Dist(Medic->GetActorLocation(), GetActorLocation()) > ReviveRange)
		{
			continue;
		}
		if (!Medic->IsPlayerControlled() || Medic->IsReviveHeld())
		{
			bBeingRevived = true;
			ReviveRate = FMath::Max(ReviveRate, Medic->GetCommanderLevel() >= DVPerks::FieldMedic ? 1.8f : 1.f);
		}
	}
	ReviveProgress = bBeingRevived ? ReviveProgress + ReviveRate * DeltaSeconds / ReviveSeconds : FMath::Max(0.f, ReviveProgress - DeltaSeconds / ReviveSeconds);
	if (ReviveProgress >= 1.f)
	{
		Revive();
	}
}

void ADVSoldierCharacter::Revive()
{
	bDowned = false;
	ReviveProgress = 0.f;
	AbilitySystemComponent->SetNumericAttributeBase(UDVAttributeSet::GetHealthAttribute(), 35.f);
	AttributeSet->ResetOutOfHealth();
	if (ADVGameMode* GM = GetWorld()->GetAuthGameMode<ADVGameMode>())
	{
		GM->NotifySoldierRevived(this);
	}
	OnRep_Downed();
}

void ADVSoldierCharacter::OnRep_Downed()
{
	if (bIsDead)
	{
		return;
	}
	// Grey-box downed pose: knelt low, can't move or shoot, camera near the ground.
	if (bDowned)
	{
		Crouch();
		GetCharacterMovement()->DisableMovement();
		FirstPersonCamera->SetRelativeLocation(CameraBaseOffset - FVector(0.f, 0.f, 45.f));
		GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, -55.f));
	}
	else
	{
		UnCrouch();
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		FirstPersonCamera->SetRelativeLocation(CameraBaseOffset);
		GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	}
}

float ADVSoldierCharacter::GetMaxHealthValue() const
{
	return AttributeSet ? AttributeSet->GetMaxHealth() : 100.f;
}

void ADVSoldierCharacter::TickWater(float DeltaSeconds)
{
	if (!bWaterChecked)
	{
		bWaterChecked = true;
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			if (It->ActorHasTag(TEXT("DV_Water")))
			{
				WaterZ = It->GetActorLocation().Z;
				// The surface is a sight-blocker only (hoverbikes skim it); bodies and vehicles pass through.
				TArray<UPrimitiveComponent*> Prims;
				It->GetComponents(Prims);
				for (UPrimitiveComponent* Prim : Prims)
				{
					Prim->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
					Prim->SetCollisionResponseToAllChannels(ECR_Ignore);
					Prim->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
				}
				break;
			}
		}

	}
	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (WaterZ < -1.e8f || bIsDead || !Move || IsHidden())
	{
		return;
	}
	// Chest height under the surface means you are in the water: float up to it and swim slowly.
	const float Chest = GetActorLocation().Z + 20.f;
	// Hysteresis: you are in once your chest goes under, and out only once you are well clear of the surface.
	const bool bNowInWater = bInWater ? Chest < WaterZ + 70.f : Chest < WaterZ;
	if (bNowInWater != bInWater)
	{
		bInWater = bNowInWater;
		Move->GravityScale = bInWater ? 0.f : 1.f;
		Move->AirControl = bInWater ? 1.f : 0.25f;
		Move->MaxWalkSpeed = bInWater ? 260.f : 420.f;
		if (bInWater)
		{
			Move->SetMovementMode(MOVE_Falling);
			DVSound::At(GetWorld(), TEXT("SW_Slide"), GetActorLocation(), 0.8f, 0, 0.3f);	// the splash
		}
	}
	if (bInWater)
	{
		// Buoyancy toward floating with the head out, heavy drag, and swim speed capped.
		const float Depth = WaterZ - Chest;
		Move->Velocity.Z = FMath::FInterpTo(Move->Velocity.Z, FMath::Clamp(Depth * 3.f, -150.f, 220.f), DeltaSeconds, 4.f);
		const FVector Planar(Move->Velocity.X, Move->Velocity.Y, 0.f);
		if (Planar.Size() > 280.f)
		{
			Move->Velocity = FVector(Planar.GetSafeNormal().X * 280.f, Planar.GetSafeNormal().Y * 280.f, Move->Velocity.Z);
		}
		Move->AddInputVector(FVector::ZeroVector);
	}
}

ADVSoldierCharacter* ADVSoldierCharacter::FindTakedownTarget() const
{
	if (bIsDead || bDowned || bInWater)
	{
		return nullptr;
	}
	for (TActorIterator<ADVSoldierCharacter> It(GetWorld()); It; ++It)
	{
		ADVSoldierCharacter* Other = *It;
		if (Other == this || Other->TeamId == TeamId || Other->IsOutOfAction() || Other->IsAlerted())
		{
			continue;
		}
		const FVector To = Other->GetActorLocation() - GetActorLocation();
		// Close, and behind them: they are facing away from you.
		if (To.Size2D() < 190.f && FVector::DotProduct(Other->GetActorForwardVector(), To.GetSafeNormal2D()) > 0.3f)
		{
			return Other;
		}
	}
	return nullptr;
}

void ADVSoldierCharacter::ServerTakedown_Implementation(ADVSoldierCharacter* Victim)
{
	if (!Victim || Victim->IsOutOfAction() || Victim->IsAlerted() || FVector::Dist2D(Victim->GetActorLocation(), GetActorLocation()) > 260.f)
	{
		return;	// server re-checks reach and awareness
	}
	DVSound::At(GetWorld(), TEXT("SW_Impact"), Victim->GetActorLocation(), 1.f, 0, 0.1f);
	DVFx::Flash(GetWorld(), Victim->GetActorLocation() + FVector(0.f, 0.f, 60.f), FLinearColor(0.3f, 0.6f, 1.f), 1200.f, 300.f, 0.08f);
	// A shock-blade to the reactor: the frame drops without a sound anyone else hears.
	DVCombat::ApplyDamage(this, Victim, 99999.f, false);
}

void ADVSoldierCharacter::OnThrusterBoost()
{
	const FVector Feet = GetActorLocation() - FVector(0.f, 0.f, 80.f);
	DVSound::At(GetWorld(), TEXT("SW_Thruster"), Feet, 0.8f, 0);
	DVFx::Flash(GetWorld(), Feet, FLinearColor(0.35f, 0.7f, 1.f), 4000.f, 600.f, 0.15f);
	if (IsLocallyControlled())
	{
		FirstPersonCamera->SetFieldOfView(FirstPersonCamera->FieldOfView + 8.f);	// eases back in Tick
	}
}

void ADVSoldierCharacter::OnSlideStarted()
{
	DVSound::At(GetWorld(), TEXT("SW_Slide"), GetActorLocation() - FVector(0.f, 0.f, 80.f), 0.7f, 0);
	if (IsLocallyControlled())
	{
		FirstPersonCamera->SetFieldOfView(FirstPersonCamera->FieldOfView + 5.f);
	}
}

float ADVSoldierCharacter::GetBoostCharges() const
{
	const UDVCharacterMovementComponent* Move = GetDVMovement();
	return Move ? Move->GetBoostCharges() : 0.f;
}

float ADVSoldierCharacter::GetMaxBoostCharges() const
{
	const UDVCharacterMovementComponent* Move = GetDVMovement();
	return Move ? Move->MaxBoostCharges : 0.f;
}

void ADVSoldierCharacter::ServerSetLook_Implementation(const FDVLook& NewLook)
{
	Look = NewLook;
	Look.bSet = true;
	SoldierName = FString::Printf(TEXT("Cdr. \"%s\""), *UDVProfileSubsystem::CallsignName(Look.Callsign));
	OnRep_Look();
}

void ADVSoldierCharacter::ServerSetCommanderLevel_Implementation(int32 Level)
{
	CommanderLevel = FMath::Clamp(Level, 1, DVPerks::MaxLevel);
	if (CommanderLevel >= DVPerks::Hardened)
	{
		AbilitySystemComponent->SetNumericAttributeBase(UDVAttributeSet::GetMaxHealthAttribute(), 125.f);
		AbilitySystemComponent->SetNumericAttributeBase(UDVAttributeSet::GetHealthAttribute(), 125.f);
	}
	if (CommanderLevel >= DVPerks::PlateCarrier)
	{
		AbilitySystemComponent->SetNumericAttributeBase(UDVAttributeSet::GetArmorAttribute(), 40.f);
	}
	OnRep_CommanderLevel();
}

void ADVSoldierCharacter::OnRep_CommanderLevel()
{
	if (UDVCharacterMovementComponent* Move = GetDVMovement())
	{
		Move->MaxBoostCharges = CommanderLevel >= DVPerks::ExtraThruster ? 3.f : 2.f;
	}
}

void ADVSoldierCharacter::OnRep_Look()
{
	ApplyTeamAppearance();
	UpdateFirstPersonBody();
}

void ADVSoldierCharacter::SetShowFullBody(bool bShow)
{
	bShowFullBody = bShow;
	UpdateFirstPersonBody();
}

void ADVSoldierCharacter::Die(AActor* DamageInstigator)
{
	if (bIsDead)
	{
		return;
	}
	bIsDead = true;
	bDowned = false;
	AbilitySystemComponent->CancelAllAbilities();
	AbilitySystemComponent->AddLooseGameplayTag(DVTags::State_Dead);
	SetAiming(false);
	SetSprinting(false);

	OnDied.Broadcast(this);

	AController* KillerController = nullptr;
	if (const APawn* KillerPawn = Cast<APawn>(DamageInstigator))
	{
		KillerController = KillerPawn->GetController();
	}
	else
	{
		KillerController = Cast<AController>(DamageInstigator);
	}

	if (ADVGameMode* GM = GetWorld()->GetAuthGameMode<ADVGameMode>())
	{
		GM->NotifySoldierKilled(this, Cast<ADVSoldierCharacter>(DamageInstigator), KillerController);
	}
	GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));

	// AI stops thinking; players keep their controller until the game mode respawns them.
	if (AController* MyController = GetController(); MyController && !MyController->IsPlayerController())
	{
		MyController->UnPossess();
		MyController->Destroy();
	}

	OnRep_IsDead();
	if (!bClockwork)
	{
		SetLifeSpan(90.f);	// wound-down constables stay where they stopped, for the Guild to collect
	}
}

void ADVSoldierCharacter::OnRep_IsDead()
{
	if (!bIsDead)
	{
		return;
	}
	if (!HasAuthority())
	{
		AbilitySystemComponent->AddLooseGameplayTag(DVTags::State_Dead);
	}
	if (bClockwork)
	{
		WindDown();
		return;
	}
	EnterRagdoll();
}

void ADVSoldierCharacter::WindDown()
{
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	FirstPersonMesh->SetVisibility(false, true);
	USkeletalMeshComponent* Body = GetMesh();
	Body->SetOwnerNoSee(false);
	Body->UnHideBoneByName(TEXT("neck_01"));
	Body->bPauseAnims = true;
	// Slump forward onto its knees, more or less: sink and pitch over, then the optics go dark.
	SetActorLocationAndRotation(GetActorLocation() - FVector(0.f, 0.f, 45.f), FRotator(-24.f, GetActorRotation().Yaw, 0.f));
	ApplyTeamAppearance();
	DVFx::Flash(GetWorld(), GetActorLocation() + FVector(0.f, 0.f, 60.f), FLinearColor(1.f, 0.85f, 0.6f), 900.f, 500.f, 0.4f);
	DVSound::At(GetWorld(), TEXT("SW_Thruster"), GetActorLocation(), 0.45f, 0, 0.2f);
	if (PlaceholderBody)
	{
		PlaceholderBody->SetOwnerNoSee(false);
		PlaceholderBody->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -40.f), FRotator(-30.f, 0.f, 0.f));
		PlaceholderVisor->SetVisibility(false);
	}
}

void ADVSoldierCharacter::EnterRagdoll()
{
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();

	USkeletalMeshComponent* Body = GetMesh();
	Body->SetOwnerNoSee(false);
	Body->UnHideBoneByName(TEXT("neck_01"));
	Body->SetCollisionProfileName(TEXT("Ragdoll"));
	Body->SetAllBodiesSimulatePhysics(true);
	Body->WakeAllRigidBodies();
	FirstPersonMesh->SetVisibility(false, true);

	// No skeleton to ragdoll: topple the grey-box body instead.
	if (PlaceholderBody)
	{
		PlaceholderBody->SetOwnerNoSee(false);
		PlaceholderBody->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -60.f), FRotator(90.f, 0.f, 0.f));
		PlaceholderVisor->SetVisibility(false);
	}
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

void ADVSoldierCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (!DefaultMappingContext)
	{
		CreateFallbackInput();
	}

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input)
	{
		UE_LOG(LogDV, Error, TEXT("Enhanced Input component missing; check DefaultInput.ini"));
		return;
	}

	Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ADVSoldierCharacter::Input_Move);
	Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &ADVSoldierCharacter::Input_Look);
	Input->BindAction(JumpAction, ETriggerEvent::Started, this, &ADVSoldierCharacter::Input_Jump);
	Input->BindAction(DashAction, ETriggerEvent::Started, this, &ADVSoldierCharacter::Input_Dash);
	Input->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	Input->BindAction(FireAction, ETriggerEvent::Started, this, &ADVSoldierCharacter::Input_FireStarted);
	Input->BindAction(FireAction, ETriggerEvent::Completed, this, &ADVSoldierCharacter::Input_FireCompleted);
	Input->BindAction(AimAction, ETriggerEvent::Started, this, &ADVSoldierCharacter::Input_AimStarted);
	Input->BindAction(AimAction, ETriggerEvent::Completed, this, &ADVSoldierCharacter::Input_AimCompleted);
	Input->BindAction(ReloadAction, ETriggerEvent::Started, this, &ADVSoldierCharacter::Input_Reload);
	Input->BindAction(SprintAction, ETriggerEvent::Started, this, &ADVSoldierCharacter::Input_SprintStarted);
	Input->BindAction(SprintAction, ETriggerEvent::Completed, this, &ADVSoldierCharacter::Input_SprintCompleted);
	Input->BindAction(CrouchAction, ETriggerEvent::Started, this, &ADVSoldierCharacter::Input_Crouch);
	Input->BindAction(LeanLeftAction, ETriggerEvent::Started, this, &ADVSoldierCharacter::Input_LeanLeft);
	Input->BindAction(LeanRightAction, ETriggerEvent::Started, this, &ADVSoldierCharacter::Input_LeanRight);
	Input->BindAction(OrderMoveAction, ETriggerEvent::Started, this, &ADVSoldierCharacter::Input_OrderMove);
	Input->BindAction(OrderHoldAction, ETriggerEvent::Started, this, &ADVSoldierCharacter::Input_OrderHold);
	Input->BindAction(OrderRegroupAction, ETriggerEvent::Started, this, &ADVSoldierCharacter::Input_OrderRegroup);
	Input->BindAction(OrderSuppressAction, ETriggerEvent::Started, this, &ADVSoldierCharacter::Input_OrderSuppress);
	Input->BindAction(OrderFlankAction, ETriggerEvent::Started, this, &ADVSoldierCharacter::Input_OrderFlank);
}

void ADVSoldierCharacter::Input_Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddMovementInput(GetActorForwardVector(), Axis.Y);
	AddMovementInput(GetActorRightVector(), Axis.X);
}

void ADVSoldierCharacter::Input_Look(const FInputActionValue& Value)
{
	if (!Controller)
	{
		return;
	}
	const FVector2D Axis = Value.Get<FVector2D>() * LookSensitivity * (bIsAiming ? 0.6f : 1.f);
	FRotator Rot = Controller->GetControlRotation();
	Rot.Yaw += Axis.X;
	Rot.Pitch = FMath::Clamp(FRotator::NormalizeAxis(Rot.Pitch + Axis.Y), -89.f, 89.f);
	Controller->SetControlRotation(Rot);
}

void ADVSoldierCharacter::Input_FireStarted()
{
	bFireHeld = true;
	const UDVWeaponData* Data = CurrentWeapon ? CurrentWeapon->GetData() : nullptr;
	if (!Data)
	{
		return;
	}
	switch (Data->FireMode)
	{
	case EDVFireMode::Semi:
		TryFire();
		break;
	case EDVFireMode::Burst:
		if (BurstShotsRemaining == 0)
		{
			BurstShotsRemaining = Data->BurstCount;
		}
		break;
	case EDVFireMode::Auto:
		TryFire();	// first round immediately; Tick continues while held
		break;
	}
}

void ADVSoldierCharacter::Input_FireCompleted()
{
	bFireHeld = false;
}

void ADVSoldierCharacter::Input_Jump()
{
	if (bDowned)
	{
		return;
	}
	UDVCharacterMovementComponent* Move = GetDVMovement();
	if (bInWater && Move)
	{
		// Kick up and out: enough, with a thruster burst, to get back onto a quay.
		LaunchCharacter(FVector(0.f, 0.f, 650.f), false, true);
		return;
	}
	if (Move && Move->IsFalling())
	{
		Move->bWantsToBoost = true;	// thruster boost jump
		return;
	}
	Jump();
}

void ADVSoldierCharacter::Input_Dash()
{
	if (UDVCharacterMovementComponent* Move = GetDVMovement(); Move && !bDowned)
	{
		Move->bWantsToBoost = true;	// on the ground this is a thruster dash
	}
}

void ADVSoldierCharacter::Input_Crouch()
{
	if (bDowned)
	{
		return;
	}
	if (UDVCharacterMovementComponent* Move = GetDVMovement(); Move && Move->IsSprinting() && Move->Velocity.Size2D() >= Move->SlideMinSpeed)
	{
		Move->bWantsToSlide = true;
		return;
	}
	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		SetSprinting(false);
		Crouch();
	}
}

void ADVSoldierCharacter::Input_OrderMove()
{
	// Context-sensitive: an unaware frame right in front of you gets a silent takedown instead of an order.
	if (ADVSoldierCharacter* Victim = FindTakedownTarget())
	{
		if (ADVPlayerController* PC = Cast<ADVPlayerController>(GetController()))
		{
			PC->LastTakedownRequest = GetWorld()->GetTimeSeconds();
		}
		ServerTakedown(Victim);
		return;
	}
	CommandComponent->IssueOrderFromView(EDVSquadOrder::MoveTo);
}
void ADVSoldierCharacter::Input_OrderHold() { CommandComponent->IssueOrderFromView(EDVSquadOrder::Hold); }
void ADVSoldierCharacter::Input_OrderRegroup() { CommandComponent->IssueOrderFromView(EDVSquadOrder::Regroup); }
void ADVSoldierCharacter::Input_OrderSuppress() { CommandComponent->IssueOrderFromView(EDVSquadOrder::Suppress); }
void ADVSoldierCharacter::Input_OrderFlank() { CommandComponent->IssueOrderFromView(EDVSquadOrder::Flank); }

void ADVSoldierCharacter::CreateFallbackInput()
{
	// Lets the project run before input assets exist. Replace with IMC/IA assets in a Blueprint child.
	auto MakeAction = [this](const TCHAR* Name, EInputActionValueType Type) -> UInputAction*
	{
		UInputAction* Action = NewObject<UInputAction>(this, Name);
		Action->ValueType = Type;
		return Action;
	};

	UInputMappingContext* Ctx = NewObject<UInputMappingContext>(this, TEXT("IMC_DVFallback"));

	MoveAction = MakeAction(TEXT("IA_Move"), EInputActionValueType::Axis2D);
	LookAction = MakeAction(TEXT("IA_Look"), EInputActionValueType::Axis2D);
	JumpAction = MakeAction(TEXT("IA_Jump"), EInputActionValueType::Boolean);
	FireAction = MakeAction(TEXT("IA_Fire"), EInputActionValueType::Boolean);
	AimAction = MakeAction(TEXT("IA_Aim"), EInputActionValueType::Boolean);
	ReloadAction = MakeAction(TEXT("IA_Reload"), EInputActionValueType::Boolean);
	SprintAction = MakeAction(TEXT("IA_Sprint"), EInputActionValueType::Boolean);
	CrouchAction = MakeAction(TEXT("IA_Crouch"), EInputActionValueType::Boolean);
	DashAction = MakeAction(TEXT("IA_Dash"), EInputActionValueType::Boolean);
	LeanLeftAction = MakeAction(TEXT("IA_LeanLeft"), EInputActionValueType::Boolean);
	LeanRightAction = MakeAction(TEXT("IA_LeanRight"), EInputActionValueType::Boolean);
	OrderMoveAction = MakeAction(TEXT("IA_OrderMove"), EInputActionValueType::Boolean);
	OrderHoldAction = MakeAction(TEXT("IA_OrderHold"), EInputActionValueType::Boolean);
	OrderRegroupAction = MakeAction(TEXT("IA_OrderRegroup"), EInputActionValueType::Boolean);
	OrderSuppressAction = MakeAction(TEXT("IA_OrderSuppress"), EInputActionValueType::Boolean);
	OrderFlankAction = MakeAction(TEXT("IA_OrderFlank"), EInputActionValueType::Boolean);

	// WASD -> Axis2D (X = right, Y = forward)
	{
		FEnhancedActionKeyMapping& W = Ctx->MapKey(MoveAction, EKeys::W);
		W.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(Ctx));
		FEnhancedActionKeyMapping& S = Ctx->MapKey(MoveAction, EKeys::S);
		S.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(Ctx));
		S.Modifiers.Add(NewObject<UInputModifierNegate>(Ctx));
		FEnhancedActionKeyMapping& A = Ctx->MapKey(MoveAction, EKeys::A);
		A.Modifiers.Add(NewObject<UInputModifierNegate>(Ctx));
		Ctx->MapKey(MoveAction, EKeys::D);
	}
	Ctx->MapKey(LookAction, EKeys::Mouse2D);
	Ctx->MapKey(JumpAction, EKeys::SpaceBar);
	Ctx->MapKey(FireAction, EKeys::LeftMouseButton);
	Ctx->MapKey(AimAction, EKeys::RightMouseButton);
	Ctx->MapKey(ReloadAction, EKeys::R);
	Ctx->MapKey(SprintAction, EKeys::LeftShift);
	Ctx->MapKey(CrouchAction, EKeys::C);
	Ctx->MapKey(CrouchAction, EKeys::LeftControl);
	Ctx->MapKey(DashAction, EKeys::LeftAlt);
	Ctx->MapKey(LeanLeftAction, EKeys::Q);
	Ctx->MapKey(LeanRightAction, EKeys::E);
	Ctx->MapKey(OrderMoveAction, EKeys::F);
	Ctx->MapKey(OrderHoldAction, EKeys::G);
	Ctx->MapKey(OrderRegroupAction, EKeys::V);
	Ctx->MapKey(OrderSuppressAction, EKeys::T);
	Ctx->MapKey(OrderFlankAction, EKeys::X);

	DefaultMappingContext = Ctx;
}
