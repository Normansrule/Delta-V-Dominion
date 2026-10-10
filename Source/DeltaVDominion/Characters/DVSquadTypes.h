#pragma once

#include "CoreMinimal.h"
#include "DVSquadTypes.generated.h"

class AActor;

UENUM(BlueprintType)
enum class EDVSquadOrder : uint8
{
	Follow,		// Formation on the leader, engage targets of opportunity
	MoveTo,		// Move to a point, take cover there
	Hold,		// Hold position, use nearby cover
	Flank,		// One member suppresses, the rest flank the target
	Suppress,	// Pin the target or area with sustained fire
	Regroup,	// Drop everything and rally on the leader
	MAX UMETA(Hidden)
};

USTRUCT(BlueprintType)
struct FDVSquadOrderData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite) EDVSquadOrder Order = EDVSquadOrder::Follow;
	UPROPERTY(BlueprintReadWrite) FVector Location = FVector::ZeroVector;
	UPROPERTY(BlueprintReadWrite) TWeakObjectPtr<AActor> Target;
};
