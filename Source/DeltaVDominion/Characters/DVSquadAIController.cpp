#include "Characters/DVSquadAIController.h"
#include "Characters/DVSoldierCharacter.h"
#include "Characters/DVCoverQuery.h"
#include "Combat/DVAttributeSet.h"
#include "Combat/DVWeaponBase.h"
#include "DeltaVDominion.h"

#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "EngineUtils.h"
#include "TimerManager.h"

ADVSquadAIController::ADVSquadAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	bWantsPlayerState = false;

	UAIPerceptionComponent* Perception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("Perception"));
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	SightConfig->SightRadius = SightRadius;
	SightConfig->LoseSightRadius = SightRadius * 1.1f;
	SightConfig->PeripheralVisionAngleDegrees = 75.f;
	SightConfig->SetMaxAge(TargetMemory);
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = false;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = false;
	Perception->ConfigureSense(*SightConfig);
	Perception->SetDominantSense(UAISense_Sight::StaticClass());
	SetPerceptionComponent(*Perception);
}

ADVSoldierCharacter* ADVSquadAIController::GetSoldier() const
{
	return Cast<ADVSoldierCharacter>(GetPawn());
}

void ADVSquadAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (ADVSoldierCharacter* Soldier = GetSoldier())
	{
		SetGenericTeamId(Soldier->GetGenericTeamId());
	}

	HoldPoint = InPawn->GetActorLocation();
	if (!Leader.IsValid())
	{
		// Unled soldiers (e.g. Hearthbound defenders) hold where they were placed.
		CurrentOrder.Order = EDVSquadOrder::Hold;
		CurrentOrder.Location = HoldPoint;
	}

	GetWorldTimerManager().SetTimer(ThinkTimer, this, &ADVSquadAIController::Think, ThinkInterval, true,
		FMath::FRandRange(0.f, ThinkInterval));	// stagger bots across frames
}

void ADVSquadAIController::OnUnPossess()
{
	GetWorldTimerManager().ClearTimer(ThinkTimer);
	Super::OnUnPossess();
}

ETeamAttitude::Type ADVSquadAIController::GetTeamAttitudeTowards(const AActor& Other) const
{
	if (const ADVSoldierCharacter* Soldier = GetSoldier())
	{
		const AActor* OtherActor = &Other;
		if (const AController* OtherController = Cast<AController>(&Other))
		{
			OtherActor = OtherController->GetPawn();
		}
		return OtherActor ? Soldier->GetTeamAttitudeTowards(*OtherActor) : ETeamAttitude::Neutral;
	}
	return Super::GetTeamAttitudeTowards(Other);
}

void ADVSquadAIController::SetLeader(ADVSoldierCharacter* InLeader, int32 InFormationSlot)
{
	Leader = InLeader;
	FormationSlot = InFormationSlot;
	CurrentOrder = FDVSquadOrderData();
	CurrentOrder.Order = EDVSquadOrder::Follow;
}

void ADVSquadAIController::ReceiveOrder(const FDVSquadOrderData& Order)
{
	CurrentOrder = Order;
	bFlankReached = false;
	bSuppressFire = false;
	BurstRemaining = 0;
	LeaveCover();
	CurrentMoveDest = FVector(FLT_MAX);

	switch (Order.Order)
	{
	case EDVSquadOrder::MoveTo:
	{
		// Spread out around the ordered point instead of stacking on it.
		const float Angle = FormationSlot * 2.1f;
		HoldPoint = Order.Location + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * (FormationSlot > 0 ? 180.f : 0.f);
		break;
	}
	case EDVSquadOrder::Hold:
		HoldPoint = GetPawn() ? GetPawn()->GetActorLocation() : Order.Location;
		break;
	case EDVSquadOrder::Flank:
	{
		const FVector TargetLoc = Order.Target.IsValid() ? Order.Target->GetActorLocation() : Order.Location;
		const FVector From = Leader.IsValid() ? Leader->GetActorLocation() : (GetPawn() ? GetPawn()->GetActorLocation() : TargetLoc);
		const FVector Dir = (TargetLoc - From).GetSafeNormal2D();
		const FVector Perp = FVector::CrossProduct(FVector::UpVector, Dir) * ((FormationSlot % 2) ? 1.f : -1.f);
		FlankPoint = TargetLoc + Perp * FlankDistance - Dir * 300.f;

		if (UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(GetWorld()))
		{
			FNavLocation Projected;
			if (Nav->ProjectPointToNavigation(FlankPoint, Projected, FVector(400.f, 400.f, 400.f)))
			{
				FlankPoint = Projected.Location;
			}
		}
		break;
	}
	case EDVSquadOrder::Suppress:
		SuppressUntil = GetWorld()->GetTimeSeconds() + SuppressDuration;
		break;
	default:
		break;
	}
}

// ---------------------------------------------------------------------------
// Think: perception + order behaviour (4 Hz)
// ---------------------------------------------------------------------------

void ADVSquadAIController::Think()
{
	ADVSoldierCharacter* Soldier = GetSoldier();
	if (!Soldier || Soldier->IsDead())
	{
		return;
	}

	if (Leader.IsValid() && Leader->IsDead())
	{
		// Leader down: hold and fight where we stand.
		Leader.Reset();
		FDVSquadOrderData HoldOrder;
		HoldOrder.Order = EDVSquadOrder::Hold;
		HoldOrder.Location = Soldier->GetActorLocation();
		ReceiveOrder(HoldOrder);
	}

	UpdateTarget();

	switch (CurrentOrder.Order)
	{
	case EDVSquadOrder::Follow:   BehaveFollow(false); break;
	case EDVSquadOrder::Regroup:  BehaveFollow(true);  break;
	case EDVSquadOrder::MoveTo:   BehaveMoveTo();      break;
	case EDVSquadOrder::Hold:     BehaveHold(HoldPoint); break;
	case EDVSquadOrder::Flank:    BehaveFlank();       break;
	case EDVSquadOrder::Suppress: BehaveSuppress();    break;
	default: break;
	}

	if (IsPinned())
	{
		State = EDVSquadAIState::Pinned;
	}
}

void ADVSquadAIController::UpdateTarget()
{
	const ADVSoldierCharacter* Soldier = GetSoldier();
	const double Now = GetWorld()->GetTimeSeconds();

	TArray<AActor*> Seen;
	GetPerceptionComponent()->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), Seen);

	AActor* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (AActor* Actor : Seen)
	{
		const ADVSoldierCharacter* Other = Cast<ADVSoldierCharacter>(Actor);
		if (!Other || Other->IsDead() || Soldier->GetTeamAttitudeTowards(*Other) != ETeamAttitude::Hostile)
		{
			continue;
		}
		// Prefer the current target slightly to avoid flip-flopping.
		const float DistSq = FVector::DistSquared(Soldier->GetActorLocation(), Other->GetActorLocation()) * (Actor == Target.Get() ? 0.7f : 1.f);
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Actor;
		}
	}

	if (Best)
	{
		Target = Best;
		LastKnownTargetLocation = Best->GetActorLocation();
		LastSeenTime = Now;
	}
	else if (Target.IsValid())
	{
		const ADVSoldierCharacter* Old = Cast<ADVSoldierCharacter>(Target.Get());
		if ((Old && Old->IsDead()) || Now - LastSeenTime > TargetMemory)
		{
			Target.Reset();
			BurstRemaining = 0;
			ClearFocus(EAIFocusPriority::Gameplay);
		}
	}
}

// ---------------------------------------------------------------------------
// Behaviours
// ---------------------------------------------------------------------------

void ADVSquadAIController::BehaveFollow(bool bRegroup)
{
	ADVSoldierCharacter* Soldier = GetSoldier();
	if (!Leader.IsValid())
	{
		BehaveHold(HoldPoint);
		return;
	}

	const FVector Slot = GetFormationSlotLocation();
	if (bRegroup)
	{
		// Ignore the fight and run to the leader.
		LeaveCover();
		BurstRemaining = 0;
		ClearFocus(EAIFocusPriority::Gameplay);
		MoveIfFar(Slot, 150.f, /*bSprint*/ true);
		State = EDVSquadAIState::Moving;
		if (FVector::Dist2D(Soldier->GetActorLocation(), Slot) < 300.f)
		{
			CurrentOrder.Order = EDVSquadOrder::Follow;
		}
		return;
	}

	const bool bLeaderNear = FVector::Dist(Soldier->GetActorLocation(), Leader->GetActorLocation()) < 2000.f;
	if (Target.IsValid() && bLeaderNear)
	{
		Engage(Leader->GetActorLocation(), CoverSearchRadius);
		return;
	}

	LeaveCover();
	SetFocalPoint(Leader->GetActorLocation() + Leader->GetActorForwardVector() * 2000.f);
	const bool bFar = FVector::Dist2D(Soldier->GetActorLocation(), Slot) > 1200.f;
	MoveIfFar(Slot, 200.f, bFar);
	State = GetMoveStatus() == EPathFollowingStatus::Moving ? EDVSquadAIState::Moving : EDVSquadAIState::Idle;
}

void ADVSquadAIController::BehaveMoveTo()
{
	ADVSoldierCharacter* Soldier = GetSoldier();
	if (FVector::Dist2D(Soldier->GetActorLocation(), HoldPoint) > 200.f)
	{
		// Shoot targets of opportunity on the way, but keep moving.
		if (Target.IsValid() && HasShotAt(TargetChest(), Target.Get()))
		{
			SetFocus(Target.Get());
			if (BurstRemaining == 0 && GetWorld()->GetTimeSeconds() >= NextBurstTime)
			{
				StartBurst(2, 4);
			}
		}
		else
		{
			SetFocalPoint(HoldPoint);
		}
		MoveIfFar(HoldPoint, 120.f);
		State = EDVSquadAIState::Moving;
		return;
	}

	// Arrived: become a hold at this point.
	CurrentOrder.Order = EDVSquadOrder::Hold;
	BehaveHold(HoldPoint);
}

void ADVSquadAIController::BehaveHold(const FVector& Anchor)
{
	if (Target.IsValid())
	{
		Engage(Anchor, CoverSearchRadius * 0.7f);
		return;
	}
	LeaveCover();
	MoveIfFar(Anchor, 150.f);
	State = EDVSquadAIState::Idle;
}

void ADVSquadAIController::BehaveFlank()
{
	ADVSoldierCharacter* Soldier = GetSoldier();
	AActor* FlankTarget = CurrentOrder.Target.IsValid() ? CurrentOrder.Target.Get() : Target.Get();
	if (const ADVSoldierCharacter* T = Cast<ADVSoldierCharacter>(FlankTarget); T && T->IsDead())
	{
		FlankTarget = nullptr;
	}

	if (!bFlankReached)
	{
		if (FVector::Dist2D(Soldier->GetActorLocation(), FlankPoint) < 250.f)
		{
			bFlankReached = true;
			HoldPoint = FlankPoint;
		}
		else
		{
			// Move fast and quiet; only fire if something is right in our face.
			if (Target.IsValid() && FVector::Dist(Soldier->GetActorLocation(), Target->GetActorLocation()) < 800.f)
			{
				SetFocus(Target.Get());
				if (BurstRemaining == 0 && GetWorld()->GetTimeSeconds() >= NextBurstTime)
				{
					StartBurst(2, 3);
				}
			}
			else
			{
				SetFocalPoint(FlankTarget ? FlankTarget->GetActorLocation() : FlankPoint);
			}
			MoveIfFar(FlankPoint, 150.f, /*bSprint*/ true);
			State = EDVSquadAIState::Flanking;
			return;
		}
	}

	if (FlankTarget)
	{
		Target = FlankTarget;
		LastKnownTargetLocation = FlankTarget->GetActorLocation();
		LastSeenTime = GetWorld()->GetTimeSeconds();
	}
	if (!Target.IsValid())
	{
		CurrentOrder.Order = EDVSquadOrder::Hold;
	}
	BehaveHold(HoldPoint);
}

void ADVSquadAIController::BehaveSuppress()
{
	ADVSoldierCharacter* Soldier = GetSoldier();
	const double Now = GetWorld()->GetTimeSeconds();

	if (Now > SuppressUntil)
	{
		bSuppressFire = false;
		CurrentOrder.Order = EDVSquadOrder::Hold;
		HoldPoint = Soldier->GetActorLocation();
		return;
	}

	const AActor* OrderTarget = CurrentOrder.Target.Get();
	SuppressPoint = OrderTarget ? OrderTarget->GetActorLocation() + FVector(0, 0, 40.f) : CurrentOrder.Location + FVector(0, 0, 60.f);

	if (!bInCover)
	{
		SeekCover(Soldier->GetActorLocation(), 500.f, SuppressPoint);
	}

	bSuppressFire = true;
	SetFocalPoint(SuppressPoint);
	if (BurstRemaining == 0 && Now >= NextBurstTime)
	{
		StartBurst(5, 9);
	}
	State = EDVSquadAIState::Suppressing;
}

// ---------------------------------------------------------------------------
// Combat helpers
// ---------------------------------------------------------------------------

void ADVSquadAIController::Engage(const FVector& Anchor, float CoverRadius)
{
	ADVSoldierCharacter* Soldier = GetSoldier();
	AActor* T = Target.Get();
	if (!T)
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	const FVector Threat = T->GetActorLocation() + FVector(0, 0, 60.f);

	// Re-validate cover periodically; the enemy may have moved around it.
	if (bInCover && Now >= NextCoverCheckTime)
	{
		bool bLow = false;
		if (!DVCover::IsCovered(GetWorld(), CoverLocation, Threat, bLow))
		{
			LeaveCover();
		}
		NextCoverCheckTime = Now + 2.0;
	}
	if (!bInCover)
	{
		SeekCover(Anchor, CoverRadius, Threat);
	}

	SetFocus(T);
	State = bInCover ? EDVSquadAIState::InCover : EDVSquadAIState::Engaging;

	const bool bCanSee = HasShotAt(TargetChest(), T);
	if (BurstRemaining == 0 && Now >= NextBurstTime && bCanSee)
	{
		if (IsPinned())
		{
			// Pinned soldiers mostly keep their heads down.
			if (FMath::FRand() < 0.25f)
			{
				StartBurst(1, 2);
			}
			else
			{
				NextBurstTime = Now + 1.0;
			}
		}
		else
		{
			StartBurst(3, 6);
		}
	}

	if (const ADVWeaponBase* Weapon = Soldier->GetCurrentWeapon(); Weapon && Weapon->NeedsReload() && BurstRemaining == 0)
	{
		Soldier->TryReload();
	}
}

bool ADVSquadAIController::SeekCover(const FVector& Anchor, float Radius, const FVector& Threat)
{
	DVCover::FResult Result;
	if (!DVCover::FindCover(GetWorld(), GetPawn(), Anchor, Radius, Threat, GetClaimedCoverPoints(), Result))
	{
		return false;
	}
	bInCover = true;
	bLowCover = Result.bLowCover;
	CoverLocation = Result.Location;
	NextCoverCheckTime = GetWorld()->GetTimeSeconds() + 2.0;
	MoveIfFar(CoverLocation, 60.f, FVector::Dist2D(GetPawn()->GetActorLocation(), CoverLocation) > 600.f);
	return true;
}

void ADVSquadAIController::LeaveCover()
{
	if (bInCover)
	{
		bInCover = false;
		if (ADVSoldierCharacter* Soldier = GetSoldier())
		{
			Soldier->UnCrouch();
		}
	}
}

void ADVSquadAIController::MoveIfFar(const FVector& Dest, float Tolerance, bool bSprint)
{
	ADVSoldierCharacter* Soldier = GetSoldier();
	if (!Soldier)
	{
		return;
	}
	if (FVector::Dist2D(Soldier->GetActorLocation(), Dest) <= Tolerance)
	{
		Soldier->SetSprinting(false);
		return;
	}
	const bool bNewDest = FVector::DistSquared(CurrentMoveDest, Dest) > FMath::Square(100.f);
	if (bNewDest || GetMoveStatus() == EPathFollowingStatus::Idle)
	{
		CurrentMoveDest = Dest;
		const EPathFollowingRequestResult::Type Result = MoveToLocation(Dest, Tolerance * 0.5f, /*bStopOnOverlap*/ true,
			/*bUsePathfinding*/ true, /*bProjectDestinationToNavigation*/ true, /*bCanStrafe*/ true);
		if (Result == EPathFollowingRequestResult::Failed)
		{
			UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(GetWorld());
			const ANavigationData* NavData = Nav ? Nav->GetNavDataForProps(Soldier->GetNavAgentPropertiesRef(), Soldier->GetNavAgentLocation()) : nullptr;
			FNavLocation Tmp;
			const bool bStartOnNav = Nav && Nav->ProjectPointToNavigation(Soldier->GetNavAgentLocation(), Tmp, FVector(100.f, 100.f, 200.f));
			const bool bDestOnNav = Nav && Nav->ProjectPointToNavigation(Dest, Tmp, FVector(100.f, 100.f, 200.f));
			UE_LOG(LogDVAI, Verbose, TEXT("%s: no path to %s (navdata %s, start on nav %d, dest on nav %d, agent r=%.0f h=%.0f)"), *GetNameSafe(Soldier),
				*Dest.ToCompactString(), *GetNameSafe(NavData), bStartOnNav, bDestOnNav,
				Soldier->GetNavAgentPropertiesRef().AgentRadius, Soldier->GetNavAgentPropertiesRef().AgentHeight);
		}
	}
	Soldier->SetSprinting(bSprint && !Target.IsValid());
}

void ADVSquadAIController::StartBurst(int32 MinShots, int32 MaxShots)
{
	BurstRemaining = FMath::RandRange(MinShots, MaxShots);
}

bool ADVSquadAIController::HasShotAt(const FVector& Point, const AActor* IgnoreTarget) const
{
	const APawn* MyPawn = GetPawn();
	if (!MyPawn)
	{
		return false;
	}
	// Test from standing eye height so soldiers behind low cover know they can pop up and fire.
	const FVector From = MyPawn->GetActorLocation() + FVector(0.f, 0.f, 70.f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DVAIShot), false, MyPawn);
	if (IgnoreTarget)
	{
		Params.AddIgnoredActor(IgnoreTarget);
	}
	return !GetWorld()->LineTraceTestByChannel(From, Point, ECC_Visibility, Params);
}

bool ADVSquadAIController::IsPinned() const
{
	const ADVSoldierCharacter* Soldier = GetSoldier();
	const UDVAttributeSet* Attr = Soldier ? Soldier->GetAttributeSet() : nullptr;
	if (!Attr)
	{
		return false;
	}
	// Steadier soldiers tolerate more incoming fire before going to ground.
	return Attr->GetSuppression() > 50.f + 35.f * Attr->GetMorale();
}

FVector ADVSquadAIController::TargetChest() const
{
	if (const AActor* T = Target.Get())
	{
		return T->GetActorLocation() + FVector(0.f, 0.f, 35.f);
	}
	return LastKnownTargetLocation;
}

FVector ADVSquadAIController::GetFormationSlotLocation() const
{
	if (!Leader.IsValid())
	{
		return HoldPoint;
	}
	// Wedge behind the leader.
	static const FVector Offsets[] = {
		FVector(-250.f, -220.f, 0.f), FVector(-250.f, 220.f, 0.f), FVector(-480.f, 0.f, 0.f),
		FVector(-480.f, -400.f, 0.f), FVector(-480.f, 400.f, 0.f)
	};
	const FVector Local = Offsets[FMath::Abs(FormationSlot) % static_cast<int32>(UE_ARRAY_COUNT(Offsets))];
	const FRotator Yaw(0.f, Leader->GetActorRotation().Yaw, 0.f);
	return Leader->GetActorLocation() + Yaw.RotateVector(Local);
}

TArray<FVector> ADVSquadAIController::GetClaimedCoverPoints() const
{
	TArray<FVector> Claimed;
	for (TActorIterator<ADVSquadAIController> It(GetWorld()); It; ++It)
	{
		if (*It != this && It->bInCover)
		{
			Claimed.Add(It->CoverLocation);
		}
	}
	return Claimed;
}

FVector ADVSquadAIController::GetAimPoint() const
{
	const ADVSoldierCharacter* Soldier = GetSoldier();
	if (!Soldier)
	{
		return FVector::ZeroVector;
	}

	const FVector Eyes = Soldier->GetPawnViewLocation();
	const FVector AimAt = (bSuppressFire || !Target.IsValid()) ? SuppressPoint : TargetChest();
	const FVector Dir = (AimAt - Eyes).GetSafeNormal();

	const UDVAttributeSet* Attr = Soldier->GetAttributeSet();
	const float Accuracy = Attr ? Attr->GetAccuracy() : 0.5f;
	const float Suppression01 = Attr ? Attr->GetSuppression() / 100.f : 0.f;
	float ErrorDeg = static_cast<float>(FMath::Lerp(AimErrorRange.X, AimErrorRange.Y, static_cast<double>(Accuracy))) * (1.f + 1.5f * Suppression01);
	if (Soldier->GetVelocity().SizeSquared() > FMath::Square(150.f))
	{
		ErrorDeg *= 1.3f;
	}

	const FVector Shot = FMath::VRandCone(Dir, FMath::DegreesToRadians(ErrorDeg));
	return Eyes + Shot * FVector::Dist(Eyes, AimAt);
}

// ---------------------------------------------------------------------------
// Tick: trigger discipline and cover posture
// ---------------------------------------------------------------------------

void ADVSquadAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ADVSoldierCharacter* Soldier = GetSoldier();
	if (!Soldier || Soldier->IsDead())
	{
		return;
	}

	const double Now = GetWorld()->GetTimeSeconds();
	const bool bHasAim = Target.IsValid() || bSuppressFire;
	const bool bArrivedInCover = bInCover && FVector::Dist2D(Soldier->GetActorLocation(), CoverLocation) < 120.f;

	if (BurstRemaining > 0 && bHasAim)
	{
		// Pop up from low cover to fire.
		if (bArrivedInCover && Soldier->bIsCrouched)
		{
			Soldier->UnCrouch();
		}
		Soldier->SetAiming(true);
		if (Soldier->TryFire())
		{
			if (--BurstRemaining == 0)
			{
				const UDVAttributeSet* Attr = Soldier->GetAttributeSet();
				const float Morale = Attr ? Attr->GetMorale() : 0.5f;
				// Calmer soldiers re-engage faster.
				NextBurstTime = Now + FMath::FRandRange(0.4f, 1.2f) * (1.5f - Morale);
			}
		}
		else if (const ADVWeaponBase* Weapon = Soldier->GetCurrentWeapon(); Weapon && (Weapon->NeedsReload() || Weapon->IsOverheated()))
		{
			BurstRemaining = 0;
			NextBurstTime = Now + 0.5;
		}
	}
	else
	{
		Soldier->SetAiming(false);
		// Between bursts, get back down behind low cover.
		if (bArrivedInCover && (bLowCover || IsPinned()) && !Soldier->bIsCrouched)
		{
			Soldier->Crouch();
		}
	}
}
