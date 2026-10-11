#include "Core/DVFx.h"
#include "Characters/DVSoldierCharacter.h"
#include "Combat/DVCombatEffects.h"
#include "DVGameplayTags.h"

#include "AbilitySystemComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"

namespace
{
	UStaticMesh* SphereMesh() { static TWeakObjectPtr<UStaticMesh> M; if (!M.IsValid()) { M = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")); } return M.Get(); }
	UStaticMesh* CylinderMesh() { static TWeakObjectPtr<UStaticMesh> M; if (!M.IsValid()) { M = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")); } return M.Get(); }
	UMaterialInterface* GlowMaterial() { static TWeakObjectPtr<UMaterialInterface> M; if (!M.IsValid()) { M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dev/M_GreyBox.M_GreyBox")); } return M.Get(); }

	AActor* SpawnHost(UWorld* World, const FVector& At, float LifeSpan)
	{
		if (!World || World->GetNetMode() == NM_DedicatedServer)
		{
			return nullptr;
		}
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

	UStaticMeshComponent* MakeMesh(AActor* Host, UStaticMesh* Mesh, const FLinearColor& Color, const FLinearColor& Emissive)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Host);
		C->SetStaticMesh(Mesh);
		C->SetMaterial(0, GlowMaterial());
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->CastShadow = false;
		C->SetUsingAbsoluteLocation(true);
		C->SetUsingAbsoluteRotation(true);
		C->SetUsingAbsoluteScale(true);
		C->RegisterComponent();
		if (UMaterialInstanceDynamic* Mat = C->CreateAndSetMaterialInstanceDynamic(0))
		{
			Mat->SetVectorParameterValue(TEXT("Color"), Color);
			Mat->SetVectorParameterValue(TEXT("Emissive"), Emissive);
		}
		return C;
	}
}

void DVFx::Beam(UWorld* World, const FVector& From, const FVector& To, const FLinearColor& Emissive, float Width, float Duration)
{
	const FVector Delta = To - From;
	AActor* Host = SpawnHost(World, From, Duration + 0.5f);
	if (!Host || Delta.IsNearlyZero())
	{
		return;
	}
	UStaticMeshComponent* Beam = MakeMesh(Host, CylinderMesh(), FLinearColor(1.f, 0.8f, 0.5f), Emissive);
	Beam->SetWorldLocationAndRotation((From + To) * 0.5f, FRotationMatrix::MakeFromZ(Delta).Rotator());
	Beam->SetWorldScale3D(FVector(Width / 100.f, Width / 100.f, Delta.Size() / 100.f));
	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(Host, [Host]() { Host->Destroy(); }), Duration, false);
}

void DVFx::Flash(UWorld* World, const FVector& Location, const FLinearColor& Color, float Candelas, float Radius, float Duration)
{
	AActor* Host = SpawnHost(World, Location, Duration + 0.5f);
	if (!Host)
	{
		return;
	}
	UPointLightComponent* Light = NewObject<UPointLightComponent>(Host);
	Light->SetUsingAbsoluteLocation(true);
	Light->RegisterComponent();
	Light->SetWorldLocation(Location);
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(Candelas);
	Light->SetLightColor(Color);
	Light->SetAttenuationRadius(Radius);
	Light->SetCastShadows(false);
	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(Host, [Host]() { Host->Destroy(); }), Duration, false);
}

void DVFx::Explosion(UWorld* World, const FVector& Location, float Scale)
{
	AActor* Host = SpawnHost(World, Location, 6.f);
	if (!Host)
	{
		return;
	}
	struct FBlast
	{
		double Start = 0.0;
		float Scale = 1.f;
		TArray<TPair<UStaticMeshComponent*, FVector>> Fire;
		TArray<TPair<UStaticMeshComponent*, FVector>> Smoke;
		UPointLightComponent* Light = nullptr;
	};
	TSharedRef<FBlast> Blast = MakeShared<FBlast>();
	Blast->Start = World->GetTimeSeconds();
	Blast->Scale = Scale;

	// Fireball: emissive spheres that bloom out and cool. Smoke: dark spheres that billow up and fade.
	const int32 FireCount = Scale > 0.6f ? 7 : 4;
	for (int32 i = 0; i < FireCount; ++i)
	{
		const FVector Offset = (FMath::VRand() * FMath::FRandRange(60.f, 260.f) * FVector(1.f, 1.f, 0.6f) + FVector(0.f, 0.f, 120.f)) * Scale;
		UStaticMeshComponent* Ball = MakeMesh(Host, SphereMesh(), FLinearColor(1.f, 0.5f, 0.1f), FLinearColor(300.f, 90.f, 15.f));
		Ball->SetWorldLocation(Location + Offset);
		Ball->SetWorldScale3D(FVector(0.1f));
		Blast->Fire.Add({ Ball, FVector(FMath::FRandRange(4.f, 7.5f) * Scale) });
	}
	const int32 SmokeCount = Scale > 0.6f ? 16 : 8;
	for (int32 i = 0; i < SmokeCount; ++i)
	{
		const FVector Offset = FMath::VRand() * FMath::FRandRange(80.f, 300.f) * FVector(1.f, 1.f, 0.3f) * Scale;
		UStaticMeshComponent* Puff = MakeMesh(Host, SphereMesh(), FLinearColor(0.008f, 0.007f, 0.006f), FLinearColor::Black);
		Puff->SetWorldLocation(Location + Offset + FVector(0.f, 0.f, 80.f * Scale));
		Puff->SetWorldScale3D(FVector(0.1f));
		Blast->Smoke.Add({ Puff, FVector(FMath::FRandRange(-60.f, 60.f), FMath::FRandRange(-60.f, 60.f), FMath::FRandRange(220.f, 420.f)) * Scale });
	}
	Blast->Light = NewObject<UPointLightComponent>(Host);
	Blast->Light->SetUsingAbsoluteLocation(true);
	Blast->Light->RegisterComponent();
	Blast->Light->SetWorldLocation(Location + FVector(0.f, 0.f, 300.f * Scale));
	Blast->Light->SetIntensityUnits(ELightUnits::Candelas);
	Blast->Light->SetLightColor(FLinearColor(1.f, 0.6f, 0.3f));
	Blast->Light->SetAttenuationRadius(7000.f * Scale);

	TSharedRef<FTimerHandle> Handle = MakeShared<FTimerHandle>();
	TWeakObjectPtr<UWorld> WeakWorld = World;
	World->GetTimerManager().SetTimer(*Handle, FTimerDelegate::CreateWeakLambda(Host, [Blast, Handle, WeakWorld]()
	{
		UWorld* W = WeakWorld.Get();
		if (!W)
		{
			return;
		}
		const float T = static_cast<float>(W->GetTimeSeconds() - Blast->Start);
		const float FireT = FMath::Clamp(T / 0.9f, 0.f, 1.f);
		for (const TPair<UStaticMeshComponent*, FVector>& Ball : Blast->Fire)
		{
			const float Grow = 1.f - FMath::Pow(1.f - FMath::Clamp(T / 0.35f, 0.f, 1.f), 3.f);
			Ball.Key->SetWorldScale3D(Ball.Value * (0.2f + 0.8f * Grow) * (1.f + 0.3f * FireT));
			if (UMaterialInstanceDynamic* Mat = Cast<UMaterialInstanceDynamic>(Ball.Key->GetMaterial(0)))
			{
				Mat->SetVectorParameterValue(TEXT("Emissive"), FLinearColor(300.f, 90.f, 15.f) * FMath::Square(1.f - FireT));
				Mat->SetVectorParameterValue(TEXT("Color"), FMath::Lerp(FLinearColor(1.f, 0.5f, 0.1f), FLinearColor(0.04f, 0.03f, 0.02f), FireT));
			}
			Ball.Key->SetVisibility(T < 1.2f);
		}
		const float SmokeT = FMath::Clamp(T / 4.f, 0.f, 1.f);
		for (const TPair<UStaticMeshComponent*, FVector>& Puff : Blast->Smoke)
		{
			Puff.Key->SetWorldLocation(Puff.Key->GetComponentLocation() + Puff.Value * (1.f / 60.f) * (1.f - SmokeT));
			Puff.Key->SetWorldScale3D(FVector((0.8f + 4.5f * FMath::Sqrt(SmokeT)) * Blast->Scale) * FMath::Clamp((4.5f - T) / 2.f, 0.f, 1.f));
		}
		Blast->Light->SetIntensity(300000.f * Blast->Scale * FMath::Square(FMath::Clamp(1.f - T / 0.6f, 0.f, 1.f)));
		if (T > 4.5f)
		{
			W->GetTimerManager().ClearTimer(*Handle);
		}
	}), 1.f / 60.f, true);

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get(); PC && PC->IsLocalController() && PC->PlayerCameraManager)
		{
			const float Dist = FVector::Dist(PC->PlayerCameraManager->GetCameraLocation(), Location);
			const float Strength = FMath::Clamp(1.f - Dist / (6000.f * Scale), 0.f, 1.f);
			if (Strength > 0.f)
			{
				FRotator Rot = PC->GetControlRotation();
				Rot.Pitch += FMath::FRandRange(1.5f, 3.f) * Strength;
				Rot.Yaw += FMath::FRandRange(-2.f, 2.f) * Strength;
				PC->SetControlRotation(Rot);
			}
		}
	}
}

void DVCombat::ApplyDamage(ADVSoldierCharacter* Shooter, AActor* Target, float Damage, bool bFriendlyFire)
{
	ADVSoldierCharacter* Victim = Cast<ADVSoldierCharacter>(Target);
	UAbilitySystemComponent* SourceASC = Shooter ? Shooter->GetAbilitySystemComponent() : nullptr;
	if (!Victim || !SourceASC || Victim->IsDead() || !Shooter->HasAuthority())
	{
		return;
	}
	if (!bFriendlyFire && Victim->TeamId == Shooter->TeamId)
	{
		return;
	}
	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddInstigator(Shooter, Shooter);
	FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(UDVDamageEffect::StaticClass(), 1.f, Context);
	if (Spec.IsValid())
	{
		Spec.Data->SetSetByCallerMagnitude(DVTags::SetByCaller_Damage, Damage);
		SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), Victim->GetAbilitySystemComponent());
	}
}

void DVCombat::RadialDamage(ADVSoldierCharacter* Shooter, const FVector& Center, float Radius, float Damage, bool bFriendlyFire)
{
	if (!Shooter || !Shooter->GetWorld())
	{
		return;
	}
	for (TActorIterator<ADVSoldierCharacter> It(Shooter->GetWorld()); It; ++It)
	{
		const float Dist = FVector::Dist(It->GetActorLocation(), Center);
		if (Dist <= Radius)
		{
			ApplyDamage(Shooter, *It, Damage * (1.f - 0.7f * Dist / Radius), bFriendlyFire);
		}
	}
}
