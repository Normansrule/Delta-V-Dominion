#include "Core/DVGameState.h"
#include "Net/UnrealNetwork.h"

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
