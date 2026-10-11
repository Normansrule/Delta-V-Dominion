#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/HUD.h"
#include "Core/DVPlayerController.h"
#include "DVGalaxy.generated.h"

class UCameraComponent;
class ADVStarSystem;
class UFont;

/**
 * Strategic map of the Milky Way (the game's start screen). Orbit, pan and zoom the galaxy,
 * select star systems and launch their operations. Map: /Game/Maps/Galaxy (Scripts/build_galaxy_map.py).
 */
UCLASS()
class DELTAVDOMINION_API ADVGalaxyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADVGalaxyGameMode();
};

/** Free camera rig for the galaxy map; the controller drives it. */
UCLASS()
class DELTAVDOMINION_API ADVGalaxyPawn : public APawn
{
	GENERATED_BODY()

public:
	ADVGalaxyPawn();

	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
};

UCLASS()
class DELTAVDOMINION_API ADVGalaxyPlayerController : public ADVPlayerController
{
	GENERATED_BODY()

public:
	ADVGalaxyPlayerController();

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

	/** Console / capture: "DVGalaxyView N" 0 = whole galaxy, 1 = Sol and Gaia, 2 = galactic core, 3 = Orion arm. */
	UFUNCTION(Exec)
	void DVGalaxyView(int32 Preset);

	/** Console / capture: "DVSelectSystem Name" selects and flies to a star system. */
	UFUNCTION(Exec)
	void DVSelectSystem(const FString& Name);

	ADVStarSystem* GetSelected() const { return Selected.Get(); }
	ADVStarSystem* GetHovered() const { return Hovered.Get(); }
	const TArray<TWeakObjectPtr<ADVStarSystem>>& GetSystems() const { return Systems; }
	float GetCameraDistance() const { return Distance; }

private:
	void Select(ADVStarSystem* System, bool bFly);
	ADVStarSystem* PickUnderCursor() const;
	void LaunchOperation(int32 Index);
	void Zoom(float Factor);
	void OnScrollUp() { Zoom(0.85f); }
	void OnScrollDown() { Zoom(1.18f); }
	void OnLeftPressed();
	void OnLeftReleased();
	void OnHome() { DVGalaxyView(1); }
	void OnOverview() { DVGalaxyView(0); }
	void Launch1() { LaunchOperation(0); }
	void Launch2() { LaunchOperation(1); }
	void Launch3() { LaunchOperation(2); }
	void Launch4() { LaunchOperation(3); }
	void Launch5() { LaunchOperation(4); }
	void Launch6() { LaunchOperation(5); }
	void Launch7() { LaunchOperation(6); }
	void Launch8() { LaunchOperation(7); }

	TArray<TWeakObjectPtr<ADVStarSystem>> Systems;
	TWeakObjectPtr<ADVStarSystem> Selected;
	TWeakObjectPtr<ADVStarSystem> Hovered;

	// Orbit camera: current and target values, eased every frame.
	FVector Focus = FVector::ZeroVector, TargetFocus = FVector::ZeroVector;
	float Distance = 90000.f, TargetDistance = 90000.f;
	float Yaw = -60.f, Pitch = -38.f;
	bool bLeftDown = false;
	float DragAmount = 0.f;
	bool bAutoSpin = true;
};

UCLASS()
class DELTAVDOMINION_API ADVGalaxyHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	float S(float V) const { return V * UIScale; }
	void Text(const FString& Str, float X, float Y, const FLinearColor& Color, UFont* Font, float Scale = 1.f, bool bCentre = false, bool bRight = false);
	float UIScale = 1.f;
};
