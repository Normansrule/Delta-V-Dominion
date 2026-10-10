#include "Combat/DVGA_FireWeapon.h"
#include "Combat/DVWeaponBase.h"
#include "Combat/DVWeaponData.h"
#include "Combat/DVCombatEffects.h"
#include "Combat/DVAttributeSet.h"
#include "Characters/DVSoldierCharacter.h"
#include "Characters/DVSquadAIController.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "GameFramework/PlayerState.h"
#include "GenericTeamAgentInterface.h"
#include "DeltaVDominion.h"
#include "DVGameplayTags.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "TimerManager.h"

UDVGA_FireWeapon::UDVGA_FireWeapon()
{
	SetAssetTags(FGameplayTagContainer(DVTags::Ability_Weapon_Fire));
	ActivationOwnedTags.AddTag(DVTags::State_Firing);
	ActivationBlockedTags.AddTag(DVTags::State_Reloading);
}

bool UDVGA_FireWeapon::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	const ADVWeaponBase* Weapon = GetWeaponFromActorInfo(ActorInfo);
	const UWorld* World = Weapon ? Weapon->GetWorld() : nullptr;
	// The server copy of a remote shooter is lenient on timing; ProcessShotOnServer enforces the sustained rate.
	const bool bLenient = ActorInfo->IsNetAuthority() && !ActorInfo->IsLocallyControlled();
	return World && Weapon->CanFire(World->GetTimeSeconds(), bLenient);
}

void UDVGA_FireWeapon::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	check(ASC);

	if (ActorInfo->IsNetAuthority() && !ActorInfo->IsLocallyControlled())
	{
		// Server copy of a remote client's shot: wait for the client's hits.
		TargetDataDelegateHandle = ASC->AbilityTargetDataSetDelegate(Handle, ActivationInfo.GetActivationPredictionKey())
			.AddUObject(this, &UDVGA_FireWeapon::OnTargetDataReady);
		ASC->CallReplicatedTargetDataDelegatesIfSet(Handle, ActivationInfo.GetActivationPredictionKey());

		if (IsActive())
		{
			GetWorld()->GetTimerManager().SetTimer(ServerWaitTimer, this, &UDVGA_FireWeapon::OnServerWaitTimedOut, 1.0f, false);
		}
		return;
	}

	TraceAndFireLocally();
}

void UDVGA_FireWeapon::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ServerWaitTimer);
	}
	if (TargetDataDelegateHandle.IsValid() && ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
	{
		ActorInfo->AbilitySystemComponent->AbilityTargetDataSetDelegate(Handle, ActivationInfo.GetActivationPredictionKey())
			.Remove(TargetDataDelegateHandle);
		TargetDataDelegateHandle.Reset();
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UDVGA_FireWeapon::TraceAndFireLocally()
{
	ADVSoldierCharacter* Soldier = GetSoldier();
	ADVWeaponBase* Weapon = GetWeapon();
	const UDVWeaponData* Data = Weapon ? Weapon->GetData() : nullptr;
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!Soldier || !Data || !ASC)
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
		return;
	}

	const double Now = GetWorldTime();
	FScopedPredictionWindow ScopedPrediction(ASC, GetCurrentActivationInfo().GetActivationPredictionKey());

	// Aim origin and direction.
	FVector Start;
	FVector AimDir;
	if (const ADVSquadAIController* AI = Cast<ADVSquadAIController>(Soldier->GetController()))
	{
		Start = Soldier->GetPawnViewLocation();
		AimDir = (AI->GetAimPoint() - Start).GetSafeNormal();
	}
	else
	{
		FRotator ViewRot;
		Soldier->GetActorEyesViewPoint(Start, ViewRot);
		if (const AController* PC = Soldier->GetController())
		{
			PC->GetPlayerViewPoint(Start, ViewRot);
		}
		AimDir = ViewRot.Vector();
	}

	const float Suppression01 = ASC->GetNumericAttribute(UDVAttributeSet::GetSuppressionAttribute()) / 100.f;
	const float SpreadRad = FMath::DegreesToRadians(Weapon->GetCurrentSpread(Soldier->IsAiming(), Suppression01));

	FCollisionQueryParams Params(SCENE_QUERY_STAT(DVWeaponTrace), /*bTraceComplex*/ true, Soldier);
	Params.AddIgnoredActor(Weapon);
	Params.bReturnPhysicalMaterial = true;

	FGameplayAbilityTargetDataHandle TargetData;
	TArray<FVector> Impacts;
	for (int32 Pellet = 0; Pellet < Data->PelletsPerShot; ++Pellet)
	{
		const FVector Dir = FMath::VRandCone(AimDir, SpreadRad);
		const FVector End = Start + Dir * Data->MaxRange;

		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, DV_TRACE_WEAPON, Params))
		{
			Hit = FHitResult(Start, End);
			Hit.Location = End;
			Hit.ImpactPoint = End;
		}
		Impacts.Add(Hit.ImpactPoint);
		TargetData.Add(new FGameplayAbilityTargetData_SingleTargetHit(Hit));
	}

	// Recoil reads the pattern timing before ConsumeShot updates it.
	if (Soldier->IsPlayerControlled())
	{
		Soldier->ApplyRecoil(Weapon->ConsumeRecoil(Now, Soldier->IsAiming()));
	}
	Weapon->ConsumeShot(Now);
	Weapon->PlayLocalFireEffects(Impacts);
	Soldier->PlayWeaponMontage(Data->FireMontage);

	if (GetCurrentActorInfo()->IsNetAuthority())
	{
		ProcessShotOnServer(TargetData, /*bTraceWasServerSide*/ true);
	}
	else
	{
		ASC->CallServerSetReplicatedTargetData(GetCurrentAbilitySpecHandle(), GetCurrentActivationInfo().GetActivationPredictionKey(),
			TargetData, FGameplayTag(), ASC->ScopedPredictionKey);
	}

	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UDVGA_FireWeapon::OnTargetDataReady(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ApplicationTag)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		return;
	}

	FScopedPredictionWindow ScopedPrediction(ASC);
	ASC->ConsumeClientReplicatedTargetData(GetCurrentAbilitySpecHandle(), GetCurrentActivationInfo().GetActivationPredictionKey());

	ProcessShotOnServer(Data, /*bTraceWasServerSide*/ false);
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UDVGA_FireWeapon::OnServerWaitTimedOut()
{
	UE_LOG(LogDVNet, Verbose, TEXT("Fire ability timed out waiting for client target data"));
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
}

void UDVGA_FireWeapon::ProcessShotOnServer(const FGameplayAbilityTargetDataHandle& Data, bool bTraceWasServerSide)
{
	ADVSoldierCharacter* Soldier = GetSoldier();
	ADVWeaponBase* Weapon = GetWeapon();
	const UDVWeaponData* WeaponData = Weapon ? Weapon->GetData() : nullptr;
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (!Soldier || !WeaponData || !SourceASC || !Soldier->HasAuthority())
	{
		return;
	}

	const double Now = GetWorldTime();
	float AllowedSpread = 0.f;

	if (!bTraceWasServerSide)
	{
		// --- Shot-level checks for client-reported fire ---
		if (!Weapon->ServerRpcLimiter.Consume(Now))
		{
			UE_LOG(LogDVNet, Warning, TEXT("%s: fire RPC rate limit exceeded"), *GetNameSafe(Soldier));
			return;
		}
		const bool bHasAmmo = WeaponData->bUsesHeat ? !Weapon->IsOverheated() : Weapon->GetAmmoInMag() > 0;
		if (!bHasAmmo || !Weapon->ServerValidateShotTiming(Now))
		{
			return;
		}
		if (Data.Num() == 0 || Data.Num() > WeaponData->PelletsPerShot)
		{
			UE_LOG(LogDVNet, Warning, TEXT("%s: invalid pellet count %d"), *GetNameSafe(Soldier), Data.Num());
			return;
		}
		Weapon->ConsumeShot(Now);

		const float Suppression01 = SourceASC->GetNumericAttribute(UDVAttributeSet::GetSuppressionAttribute()) / 100.f;
		AllowedSpread = Weapon->GetCurrentSpread(false, Suppression01);
	}

	TArray<FVector_NetQuantize> Impacts;
	for (int32 i = 0; i < Data.Num(); ++i)
	{
		const FGameplayAbilityTargetData* Entry = Data.Get(i);
		const FHitResult* Hit = Entry ? Entry->GetHitResult() : nullptr;
		if (!Hit)
		{
			continue;
		}
		if (!bTraceWasServerSide && !ValidateHit(*Hit, AllowedSpread))
		{
			continue;
		}

		Impacts.Add(Hit->ImpactPoint);
		AActor* HitActor = Hit->GetActor();

		// Damage hostile targets only (no friendly fire in the campaign).
		if (HitActor && FGenericTeamId::GetAttitude(Soldier, HitActor) == ETeamAttitude::Hostile)
		{
			if (UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor))
			{
				FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(UDVDamageEffect::StaticClass(), GetAbilityLevel());
				if (Spec.IsValid())
				{
					Spec.Data->SetSetByCallerMagnitude(DVTags::SetByCaller_Damage, WeaponData->Damage);
					FGameplayEffectContextHandle Context = Spec.Data->GetContext();
					Context.AddHitResult(*Hit, /*bReset*/ true);
					if (Hit->BoneName.ToString().Contains(TEXT("head")))
					{
						Spec.Data->AddDynamicAssetTag(DVTags::Damage_Headshot);
					}
					SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
				}
			}
		}

		ApplySuppressionAlongShot(Hit->TraceStart, Hit->ImpactPoint, HitActor);
	}

	Weapon->MulticastFireEffects(Impacts);
}

bool UDVGA_FireWeapon::ValidateHit(const FHitResult& Hit, float AllowedSpreadDeg) const
{
	const ADVSoldierCharacter* Soldier = GetSoldier();
	const ADVWeaponBase* Weapon = GetWeapon();
	const UDVWeaponData* Data = Weapon ? Weapon->GetData() : nullptr;
	if (!Soldier || !Data)
	{
		return false;
	}

	// 1. Origin: the shot must start at (roughly) the shooter's eyes on the server.
	const FVector ServerEyes = Soldier->GetPawnViewLocation();
	if (FVector::Dist(Hit.TraceStart, ServerEyes) > MaxOriginError)
	{
		UE_LOG(LogDVNet, Warning, TEXT("%s: shot origin off by %.0fcm"), *GetNameSafe(Soldier), FVector::Dist(Hit.TraceStart, ServerEyes));
		return false;
	}

	// 2. Range.
	if (FVector::Dist(Hit.TraceStart, Hit.ImpactPoint) > Data->MaxRange + 50.f)
	{
		return false;
	}

	// 3. Aim: the shot must roughly follow the shooter's replicated view direction.
	const FVector ShotDir = (Hit.ImpactPoint - Hit.TraceStart).GetSafeNormal();
	const FVector ViewDir = Soldier->GetBaseAimRotation().Vector();
	const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(ShotDir, ViewDir), -1.f, 1.f)));
	if (AngleDeg > AllowedSpreadDeg + AimToleranceDegrees)
	{
		UE_LOG(LogDVNet, Warning, TEXT("%s: shot %.1f deg off view direction"), *GetNameSafe(Soldier), AngleDeg);
		return false;
	}

	AActor* HitActor = Hit.GetActor();

	// 4. Line of sight: nothing solid between origin and impact (no shooting through walls).
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DVValidateShot), true, Soldier);
	Params.AddIgnoredActor(Weapon);
	if (HitActor)
	{
		Params.AddIgnoredActor(HitActor);
	}
	FHitResult Blocker;
	const FVector ToImpact = Hit.ImpactPoint - Hit.TraceStart;
	const FVector TraceEnd = Hit.TraceStart + ToImpact.GetSafeNormal() * FMath::Max(0.f, ToImpact.Size() - 10.f);
	if (GetWorld()->LineTraceSingleByChannel(Blocker, Hit.TraceStart, TraceEnd, DV_TRACE_WEAPON, Params))
	{
		UE_LOG(LogDVNet, Log, TEXT("%s: hit rejected, blocked by %s"), *GetNameSafe(Soldier), *GetNameSafe(Blocker.GetActor()));
		return false;
	}

	// 5. Target position: the server's copy of the target must be near the reported impact.
	//    Tolerance grows with the shooter's ping and the target's speed (stand-in for rewind).
	if (const APawn* TargetPawn = Cast<APawn>(HitActor))
	{
		const APlayerState* PS = Soldier->GetPlayerState();
		const float PingSeconds = PS ? PS->GetPingInMilliseconds() / 1000.f : 0.f;
		const float Tolerance = HitPositionTolerance + static_cast<float>(TargetPawn->GetVelocity().Size()) * FMath::Min(PingSeconds, 0.4f);

		const FBox Bounds = TargetPawn->GetComponentsBoundingBox(/*bNonColliding*/ false).ExpandBy(Tolerance);
		if (!Bounds.IsInside(Hit.ImpactPoint))
		{
			UE_LOG(LogDVNet, Warning, TEXT("%s: hit on %s outside tolerance"), *GetNameSafe(Soldier), *GetNameSafe(TargetPawn));
			return false;
		}
	}

	return true;
}

void UDVGA_FireWeapon::ApplySuppressionAlongShot(const FVector& Start, const FVector& End, AActor* DirectHitActor) const
{
	const ADVSoldierCharacter* Soldier = GetSoldier();
	const ADVWeaponBase* Weapon = GetWeapon();
	const UDVWeaponData* Data = Weapon ? Weapon->GetData() : nullptr;
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (!Soldier || !Data || !SourceASC || Data->SuppressionPerShot <= 0.f)
	{
		return;
	}

	const FVector Segment = End - Start;
	const float Length = Segment.Size();
	if (Length < 1.f)
	{
		return;
	}

	// Capsule swept along the bullet path catches everyone the round passed near.
	const FQuat Rotation = FRotationMatrix::MakeFromZ(Segment / Length).ToQuat();
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Data->SuppressionRadius, Length * 0.5f + Data->SuppressionRadius);

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DVSuppression), false, Soldier);
	GetWorld()->OverlapMultiByObjectType(Overlaps, Start + Segment * 0.5f, Rotation,
		FCollisionObjectQueryParams(ECC_Pawn), Shape, Params);

	TSet<AActor*> Done;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Other = Overlap.GetActor();
		if (!Other || Other == DirectHitActor || Done.Contains(Other))
		{
			continue;
		}
		Done.Add(Other);
		if (FGenericTeamId::GetAttitude(Soldier, Other) != ETeamAttitude::Hostile)
		{
			continue;
		}

		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Other);
		if (!TargetASC)
		{
			continue;
		}

		const float Distance = FMath::PointDistToSegment(Other->GetActorLocation(), Start, End);
		const float Falloff = 1.f - FMath::Clamp(Distance / Data->SuppressionRadius, 0.f, 1.f);
		FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(UDVSuppressionEffect::StaticClass(), GetAbilityLevel());
		if (Spec.IsValid() && Falloff > 0.f)
		{
			Spec.Data->SetSetByCallerMagnitude(DVTags::SetByCaller_Suppression, Data->SuppressionPerShot * (0.35f + 0.65f * Falloff));
			SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
		}
	}
}
