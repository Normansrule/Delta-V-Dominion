#include "Core/DVPlayerController.h"
#include "Characters/DVSoldierCharacter.h"
#include "Characters/DVCharacterMovementComponent.h"
#include "Characters/DVCommandComponent.h"
#include "Core/DVGameMode.h"
#include "Vehicles/DVVehicleKit.h"
#include "Core/DVInteractable.h"
#include "Core/DVCallIns.h"
#include "Core/DVProfileSubsystem.h"
#include "Core/DVRosterSubsystem.h"
#include "Combat/DVLoadouts.h"
#include "Combat/DVWeaponData.h"
#include "Engine/GameInstance.h"
#include "Vehicles/DVGroundVehicle.h"
#include "Vehicles/DVGunship.h"
#include "Vehicles/DVHoverbike.h"
#include "Vehicles/DVBoat.h"
#include "Core/DVGameState.h"
#include "Core/DVFx.h"
#include "Characters/DVBulwark.h"

#include "Camera/CameraActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/HUD.h"
#include "Components/InputComponent.h"
#include "InputCoreTypes.h"
#include "HighResScreenshot.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "EngineUtils.h"
#include "TimerManager.h"

ADVPlayerController::ADVPlayerController()
{
	bShowMouseCursor = false;
}

void ADVPlayerController::DVShowcase(int32 CameraIndex)
{
	if (CameraIndex < 0)
	{
		SetViewTargetWithBlend(GetPawn(), 0.5f);
		return;
	}

	const FName Tag(*FString::Printf(TEXT("Showcase%d"), CameraIndex));
	for (TActorIterator<ACameraActor> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(Tag))
		{
			SetViewTargetWithBlend(*It, 0.f);
			break;
		}
	}

	GetWorldTimerManager().SetTimer(ShowcaseTimer, this, &ADVPlayerController::ShowcaseOrderFlank, 1.5f, false);
}

void ADVPlayerController::ShowcaseOrderFlank()
{
	ADVSoldierCharacter* Leader = Cast<ADVSoldierCharacter>(GetPawn());
	if (!Leader)
	{
		return;
	}

	ADVSoldierCharacter* Nearest = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (TActorIterator<ADVSoldierCharacter> It(GetWorld()); It; ++It)
	{
		if (It->TeamId != Leader->TeamId && !It->IsDead())
		{
			const float DistSq = FVector::DistSquared(It->GetActorLocation(), Leader->GetActorLocation());
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				Nearest = *It;
			}
		}
	}
	if (Nearest)
	{
		Leader->GetCommandComponent()->IssueOrderAt(EDVSquadOrder::Flank, Nearest->GetActorLocation(), Nearest);
	}
}

void ADVPlayerController::DVCapture(const FString& Name, int32 Frames, float Interval, float Delay, bool bShowUI)
{
	bCaptureShowUI = bShowUI;
	if (MyHUD)
	{
		MyHUD->bShowHUD = bShowUI;	// clean plates for trailers and README shots
	}
	CaptureName = Name.IsEmpty() ? TEXT("capture") : Name;
	CaptureFramesLeft = FMath::Max(1, Frames);
	CaptureIndex = 0;
	CaptureInterval = Interval;
	GetWorldTimerManager().SetTimer(CaptureTimer, this, &ADVPlayerController::StartCapture, FMath::Max(Delay, 0.001f), false);
}

void ADVPlayerController::StartCapture()
{
	// Interval 0 = one shot per rendered frame; pair with -BENCHMARK -FPS=N for smooth fixed-step captures.
	if (CaptureInterval <= 0.f)
	{
		bCaptureEveryFrame = true;
		CaptureTick();
	}
	else
	{
		CaptureTick();
		GetWorldTimerManager().SetTimer(CaptureTimer, this, &ADVPlayerController::CaptureTick, CaptureInterval, true);
	}
}

void ADVPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (IsLocalController())
	{
		TickProgression();
	}
	if (bCaptureEveryFrame)
	{
		CaptureTick();
	}
}

void ADVPlayerController::CaptureTick()
{
	if (CaptureFramesLeft <= 0)
	{
		bCaptureEveryFrame = false;
		GetWorldTimerManager().ClearTimer(CaptureTimer);
		if (FParse::Param(FCommandLine::Get(), TEXT("DVQuitAfterCapture")))
		{
			// Give the last screenshot a moment to be read back and written before exiting.
			FTimerHandle QuitTimer;
			GetWorldTimerManager().SetTimer(QuitTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
			}), 2.f, false);
		}
		return;
	}

	const FString File = FPaths::ProjectSavedDir() / TEXT("Screenshots/DVCapture") / FString::Printf(TEXT("%s_%04d.png"), *CaptureName, CaptureIndex++);
	FScreenshotRequest::RequestScreenshot(File, bCaptureShowUI, /*bAddFilenameSuffix*/ false);
	--CaptureFramesLeft;
}

void ADVPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	// Plain key binding so it works with no pawn (dead, or on the after-action screen).
	InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &ADVPlayerController::OnRedeployPressed);
	InputComponent->BindKey(EKeys::B, IE_Pressed, this, &ADVPlayerController::OnOrbitalPressed);
	InputComponent->BindKey(EKeys::N, IE_Pressed, this, &ADVPlayerController::OnSupplyPressed);
	const FKey LockerKeys[4] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four };
	for (int32 Field = 0; Field < 4; ++Field)
	{
		FInputKeyBinding Binding(FInputChord(LockerKeys[Field]), IE_Pressed);
		Binding.KeyDelegate.GetDelegateForManualSet().BindWeakLambda(this, [this, Field]() { OnLockerKey(Field); });
		InputComponent->KeyBindings.Add(Binding);
	}
	InputComponent->BindKey(EKeys::H, IE_Pressed, this, &ADVPlayerController::OnStrafePressed);
	InputComponent->BindKey(EKeys::M, IE_Pressed, this, &ADVPlayerController::OnMapPressed);
	InputComponent->BindKey(EKeys::Z, IE_Pressed, this, &ADVPlayerController::OnInteractPressed);
	InputComponent->BindKey(EKeys::Z, IE_Released, this, &ADVPlayerController::OnInteractReleased);
}

void ADVPlayerController::OnRedeployPressed()
{
	const ADVGameState* GS = GetWorld()->GetGameState<ADVGameState>();
	if (GS && GS->Phase != EDVMissionPhase::InProgress)
	{
		ServerRequestRedeploy();
	}
}

void ADVPlayerController::ServerRequestRedeploy_Implementation()
{
	if (ADVGameMode* GM = GetWorld()->GetAuthGameMode<ADVGameMode>())
	{
		GM->RequestRedeploy();
	}
}

void ADVPlayerController::ClientHitConfirmed_Implementation(bool bKill, bool bHeadshot)
{
	LastHitTime = GetWorld()->GetTimeSeconds();
	DVSound::UI(GetWorld(), bKill ? TEXT("SW_KillConfirm") : TEXT("SW_HitMarker"), bKill ? 0.6f : 0.4f);
	if (bKill)
	{
		const bool bQuiet = GetWorld()->GetTimeSeconds() - LastTakedownRequest < 1.0;
		AwardXP(bQuiet ? 40 : bHeadshot ? 35 : 25, bQuiet ? TEXT("Silent takedown") : bHeadshot ? TEXT("Headshot") : TEXT("Kill"));
	}
	bLastHitKill = bKill;
	bLastHitHeadshot = bHeadshot;
}

void ADVPlayerController::DVEndMission(bool bSuccess, float Delay)
{
#if !UE_BUILD_SHIPPING
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, bSuccess]() { ServerEndMission(bSuccess); }),
		FMath::Max(Delay, 0.001f), false);
#endif
}

void ADVPlayerController::ServerEndMission_Implementation(bool bSuccess)
{
#if !UE_BUILD_SHIPPING
	if (ADVGameMode* GM = GetWorld()->GetAuthGameMode<ADVGameMode>())
	{
		GM->EndMission(bSuccess);
	}
#endif
}

void ADVPlayerController::DVOrbitalStrike(float Delay)
{
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this]() { OnOrbitalPressed(); }), FMath::Max(Delay, 0.001f), false);
}

void ADVPlayerController::OnOrbitalPressed()
{
	const APawn* MyPawn = GetPawn();
	if (!MyPawn || !PlayerCameraManager)
	{
		return;
	}
	// Designate whatever is under the crosshair (ground or target).
	const FVector Start = PlayerCameraManager->GetCameraLocation();
	const FVector End = Start + PlayerCameraManager->GetActorForwardVector() * 20000.f;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DVOrbitalDesignate), false, MyPawn);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		ServerCallOrbitalStrike(Hit.ImpactPoint);
	}
}

bool ADVPlayerController::DesignateTarget(FVector& OutTarget) const
{
	const APawn* MyPawn = GetPawn();
	if (!MyPawn || !PlayerCameraManager)
	{
		return false;
	}
	const FVector Start = PlayerCameraManager->GetCameraLocation();
	const FVector End = Start + PlayerCameraManager->GetActorForwardVector() * 20000.f;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DVCallInDesignate), false, MyPawn);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		OutTarget = Hit.ImpactPoint;
		return true;
	}
	return false;
}

void ADVPlayerController::OnSupplyPressed()
{
	FVector Target;
	if (DesignateTarget(Target))
	{
		ServerCallIn(0, Target);
	}
}

void ADVPlayerController::OnStrafePressed()
{
	FVector Target;
	if (DesignateTarget(Target))
	{
		ServerCallIn(1, Target);
	}
}

void ADVPlayerController::DVSupplyDrop(float Delay, float Ahead)
{
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, Ahead]()
	{
		const APawn* MyPawn = GetPawn();
		if (Ahead <= 0.f || !MyPawn)
		{
			OnSupplyPressed();
			return;
		}
		// Drop a fixed distance in front of the soldier, onto the ground.
		const FVector Spot = MyPawn->GetActorLocation() + MyPawn->GetActorForwardVector().GetSafeNormal2D() * Ahead;
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(DVSupplyAhead), false, MyPawn);
		if (GetWorld()->LineTraceSingleByChannel(Hit, Spot + FVector(0.f, 0.f, 2000.f), Spot - FVector(0.f, 0.f, 4000.f), ECC_Visibility, Params))
		{
			ServerCallIn(0, Hit.ImpactPoint);
		}
	}), FMath::Max(Delay, 0.001f), false);
}

void ADVPlayerController::DVStrafeRun(float Delay)
{
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this]() { OnStrafePressed(); }), FMath::Max(Delay, 0.001f), false);
}

void ADVPlayerController::ServerCallIn_Implementation(uint8 Kind, FVector_NetQuantize Target)
{
	if (ADVGameMode* GM = GetWorld()->GetAuthGameMode<ADVGameMode>())
	{
		if (Kind == 0)
		{
			GM->RequestSupplyDrop(this, Target);
		}
		else
		{
			GM->RequestStrafeRun(this, Target);
		}
	}
}

void ADVPlayerController::DVReadyCallIns()
{
#if !UE_BUILD_SHIPPING
	ServerReadyCallIns();
#endif
}

void ADVPlayerController::DVSkipStage(float Delay)
{
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this]() { ServerSkipStage(); }), FMath::Max(Delay, 0.001f), false);
}

void ADVPlayerController::DVHurtVehicle(float Amount, float Delay)
{
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, Amount]() { ServerHurtVehicle(Amount); }), FMath::Max(Delay, 0.001f), false);
}

void ADVPlayerController::ServerHurtVehicle_Implementation(float Amount)
{
#if !UE_BUILD_SHIPPING
	const APawn* MyPawn = GetPawn();
	UDVSeatComponent* Best = MyPawn ? MyPawn->FindComponentByClass<UDVSeatComponent>() : nullptr;
	float BestDist = TNumericLimits<float>::Max();
	const bool bInVehicle = Best != nullptr;
	for (TActorIterator<APawn> It(GetWorld()); It && !bInVehicle; ++It)
	{
		UDVSeatComponent* Seat = It->FindComponentByClass<UDVSeatComponent>();
		const float Dist = MyPawn ? FVector::Dist(It->GetActorLocation(), MyPawn->GetActorLocation()) : 0.f;
		if (Seat && !Seat->bWrecked && Dist < BestDist)
		{
			BestDist = Dist;
			Best = Seat;
		}
	}
	if (Best)
	{
		Best->ApplyHullDamage(Amount, nullptr, true);
	}
#endif
}

void ADVPlayerController::ServerSkipStage_Implementation()
{
	if (ADVGameMode* GM = GetWorld()->GetAuthGameMode<ADVGameMode>())
	{
		GM->DevAdvanceStage();
	}
}

void ADVPlayerController::ServerReadyCallIns_Implementation()
{
#if !UE_BUILD_SHIPPING
	if (ADVGameState* GS = GetWorld()->GetGameState<ADVGameState>())
	{
		const float Now = GetWorld()->GetTimeSeconds();
		GS->OrbitalReadyTime = FMath::Min(GS->OrbitalReadyTime, Now);
		GS->SupplyReadyTime = FMath::Min(GS->SupplyReadyTime, Now);
		GS->StrafeReadyTime = FMath::Min(GS->StrafeReadyTime, Now);
	}
#endif
}

void ADVPlayerController::DVViewBulwark(float Distance)
{
	const APawn* MyPawn = GetPawn();
	ADVBulwark* Best = nullptr;
	for (TActorIterator<ADVBulwark> It(GetWorld()); It; ++It)
	{
		if (!Best || (MyPawn && FVector::DistSquared(It->GetActorLocation(), MyPawn->GetActorLocation()) < FVector::DistSquared(Best->GetActorLocation(), MyPawn->GetActorLocation())))
		{
			Best = *It;
		}
	}
	if (!Best)
	{
		return;
	}
	const FVector Focus = Best->GetActorLocation() + FVector(0.f, 0.f, 40.f);
	// In front of it, slightly off-axis; swing around until nothing blocks the view.
	const FVector Forward = Best->GetActorForwardVector().GetSafeNormal2D();
	FVector Eye = Focus + Forward.RotateAngleAxis(25.f, FVector::UpVector) * Distance + FVector(0.f, 0.f, 70.f);
	FCollisionQueryParams Q(SCENE_QUERY_STAT(DVViewBulwark), false, Best);
	for (int32 Step = 0; Step < 24; ++Step)
	{
		const float Angle = 25.f + (Step % 2 ? 1.f : -1.f) * 15.f * ((Step + 1) / 2);
		const FVector Try = Focus + Forward.RotateAngleAxis(Angle, FVector::UpVector) * Distance + FVector(0.f, 0.f, 70.f);
		if (!GetWorld()->LineTraceTestByChannel(Focus, Try, ECC_Visibility, Q))
		{
			Eye = Try;
			break;
		}
	}
	FActorSpawnParameters Params;
	if (ACameraActor* Cam = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), Eye, (Focus - Eye).Rotation(), Params))
	{
		SetViewTarget(Cam);
	}
}

void ADVPlayerController::ServerCallOrbitalStrike_Implementation(FVector_NetQuantize Target)
{
	if (ADVGameMode* GM = GetWorld()->GetAuthGameMode<ADVGameMode>())
	{
		GM->RequestOrbitalStrike(this, Target);
	}
}

void ADVPlayerController::OnMapPressed()
{
	const ADVGameState* GS = GetWorld()->GetGameState<ADVGameState>();
	if (GS && GS->Phase != EDVMissionPhase::InProgress && IsLocalController())
	{
		UGameplayStatics::OpenLevel(this, FName(TEXT("/Game/Maps/Kestrel")));	// back aboard the Kestrel
	}
}

void ADVPlayerController::OnInteractReleased()
{
	ServerSetReviveHeld(false);
}

void ADVPlayerController::ServerSetReviveHeld_Implementation(bool bHeld)
{
	if (ADVSoldierCharacter* Soldier = Cast<ADVSoldierCharacter>(GetPawn()))
	{
		Soldier->SetReviveHeld(bHeld && !Soldier->IsOutOfAction());
	}
}

void ADVPlayerController::OnInteractPressed()
{
	APawn* MyPawn = GetPawn();
	if (!MyPawn)
	{
		return;
	}
	// A downed squadmate in reach takes priority over everything else Z does.
	if (const ADVSoldierCharacter* Me = Cast<ADVSoldierCharacter>(MyPawn))
	{
		for (TActorIterator<ADVSoldierCharacter> It(GetWorld()); It; ++It)
		{
			if (*It != Me && It->IsDowned() && It->TeamId == Me->TeamId && FVector::Dist(It->GetActorLocation(), Me->GetActorLocation()) < ADVSoldierCharacter::ReviveRange)
			{
				ServerSetReviveHeld(true);
				return;
			}
		}
	}
	if (MyPawn->FindComponentByClass<UDVSeatComponent>())
	{
		ServerExitVehicle();
	}
	else if (ADVInteractable* Thing = ADVInteractable::FindNear(MyPawn))
	{
		switch (Thing->Interaction)
		{
		case EDVInteraction::WarTable:
			UGameplayStatics::OpenLevel(this, FName(TEXT("/Game/Maps/Galaxy")));
			break;
		case EDVInteraction::Memorial:
			bShowMemorial = !bShowMemorial;
			break;
		case EDVInteraction::Locker:
			SetLockerOpen(!bShowLocker);
			break;
		case EDVInteraction::Armoury:
			if (const UDVRosterSubsystem* Roster = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDVRosterSubsystem>() : nullptr)
			{
				// Each press takes the next weapon off the rack; racks you haven't earned the standing for stay locked.
				const UDVProfileSubsystem* Profile = GetGameInstance()->GetSubsystem<UDVProfileSubsystem>();
				ArmouryNote.Reset();
				int32 Next = Roster->PlayerLoadout;
				for (int32 Tries = 0; Tries < DVLoadouts::Num(); ++Tries)
				{
					Next = (Next + 1) % DVLoadouts::Num();
					int32 Faction = 0, Needed = 0;
					if (WeaponLock(Next, Faction, Needed) && Profile && Profile->GetRep(Faction) < Needed)
					{
						ArmouryNote += FString::Printf(TEXT("%s locked: needs %s %s.  "), *DVLoadouts::Get(Next)->DisplayName.ToString(),
							*UDVProfileSubsystem::FactionName(Faction), *UDVProfileSubsystem::TierName(Needed));
						continue;
					}
					break;
				}
				DVLoadout(Next);
			}
			break;
		default:
			break;
		}
	}
	else if (UDVSeatComponent* Seat = UDVSeatComponent::FindNear(Cast<ADVSoldierCharacter>(MyPawn)))
	{
		ServerEnterVehicle(Cast<APawn>(Seat->GetOwner()));
	}
}

void ADVPlayerController::DVLoadout(int32 Index)
{
	if (Index < 0 || Index >= DVLoadouts::Num())
	{
		return;
	}
	if (UDVRosterSubsystem* Roster = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDVRosterSubsystem>() : nullptr)
	{
		Roster->PlayerLoadout = Index;
	}
	ArmouryTime = GetWorld()->GetTimeSeconds();
	DVSound::UI(GetWorld(), TEXT("SW_UIClick"), 0.6f);
	ServerEquipLoadout(Index);
}

void ADVPlayerController::ServerEquipLoadout_Implementation(int32 Index)
{
	ADVSoldierCharacter* Soldier = Cast<ADVSoldierCharacter>(GetPawn());
	UDVWeaponData* Data = DVLoadouts::Get(Index);
	if (Soldier && Data)
	{
		Soldier->EquipWeaponData(Data);
	}
}

void ADVPlayerController::AwardXP(int32 Amount, const FString& Reason)
{
	UDVProfileSubsystem* Profile = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDVProfileSubsystem>() : nullptr;
	if (!Profile)
	{
		return;
	}
	const int32 Before = Profile->GetLevel();
	Profile->AddXP(Amount, Reason);
	if (Profile->GetLevel() > Before)
	{
		DVSound::UI(GetWorld(), TEXT("SW_KillConfirm"), 0.9f);
		if (ADVSoldierCharacter* Soldier = Cast<ADVSoldierCharacter>(GetPawn()))
		{
			Soldier->ServerSetCommanderLevel(Profile->GetLevel());
		}
	}
}

void ADVPlayerController::TickProgression()
{
	const ADVGameState* GS = GetWorld()->GetGameState<ADVGameState>();
	if (!GS)
	{
		return;
	}
	// Each operation stage reached is worth XP; so is finishing (more for a win).
	const uint8 Stage = static_cast<uint8>(GS->Stage);
	if (GS->bMultiStage && Stage > SeenStage)
	{
		SeenStage = Stage;
		AwardXP(100, Stage == 1 ? TEXT("Garrison cleared") : Stage == 2 ? TEXT("Charges planted") : TEXT("Target destroyed"));
	}
	if (!bMissionXPAwarded && GS->Phase != EDVMissionPhase::InProgress && !GetWorld()->GetMapName().Contains(TEXT("Kestrel")))
	{
		bMissionXPAwarded = true;
		AwardXP(GS->Phase == EDVMissionPhase::Success ? 300 : 60, GS->Phase == EDVMissionPhase::Success ? TEXT("Mission accomplished") : TEXT("Survived to fight again"));
	}
}

bool ADVPlayerController::WeaponLock(int32 Index, int32& OutFaction, int32& OutNeeded)
{
	switch (Index)
	{
	case 3: OutFaction = DVFactions::Accord; OutNeeded = DVFactions::Trusted; return true;		// Longreach DMR: Accord issue, earned
	case 4: OutFaction = DVFactions::FreePorts; OutNeeded = DVFactions::Trusted; return true;	// Pulse Carbine: black-market tech
	default: return false;
	}
}

void ADVPlayerController::PushReputation()
{
	if (const UDVProfileSubsystem* Profile = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDVProfileSubsystem>() : nullptr)
	{
		ServerSetReputation(Profile->GetRep(DVFactions::Accord), Profile->GetRep(DVFactions::Colonies), Profile->GetRep(DVFactions::FreePorts));
	}
}

void ADVPlayerController::ServerSetReputation_Implementation(int32 Accord, int32 Colonies, int32 FreePorts)
{
	ServerRep = { FMath::Clamp(Accord, -100, 100), FMath::Clamp(Colonies, -100, 100), FMath::Clamp(FreePorts, -100, 100) };
}

void ADVPlayerController::PushLookToPawn()
{
	const UDVProfileSubsystem* Profile = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDVProfileSubsystem>() : nullptr;
	if (ADVSoldierCharacter* Soldier = Cast<ADVSoldierCharacter>(GetPawn()); Soldier && Profile)
	{
		Soldier->ServerSetLook(Profile->GetLook());
		Soldier->ServerSetCommanderLevel(Profile->GetLevel());
		PushReputation();
	}
}

void ADVPlayerController::SetLockerOpen(bool bOpen)
{
	bShowLocker = bOpen;
	ADVSoldierCharacter* Soldier = Cast<ADVSoldierCharacter>(GetPawn());
	if (!Soldier)
	{
		return;
	}
	Soldier->SetShowFullBody(bOpen);
	if (bOpen)
	{
		// A mirror: look at yourself from a few metres in front.
		const FVector Focus = Soldier->GetActorLocation() + FVector(0.f, 0.f, 30.f);
		const FVector Eye = Focus + Soldier->GetActorForwardVector() * 300.f + Soldier->GetActorRightVector() * 60.f + FVector(0.f, 0.f, 40.f);
		if (!MirrorCamera)
		{
			MirrorCamera = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), Eye, (Focus - Eye).Rotation());
		}
		MirrorCamera->SetActorLocationAndRotation(Eye, (Focus - Eye).Rotation());
		SetViewTargetWithBlend(MirrorCamera, 0.35f);
	}
	else
	{
		SetViewTargetWithBlend(Soldier, 0.35f);
	}
}

void ADVPlayerController::DVMoveDemo(int32 Mode, float Delay)
{
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, Mode]()
	{
		ADVSoldierCharacter* Soldier = Cast<ADVSoldierCharacter>(GetPawn());
		UDVCharacterMovementComponent* Move = Soldier ? Cast<UDVCharacterMovementComponent>(Soldier->GetCharacterMovement()) : nullptr;
		if (!Move)
		{
			return;
		}
		if (Mode == 0)
		{
			Soldier->Jump();
			FTimerHandle Boost;
			GetWorldTimerManager().SetTimer(Boost, FTimerDelegate::CreateWeakLambda(this, [Move]() { Move->bWantsToBoost = true; }), 0.35f, false);
		}
		else if (Mode == 1)
		{
			Move->Velocity = Soldier->GetActorForwardVector() * 700.f;	// as if sprinting
			Move->bWantsToSlide = true;
		}
		else
		{
			Move->bWantsToBoost = true;
		}
	}), FMath::Max(Delay, 0.001f), false);
}

void ADVPlayerController::DVRavenMode(int32 bJet, int32 bCockpit, float Delay)
{
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, bJet, bCockpit]()
	{
		if (ADVGunship* Air = Cast<ADVGunship>(GetPawn()))
		{
			Air->DebugSetModes(bJet != 0, bCockpit != 0);
		}
	}), FMath::Max(Delay, 0.001f), false);
}

void ADVPlayerController::DVWarp(float X, float Y, float Z)
{
	ServerWarp(FVector(X, Y, Z), GetControlRotation().Yaw);
}

void ADVPlayerController::ServerWarp_Implementation(FVector_NetQuantize To, float Yaw)
{
#if !UE_BUILD_SHIPPING
	if (APawn* P = GetPawn())
	{
		P->TeleportTo(To, FRotator(0.f, Yaw, 0.f));
	}
#endif
}

void ADVPlayerController::DVSalvageTest(float Delay)
{
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		for (TActorIterator<ADVSalvage> It(GetWorld()); It; ++It)
		{
			ServerWarp(It->GetActorLocation() + FVector(-120.f, 0.f, 100.f), 0.f);
			return;
		}
	}), FMath::Max(Delay, 0.001f), false);
}

void ADVPlayerController::DVTakedownTest(float Delay)
{
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		ADVSoldierCharacter* Me = Cast<ADVSoldierCharacter>(GetPawn());
		ADVSoldierCharacter* Best = nullptr;
		for (TActorIterator<ADVSoldierCharacter> It(GetWorld()); It && Me; ++It)
		{
			if (It->TeamId == Me->TeamId || It->IsOutOfAction() || It->IsAlerted())
			{
				continue;
			}
			// Pick a sentry standing on its own, so nobody else watches the approach.
			bool bAlone = true;
			for (TActorIterator<ADVSoldierCharacter> Other(GetWorld()); Other; ++Other)
			{
				bAlone &= *Other == *It || Other->TeamId == Me->TeamId || FVector::Dist(Other->GetActorLocation(), It->GetActorLocation()) > 900.f;
			}
			if (bAlone && (!Best || FVector::Dist(It->GetActorLocation(), Me->GetActorLocation()) < FVector::Dist(Best->GetActorLocation(), Me->GetActorLocation())))
			{
				Best = *It;
			}
		}
		UE_LOG(LogTemp, Log, TEXT("DVTakedownTest: sneaking up on %s"), Best ? *Best->GetName() : TEXT("nobody (no isolated sentry)"));
		if (!Best)
		{
			return;
		}
		// Step in right behind them, facing their back.
		const FVector Behind = Best->GetActorLocation() - Best->GetActorForwardVector() * 130.f;
		const float Yaw = Best->GetActorRotation().Yaw;
		SetControlRotation(FRotator(-10.f, Yaw, 0.f));
		ServerWarp(Behind, Yaw);
		Me->Crouch();	// sneaking, as a player would
		// ...and take them down a moment later, as if F was pressed.
		FTimerHandle Strike;
		GetWorldTimerManager().SetTimer(Strike, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (ADVSoldierCharacter* Me2 = Cast<ADVSoldierCharacter>(GetPawn()))
			{
				ADVSoldierCharacter* Victim = Me2->FindTakedownTarget();
				UE_LOG(LogTemp, Log, TEXT("DVTakedownTest: target %s"), Victim ? *Victim->GetName() : TEXT("none in reach"));
				if (Victim)
				{
					LastTakedownRequest = GetWorld()->GetTimeSeconds();
					Me2->ServerTakedown(Victim);
				}
			}
		}), 0.3f, false);
	}), FMath::Max(Delay, 0.001f), false);
}

void ADVPlayerController::DVLocker(int32 Field, int32 Times)
{
	if (!bShowLocker)
	{
		SetLockerOpen(true);
	}
	for (int32 i = 0; i < Times; ++i)
	{
		OnLockerKey(Field);
	}
}

void ADVPlayerController::OnLockerKey(int32 Field)
{
	if (!bShowLocker)
	{
		return;
	}
	if (UDVProfileSubsystem* Profile = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDVProfileSubsystem>() : nullptr)
	{
		Profile->Cycle(Field);
		DVSound::UI(GetWorld(), TEXT("SW_UIClick"), 0.6f);
		PushLookToPawn();
	}
}

void ADVPlayerController::AcknowledgePossession(APawn* P)
{
	Super::AcknowledgePossession(P);
	if (Cast<ADVSoldierCharacter>(P))
	{
		PushLookToPawn();	// your commander, the way you dressed them
	}
	// Carry the armoury pick into every mission (and onto a redeployed soldier).
	const UDVRosterSubsystem* Roster = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDVRosterSubsystem>() : nullptr;
	if (Roster && Roster->PlayerLoadout > 0 && Cast<ADVSoldierCharacter>(P))
	{
		ServerEquipLoadout(Roster->PlayerLoadout);
	}
}

void ADVPlayerController::ServerEnterVehicle_Implementation(APawn* Vehicle)
{
	UDVSeatComponent* Seat = Vehicle ? Vehicle->FindComponentByClass<UDVSeatComponent>() : nullptr;
	if (Seat)
	{
		Seat->Enter(Cast<ADVSoldierCharacter>(GetPawn()), this);
	}
}

void ADVPlayerController::ServerExitVehicle_Implementation()
{
	if (UDVSeatComponent* Seat = GetPawn() ? GetPawn()->FindComponentByClass<UDVSeatComponent>() : nullptr)
	{
		Seat->Exit(this);
	}
}

void ADVPlayerController::DVDebugPawns()
{
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		const USkeletalMeshComponent* Skel = It->FindComponentByClass<USkeletalMeshComponent>();
		UE_LOG(LogTemp, Log, TEXT("DVPAWN %s %s loc=%s hidden=%d skel=%s skelVisible=%d sim=%d"), *It->GetName(), *It->GetClass()->GetName(),
			*It->GetActorLocation().ToCompactString(), It->IsHidden(), Skel ? *GetNameSafe(Skel->GetSkeletalMeshAsset()) : TEXT("-"),
			Skel ? Skel->IsVisible() : 0, Skel ? Skel->IsSimulatingPhysics() : 0);
	}
}

void ADVPlayerController::DVInteract(float Delay)
{
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this]() { OnInteractPressed(); }), FMath::Max(Delay, 0.001f), false);
}

void ADVPlayerController::DVAutopilot(float Delay, float Throttle, float Steer, float Up, int32 Fire)
{
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, Throttle, Steer, Up, Fire]()
	{
		if (ADVGroundVehicle* Ground = Cast<ADVGroundVehicle>(GetPawn()))
		{
			Ground->SetAutopilot(Throttle, Steer, Fire != 0);
		}
		else if (ADVGunship* Air = Cast<ADVGunship>(GetPawn()))
		{
			Air->SetAutopilot(FVector(Throttle, 0.f, Up), Steer * 30.f, Fire == 1 || Fire == 3, Fire >= 2);
		}
		else if (ADVHoverbike* Bike = Cast<ADVHoverbike>(GetPawn()))
		{
			Bike->SetAutopilot(Throttle, Steer * 40.f, Fire != 0);
		}
		else if (ADVBoat* Boat = Cast<ADVBoat>(GetPawn()))
		{
			Boat->SetAutopilot(Throttle, Steer * 30.f, Fire != 0);
		}
	}), FMath::Max(Delay, 0.001f), false);
}

void ADVPlayerController::DVBoard(const FString& Which, float Delay)
{
#if !UE_BUILD_SHIPPING
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, Which]()
	{
		for (TActorIterator<APawn> It(GetWorld()); It; ++It)
		{
			const bool bMatch = (Which.Equals(TEXT("tank"), ESearchCase::IgnoreCase) && It->IsA<ADVTank>())
				|| (Which.Equals(TEXT("ltv"), ESearchCase::IgnoreCase) && It->IsA<ADVGroundVehicle>() && !It->IsA<ADVTank>())
				|| (Which.Equals(TEXT("gunship"), ESearchCase::IgnoreCase) && It->IsA<ADVGunship>())
				|| (Which.Equals(TEXT("bike"), ESearchCase::IgnoreCase) && It->IsA<ADVHoverbike>())
				|| (Which.Equals(TEXT("boat"), ESearchCase::IgnoreCase) && It->IsA<ADVBoat>());
			if (bMatch && GetPawn())
			{
				GetPawn()->TeleportTo(It->GetActorLocation() + FVector(0.f, -300.f, 120.f), GetPawn()->GetActorRotation());
				ServerEnterVehicle(*It);
				return;
			}
		}
	}), FMath::Max(Delay, 0.001f), false);
#endif
}
