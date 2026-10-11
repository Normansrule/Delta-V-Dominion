#include "Characters/DVSoldierCharacter.h"
#include "Characters/DVCharacterMovementComponent.h"
#include "Characters/DVCommandComponent.h"
#include "Characters/DVSquadAIController.h"
#include "Combat/DVAttributeSet.h"
#include "Combat/DVWeaponBase.h"
#include "Combat/DVWeaponData.h"
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
	FirstPersonCamera->SetRelativeLocation(FVector(-5.f, 0.f, 64.f));
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
	DOREPLIFETIME(ADVSoldierCharacter, bIsDead);
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

void ADVSoldierCharacter::SetupPlaceholderBody()
{
	if (GetMesh()->GetSkeletalMeshAsset())
	{
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
	if (bIsDead || !CurrentWeapon)
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
	if (bIsDead)
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
	USkeletalMeshComponent* TargetMesh = (IsLocallyControlled() && IsPlayerControlled()) ? FirstPersonMesh.Get() : GetMesh();
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

	// Lean: side-step the camera (and arms) for peeking out of cover.
	CurrentLeanVisual = FMath::FInterpTo(CurrentLeanVisual, LeanAmount, DeltaSeconds, 10.f);
	FirstPersonCamera->SetRelativeLocation(CameraBaseOffset + FVector(0.f, CurrentLeanVisual * LeanOffset, -FMath::Abs(CurrentLeanVisual) * 4.f));
}

// ---------------------------------------------------------------------------
// Death
// ---------------------------------------------------------------------------

void ADVSoldierCharacter::HandleOutOfHealth(AActor* DamageInstigator, AActor* DamageCauser)
{
	if (bIsDead || !HasAuthority())
	{
		return;
	}

	bIsDead = true;
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

	// AI stops thinking; players keep their controller until the game mode respawns them.
	if (AController* MyController = GetController(); MyController && !MyController->IsPlayerController())
	{
		MyController->UnPossess();
		MyController->Destroy();
	}

	OnRep_IsDead();
	SetLifeSpan(90.f);
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
	EnterRagdoll();
}

void ADVSoldierCharacter::EnterRagdoll()
{
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();

	USkeletalMeshComponent* Body = GetMesh();
	Body->SetOwnerNoSee(false);
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
	Input->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
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

void ADVSoldierCharacter::Input_Crouch()
{
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

void ADVSoldierCharacter::Input_OrderMove() { CommandComponent->IssueOrderFromView(EDVSquadOrder::MoveTo); }
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
	Ctx->MapKey(LeanLeftAction, EKeys::Q);
	Ctx->MapKey(LeanRightAction, EKeys::E);
	Ctx->MapKey(OrderMoveAction, EKeys::F);
	Ctx->MapKey(OrderHoldAction, EKeys::G);
	Ctx->MapKey(OrderRegroupAction, EKeys::V);
	Ctx->MapKey(OrderSuppressAction, EKeys::T);
	Ctx->MapKey(OrderFlankAction, EKeys::X);

	DefaultMappingContext = Ctx;
}
