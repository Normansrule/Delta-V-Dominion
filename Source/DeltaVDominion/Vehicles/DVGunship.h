#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "DVGunship.generated.h"

class UBoxComponent;
class UCameraComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class UDVSeatComponent;

/**
 * AV-41 "Raven" VTOL gunship. HOVER mode: W/S forward/back, A/D strafe, Space climb, C/Ctrl descend, mouse
 * steers. JET mode (F): engines swing aft, the mouse flies pitch and heading, W is afterburner. V switches to
 * the cockpit view with a flight HUD; X dumps flares (incoming damage cut for 3 s). LMB 30 mm, RMB rockets.
 * Movement runs on the server from the pilot's inputs and replicates to everyone.
 */
UCLASS()
class DELTAVDOMINION_API ADVGunship : public APawn
{
	GENERATED_BODY()

public:
	ADVGunship();

	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UDVSeatComponent* GetSeat() const { return Seat; }
	float GetSpeedKph() const { return Velocity.Size() * 0.036f; }
	float GetAltitude() const;
	float GetRocketReadyFraction() const;
	bool IsJetMode() const { return bJetMode; }
	bool IsCockpitView() const { return bCockpitView; }
	float GetFlareReadyFraction() const;
	bool AreFlaresActive() const;
	float GetPitch() const { return Body ? Body->GetComponentRotation().Pitch : 0.f; }

	/** Moved by script (strafing runs): skips its own flight model. */
	void SetScriptedFlight(bool bOn);
	/** Tests and trailers: switch flight mode / camera without keys. */
	void DebugSetModes(bool bJet, bool bCockpit) { ServerSetJetMode(bJet); SetCockpitView(bCockpit); }

	/** Scripted input for trailers and tests: local move (fwd, right, up), yaw rate deg/s, fire cannon/rockets. */
	void SetAutopilot(const FVector& Move, float YawRate, bool bCannon, bool bRockets) { AutoMove = Move; AutoYawRate = YawRate; bAutoCannon = bCannon; bAutoRockets = bRockets; }

protected:
	UFUNCTION(Server, Unreliable) void ServerInput(FVector_NetQuantize10 Move, float Yaw, float Pitch);
	UFUNCTION(Server, Reliable) void ServerFire(bool bRocket, FVector_NetQuantize AimPoint);
	UFUNCTION(NetMulticast, Unreliable) void MulticastShot(FVector_NetQuantize Muzzle, FVector_NetQuantize Impact, bool bRocket, bool bHit);
	UFUNCTION(Server, Reliable) void ServerSetJetMode(bool bJet);
	UFUNCTION(Server, Reliable) void ServerFlares();
	UFUNCTION(NetMulticast, Unreliable) void MulticastFlares();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	void SetCockpitView(bool bCockpit);

	FVector GetAimPoint() const;

	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Collision;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Body;	// tilts with acceleration
	UPROPERTY(VisibleAnywhere) TObjectPtr<UDVSeatComponent> Seat;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USpringArmComponent> CameraArm;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> CockpitCamera;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> EnginePods;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> EngineGlows;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> NavLights;

	FVector MoveInput = FVector::ZeroVector;	// local: forward, right, up (-1..1)
	float TargetYaw = 0.f;
	float TargetPitch = 0.f;
	FVector Velocity = FVector::ZeroVector;
	double LastCannon = -100.0, LastRocket = -100.0;
	bool bCannonHeld = false;
	bool bRocketHeld = false;
	int32 RocketSide = 1;
	FVector AutoMove = FVector::ZeroVector;
	float AutoYawRate = 0.f;
	bool bAutoCannon = false;
	bool bAutoRockets = false;
	bool bScriptedFlight = false;
	UPROPERTY(Replicated) bool bJetMode = false;
	UPROPERTY(Replicated) float FlaresUntil = -100.f;
	UPROPERTY(Replicated) float FlareReadyTime = 0.f;
	bool bCockpitView = false;
	bool bWasF = false, bWasV = false, bWasX = false;
	float BaseArmour = 0.4f;
	UPROPERTY() TObjectPtr<class UAudioComponent> EngineSound;
};
