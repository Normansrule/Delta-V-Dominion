#include "Core/DVGameInstance.h"
#include "AbilitySystemGlobals.h"

void UDVGameInstance::Init()
{
	Super::Init();

	// Required for GAS target data (used by the fire ability's client -> server hits).
	UAbilitySystemGlobals::Get().InitGlobalData();
}
