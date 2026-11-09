#include "Core/DVGameInstance.h"
#include "AbilitySystemGlobals.h"
#include "Core/DVMissions.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "Misc/CommandLine.h"

void UDVGameInstance::Init()
{
	Super::Init();

	// Required for GAS target data (used by the fire ability's client -> server hits).
	UAbilitySystemGlobals::Get().InitGlobalData();
	DVMissions::bUnlockAll = FParse::Param(FCommandLine::Get(), TEXT("DVUnlockAll"));

	// The game is built to be seen at its best: every scalability group at Cinematic (Lumen and shadow quality, view
	// distance, textures, effects, foliage, post). -DVLowQuality drops to Epic for slower machines.
	if (UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		Settings->SetOverallScalabilityLevel(FParse::Param(FCommandLine::Get(), TEXT("DVLowQuality")) ? 3 : 4);
		Settings->ApplySettings(false);
	}
}
