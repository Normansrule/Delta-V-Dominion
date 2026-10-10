#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "DVPlayerController.generated.h"

/** Player controller. Extension point for the command wheel, HUD and anti-cheat hooks. */
UCLASS()
class DELTAVDOMINION_API ADVPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ADVPlayerController();
};
