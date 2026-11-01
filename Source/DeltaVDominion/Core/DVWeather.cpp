#include "Core/DVWeather.h"
#include "Core/DVGameMode.h"
#include "Characters/DVSquadAIController.h"
#include "DeltaVDominion.h"

#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

ADVWeather::ADVWeather()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(4.f);
}

void ADVWeather::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADVWeather, Strength);
	DOREPLIFETIME(ADVWeather, FrontName);
}

ADVWeather* ADVWeather::Find(const UWorld* World)
{
	for (TActorIterator<ADVWeather> It(World); World && It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void ADVWeather::BeginPlay()
{
	Super::BeginPlay();
	for (TActorIterator<AExponentialHeightFog> It(GetWorld()); It; ++It)
	{
		Fog = It->GetComponent();
		break;
	}
	if (Fog.IsValid())
	{
		BaseDensity = Fog->FogDensity;
		BaseColor = Fog->FogInscatteringLuminance;
	}
	if (HasAuthority())
	{
		NextFront = GetWorld()->GetTimeSeconds() + FMath::FRandRange(CalmMin * 0.6f, CalmMax * 0.6f);	// the first comes a little sooner
	}
}

void ADVWeather::ForceFront(float Delay)
{
	NextFront = GetWorld()->GetTimeSeconds() + Delay;
	bWarned = false;
}

void ADVWeather::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double Now = GetWorld()->GetTimeSeconds();
	if (HasAuthority())
	{
		if (FrontStart < 0.0 && !bWarned && NextFront - Now < 10.0)
		{
			bWarned = true;
			if (ADVGameMode* GM = GetWorld()->GetAuthGameMode<ADVGameMode>())
			{
				GM->SayOnRadio(TEXT("Gny. Sgt. Halloran"), FString::Printf(TEXT("%s rolling in. Their sentries go blind in it. Move when it hits."), *FrontName));
			}
		}
		if (FrontStart < 0.0 && Now >= NextFront)
		{
			FrontStart = Now;
		}
		if (FrontStart >= 0.0)
		{
			const float T = float(Now - FrontStart);
			const float Total = StormSeconds + 2.f * RampSeconds;
			Strength = T < RampSeconds ? T / RampSeconds : T < RampSeconds + StormSeconds ? 1.f : FMath::Max(0.f, 1.f - (T - RampSeconds - StormSeconds) / RampSeconds);
			if (T >= Total)
			{
				FrontStart = -1.0;
				Strength = 0.f;
				bWarned = false;
				NextFront = Now + FMath::FRandRange(CalmMin, CalmMax);
				if (ADVGameMode* GM = GetWorld()->GetAuthGameMode<ADVGameMode>())
				{
					GM->SayOnRadio(TEXT("Gny. Sgt. Halloran"), TEXT("It's clearing. They can see again. Get to cover."));
				}
			}
		}
		// Every AI's eyes shorten with the front (applied in steps, not every frame).
		const float Sight = FMath::Lerp(1.f, SightFraction, Strength);
		if (FMath::Abs(Sight - AppliedSight) > 0.04f || (Sight == 1.f && AppliedSight != 1.f))
		{
			ApplySight(Sight);
		}
	}
	if (Fog.IsValid())
	{
		const float S = FMath::SmoothStep(0.f, 1.f, Strength);
		Fog->SetFogDensity(BaseDensity * FMath::Lerp(1.f, FogScale, S));
		Fog->SetFogInscatteringColor(FMath::Lerp(BaseColor, FrontColor, 0.7f * S));
	}
}

void ADVWeather::ApplySight(float Fraction)
{
	AppliedSight = Fraction;
	for (TActorIterator<ADVSquadAIController> It(GetWorld()); It; ++It)
	{
		It->SetSightScale(Fraction);
	}
}
