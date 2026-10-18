#include "Core/DVKestrelMemory.h"
#include "Core/DVRosterSubsystem.h"
#include "Characters/DVTownsfolk.h"
#include "Vehicles/DVVehicleKit.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	// Hangar layout (cm, the Kestrel map's own coordinates; see Scripts/build_kestrel_hub.py).
	constexpr float HalfWidth = 3000.f;
	constexpr int32 MaxPlates = 24;
	constexpr int32 LockerCount = 8;

	const TCHAR* PatientLines[] = {
		TEXT("Doc says I'm back on the line in %d missions. Doc lies."),
		TEXT("Tell Halloran I'm not done. %d more ops on my back, that's all."),
		TEXT("Can't feel my left arm yet. Doc says %d missions. Bring me something to read."),
		TEXT("Hey, Commander. Save me a seat on the Raven. %d missions, tops."),
	};
}

ADVKestrelMemory::ADVKestrelMemory()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;	// set dressing, rebuilt from saved data on load
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

UStaticMeshComponent* ADVKestrelMemory::Part(UStaticMesh* Mesh, const TCHAR* Material, const FVector& Loc, const FVector& SizeCm, const FRotator& Rot)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetupAttachment(Root);
	C->SetStaticMesh(Mesh);
	C->SetMaterial(0, DVKit::Material(Material));
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

UStaticMeshComponent* ADVKestrelMemory::Glow(const FVector& Loc, const FVector& SizeCm, const FLinearColor& Color, float Strength)
{
	UStaticMeshComponent* C = Part(DVKit::Box(), TEXT("Glow"), Loc, SizeCm);
	DVKit::SetGlow(C, Color, Strength);
	return C;
}

UPointLightComponent* ADVKestrelMemory::Lamp(const FVector& Loc, const FLinearColor& Color, float Candelas, float Radius)
{
	UPointLightComponent* L = NewObject<UPointLightComponent>(this);
	L->SetupAttachment(Root);
	L->SetRelativeLocation(Loc);
	L->SetIntensityUnits(ELightUnits::Candelas);
	L->SetIntensity(Candelas);
	L->SetLightColor(Color);
	L->SetAttenuationRadius(Radius);
	L->SetCastShadows(false);
	L->RegisterComponent();
	return L;
}

ADVTownsfolk* ADVKestrelMemory::Person(const FVector& Loc, float Yaw, const FString& Name, const FString& Line, int32 Trade, bool bLying)
{
	if (!HasAuthority())
	{
		return nullptr;	// people are replicated characters: the server spawns them
	}
	const FTransform Xf(FRotator(0.f, Yaw, 0.f) + GetActorRotation(), GetActorTransform().TransformPosition(Loc));
	ADVTownsfolk* P = GetWorld()->SpawnActorDeferred<ADVTownsfolk>(ADVTownsfolk::StaticClass(), Xf, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (P)
	{
		P->DisplayName = Name;
		P->CustomLine = Line;
		P->Trade = Trade;
		P->bWanders = false;
		P->bLyingDown = bLying;
		UGameplayStatics::FinishSpawningActor(P, Xf);
	}
	return P;
}

void ADVKestrelMemory::BeginPlay()
{
	Super::BeginPlay();
	const UDVRosterSubsystem* Roster = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDVRosterSubsystem>() : nullptr;
	if (!Roster)
	{
		return;
	}
	const TArray<FDVOpRecord>& Log = Roster->GetOpLog();
	int32 Trophies = 0;
	for (const FDVOpRecord& Op : Log)
	{
		Trophies += Op.Bulwarks;
	}
	BuildMemorial(Roster->GetMemorial());
	BuildMedbay(Roster->GetWounded());
	if (Log.Num() > 0)
	{
		BuildRepairs(Log.Last());
	}
	BuildTrophies(Trophies);
	BuildEvacuees();
	BuildOpsBoard(Log);
	UE_LOG(LogTemp, Log, TEXT("DVKestrelMemory: %d ops, %d fallen, %d wounded, %d trophies"), Log.Num(), Roster->GetMemorial().Num(),
		Roster->GetWounded().Num(), Trophies);
}

void ADVKestrelMemory::BuildMemorial(const TArray<FDVSoldierRecord>& Fallen)
{
	// A dark plate with a lit name line for each of the fallen, eight to a row, below the catwalk so the deck sees them.
	for (int32 i = 0; i < Fallen.Num() && i < MaxPlates; ++i)
	{
		const float X = 560.f + (i % 8) * 270.f;
		const float Z = 800.f - (i / 8) * 190.f;
		Part(DVKit::Box(), TEXT("Gunmetal"), FVector(X, -HalfWidth + 192.f, Z), FVector(240.f, 8.f, 64.f));
		Glow(FVector(X, -HalfWidth + 197.f, Z + 8.f), FVector(190.f, 4.f, 9.f), FLinearColor(1.f, 0.8f, 0.5f), 6.f);
		Glow(FVector(X - 30.f, -HalfWidth + 197.f, Z - 14.f), FVector(110.f, 4.f, 5.f), FLinearColor(1.f, 0.8f, 0.5f), 3.f);
	}
	// Candles along the foot of the wall.
	for (int32 i = 0; i < Fallen.Num() && i < 12; ++i)
	{
		const float X = 500.f + i * 180.f;
		const float H = 18.f + (i * 7) % 16;
		Part(DVKit::Cylinder(), TEXT("Concrete"), FVector(X, -HalfWidth + 300.f, H / 2.f), FVector(12.f, 12.f, H));
		Candles.Add(Glow(FVector(X, -HalfWidth + 300.f, H + 5.f), FVector(5.f, 5.f, 9.f), FLinearColor(1.f, 0.55f, 0.15f), 25.f));
		if (i % 4 == 0)
		{
			Lamp(FVector(X + 270.f, -HalfWidth + 380.f, 60.f), FLinearColor(1.f, 0.6f, 0.25f), 60.f, 500.f);
		}
	}
	// Their lockers are sealed: a red seal across the door and the flag folded on top.
	for (int32 i = 0; i < Fallen.Num() && i < LockerCount; ++i)
	{
		const float X = 1700.f + i * 170.f;
		Glow(FVector(X, HalfWidth - 199.f, 120.f), FVector(140.f, 4.f, 16.f), FLinearColor(1.f, 0.05f, 0.03f), 10.f);
		Part(DVKit::Box(), TEXT("Navy"), FVector(X, HalfWidth - 160.f, 228.f), FVector(70.f, 45.f, 14.f));
		Glow(FVector(X, HalfWidth - 160.f, 236.f), FVector(70.f, 46.f, 3.f), FLinearColor(1.f, 0.8f, 0.3f), 4.f);
	}
}

void ADVKestrelMemory::BuildMedbay(const TArray<FDVSoldierRecord>& Wounded)
{
	// Four beds at the aft end of the starboard side, a green cross on the bulkhead, the doctor.
	Glow(FVector(5450.f, -HalfWidth + 110.f, 520.f), FVector(170.f, 6.f, 50.f), FLinearColor(0.2f, 1.f, 0.4f), 12.f);
	Glow(FVector(5450.f, -HalfWidth + 110.f, 520.f), FVector(50.f, 6.f, 170.f), FLinearColor(0.2f, 1.f, 0.4f), 12.f);
	Lamp(FVector(5200.f, -2400.f, 500.f), FLinearColor(0.85f, 1.f, 0.9f), 500.f, 1400.f);
	for (int32 i = 0; i < 4; ++i)
	{
		const float X = 4850.f + i * 250.f;
		Part(DVKit::Box(), TEXT("Gunmetal"), FVector(X, -2450.f, 30.f), FVector(100.f, 210.f, 40.f));
		Part(DVKit::Box(), TEXT("Concrete"), FVector(X, -2450.f, 57.f), FVector(90.f, 200.f, 14.f));
		Part(DVKit::Box(), TEXT("Concrete"), FVector(X, -2370.f, 69.f), FVector(70.f, 40.f, 10.f));
		if (i < 3)
		{
			Part(DVKit::Box(), TEXT("Gunmetal"), FVector(X + 125.f, -2450.f, 110.f), FVector(6.f, 220.f, 200.f));	// divider screen
		}
		if (i < Wounded.Num())
		{
			const FDVSoldierRecord& S = Wounded[i];
			Person(FVector(X, -2450.f, 146.f), 90.f, FString::Printf(TEXT("%s - medbay"), *S.GetDisplayName()),
				FString(PatientLines[i % UE_ARRAY_COUNT(PatientLines)]).Replace(TEXT("%d"), *FString::FromInt(FMath::Max(1, S.WoundedMissionsRemaining))), 2, true);
		}
		else
		{
			Part(DVKit::Box(), TEXT("Olive"), FVector(X, -2510.f, 70.f), FVector(80.f, 50.f, 12.f));	// folded blanket: an empty bed
		}
	}
	const FString DocLine = Wounded.Num() == 0 ? FString(TEXT("Empty beds. Best sight on this ship. Keep it that way."))
		: FString::Printf(TEXT("%d in my care. They'll mend. Talk to them; it helps more than anything I give them."), Wounded.Num());
	Person(FVector(5300.f, -2050.f, 100.f), -90.f, TEXT("Doc Amara Pell, medbay"), DocLine, 1);
}

void ADVKestrelMemory::BuildRepairs(const FDVOpRecord& Last)
{
	if (Last.bSuccess && Last.Lost == 0)
	{
		return;
	}
	// The Raven on pad one came back hurt: scaffold, a welder's flicker, crew working on her.
	const FVector Pad(-3000.f, -1500.f, 0.f);
	for (int32 i = 0; i < 4; ++i)
	{
		const FVector Corner = Pad + FVector(i % 2 ? 620.f : -620.f, i / 2 ? 620.f : -620.f, 0.f);
		Part(DVKit::Box(), TEXT("Rust"), Corner + FVector(0.f, 0.f, 350.f), FVector(20.f, 20.f, 700.f));
	}
	Part(DVKit::Box(), TEXT("Rust"), Pad + FVector(0.f, -620.f, 700.f), FVector(1260.f, 20.f, 20.f));
	Part(DVKit::Box(), TEXT("Rust"), Pad + FVector(0.f, 620.f, 700.f), FVector(1260.f, 20.f, 20.f));
	Part(DVKit::Box(), TEXT("Gunmetal"), Pad + FVector(-300.f, 0.f, 690.f), FVector(200.f, 1260.f, 30.f));	// gantry walk
	Glow(Pad + FVector(250.f, 300.f, 260.f), FVector(10.f, 10.f, 10.f), FLinearColor(1.f, 0.7f, 0.3f), 80.f);
	WeldLight = Lamp(Pad + FVector(250.f, 300.f, 280.f), FLinearColor(1.f, 0.6f, 0.25f), 800.f, 1200.f);
	const FString Line = Last.bSuccess ? FString::Printf(TEXT("She took a beating on %s. We'll have her flying by morning."), *Last.Name)
		: FString::Printf(TEXT("%s chewed her up. Half her armour's on the floor. Give us a day."), *Last.Name);
	Person(Pad + FVector(700.f, 700.f, 100.f), -135.f, TEXT("Deck crew"), Line, 3);
	Person(Pad + FVector(-750.f, -650.f, 100.f), 40.f, TEXT("Deck crew"), TEXT("Pass me the torch. No, the other torch."), 3);
}

void ADVKestrelMemory::BuildTrophies(int32 Count)
{
	if (Count <= 0)
	{
		return;
	}
	// A shelf between the armoury and the lockers: each Bulwark head still has a dull red optic.
	const float Y = HalfWidth - 240.f;
	const float ShelfX = 600.f;
	for (int32 Level = 0; Level < 2; ++Level)
	{
		Part(DVKit::Box(), TEXT("Gunmetal"), FVector(ShelfX, Y, 150.f + Level * 180.f), FVector(860.f, 130.f, 16.f));
	}
	Part(DVKit::Box(), TEXT("Navy"), FVector(ShelfX, Y + 70.f, 250.f), FVector(880.f, 10.f, 360.f));
	Glow(FVector(ShelfX, Y - 66.f, 60.f), FVector(500.f, 4.f, 10.f), FLinearColor(1.f, 0.5f, 0.1f), 6.f);
	Lamp(FVector(ShelfX, Y - 300.f, 600.f), FLinearColor(1.f, 0.75f, 0.5f), 400.f, 900.f);
	for (int32 i = 0; i < Count && i < 8; ++i)
	{
		// A war-frame head: armoured skull, brow plate, and the optic slit that used to hunt us.
		const float X = ShelfX - 320.f + (i % 4) * 210.f;
		const float Z = 158.f + (i / 4) * 180.f + 55.f;
		Part(DVKit::Box(), TEXT("Gunmetal"), FVector(X, Y, Z), FVector(110.f, 100.f, 110.f));
		Part(DVKit::Box(), TEXT("Navy"), FVector(X, Y - 20.f, Z + 50.f), FVector(124.f, 80.f, 26.f));
		Glow(FVector(X, Y - 52.f, Z + 14.f), FVector(80.f, 4.f, 16.f), FLinearColor(1.f, 0.06f, 0.02f), 14.f);
	}
}

void ADVKestrelMemory::BuildEvacuees()
{
	const UDVRosterSubsystem* Roster = GetGameInstance()->GetSubsystem<UDVRosterSubsystem>();
	bool bHalden = false, bAlbion = false;
	for (const FDVOpRecord& Op : Roster->GetOpLog())
	{
		bHalden |= Op.bSuccess && Op.MapKey.Contains(TEXT("Breakwater"));
		bAlbion |= Op.bSuccess && Op.MapKey.Contains(TEXT("Proxima"));
	}
	if (bHalden)
	{
		// Port Halden families waiting for a berth: bedrolls on the deck and the people who sleep on them.
		for (int32 i = 0; i < 4; ++i)
		{
			Part(DVKit::Box(), TEXT("Olive"), FVector(2150.f + i * 150.f, 1250.f, 6.f), FVector(80.f, 190.f, 12.f));
		}
		Person(FVector(2300.f, 650.f, 100.f), -160.f, TEXT("Port Halden evacuee"), TEXT("You came back for us. I won't forget the boat, or the people on it."), 2);
		Person(FVector(2500.f, 850.f, 100.f), 170.f, TEXT("Port Halden harbour pilot"), TEXT("Forty years on those docks. Never thought I'd see them from orbit."), 1);
		Person(FVector(2150.f, 950.f, 100.f), -30.f, TEXT("Colony kid"), TEXT("Is this your ship? It's so big. Do you live here?"), 4);
		Person(FVector(2650.f, 600.f, 100.f), 150.f, TEXT("Colony kid"), TEXT("My dad says you're Delta. Are you Delta?"), 4);
	}
	if (bAlbion)
	{
		Person(FVector(2100.f, -700.f, 100.f), 30.f, TEXT("New Albion colonist"), TEXT("They say the landing field's ours again. We go home on the next transport."), 2);
		Person(FVector(2280.f, -850.f, 100.f), 120.f, TEXT("New Albion colonist"), TEXT("Thank you. I don't know what else to say. Thank you."), 1);
	}
}

void ADVKestrelMemory::BuildOpsBoard(const TArray<FDVOpRecord>& Log)
{
	// Every operation flown: a green light for a win, red for a loss, a white mark beneath for each soldier lost.
	const FVector Centre(-1500.f, -HalfWidth + 190.f, 1550.f);	// above the catwalk, readable from the deck
	Part(DVKit::Box(), TEXT("Gunmetal"), Centre, FVector(1400.f, 10.f, 520.f));
	Glow(Centre + FVector(0.f, 8.f, 230.f), FVector(1300.f, 4.f, 14.f), FLinearColor(0.1f, 0.7f, 1.f), 10.f);
	for (int32 i = 0; i < Log.Num() && i < 20; ++i)
	{
		const FVector Cell = Centre + FVector(-585.f + (i % 10) * 130.f, 8.f, 120.f - (i / 10) * 220.f);
		Glow(Cell, FVector(90.f, 4.f, 90.f), Log[i].bSuccess ? FLinearColor(0.1f, 1.f, 0.25f) : FLinearColor(1.f, 0.08f, 0.04f), 3.f);
		for (int32 k = 0; k < Log[i].Lost && k < 4; ++k)
		{
			Glow(Cell + FVector(-33.f + k * 22.f, 0.f, -70.f), FVector(14.f, 4.f, 14.f), FLinearColor(1.f, 1.f, 1.f), 6.f);
		}
	}
}

void ADVKestrelMemory::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float T = GetWorld()->GetTimeSeconds();
	for (int32 i = 0; i < Candles.Num(); ++i)
	{
		DVKit::SetGlow(Candles[i], FLinearColor(1.f, 0.55f, 0.15f), 18.f + 10.f * FMath::Sin(T * 9.f + i * 1.7f) * FMath::Sin(T * 3.1f + i));
	}
	if (WeldLight)
	{
		// A welder's arc: bright bursts with dark gaps.
		const bool bArc = FMath::Fmod(T * 1.3f, 2.f) < 1.1f && FMath::FRand() < 0.7f;
		WeldLight->SetIntensity(bArc ? FMath::FRandRange(600.f, 1500.f) : 0.f);
	}
}
