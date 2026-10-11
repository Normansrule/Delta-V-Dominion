#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "DVPlayerController.generated.h"

/** Player controller. Extension point for the command wheel, HUD and anti-cheat hooks. */
UCLASS()
class DELTAVDOMINION_API ADVPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ADVPlayerController();

	/**
	 * Console: "DVShowcase [N]". Cuts to the map's CameraActor tagged "ShowcaseN" and, after a
	 * moment, orders the squad to flank the nearest enemy, so a fight plays out on camera.
	 * Used for trailers/README captures; "DVShowcase -1" returns to the soldier's view.
	 */
	UFUNCTION(Exec)
	void DVShowcase(int32 CameraIndex = 0);

	/**
	 * Console: "DVCapture Name Frames [Interval] [Delay]". After Delay seconds, saves Frames viewport screenshots to
	 * Saved/Screenshots/DVCapture/Name_0000.png, one every Interval seconds (0 = every frame),
	 * then quits if launched with -DVQuitAfterCapture. Rendered in-engine, so it works with
	 * -RenderOffScreen and never picks up other windows.
	 */
	UFUNCTION(Exec)
	void DVCapture(const FString& Name, int32 Frames, float Interval = 0.f, float Delay = 0.f);

	virtual void PlayerTick(float DeltaTime) override;

private:
	void ShowcaseOrderFlank();
	void CaptureTick();
	void StartCapture();

	FString CaptureName;
	int32 CaptureFramesLeft = 0;
	int32 CaptureIndex = 0;
	float CaptureInterval = 0.f;
	FTimerHandle CaptureTimer;
	bool bCaptureEveryFrame = false;

	FTimerHandle ShowcaseTimer;
};
