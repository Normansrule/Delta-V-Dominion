#include "Core/DVAscent.h"
#include "Core/DVGameMode.h"
#include "Vehicles/DVGunship.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

namespace
{
	float EaseInOut(float T) { T = FMath::Clamp(T, 0.f, 1.f); return T * T * (3.f - 2.f * T); }
	float EaseOut(float T) { T = FMath::Clamp(T, 0.f, 1.f); return 1.f - (1.f - T) * (1.f - T) * (1.f - T); }

	FVector Bezier(const FVector& A, const FVector& B, const FVector& C, const FVector& D, float T)
	{
		const float U = 1.f - T;
		return U * U * U * A + 3.f * U * U * T * B + 3.f * U * T * T * C + T * T * T * D;
	}

	constexpr float LiftSeconds = 4.f;
	constexpr float ApproachSeconds = 4.5f;
	constexpr float HoverSeconds = 3.f;
	constexpr float SettleSeconds = 2.5f;
}

ADVAscent::ADVAscent()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;	// the server flies the Raven; its own movement replication does the rest
}

void ADVAscent::Begin(ADVGunship* InRaven, APlayerController* InPilot, const FTransform& Hangar)
{
	Raven = InRaven;
	Pilot = InPilot;
	HangarXf = Hangar;
	Start = InRaven->GetActorLocation();
	Lifted = Start + FVector(0.f, 0.f, 2200.f);
	ApproachW = Hangar.TransformPosition(Approach);
	EntryW = Hangar.TransformPosition(Entry);
	HoverW = Hangar.TransformPosition(Hover);
	TouchW = Hangar.TransformPosition(Touchdown);
	// Cruise at roughly 250 m/s, but never so fast the bay can't stream in, never so slow it drags.
	CruiseSeconds = FMath::Clamp(FVector::Dist(Lifted, ApproachW) / 25000.f, 9.f, 18.f);
	StartTime = GetWorld()->GetTimeSeconds();
	LastPos = Start;
	InRaven->SetScriptedFlight(true);
}

void ADVAscent::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ADVGunship* Ship = Raven.Get();
	if (!Ship)
	{
		Destroy();
		return;
	}
	const float T = GetWorld()->GetTimeSeconds() - StartTime;
	const float T1 = LiftSeconds, T2 = T1 + CruiseSeconds, T3 = T2 + ApproachSeconds, T4 = T3 + HoverSeconds, T5 = T4 + SettleSeconds;
	const FVector Into = HangarXf.GetRotation().GetForwardVector();	// out of the mouth is -Into
	const float BayYaw = Into.Rotation().Yaw;

	FVector Pos;
	float Yaw = Ship->GetActorRotation().Yaw;
	float Pitch = 0.f;
	if (T < T1)
	{
		// Dust off: straight up, turning toward the carrier.
		Pos = FMath::Lerp(Start, Lifted, EaseInOut(T / T1));
		const float TowardKestrel = (ApproachW - Start).GetSafeNormal2D().Rotation().Yaw;
		Yaw = FMath::Lerp(FRotator(0.f, Yaw, 0.f), FRotator(0.f, TowardKestrel, 0.f), FMath::Clamp(DeltaSeconds * 1.2f, 0.f, 1.f)).Yaw;
	}
	else if (T < T2)
	{
		if (Phase < 1)
		{
			Phase = 1;
			Ship->DebugSetModes(true, false);	// engines swing aft for the climb
		}
		// Climb and cruise on a curve that arrives lined up with the bay, already pointing in.
		const float D = FVector::Dist(Lifted, ApproachW);
		const FVector C1 = Lifted + (ApproachW - Lifted).GetSafeNormal2D() * D * 0.35f + FVector(0.f, 0.f, D * 0.1f);
		const FVector C2 = ApproachW - Into * D * 0.3f;
		Pos = Bezier(Lifted, C1, C2, ApproachW, EaseInOut((T - T1) / CruiseSeconds));
	}
	else if (T < T3)
	{
		if (Phase < 2)
		{
			Phase = 2;
			Ship->DebugSetModes(false, false);	// hover mode to come aboard
		}
		Pos = FMath::Lerp(ApproachW, EntryW, EaseOut((T - T2) / ApproachSeconds));
		Yaw = BayYaw;
	}
	else if (T < T4)
	{
		Pos = FMath::Lerp(EntryW, HoverW, EaseInOut((T - T3) / HoverSeconds));
		Yaw = BayYaw;
	}
	else if (T < T5)
	{
		Pos = FMath::Lerp(HoverW, TouchW, EaseInOut((T - T4) / SettleSeconds));
		Yaw = BayYaw;
	}
	else
	{
		Ship->SetActorLocationAndRotation(TouchW, FRotator(0.f, BayYaw, 0.f));
		Land();
		return;
	}

	// Face where we're going during the cruise; nose dips with speed.
	const FVector Step = Pos - LastPos;
	if (T >= T1 && T < T2 && Step.SizeSquared2D() > 1.f)
	{
		Yaw = Step.GetSafeNormal2D().Rotation().Yaw;
	}
	if (DeltaSeconds > 0.f)
	{
		Pitch = -FMath::Clamp(Step.Size() / DeltaSeconds / 2500.f, 0.f, 10.f);
	}
	LastPos = Pos;
	Ship->SetActorLocationAndRotation(Pos, FRotator(Pitch, Yaw, 0.f));

	// The chase camera follows the pilot's view, so steer the view with the ship: you look where Two-One is flying.
	if (APlayerController* PC = Pilot.Get())
	{
		const FRotator Want(-8.f, Yaw, 0.f);
		const FRotator View = FMath::RInterpTo(PC->GetControlRotation(), Want, DeltaSeconds, 2.5f);
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

void ADVAscent::Land()
{
	ADVGameMode* GM = GetWorld()->GetAuthGameMode<ADVGameMode>();
	if (GM)
	{
		GM->OnAscentLanded(Pilot.Get(), Raven.Get());
	}
	Destroy();
}
