#pragma once

#include "CoreMinimal.h"

class AHUD;
class UFont;

/**
 * The shared look of every on-screen control, after the console shooters it borrows from: dark glass panels with a
 * hairline border and corner ticks, buttons with a solid accent spine that fill on hover, and key prompts drawn as
 * keycaps instead of "[Z]" in brackets.
 */
namespace DVUI
{
	struct FStyle
	{
		float Pixel = 1.f;			// one design pixel at the current UI scale
		float TextScale = 1.f;		// the HUD's font scale (already including its UI scale)
	};

	/** Draws Str, turning every [K] token (1-5 characters) into a keycap. Returns the drawn width. */
	DELTAVDOMINION_API float RichText(AHUD* Hud, const FStyle& Style, const FString& Str, float X, float Y, const FLinearColor& Color, UFont* Font,
		bool bCentre = false, bool bRight = false, bool bShadow = true);

	/** Width RichText would draw Str at. */
	DELTAVDOMINION_API float RichWidth(AHUD* Hud, const FStyle& Style, const FString& Str, UFont* Font);

	/** A glass panel: translucent fill, hairline border, corner ticks and an accent rule along the top. */
	DELTAVDOMINION_API void Panel(AHUD* Hud, const FStyle& Style, float X, float Y, float W, float H, const FLinearColor& Accent);

	/** A button: accent spine on the left, fills with the accent on hover, a chevron that slides in. */
	DELTAVDOMINION_API void Button(AHUD* Hud, const FStyle& Style, const FBox2D& Box, const FString& Label, const FLinearColor& Accent, UFont* Font,
		bool bHover, const FString& Key = FString());
}
