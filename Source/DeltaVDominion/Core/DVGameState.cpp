#include "Core/DVGameState.h"
#include "Core/DVProfileSubsystem.h"
#include "Core/DVPlayerController.h"
#include "Engine/GameInstance.h"
#include "Net/UnrealNetwork.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Core/DVFx.h"
#include "Vehicles/DVKestrel.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/PlayerStart.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Engine/Level.h"
#include "UObject/ConstructorHelpers.h"

ADVGameState::ADVGameState()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GreyBox(TEXT("/Game/Dev/M_GreyBox.M_GreyBox"));
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> Explosion(TEXT("/Niagara/DefaultAssets/Templates/Systems/SimpleExplosion.SimpleExplosion"));
	BeamMesh = Cylinder.Object;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	SphereMesh = SphereFinder.Object;
	BeamMaterial = GreyBox.Object;
	ExplosionFX = Explosion.Object;
}

void ADVGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADVGameState, Phase);
	DOREPLIFETIME(ADVGameState, MissionName);
	DOREPLIFETIME(ADVGameState, Ascent);
	DOREPLIFETIME(ADVGameState, bWalkerOp);
	DOREPLIFETIME(ADVGameState, ObjectiveText);
	DOREPLIFETIME(ADVGameState, EnemiesTotal);
	DOREPLIFETIME(ADVGameState, EnemiesRemaining);
	DOREPLIFETIME(ADVGameState, AfterAction);
	DOREPLIFETIME(ADVGameState, MissionStartTime);
	DOREPLIFETIME(ADVGameState, MissionEndTime);
	DOREPLIFETIME(ADVGameState, MissionLocation);
	DOREPLIFETIME(ADVGameState, MissionSubtitle);
	DOREPLIFETIME(ADVGameState, OrbitalReadyTime);
	DOREPLIFETIME(ADVGameState, SupplyReadyTime);
	DOREPLIFETIME(ADVGameState, bMultiStage);
	DOREPLIFETIME(ADVGameState, Stage);
	DOREPLIFETIME(ADVGameState, ObjectiveLocation);
	DOREPLIFETIME(ADVGameState, ExtractLocation);
	DOREPLIFETIME(ADVGameState, StageProgress);
	DOREPLIFETIME(ADVGameState, StageEndTime);
	DOREPLIFETIME(ADVGameState, StrafeReadyTime);
}

void ADVGameState::MulticastKillFeed_Implementation(const FString& Killer, const FString& Victim, bool bFriendlyLoss)
{
	FDVKillFeedEntry& Entry = KillFeed.AddDefaulted_GetRef();
	Entry.Killer = Killer;
	Entry.Victim = Victim;
	Entry.bFriendlyLoss = bFriendlyLoss;
	Entry.Time = GetWorld()->GetTimeSeconds();
	if (KillFeed.Num() > 6)
	{
		KillFeed.RemoveAt(0);
	}
}

void ADVGameState::BeginPlay()
{
	Super::BeginPlay();
	// Ambience: hull hum and vents aboard the Kestrel, wind on every planet.
	const FString Map = GetWorld()->GetMapName();
	if (Map.Contains(TEXT("Kestrel")))
	{
		DVSound::Loop(GetWorld(), nullptr, TEXT("SW_AmbHangar"), 0.5f);
	}
	else if (!Map.Contains(TEXT("Galaxy")))
	{
		DVSound::Loop(GetWorld(), nullptr, TEXT("SW_AmbWind"), 0.4f);
	}
}

void ADVGameState::MulticastReputation_Implementation(uint8 Faction, int32 Delta, const FString& Reason)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (UDVProfileSubsystem* Profile = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDVProfileSubsystem>() : nullptr)
	{
		Profile->AddRep(Faction, Delta, Reason);
	}
	DVSound::UI(GetWorld(), Delta > 0 ? TEXT("SW_KillConfirm") : TEXT("SW_RadioSquelch"), 0.5f);
	// Let the server's copy of this player's standing catch up (it gates call-in bonuses).
	if (ADVPlayerController* PC = Cast<ADVPlayerController>(GetWorld()->GetFirstPlayerController()))
	{
		PC->PushReputation();
	}
}

void ADVGameState::MulticastContractEvent_Implementation(uint8 Kind)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (UDVProfileSubsystem* Profile = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDVProfileSubsystem>() : nullptr)
	{
		const bool bWasDone = Profile->IsContractDone();
		Profile->ContractEvent(Kind);
		if (Profile->IsContractDone() != bWasDone)
		{
			DVSound::UI(GetWorld(), TEXT("SW_KillConfirm"), 0.7f);
		}
	}
}

void ADVGameState::MulticastStreamHangar_Implementation(FVector_NetQuantize Location, float Yaw)
{
	if (HangarLevel)
	{
		return;
	}
	if (ADVKestrel* Kestrel = ADVKestrel::Find(GetWorld()))
	{
		Kestrel->HoldStation();
	}
	bool bOk = false;
	HangarLevel = ULevelStreamingDynamic::LoadLevelInstance(GetWorld(), TEXT("/Game/Maps/Kestrel"), FVector(Location), FRotator(0.f, Yaw, 0.f), bOk,
		TEXT("KestrelHangarDeck"));
	if (HangarLevel)
	{
		HangarLevel->OnLevelShown.AddDynamic(this, &ADVGameState::OnHangarShown);
	}
	UE_LOG(LogTemp, Log, TEXT("DVAscent: streaming hangar at %s yaw %.0f (%s)"), *FVector(Location).ToCompactString(), Yaw, bOk ? TEXT("ok") : TEXT("failed"));
}

void ADVGameState::OnHangarShown()
{
	// The hangar map brings its own sky, sun and a 60 km Mars for the view out of the mouth. Here the real
	// battlefield is outside the mouth, so keep only the ship.
	ULevel* Level = HangarLevel ? HangarLevel->GetLoadedLevel() : nullptr;
	if (!Level)
	{
		return;
	}
	int32 Removed = 0;
	for (AActor* A : Level->Actors)
	{
		if (!A)
		{
			continue;
		}
		bool bStrip = A->IsA<ADirectionalLight>() || A->IsA<ASkyLight>() || A->IsA<APostProcessVolume>() || A->IsA<AExponentialHeightFog>()
			|| A->IsA<APlayerStart>() || A->FindComponentByClass<USkyAtmosphereComponent>() != nullptr;
		if (const AStaticMeshActor* Mesh = Cast<AStaticMeshActor>(A))
		{
			FVector Origin, Extent;
			Mesh->GetActorBounds(false, Origin, Extent);
			bStrip |= Extent.GetMax() > 50000.f;	// Mars and Phobos
		}
		if (bStrip)
		{
			A->Destroy();
			++Removed;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("DVAscent: hangar shown, stripped %d actors"), Removed);
}

bool ADVGameState::IsHangarReady() const
{
	return HangarLevel && HangarLevel->IsLevelVisible();
}

void ADVGameState::MulticastSound_Implementation(FName Sound, FVector_NetQuantize Location, float Volume, uint8 Range)
{
	const FString Name = Sound.ToString();
	if (FVector(Location).IsNearlyZero())
	{
		DVSound::UI(GetWorld(), *Name, Volume);
	}
	else
	{
		DVSound::At(GetWorld(), *Name, Location, Volume, Range, 0.f);
	}
}

void ADVGameState::MulticastRadio_Implementation(const FString& Speaker, const FString& Text)
{
	DVSound::UI(GetWorld(), TEXT("SW_RadioSquelch"), 0.5f);
	FDVRadioLine& Line = Radio.AddDefaulted_GetRef();
	Line.Speaker = Speaker;
	Line.Text = Text;
	Line.Time = GetWorld()->GetTimeSeconds();
	if (Radio.Num() > 4)
	{
		Radio.RemoveAt(0);
	}
}

static AActor* SpawnFxHost(UWorld* World, const FVector& At, float LifeSpan)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Host = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(At), Params);
	if (Host)
	{
		USceneComponent* Root = NewObject<USceneComponent>(Host, TEXT("Root"));
		Host->SetRootComponent(Root);
		Root->RegisterComponent();
		Host->SetLifeSpan(LifeSpan);
	}
	return Host;
}

bool ADVGameState::GetPendingStrike(FVector& OutLocation, double& OutImpactTime) const
{
	if (PendingStrikeImpactTime < 0.0)
	{
		return false;
	}
	OutLocation = PendingStrikeLocation;
	OutImpactTime = PendingStrikeImpactTime;
	return true;
}

void ADVGameState::MulticastOrbitalStrike_Implementation(FVector_NetQuantize Location, float Delay)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	PendingStrikeLocation = Location;
	PendingStrikeImpactTime = GetWorld()->GetTimeSeconds() + Delay;

	// Targeting beam: a thin red column from the sky with a pulsing light at its base.
	UStaticMesh* Cylinder = BeamMesh;
	UMaterialInterface* GreyBox = BeamMaterial;
	AActor* BeamHost = SpawnFxHost(GetWorld(), Location, Delay + 1.f);
	if (!BeamHost)
	{
		return;
	}
	UStaticMeshComponent* Beam = NewObject<UStaticMeshComponent>(BeamHost);
	Beam->SetStaticMesh(Cylinder);
	Beam->SetMaterial(0, GreyBox);
	Beam->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Beam->CastShadow = false;
	Beam->SetUsingAbsoluteLocation(true);
	Beam->SetUsingAbsoluteScale(true);
	Beam->RegisterComponent();
	// Targeting laser: from the Kestrel's battery when the ship is overhead, otherwise straight down from orbit.
	const ADVKestrel* Kestrel = ADVKestrel::Find(GetWorld());
	const FVector BeamFrom = Kestrel ? Kestrel->GetBatteryLocation() : FVector(Location) + FVector(0.f, 0.f, 5000.f);
	const FVector BeamDelta = BeamFrom - FVector(Location);
	Beam->SetWorldLocationAndRotation((BeamFrom + FVector(Location)) * 0.5f, FRotationMatrix::MakeFromZ(BeamDelta).Rotator());
	Beam->SetWorldScale3D(FVector(0.35f, 0.35f, BeamDelta.Size() / 100.f));
	Beam->SetUsingAbsoluteRotation(true);
	if (UMaterialInstanceDynamic* Mat = Beam->CreateAndSetMaterialInstanceDynamic(0))
	{
		Mat->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.1f, 0.05f));
		Mat->SetVectorParameterValue(TEXT("Emissive"), FLinearColor(120.f, 8.f, 2.f));
	}
	UPointLightComponent* Marker = NewObject<UPointLightComponent>(BeamHost);
	Marker->SetUsingAbsoluteLocation(true);
	Marker->RegisterComponent();
	Marker->SetWorldLocation(FVector(Location) + FVector(0.f, 0.f, 60.f));
	Marker->SetIntensityUnits(ELightUnits::Candelas);
	Marker->SetIntensity(1500.f);
	Marker->SetLightColor(FLinearColor(1.f, 0.1f, 0.05f));
	Marker->SetAttenuationRadius(900.f);

	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, Beam, Marker, Location]()
	{
		Beam->DestroyComponent();
		Marker->DestroyComponent();
		PendingStrikeImpactTime = -1.0;
		PlayOrbitalImpact(Location);
	}), FMath::Max(Delay, 0.01f), false);
}

void ADVGameState::PlayOrbitalImpact(FVector Location)
{
	if (UNiagaraSystem* Explosion = ExplosionFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Explosion, Location, FRotator::ZeroRotator, FVector(6.f));
	}
	if (ADVKestrel* Kestrel = ADVKestrel::Find(GetWorld()))
	{
		Kestrel->FireMainBattery(Location);	// the round visibly comes from the ship overhead
	}
	DVFx::Explosion(GetWorld(), Location, 1.f);
}
