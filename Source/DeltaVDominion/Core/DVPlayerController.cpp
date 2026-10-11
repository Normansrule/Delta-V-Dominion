#include "Core/DVPlayerController.h"
#include "Characters/DVSoldierCharacter.h"
#include "Characters/DVCommandComponent.h"
#include "Core/DVGameMode.h"
#include "Core/DVGameState.h"

#include "Camera/CameraActor.h"
#include "Components/InputComponent.h"
#include "InputCoreTypes.h"
#include "HighResScreenshot.h"
#include "Kismet/KismetSystemLibrary.h"
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
