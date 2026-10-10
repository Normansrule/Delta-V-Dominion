#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "DVGameInstance.generated.h"

/**
 * Lives for the whole session. Owns the roster subsystem (UDVRosterSubsystem) and will
 * own EOS session / Easy Anti-Cheat setup in the co-op milestone.
 */
UCLASS()
class DELTAVDOMINION_API UDVGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
};
