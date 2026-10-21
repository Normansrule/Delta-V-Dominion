#include "Core/DVPressureManifold.h"
#include "Core/DVGameMode.h"
#include "Core/DVFx.h"
#include "Vehicles/DVVehicleKit.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"

namespace
{
	// Local layout (cm). The manifold faces local -X: the side the squad walks up to.
	constexpr float ValveX = -50.f;
	constexpr float ValveZ = 115.f;
	constexpr float GaugeX = -28.f;
	constexpr float GaugeZ = 300.f;
	constexpr float ColumnY[ADVPressureManifold::NumValves] = { -200.f, 0.f, 200.f };

	/** Needle angle for a pressure step: empty hard left, green straight up, red hard right. */
	float NeedleFor(int32 Step) { return -120.f + Step * 60.f; }

	FLinearColor LampColor(int32 Step)
	{
		return Step == ADVPressureManifold::Green ? FLinearColor(0.15f, 1.f, 0.3f)
			: Step >= ADVPressureManifold::Steps - 1 ? FLinearColor(1.f, 0.08f, 0.03f) : FLinearColor(1.f, 0.5f, 0.08f);
	}

	UMaterialInterface* Metal(UObject* Outer, const FLinearColor& Tint, float Metallic, float Roughness)
	{
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(DVKit::Material(TEXT("Rust")), Outer);
		if (M)
		{
			M->SetVectorParameterValue(TEXT("TintColor"), Tint);
			M->SetScalarParameterValue(TEXT("MetallicAmount"), Metallic);
			M->SetScalarParameterValue(TEXT("RoughnessAmount"), Roughness);
		}
		return M;
	}
}

ADVPressureManifold::ADVPressureManifold()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Gauges.Init(0, NumValves);
	ValveTurns.Init(0, NumValves);
}

void ADVPressureManifold::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADVPressureManifold, Gauges);
	DOREPLIFETIME(ADVPressureManifold, ValveTurns);
	DOREPLIFETIME(ADVPressureManifold, TotalTurns);
	DOREPLIFETIME(ADVPressureManifold, bSolved);
}

ADVPressureManifold* ADVPressureManifold::Find(const UWorld* World)
{
	if (World)
	{
		for (TActorIterator<ADVPressureManifold> It(const_cast<UWorld*>(World)); It; ++It)
		{
			return *It;
		}
	}
	return nullptr;
}

bool ADVPressureManifold::Feeds(int32 Valve, int32 Gauge)
{
	// A (0) feeds gauges 1-2, B (1) feeds all three, C (2) feeds 2-3: neighbours share a pipe.
	return FMath::Abs(Valve - Gauge) <= 1;
}

FString ADVPressureManifold::ValveName(int32 Index)
{
	return Index == 0 ? TEXT("A") : Index == 1 ? TEXT("B") : TEXT("C");
}

UStaticMeshComponent* ADVPressureManifold::Part(UStaticMesh* Mesh, UMaterialInterface* Material, const FVector& Loc, const FVector& SizeCm, const FRotator& Rot, USceneComponent* Parent)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetupAttachment(Parent ? Parent : Root.Get());
	C->SetStaticMesh(Mesh);
	C->SetMaterial(0, Material);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetRelativeLocationAndRotation(Loc, Rot);
	if (Mesh)
	{
		const FVector Extent = Mesh->GetBounds().BoxExtent * 2.f;
		C->SetRelativeScale3D(FVector(SizeCm.X / FMath::Max(Extent.X, 1.f), SizeCm.Y / FMath::Max(Extent.Y, 1.f), SizeCm.Z / FMath::Max(Extent.Z, 1.f)));
	}
	C->RegisterComponent();
	return C;
}

UStaticMeshComponent* ADVPressureManifold::Glow(const FVector& Loc, const FVector& SizeCm, const FLinearColor& Color, float Strength, USceneComponent* Parent)
{
	UStaticMeshComponent* C = Part(DVKit::Sphere(), DVKit::Material(TEXT("Glow")), Loc, SizeCm, FRotator::ZeroRotator, Parent);
	DVKit::SetGlow(C, Color, Strength);
	return C;
}

void ADVPressureManifold::BeginPlay()
{
	Super::BeginPlay();
	UMaterialInterface* Brass = Metal(this, FLinearColor(0.55f, 0.36f, 0.12f), 0.9f, 0.42f);
	UMaterialInterface* Iron = Metal(this, FLinearColor(0.07f, 0.05f, 0.04f), 0.6f, 0.6f);
	UMaterialInterface* Copper = Metal(this, FLinearColor(0.6f, 0.22f, 0.1f), 1.f, 0.38f);
	UMaterialInterface* Soot = Metal(this, FLinearColor(0.03f, 0.028f, 0.026f), 0.4f, 0.75f);
	UMaterialInterface* Ink = Metal(this, FLinearColor(0.01f, 0.01f, 0.01f), 0.2f, 0.5f);
	const FRotator Facing(90.f, 0.f, 0.f);	// cylinders lie on their side, faces toward -X

	// Plinth, riveted back panel, and the boiler it all feeds, with its stack.
	Part(DVKit::Box(), Soot, FVector(40.f, 0.f, 10.f), FVector(260.f, 700.f, 20.f));
	Part(DVKit::Box(), Iron, FVector(30.f, 0.f, 230.f), FVector(24.f, 640.f, 440.f));	// a dark iron panel, so the brass reads against it
	Part(DVKit::Box(), Copper, FVector(16.f, 0.f, 452.f), FVector(30.f, 660.f, 18.f));
	Part(DVKit::Cylinder(), Brass, FVector(330.f, 0.f, 290.f), FVector(420.f, 420.f, 560.f));
	for (int32 k = 0; k < 3; ++k)
	{
		Part(DVKit::Cylinder(), Copper, FVector(330.f, 0.f, 90.f + k * 200.f), FVector(436.f, 436.f, 22.f));
	}
	Part(DVKit::Cylinder(), Soot, FVector(330.f, 0.f, 820.f), FVector(90.f, 90.f, 520.f));

	for (int32 i = 0; i < NumValves; ++i)
	{
		const float Y = ColumnY[i];
		// The wheel: rim, cross spokes and hub, turning about the X axis.
		USceneComponent* Wheel = NewObject<USceneComponent>(this);
		Wheel->SetupAttachment(Root);
		Wheel->SetRelativeLocation(FVector(ValveX, Y, ValveZ));
		Wheel->RegisterComponent();
		Wheels.Add(Wheel);
		Part(DVKit::Cylinder(), Brass, FVector::ZeroVector, FVector(96.f, 96.f, 10.f), Facing, Wheel);
		Part(DVKit::Cylinder(), Soot, FVector(-2.f, 0.f, 0.f), FVector(76.f, 76.f, 12.f), Facing, Wheel);
		Part(DVKit::Box(), Brass, FVector(-4.f, 0.f, 0.f), FVector(8.f, 84.f, 9.f), FRotator::ZeroRotator, Wheel);
		Part(DVKit::Box(), Brass, FVector(-4.f, 0.f, 0.f), FVector(8.f, 9.f, 84.f), FRotator::ZeroRotator, Wheel);
		Part(DVKit::Cylinder(), Copper, FVector(-6.f, 0.f, 0.f), FVector(22.f, 22.f, 14.f), Facing, Wheel);
		Part(DVKit::Cylinder(), Copper, FVector(0.f, Y, ValveZ), FVector(22.f, 22.f, 70.f), Facing);

		// The gauge above it: dial face, green mark at twelve o'clock, red at the end of the sweep, and the needle.
		Part(DVKit::Cylinder(), Brass, FVector(GaugeX + 6.f, Y, GaugeZ), FVector(84.f, 84.f, 10.f), Facing);
		UStaticMeshComponent* Dial = Part(DVKit::Cylinder(), DVKit::Material(TEXT("Glow")), FVector(GaugeX, Y, GaugeZ), FVector(70.f, 70.f, 6.f), Facing);
		DVKit::SetGlow(Dial, FLinearColor(0.9f, 0.82f, 0.62f), 0.6f);
		UStaticMeshComponent* Mark = Part(DVKit::Box(), DVKit::Material(TEXT("Glow")), FVector(GaugeX - 4.f, Y, GaugeZ + 27.f), FVector(3.f, 12.f, 9.f));
		DVKit::SetGlow(Mark, FLinearColor(0.1f, 1.f, 0.25f), 6.f);
		const FVector RedDir = FRotator(0.f, 0.f, NeedleFor(Steps - 1)).RotateVector(FVector::UpVector);	// same rotation the needle uses
		UStaticMeshComponent* Red = Part(DVKit::Box(), DVKit::Material(TEXT("Glow")), FVector(GaugeX - 4.f, Y + RedDir.Y * 27.f, GaugeZ + RedDir.Z * 27.f), FVector(3.f, 9.f, 9.f));
		DVKit::SetGlow(Red, FLinearColor(1.f, 0.08f, 0.03f), 6.f);
		USceneComponent* Needle = NewObject<USceneComponent>(this);
		Needle->SetupAttachment(Root);
		Needle->SetRelativeLocation(FVector(GaugeX - 6.f, Y, GaugeZ));
		Needle->RegisterComponent();
		Needles.Add(Needle);
		Part(DVKit::Box(), Ink, FVector(0.f, 0.f, 13.f), FVector(3.f, 3.f, 28.f), FRotator::ZeroRotator, Needle);
		Lamps.Add(Glow(FVector(GaugeX, Y, GaugeZ + 72.f), FVector(16.f, 16.f, 16.f), LampColor(0), 12.f));

		// Its own riser from valve to gauge.
		Part(DVKit::Cylinder(), Copper, FVector(-6.f, Y, (ValveZ + GaugeZ) * 0.5f + 20.f), FVector(16.f, 16.f, GaugeZ - ValveZ - 90.f));
	}
	// Shared pipes that make it a puzzle: A and B share the left header, B and C the right, B also drives a crossover to 1 and 3.
	Part(DVKit::Cylinder(), Copper, FVector(-6.f, -100.f, 196.f), FVector(14.f, 14.f, 200.f), FRotator(0.f, 0.f, 90.f));
	Part(DVKit::Cylinder(), Copper, FVector(-6.f, 100.f, 222.f), FVector(14.f, 14.f, 200.f), FRotator(0.f, 0.f, 90.f));
	Part(DVKit::Cylinder(), Copper, FVector(6.f, 0.f, 176.f), FVector(12.f, 12.f, 420.f), FRotator(0.f, 0.f, 90.f));

	Beacon = Glow(FVector(20.f, 0.f, 500.f), FVector(40.f, 40.f, 40.f), FLinearColor(1.f, 0.1f, 0.03f), 20.f);
	BeaconLight = NewObject<UPointLightComponent>(this);
	BeaconLight->SetupAttachment(Root);
	BeaconLight->SetRelativeLocation(FVector(-60.f, 0.f, 500.f));
	BeaconLight->SetIntensityUnits(ELightUnits::Candelas);
	BeaconLight->SetIntensity(120.f);
	BeaconLight->SetLightColor(FLinearColor(1.f, 0.15f, 0.05f));
	BeaconLight->SetAttenuationRadius(700.f);
	BeaconLight->SetCastShadows(false);
	BeaconLight->RegisterComponent();
	bBuilt = true;

	if (HasAuthority())
	{
		Scramble();
	}
	for (int32 i = 0; i < NumValves; ++i)
	{
		NeedleAngle[i] = NeedleFor(GetGauge(i));
	}
	OnRep_Gauges();
}

void ADVPressureManifold::Scramble()
{
	// A fresh, unsolved state that takes at least five turns to fix.
	for (int32 Tries = 0; Tries < 200; ++Tries)
	{
		for (int32 i = 0; i < NumValves; ++i)
		{
			Gauges[i] = static_cast<uint8>(FMath::RandRange(0, Steps - 1));
		}
		int32 Turns[NumValves];
		GetSolution(Turns);
		if (Turns[0] + Turns[1] + Turns[2] >= 5 && GetBalance() < 1.f)
		{
			break;
		}
	}
	bSolved = false;
	int32 Turns[NumValves];
	GetSolution(Turns);
	UE_LOG(LogTemp, Display, TEXT("Steamheart manifold: gauges %d %d %d (green is %d); answer A x%d, B x%d, C x%d"),
		Gauges[0], Gauges[1], Gauges[2], Green, Turns[0], Turns[1], Turns[2]);
}

void ADVPressureManifold::GetSolution(int32 OutTurns[NumValves]) const
{
	// Gauge_g = start_g + sum of turns on the valves that feed it (mod 5). Solved by hand for this coupling:
	// d = green - start;  C = d1 - d0;  B = d2 - d1 + d0;  A = d1 - d2.
	int32 D[NumValves];
	for (int32 g = 0; g < NumValves; ++g)
	{
		D[g] = Green - GetGauge(g);
	}
	auto Mod = [](int32 V) { return ((V % Steps) + Steps) % Steps; };
	OutTurns[0] = Mod(D[1] - D[2]);
	OutTurns[1] = Mod(D[2] - D[1] + D[0]);
	OutTurns[2] = Mod(D[1] - D[0]);
}

float ADVPressureManifold::GetBalance() const
{
	int32 InGreen = 0;
	for (int32 g = 0; g < NumValves; ++g)
	{
		InGreen += GetGauge(g) == Green ? 1 : 0;
	}
	return InGreen / float(NumValves);
}

FVector ADVPressureManifold::GetValveLocation(int32 Index) const
{
	return GetActorTransform().TransformPosition(FVector(ValveX, ColumnY[FMath::Clamp(Index, 0, NumValves - 1)], ValveZ));
}

int32 ADVPressureManifold::FindValveNear(const AActor* User) const
{
	if (!User || bSolved)
	{
		return -1;
	}
	int32 Best = -1;
	float BestDist = Reach;
	for (int32 i = 0; i < NumValves; ++i)
	{
		const FVector V = GetValveLocation(i);
		const float D = FVector::Dist2D(User->GetActorLocation(), V);
		if (D < BestDist && FMath::Abs(User->GetActorLocation().Z - V.Z) < 220.f)
		{
			BestDist = D;
			Best = i;
		}
	}
	return Best;
}

void ADVPressureManifold::TurnValve(int32 Index)
{
	if (!HasAuthority() || bSolved || Index < 0 || Index >= NumValves)
	{
		return;
	}
	++ValveTurns[Index];
	++TotalTurns;
	uint8 Vented = 0;
	for (int32 g = 0; g < NumValves; ++g)
	{
		if (Feeds(Index, g))
		{
			const int32 Next = Gauges[g] + 1;
			if (Next >= Steps)
			{
				Vented |= 1 << g;	// past red: the safety valve blows and the gauge drops to empty
			}
			Gauges[g] = static_cast<uint8>(Next % Steps);
		}
	}
	bSolved = GetBalance() >= 1.f;
	OnRep_Gauges();
	MulticastTurned(static_cast<uint8>(Index), Vented);
	if (ADVGameMode* GM = GetWorld()->GetAuthGameMode<ADVGameMode>())
	{
		GM->OnManifoldTurned(Index, bSolved);
	}
}

void ADVPressureManifold::MulticastTurned_Implementation(uint8 Index, uint8 VentMask)
{
	DVSound::At(GetWorld(), TEXT("SW_Slide"), GetValveLocation(Index), 0.7f, 0, 0.15f);
	for (int32 g = 0; g < NumValves; ++g)
	{
		if (VentMask & (1 << g))
		{
			const FVector At = GetActorTransform().TransformPosition(FVector(GaugeX, ColumnY[g], GaugeZ + 120.f));
			DVSound::At(GetWorld(), TEXT("SW_Thruster"), At, 0.8f, 1, 0.1f);
			DVFx::Flash(GetWorld(), At, FLinearColor(1.f, 0.95f, 0.85f), 1500.f, 700.f, 0.35f);
		}
	}
}

void ADVPressureManifold::OnRep_Gauges()
{
	if (!bBuilt)
	{
		return;
	}
	for (int32 i = 0; i < Lamps.Num(); ++i)
	{
		DVKit::SetGlow(Lamps[i], LampColor(GetGauge(i)), 12.f);
	}
	const bool bDone = GetBalance() >= 1.f;
	DVKit::SetGlow(Beacon, bDone ? FLinearColor(0.1f, 1.f, 0.3f) : FLinearColor(1.f, 0.1f, 0.03f), 20.f);
	BeaconLight->SetLightColor(bDone ? FLinearColor(0.2f, 1.f, 0.35f) : FLinearColor(1.f, 0.15f, 0.05f));
	if (bDone)
	{
		DVSound::At(GetWorld(), TEXT("SW_PlantBeep"), GetActorLocation() + FVector(0.f, 0.f, 400.f), 1.f, 1, 0.f);
	}
}

void ADVPressureManifold::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bBuilt)
	{
		return;
	}
	// Wheels turn a quarter per press; needles swing over with a little overshoot, like a real Bourdon tube.
	const float Alpha = 1.f - FMath::Exp(-DeltaSeconds * 9.f);
	for (int32 i = 0; i < NumValves; ++i)
	{
		const float WheelTarget = (ValveTurns.IsValidIndex(i) ? ValveTurns[i] : 0) * 90.f;
		WheelAngle[i] = FMath::Lerp(WheelAngle[i], WheelTarget, Alpha);
		Wheels[i]->SetRelativeRotation(FRotator(0.f, 0.f, WheelAngle[i]));
		const float Wobble = FMath::Sin(GetWorld()->GetTimeSeconds() * 23.f + i) * 1.5f;
		NeedleAngle[i] = FMath::Lerp(NeedleAngle[i], NeedleFor(GetGauge(i)), Alpha * 0.7f);
		Needles[i]->SetRelativeRotation(FRotator(0.f, 0.f, NeedleAngle[i] + Wobble));
	}
}
