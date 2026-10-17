#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DVProfileSubsystem.generated.h"

/** How the commander looks: picked at the Kestrel's barracks locker, applied to the player's soldier. */
USTRUCT(BlueprintType)
struct FDVLook
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) uint8 Body = 0;	// 0 = frame A (Manny), 1 = frame B (Quinn)
	UPROPERTY(BlueprintReadOnly) uint8 Paint = 0;
	UPROPERTY(BlueprintReadOnly) uint8 Optics = 0;
	UPROPERTY(BlueprintReadOnly) uint8 Callsign = 0;
	UPROPERTY(BlueprintReadOnly) bool bSet = false;
};

UCLASS()
class DELTAVDOMINION_API UDVCommanderSave : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY() FDVLook Look;
	UPROPERTY() int32 XP = 0;
	UPROPERTY() TArray<int32> Reputation;
	UPROPERTY() int32 ActiveContract = -1;
	UPROPERTY() int32 ContractProgress = 0;
};

/** The three powers that remember what Delta does (-100..100 each). */
namespace DVFactions
{
	constexpr int32 Accord = 0;		// Accord Command: the chain of command and the fleet
	constexpr int32 Colonies = 1;	// the civilian colonies you evacuate, defend or fail
	constexpr int32 FreePorts = 2;	// smugglers and salvagers who trade in what the war leaves behind
	constexpr int32 Num = 3;
	constexpr int32 Trusted = 20;
	constexpr int32 Allied = 50;
}

/** Side jobs posted on the board at Rook's Rest. Taken in town, worked on the next operations, paid on a win. */
namespace DVContracts
{
	enum EKind : uint8 { SilentTakedown, Salvage, Headshot, Revive, HeavyFrame, Num };
}

struct FDVContract
{
	const TCHAR* Name;
	const TCHAR* Giver;		// who posted it
	int32 Faction;			// who pays (DVFactions)
	const TCHAR* Brief;
	const TCHAR* Unit;		// "silent takedowns"
	int32 Goal;
	int32 Rep;
	int32 XP;
};

/** Commander perks, unlocked by level (see UDVProfileSubsystem::PerkName). */
namespace DVPerks
{
	constexpr int32 ExtraThruster = 2;	// a third thruster charge
	constexpr int32 FieldMedic = 3;		// revive squadmates almost twice as fast
	constexpr int32 Hardened = 4;		// +25 max health
	constexpr int32 PlateCarrier = 5;	// start missions with 40 armour
	constexpr int32 KestrelFavour = 6;	// call-ins recharge 25% faster
	constexpr int32 MaxLevel = 10;
}

/**
 * The player's commander profile (separate from the squad roster): appearance and callsign.
 * Saved to its own slot so a new campaign keeps your look.
 */
UCLASS()
class DELTAVDOMINION_API UDVProfileSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	const FDVLook& GetLook() const { return Look; }

	/** Commander progression: XP from kills, objectives, revives and missions; perks unlock by level. */
	void AddXP(int32 Amount, const FString& Reason);
	int32 GetXP() const { return XP; }
	int32 GetLevel() const;
	int32 XPForLevel(int32 Level) const;	// total XP needed to reach Level
	static FString PerkName(int32 Level);

	/** Reputation with a faction (see DVFactions). Saved with the commander. */
	int32 GetRep(int32 Faction) const { return Reputation.IsValidIndex(Faction) ? Reputation[Faction] : 0; }
	void AddRep(int32 Faction, int32 Delta, const FString& Reason);
	static FString FactionName(int32 Faction);
	static FString TierName(int32 Value);
	/** Recent standing changes (newest last), for the HUD. */
	TArray<FString> RecentRep;
	double LastRepTime = -100.0;
	/** Last XP award, for the HUD toast. */
	FString LastAward;
	double LastAwardTime = -100.0;
	int32 LastLevelUp = 0;
	double LastLevelUpTime = -100.0;
	/** Contracts (see DVContracts): one active at a time, saved with the commander. */
	static int32 NumContracts();
	static const FDVContract& Contract(int32 Kind);
	int32 GetActiveContract() const { return ActiveContract; }
	int32 GetContractProgress() const { return ContractProgress; }
	bool IsContractDone() const { return ActiveContract >= 0 && ContractProgress >= Contract(ActiveContract).Goal; }
	void TakeContract(int32 Kind);
	/** Something happened that a contract might count (Kind is a DVContracts::EKind). */
	void ContractEvent(int32 Kind);
	/** Pays a finished contract (reputation and XP); returns the XP to award, 0 if nothing was due. */
	int32 PayContract();
	FString ContractNote;
	double ContractNoteTime = -100.0;
	/** Field 0 body, 1 paint, 2 optics, 3 callsign: step to the next option and save. */
	void Cycle(int32 Field);

	static int32 NumPaints();
	static int32 NumOptics();
	static int32 NumCallsigns();
	static FLinearColor PaintColor(int32 Index);
	static FString PaintName(int32 Index);
	static FLinearColor OpticsColor(int32 Index);
	static FString OpticsName(int32 Index);
	static FString CallsignName(int32 Index);

private:
	void Save();
	FDVLook Look;
	int32 XP = 0;
	TArray<int32> Reputation;
	int32 ActiveContract = -1;
	int32 ContractProgress = 0;
	double Now() const;
};
