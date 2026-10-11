#include "Core/DVGameState.h"
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
	DOREPLIFETIME(ADVGameState, ObjectiveText);
	DOREPLIFETIME(ADVGameState, EnemiesTotal);
	DOREPLIFETIME(ADVGameState, EnemiesRemaining);
	DOREPLIFETIME(ADVGameState, AfterAction);
	DOREPLIFETIME(ADVGameState, MissionStartTime);
	DOREPLIFETIME(ADVGameState, MissionEndTime);
	DOREPLIFETIME(ADVGameState, MissionLocation);
	DOREPLIFETIME(ADVGameState, MissionSubtitle);
	DOREPLIFETIME(ADVGameState, OrbitalReadyTime);
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

void ADVGameState::MulticastRadio_Implementation(const FString& Speaker, const FString& Text)
{
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
	Beam->SetWorldLocation(FVector(Location) + FVector(0.f, 0.f, 2500.f));
	Beam->SetWorldScale3D(FVector(0.35f, 0.35f, 50.f));
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

	// Procedural fireball and smoke column: emissive spheres that bloom out and cool,
	// then dark spheres that billow upward. No art assets needed; real VFX can replace it later.
	struct FBlast
	{
		double Start = 0.0;
		TArray<TPair<UStaticMeshComponent*, FVector>> Fire;   // component, end scale
		TArray<TPair<UStaticMeshComponent*, FVector>> Smoke;  // component, drift velocity
		UPointLightComponent* Flash = nullptr;
	};
	AActor* Host = SpawnFxHost(GetWorld(), Location, 6.f);
	if (!Host)
	{
		return;
	}
	TSharedRef<FBlast> Blast = MakeShared<FBlast>();
	Blast->Start = GetWorld()->GetTimeSeconds();
	UStaticMesh* Sphere = SphereMesh;
	auto MakeBall = [this, Sphere, Host](const FVector& At, const FLinearColor& Color, const FLinearColor& Emissive)
	{
		UStaticMeshComponent* Ball = NewObject<UStaticMeshComponent>(Host);
		Ball->SetStaticMesh(Sphere ? Sphere : BeamMesh.Get());
		Ball->SetMaterial(0, BeamMaterial);
		Ball->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Ball->SetUsingAbsoluteLocation(true);
		Ball->SetUsingAbsoluteScale(true);
		Ball->RegisterComponent();
		Ball->SetWorldLocation(At);
		Ball->SetWorldScale3D(FVector(0.1f));
		if (UMaterialInstanceDynamic* Mat = Ball->CreateAndSetMaterialInstanceDynamic(0))
		{
			Mat->SetVectorParameterValue(TEXT("Color"), Color);
			Mat->SetVectorParameterValue(TEXT("Emissive"), Emissive);
		}
		return Ball;
	};
	for (int32 i = 0; i < 7; ++i)
	{
		const FVector Offset = FMath::VRand() * FMath::FRandRange(60.f, 260.f) * FVector(1.f, 1.f, 0.6f) + FVector(0.f, 0.f, 120.f);
		Blast->Fire.Add({ MakeBall(Location + Offset, FLinearColor(1.f, 0.5f, 0.1f), FLinearColor(300.f, 90.f, 15.f)), FVector(FMath::FRandRange(4.f, 7.5f)) });
	}
	for (int32 i = 0; i < 16; ++i)
	{
		const FVector Offset = FMath::VRand() * FMath::FRandRange(80.f, 300.f) * FVector(1.f, 1.f, 0.3f);
		const FVector Drift = FVector(FMath::FRandRange(-60.f, 60.f), FMath::FRandRange(-60.f, 60.f), FMath::FRandRange(220.f, 420.f));
		Blast->Smoke.Add({ MakeBall(Location + Offset + FVector(0.f, 0.f, 80.f), FLinearColor(0.008f, 0.007f, 0.006f), FLinearColor::Black), Drift });
	}
	Blast->Flash = NewObject<UPointLightComponent>(Host);
	Blast->Flash->SetUsingAbsoluteLocation(true);
	Blast->Flash->RegisterComponent();
	Blast->Flash->SetWorldLocation(Location + FVector(0.f, 0.f, 300.f));
	Blast->Flash->SetIntensityUnits(ELightUnits::Candelas);
	Blast->Flash->SetIntensity(300000.f);
	Blast->Flash->SetLightColor(FLinearColor(1.f, 0.6f, 0.3f));
	Blast->Flash->SetAttenuationRadius(7000.f);

	TSharedRef<FTimerHandle> Handle = MakeShared<FTimerHandle>();
	GetWorldTimerManager().SetTimer(*Handle, FTimerDelegate::CreateWeakLambda(Host, [this, Blast, Handle]()
	{
		const float T = static_cast<float>(GetWorld()->GetTimeSeconds() - Blast->Start);
		const float FireT = FMath::Clamp(T / 0.9f, 0.f, 1.f);
		for (const TPair<UStaticMeshComponent*, FVector>& Ball : Blast->Fire)
		{
			const float Grow = 1.f - FMath::Pow(1.f - FMath::Clamp(T / 0.35f, 0.f, 1.f), 3.f);
			Ball.Key->SetWorldScale3D(Ball.Value * (0.2f + 0.8f * Grow) * (1.f + 0.3f * FireT));
			if (UMaterialInstanceDynamic* Mat = Cast<UMaterialInstanceDynamic>(Ball.Key->GetMaterial(0)))
			{
				const float Heat = FMath::Square(1.f - FireT);
				Mat->SetVectorParameterValue(TEXT("Emissive"), FLinearColor(300.f, 90.f, 15.f) * Heat);
				Mat->SetVectorParameterValue(TEXT("Color"), FMath::Lerp(FLinearColor(1.f, 0.5f, 0.1f), FLinearColor(0.04f, 0.03f, 0.02f), FireT));
			}
			Ball.Key->SetVisibility(T < 1.2f);
		}
		const float SmokeT = FMath::Clamp(T / 4.f, 0.f, 1.f);
		for (const TPair<UStaticMeshComponent*, FVector>& Puff : Blast->Smoke)
		{
			Puff.Key->SetWorldLocation(Puff.Key->GetComponentLocation() + Puff.Value * (1.f / 60.f) * (1.f - SmokeT));
			Puff.Key->SetWorldScale3D(FVector(0.8f + 4.5f * FMath::Sqrt(SmokeT)) * FMath::Clamp((4.5f - T) / 2.f, 0.f, 1.f));
		}
		Blast->Flash->SetIntensity(300000.f * FMath::Square(FMath::Clamp(1.f - T / 0.6f, 0.f, 1.f)));
		if (T > 4.5f)
		{
			for (const auto& Ball : Blast->Fire) { Ball.Key->DestroyComponent(); }
			for (const auto& Puff : Blast->Smoke) { Puff.Key->DestroyComponent(); }
			Blast->Flash->DestroyComponent();
			GetWorldTimerManager().ClearTimer(*Handle);
		}
	}), 1.f / 60.f, true);

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get(); PC && PC->IsLocalController() && PC->PlayerCameraManager)
		{
			const float Dist = FVector::Dist(PC->PlayerCameraManager->GetCameraLocation(), Location);
			const float Strength = FMath::Clamp(1.f - Dist / 6000.f, 0.f, 1.f);
			if (Strength > 0.f)
			{
				// Kick the view: cheap, readable "camera shake" without needing a shake asset.
				FRotator Rot = PC->GetControlRotation();
				Rot.Pitch += FMath::FRandRange(1.5f, 3.f) * Strength;
				Rot.Yaw += FMath::FRandRange(-2.f, 2.f) * Strength;
				PC->SetControlRotation(Rot);
			}
		}
	}
}
