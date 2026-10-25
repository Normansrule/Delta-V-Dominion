#include "Core/DVNeonVerge.h"
#include "Core/DVFx.h"
#include "DeltaVDominion.h"

#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Navigation/PathFollowingComponent.h"
#include "UObject/ConstructorHelpers.h"

ADVCourier::ADVCourier()
{
	PrimaryActorTick.bCanEverTick = true;
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	bUseControllerRotationYaw = false;
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0.f, 720.f, 0.f);
	Move->MaxWalkSpeed = RunSpeed;
	Move->bRunPhysicsWithNoController = true;
	// The sample he carries is the only clean copy: rounds and foam go round him.
	GetCapsuleComponent()->SetCollisionResponseToChannel(DV_TRACE_WEAPON, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(DV_TRACE_WEAPON, ECR_Ignore);

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Quinn(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple"));
	static ConstructorHelpers::FClassFinder<UAnimInstance> Unarmed(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"));
	if (Quinn.Succeeded())
	{
		GetMesh()->SetSkeletalMeshAsset(Quinn.Object);
		GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -92.f), FRotator(0.f, -90.f, 0.f));
	}
	if (Unarmed.Succeeded())
	{
		GetMesh()->SetAnimInstanceClass(Unarmed.Class);
	}
}

void ADVCourier::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADVCourier, bRunning);
	DOREPLIFETIME(ADVCourier, bCaught);
	DOREPLIFETIME(ADVCourier, bEscaped);
}

ADVCourier* ADVCourier::Find(const UWorld* World)
{
	if (World)
	{
		for (TActorIterator<ADVCourier> It(const_cast<UWorld*>(World)); It; ++It)
		{
			return *It;
		}
	}
	return nullptr;
}

void ADVCourier::BeginPlay()
{
	Super::BeginPlay();
	// A broker's runner: black jacket, hot orange trim, so he reads against the blue city.
	for (int32 i = 0; i < GetMesh()->GetNumMaterials(); ++i)
	{
		if (UMaterialInstanceDynamic* Mat = GetMesh()->CreateAndSetMaterialInstanceDynamic(i))
		{
			Mat->SetVectorParameterValue(TEXT("Paint Tint"), FLinearColor(0.02f, 0.02f, 0.025f));
			Mat->SetVectorParameterValue(TEXT("LogoTint"), FLinearColor(1.f, 0.42f, 0.05f));
			Mat->SetScalarParameterValue(TEXT("MetalPaintMetallic"), 0.3f);
			Mat->SetScalarParameterValue(TEXT("MetalPaintRoughness"), 0.35f);
			Mat->SetScalarParameterValue(TEXT("EmissivePower"), 30.f);
		}
	}
	if (!HasAuthority())
	{
		return;
	}
	if (!GetController())
	{
		SpawnDefaultController();
	}
	// The route: every actor tagged DV_Route_NN, in order.
	TArray<TPair<FString, FVector>> Points;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		for (const FName& Tag : It->Tags)
		{
			if (Tag.ToString().StartsWith(TEXT("DV_Route_")))
			{
				Points.Emplace(Tag.ToString(), It->GetActorLocation());
			}
		}
	}
	Points.Sort([](const TPair<FString, FVector>& A, const TPair<FString, FVector>& B) { return A.Key < B.Key; });
	for (const TPair<FString, FVector>& P : Points)
	{
		Route.Add(P.Value);
	}
}

void ADVCourier::StartRunning()
{
	if (!HasAuthority() || bRunning || bCaught || Route.Num() == 0)
	{
		return;
	}
	bRunning = true;
	Next = 0;
	LastProgress = GetWorld()->GetTimeSeconds();
	LastSpot = GetActorLocation();
}

void ADVCourier::Catch()
{
	if (!HasAuthority() || bCaught || bEscaped)
	{
		return;
	}
	bCaught = true;
	bRunning = false;
	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		AI->StopMovement();
	}
	OnRep_Caught();
}

void ADVCourier::OnRep_Caught()
{
	if (bCaught)
	{
		GetCharacterMovement()->StopMovementImmediately();
		DVFx::Flash(GetWorld(), GetActorLocation() + FVector(0.f, 0.f, 60.f), FLinearColor(1.f, 0.5f, 0.1f), 900.f, 500.f, 0.3f);
	}
}

void ADVCourier::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || !bRunning || bCaught || bEscaped)
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (FVector::Dist2D(GetActorLocation(), Route[Next]) < 160.f)
	{
		++Next;
		if (Next >= Route.Num())
		{
			bRunning = false;
			bEscaped = true;	// on the maglev and away
			return;
		}
	}
	AAIController* AI = Cast<AAIController>(GetController());
	if (AI && AI->GetMoveStatus() != EPathFollowingStatus::Moving)
	{
		AI->MoveToLocation(Route[Next], 80.f, false);
	}
	// If the navmesh can't get him there (a ramp it missed), he runs straight at the next point.
	if (FVector::Dist2D(GetActorLocation(), LastSpot) > 120.f)
	{
		LastSpot = GetActorLocation();
		LastProgress = Now;
	}
	else if (Now - LastProgress > 0.6)
	{
		AddMovementInput((Route[Next] - GetActorLocation()).GetSafeNormal2D(), 1.f);
	}
}
