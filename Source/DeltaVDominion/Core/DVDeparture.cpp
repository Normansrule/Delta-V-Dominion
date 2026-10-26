#include "Core/DVDeparture.h"
#include "Core/DVGameState.h"
#include "Core/DVFx.h"
#include "Vehicles/DVGunship.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	float EaseInOut(float T) { T = FMath::Clamp(T, 0.f, 1.f); return T * T * (3.f - 2.f * T); }
	float EaseIn(float T) { T = FMath::Clamp(T, 0.f, 1.f); return T * T * T; }

	constexpr float LiftSeconds = 3.f;
	constexpr float TaxiSeconds = 5.f;
	constexpr float BurnSeconds = 7.5f;
	/** Out of the mouth and toward Mars below (the hangar map puts the planet at -X, a little +Y, well below). */
	const FVector BurnDir = FVector(-0.95f, 0.1f, -0.28f).GetSafeNormal();
	constexpr float BurnDistance = 32000.f;	// far enough to feel the burn, near enough that the chase camera keeps up
}

ADVDeparture::ADVDeparture()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;	// the server flies the Raven; its own movement replication does the rest
}

void ADVDeparture::Begin(ADVGunship* InRaven, APlayerController* InPilot)
{
	Raven = InRaven;
	Pilot = InPilot;
	Start = InRaven->GetActorLocation();
	Lifted = FVector(Start.X, Start.Y, InsideMouth.Z);
	Burn = OutsideMouth + BurnDir * BurnDistance;
	StartTime = GetWorld()->GetTimeSeconds();
	InRaven->SetScriptedFlight(true);
	InRaven->DebugSetModes(false, false);	// hover mode on the deck
}

void ADVDeparture::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ADVGunship* Ship = Raven.Get();
	ADVGameState* GS = GetWorld()->GetGameState<ADVGameState>();
	if (!Ship || !GS)
	{
		Destroy();
		return;
	}
	const float T = GetWorld()->GetTimeSeconds() - StartTime;
	const float T1 = LiftSeconds, T2 = T1 + TaxiSeconds, T3 = T2 + BurnSeconds;
	FVector Pos;
	float Yaw = Ship->GetActorRotation().Yaw;
	if (T < T1)
	{
		// Up off the pad, swinging round to face the mouth.
		Pos = FMath::Lerp(Start, Lifted, EaseInOut(T / T1));
		Yaw = FMath::Lerp(FRotator(0.f, Yaw, 0.f), FRotator(0.f, 180.f, 0.f), FMath::Clamp(DeltaSeconds * 1.5f, 0.f, 1.f)).Yaw;
	}
	else if (T < T2)
	{
		// Slide down the deck, out over the launch lip and clear of the hull.
		const float A = EaseInOut((T - T1) / TaxiSeconds);
		Pos = A < 0.5f ? FMath::Lerp(Lifted, InsideMouth, A * 2.f) : FMath::Lerp(InsideMouth, OutsideMouth, (A - 0.5f) * 2.f);
		Yaw = 180.f;
	}
	else if (T < T3)
	{
		if (Phase < 1)
		{
			Phase = 1;
			Ship->DebugSetModes(true, false);	// engines aft: full burn
			GS->MulticastSound(TEXT("SW_Thruster"), Ship->GetActorLocation(), 1.f, 2);
			GS->MulticastRadio(TEXT("Raven Two-One"), TEXT("Clear of the Kestrel. Burning for the Lattice. Hold on to something."));
		}
		const float A = (T - T2) / BurnSeconds;
		Pos = FMath::Lerp(OutsideMouth, Burn, EaseIn(A));
		Yaw = BurnDir.Rotation().Yaw;
		if (Phase < 2 && T > T3 - JumpSeconds)
		{
			Phase = 2;
			GS->JumpTime = GetWorld()->GetTimeSeconds();	// every screen starts washing out to white
			GS->MulticastSound(TEXT("SW_OrbitalImpact"), Ship->GetActorLocation(), 0.6f, 2);
		}
	}
	else
	{
		Jump();
		return;
	}
	const float Pitch = T >= T2 ? BurnDir.Rotation().Pitch * 0.6f : 0.f;
	Ship->SetActorLocationAndRotation(Pos, FRotator(Pitch, Yaw, 0.f));

	// Look where Two-One is going, as on the way home.
	if (APlayerController* PC = Pilot.Get())
	{
		const FRotator Want(T >= T2 ? -12.f : -6.f, Yaw, 0.f);
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

void ADVDeparture::Jump()
{
	// Everyone goes together: the galaxy map, opening with the pull-back from Sol.
	if (GetNetMode() == NM_Standalone)
	{
		UGameplayStatics::OpenLevel(this, FName(TEXT("/Game/Maps/Galaxy")), true, TEXT("launch"));
	}
	else
	{
		GetWorld()->ServerTravel(TEXT("/Game/Maps/Galaxy?launch"));
	}
	Destroy();
}
