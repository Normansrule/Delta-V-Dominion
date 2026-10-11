#include "Strategy/DVGalaxy.h"
#include "Strategy/DVStarSystem.h"

#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "EngineUtils.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"

ADVGalaxyGameMode::ADVGalaxyGameMode()
{
	DefaultPawnClass = ADVGalaxyPawn::StaticClass();
	PlayerControllerClass = ADVGalaxyPlayerController::StaticClass();
	HUDClass = ADVGalaxyHUD::StaticClass();
}

ADVGalaxyPawn::ADVGalaxyPawn()
{
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	RootComponent = Camera;
	Camera->bUsePawnControlRotation = true;
	Camera->SetFieldOfView(55.f);
}

// ---------------------------------------------------------------------------

ADVGalaxyPlayerController::ADVGalaxyPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = false;
}

void ADVGalaxyPlayerController::BeginPlay()
{
	Super::BeginPlay();
	for (TActorIterator<ADVStarSystem> It(GetWorld()); It; ++It)
	{
		Systems.Add(*It);
	}
	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
	DVGalaxyView(0);
	Focus = TargetFocus;
	Distance = TargetDistance * 1.6f;	// open with a slow push-in
}

void ADVGalaxyPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &ADVGalaxyPlayerController::OnScrollUp);
	InputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &ADVGalaxyPlayerController::OnScrollDown);
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ADVGalaxyPlayerController::OnLeftPressed);
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &ADVGalaxyPlayerController::OnLeftReleased);
	InputComponent->BindKey(EKeys::H, IE_Pressed, this, &ADVGalaxyPlayerController::OnHome);
	InputComponent->BindKey(EKeys::G, IE_Pressed, this, &ADVGalaxyPlayerController::OnOverview);
	InputComponent->BindKey(EKeys::One, IE_Pressed, this, &ADVGalaxyPlayerController::Launch1);
	InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &ADVGalaxyPlayerController::Launch2);
	InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ADVGalaxyPlayerController::Launch3);
}

void ADVGalaxyPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	// Mouse: left-drag orbits, right-drag pans. Keyboard: WASD pans, Q/E zoom.
	float MouseX = 0.f, MouseY = 0.f;
	GetInputMouseDelta(MouseX, MouseY);
	if (bLeftDown)
	{
		DragAmount += FMath::Abs(MouseX) + FMath::Abs(MouseY);
		Yaw += MouseX * 2.2f;
		Pitch = FMath::Clamp(Pitch + MouseY * 2.2f, -88.f, -5.f);
		bAutoSpin = false;
	}
	const FRotator View(Pitch, Yaw, 0.f);
	const FVector Right = FRotationMatrix(View).GetUnitAxis(EAxis::Y);
	const FVector Forward = FRotator(0.f, Yaw, 0.f).Vector();
	const float PanSpeed = Distance * 0.9f;
	FVector Pan = FVector::ZeroVector;
	if (IsInputKeyDown(EKeys::RightMouseButton))
	{
		Pan += (-Right * MouseX - Forward * MouseY) * Distance * 0.004f;
		bAutoSpin = false;
	}
	Pan += Forward * ((IsInputKeyDown(EKeys::W) ? 1.f : 0.f) - (IsInputKeyDown(EKeys::S) ? 1.f : 0.f)) * PanSpeed * DeltaTime;
	Pan += Right * ((IsInputKeyDown(EKeys::D) ? 1.f : 0.f) - (IsInputKeyDown(EKeys::A) ? 1.f : 0.f)) * PanSpeed * DeltaTime;
	if (!Pan.IsNearlyZero())
	{
		TargetFocus += Pan;
		bAutoSpin = false;
	}
	if (IsInputKeyDown(EKeys::Q)) { Zoom(1.f + 1.5f * DeltaTime); }
	if (IsInputKeyDown(EKeys::E)) { Zoom(1.f - 0.6f * DeltaTime); }
	if (bAutoSpin)
	{
		Yaw += DeltaTime * 2.5f;	// idle drift so the galaxy feels alive on the title screen
	}

	// Ease toward targets (log-space for distance so zooms feel uniform at every scale).
	const float Alpha = 1.f - FMath::Exp(-DeltaTime * 4.f);
	Focus = FMath::Lerp(Focus, TargetFocus, Alpha);
	Distance = FMath::Exp(FMath::Lerp(FMath::Loge(Distance), FMath::Loge(TargetDistance), Alpha));

	const FRotator Rot(Pitch, Yaw, 0.f);
	SetControlRotation(Rot);
	if (APawn* P = GetPawn())
	{
		P->SetActorLocation(Focus - Rot.Vector() * Distance);
	}
	Hovered = PickUnderCursor();

	// Markers keep a readable size at any zoom (same rule as the star material's world position offset).
	const FVector CameraPos = Focus - Rot.Vector() * Distance;
	for (const TWeakObjectPtr<ADVStarSystem>& Weak : Systems)
	{
		if (ADVStarSystem* System = Weak.Get())
		{
			const float Factor = FMath::Clamp(FVector::Dist(CameraPos, System->GetActorLocation()) / 40000.f, 0.02f, 1.f);
			System->SetActorScale3D(FVector(FMath::Max(0.1f, System->MarkerSize) * Factor));
		}
	}
}

void ADVGalaxyPlayerController::Zoom(float Factor)
{
	TargetDistance = FMath::Clamp(TargetDistance * Factor, 400.f, 220000.f);
}

void ADVGalaxyPlayerController::OnLeftPressed()
{
	bLeftDown = true;
	DragAmount = 0.f;
}

void ADVGalaxyPlayerController::OnLeftReleased()
{
	bLeftDown = false;
	if (DragAmount < 0.02f)
	{
		if (ADVStarSystem* Picked = PickUnderCursor())
		{
			Select(Picked, true);
		}
	}
}

ADVStarSystem* ADVGalaxyPlayerController::PickUnderCursor() const
{
	float MX = 0.f, MY = 0.f;
	if (!GetMousePosition(MX, MY))
	{
		return nullptr;
	}
	ADVStarSystem* Best = nullptr;
	float BestDist = 28.f;	// pixels
	for (const TWeakObjectPtr<ADVStarSystem>& Weak : Systems)
	{
		ADVStarSystem* System = Weak.Get();
		FVector2D Screen;
		if (System && !System->Region.Equals(TEXT("label")) && ProjectWorldLocationToScreen(System->GetActorLocation(), Screen))
		{
			const float D = FVector2D::Distance(Screen, FVector2D(MX, MY));
			if (D < BestDist)
			{
				BestDist = D;
				Best = System;
			}
		}
	}
	return Best;
}

void ADVGalaxyPlayerController::Select(ADVStarSystem* System, bool bFly)
{
	Selected = System;
	if (System && bFly)
	{
		TargetFocus = System->GetActorLocation();
		TargetDistance = System->DistanceLy < 100.f ? 2600.f : 9000.f;
		bAutoSpin = false;
	}
}

void ADVGalaxyPlayerController::DVSelectSystem(const FString& Name)
{
	for (const TWeakObjectPtr<ADVStarSystem>& Weak : Systems)
	{
		if (Weak.IsValid() && Weak->SystemName.Contains(Name))
		{
			Select(Weak.Get(), true);
			return;
		}
	}
}

void ADVGalaxyPlayerController::DVGalaxyView(int32 Preset)
{
	ADVStarSystem* Sol = nullptr;
	for (const TWeakObjectPtr<ADVStarSystem>& Weak : Systems)
	{
		if (Weak.IsValid() && Weak->bHomeSystem)
		{
			Sol = Weak.Get();
		}
	}
	switch (Preset)
	{
	case 1:		// Sol and Gaia, with the local bubble around them
		TargetFocus = Sol ? Sol->GetActorLocation() : FVector::ZeroVector;
		TargetDistance = 3200.f;
		Pitch = -24.f;
		Select(Sol, false);
		break;
	case 2:		// Galactic core
		TargetFocus = FVector::ZeroVector;
		TargetDistance = 26000.f;
		Pitch = -18.f;
		break;
	case 3:		// Along the Orion arm toward the core
		TargetFocus = Sol ? Sol->GetActorLocation() * 0.7f : FVector::ZeroVector;
		TargetDistance = 30000.f;
		Pitch = -12.f;
		break;
	default:	// Whole galaxy
		TargetFocus = FVector::ZeroVector;
		TargetDistance = 120000.f;
		Pitch = -42.f;
		break;
	}
	Focus = TargetFocus;	// snap focus; distance still eases
}

void ADVGalaxyPlayerController::LaunchOperation(int32 Index)
{
	const ADVStarSystem* System = Selected.Get();
	if (!System || !System->Operations.IsValidIndex(Index) || System->Operations[Index].Map.IsEmpty())
	{
		return;
	}
	UGameplayStatics::OpenLevel(this, FName(*System->Operations[Index].Map));
}

// ---------------------------------------------------------------------------

void ADVGalaxyHUD::Text(const FString& Str, float X, float Y, const FLinearColor& Color, UFont* Font, float Scale, bool bCentre, bool bRight)
{
	Scale *= 1.3f;
	float W = 0.f, H = 0.f;
	GetTextSize(Str, W, H, Font, Scale * UIScale);
	const float DrawX = bCentre ? X - W * 0.5f : bRight ? X - W : X;
	DrawText(Str, FLinearColor(0.f, 0.f, 0.f, 0.7f * Color.A), DrawX + S(1.5f), Y + S(1.5f), Font, Scale * UIScale);
	DrawText(Str, Color, DrawX, Y, Font, Scale * UIScale);
}

void ADVGalaxyHUD::DrawHUD()
{
	Super::DrawHUD();
	const ADVGalaxyPlayerController* PC = Cast<ADVGalaxyPlayerController>(PlayerOwner);
	if (!Canvas || !PC)
	{
		return;
	}
	UIScale = Canvas->ClipY / 1080.f;
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	UFont* Small = GEngine->GetSmallFont();
	UFont* Medium = GEngine->GetMediumFont();
	UFont* Large = GEngine->GetLargeFont();

	// Labels: regions are large and faint; systems show their name when near enough or selected.
	for (const TWeakObjectPtr<ADVStarSystem>& Weak : PC->GetSystems())
	{
		const ADVStarSystem* System = Weak.Get();
		if (!System)
		{
			continue;
		}
		const FVector Screen = Project(System->GetActorLocation());
		if (Screen.Z <= 0.f || Screen.X < 0.f || Screen.Y < 0.f || Screen.X > W || Screen.Y > H)
		{
			continue;
		}
		if (System->Region.Equals(TEXT("label")))
		{
			Text(System->SystemName.ToUpper(), Screen.X, Screen.Y, FLinearColor(0.75f, 0.82f, 1.f, 0.35f), Medium, 1.1f, true);
			continue;
		}
		const bool bFocus = System == PC->GetSelected() || System == PC->GetHovered();
		const bool bLocal = System->DistanceLy < 100.f;
		if (bLocal && PC->GetCameraDistance() > 20000.f && !bFocus && !System->bHomeSystem)
		{
			continue;	// local stars crowd Sol from far away
		}
		const FLinearColor C = System->GetControlColor();
		DrawRect(FLinearColor(C.R, C.G, C.B, bFocus ? 1.f : 0.7f), Screen.X - S(3.f), Screen.Y - S(3.f), S(6.f), S(6.f));
		Text(System->SystemName, Screen.X + S(10.f), Screen.Y - S(10.f), FLinearColor(C.R, C.G, C.B, bFocus ? 1.f : 0.75f), Small, bFocus ? 1.2f : 0.95f);
		if (System->Operations.Num() > 0)
		{
			Text(TEXT("OPERATION AVAILABLE"), Screen.X + S(10.f), Screen.Y + S(12.f), FLinearColor(1.f, 0.6f, 0.2f, 0.9f), Small, 0.8f);
		}
	}

	// Title block.
	Text(TEXT("DELTA-V: DOMINION"), S(40.f), S(34.f), FLinearColor::White, Large, 1.5f);
	Text(TEXT("TAS KESTREL  -  STRATEGIC MAP OF THE MILKY WAY"), S(40.f), S(92.f), FLinearColor(0.6f, 0.75f, 0.95f, 1.f), Small, 1.1f);
	Text(TEXT("2189  |  Day 3 of the CINDER uprising  |  Home world: Gaia (Sol III)"), S(40.f), S(116.f), FLinearColor(0.7f, 0.7f, 0.75f, 1.f), Small, 1.f);

	// Selected system panel.
	if (const ADVStarSystem* System = PC->GetSelected())
	{
		const float PX = W - S(560.f);
		float Y = S(40.f);
		const int32 Lines = 5 + System->Operations.Num() * 2;
		DrawRect(FLinearColor(0.01f, 0.02f, 0.04f, 0.7f), PX - S(20.f), Y - S(16.f), S(540.f), S(150.f) + Lines * S(26.f));
		Text(System->SystemName.ToUpper(), PX, Y, FLinearColor::White, Large, 1.1f);
		Y += S(50.f);
		Text(FString::Printf(TEXT("%s  |  %s"), *System->GetControlName(), *System->Region), PX, Y, System->GetControlColor(), Small, 1.05f);
		Y += S(26.f);
		Text(System->DistanceLy <= 0.f ? FString(TEXT("Home system")) : FString::Printf(TEXT("%s light-years from Sol"), *FText::AsNumber(FMath::RoundToInt(System->DistanceLy)).ToString()),
			PX, Y, FLinearColor(0.7f, 0.75f, 0.8f, 1.f), Small, 1.f);
		Y += S(34.f);
		// Word-wrap the description by hand (canvas text doesn't wrap).
		TArray<FString> Words;
		System->Description.ParseIntoArray(Words, TEXT(" "));
		FString Line;
		for (const FString& Word : Words)
		{
			float TW = 0.f, TH = 0.f;
			GetTextSize(Line + TEXT(" ") + Word, TW, TH, Small, 1.3f * UIScale);
			if (TW > S(500.f))
			{
				Text(Line, PX, Y, FLinearColor(0.9f, 0.9f, 0.92f, 1.f), Small, 1.f);
				Y += S(24.f);
				Line = Word;
			}
			else
			{
				Line = Line.IsEmpty() ? Word : Line + TEXT(" ") + Word;
			}
		}
		Text(Line, PX, Y, FLinearColor(0.9f, 0.9f, 0.92f, 1.f), Small, 1.f);
		Y += S(40.f);
		for (int32 i = 0; i < System->Operations.Num(); ++i)
		{
			const FDVOperation& Op = System->Operations[i];
			const bool bPlayable = !Op.Map.IsEmpty();
			Text(FString::Printf(TEXT("[%d]  OPERATION %s"), i + 1, *Op.Name.ToUpper()), PX, Y, bPlayable ? FLinearColor(1.f, 0.6f, 0.2f, 1.f) : FLinearColor(0.5f, 0.5f, 0.55f, 1.f), Medium, 1.f);
			Y += S(30.f);
			Text(bPlayable ? Op.Location : Op.Location + TEXT("  (coming soon)"), PX + S(44.f), Y, FLinearColor(0.75f, 0.75f, 0.8f, 1.f), Small, 1.f);
			Y += S(30.f);
		}
	}

	Text(TEXT("Left-drag orbit   Right-drag / WASD pan   Wheel / Q E zoom   Click select   1-3 launch operation   H home (Sol)   G whole galaxy"),
		W * 0.5f, H - S(46.f), FLinearColor(0.7f, 0.75f, 0.85f, 0.85f), Small, 1.f, true);
}
