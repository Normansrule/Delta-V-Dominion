#include "UI/DVUIStyle.h"
#include "GameFramework/HUD.h"
#include "Engine/Font.h"

namespace DVUIStyleImpl	// named: unity builds merge anonymous namespaces
{
	const FLinearColor Ink(0.94f, 0.95f, 0.97f, 1.f);
	const FLinearColor Glass(0.004f, 0.006f, 0.009f, 0.72f);
	const FLinearColor Hair(1.f, 1.f, 1.f, 0.16f);
	const FLinearColor CapFill(0.92f, 0.93f, 0.95f, 0.95f);
	const FLinearColor CapInk(0.02f, 0.022f, 0.026f, 1.f);

	struct FRun
	{
		FString Str;
		bool bKey = false;
	};

	TArray<FRun> Split(const FString& Str)
	{
		TArray<FRun> Runs;
		FString Plain;
		for (int32 i = 0; i < Str.Len(); ++i)
		{
			if (Str[i] == TEXT('['))
			{
				const int32 Close = Str.Find(TEXT("]"), ESearchCase::CaseSensitive, ESearchDir::FromStart, i + 1);
				if (Close > i + 1 && Close - i - 1 <= 5)
				{
					if (!Plain.IsEmpty())
					{
						Runs.Add({ Plain, false });
						Plain.Reset();
					}
					Runs.Add({ Str.Mid(i + 1, Close - i - 1), true });
					i = Close;
					continue;
				}
			}
			Plain.AppendChar(Str[i]);
		}
		if (!Plain.IsEmpty())
		{
			Runs.Add({ Plain, false });
		}
		return Runs;
	}

	float CapWidth(AHUD* Hud, const DVUI::FStyle& S, const FString& Key, UFont* Font, float& OutH)
	{
		float W = 0.f, H = 0.f;
		Hud->GetTextSize(Key, W, H, Font, S.TextScale * 0.82f);
		OutH = H;
		return FMath::Max(W + 14.f * S.Pixel, H + 6.f * S.Pixel);
	}
}

float DVUI::RichWidth(AHUD* Hud, const FStyle& Style, const FString& Str, UFont* Font)
{
	float Total = 0.f;
	for (const DVUIStyleImpl::FRun& Run : DVUIStyleImpl::Split(Str))
	{
		if (Run.bKey)
		{
			float H = 0.f;
			Total += DVUIStyleImpl::CapWidth(Hud, Style, Run.Str, Font, H) + 8.f * Style.Pixel;
		}
		else
		{
			float W = 0.f, H = 0.f;
			Hud->GetTextSize(Run.Str.TrimStart(), W, H, Font, Style.TextScale);
			Total += W;
		}
	}
	return Total;
}

float DVUI::RichText(AHUD* Hud, const FStyle& Style, const FString& Str, float X, float Y, const FLinearColor& Color, UFont* Font, bool bCentre, bool bRight, bool bShadow)
{
	const float Width = RichWidth(Hud, Style, Str, Font);
	float Cursor = bCentre ? X - Width * 0.5f : bRight ? X - Width : X;
	float LineW = 0.f, LineH = 0.f;
	Hud->GetTextSize(TEXT("Ag"), LineW, LineH, Font, Style.TextScale);
	for (const DVUIStyleImpl::FRun& Run : DVUIStyleImpl::Split(Str))
	{
		if (Run.bKey)
		{
			// A keycap: a light key with dark ink, a darker lip along the bottom, the size of the line it sits in.
			float KeyH = 0.f;
			const float CapW = DVUIStyleImpl::CapWidth(Hud, Style, Run.Str, Font, KeyH);
			const float CapH = LineH * 0.92f;
			const float CapY = Y + (LineH - CapH) * 0.5f;
			const FLinearColor Fill = DVUIStyleImpl::CapFill.CopyWithNewOpacity(DVUIStyleImpl::CapFill.A * Color.A);
			Hud->DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f * Color.A), Cursor + Style.Pixel, CapY + 2.f * Style.Pixel, CapW, CapH);
			Hud->DrawRect(Fill, Cursor, CapY, CapW, CapH);
			Hud->DrawRect(FLinearColor(0.55f, 0.57f, 0.6f, Color.A), Cursor, CapY + CapH - 2.f * Style.Pixel, CapW, 2.f * Style.Pixel);
			float TW = 0.f, TH = 0.f;
			Hud->GetTextSize(Run.Str, TW, TH, Font, Style.TextScale * 0.82f);
			Hud->DrawText(Run.Str, DVUIStyleImpl::CapInk.CopyWithNewOpacity(Color.A), Cursor + (CapW - TW) * 0.5f, CapY + (CapH - TH) * 0.5f - Style.Pixel, Font, Style.TextScale * 0.82f);
			Cursor += CapW + 8.f * Style.Pixel;
		}
		else
		{
			const FString Part = Run.Str.TrimStart();
			if (bShadow)
			{
				Hud->DrawText(Part, FLinearColor(0.f, 0.f, 0.f, 0.6f * Color.A), Cursor + 1.5f * Style.Pixel, Y + 1.5f * Style.Pixel, Font, Style.TextScale);
			}
			Hud->DrawText(Part, Color, Cursor, Y, Font, Style.TextScale);
			float W = 0.f, H = 0.f;
			Hud->GetTextSize(Part, W, H, Font, Style.TextScale);
			Cursor += W;
		}
	}
	return Width;
}

void DVUI::Panel(AHUD* Hud, const FStyle& Style, float X, float Y, float W, float H, const FLinearColor& Accent)
{
	const float P = Style.Pixel;
	Hud->DrawRect(DVUIStyleImpl::Glass, X, Y, W, H);
	// Hairline border.
	Hud->DrawRect(DVUIStyleImpl::Hair, X, Y, W, P);
	Hud->DrawRect(DVUIStyleImpl::Hair, X, Y + H - P, W, P);
	Hud->DrawRect(DVUIStyleImpl::Hair, X, Y, P, H);
	Hud->DrawRect(DVUIStyleImpl::Hair, X + W - P, Y, P, H);
	// Accent rule along the top and bright corner ticks.
	Hud->DrawRect(Accent, X, Y, W * 0.22f, 3.f * P);
	const float T = 14.f * P;
	const FLinearColor Tick = DVUIStyleImpl::Ink.CopyWithNewOpacity(0.8f);
	Hud->DrawRect(Tick, X + W - T, Y + H - 2.f * P, T, 2.f * P);
	Hud->DrawRect(Tick, X + W - 2.f * P, Y + H - T, 2.f * P, T);
	Hud->DrawRect(Tick, X, Y + H - 2.f * P, T, 2.f * P);
	Hud->DrawRect(Tick, X, Y + H - T, 2.f * P, T);
}

void DVUI::Button(AHUD* Hud, const FStyle& Style, const FBox2D& Box, const FString& Label, const FLinearColor& Accent, UFont* Font, bool bHover, const FString& Key)
{
	const float P = Style.Pixel;
	const FVector2D Size = Box.GetSize();
	// Resting: dark glass, an accent spine and a hairline. Hover: the accent floods the button and the ink goes dark.
	Hud->DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), Box.Min.X + 2.f * P, Box.Min.Y + 3.f * P, Size.X, Size.Y);
	Hud->DrawRect(bHover ? Accent : DVUIStyleImpl::Glass, Box.Min.X, Box.Min.Y, Size.X, Size.Y);
	Hud->DrawRect(Accent, Box.Min.X, Box.Min.Y, 5.f * P, Size.Y);
	const FLinearColor Edge = bHover ? FLinearColor(1.f, 1.f, 1.f, 0.7f) : Accent.CopyWithNewOpacity(0.55f);
	Hud->DrawRect(Edge, Box.Min.X, Box.Min.Y, Size.X, P);
	Hud->DrawRect(Edge, Box.Min.X, Box.Max.Y - P, Size.X, P);
	Hud->DrawRect(Edge, Box.Max.X - P, Box.Min.Y, P, Size.Y);
	const FLinearColor InkC = bHover ? FLinearColor(0.03f, 0.025f, 0.015f, 1.f) : DVUIStyleImpl::Ink;
	float TW = 0.f, TH = 0.f;
	Hud->GetTextSize(Label, TW, TH, Font, Style.TextScale);
	const FString Full = Key.IsEmpty() ? Label : FString::Printf(TEXT("[%s] %s"), *Key, *Label);
	const float LabelW = RichWidth(Hud, Style, Full, Font);
	const float Chevron = bHover ? 10.f * P : 0.f;
	RichText(Hud, Style, Full, Box.GetCenter().X - Chevron * 0.5f - LabelW * 0.5f + 2.f * P, Box.GetCenter().Y - TH * 0.5f, InkC, Font, false, false, !bHover);
	if (bHover)
	{
		// A chevron points the way: two short strokes.
		const float CX = Box.Max.X - 18.f * P, CY = Box.GetCenter().Y;
		Hud->DrawLine(CX - 5.f * P, CY - 6.f * P, CX + P, CY, InkC, 2.f * P);
		Hud->DrawLine(CX + P, CY, CX - 5.f * P, CY + 6.f * P, InkC, 2.f * P);
	}
}
