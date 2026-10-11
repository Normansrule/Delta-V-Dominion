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
	 * Console: "DVCapture Name Frames [Interval] [Delay] [ShowUI]". After Delay seconds, saves Frames viewport screenshots to
	 * Saved/Screenshots/DVCapture/Name_0000.png, one every Interval seconds (0 = every frame),
	 * then quits if launched with -DVQuitAfterCapture. Rendered in-engine, so it works with
	 * -RenderOffScreen and never picks up other windows.
	 */
	UFUNCTION(Exec)
	void DVCapture(const FString& Name, int32 Frames, float Interval = 0.f, float Delay = 0.f, bool bShowUI = false);

	/** Console (dev builds): "DVEndMission 1|0 [Delay]" ends the mission as a success or failure. */
	UFUNCTION(Exec)
	void DVEndMission(bool bSuccess, float Delay = 0.f);

	/** Console: "DVOrbitalStrike [Delay]" calls the Kestrel's strike on whatever the current view is aimed at (same as B). */
	UFUNCTION(Exec)
	void DVOrbitalStrike(float Delay = 0.f);

	virtual void PlayerTick(float DeltaTime) override;
	virtual void SetupInputComponent() override;

	/** Server -> shooter: one of your shots landed (drives the HUD hit marker). */
	UFUNCTION(Client, Unreliable)
	void ClientHitConfirmed(bool bKill, bool bHeadshot);

	double GetLastHitTime() const { return LastHitTime; }
	bool WasLastHitKill() const { return bLastHitKill; }
	bool WasLastHitHeadshot() const { return bLastHitHeadshot; }

protected:
	UFUNCTION(Server, Reliable)
	void ServerRequestRedeploy();

	UFUNCTION(Server, Reliable)
	void ServerEndMission(bool bSuccess);

	UFUNCTION(Server, Reliable)
	void ServerCallOrbitalStrike(FVector_NetQuantize Target);

private:
	void OnRedeployPressed();
	void OnOrbitalPressed();
	void ShowcaseOrderFlank();
	void CaptureTick();
	void StartCapture();

	FString CaptureName;
	int32 CaptureFramesLeft = 0;
	int32 CaptureIndex = 0;
	float CaptureInterval = 0.f;
	bool bCaptureShowUI = false;
	FTimerHandle CaptureTimer;
	bool bCaptureEveryFrame = false;

	double LastHitTime = -100.0;
	bool bLastHitKill = false;
	bool bLastHitHeadshot = false;

	FTimerHandle ShowcaseTimer;
};
