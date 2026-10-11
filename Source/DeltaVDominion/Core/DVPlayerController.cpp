#include "Core/DVPlayerController.h"
#include "Characters/DVSoldierCharacter.h"
#include "Characters/DVCommandComponent.h"
#include "Core/DVGameMode.h"
#include "Vehicles/DVVehicleKit.h"
#include "Vehicles/DVGroundVehicle.h"
#include "Vehicles/DVGunship.h"
#include "Core/DVGameState.h"

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
	InputComponent->BindKey(EKeys::M, IE_Pressed, this, &ADVPlayerController::OnMapPressed);
	InputComponent->BindKey(EKeys::Z, IE_Pressed, this, &ADVPlayerController::OnInteractPressed);
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
		UGameplayStatics::OpenLevel(this, FName(TEXT("/Game/Maps/Galaxy")));	// back to the Kestrel's strategic map
	}
}

void ADVPlayerController::OnInteractPressed()
{
	APawn* MyPawn = GetPawn();
	if (!MyPawn)
	{
		return;
	}
	if (MyPawn->FindComponentByClass<UDVSeatComponent>())
	{
		ServerExitVehicle();
	}
	else if (UDVSeatComponent* Seat = UDVSeatComponent::FindNear(Cast<ADVSoldierCharacter>(MyPawn)))
	{
		ServerEnterVehicle(Cast<APawn>(Seat->GetOwner()));
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
				|| (Which.Equals(TEXT("gunship"), ESearchCase::IgnoreCase) && It->IsA<ADVGunship>());
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
