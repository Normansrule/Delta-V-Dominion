#include "Characters/DVTownsfolk.h"
#include "Core/DVProfileSubsystem.h"
#include "Core/DVGameMode.h"

#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// Workwear and market colours: dust, rust, faded dyes. Nobody in Rook's Rest wears Accord olive.
	const FLinearColor TownCloth[] = {
		FLinearColor(0.32f, 0.22f, 0.12f), FLinearColor(0.38f, 0.12f, 0.06f), FLinearColor(0.12f, 0.16f, 0.22f),
		FLinearColor(0.25f, 0.25f, 0.22f), FLinearColor(0.42f, 0.32f, 0.12f), FLinearColor(0.18f, 0.08f, 0.2f),
		FLinearColor(0.08f, 0.2f, 0.18f), FLinearColor(0.5f, 0.42f, 0.32f) };
	const FLinearColor TownTrim[] = {
		FLinearColor(1.f, 0.55f, 0.1f), FLinearColor(1.f, 0.2f, 0.5f), FLinearColor(0.2f, 0.8f, 1.f), FLinearColor(0.9f, 0.9f, 0.6f) };

	// [role][mood][variant]: mood 0 = the Free Ports are wary of Delta, 1 = neutral, 2 = trusted.
	const TCHAR* TownLines[5][3][2] = {
		{ { TEXT("Accord coin spends the same, but I count it twice."), TEXT("Buy something or move along, soldier.") },
		  { TEXT("Fresh coolant, real coffee, nothing that hums. Take a look."), TEXT("Prices went up when the Kestrel came back. Supply and demand.") },
		  { TEXT("For Delta? Mags says you're good for it. Take what you need."), TEXT("Heard you brought the Breakwater convoy home. Coffee's on me.") } },
		{ { TEXT("Watch where you put your boots. That's Ports cargo."), TEXT("Last Accord patrol through here took half a hold. 'Requisitioned.'") },
		  { TEXT("Third shift running. The war's good for freight, bad for sleep."), TEXT("Those crates? Medical. Probably. Don't ask.") },
		  { TEXT("You ever need something moved quiet, you find me."), TEXT("The Ports remember who pays. You pay.") } },
		{ { TEXT("Uniforms. Always uniforms."), TEXT("Go save somebody else's planet.") },
		  { TEXT("I was a miner on Ceres till the frames came. Now I'm this."), TEXT("They say CINDER talks to you if you listen to the static long enough.") },
		  { TEXT("You're the one who came back for the colonists. I heard."), TEXT("If you see my brother out there, Tomas Reyes, tell him to write.") } },
		{ { TEXT("I don't fix Accord rides. Bad for business."), TEXT("Your Raven's leaking. Not my problem.") },
		  { TEXT("Skiff bikes, salvage frames, whatever you've got. I fix it."), TEXT("That Pulse Carbine? Ports tech. You'll need friends here to carry one.") },
		  { TEXT("Bring me a Bulwark plate sometime. I'll make you something nice."), TEXT("Your Skiff's tuned. On the house, Commander.") } },
		{ { TEXT("Mum says don't talk to soldiers."), TEXT("...") },
		  { TEXT("Is it true the Kestrel is bigger than the whole town?"), TEXT("Can I see your helmet? Just for a second?") },
		  { TEXT("You're Delta! You bring people home!"), TEXT("When I grow up I'm going to fly a Raven.") } } };
}

ADVTownsfolk::ADVTownsfolk()
{
	PrimaryActorTick.bCanEverTick = true;
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 220.f, 0.f);
	GetCharacterMovement()->MaxWalkSpeed = 135.f;
	GetCharacterMovement()->bUseRVOAvoidance = true;

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Manny(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Quinn(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple"));
	static ConstructorHelpers::FClassFinder<UAnimInstance> Unarmed(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"));
	if (Manny.Succeeded())
	{
		GetMesh()->SetSkeletalMeshAsset(Manny.Object);
		GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -92.f), FRotator(0.f, -90.f, 0.f));
	}
	AltBody = Quinn.Object;
	if (Unarmed.Succeeded())
	{
		GetMesh()->SetAnimInstanceClass(Unarmed.Class);
	}
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
}

void ADVTownsfolk::BeginPlay()
{
	Super::BeginPlay();
	Home = GetActorLocation();
	Seed = GetTypeHash(GetName());
	FRandomStream Rng(Seed);
	SetActorScale3D(FVector(Trade == 4 ? 0.62f : Rng.FRandRange(0.94f, 1.05f)));	// trade 4: a colony kid
	if (AltBody && Rng.FRand() < 0.45f)
	{
		GetMesh()->SetSkeletalMeshAsset(AltBody);
	}
	const FLinearColor Cloth = TownCloth[Rng.RandRange(0, UE_ARRAY_COUNT(TownCloth) - 1)];
	const FLinearColor Trim = TownTrim[Rng.RandRange(0, UE_ARRAY_COUNT(TownTrim) - 1)];
	for (int32 i = 0; i < GetMesh()->GetNumMaterials(); ++i)
	{
		if (UMaterialInstanceDynamic* Mat = GetMesh()->CreateAndSetMaterialInstanceDynamic(i))
		{
			Mat->SetVectorParameterValue(TEXT("Paint Tint"), Cloth);
			Mat->SetVectorParameterValue(TEXT("LogoTint"), Trim);
			Mat->SetScalarParameterValue(TEXT("MetalPaintMetallic"), 0.f);
			Mat->SetScalarParameterValue(TEXT("MetalPaintRoughness"), 0.85f);
			Mat->SetScalarParameterValue(TEXT("EmissivePower"), 1.5f);
		}
	}
	GetCharacterMovement()->MaxWalkSpeed = Trade == 4 ? 220.f : Rng.FRandRange(110.f, 160.f);
	if (bLyingDown)
	{
		// On its back along the actor's +X (head end), face up; the idle animation reads as breathing.
		bWanders = false;
		GetCharacterMovement()->DisableMovement();
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		GetMesh()->SetRelativeLocationAndRotation(FVector(-85.f, 0.f, -70.f), FRotationMatrix::MakeFromYZ(FVector(0.f, 0.f, 1.f), FVector(1.f, 0.f, 0.f)).Rotator());
	}
	NextWander = GetWorld()->GetTimeSeconds() + Rng.FRandRange(0.5f, 5.f);
}

void ADVTownsfolk::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority())
	{
		return;
	}
	if (GetWorld()->GetTimeSeconds() < FleeUntil)
	{
		return;	// running from the shooting; nobody stops to chat
	}
	// Someone in a uniform walked up: stop and face them.
	const APawn* Visitor = nullptr;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APawn* P = It->Get() ? It->Get()->GetPawn() : nullptr;
		if (P && FVector::Dist2D(P->GetActorLocation(), GetActorLocation()) < TalkRange * 1.3f)
		{
			Visitor = P;
			break;
		}
	}
	AAIController* AI = Cast<AAIController>(GetController());
	if (Visitor && !bLyingDown)
	{
		if (AI && GetVelocity().SizeSquared() > 1.f)
		{
			AI->StopMovement();
		}
		const FRotator Want = (Visitor->GetActorLocation() - GetActorLocation()).GetSafeNormal2D().Rotation();
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), FRotator(0.f, Want.Yaw, 0.f), DeltaSeconds, 4.f));
		NextWander = GetWorld()->GetTimeSeconds() + 3.0;
		return;
	}
	if (bWanders && GetWorld()->GetTimeSeconds() >= NextWander)
	{
		Wander();
	}
}

void ADVTownsfolk::Wander()
{
	FRandomStream Rng(Seed + int32(GetWorld()->GetTimeSeconds() * 10.0));
	NextWander = GetWorld()->GetTimeSeconds() + Rng.FRandRange(6.f, 14.f);
	AAIController* AI = Cast<AAIController>(GetController());
	UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(GetWorld());
	FNavLocation Spot;
	if (AI && Nav && Nav->GetRandomReachablePointInRadius(Home, WanderRadius, Spot))
	{
		AI->MoveToLocation(Spot.Location, 60.f);
	}
}

ADVTownsfolk* ADVTownsfolk::FindNear(const AActor* Listener)
{
	if (!Listener)
	{
		return nullptr;
	}
	ADVTownsfolk* Best = nullptr;
	float BestDist = TalkRange;
	for (TActorIterator<ADVTownsfolk> It(Listener->GetWorld()); It; ++It)
	{
		const float D = FVector::Dist2D(It->GetActorLocation(), Listener->GetActorLocation());
		if (D < BestDist)
		{
			BestDist = D;
			Best = *It;
		}
	}
	return Best;
}

void ADVTownsfolk::OnShot(AActor* Shooter)
{
	const double Now = GetWorld()->GetTimeSeconds();
	if (!HasAuthority() || bLyingDown || Now - LastShot < 3.0)
	{
		return;	// one burst is one incident, not ten
	}
	LastShot = Now;
	FleeUntil = Now + 9.0;
	NextWander = FleeUntil;
	// Run directly away from whoever fired.
	const FVector From = Shooter ? Shooter->GetActorLocation() : GetActorLocation() - GetActorForwardVector() * 100.f;
	const FVector Away = (GetActorLocation() - From).GetSafeNormal2D();
	GetCharacterMovement()->MaxWalkSpeed = 420.f;
	UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(GetWorld());
	FNavLocation Spot;
	AAIController* AI = Cast<AAIController>(GetController());
	if (AI && Nav && Nav->ProjectPointToNavigation(GetActorLocation() + Away * 1100.f, Spot, FVector(600.f, 600.f, 400.f)))
	{
		AI->MoveToLocation(Spot.Location, 60.f);
	}
	FTimerHandle Calm;
	GetWorldTimerManager().SetTimer(Calm, FTimerDelegate::CreateWeakLambda(this, [this]() { GetCharacterMovement()->MaxWalkSpeed = 140.f; }), 9.f, false);
	if (ADVGameMode* GM = GetWorld()->GetAuthGameMode<ADVGameMode>())
	{
		GM->NotifyCivilianHurt(this, Shooter);
	}
}

void ADVTownsfolk::BlastNear(UWorld* World, const FVector& Center, float Radius, AActor* Shooter)
{
	if (!World)
	{
		return;
	}
	for (TActorIterator<ADVTownsfolk> It(World); It; ++It)
	{
		if (FVector::Dist(It->GetActorLocation(), Center) <= Radius)
		{
			It->OnShot(Shooter);
		}
	}
}

FString ADVTownsfolk::Line(int32 FreePortsRep) const
{
	if (!CustomLine.IsEmpty())
	{
		return CustomLine;
	}
	const int32 Mood = FreePortsRep < -15 ? 0 : FreePortsRep < DVFactions::Trusted ? 1 : 2;
	return TownLines[FMath::Clamp(Trade, 0, 4)][Mood][(Seed & 0x7fffffff) % 2];
}
