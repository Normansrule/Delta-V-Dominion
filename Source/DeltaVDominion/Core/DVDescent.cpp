#include "Core/DVDescent.h"
#include "Core/DVGameMode.h"
#include "Vehicles/DVGunship.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

namespace DVDescentFlight	// named: unity builds merge the anonymous namespaces of the other flight directors
{
	float EaseInOut(float T) { T = FMath::Clamp(T, 0.f, 1.f); return T * T * (3.f - 2.f * T); }
	float EaseOut(float T) { T = FMath::Clamp(T, 0.f, 1.f); return 1.f - (1.f - T) * (1.f - T) * (1.f - T); }

	constexpr float InboundSeconds = 11.f;
	constexpr float FlareSeconds = 3.5f;
	constexpr float SettleSeconds = 2.5f;
}

ADVDescent::ADVDescent()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;	// the server flies the Raven; its own movement replication does the rest
}

void ADVDescent::Begin(ADVGunship* InRaven, APlayerController* InPilot, const FTransform& Pad)
{
	Raven = InRaven;
	Pilot = InPilot;
	PadYaw = Pad.Rotator().Yaw;
	Touch = Pad.GetLocation();
	Hover = Touch + FVector(0.f, 0.f, 650.f);
	Start = Touch + StartOffset;
	// Come in along the approach line and flare a little short of the pad, nose up, bleeding speed.
	Flare = Hover + (Start - Hover).GetSafeNormal2D() * 1400.f + FVector(0.f, 0.f, 300.f);
	StartTime = GetWorld()->GetTimeSeconds();
	InRaven->SetScriptedFlight(true);
	InRaven->DebugSetModes(true, false);	// jet mode for the run in
	InRaven->SetActorLocationAndRotation(Start, (Hover - Start).Rotation());
}

void ADVDescent::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ADVGunship* Ship = Raven.Get();
	if (!Ship)
	{
		Destroy();
		return;
	}
	const float T = GetWorld()->GetTimeSeconds() - StartTime;
	const float T1 = DVDescentFlight::InboundSeconds, T2 = T1 + DVDescentFlight::FlareSeconds, T3 = T2 + DVDescentFlight::SettleSeconds;
	const float InboundYaw = (Hover - Start).GetSafeNormal2D().Rotation().Yaw;
	FVector Pos;
	FRotator Rot;
	if (T < T1)
	{
		// The long descent: fast at first, slowing as the ground comes up, nose down.
		Pos = FMath::Lerp(Start, Flare, DVDescentFlight::EaseOut(T / T1));
		Rot = FRotator(-8.f * (1.f - T / T1), InboundYaw, 0.f);
	}
	else if (T < T2)
	{
		if (Phase < 1)
		{
			Phase = 1;
			Ship->DebugSetModes(false, false);	// engines down for the hover
		}
		// Flare: nose up, swing round to the pad's heading, settle into a hover over the pad.
		const float A = DVDescentFlight::EaseInOut((T - T1) / DVDescentFlight::FlareSeconds);
		Pos = FMath::Lerp(Flare, Hover, A);
		Rot = FRotator(FMath::Sin(A * PI) * 10.f, FMath::Lerp(FRotator(0.f, InboundYaw, 0.f), FRotator(0.f, PadYaw, 0.f), A).Yaw, 0.f);
	}
	else if (T < T3)
	{
		Pos = FMath::Lerp(Hover, Touch, DVDescentFlight::EaseInOut((T - T2) / DVDescentFlight::SettleSeconds));
		Rot = FRotator(0.f, PadYaw, 0.f);
	}
	else
	{
		Ship->SetActorLocationAndRotation(Touch, FRotator(0.f, PadYaw, 0.f));
		if (ADVGameMode* GM = GetWorld()->GetAuthGameMode<ADVGameMode>())
		{
			GM->OnDescentLanded(Pilot.Get(), Ship);
		}
		Destroy();
		return;
	}
	Ship->SetActorLocationAndRotation(Pos, Rot);

	// Look out of the nose at the ground coming up.
	if (APlayerController* PC = Pilot.Get())
	{
		const FRotator Want(T < T1 ? -16.f : -8.f, Rot.Yaw, 0.f);
		const FRotator View = FMath::RInterpTo(PC->GetControlRotation(), Want, DeltaSeconds, 2.f);
		if (PC->IsLocalController())
		{
			PC->SetControlRotation(View);
		}
		else
		{
			PC->ClientSetRotation(View);
		}
	}
}
