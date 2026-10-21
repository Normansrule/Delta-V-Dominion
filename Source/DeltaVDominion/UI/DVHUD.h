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
	void DrawContractBoard();
	void DrawContract();
	void DrawAscent(const class ADVGameState* GS);
	void DrawTownsfolk(const ADVSoldierCharacter* Soldier);
	void DrawAwareness(const class ADVSoldierCharacter* Viewer);
	void DrawCockpit(const class ADVGunship* Air);
	void DrawProgression();
	void DrawRadio(const ADVGameState* GS);
	void DrawOrbital(const ADVGameState* GS);
	void BindOrders(ADVSoldierCharacter* Soldier);

	// Helpers (all positions in 1080p reference units, scaled to the viewport).
	float S(float V) const { return V * UIScale; }
	void Text(const FString& Str, float X, float Y, const FLinearColor& Color, UFont* Font, float Scale = 1.f, bool bCentre = false, bool bRight = false, bool bShadow = true);
	/** A painted deck marking (flat amber fill by default); text on it is drawn in DeckInk without a shadow. */
	void DeckBand(float X, float Y, float W, float H, const FLinearColor& Fill = FLinearColor(0.888f, 0.434f, 0.012f, 1.f));
	void Bar(float X, float Y, float W, float H, float Fraction, const FLinearColor& Fill);

	float UIScale = 1.f;
	/** Set while the job board is open this frame, so toasts that would sit under it stay away. */
	bool bBoardOpen = false;
	TWeakObjectPtr<UDVCommandComponent> BoundCommand;
	FString OrderToast;
	double OrderToastTime = -100.0;
};
