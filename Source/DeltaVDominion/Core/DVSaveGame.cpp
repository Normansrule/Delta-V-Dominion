#include "Core/DVSaveGame.h"
#include "DeltaVDominion.h"
#include "Misc/SecureHash.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"

namespace
{
	const TCHAR* const DVSaveSalt = TEXT("DeltaV::Dominion::Roster::v1");
}

FString UDVSaveGame::ComputeChecksum(int32 InVersion, const TArray<uint8>& InPayload)
{
	FSHA1 Sha;
	Sha.Update(reinterpret_cast<const uint8*>(&InVersion), sizeof(InVersion));
	const FTCHARToUTF8 Salt(DVSaveSalt);
	Sha.Update(reinterpret_cast<const uint8*>(Salt.Get()), Salt.Length());
	Sha.Update(InPayload.GetData(), InPayload.Num());
	Sha.Final();

	uint8 Digest[FSHA1::DigestSize];
	Sha.GetHash(Digest);
	return BytesToHex(Digest, FSHA1::DigestSize);
}

void UDVSaveGame::WriteRoster(const FDVRosterState& State)
{
	Payload.Reset();
	FMemoryWriter Writer(Payload, /*bIsPersistent*/ true);
	FDVRosterState::StaticStruct()->SerializeBin(Writer, const_cast<FDVRosterState*>(&State));

	Version = CurrentVersion;
	Checksum = ComputeChecksum(Version, Payload);
	SavedAtUtc = FDateTime::UtcNow().ToIso8601();
}

bool UDVSaveGame::ReadRoster(FDVRosterState& OutState) const
{
	if (Version != CurrentVersion)
	{
		UE_LOG(LogDVRoster, Warning, TEXT("Save version %d not supported (expected %d)"), Version, CurrentVersion);
		return false;
	}
	if (Payload.Num() == 0 || ComputeChecksum(Version, Payload) != Checksum)
	{
		UE_LOG(LogDVRoster, Error, TEXT("Save checksum mismatch: file rejected"));
		return false;
	}

	FMemoryReader Reader(Payload, /*bIsPersistent*/ true);
	FDVRosterState::StaticStruct()->SerializeBin(Reader, &OutState);
	return !Reader.IsError();
}
