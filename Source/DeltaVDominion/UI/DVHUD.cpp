#include "UI/DVHUD.h"
#include "Characters/DVSoldierCharacter.h"
#include "Characters/DVCommandComponent.h"
#include "Combat/DVAttributeSet.h"
#include "Combat/DVWeaponBase.h"
#include "Combat/DVWeaponData.h"
#include "Core/DVGameState.h"
#include "Core/DVPlayerController.h"
#include "Vehicles/DVVehicleKit.h"
#include "Core/DVInteractable.h"
#include "Characters/DVTownsfolk.h"
#include "Core/DVProfileSubsystem.h"
#include "Characters/DVBulwark.h"
#include "Core/DVCallIns.h"
#include "Combat/DVLoadouts.h"
#include "Core/DVRosterSubsystem.h"
#include "Engine/GameInstance.h"
#include "Vehicles/DVGroundVehicle.h"
#include "Vehicles/DVGunship.h"
#include "Vehicles/DVHoverbike.h"
#include "Vehicles/DVBoat.h"
#include "Vehicles/DVKestrel.h"
#include "Vehicles/DVWalker.h"
#include "Core/DVPressureManifold.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "EngineUtils.h"

namespace DVType
{
	// The HUD's type scale (multipliers on the engine's small/medium/large faces; Text() adds the 1080p factor).
	constexpr float Caption = 1.f;	// small: labels, hints, secondary lines
	constexpr float Body = 1.15f;	// small: names, list rows, readable status
	constexpr float Emph = 1.05f;	// medium: radio, prompts, toasts
	constexpr float Title = 1.15f;	// large: panel titles
	constexpr float Display = 1.7f;	// large: banners (level up, mission accomplished)
	constexpr float Hero = 2.4f;	// large: the mission title card
}

namespace DVHudColors
{
	// One ink, one dim ink, two panel weights, and a small set of meaning colours. Nothing else is drawn.
	const FLinearColor Text(0.94f, 0.95f, 0.97f, 1.f);	// ink
	const FLinearColor Dim(0.74f, 0.78f, 0.83f, 1.f);	// secondary ink (was 0.6: under 4.5:1 on the panel)
	const FLinearColor Faded(0.55f, 0.57f, 0.6f, 1.f);	// the dead, the unaware
	// Flight Deck: gunmetal panels, deck-crew amber for the thing that matters, brass for the names of the fallen.
	const FLinearColor Panel(0.006f, 0.008f, 0.009f, 0.74f);	// gunmetal #181b1e
	const FLinearColor PanelStrong(0.005f, 0.006f, 0.007f, 0.9f);
	const FLinearColor Shade(0.f, 0.f, 0.f, 1.f);	// letterbox, scrims (always with an opacity)
	const FLinearColor Friendly(0.38f, 0.64f, 1.f, 1.f);
	const FLinearColor Cyan(0.45f, 0.82f, 1.f, 1.f);	// the Kestrel: radio, extraction, flight, armour
	const FLinearColor Hostile(1.f, 0.32f, 0.24f, 1.f);
	const FLinearColor Wound(0.32f, 0.f, 0.f, 1.f);	// full-screen damage tints
	const FLinearColor Health(0.4f, 0.92f, 0.5f, 1.f);	// health, success, the armoury
	const FLinearColor Armor = Cyan;
	const FLinearColor Accent(0.888f, 0.434f, 0.012f, 1.f);	// deck amber #f2b01e: the job, painted deck bands
	const FLinearColor DeckInk(0.008f, 0.007f, 0.004f, 1.f);	// text on a deck band
	const FLinearColor Brass(0.63f, 0.41f, 0.17f, 1.f);	// roll call: the fallen
	const FLinearColor Plate(0.023f, 0.019f, 0.014f, 0.94f);	// a name plate
	const FLinearColor Gold(1.f, 0.8f, 0.36f, 1.f);	// progression: XP, levels, standing
	const FLinearColor Violet(0.85f, 0.62f, 1.f, 1.f);	// the barracks locker
	const FLinearColor Warn = Accent;
	const FLinearColor Track(1.f, 1.f, 1.f, 0.14f);

	inline FLinearColor A(const FLinearColor& C, float Alpha) { return C.CopyWithNewOpacity(Alpha); }
}

namespace DVMotion
{
	// Toasts and banners: in over 180 ms with a strong ease-out and an 8 px rise, out over 150 ms (exit faster
	// than entry). Prompts that appear many times a minute (board, use, revive) are drawn instantly instead.
	constexpr float In = 0.18f;
	constexpr float Out = 0.15f;
	inline float EaseOut(float T) { T = FMath::Clamp(T, 0.f, 1.f); return 1.f - FMath::Pow(1.f - T, 3.f); }

	/** Alpha and upward offset (in 1080p pixels, scale with S()) for something shown at Age seconds, held Hold seconds. */
	inline void Reveal(double Age, double Hold, float& OutAlpha, float& OutRise)
	{
		const float Enter = EaseOut(float(Age) / In);
		const float Exit = Age > Hold ? 1.f - FMath::Clamp(float(Age - Hold) / Out, 0.f, 1.f) : 1.f;
		OutAlpha = Enter * Exit;
		OutRise = 8.f * (1.f - Enter);
	}
}

static FString OrderName(EDVSquadOrder Order)
{
	switch (Order)
	{
	case EDVSquadOrder::Follow:   return TEXT("Follow me");
	case EDVSquadOrder::MoveTo:   return TEXT("Move to position");
	case EDVSquadOrder::Hold:     return TEXT("Hold position");
	case EDVSquadOrder::Flank:    return TEXT("Flank target");
	case EDVSquadOrder::Suppress: return TEXT("Suppressing fire");
	case EDVSquadOrder::Regroup:  return TEXT("Regroup on me");
	default:                      return TEXT("");
	}
}

void ADVHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas)
	{
		return;
	}
	UIScale = Canvas->ClipY / 1080.f;
	bBoardOpen = false;

	const ADVGameState* GS = GetWorld()->GetGameState<ADVGameState>();
	ADVSoldierCharacter* Soldier = Cast<ADVSoldierCharacter>(GetOwningPawn());
	const bool bViewingSoldier = Soldier && PlayerOwner && PlayerOwner->GetViewTarget() == Soldier;

	if (GS && GS->Phase != EDVMissionPhase::InProgress && GS->Ascent == 0)
	{
		DrawAfterAction(GS);
		DrawProgression();
		return;
	}
	if (const ADVPlayerController* LockerPC = Cast<ADVPlayerController>(PlayerOwner); LockerPC && LockerPC->IsLockerOpen())
	{
		// Mirror view at the barracks locker: just the customisation and service record.
		DrawLocker();
		DrawProgression();
		return;
	}

	APawn* OwningPawn = GetOwningPawn();
	if (OwningPawn && OwningPawn->FindComponentByClass<UDVSeatComponent>())
	{
		if (GS && GS->Ascent == 1)
		{
			DrawAscent(GS);	// cinematic: no gunship instruments, just the ride home
			return;
		}
		DrawVehicle(OwningPawn);
		DrawSquad(nullptr);
		DrawObjective(GS);
		DrawKillFeed(GS);
		DrawOrbital(GS);
		DrawRadio(GS);
		DrawTitleCard(GS);
		return;
	}

	if (bViewingSoldier && Soldier->IsDowned())
	{
		DrawDowned(Soldier);
		DrawCasualties(Soldier);
		DrawSquad(Soldier);
		DrawObjective(GS);
		DrawRadio(GS);
		return;
	}
	if (bViewingSoldier && !Soldier->IsDead())
	{
		DrawCasualties(Soldier);
		DrawAwareness(Soldier);
		DrawVehiclePrompt(Soldier);
		BindOrders(Soldier);
		DrawSuppression(Soldier);
		DrawCrosshair(Soldier);
		DrawHitMarker();
		DrawVitals(Soldier);
		DrawThrusters(Soldier);
		DrawAmmo(Soldier);
		DrawOrderToast();
		DrawContract();
	}
	else if (!Soldier || Soldier->IsDead())
	{
		DrawDeathScreen();
	}

	DrawSquad(Soldier);
	DrawObjective(GS);
	DrawKillFeed(GS);
	DrawOrbital(GS);
	DrawRadio(GS);
	DrawTitleCard(GS);
}

// ---------------------------------------------------------------------------

void ADVHUD::Text(const FString& Str, float X, float Y, const FLinearColor& Color, UFont* Font, float Scale, bool bCentre, bool bRight, bool bShadow)
{
	Scale *= 1.3f;	// engine fonts are small at 1080p
	float W = 0.f, H = 0.f;
	GetTextSize(Str, W, H, Font, Scale * UIScale);
	const float DrawX = bCentre ? X - W * 0.5f : bRight ? X - W : X;
	// Drop shadow for legibility over bright sky (not on painted bands, where it only muddies the ink).
	if (bShadow)
	{
		DrawText(Str, DVHudColors::A(DVHudColors::Shade, 0.6f * Color.A), DrawX + S(1.5f), Y + S(1.5f), Font, Scale * UIScale);
	}
	DrawText(Str, Color, DrawX, Y, Font, Scale * UIScale);
}

void ADVHUD::DeckBand(float X, float Y, float W, float H, const FLinearColor& Fill)
{
	// A painted deck marking: a flat, square fill. Text on it uses DeckInk.
	DrawRect(Fill, X, Y, W, H);
}

void ADVHUD::Bar(float X, float Y, float W, float H, float Fraction, const FLinearColor& Fill)
{
	DrawRect(DVHudColors::Track, X, Y, W, H);
	DrawRect(Fill, X, Y, W * FMath::Clamp(Fraction, 0.f, 1.f), H);
}

void ADVHUD::BindOrders(ADVSoldierCharacter* Soldier)
{
	UDVCommandComponent* Command = Soldier->GetCommandComponent();
	if (Command && BoundCommand.Get() != Command)
	{
		Command->OnOrderIssued.AddUniqueDynamic(this, &ADVHUD::OnOrderIssued);
		BoundCommand = Command;
	}
}

void ADVHUD::OnOrderIssued(EDVSquadOrder Order, FVector Location)
{
	OrderToast = OrderName(Order);
	OrderToastTime = GetWorld()->GetTimeSeconds();
}

// ---------------------------------------------------------------------------

void ADVHUD::DrawCrosshair(const ADVSoldierCharacter* Soldier)
{
	const ADVWeaponBase* Weapon = Soldier->GetCurrentWeapon();
	const float CX = Canvas->ClipX * 0.5f;
	const float CY = Canvas->ClipY * 0.5f;

	// Gap = the real cone half-angle projected onto the screen, so the crosshair never lies about accuracy.
	float Gap = S(10.f);
	if (Weapon && PlayerOwner && PlayerOwner->PlayerCameraManager)
	{
		const float Spread = FMath::DegreesToRadians(Weapon->GetCurrentSpread(Soldier->IsAiming(), Soldier->GetSuppression() / 100.f));
		const float HalfFov = FMath::DegreesToRadians(PlayerOwner->PlayerCameraManager->GetFOVAngle() * 0.5f);
		Gap = FMath::Max(S(4.f), FMath::Tan(Spread) / FMath::Tan(HalfFov) * CX);
	}

	const FLinearColor Color = Soldier->IsAiming() ? DVHudColors::A(DVHudColors::Text, 0.55f) : DVHudColors::A(DVHudColors::Text, 0.9f);
	const float Len = S(9.f);
	const float Thick = FMath::Max(1.f, S(2.f));
	DrawLine(CX - Gap - Len, CY, CX - Gap, CY, Color, Thick);
	DrawLine(CX + Gap, CY, CX + Gap + Len, CY, Color, Thick);
	DrawLine(CX, CY - Gap - Len, CX, CY - Gap, Color, Thick);
	DrawLine(CX, CY + Gap, CX, CY + Gap + Len, Color, Thick);
	DrawRect(Color, CX - Thick * 0.5f, CY - Thick * 0.5f, Thick, Thick);
}

void ADVHUD::DrawHitMarker()
{
	const ADVPlayerController* PC = Cast<ADVPlayerController>(PlayerOwner);
	if (!PC)
	{
		return;
	}
	const double Age = GetWorld()->GetTimeSeconds() - PC->GetLastHitTime();
	const double Duration = PC->WasLastHitKill() ? 0.45 : 0.2;
	if (Age > Duration)
	{
		return;
	}

	const float Alpha = 1.f - static_cast<float>(Age / Duration);
	const FLinearColor Color = PC->WasLastHitKill() ? DVHudColors::A(DVHudColors::Hostile, Alpha)
		: PC->WasLastHitHeadshot() ? DVHudColors::A(DVHudColors::Gold, Alpha) : DVHudColors::A(DVHudColors::Text, Alpha);
	const float CX = Canvas->ClipX * 0.5f;
	const float CY = Canvas->ClipY * 0.5f;
	const float In = S(7.f);
	const float Out = S(PC->WasLastHitKill() ? 18.f : 14.f);
	const float Thick = FMath::Max(1.f, S(2.5f));
	for (const FVector2D Dir : { FVector2D(1, 1), FVector2D(-1, 1), FVector2D(1, -1), FVector2D(-1, -1) })
	{
		DrawLine(CX + Dir.X * In, CY + Dir.Y * In, CX + Dir.X * Out, CY + Dir.Y * Out, Color, Thick);
	}
}

void ADVHUD::DrawVitals(const ADVSoldierCharacter* Soldier)
{
	const UDVAttributeSet* Attr = Soldier->GetAttributeSet();
	if (!Attr)
	{
		return;
	}
	const float X = S(40.f);
	const float Y = Canvas->ClipY - S(118.f);
	DrawRect(DVHudColors::Panel, X - S(14.f), Y - S(14.f), S(360.f), S(92.f));

	const float Health = Attr->GetHealth();
	const float MaxHealth = FMath::Max(1.f, Attr->GetMaxHealth());
	const FLinearColor HealthColor = Health / MaxHealth < 0.3f ? DVHudColors::Hostile : DVHudColors::Health;
	Text(FString::Printf(TEXT("%d"), FMath::CeilToInt(Health)), X, Y - S(4.f), HealthColor, GEngine->GetLargeFont(), DVType::Display);
	Text(TEXT("Health"), X + S(70.f), Y + S(4.f), DVHudColors::Dim, GEngine->GetSmallFont(), DVType::Body);
	Bar(X + S(70.f), Y + S(26.f), S(250.f), S(8.f), Health / MaxHealth, HealthColor);

	Text(TEXT("Armour"), X + S(70.f), Y + S(40.f), DVHudColors::Dim, GEngine->GetSmallFont(), DVType::Body);
	Bar(X + S(70.f), Y + S(60.f), S(250.f), S(5.f), Attr->GetArmor() / 100.f, DVHudColors::Armor);
}

void ADVHUD::DrawAmmo(const ADVSoldierCharacter* Soldier)
{
	const ADVWeaponBase* Weapon = Soldier->GetCurrentWeapon();
	if (!Weapon || !Weapon->GetData())
	{
		return;
	}
	const UDVWeaponData* Data = Weapon->GetData();
	const float Right = Canvas->ClipX - S(40.f);
	const float Y = Canvas->ClipY - S(118.f);
	DrawRect(DVHudColors::Panel, Right - S(300.f), Y - S(14.f), S(314.f), S(92.f));

	const FString WeaponName = Data->DisplayName.IsEmpty() ? TEXT("M9 BASTION") : Data->DisplayName.ToString().ToUpper();
	Text(WeaponName, Right, Y + S(44.f), DVHudColors::Dim, GEngine->GetSmallFont(), DVType::Body, false, true);

	if (Data->bUsesHeat)
	{
		const float Heat = FMath::Clamp(Weapon->GetHeat(), 0.f, 1.f);	// 0..1
		const FLinearColor Color = Weapon->IsOverheated() ? DVHudColors::Hostile : Heat > 0.7f ? DVHudColors::Warn : DVHudColors::Text;
		Text(Weapon->IsOverheated() ? TEXT("OVERHEATED") : FString::Printf(TEXT("HEAT %d%%"), FMath::RoundToInt(Heat * 100.f)),
			Right, Y - S(2.f), Color, GEngine->GetLargeFont(), DVType::Title, false, true);
		Bar(Right - S(250.f), Y + S(36.f), S(250.f), S(5.f), Heat, Color);
		return;
	}

	const int32 Mag = Weapon->GetAmmoInMag();
	const bool bLow = Mag <= Data->MagazineSize / 4;
	const FString Reserve = FString::Printf(TEXT(" / %d"), Weapon->GetReserveAmmo());
	float ReserveW = 0.f, ReserveH = 0.f;
	GetTextSize(Reserve, ReserveW, ReserveH, GEngine->GetMediumFont(), DVType::Emph * 1.3f * UIScale);
	Text(Reserve, Right, Y + S(10.f), DVHudColors::Dim, GEngine->GetMediumFont(), DVType::Emph, false, true);
	Text(FString::FromInt(Mag), Right - ReserveW, Y - S(6.f), bLow ? DVHudColors::Warn : DVHudColors::Text, GEngine->GetLargeFont(), DVType::Display, false, true);
	if (Weapon->NeedsReload())
	{
		Text(Weapon->CanReload() ? TEXT("RELOAD [R]") : TEXT("NO AMMO"), Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f + S(48.f),
			DVHudColors::Warn, GEngine->GetMediumFont(), DVType::Emph, true);
	}
}

void ADVHUD::DrawSuppression(const ADVSoldierCharacter* Soldier)
{
	const float Amount = FMath::Clamp(Soldier->GetSuppression() / 100.f, 0.f, 1.f);
	if (Amount <= 0.02f)
	{
		return;
	}
	// Darkened, blood-tinted edges that close in as rounds keep snapping past.
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const int32 Rings = 6;
	for (int32 i = 0; i < Rings; ++i)
	{
		const float T = static_cast<float>(i) / Rings;
		const float Inset = W * 0.08f * Amount * (1.f - T);
		const FLinearColor Color = DVHudColors::A(DVHudColors::Wound, 0.12f * Amount);
		DrawRect(Color, 0.f, 0.f, W, Inset * 0.6f);
		DrawRect(Color, 0.f, H - Inset * 0.6f, W, Inset * 0.6f);
		DrawRect(Color, 0.f, 0.f, Inset, H);
		DrawRect(Color, W - Inset, 0.f, Inset, H);
	}
	if (Amount > 0.6f)
	{
		Text(TEXT("PINNED"), Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f - S(70.f), DVHudColors::Hostile, GEngine->GetMediumFont(), DVType::Emph, true);
	}
}

void ADVHUD::DrawSquad(const ADVSoldierCharacter* Soldier)
{
	const uint8 Team = Soldier ? Soldier->TeamId : 1;
	TArray<const ADVSoldierCharacter*> Squad;
	for (TActorIterator<ADVSoldierCharacter> It(GetWorld()); It; ++It)
	{
		if (It->TeamId == Team && *It != Soldier && !It->GetSoldierName().IsEmpty())
		{
			Squad.Add(*It);
		}
	}
	if (Squad.Num() == 0)
	{
		return;
	}

	const float X = S(40.f);
	float Y = S(40.f);
	DrawRect(DVHudColors::Panel, X - S(14.f), Y - S(12.f), S(440.f), S(40.f) + Squad.Num() * S(52.f));
	Text(TEXT("Squad"), X, Y, DVHudColors::Dim, GEngine->GetSmallFont(), DVType::Body);
	Y += S(30.f);

	for (const ADVSoldierCharacter* Mate : Squad)
	{
		const bool bDead = Mate->IsDead();
		const UDVAttributeSet* Attr = Mate->GetAttributeSet();
		const float Health = Attr ? Attr->GetHealth() / FMath::Max(1.f, Attr->GetMaxHealth()) : 0.f;
		Text(Mate->GetSoldierName(), X, Y, bDead ? DVHudColors::A(DVHudColors::Faded, 0.8f) : DVHudColors::Friendly, GEngine->GetSmallFont(), DVType::Body);
		if (bDead)
		{
			Text(TEXT("KIA"), X + S(412.f), Y, DVHudColors::Hostile, GEngine->GetSmallFont(), DVType::Body, false, true);
		}
		else
		{
			Bar(X, Y + S(28.f), S(412.f), S(4.f), Health, Health < 0.3f ? DVHudColors::Hostile : DVHudColors::Health);
			if (Mate->GetSuppression() > 60.f)
			{
				Text(TEXT("PINNED"), X + S(412.f), Y, DVHudColors::Warn, GEngine->GetSmallFont(), DVType::Caption, false, true);
			}
		}
		Y += S(52.f);
	}
}

void ADVHUD::DrawObjective(const ADVGameState* GS)
{
	if (!GS || GS->ObjectiveText.IsEmpty())
	{
		return;
	}
	const float CX = Canvas->ClipX * 0.5f;
	const float Y = S(34.f);
	const float Elapsed = GetWorld()->GetTimeSeconds() - GS->MissionStartTime;
	const FString Clock = FString::Printf(TEXT("%02d:%02d"), FMath::FloorToInt(Elapsed / 60.f), FMath::FloorToInt(FMath::Fmod(Elapsed, 60.f)));

	DrawRect(DVHudColors::Panel, CX - S(320.f), Y - S(10.f), S(640.f), S(84.f));
	DeckBand(CX - S(320.f), Y - S(10.f), S(640.f), S(30.f));
	Text(FString::Printf(TEXT("%s  -  %s"), *GS->MissionName.ToUpper(), *Clock), CX, Y - S(7.f), DVHudColors::DeckInk, GEngine->GetSmallFont(), DVType::Body, true, false, false);
	Text(GS->ObjectiveText, CX, Y + S(26.f), DVHudColors::Text, GEngine->GetMediumFont(), DVType::Emph, true);
	const int32 Down = GS->EnemiesTotal - GS->EnemiesRemaining;
	if (!GS->bMultiStage || GS->Stage == EDVMissionStage::Clear)
	{
		Bar(CX - S(200.f), Y + S(62.f), S(400.f), S(4.f), GS->EnemiesTotal > 0 ? static_cast<float>(Down) / GS->EnemiesTotal : 0.f, DVHudColors::Hostile);
		Text(FString::Printf(TEXT("%d / %d"), Down, GS->EnemiesTotal), CX + S(310.f), Y + S(52.f), DVHudColors::Dim, GEngine->GetSmallFont(), DVType::Caption, false, true);
	}
	else
	{
		const FLinearColor Amber = DVHudColors::Accent;
		const FLinearColor Cyan = DVHudColors::Cyan;
		FString Right;
		FLinearColor Color = Amber;
		switch (GS->Stage)
		{
		case EDVMissionStage::Sabotage:
			if (const ADVWalker* Walker = GS->bWalkerOp ? ADVWalker::Find(GetWorld()) : nullptr)
			{
				Right = FString::Printf(TEXT("COUPLINGS %d / %d"), Walker->GetCouplingsBlown(), ADVWalker::NumCouplings);
				// Plain guidance under the panel: what the walker is doing and what that means for you.
				const APawn* Me = GetOwningPawn();
				FString Hint;
				switch (Walker->GetState())
				{
				case EDVWalkerState::Walking: Hint = Walker->IsAboard(Me) ? TEXT("You're aboard. Stand on the glowing coupling to overload it") : TEXT("The walker is striding. Wait for it to kneel, then board up the rear ramp"); break;
				case EDVWalkerState::Kneeling: Hint = TEXT("It's kneeling. Get to the rear ramp"); break;
				case EDVWalkerState::Knelt: Hint = Walker->IsAboard(Me) ? TEXT("Aboard. Stand on the glowing coupling to overload it") : TEXT("The rear ramp is down: board now"); break;
				case EDVWalkerState::Rising: Hint = TEXT("It's rising. Ride it, or wait for the next kneel"); break;
				default: break;
				}
				if (Walker->GetCouplingProgress() > 0.01f)
				{
					Hint = FString::Printf(TEXT("OVERLOADING  %d%%"), FMath::RoundToInt(Walker->GetCouplingProgress() * 100.f));
				}
				Text(Hint, CX, Y + S(150.f), Amber, GEngine->GetSmallFont(), DVType::Caption, true);
			}
			else if (const ADVPressureManifold* Manifold = GS->bManifoldOp ? ADVPressureManifold::Find(GetWorld()) : nullptr)
			{
				Right = FString::Printf(TEXT("GREEN %d / %d"), FMath::RoundToInt(Manifold->GetBalance() * ADVPressureManifold::NumValves), ADVPressureManifold::NumValves);
				const APawn* Me = GetOwningPawn();
				if (Me && FVector::Dist(Me->GetActorLocation(), Manifold->GetActorLocation()) > 1600.f)
				{
					Text(TEXT("The manifold is in the Steamheart's yard. Follow the marker"), CX, Y + S(150.f), Amber, GEngine->GetSmallFont(), DVType::Caption, true);
				}
			}
			else
			{
				Right = FString::Printf(TEXT("PLANTING %d%%"), FMath::RoundToInt(GS->StageProgress * 100.f));
			}
			break;
		case EDVMissionStage::Capture: Right = FString::Printf(TEXT("CAPTURING %d%%"), FMath::RoundToInt(GS->StageProgress * 100.f)); break;
		case EDVMissionStage::Defend: { const int32 Secs = FMath::Max(0, FMath::CeilToInt(GS->StageEndTime - GetWorld()->GetTimeSeconds())); Right = GS->bManifoldOp ? FString::Printf(TEXT("RELIGHT %ds"), Secs) : FString::Printf(TEXT("DETONATION %ds"), Secs); } Color = DVHudColors::Hostile; break;
		case EDVMissionStage::Extract: Right = GS->StageProgress < 1.f ? FString(TEXT("RAVEN INBOUND")) : FString(TEXT("RAVEN ON THE PAD")); Color = Cyan; break;
		default: break;
		}
		Bar(CX - S(200.f), Y + S(62.f), S(400.f), S(4.f), GS->StageProgress, Color);
		Text(Right, CX + S(310.f), Y + S(52.f), Color, GEngine->GetSmallFont(), DVType::Caption, false, true);

		// Waypoint: the target while planting/holding, the landing zone for extraction.
		const bool bLZ = GS->Stage == EDVMissionStage::Extract;
		const FVector Where = bLZ ? FVector(GS->ExtractLocation) : FVector(GS->ObjectiveLocation);
		const FVector Screen = Project(Where + FVector(0.f, 0.f, 220.f));
		const APawn* Viewer = GetOwningPawn();
		const float Meters = Viewer ? FVector::Dist(Viewer->GetActorLocation(), Where) / 100.f : 0.f;
		if (Screen.Z > 0.f)
		{
			Text(FString::Printf(TEXT("%s  %dm"), bLZ ? TEXT("EXTRACT") : TEXT("TARGET"), FMath::RoundToInt(Meters)), Screen.X, Screen.Y, bLZ ? Cyan : Amber, GEngine->GetSmallFont(), DVType::Body, true);
		}
	}
}

void ADVHUD::DrawKillFeed(const ADVGameState* GS)
{
	if (!GS)
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	const float Right = Canvas->ClipX - S(40.f);
	float Y = S(40.f);
	for (int32 i = GS->GetKillFeed().Num() - 1; i >= 0; --i)
	{
		const FDVKillFeedEntry& Entry = GS->GetKillFeed()[i];
		const double Age = Now - Entry.Time;
		if (Age > 8.0)
		{
			continue;
		}
		const float Alpha = Age > 6.0 ? 1.f - static_cast<float>((Age - 6.0) / 2.0) : 1.f;
		FLinearColor Color = Entry.bFriendlyLoss ? DVHudColors::Hostile : DVHudColors::Text;
		Color.A = Alpha;
		const FString Line = Entry.Killer.IsEmpty() ? (Entry.Victim.EndsWith(TEXT("(wound down)")) ? Entry.Victim : FString::Printf(TEXT("%s killed"), *Entry.Victim))
			: FString::Printf(TEXT("%s  >  %s"), *Entry.Killer, *Entry.Victim);
		Text(Line, Right, Y, Color, GEngine->GetSmallFont(), DVType::Body, false, true);
		Y += S(26.f);
	}
}

void ADVHUD::DrawOrderToast()
{
	const double Age = GetWorld()->GetTimeSeconds() - OrderToastTime;
	if (Age > 1.6 + DVMotion::Out || OrderToast.IsEmpty())
	{
		return;
	}
	float Alpha = 1.f, Rise = 0.f;
	DVMotion::Reveal(Age, 1.6, Alpha, Rise);
	const FLinearColor Color = DVHudColors::A(DVHudColors::Friendly, Alpha);
	Text(FString::Printf(TEXT("Squad: %s"), *OrderToast), Canvas->ClipX * 0.5f, Canvas->ClipY * 0.68f + S(Rise), Color, GEngine->GetMediumFont(), DVType::Emph, true);
}

void ADVHUD::DrawDeathScreen()
{
	DrawRect(DVHudColors::A(DVHudColors::Wound, 0.35f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
	Text(TEXT("YOU ARE DOWN"), Canvas->ClipX * 0.5f, Canvas->ClipY * 0.42f, DVHudColors::Hostile, GEngine->GetLargeFont(), DVType::Display, true);
	Text(TEXT("Reinforcing on your squad..."), Canvas->ClipX * 0.5f, Canvas->ClipY * 0.42f + S(64.f), DVHudColors::Text, GEngine->GetMediumFont(), DVType::Emph, true);
}

void ADVHUD::DrawAfterAction(const ADVGameState* GS)
{
	const bool bWin = GS->Phase == EDVMissionPhase::Success;
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float CX = W * 0.5f;
	DrawRect(DVHudColors::A(DVHudColors::PanelStrong, 0.82f), 0.f, 0.f, W, H);

	float Y = H * 0.14f;
	// The result is painted across the deck: amber for a win, red for a loss.
	DeckBand(0.f, Y - S(14.f), W, S(96.f), bWin ? DVHudColors::Accent : DVHudColors::Hostile);
	Text(bWin ? TEXT("MISSION ACCOMPLISHED") : TEXT("SQUAD LOST"), CX, Y, DVHudColors::DeckInk, GEngine->GetLargeFont(), DVType::Hero, true, false, false);
	Y += S(104.f);
	const float Duration = GS->MissionEndTime - GS->MissionStartTime;
	Text(FString::Printf(TEXT("%s  |  %d of %d hostiles neutralised  |  %02d:%02d"), *GS->MissionName.ToUpper(),
		GS->EnemiesTotal - GS->EnemiesRemaining, GS->EnemiesTotal, FMath::FloorToInt(Duration / 60.f), FMath::FloorToInt(FMath::Fmod(Duration, 60.f))),
		CX, Y, DVHudColors::Dim, GEngine->GetMediumFont(), DVType::Emph, true);

	Y += S(70.f);
	const float Left = CX - S(420.f);
	const float RightCol = CX + S(420.f);
	Text(TEXT("After-action report"), Left, Y, DVHudColors::Dim, GEngine->GetSmallFont(), DVType::Body);
	Y += S(44.f);

	for (const FDVAfterActionLine& Line : GS->AfterAction)
	{
		// Roll call: the fallen get a brass plate; everyone else reads off the deck.
		if (Line.bKIA)
		{
			DrawRect(DVHudColors::Plate, Left - S(16.f), Y - S(10.f), S(872.f), S(48.f));
		}
		const FLinearColor NameColor = Line.bKIA ? DVHudColors::Brass : DVHudColors::Text;
		Text(FString::Printf(TEXT("%s %s"), *Line.Rank, *Line.Name), Left, Y, NameColor, GEngine->GetMediumFont(), DVType::Emph);
		const FString Status = Line.bKIA ? TEXT("KILLED IN ACTION") : Line.bWounded ? TEXT("WOUNDED  -  MEDBAY")
			: Line.bPromoted ? TEXT("PROMOTED") : TEXT("RETURNED TO DUTY");
		const FLinearColor StatusColor = Line.bKIA ? DVHudColors::Brass : Line.bWounded ? DVHudColors::Warn
			: Line.bPromoted ? DVHudColors::Health : DVHudColors::Dim;
		Text(Status, RightCol, Y, StatusColor, GEngine->GetMediumFont(), DVType::Emph, false, true);
		Text(FString::Printf(TEXT("%d career kills"), Line.Kills), CX + S(40.f), Y + S(4.f), DVHudColors::Dim, GEngine->GetSmallFont(), DVType::Body, true);
		Y += S(54.f);
	}

	if (GS->bRestrained || GS->CiviliansHurt > 0)
	{
		Y += S(12.f);
		Text(GS->CiviliansHurt == 0 ? FString(TEXT("Civilians hurt: none. The Guild noticed."))
			: FString::Printf(TEXT("Civilians fired on: %d. Their town will remember."), GS->CiviliansHurt), CX, Y,
			GS->CiviliansHurt == 0 ? DVHudColors::Health : DVHudColors::Warn, GEngine->GetMediumFont(), DVType::Emph, true);
		Y += S(40.f);
	}
	if (GS->AfterAction.ContainsByPredicate([](const FDVAfterActionLine& L) { return L.bKIA; }))
	{
		Y += S(12.f);
		Text(TEXT("The fallen are added to the Memorial Wall. Replacements are greener."), CX, Y, DVHudColors::Dim, GEngine->GetSmallFont(), DVType::Body, true);
	}

	const float Pulse = 0.6f + 0.4f * FMath::Sin(GetWorld()->GetRealTimeSeconds() * 3.f);
	Text(ADVKestrel::Find(GetWorld()) ? TEXT("ENTER  redeploy        K  fly home to the Kestrel        M  strategic map") : TEXT("ENTER  redeploy        M  strategic map"), CX, H * 0.86f, DVHudColors::A(DVHudColors::Text, Pulse), GEngine->GetMediumFont(), DVType::Emph, true);
}

void ADVHUD::DrawTitleCard(const ADVGameState* GS)
{
	const ADVPlayerController* CardPC = Cast<ADVPlayerController>(PlayerOwner);
	if (!GS || GS->MissionLocation.IsEmpty() || bBoardOpen || (CardPC && CardPC->IsMemorialOpen()))
	{
		return;	// never on top of an open panel
	}
	const float Age = GetWorld()->GetTimeSeconds() - GS->MissionStartTime;
	if (Age > 8.f)
	{
		return;
	}
	// Fade in, hold, fade out; letterboxed like a mission intro.
	const float Alpha = FMath::Clamp(Age / 0.8f, 0.f, 1.f) * FMath::Clamp((8.f - Age) / 1.2f, 0.f, 1.f);
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	DrawRect(DVHudColors::A(DVHudColors::Shade, 0.85f * Alpha), 0.f, 0.f, W, H * 0.1f);
	DrawRect(DVHudColors::A(DVHudColors::Shade, 0.85f * Alpha), 0.f, H * 0.9f, W, H * 0.1f);

	// No accent bar: the name's size carries it. It rises 12 px as it fades in, with the same ease-out as the toasts.
	const float X = S(90.f);
	const float Y = H * 0.62f + S(12.f * (1.f - DVMotion::EaseOut(Age / 0.8f)));
	Text(GS->MissionName.ToUpper(), X, Y, DVHudColors::A(DVHudColors::Text, Alpha), GEngine->GetLargeFont(), DVType::Hero);
	Text(GS->MissionLocation, X, Y + S(70.f), DVHudColors::A(DVHudColors::Accent, Alpha), GEngine->GetMediumFont(), DVType::Emph);
	Text(GS->MissionSubtitle, X, Y + S(104.f), DVHudColors::A(DVHudColors::Dim, Alpha), GEngine->GetSmallFont(), DVType::Body);
}

void ADVHUD::DrawRadio(const ADVGameState* GS)
{
	if (!GS || GS->GetRadio().Num() == 0)
	{
		return;
	}
	const FDVRadioLine& Line = GS->GetRadio().Last();
	const double Age = GetWorld()->GetTimeSeconds() - Line.Time;
	const double Hold = 3.0 + Line.Text.Len() * 0.045;
	if (Age > Hold + DVMotion::Out)
	{
		return;
	}
	float Alpha = 1.f, Rise = 0.f;
	DVMotion::Reveal(Age, Hold, Alpha, Rise);
	const bool bCinder = Line.Speaker == TEXT("CINDER");
	const FLinearColor SpeakerColor = bCinder ? DVHudColors::A(DVHudColors::Hostile, Alpha) : DVHudColors::A(DVHudColors::Cyan, Alpha);
	const float CX = Canvas->ClipX * 0.5f;
	const float Y = Canvas->ClipY * 0.8f + S(Rise);

	float TW = 0.f, TH = 0.f;
	GetTextSize(Line.Text, TW, TH, GEngine->GetMediumFont(), DVType::Emph * UIScale);
	DrawRect(DVHudColors::A(DVHudColors::Shade, 0.45f * Alpha), CX - TW * 0.5f - S(20.f), Y - S(30.f), TW + S(40.f), S(70.f));
	Text(Line.Speaker.ToUpper(), CX, Y - S(24.f), SpeakerColor, GEngine->GetSmallFont(), DVType::Caption, true);
	Text(Line.Text, CX, Y, DVHudColors::A(DVHudColors::Text, Alpha), GEngine->GetMediumFont(), DVType::Emph, true);
}

void ADVHUD::DrawOrbital(const ADVGameState* GS)
{
	if (!GS || GS->Phase != EDVMissionPhase::InProgress || GS->OrbitalReadyTime > 1.e8f)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	const float Remaining = GS->OrbitalReadyTime - Now;
	const float CX = Canvas->ClipX * 0.5f;
	const float Y = Canvas->ClipY - S(70.f);
	if (GS->bRestrained)
	{
		Text(TEXT("RESTRAINED RULES  -  CIVILIANS PRESENT"), CX, Y, DVHudColors::Dim, GEngine->GetSmallFont(), DVType::Body, true);
	}
	else if (Remaining <= 0.f)
	{
		const float Pulse = 0.7f + 0.3f * FMath::Sin(GetWorld()->GetRealTimeSeconds() * 4.f);
		Text(TEXT("ORBITAL STRIKE READY  [B]"), CX, Y, DVHudColors::A(DVHudColors::Accent, Pulse), GEngine->GetMediumFont(), DVType::Emph, true);
	}
	else
	{
		Text(FString::Printf(TEXT("ORBITAL STRIKE  %ds"), FMath::CeilToInt(Remaining)), CX, Y, DVHudColors::Dim, GEngine->GetSmallFont(), DVType::Body, true);
	}

	DrawCallIns(GS);
	DrawHeavies();

	// Countdown over the painted target.
	FVector Target;
	double ImpactTime = 0.0;
	if (GS->GetPendingStrike(Target, ImpactTime))
	{
		const FVector Screen = Project(Target + FVector(0.f, 0.f, 150.f));
		if (Screen.Z > 0.f)
		{
			const float T = FMath::Max(0.0, ImpactTime - Now);
			Text(FString::Printf(TEXT("IMPACT %.1f"), T), Screen.X, Screen.Y, DVHudColors::Hostile, GEngine->GetMediumFont(), DVType::Emph, true);
		}
	}
}

void ADVHUD::DrawVehiclePrompt(const ADVSoldierCharacter* Soldier)
{
	if (const UDVSeatComponent* Seat = UDVSeatComponent::FindNear(Soldier))
	{
		Text(FString::Printf(TEXT("[Z]  Board %s"), *Seat->VehicleName), Canvas->ClipX * 0.5f, Canvas->ClipY * 0.6f, DVHudColors::Friendly, GEngine->GetMediumFont(), DVType::Emph, true);
	}
	else if (const ADVPressureManifold* Manifold = ADVPressureManifold::Find(GetWorld()); Manifold && Manifold->FindValveNear(Soldier) >= 0)
	{
		const int32 Valve = Manifold->FindValveNear(Soldier);
		Text(FString::Printf(TEXT("[Z]  Turn valve %s"), *ADVPressureManifold::ValveName(Valve)), Canvas->ClipX * 0.5f, Canvas->ClipY * 0.6f, DVHudColors::Warn, GEngine->GetMediumFont(), DVType::Emph, true);
		DrawManifold(Manifold, Valve);
	}
	else if (const ADVInteractable* Thing = ADVInteractable::FindNear(Soldier))
	{
		Text(FString::Printf(TEXT("[Z]  %s"), *Thing->Prompt), Canvas->ClipX * 0.5f, Canvas->ClipY * 0.6f, DVHudColors::Warn, GEngine->GetMediumFont(), DVType::Emph, true);
		if (Thing->Interaction == EDVInteraction::ContractBoard)
		{
			DrawContractBoard();
		}
	}
	else
	{
		DrawTownsfolk(Soldier);
	}
	if (const ADVPlayerController* PC = Cast<ADVPlayerController>(PlayerOwner); PC && PC->IsMemorialOpen())
	{
		DrawMemorial();
	}
	DrawProgression();
	if (const ADVPlayerController* PC = Cast<ADVPlayerController>(PlayerOwner); PC && GetWorld()->GetTimeSeconds() - PC->GetArmouryTime() < 4.0)
	{
		const UDVWeaponData* Data = Soldier->GetCurrentWeapon() ? Soldier->GetCurrentWeapon()->GetData() : nullptr;
		if (Data)
		{
			const float W = Canvas->ClipX;
			const float H = Canvas->ClipY;
			DrawRect(DVHudColors::Panel, W * 0.3f, H * 0.62f, W * 0.4f, S(96.f));
			Text(FString::Printf(TEXT("ARMOURY  >  %s"), *Data->DisplayName.ToString().ToUpper()), W * 0.5f, H * 0.62f + S(12.f),
				DVHudColors::Health, GEngine->GetLargeFont(), DVType::Title, true);
			Text(DVLoadouts::Describe(Data), W * 0.5f, H * 0.62f + S(60.f), DVHudColors::Text, GEngine->GetSmallFont(), DVType::Body, true);
			if (!PC->ArmouryNote.IsEmpty())
			{
				Text(PC->ArmouryNote, W * 0.5f, H * 0.62f + S(104.f), DVHudColors::Warn, GEngine->GetSmallFont(), DVType::Caption, true);
			}
		}
	}
}

void ADVHUD::DrawVehicle(APawn* Vehicle)
{
	const UDVSeatComponent* Seat = Vehicle->FindComponentByClass<UDVSeatComponent>();
	const float CX = Canvas->ClipX * 0.5f;
	const float CY = Canvas->ClipY * 0.5f;

	// Reticle: a box for vehicle weapons.
	const float R = S(16.f);
	const float Thick = FMath::Max(1.f, S(2.f));
	const FLinearColor C = DVHudColors::A(DVHudColors::Text, 0.85f);
	DrawLine(CX - R, CY - R, CX + R, CY - R, C, Thick);
	DrawLine(CX - R, CY + R, CX + R, CY + R, C, Thick);
	DrawLine(CX - R, CY - R, CX - R, CY + R, C, Thick);
	DrawLine(CX + R, CY - R, CX + R, CY + R, C, Thick);
	DrawRect(C, CX - Thick, CY - Thick, Thick * 2.f, Thick * 2.f);

	float Speed = 0.f;
	FString Extra;
	float Ready = 1.f;
	if (const ADVGroundVehicle* Ground = Cast<ADVGroundVehicle>(Vehicle))
	{
		Speed = Ground->GetSpeedKph();
		Ready = Ground->GetReloadFraction();
	}
	else if (const ADVGunship* Air = Cast<ADVGunship>(Vehicle))
	{
		Speed = Air->GetSpeedKph();
		Ready = Air->GetRocketReadyFraction();
		Extra = FString::Printf(TEXT("ALT %d m   %s   FLARES %s"), FMath::RoundToInt(Air->GetAltitude()), Air->IsJetMode() ? TEXT("JET") : TEXT("HOVER"),
			Air->AreFlaresActive() ? TEXT("OUT") : Air->GetFlareReadyFraction() >= 1.f ? TEXT("READY") : TEXT("..."));
		if (Air->IsCockpitView())
		{
			DrawCockpit(Air);
		}
	}
	else if (const ADVBoat* Boat = Cast<ADVBoat>(Vehicle))
	{
		Speed = Boat->GetSpeedKph();
		Ready = Boat->GetMissileReadyFraction();
		Extra = TEXT("MISSILES");
	}
	else if (const ADVHoverbike* Bike = Cast<ADVHoverbike>(Vehicle))
	{
		Speed = Bike->GetSpeedKph();
		Ready = Bike->GetBoostFraction();
		Extra = TEXT("BOOST");
	}

	const float X = S(40.f);
	const float Y = Canvas->ClipY - S(150.f);
	DrawRect(DVHudColors::Panel, X - S(14.f), Y - S(14.f), S(470.f), S(124.f));
	Text(Seat ? Seat->VehicleName.ToUpper() : FString(), X, Y, DVHudColors::Text, GEngine->GetMediumFont(), DVType::Emph);
	Text(FString::Printf(TEXT("%d km/h   %s"), FMath::RoundToInt(Speed), *Extra), X, Y + S(32.f), DVHudColors::Friendly, GEngine->GetMediumFont(), DVType::Emph);
	Text(Seat ? Seat->WeaponName : FString(), X, Y + S(64.f), DVHudColors::Dim, GEngine->GetSmallFont(), DVType::Caption);
	Bar(X, Y + S(92.f), S(430.f), S(5.f), Ready, Ready >= 1.f ? DVHudColors::Health : DVHudColors::Warn);
	if (Seat)
	{
		const float Hull = Seat->GetHealthFraction();
		Text(FString::Printf(TEXT("HULL %d%%"), FMath::RoundToInt(Hull * 100.f)), X + S(430.f), Y, Hull < 0.3f ? DVHudColors::Hostile : DVHudColors::Dim, GEngine->GetSmallFont(), DVType::Body, false, true);
		Bar(X + S(250.f), Y + S(26.f), S(180.f), S(5.f), Hull, Hull < 0.3f ? DVHudColors::Hostile : DVHudColors::Health);
	}
	const FString Help = Cast<ADVGunship>(Vehicle) ? TEXT("F hover/jet  |  V cockpit  |  X flares  |  LMB cannon  RMB rockets  |  Z exit")
		: Cast<ADVHoverbike>(Vehicle) ? TEXT("W/S throttle  |  mouse steer  |  Shift boost  |  Space hop  |  LMB guns  |  Z exit")
		: TEXT("WASD drive  |  mouse aim  |  LMB fire  |  Z exit");
	Text(Help, CX, Canvas->ClipY - S(40.f), DVHudColors::Dim, GEngine->GetSmallFont(), DVType::Caption, true);
}

void ADVHUD::DrawMemorial()
{
	const UGameInstance* GI = GetGameInstance();
	const UDVRosterSubsystem* Roster = GI ? GI->GetSubsystem<UDVRosterSubsystem>() : nullptr;
	if (!Roster)
	{
		return;
	}
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	DrawRect(DVHudColors::A(DVHudColors::PanelStrong, 0.86f), W * 0.12f, H * 0.1f, W * 0.76f, H * 0.78f);
	const float L = W * 0.15f;
	const float R = W * 0.52f;
	float Y = H * 0.13f;
	Text(TEXT("Memorial wall"), L, Y, DVHudColors::Brass, GEngine->GetLargeFont(), DVType::Title);
	Text(TEXT("Active roster"), R, Y, DVHudColors::Friendly, GEngine->GetLargeFont(), DVType::Title);
	Y += S(64.f);

	float LY = Y;
	const TArray<FDVSoldierRecord> Fallen = Roster->GetMemorial();
	if (Fallen.Num() == 0)
	{
		Text(TEXT("No names yet. Keep it that way."), L, LY, DVHudColors::Dim, GEngine->GetSmallFont(), DVType::Body);
	}
	for (int32 i = Fallen.Num() - 1; i >= 0 && LY < H * 0.82f; --i)
	{
		const FDVSoldierRecord& Rec = Fallen[i];
		DrawRect(DVHudColors::Plate, L - S(12.f), LY - S(6.f), W * 0.34f, S(50.f));	// a name plate
		Text(FString::Printf(TEXT("%s %s"), *UDVRosterSubsystem::RankToString(Rec.Rank), *Rec.GetDisplayName()), L, LY, DVHudColors::Brass, GEngine->GetSmallFont(), DVType::Body);
		Text(FString::Printf(TEXT("%s  -  %s"), *Rec.DiedAt, *Rec.CauseOfDeath), L, LY + S(22.f), DVHudColors::Dim, GEngine->GetSmallFont(), DVType::Caption);
		LY += S(54.f);
	}

	float RY = Y;
	for (const FDVSoldierRecord& Rec : Roster->GetState().Soldiers)
	{
		if (Rec.Status == EDVSoldierStatus::KIA || RY > H * 0.82f)
		{
			continue;
		}
		const bool bWounded = Rec.Status == EDVSoldierStatus::Wounded;
		Text(FString::Printf(TEXT("%s %s"), *UDVRosterSubsystem::RankToString(Rec.Rank), *Rec.GetDisplayName()), R, RY,
			bWounded ? DVHudColors::Warn : DVHudColors::Text, GEngine->GetSmallFont(), DVType::Body);
		Text(FString::Printf(TEXT("%d missions  |  %d kills%s"), Rec.MissionsFlown, Rec.Kills, bWounded ? TEXT("  |  MEDBAY") : TEXT("")),
			R + S(20.f), RY + S(24.f), DVHudColors::Dim, GEngine->GetSmallFont(), DVType::Caption);
		RY += S(54.f);
	}
	Text(TEXT("[Z] close"), W * 0.5f, H * 0.84f, DVHudColors::Dim, GEngine->GetMediumFont(), DVType::Emph, true);
}

void ADVHUD::DrawCallIns(const ADVGameState* GS)
{
	const float Now = GetWorld()->GetTimeSeconds();
	const float Y = Canvas->ClipY - S(44.f);
	struct FCallIn { const TCHAR* Name; const TCHAR* Key; float Ready; };
	const FCallIn Items[] = { { TEXT("SUPPLY DROP"), TEXT("N"), GS->SupplyReadyTime }, { TEXT("STRAFING RUN"), TEXT("H"), GS->StrafeReadyTime } };
	for (int32 i = 0; i < 2; ++i)
	{
		const float X = Canvas->ClipX * 0.5f + (i == 0 ? -S(150.f) : S(150.f));
		const float Remaining = Items[i].Ready - Now;
		if (i == 1 && GS->bRestrained)
		{
			Text(TEXT("STRAFING RUN  NOT CLEARED"), X, Y, DVHudColors::Dim, GEngine->GetSmallFont(), DVType::Body, true);
			continue;
		}
		if (Remaining <= 0.f)
		{
			Text(FString::Printf(TEXT("%s READY  [%s]"), Items[i].Name, Items[i].Key), X, Y, i == 0 ? DVHudColors::Health : DVHudColors::Accent, GEngine->GetSmallFont(), DVType::Body, true);
		}
		else
		{
			Text(FString::Printf(TEXT("%s  %ds"), Items[i].Name, FMath::CeilToInt(Remaining)), X, Y, DVHudColors::Dim, GEngine->GetSmallFont(), DVType::Body, true);
		}
	}
	// Supply pods on the ground: how many refills are left.
	for (TActorIterator<ADVSupplyPod> It(GetWorld()); It; ++It)
	{
		if (!It->HasLanded())
		{
			continue;
		}
		const FVector Screen = Project(It->GetActorLocation() + FVector(0.f, 0.f, 330.f));
		if (Screen.Z > 0.f)
		{
			Text(It->GetChargesLeft() > 0 ? FString::Printf(TEXT("RESUPPLY x%d"), It->GetChargesLeft()) : FString(TEXT("EMPTY")), Screen.X, Screen.Y,
				DVHudColors::Health, GEngine->GetSmallFont(), DVType::Caption, true);
		}
	}
}

void ADVHUD::DrawHeavies()
{
	const APawn* Viewer = GetOwningPawn();
	for (TActorIterator<ADVBulwark> It(GetWorld()); It; ++It)
	{
		if (It->IsDead() || It->IsHidden() || (Viewer && FVector::Dist(Viewer->GetActorLocation(), It->GetActorLocation()) > 7000.f))
		{
			continue;
		}
		const FVector Screen = Project(It->GetActorLocation() + FVector(0.f, 0.f, 190.f));
		if (Screen.Z <= 0.f)
		{
			continue;
		}
		const float W = S(160.f);
		Text(TEXT("BULWARK"), Screen.X, Screen.Y - S(26.f), DVHudColors::Hostile, GEngine->GetSmallFont(), DVType::Body, true);
		Bar(Screen.X - W * 0.5f, Screen.Y, W, S(6.f), FMath::Clamp(It->GetHealth() / ADVBulwark::BulwarkHealth, 0.f, 1.f), DVHudColors::Hostile);
	}
}

void ADVHUD::DrawDowned(const ADVSoldierCharacter* Soldier)
{
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float Remaining = Soldier->GetBleedOutRemaining();
	const float Pulse = 0.5f + 0.5f * FMath::Sin(GetWorld()->GetRealTimeSeconds() * (3.f + 6.f * (1.f - Remaining / ADVSoldierCharacter::BleedOutSeconds)));
	DrawRect(DVHudColors::A(DVHudColors::Wound, 0.25f + 0.2f * Pulse), 0.f, 0.f, W, H);
	Text(TEXT("YOU ARE DOWN"), W * 0.5f, H * 0.38f, DVHudColors::Hostile, GEngine->GetLargeFont(), DVType::Display, true);
	Text(FString::Printf(TEXT("Bleeding out  %ds"), FMath::CeilToInt(Remaining)), W * 0.5f, H * 0.38f + S(56.f), DVHudColors::Text, GEngine->GetMediumFont(), DVType::Emph, true);
	if (Soldier->GetReviveProgress() > 0.f)
	{
		Text(TEXT("A squadmate is working on you..."), W * 0.5f, H * 0.38f + S(96.f), DVHudColors::Friendly, GEngine->GetMediumFont(), DVType::Emph, true);
		Bar(W * 0.4f, H * 0.38f + S(130.f), W * 0.2f, S(6.f), Soldier->GetReviveProgress(), DVHudColors::Friendly);
	}
	else
	{
		Text(TEXT("Hold on. Your squad is coming."), W * 0.5f, H * 0.38f + S(96.f), DVHudColors::Dim, GEngine->GetMediumFont(), DVType::Emph, true);
	}
}

void ADVHUD::DrawCasualties(const ADVSoldierCharacter* Viewer)
{
	for (TActorIterator<ADVSoldierCharacter> It(GetWorld()); It; ++It)
	{
		const ADVSoldierCharacter* Casualty = *It;
		if (Casualty == Viewer || !Casualty->IsDowned() || Casualty->TeamId != Viewer->TeamId)
		{
			continue;
		}
		const FVector Screen = Project(Casualty->GetActorLocation() + FVector(0.f, 0.f, 110.f));
		if (Screen.Z <= 0.f)
		{
			continue;
		}
		const float Dist = FVector::Dist(Casualty->GetActorLocation(), Viewer->GetActorLocation());
		const bool bInReach = Dist < ADVSoldierCharacter::ReviveRange;
		Text(FString::Printf(TEXT("+ %s  %ds"), *Casualty->GetSoldierName(), FMath::CeilToInt(Casualty->GetBleedOutRemaining())), Screen.X, Screen.Y - S(30.f),
			DVHudColors::Hostile, GEngine->GetSmallFont(), DVType::Body, true);
		Bar(Screen.X - S(60.f), Screen.Y - S(4.f), S(120.f), S(5.f), Casualty->GetReviveProgress(), DVHudColors::Friendly);
		Text(bInReach ? FString(TEXT("[hold Z] revive")) : FString::Printf(TEXT("%dm"), FMath::RoundToInt(Dist / 100.f)), Screen.X, Screen.Y + S(8.f),
			bInReach ? DVHudColors::Friendly : DVHudColors::Dim, GEngine->GetSmallFont(), DVType::Caption, true);
	}
}

void ADVHUD::DrawThrusters(const ADVSoldierCharacter* Soldier)
{
	const float Max = Soldier->GetMaxBoostCharges();
	const float Charges = Soldier->GetBoostCharges();
	const float X = S(40.f);
	const float Y = Canvas->ClipY - S(160.f);
	Text(TEXT("Thrust"), X, Y, DVHudColors::Dim, GEngine->GetSmallFont(), DVType::Caption);
	for (int32 i = 0; i < FMath::RoundToInt(Max); ++i)
	{
		const float Fill = FMath::Clamp(Charges - i, 0.f, 1.f);
		const float PX = X + S(80.f) + i * S(46.f);
		DrawRect(DVHudColors::A(DVHudColors::PanelStrong, 0.8f), PX, Y + S(4.f), S(40.f), S(10.f));
		DrawRect(Fill >= 1.f ? DVHudColors::Cyan : DVHudColors::Track, PX, Y + S(4.f), S(40.f) * Fill, S(10.f));
	}
}

void ADVHUD::DrawLocker()
{
	const UDVProfileSubsystem* Profile = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDVProfileSubsystem>() : nullptr;
	if (!Profile)
	{
		return;
	}
	const FDVLook& L = Profile->GetLook();
	const float X = Canvas->ClipX * 0.06f;
	float Y = Canvas->ClipY * 0.22f;
	DrawRect(DVHudColors::A(DVHudColors::PanelStrong, 0.82f), X - S(20.f), Y - S(20.f), S(560.f), S(330.f));
	Text(TEXT("Barracks locker"), X, Y, DVHudColors::Violet, GEngine->GetLargeFont(), DVType::Title);
	Y += S(60.f);
	const FString Rows[4] = {
		FString::Printf(TEXT("[1]  Frame          %s"), L.Body == 1 ? TEXT("Light (B)") : TEXT("Standard (A)")),
		FString::Printf(TEXT("[2]  Armour paint   %s"), *UDVProfileSubsystem::PaintName(L.Paint)),
		FString::Printf(TEXT("[3]  Optics         %s"), *UDVProfileSubsystem::OpticsName(L.Optics)),
		FString::Printf(TEXT("[4]  Callsign       \"%s\""), *UDVProfileSubsystem::CallsignName(L.Callsign)) };
	const FLinearColor Swatch[4] = { DVHudColors::Text, UDVProfileSubsystem::PaintColor(L.Paint) * 2.f, UDVProfileSubsystem::OpticsColor(L.Optics), DVHudColors::Text };
	for (int32 i = 0; i < 4; ++i)
	{
		Text(Rows[i], X, Y, DVHudColors::Text, GEngine->GetMediumFont(), DVType::Emph);
		if (i == 1 || i == 2)
		{
			DrawRect(FLinearColor(Swatch[i].R, Swatch[i].G, Swatch[i].B, 1.f), X + S(470.f), Y + S(6.f), S(30.f), S(22.f));
		}
		Y += S(48.f);
	}
	Text(TEXT("Saved automatically.   [Z] close"), X, Y + S(10.f), DVHudColors::Dim, GEngine->GetSmallFont(), DVType::Caption);

	// Service record: level, XP to next, perks.
	const float RX = X + S(600.f);
	float RY = Canvas->ClipY * 0.22f;
	const int32 Level = Profile->GetLevel();
	DrawRect(DVHudColors::A(DVHudColors::PanelStrong, 0.82f), RX - S(20.f), RY - S(20.f), S(480.f), S(330.f));
	Text(FString::Printf(TEXT("Commander level %d"), Level), RX, RY, DVHudColors::Gold, GEngine->GetLargeFont(), DVType::Title);
	RY += S(52.f);
	const int32 Cur = Profile->XPForLevel(Level);
	const int32 Next = Profile->XPForLevel(Level + 1);
	if (Level < DVPerks::MaxLevel)
	{
		Bar(RX, RY, S(420.f), S(6.f), float(Profile->GetXP() - Cur) / FMath::Max(1, Next - Cur), DVHudColors::Gold);
		Text(FString::Printf(TEXT("%d / %d XP"), Profile->GetXP(), Next), RX, RY + S(12.f), DVHudColors::Dim, GEngine->GetSmallFont(), DVType::Caption);
	}
	RY += S(46.f);
	for (int32 PerkLevel = DVPerks::ExtraThruster; PerkLevel <= DVPerks::KestrelFavour; ++PerkLevel)
	{
		const bool bHave = Level >= PerkLevel;
		Text(FString::Printf(TEXT("%s  L%d  %s"), bHave ? TEXT("+") : TEXT("-"), PerkLevel, *UDVProfileSubsystem::PerkName(PerkLevel)), RX, RY,
			bHave ? DVHudColors::Friendly : DVHudColors::Dim, GEngine->GetSmallFont(), DVType::Caption);
		RY += S(32.f);
	}

	// Standing with the three powers, from Hostile to Allied.
	const float FX = RX + S(500.f);
	float FY = Canvas->ClipY * 0.22f;
	DrawRect(DVHudColors::A(DVHudColors::PanelStrong, 0.82f), FX - S(20.f), FY - S(20.f), S(440.f), S(76.f + 86.f * DVFactions::Num));
	Text(TEXT("Standing"), FX, FY, DVHudColors::Gold, GEngine->GetLargeFont(), DVType::Title);
	FY += S(56.f);
	const TCHAR* Perks[DVFactions::Num] = { TEXT("Trusted: Longreach DMR  |  Allied: orbital priority"),
		TEXT("Trusted: fuller supply pods"), TEXT("Trusted: Pulse Carbine  |  Allied: faster supply drops"),
		TEXT("Earned in Brasshaven: wind down, don't scrap, and mind the townsfolk") };
	for (int32 F = 0; F < DVFactions::Num; ++F)
	{
		const int32 V = Profile->GetRep(F);
		Text(FString::Printf(TEXT("%s  -  %s (%d)"), *UDVProfileSubsystem::FactionName(F), *UDVProfileSubsystem::TierName(V), V), FX, FY, DVHudColors::Text, GEngine->GetSmallFont(), DVType::Body);
		const float Mid = FX + S(200.f);
		DrawRect(DVHudColors::A(DVHudColors::Text, 0.15f), FX, FY + S(30.f), S(400.f), S(5.f));
		DrawRect(V >= 0 ? DVHudColors::Health : DVHudColors::Hostile, V >= 0 ? Mid : Mid + V * S(2.f), FY + S(30.f), FMath::Abs(V) * S(2.f), S(5.f));
		Text(Perks[F], FX, FY + S(42.f), DVHudColors::Dim, GEngine->GetSmallFont(), DVType::Caption);
		FY += S(86.f);
	}
}

void ADVHUD::DrawProgression()
{
	const UDVProfileSubsystem* Profile = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDVProfileSubsystem>() : nullptr;
	if (!Profile)
	{
		return;
	}
	const double Now = GetWorld()->GetRealTimeSeconds();
	if (Now - Profile->LastAwardTime < 2.2 + DVMotion::Out)
	{
		float A = 1.f, Rise = 0.f;
		DVMotion::Reveal(Now - Profile->LastAwardTime, 2.2, A, Rise);
		Text(Profile->LastAward, Canvas->ClipX * 0.5f, Canvas->ClipY * 0.56f + S(Rise), DVHudColors::A(DVHudColors::Gold, A), GEngine->GetSmallFont(), DVType::Body, true);
	}
	if (Now - Profile->LastRepTime < 3.8 + DVMotion::Out)
	{
		float A = 1.f, Rise = 0.f;
		DVMotion::Reveal(Now - Profile->LastRepTime, 3.8, A, Rise);
		// Under the after-action roster when the report is up, so the result headline stays clear.
		const ADVGameState* RepGS = GetWorld()->GetGameState<ADVGameState>();
		const float RepY = RepGS && RepGS->Phase != EDVMissionPhase::InProgress && RepGS->Ascent == 0 ? 0.64f : 0.2f;
		for (int32 i = 0; i < Profile->RecentRep.Num(); ++i)
		{
			Text(Profile->RecentRep[i], Canvas->ClipX * 0.5f, Canvas->ClipY * RepY + i * S(34.f) + S(Rise), DVHudColors::A(DVHudColors::Gold, A), GEngine->GetMediumFont(), DVType::Emph, true);
		}
	}
	if (Now - Profile->LastLevelUpTime < 4.8 + DVMotion::Out)
	{
		float A = 1.f, Rise = 0.f;
		DVMotion::Reveal(Now - Profile->LastLevelUpTime, 4.8, A, Rise);
		const float CX = Canvas->ClipX * 0.5f;
		const float Y = Canvas->ClipY * 0.26f + S(Rise);
		DrawRect(DVHudColors::A(DVHudColors::PanelStrong, 0.86f * A), CX - S(330.f), Y - S(16.f), S(660.f), S(110.f));
		Text(FString::Printf(TEXT("Commander level %d"), Profile->LastLevelUp), CX, Y, DVHudColors::A(DVHudColors::Gold, A), GEngine->GetLargeFont(), DVType::Display, true);
		const FString Perk = UDVProfileSubsystem::PerkName(Profile->LastLevelUp);
		if (!Perk.IsEmpty())
		{
			Text(FString::Printf(TEXT("Unlocked: %s"), *Perk), CX, Y + S(58.f), DVHudColors::A(DVHudColors::Text, A), GEngine->GetMediumFont(), DVType::Emph, true);
		}
	}
}

void ADVHUD::DrawCockpit(const ADVGunship* Air)
{
	// Flight HUD in the canopy glass: horizon and pitch ladder, heading tape, speed and altitude boxes.
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float CX = W * 0.5f;
	const float CY = H * 0.5f;
	const FLinearColor Green = DVHudColors::A(DVHudColors::Health, 0.85f);	// cockpit symbology
	const FRotator View = PlayerOwner && PlayerOwner->PlayerCameraManager ? PlayerOwner->PlayerCameraManager->GetCameraRotation() : FRotator::ZeroRotator;
	const float PxPerDeg = H / 70.f;
	for (int32 Deg = -60; Deg <= 60; Deg += 10)
	{
		const float Y = CY + (View.Pitch - Deg) * PxPerDeg;
		if (Y < H * 0.15f || Y > H * 0.85f)
		{
			continue;
		}
		const float Half = Deg == 0 ? S(260.f) : S(90.f);
		DrawLine(CX - Half, Y, CX - S(40.f), Y, Green, Deg == 0 ? 2.f : 1.f);
		DrawLine(CX + S(40.f), Y, CX + Half, Y, Green, Deg == 0 ? 2.f : 1.f);
		if (Deg != 0)
		{
			Text(FString::FromInt(Deg), CX + Half + S(8.f), Y - S(10.f), Green, GEngine->GetSmallFont(), DVType::Caption);
		}
	}
	// Heading tape.
	const float Heading = FRotator::NormalizeAxis(View.Yaw) < 0.f ? FRotator::NormalizeAxis(View.Yaw) + 360.f : FRotator::NormalizeAxis(View.Yaw);
	for (int32 Mark = -40; Mark <= 40; Mark += 10)
	{
		const int32 Deg = (FMath::RoundToInt(Heading / 10.f) * 10 + Mark + 360) % 360;
		const float X = CX + (Deg - Heading + (Deg - Heading > 180.f ? -360.f : Deg - Heading < -180.f ? 360.f : 0.f)) * S(9.f);
		DrawLine(X, H * 0.12f, X, H * 0.12f + S(10.f), Green, 1.f);
		Text(FString::Printf(TEXT("%03d"), Deg), X, H * 0.12f + S(12.f), Green, GEngine->GetSmallFont(), DVType::Caption, true);
	}
	// Speed (left) and altitude (right) boxes.
	DrawRect(DVHudColors::A(DVHudColors::Shade, 0.3f), CX - S(420.f), CY - S(20.f), S(120.f), S(40.f));
	Text(FString::Printf(TEXT("%d"), FMath::RoundToInt(Air->GetSpeedKph())), CX - S(360.f), CY - S(14.f), Green, GEngine->GetMediumFont(), DVType::Emph, true);
	DrawRect(DVHudColors::A(DVHudColors::Shade, 0.3f), CX + S(300.f), CY - S(20.f), S(120.f), S(40.f));
	Text(FString::Printf(TEXT("%d"), FMath::RoundToInt(Air->GetAltitude())), CX + S(360.f), CY - S(14.f), Green, GEngine->GetMediumFont(), DVType::Emph, true);
	Text(Air->IsJetMode() ? TEXT("JET") : TEXT("HOVER"), CX - S(360.f), CY + S(30.f), Green, GEngine->GetSmallFont(), DVType::Caption, true);
	Text(Air->AreFlaresActive() ? TEXT("FLARES OUT") : TEXT("ECM ARMED"), CX + S(360.f), CY + S(30.f), Air->AreFlaresActive() ? DVHudColors::Accent : Green, GEngine->GetSmallFont(), DVType::Caption, true);
	Text(TEXT("30MM"), CX - S(60.f), CY + S(120.f), Green, GEngine->GetSmallFont(), DVType::Caption, true);
	Text(Air->GetRocketReadyFraction() >= 1.f ? TEXT("RKT RDY") : TEXT("RKT ..."), CX + S(60.f), CY + S(120.f), Green, GEngine->GetSmallFont(), DVType::Caption, true);
}

void ADVHUD::DrawAwareness(const ADVSoldierCharacter* Viewer)
{
	// Unaware sentries show a grey eye-line marker; alerted frames a red "!".
	for (TActorIterator<ADVSoldierCharacter> It(GetWorld()); It; ++It)
	{
		const ADVSoldierCharacter* Frame = *It;
		if (Frame->TeamId == Viewer->TeamId || Frame->IsOutOfAction() || FVector::Dist(Frame->GetActorLocation(), Viewer->GetActorLocation()) > 4000.f)
		{
			continue;
		}
		const FVector Screen = Project(Frame->GetActorLocation() + FVector(0.f, 0.f, 125.f));
		if (Screen.Z <= 0.f)
		{
			continue;
		}
		if (Frame->IsAlerted())
		{
			Text(TEXT("!"), Screen.X, Screen.Y - S(10.f), DVHudColors::Hostile, GEngine->GetMediumFont(), DVType::Emph, true);
		}
		else
		{
			Text(TEXT("- -"), Screen.X, Screen.Y - S(6.f), DVHudColors::A(DVHudColors::Faded, 0.8f), GEngine->GetSmallFont(), DVType::Caption, true);
		}
	}
	if (ADVSoldierCharacter* Victim = Viewer->FindTakedownTarget())
	{
		const FVector Screen = Project(Victim->GetActorLocation() + FVector(0.f, 0.f, 60.f));
		if (Screen.Z > 0.f)
		{
			Text(TEXT("[F]  Silent takedown"), Screen.X, Screen.Y, DVHudColors::Cyan, GEngine->GetMediumFont(), DVType::Emph, true);
		}
		if (ADVPlayerController* PC = Cast<ADVPlayerController>(PlayerOwner))
		{
			PC->LastTakedownPrompt = GetWorld()->GetTimeSeconds();
		}
	}
	if (Viewer->IsSwimming())
	{
		Text(TEXT("SWIMMING  -  [Space] climb out"), Canvas->ClipX * 0.5f, Canvas->ClipY * 0.66f, DVHudColors::Cyan, GEngine->GetSmallFont(), DVType::Body, true);
	}
}

void ADVHUD::DrawContractBoard()
{
	const UDVProfileSubsystem* Profile = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDVProfileSubsystem>() : nullptr;
	if (!Profile)
	{
		return;
	}
	const FLinearColor Amber = DVHudColors::Accent;
	const float X = Canvas->ClipX - S(800.f);	// right side: the squad list and radio own the left
	float Y = Canvas->ClipY * 0.13f;
	bBoardOpen = true;
	const int32 Num = UDVProfileSubsystem::NumContracts();
	DrawRect(DVHudColors::A(DVHudColors::PanelStrong, 0.85f), X - S(20.f), Y - S(20.f), S(780.f), S(112.f) + Num * S(98.f));
	DeckBand(X - S(20.f), Y - S(20.f), S(780.f), S(58.f));
	Text(TEXT("Rook's Rest job board"), X, Y - S(8.f), DVHudColors::DeckInk, GEngine->GetLargeFont(), DVType::Title, false, false, false);
	Text(TEXT("One job at a time. Work it on your next operations; paid when an operation is won."), X, Y + S(44.f), DVHudColors::Dim, GEngine->GetSmallFont(), DVType::Caption);
	Y += S(84.f);
	for (int32 K = 0; K < Num; ++K)
	{
		const FDVContract& C = UDVProfileSubsystem::Contract(K);
		const bool bActive = Profile->GetActiveContract() == K;
		if (bActive)
		{
			DrawRect(DVHudColors::A(DVHudColors::Accent, 0.16f), X - S(10.f), Y - S(6.f), S(760.f), S(92.f));
		}
		Text(FString::Printf(TEXT("%s%s"), bActive ? TEXT("> ") : TEXT(""), C.Name), X, Y, bActive ? Amber : DVHudColors::Text, GEngine->GetMediumFont(), DVType::Emph);
		Text(FString::Printf(TEXT("+%d %s  |  %d XP"), C.Rep, *UDVProfileSubsystem::FactionName(C.Faction), C.XP),
			X + S(740.f), Y + S(4.f), DVHudColors::Friendly, GEngine->GetSmallFont(), DVType::Caption, false, true);
		Text(bActive ? FString::Printf(TEXT("%s   (%d / %d)"), C.Brief, Profile->GetContractProgress(), C.Goal) : FString(C.Brief),
			X, Y + S(38.f), DVHudColors::Text, GEngine->GetSmallFont(), DVType::Caption);
		Text(FString::Printf(TEXT("%s  -  %d %s"), C.Giver, C.Goal, C.Unit), X, Y + S(68.f), DVHudColors::Dim, GEngine->GetSmallFont(), DVType::Caption);
		Y += S(98.f);
	}
	Text(TEXT("[Z] take the next job"), X, Y + S(4.f), Amber, GEngine->GetSmallFont(), DVType::Caption);
}

void ADVHUD::DrawContract()
{
	const UDVProfileSubsystem* Profile = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDVProfileSubsystem>() : nullptr;
	if (!Profile)
	{
		return;
	}
	const float Now = GetWorld()->GetRealTimeSeconds();
	if (!bBoardOpen && Now - Profile->ContractNoteTime < 4.5 + DVMotion::Out && !Profile->ContractNote.IsEmpty())
	{
		float A = 1.f, Rise = 0.f;
		DVMotion::Reveal(Now - Profile->ContractNoteTime, 4.5, A, Rise);
		Text(Profile->ContractNote, Canvas->ClipX * 0.5f, Canvas->ClipY * 0.24f + S(Rise), DVHudColors::A(DVHudColors::Accent, A), GEngine->GetMediumFont(), DVType::Emph, true);
	}
	if (!bBoardOpen && Profile->GetActiveContract() >= 0)	// the open board already shows the active job
	{
		const FDVContract& C = UDVProfileSubsystem::Contract(Profile->GetActiveContract());
		const FString Line = Profile->IsContractDone() ? FString::Printf(TEXT("CONTRACT  %s  -  complete, paid on a win"), C.Name)
			: FString::Printf(TEXT("CONTRACT  %s  -  %d / %d %s"), C.Name, Profile->GetContractProgress(), C.Goal, C.Unit);
		Text(Line, Canvas->ClipX * 0.5f, S(122.f), Profile->IsContractDone() ? DVHudColors::Friendly : DVHudColors::A(DVHudColors::Accent, 0.9f), GEngine->GetSmallFont(), DVType::Caption, true);
	}
}

void ADVHUD::DrawTownsfolk(const ADVSoldierCharacter* Soldier)
{
	const ADVTownsfolk* Local = ADVTownsfolk::FindNear(Soldier);
	const UDVProfileSubsystem* Profile = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDVProfileSubsystem>() : nullptr;
	if (!Local || !Profile)
	{
		return;
	}
	const FVector Screen = Project(Local->GetActorLocation() + FVector(0.f, 0.f, 120.f * Local->GetActorScale3D().Z));
	if (Screen.Z <= 0.f)
	{
		return;
	}
	Text(Local->DisplayName, Screen.X, Screen.Y - S(34.f), DVHudColors::Accent, GEngine->GetSmallFont(), DVType::Caption, true);
	Text(FString::Printf(TEXT("\"%s\""), *Local->Line(Profile->GetRep(DVFactions::FreePorts))), Screen.X, Screen.Y, DVHudColors::Text, GEngine->GetMediumFont(), DVType::Emph, true);
}

void ADVHUD::DrawAscent(const ADVGameState* GS)
{
	// Letterbox bars, a caption and the radio: the flight home plays like a cutscene you're sitting in.
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	DrawRect(DVHudColors::A(DVHudColors::Shade, 0.92f), 0.f, 0.f, W, H * 0.1f);
	DrawRect(DVHudColors::A(DVHudColors::Shade, 0.92f), 0.f, H * 0.9f, W, H * 0.1f);
	const ADVKestrel* Kestrel = ADVKestrel::Find(GetWorld());
	const APawn* Ship = GetOwningPawn();
	const float Dist = Kestrel && Ship ? FVector::Dist(Kestrel->GetHangarTransform().GetLocation(), Ship->GetActorLocation()) / 100.f : 0.f;
	Text(TEXT("RAVEN TWO-ONE  -  RETURNING TO THE KESTREL"), W * 0.5f, H * 0.035f, DVHudColors::Cyan, GEngine->GetMediumFont(), DVType::Emph, true);
	Text(Dist > 60.f ? FString::Printf(TEXT("Starboard bay  %.1f km"), Dist / 1000.f) : FString(TEXT("Coming aboard")), W * 0.5f, H * 0.925f,
		DVHudColors::Dim, GEngine->GetSmallFont(), DVType::Body, true);
	DrawRadio(GS);
}

void ADVHUD::DrawManifold(const ADVPressureManifold* Manifold, int32 NearValve)
{
	// Three dials as five-step ladders: the target step is outlined in green, the current step lit in its warning colour.
	const float W = S(420.f);
	const float X0 = Canvas->ClipX * 0.66f;	// beside the crosshair, clear of the radio subtitles below
	const float Y0 = Canvas->ClipY * 0.4f;
	DrawRect(DVHudColors::Panel, X0, Y0, W, S(196.f));
	Text(TEXT("Steamheart pressure"), X0 + S(20.f), Y0 + S(12.f), DVHudColors::Dim, GEngine->GetSmallFont(), DVType::Caption);
	const float ColW = W / ADVPressureManifold::NumValves;
	for (int32 g = 0; g < ADVPressureManifold::NumValves; ++g)
	{
		const float CX = X0 + ColW * (g + 0.5f);
		const bool bFed = ADVPressureManifold::Feeds(NearValve, g);
		Text(FString::Printf(TEXT("Gauge %d"), g + 1), CX, Y0 + S(40.f), bFed ? DVHudColors::Text : DVHudColors::Dim, GEngine->GetSmallFont(), DVType::Caption, true);
		const int32 Step = Manifold->GetGauge(g);
		for (int32 k = 0; k < ADVPressureManifold::Steps; ++k)
		{
			const float SY = Y0 + S(150.f) - k * S(18.f);
			const FLinearColor Lit = k == ADVPressureManifold::Green ? DVHudColors::Health : k == ADVPressureManifold::Steps - 1 ? DVHudColors::Hostile : DVHudColors::Accent;
			DrawRect(k == Step ? Lit : DVHudColors::A(DVHudColors::Text, 0.12f), CX - S(36.f), SY, S(72.f), S(12.f));
			if (k == ADVPressureManifold::Green && k != Step)
			{
				DrawRect(DVHudColors::A(DVHudColors::Health, 0.55f), CX - S(40.f), SY + S(4.f), S(4.f), S(4.f));
				DrawRect(DVHudColors::A(DVHudColors::Health, 0.55f), CX + S(36.f), SY + S(4.f), S(4.f), S(4.f));
			}
		}
	}
	Text(FString::Printf(TEXT("Valve %s feeds %s"), *ADVPressureManifold::ValveName(NearValve), NearValve == 1 ? TEXT("all three gauges") : NearValve == 0 ? TEXT("gauges 1 and 2") : TEXT("gauges 2 and 3")),
		X0 + W * 0.5f, Y0 + S(168.f), DVHudColors::Warn, GEngine->GetSmallFont(), DVType::Caption, true);
}
