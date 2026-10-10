#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Core/DVRosterTypes.h"
#include "DVSaveGame.generated.h"

/**
 * Campaign save. The roster is stored as a serialized byte payload plus a SHA-1
 * checksum over (version + salt + payload). A mismatch on load means the file was
 * edited or corrupted and is rejected.
 *
 * This is tamper-evidence, not DRM: the salt ships in the binary. In co-op the host's
 * save is the only authority and clients never send roster data to the server.
 */
UCLASS()
class DELTAVDOMINION_API UDVSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 CurrentVersion = 1;

	void WriteRoster(const FDVRosterState& State);
	bool ReadRoster(FDVRosterState& OutState) const;

	UPROPERTY() int32 Version = CurrentVersion;
	UPROPERTY() TArray<uint8> Payload;
	UPROPERTY() FString Checksum;
	UPROPERTY() FString SavedAtUtc;

private:
	static FString ComputeChecksum(int32 InVersion, const TArray<uint8>& InPayload);
};
