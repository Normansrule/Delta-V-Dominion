#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Characters/DVSquadTypes.h"
#include "DVHUD.generated.h"

class ADVSoldierCharacter;
class ADVGameState;
class UDVCommandComponent;
class UFont;

/**
 * Combat HUD drawn straight to the canvas, so it needs no widget assets:
 * crosshair (sized by real weapon spread), hit markers, health/armour, ammo/heat,
 * suppression vignette, squad roster, objective, kill feed, order toasts,
 * respawn countdown and the after-action report.
 *
 * Built to be replaced piece by piece by CommonUI widgets in the UI pass.
 */
UCLASS()
class DELTAVDOMINION_API ADVHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

protected:
	UFUNCTION()
	void OnOrderIssued(EDVSquadOrder Order, FVector Location);

private:
	void DrawCrosshair(const ADVSoldierCharacter* Soldier);
	void DrawHitMarker();
	void DrawVitals(const ADVSoldierCharacter* Soldier);
	void DrawAmmo(const ADVSoldierCharacter* Soldier);
	void DrawSuppression(const ADVSoldierCharacter* Soldier);
	void DrawSquad(const ADVSoldierCharacter* Soldier);
	void DrawObjective(const ADVGameState* GS);
	void DrawKillFeed(const ADVGameState* GS);
	void DrawOrderToast();
	void DrawDeathScreen();
	void DrawAfterAction(const ADVGameState* GS);
	void DrawTitleCard(const ADVGameState* GS);
	void DrawVehicle(APawn* Vehicle);
	void DrawVehiclePrompt(const ADVSoldierCharacter* Soldier);
	void DrawMemorial();
	void DrawCallIns(const class ADVGameState* GS);
	void DrawHeavies();
	void DrawCasualties(const class ADVSoldierCharacter* Viewer);
	void DrawDowned(const class ADVSoldierCharacter* Soldier);
	void DrawThrusters(const class ADVSoldierCharacter* Soldier);
	void DrawLocker();
	void DrawCockpit(const class ADVGunship* Air);
	void DrawProgression();
	void DrawRadio(const ADVGameState* GS);
	void DrawOrbital(const ADVGameState* GS);
	void BindOrders(ADVSoldierCharacter* Soldier);

	// Helpers (all positions in 1080p reference units, scaled to the viewport).
	float S(float V) const { return V * UIScale; }
	void Text(const FString& Str, float X, float Y, const FLinearColor& Color, UFont* Font, float Scale = 1.f, bool bCentre = false, bool bRight = false);
	void Bar(float X, float Y, float W, float H, float Fraction, const FLinearColor& Fill);

	float UIScale = 1.f;
	TWeakObjectPtr<UDVCommandComponent> BoundCommand;
	FString OrderToast;
	double OrderToastTime = -100.0;
};
