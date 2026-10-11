#include "UI/DVHUD.h"
#include "Characters/DVSoldierCharacter.h"
#include "Characters/DVCommandComponent.h"
#include "Combat/DVAttributeSet.h"
#include "Combat/DVWeaponBase.h"
#include "Combat/DVWeaponData.h"
#include "Core/DVGameState.h"
#include "Core/DVPlayerController.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "EngineUtils.h"

namespace DVHudColors
{
	const FLinearColor Text(0.92f, 0.94f, 0.96f, 1.f);
	const FLinearColor Dim(0.6f, 0.65f, 0.7f, 1.f);
	const FLinearColor Panel(0.02f, 0.03f, 0.05f, 0.55f);
	const FLinearColor Friendly(0.35f, 0.6f, 1.f, 1.f);
	const FLinearColor Hostile(1.f, 0.35f, 0.25f, 1.f);
	const FLinearColor Health(0.35f, 0.9f, 0.45f, 1.f);
	const FLinearColor Armor(0.4f, 0.75f, 1.f, 1.f);
	const FLinearColor Warn(1.f, 0.75f, 0.2f, 1.f);
	const FLinearColor Track(1.f, 1.f, 1.f, 0.12f);
}

static FString OrderName(EDVSquadOrder Order)
{
	switch (Order)
	{
	case EDVSquadOrder::Follow:   return TEXT("FOLLOW ME");
	case EDVSquadOrder::MoveTo:   return TEXT("MOVE TO POSITION");
	case EDVSquadOrder::Hold:     return TEXT("HOLD POSITION");
	case EDVSquadOrder::Flank:    return TEXT("FLANK TARGET");
	case EDVSquadOrder::Suppress: return TEXT("SUPPRESSING FIRE");
	case EDVSquadOrder::Regroup:  return TEXT("REGROUP ON ME");
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

	const ADVGameState* GS = GetWorld()->GetGameState<ADVGameState>();
	ADVSoldierCharacter* Soldier = Cast<ADVSoldierCharacter>(GetOwningPawn());
	const bool bViewingSoldier = Soldier && PlayerOwner && PlayerOwner->GetViewTarget() == Soldier;

	if (GS && GS->Phase != EDVMissionPhase::InProgress)
	{
		DrawAfterAction(GS);
		return;
	}

	if (bViewingSoldier && !Soldier->IsDead())
	{
		BindOrders(Soldier);
		DrawSuppression(Soldier);
		DrawCrosshair(Soldier);
		DrawHitMarker();
		DrawVitals(Soldier);
		DrawAmmo(Soldier);
		DrawOrderToast();
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

void ADVHUD::Text(const FString& Str, float X, float Y, const FLinearColor& Color, UFont* Font, float Scale, bool bCentre, bool bRight)
{
	Scale *= 1.3f;	// engine fonts are small at 1080p
	float W = 0.f, H = 0.f;
	GetTextSize(Str, W, H, Font, Scale * UIScale);
	const float DrawX = bCentre ? X - W * 0.5f : bRight ? X - W : X;
	// Drop shadow for legibility over bright sky.
	DrawText(Str, FLinearColor(0.f, 0.f, 0.f, 0.6f * Color.A), DrawX + S(1.5f), Y + S(1.5f), Font, Scale * UIScale);
	DrawText(Str, Color, DrawX, Y, Font, Scale * UIScale);
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

	const FLinearColor Color = Soldier->IsAiming() ? FLinearColor(1.f, 1.f, 1.f, 0.55f) : FLinearColor(1.f, 1.f, 1.f, 0.9f);
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
	const FLinearColor Color = PC->WasLastHitKill() ? FLinearColor(1.f, 0.25f, 0.2f, Alpha)
		: PC->WasLastHitHeadshot() ? FLinearColor(1.f, 0.85f, 0.2f, Alpha) : FLinearColor(1.f, 1.f, 1.f, Alpha);
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
	Text(FString::Printf(TEXT("%d"), FMath::CeilToInt(Health)), X, Y - S(4.f), HealthColor, GEngine->GetLargeFont(), 1.4f);
	Text(TEXT("HEALTH"), X + S(70.f), Y + S(4.f), DVHudColors::Dim, GEngine->GetSmallFont(), 1.1f);
	Bar(X + S(70.f), Y + S(26.f), S(250.f), S(8.f), Health / MaxHealth, HealthColor);

	Text(TEXT("ARMOUR"), X + S(70.f), Y + S(40.f), DVHudColors::Dim, GEngine->GetSmallFont(), 1.1f);
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
	Text(WeaponName, Right, Y + S(44.f), DVHudColors::Dim, GEngine->GetSmallFont(), 1.1f, false, true);

	if (Data->bUsesHeat)
	{
		const float Heat = FMath::Clamp(Weapon->GetHeat(), 0.f, 1.f);	// 0..1
		const FLinearColor Color = Weapon->IsOverheated() ? DVHudColors::Hostile : Heat > 0.7f ? DVHudColors::Warn : DVHudColors::Text;
		Text(Weapon->IsOverheated() ? TEXT("OVERHEATED") : FString::Printf(TEXT("HEAT %d%%"), FMath::RoundToInt(Heat * 100.f)),
			Right, Y - S(2.f), Color, GEngine->GetLargeFont(), 1.2f, false, true);
		Bar(Right - S(250.f), Y + S(36.f), S(250.f), S(5.f), Heat, Color);
		return;
	}

	const int32 Mag = Weapon->GetAmmoInMag();
	const bool bLow = Mag <= Data->MagazineSize / 4;
	const FString Reserve = FString::Printf(TEXT(" / %d"), Weapon->GetReserveAmmo());
	float ReserveW = 0.f, ReserveH = 0.f;
	GetTextSize(Reserve, ReserveW, ReserveH, GEngine->GetMediumFont(), 1.2f * 1.3f * UIScale);
	Text(Reserve, Right, Y + S(10.f), DVHudColors::Dim, GEngine->GetMediumFont(), 1.2f, false, true);
	Text(FString::FromInt(Mag), Right - ReserveW, Y - S(6.f), bLow ? DVHudColors::Warn : DVHudColors::Text, GEngine->GetLargeFont(), 1.5f, false, true);
	if (Weapon->NeedsReload())
	{
		Text(Weapon->CanReload() ? TEXT("RELOAD [R]") : TEXT("NO AMMO"), Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f + S(48.f),
			DVHudColors::Warn, GEngine->GetMediumFont(), 1.f, true);
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
		const FLinearColor Color(0.12f, 0.f, 0.f, 0.12f * Amount);
		DrawRect(Color, 0.f, 0.f, W, Inset * 0.6f);
		DrawRect(Color, 0.f, H - Inset * 0.6f, W, Inset * 0.6f);
		DrawRect(Color, 0.f, 0.f, Inset, H);
		DrawRect(Color, W - Inset, 0.f, Inset, H);
	}
	if (Amount > 0.6f)
	{
		Text(TEXT("PINNED"), Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f - S(70.f), DVHudColors::Hostile, GEngine->GetMediumFont(), 1.f, true);
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
	Text(TEXT("SQUAD"), X, Y, DVHudColors::Dim, GEngine->GetSmallFont(), 1.1f);
	Y += S(30.f);

	for (const ADVSoldierCharacter* Mate : Squad)
	{
		const bool bDead = Mate->IsDead();
		const UDVAttributeSet* Attr = Mate->GetAttributeSet();
		const float Health = Attr ? Attr->GetHealth() / FMath::Max(1.f, Attr->GetMaxHealth()) : 0.f;
		Text(Mate->GetSoldierName(), X, Y, bDead ? FLinearColor(0.5f, 0.5f, 0.5f, 0.8f) : DVHudColors::Friendly, GEngine->GetSmallFont(), 1.15f);
		if (bDead)
		{
			Text(TEXT("KIA"), X + S(412.f), Y, DVHudColors::Hostile, GEngine->GetSmallFont(), 1.15f, false, true);
		}
		else
		{
			Bar(X, Y + S(28.f), S(412.f), S(4.f), Health, Health < 0.3f ? DVHudColors::Hostile : DVHudColors::Health);
			if (Mate->GetSuppression() > 60.f)
			{
				Text(TEXT("PINNED"), X + S(412.f), Y, DVHudColors::Warn, GEngine->GetSmallFont(), 1.f, false, true);
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
	Text(FString::Printf(TEXT("%s  -  %s"), *GS->MissionName.ToUpper(), *Clock), CX, Y, DVHudColors::Dim, GEngine->GetSmallFont(), 1.1f, true);
	Text(GS->ObjectiveText, CX, Y + S(26.f), DVHudColors::Text, GEngine->GetMediumFont(), 1.f, true);
	const int32 Down = GS->EnemiesTotal - GS->EnemiesRemaining;
	Bar(CX - S(200.f), Y + S(62.f), S(400.f), S(4.f), GS->EnemiesTotal > 0 ? static_cast<float>(Down) / GS->EnemiesTotal : 0.f, DVHudColors::Hostile);
	Text(FString::Printf(TEXT("%d / %d"), Down, GS->EnemiesTotal), CX + S(310.f), Y + S(52.f), DVHudColors::Dim, GEngine->GetSmallFont(), 1.f, false, true);
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
		const FString Line = Entry.Killer.IsEmpty() ? FString::Printf(TEXT("%s killed"), *Entry.Victim)
			: FString::Printf(TEXT("%s  >  %s"), *Entry.Killer, *Entry.Victim);
		Text(Line, Right, Y, Color, GEngine->GetSmallFont(), 1.15f, false, true);
		Y += S(26.f);
	}
}

void ADVHUD::DrawOrderToast()
{
	const double Age = GetWorld()->GetTimeSeconds() - OrderToastTime;
	if (Age > 2.0 || OrderToast.IsEmpty())
	{
		return;
	}
	const float Alpha = Age > 1.5 ? 1.f - static_cast<float>((Age - 1.5) / 0.5) : 1.f;
	FLinearColor Color = DVHudColors::Friendly;
	Color.A = Alpha;
	Text(FString::Printf(TEXT("SQUAD: %s"), *OrderToast), Canvas->ClipX * 0.5f, Canvas->ClipY * 0.68f, Color, GEngine->GetMediumFont(), 1.1f, true);
}

void ADVHUD::DrawDeathScreen()
{
	DrawRect(FLinearColor(0.1f, 0.f, 0.f, 0.35f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
	Text(TEXT("YOU ARE DOWN"), Canvas->ClipX * 0.5f, Canvas->ClipY * 0.42f, DVHudColors::Hostile, GEngine->GetLargeFont(), 2.f, true);
	Text(TEXT("Reinforcing on your squad..."), Canvas->ClipX * 0.5f, Canvas->ClipY * 0.42f + S(64.f), DVHudColors::Text, GEngine->GetMediumFont(), 1.f, true);
}

void ADVHUD::DrawAfterAction(const ADVGameState* GS)
{
	const bool bWin = GS->Phase == EDVMissionPhase::Success;
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float CX = W * 0.5f;
	DrawRect(FLinearColor(0.01f, 0.015f, 0.025f, 0.82f), 0.f, 0.f, W, H);

	float Y = H * 0.16f;
	Text(bWin ? TEXT("MISSION ACCOMPLISHED") : TEXT("SQUAD LOST"), CX, Y, bWin ? DVHudColors::Health : DVHudColors::Hostile, GEngine->GetLargeFont(), 2.4f, true);
	Y += S(84.f);
	const float Duration = GS->MissionEndTime - GS->MissionStartTime;
	Text(FString::Printf(TEXT("%s  |  %d of %d hostiles neutralised  |  %02d:%02d"), *GS->MissionName.ToUpper(),
		GS->EnemiesTotal - GS->EnemiesRemaining, GS->EnemiesTotal, FMath::FloorToInt(Duration / 60.f), FMath::FloorToInt(FMath::Fmod(Duration, 60.f))),
		CX, Y, DVHudColors::Dim, GEngine->GetMediumFont(), 1.f, true);

	Y += S(70.f);
	const float Left = CX - S(420.f);
	const float RightCol = CX + S(420.f);
	Text(TEXT("AFTER-ACTION REPORT"), Left, Y, DVHudColors::Dim, GEngine->GetSmallFont(), 1.2f);
	Y += S(34.f);
	DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.15f), Left, Y, S(840.f), FMath::Max(1.f, S(1.5f)));
	Y += S(14.f);

	for (const FDVAfterActionLine& Line : GS->AfterAction)
	{
		const FLinearColor NameColor = Line.bKIA ? FLinearColor(0.55f, 0.55f, 0.55f, 1.f) : DVHudColors::Text;
		Text(FString::Printf(TEXT("%s %s"), *Line.Rank, *Line.Name), Left, Y, NameColor, GEngine->GetMediumFont(), 1.f);
		const FString Status = Line.bKIA ? TEXT("KILLED IN ACTION") : Line.bWounded ? TEXT("WOUNDED  -  MEDBAY")
			: Line.bPromoted ? TEXT("PROMOTED") : TEXT("RETURNED TO DUTY");
		const FLinearColor StatusColor = Line.bKIA ? DVHudColors::Hostile : Line.bWounded ? DVHudColors::Warn
			: Line.bPromoted ? DVHudColors::Health : DVHudColors::Dim;
		Text(Status, RightCol, Y, StatusColor, GEngine->GetMediumFont(), 1.f, false, true);
		Text(FString::Printf(TEXT("%d career kills"), Line.Kills), CX + S(40.f), Y + S(4.f), DVHudColors::Dim, GEngine->GetSmallFont(), 1.1f, true);
		Y += S(54.f);
	}

	if (GS->AfterAction.ContainsByPredicate([](const FDVAfterActionLine& L) { return L.bKIA; }))
	{
		Y += S(12.f);
		Text(TEXT("The fallen are added to the Memorial Wall. Replacements are greener."), CX, Y, DVHudColors::Dim, GEngine->GetSmallFont(), 1.15f, true);
	}

	const float Pulse = 0.6f + 0.4f * FMath::Sin(GetWorld()->GetRealTimeSeconds() * 3.f);
	Text(TEXT("ENTER  redeploy        M  strategic map"), CX, H * 0.86f, FLinearColor(1.f, 1.f, 1.f, Pulse), GEngine->GetMediumFont(), 1.2f, true);
}

void ADVHUD::DrawTitleCard(const ADVGameState* GS)
{
	if (!GS || GS->MissionLocation.IsEmpty())
	{
		return;
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
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.85f * Alpha), 0.f, 0.f, W, H * 0.1f);
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.85f * Alpha), 0.f, H * 0.9f, W, H * 0.1f);

	const float X = S(90.f);
	const float Y = H * 0.62f;
	DrawRect(FLinearColor(1.f, 0.45f, 0.15f, Alpha), X - S(18.f), Y + S(4.f), FMath::Max(2.f, S(4.f)), S(118.f));
	Text(GS->MissionName.ToUpper(), X, Y, FLinearColor(1.f, 1.f, 1.f, Alpha), GEngine->GetLargeFont(), 2.4f);
	Text(GS->MissionLocation, X, Y + S(70.f), FLinearColor(1.f, 0.62f, 0.3f, Alpha), GEngine->GetMediumFont(), 1.25f);
	Text(GS->MissionSubtitle, X, Y + S(104.f), FLinearColor(0.75f, 0.8f, 0.85f, Alpha), GEngine->GetSmallFont(), 1.2f);
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
	if (Age > Hold + 0.5)
	{
		return;
	}
	const float Alpha = Age > Hold ? 1.f - static_cast<float>((Age - Hold) / 0.5) : 1.f;
	const bool bCinder = Line.Speaker == TEXT("CINDER");
	const FLinearColor SpeakerColor = bCinder ? FLinearColor(1.f, 0.2f, 0.12f, Alpha) : FLinearColor(0.45f, 0.8f, 1.f, Alpha);
	const float CX = Canvas->ClipX * 0.5f;
	const float Y = Canvas->ClipY * 0.8f;

	float TW = 0.f, TH = 0.f;
	GetTextSize(Line.Text, TW, TH, GEngine->GetMediumFont(), 1.3f * UIScale);
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f * Alpha), CX - TW * 0.5f - S(20.f), Y - S(30.f), TW + S(40.f), S(70.f));
	Text(Line.Speaker.ToUpper(), CX, Y - S(24.f), SpeakerColor, GEngine->GetSmallFont(), 1.05f, true);
	Text(Line.Text, CX, Y, FLinearColor(1.f, 1.f, 1.f, Alpha), GEngine->GetMediumFont(), 1.f, true);
}

void ADVHUD::DrawOrbital(const ADVGameState* GS)
{
	if (!GS || GS->Phase != EDVMissionPhase::InProgress)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	const float Remaining = GS->OrbitalReadyTime - Now;
	const float CX = Canvas->ClipX * 0.5f;
	const float Y = Canvas->ClipY - S(70.f);
	if (Remaining <= 0.f)
	{
		const float Pulse = 0.7f + 0.3f * FMath::Sin(GetWorld()->GetRealTimeSeconds() * 4.f);
		Text(TEXT("ORBITAL STRIKE READY  [B]"), CX, Y, FLinearColor(1.f, 0.5f, 0.2f, Pulse), GEngine->GetMediumFont(), 1.f, true);
	}
	else
	{
		Text(FString::Printf(TEXT("ORBITAL STRIKE  %ds"), FMath::CeilToInt(Remaining)), CX, Y, DVHudColors::Dim, GEngine->GetSmallFont(), 1.2f, true);
	}

	// Countdown over the painted target.
	FVector Target;
	double ImpactTime = 0.0;
	if (GS->GetPendingStrike(Target, ImpactTime))
	{
		const FVector Screen = Project(Target + FVector(0.f, 0.f, 150.f));
		if (Screen.Z > 0.f)
		{
			const float T = FMath::Max(0.0, ImpactTime - Now);
			Text(FString::Printf(TEXT("IMPACT %.1f"), T), Screen.X, Screen.Y, FLinearColor(1.f, 0.2f, 0.1f, 1.f), GEngine->GetMediumFont(), 1.1f, true);
		}
	}
}
