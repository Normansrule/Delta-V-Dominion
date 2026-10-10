#include "Characters/DVCommandComponent.h"
#include "Characters/DVSoldierCharacter.h"
#include "Characters/DVSquadAIController.h"
#include "DeltaVDominion.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

UDVCommandComponent::UDVCommandComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
}

void UDVCommandComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UDVCommandComponent, SquadMembers, COND_OwnerOnly);
}

void UDVCommandComponent::RegisterSquadMember(ADVSoldierCharacter* Member)
{
	if (GetOwner()->HasAuthority() && Member)
	{
		SquadMembers.AddUnique(Member);
	}
}

TArray<ADVSoldierCharacter*> UDVCommandComponent::GetLivingSquad() const
{
	TArray<ADVSoldierCharacter*> Result;
	for (ADVSoldierCharacter* Member : SquadMembers)
	{
		if (IsValid(Member) && !Member->IsDead())
		{
			Result.Add(Member);
		}
	}
	return Result;
}

void UDVCommandComponent::IssueOrderFromView(EDVSquadOrder Order)
{
	const ADVSoldierCharacter* Leader = Cast<ADVSoldierCharacter>(GetOwner());
	const AController* Controller = Leader ? Leader->GetController() : nullptr;
	if (!Controller || Leader->IsDead())
	{
		return;
	}

	FVector ViewLoc;
	FRotator ViewRot;
	Controller->GetPlayerViewPoint(ViewLoc, ViewRot);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(DVOrderTrace), false, Leader);
	FHitResult Hit;
	const FVector End = ViewLoc + ViewRot.Vector() * MaxOrderDistance;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, ViewLoc, End, ECC_Visibility, Params);

	FVector Location = bHit ? Hit.ImpactPoint : End;
	AActor* Target = nullptr;
	if (bHit && Hit.GetActor() && Leader->GetTeamAttitudeTowards(*Hit.GetActor()) == ETeamAttitude::Hostile)
	{
		Target = Hit.GetActor();
	}

	ServerIssueOrder(Order, Location, Target);
}

bool UDVCommandComponent::ServerIssueOrder_Validate(EDVSquadOrder Order, FVector_NetQuantize Location, AActor* Target)
{
	// Malformed enum values only come from a modified client.
	return static_cast<uint8>(Order) < static_cast<uint8>(EDVSquadOrder::MAX);
}

void UDVCommandComponent::ServerIssueOrder_Implementation(EDVSquadOrder Order, FVector_NetQuantize Location, AActor* Target)
{
	const ADVSoldierCharacter* Leader = Cast<ADVSoldierCharacter>(GetOwner());
	if (!Leader || Leader->IsDead())
	{
		return;
	}
	if (!OrderLimiter.Consume(GetWorld()->GetTimeSeconds()))
	{
		UE_LOG(LogDVNet, Warning, TEXT("%s: squad order rate limit exceeded"), *GetNameSafe(Leader));
		return;
	}
	if (FVector::Dist(Leader->GetActorLocation(), Location) > MaxOrderDistance + 100.f)
	{
		UE_LOG(LogDVNet, Warning, TEXT("%s: squad order out of range"), *GetNameSafe(Leader));
		return;
	}
	if (Target && Leader->GetTeamAttitudeTowards(*Target) != ETeamAttitude::Hostile)
	{
		Target = nullptr;
	}

	FDVSquadOrderData Data;
	Data.Order = Order;
	Data.Location = Location;
	Data.Target = Target;
	DispatchOrder(Data);
	ClientOrderAcknowledged(Order, Location);
}

void UDVCommandComponent::DispatchOrder(const FDVSquadOrderData& Data)
{
	TArray<ADVSquadAIController*> Bots;
	for (ADVSoldierCharacter* Member : GetLivingSquad())
	{
		if (ADVSquadAIController* AI = Cast<ADVSquadAIController>(Member->GetController()))
		{
			Bots.Add(AI);
		}
	}

	for (int32 i = 0; i < Bots.Num(); ++i)
	{
		FDVSquadOrderData MemberOrder = Data;
		// Fireteam flank: the first bot lays down suppressing fire while the others move.
		if (Data.Order == EDVSquadOrder::Flank && i == 0 && Bots.Num() > 1)
		{
			MemberOrder.Order = EDVSquadOrder::Suppress;
		}
		Bots[i]->ReceiveOrder(MemberOrder);
	}

	UE_LOG(LogDVAI, Log, TEXT("Order %d issued to %d squadmates"), static_cast<int32>(Data.Order), Bots.Num());
}

void UDVCommandComponent::ClientOrderAcknowledged_Implementation(EDVSquadOrder Order, FVector_NetQuantize Location)
{
	OnOrderIssued.Broadcast(Order, Location);
}
