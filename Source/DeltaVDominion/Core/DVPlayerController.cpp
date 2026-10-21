#include "Core/DVPlayerController.h"
#include "Characters/DVSoldierCharacter.h"
#include "Characters/DVCharacterMovementComponent.h"
#include "Characters/DVCommandComponent.h"
#include "Core/DVGameMode.h"
#include "Vehicles/DVWalker.h"
#include "Combat/DVAttributeSet.h"
#include "Vehicles/DVKestrel.h"
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
#include "Core/DVPressureManifold.h"
#include "Core/DVHollyhock.h"
#include "Characters/DVTownsfolk.h"

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
	if (bBoardTest)
	{
		TickBoardTest();
	}
	if (bHerdTest || bRepairTest)
	{
		TickHerdTest();
	}
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
	InputComponent->BindKey(EKeys::K, IE_Pressed, this, &ADVPlayerController::OnFlyHomePressed);
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
		if (UDVProfileSubsystem* Profile = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDVProfileSubsystem>() : nullptr)
		{
			const bool bWasDone = Profile->IsContractDone();
			if (bQuiet)
			{
				Profile->ContractEvent(DVContracts::SilentTakedown);
			}
			if (bHeadshot)
			{
				Profile->ContractEvent(DVContracts::Headshot);
			}
			if (Profile->IsContractDone() != bWasDone)
			{
				DVSound::UI(GetWorld(), TEXT("SW_KillConfirm"), 0.7f);
			}
		}
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

void ADVPlayerController::OnFlyHomePressed()
{
	const ADVGameState* GS = GetWorld()->GetGameState<ADVGameState>();
	if (GS && GS->Phase != EDVMissionPhase::InProgress && GS->Ascent == 0)
	{
		ServerRequestAscent();
	}
}

void ADVPlayerController::ServerRequestAscent_Implementation()
{
	if (ADVGameMode* GM = GetWorld()->GetAuthGameMode<ADVGameMode>())
	{
		GM->BeginAscent(this);
	}
}

void ADVPlayerController::DVFlyHome(float Delay)
{
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this]() { ServerRequestAscent(); }), FMath::Max(Delay, 0.001f), false);
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
		const ADVGameState* FlightGS = GetWorld()->GetGameState<ADVGameState>();
		if (!FlightGS || FlightGS->Ascent != 1)	// no stepping out of a Raven on its way to orbit
		{
			ServerExitVehicle();
		}
	}
	else if (const ADVPressureManifold* Manifold = ADVPressureManifold::Find(GetWorld()); Manifold && Manifold->FindValveNear(MyPawn) >= 0)
	{
		ServerTurnValve(static_cast<uint8>(Manifold->FindValveNear(MyPawn)));
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
		case EDVInteraction::ContractBoard:
			// Each press takes the next job on the board; the one you hold is the one you work.
			if (UDVProfileSubsystem* Profile = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDVProfileSubsystem>() : nullptr)
			{
				Profile->TakeContract((Profile->GetActiveContract() + 1) % UDVProfileSubsystem::NumContracts());
				DVSound::UI(GetWorld(), TEXT("SW_UIClick"), 0.6f);
			}
			break;
		case EDVInteraction::Shuttle:
			if (Thing->Destination.Contains(TEXT("Kestrel")) && ADVKestrel::Find(GetWorld()))
			{
				ServerRequestAscent();	// the Kestrel is right overhead: fly up to it instead of loading
			}
			else if (!Thing->Destination.IsEmpty())
			{
				UGameplayStatics::OpenLevel(this, FName(*Thing->Destination));
			}
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
		// A finished Rook's Rest contract is paid when the operation is won.
		UDVProfileSubsystem* Profile = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDVProfileSubsystem>() : nullptr;
		if (Profile && GS->Phase == EDVMissionPhase::Success && Profile->IsContractDone())
		{
			const FString Name = UDVProfileSubsystem::Contract(Profile->GetActiveContract()).Name;
			const int32 Pay = Profile->PayContract();
			AwardXP(Pay, FString::Printf(TEXT("Contract: %s"), *Name));
			PushReputation();
		}
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

void ADVPlayerController::DVWarp(float X, float Y, float Z, float Yaw, float Pitch)
{
	if (Yaw > -999.f)
	{
		SetControlRotation(FRotator(Pitch, Yaw, 0.f));
	}
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

void ADVPlayerController::DVWalker(int32 Mode)
{
	ServerDVWalker(Mode);
}

void ADVPlayerController::ServerDVWalker_Implementation(int32 Mode)
{
#if !UE_BUILD_SHIPPING
	ADVWalker* Walker = ADVWalker::Find(GetWorld());
	if (!Walker)
	{
		return;
	}
	if (Mode == 0)
	{
		Walker->DevKneel();
	}
	else if (Mode == 1)
	{
		Walker->DevBlowAll();
	}
	else if (Mode == 3)
	{
		// Tests: clear every hostile and silence the cannon, so a boarding run measures the walker, not the firefight.
		for (TActorIterator<ADVSoldierCharacter> It(GetWorld()); It; ++It)
		{
			if (It->TeamId != 1 && !It->IsOutOfAction())
			{
				It->DevKill();
			}
		}
		Walker->DevHoldFire();
	}
	else if (APawn* P = GetPawn())
	{
		P->TeleportTo(Walker->GetActorTransform().TransformPosition(FVector(-1000.f, 0.f, 650.f)), Walker->GetActorRotation());
	}
#endif
}

void ADVPlayerController::DVBoardTest(float Delay, int32 JumpOffOnce)
{
	BoardTestStep = 0;
	bBoardTestJumpOff = JumpOffOnce != 0;
	FTimerHandle Start;
	GetWorldTimerManager().SetTimer(Start, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		BoardTestStart = GetWorld()->GetTimeSeconds();
		bBoardTest = true;	// driven every frame from PlayerTick, like a held key
	}), FMath::Max(Delay, 0.001f), false);
}

void ADVPlayerController::TickBoardTest()
{
	ADVSoldierCharacter* Me = Cast<ADVSoldierCharacter>(GetPawn());
	const ADVWalker* Walker = ADVWalker::Find(GetWorld());
	if (!Me || !Walker || GetWorld()->GetTimeSeconds() - BoardTestStart > 260.0 || Walker->GetState() == EDVWalkerState::Down)
	{
		UE_LOG(LogTemp, Log, TEXT("DVBoardTest: done (step %d, walker state %d, couplings %d)"), BoardTestStep, int32(Walker ? Walker->GetState() : EDVWalkerState::Down),
			Walker ? Walker->GetCouplingsBlown() : -1);
		bBoardTest = false;
		return;
	}
	const bool bAboard = Walker->IsAboard(Me);
	if (BoardTestStep >= 3 && !bAboard && Me->GetActorLocation().Z < Walker->GetActorLocation().Z + 200.f)
	{
		UE_LOG(LogTemp, Log, TEXT("DVBoardTest: fell off the deck, going back to the ramp"));
		BoardTestStep = 0;
	}
	FVector Goal;
	const FTransform Xf = Walker->GetActorTransform();
	switch (BoardTestStep)
	{
	case 0: Goal = Xf.TransformPosition(FVector(-3400.f, 0.f, 0.f)); break;	// behind the ramp
	case 1: Goal = Xf.TransformPosition(FVector(-2800.f, 0.f, 0.f)); break;	// the ramp's bottom edge, on its centreline
	case 2: Goal = Xf.TransformPosition(FVector(-900.f, 0.f, 600.f)); break;	// up onto the deck
	default: Goal = Walker->GetActiveCouplingLocation(); break;
	}
	const FVector To = (Goal - Me->GetActorLocation());
	if (FMath::Fmod(GetWorld()->GetTimeSeconds(), 3.0) < 0.021)
	{
		UE_LOG(LogTemp, Log, TEXT("DVBoardTest: step %d, %.0f cm to go (z %.0f), me %s, walker state %d, speed %.0f"), BoardTestStep, To.Size2D(), To.Z,
			*Me->GetActorLocation().ToCompactString(), int32(Walker->GetState()), Me->GetVelocity().Size2D());
	}
	if (BoardTestStep == 1 && To.Size2D() < 60.f)
	{
		BoardTestStep = 2;
	}
	if (BoardTestStep == 0 && To.Size2D() < 120.f)
	{
		// Wait at the foot of the ramp until it's down, then walk up.
		if (Walker->GetState() == EDVWalkerState::Knelt)
		{
			UE_LOG(LogTemp, Log, TEXT("DVBoardTest: at the ramp, walker knelt: going up"));
			BoardTestStep = 1;
		}
		return;
	}
	if (BoardTestStep == 2 && bAboard)
	{
		UE_LOG(LogTemp, Log, TEXT("DVBoardTest: aboard after %.1f s"), GetWorld()->GetTimeSeconds() - BoardTestStart);
		BoardTestStep = 3;
	}
	if (BoardTestStep == 2 && Walker->GetState() != EDVWalkerState::Knelt && !bAboard)
	{
		BoardTestStep = 0;	// the ramp went up before we made it: try the next kneel
	}
	if (BoardTestStep >= 3 && bBoardTestJumpOff && Walker->GetState() != EDVWalkerState::Walking)
	{
		return;	// ride it up first
	}
	if (BoardTestStep >= 3 && bBoardTestJumpOff && Walker->GetState() == EDVWalkerState::Walking)
	{
		// Fall-recovery test: once it's striding at full height, walk off the stern (the ramp gap) and drop 25 m.
		const FVector Off = Xf.TransformPosition(FVector(-1900.f, 0.f, 600.f)) - Me->GetActorLocation();
		SetControlRotation(FRotator(-5.f, Off.Rotation().Yaw, 0.f));
		Me->AddMovementInput(Off.GetSafeNormal2D(), 1.f);
		if (!bAboard && Me->GetCharacterMovement()->IsFalling())
		{
			UE_LOG(LogTemp, Log, TEXT("DVBoardTest: jumped off at %.0f cm above the ground"), Me->GetActorLocation().Z - Xf.GetLocation().Z + ADVWalker::WalkHeight);
			bBoardTestJumpOff = false;
		}
		return;
	}
	if (BoardTestStep >= 3 && To.Size2D() < 210.f)
	{
		return;	// standing on the coupling: let it overload
	}
	SetControlRotation(FRotator(-5.f, To.Rotation().Yaw, 0.f));
	Me->AddMovementInput(To.GetSafeNormal2D(), 1.f);
}

void ADVPlayerController::DVCasualty(int32 Mode)
{
	ServerCasualty(Mode);
}

void ADVPlayerController::ServerCasualty_Implementation(int32 Mode)
{
#if !UE_BUILD_SHIPPING
	const ADVSoldierCharacter* Me = Cast<ADVSoldierCharacter>(GetPawn());
	if (Mode == 2)
	{
		for (TActorIterator<ADVBulwark> It(GetWorld()); It; ++It)
		{
			if (Me && It->TeamId != Me->TeamId && !It->IsOutOfAction())
			{
				It->DevKill();	// a Bulwark destroyed, for trophy and contract tests
				return;
			}
		}
		return;
	}
	for (TActorIterator<ADVSoldierCharacter> It(GetWorld()); It; ++It)
	{
		if (Me && *It != Me && It->TeamId == Me->TeamId && !It->IsPlayerControlled() && !It->IsOutOfAction() && It->GetRosterId().IsValid()
			&& !It->Tags.Contains(TEXT("DVCasualty")))
		{
			It->Tags.Add(TEXT("DVCasualty"));	// repeated calls pick different soldiers
			if (Mode == 0)
			{
				It->DevKill();
			}
			else if (UDVAttributeSet* Attr = It->GetAttributeSet())
			{
				Attr->DamageTakenFraction = 0.8f;
			}
			return;
		}
	}
#endif
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

void ADVPlayerController::ServerTurnValve_Implementation(uint8 Valve)
{
	ADVPressureManifold* Manifold = ADVPressureManifold::Find(GetWorld());
	const ADVSoldierCharacter* Soldier = Cast<ADVSoldierCharacter>(GetPawn());
	// The server checks the reach itself: a client can only turn the wheel it is standing at.
	if (Manifold && Soldier && !Soldier->IsOutOfAction() && Manifold->FindValveNear(Soldier) == Valve)
	{
		Manifold->TurnValve(Valve);
	}
}

void ADVPlayerController::DVValve(int32 Valve, int32 Times, float Delay)
{
#if !UE_BUILD_SHIPPING
	if (Valve < 0)
	{
		// Solve it the way a player would: walk to each wheel in turn and press Z the right number of times.
		FTimerHandle Plan;
		GetWorldTimerManager().SetTimer(Plan, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			const ADVPressureManifold* Manifold = ADVPressureManifold::Find(GetWorld());
			if (!Manifold)
			{
				return;
			}
			int32 Turns[ADVPressureManifold::NumValves];
			Manifold->GetSolution(Turns);
			UE_LOG(LogTemp, Display, TEXT("DVValve: gauges %d %d %d, solving with A x%d B x%d C x%d"),
				Manifold->GetGauge(0), Manifold->GetGauge(1), Manifold->GetGauge(2), Turns[0], Turns[1], Turns[2]);
			float At = 0.f;
			for (int32 v = 0; v < ADVPressureManifold::NumValves; ++v)
			{
				if (Turns[v] > 0)
				{
					DVValve(v, Turns[v], At);
					At += 1.5f + Turns[v] * 0.6f;
				}
			}
		}), FMath::Max(Delay, 0.001f), false);
		return;
	}
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, Valve, Times]()
	{
		const ADVPressureManifold* Manifold = ADVPressureManifold::Find(GetWorld());
		APawn* Me = GetPawn();
		if (!Manifold || !Me)
		{
			return;
		}
		// Stand at the wheel, facing the panel, then turn it the way Z does.
		const FVector At = Manifold->GetValveLocation(Valve) - Manifold->GetActorForwardVector() * 175.f;
		Me->TeleportTo(FVector(At.X, At.Y, At.Z - 10.f), Manifold->GetActorRotation());
		SetControlRotation(FRotator(-4.f, Manifold->GetActorRotation().Yaw, 0.f));
		for (int32 i = 0; i < Times; ++i)
		{
			FTimerHandle Turn;
			GetWorldTimerManager().SetTimer(Turn, FTimerDelegate::CreateWeakLambda(this, [this]() { OnInteractPressed(); }), 0.4f + i * 0.6f, false);
		}
	}), FMath::Max(Delay, 0.001f), false);
#endif
}

void ADVPlayerController::DVCivilianTest(float Delay)
{
#if !UE_BUILD_SHIPPING
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		APawn* Me = GetPawn();
		ADVTownsfolk* Nearest = nullptr;
		float Best = TNumericLimits<float>::Max();
		for (TActorIterator<ADVTownsfolk> It(GetWorld()); Me && It; ++It)
		{
			const float D = FVector::Dist(It->GetActorLocation(), Me->GetActorLocation());
			if (D < Best)
			{
				Best = D;
				Nearest = *It;
			}
		}
		if (Nearest && HasAuthority())
		{
			UE_LOG(LogTemp, Display, TEXT("DVCivilianTest: firing on %s (%.0f m)"), *Nearest->DisplayName, Best / 100.f);
			Nearest->OnShot(Me);
		}
	}), FMath::Max(Delay, 0.001f), false);
#endif
}

void ADVPlayerController::DVClearHostiles(float Delay, float Spacing)
{
#if !UE_BUILD_SHIPPING
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, Spacing]()
	{
		if (!HasAuthority())
		{
			return;
		}
		int32 n = 0;
		for (TActorIterator<ADVSoldierCharacter> It(GetWorld()); It; ++It)
		{
			if (It->TeamId != 1 && !It->IsDead())
			{
				TWeakObjectPtr<ADVSoldierCharacter> Target = *It;
				FTimerHandle Each;
				GetWorldTimerManager().SetTimer(Each, FTimerDelegate::CreateWeakLambda(this, [Target]() { if (Target.IsValid()) { Target->DevKill(); } }), 0.05f + Spacing * n++, false);
			}
		}
	}), FMath::Max(Delay, 0.001f), false);
#endif
}

void ADVPlayerController::DVHerdTest(float Delay)
{
#if !UE_BUILD_SHIPPING
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this]() { bHerdTest = true; HerdTestStart = GetWorld()->GetTimeSeconds(); }), FMath::Max(Delay, 0.001f), false);
#endif
}

void ADVPlayerController::DVRepairTest(float Delay)
{
#if !UE_BUILD_SHIPPING
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this]() { bRepairTest = true; HerdTestStart = GetWorld()->GetTimeSeconds(); }), FMath::Max(Delay, 0.001f), false);
#endif
}

void ADVPlayerController::TickHerdTest()
{
	APawn* Me = GetPawn();
	if (!Me || GetWorld()->GetTimeSeconds() - HerdTestStart > 400.0)
	{
		bHerdTest = bRepairTest = false;
		return;
	}
	auto Walk = [this, Me](const FVector& Goal, float Arrive)
	{
		const FVector To = Goal - Me->GetActorLocation();
		if (To.Size2D() < Arrive)
		{
			return true;
		}
		FVector Dir = To.GetSafeNormal2D();
		// Like a player would: if something (a villager, a cottage) is in the way, step round it to the right.
		const double Now = GetWorld()->GetTimeSeconds();
		if (FVector::Dist2D(Me->GetActorLocation(), HerdLastPos) > 60.f)
		{
			HerdLastPos = Me->GetActorLocation();
			HerdStuckSince = Now;
		}
		else if (Now - HerdStuckSince > 1.0 && Now - HerdStuckSince < 2.2)
		{
			Dir = FVector::CrossProduct(FVector::UpVector, Dir);
		}
		else if (Now - HerdStuckSince >= 2.2)
		{
			HerdStuckSince = Now;
		}
		SetControlRotation(FRotator(-8.f, Dir.Rotation().Yaw, 0.f));
		Me->AddMovementInput(Dir, 1.f);
		return false;
	};
	if (bRepairTest)
	{
		// Nearest pump that still needs mending; stand beside it until it turns.
		const ADVRepairPoint* Target = nullptr;
		float Best = TNumericLimits<float>::Max();
		for (TActorIterator<ADVRepairPoint> It(GetWorld()); It; ++It)
		{
			const float D = FVector::Dist2D(It->GetActorLocation(), Me->GetActorLocation());
			if (!It->IsFixed() && D < Best)
			{
				Best = D;
				Target = *It;
			}
		}
		if (!Target)
		{
			UE_LOG(LogTemp, Display, TEXT("DVRepairTest: every pump fixed after %.1f s"), GetWorld()->GetTimeSeconds() - HerdTestStart);
			bRepairTest = false;
			return;
		}
		Walk(Target->GetActorLocation() + FVector(-200.f, 0.f, 0.f), 80.f);
		return;
	}
	const ADVPen* Pen = ADVPen::Find(GetWorld());
	const ADVPuffbot* Target = nullptr;
	float Best = TNumericLimits<float>::Max();
	for (TActorIterator<ADVPuffbot> It(GetWorld()); Pen && It; ++It)
	{
		const float D = FVector::Dist2D(It->GetActorLocation(), Pen->GetActorLocation());
		if (!It->IsPenned() && D < Best)
		{
			Best = D;
			Target = *It;
		}
	}
	if (!Target)
	{
		UE_LOG(LogTemp, Display, TEXT("DVHerdTest: every puffbot penned after %.1f s"), GetWorld()->GetTimeSeconds() - HerdTestStart);
		bHerdTest = false;
		return;
	}
	// Get behind it (on the side away from the pen), going wide so we don't spook it the wrong way, then push.
	const FVector Bot = Target->GetActorLocation();
	const FVector ToPen = (Pen->GetActorLocation() - Bot).GetSafeNormal2D();
	const FVector Behind = Bot - ToPen * 420.f;
	const float FromBehind = FVector::Dist2D(Me->GetActorLocation(), Behind);
	const float FromBot = FVector::Dist2D(Me->GetActorLocation(), Bot);
	const bool bOnFarSide = FVector::DotProduct((Me->GetActorLocation() - Bot).GetSafeNormal2D(), -ToPen) > 0.55f;
	if (!bOnFarSide && FromBehind > 200.f)
	{
		// Circle round at a polite distance.
		const FVector Side = FVector::CrossProduct(ToPen, FVector::UpVector);
		const FVector Wide = FromBot < 800.f ? Bot + (Me->GetActorLocation() - Bot).GetSafeNormal2D() * 900.f + Side * 300.f - ToPen * 300.f : Behind;
		Walk(Wide, 60.f);
	}
	else
	{
		Walk(Bot + ToPen * 150.f, 40.f);	// walk on through it toward the pen; it runs ahead
	}
	if (FMath::Fmod(GetWorld()->GetTimeSeconds(), 4.0) < 0.021)
	{
		UE_LOG(LogTemp, Display, TEXT("DVHerdTest: chasing %s, %.0f m from the pen, far side %d"), *Target->GetName(), Best / 100.f, bOnFarSide ? 1 : 0);
	}
}
