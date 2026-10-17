#include "Core/DVProfileSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

namespace
{
	const TCHAR* CommanderSlot = TEXT("Commander");

	struct FPaint { const TCHAR* Name; FLinearColor Color; };
	const FPaint CommanderPaints[] = {
		{ TEXT("Accord Olive"), FLinearColor(0.16f, 0.2f, 0.13f) },
		{ TEXT("Arctic"), FLinearColor(0.62f, 0.64f, 0.66f) },
		{ TEXT("Tharsis Tan"), FLinearColor(0.42f, 0.3f, 0.18f) },
		{ TEXT("Fleet Navy"), FLinearColor(0.04f, 0.07f, 0.16f) },
		{ TEXT("Crimson"), FLinearColor(0.32f, 0.03f, 0.03f) },
		{ TEXT("Urban Grey"), FLinearColor(0.2f, 0.21f, 0.22f) },
		{ TEXT("Black Ops"), FLinearColor(0.02f, 0.02f, 0.025f) },
		{ TEXT("Hazard Yellow"), FLinearColor(0.6f, 0.42f, 0.02f) },
	};
	const FPaint CommanderOptics[] = {
		{ TEXT("Cyan"), FLinearColor(0.2f, 0.7f, 1.f) },
		{ TEXT("Amber"), FLinearColor(1.f, 0.55f, 0.08f) },
		{ TEXT("Green"), FLinearColor(0.2f, 1.f, 0.35f) },
		{ TEXT("White"), FLinearColor(1.f, 1.f, 1.f) },
		{ TEXT("Violet"), FLinearColor(0.6f, 0.25f, 1.f) },
		{ TEXT("Gold"), FLinearColor(1.f, 0.8f, 0.3f) },
	};
	// The board at Rook's Rest. Free Ports jobs pay in standing with the smugglers; the others are posted by
	// an Accord liaison and a Colonies relief officer who drink in the same cantina.
	const FDVContract CommanderContracts[DVContracts::Num] = {
		{ TEXT("Quiet Money"), TEXT("Mags Okafor, Free Ports fixer"), DVFactions::FreePorts,
		  TEXT("A client wants Hearthbound sentries gone without an alarm. Get behind them."), TEXT("silent takedowns"), 3, 10, 250 },
		{ TEXT("Scrap Rights"), TEXT("Mags Okafor, Free Ports fixer"), DVFactions::FreePorts,
		  TEXT("Sealed cargo is lying in the war zones. Bring the Ports their share."), TEXT("salvage crates claimed"), 2, 12, 200 },
		{ TEXT("Marksman's Purse"), TEXT("Lt. Adaeze Kim, Accord liaison"), DVFactions::Accord,
		  TEXT("Command is testing the new optics. Prove them on frame cores."), TEXT("headshots"), 6, 10, 250 },
		{ TEXT("Nobody Left Behind"), TEXT("Sister Ilse, Colonies relief"), DVFactions::Colonies,
		  TEXT("The colonies watch how Delta treats its own. Pick your people up."), TEXT("squadmates revived"), 2, 12, 250 },
		{ TEXT("Heavy Bounty"), TEXT("Lt. Adaeze Kim, Accord liaison"), DVFactions::Accord,
		  TEXT("Bulwark war-frames are holding our lines back. Command pays per wreck."), TEXT("Bulwarks destroyed"), 1, 12, 300 },
	};

	const TCHAR* CommanderCallsigns[] = { TEXT("Halcyon"), TEXT("Ghost"), TEXT("Nomad"), TEXT("Atlas"), TEXT("Saint"), TEXT("Kodiak"),
		TEXT("Echo"), TEXT("Wraith"), TEXT("Lantern"), TEXT("Comet"), TEXT("Juniper"), TEXT("Anvil") };
}

void UDVProfileSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (const UDVCommanderSave* Saved = Cast<UDVCommanderSave>(UGameplayStatics::LoadGameFromSlot(CommanderSlot, 0)))
	{
		Look = Saved->Look;
		XP = Saved->XP;
		Reputation = Saved->Reputation;
		ActiveContract = Saved->ActiveContract < NumContracts() ? Saved->ActiveContract : -1;
		ContractProgress = Saved->ContractProgress;
	}
	Reputation.SetNumZeroed(DVFactions::Num);
	Look.bSet = true;	// a fresh profile is the default commander
}

void UDVProfileSubsystem::Cycle(int32 Field)
{
	switch (Field)
	{
	case 0: Look.Body = (Look.Body + 1) % 2; break;
	case 1: Look.Paint = (Look.Paint + 1) % NumPaints(); break;
	case 2: Look.Optics = (Look.Optics + 1) % NumOptics(); break;
	case 3: Look.Callsign = (Look.Callsign + 1) % NumCallsigns(); break;
	default: return;
	}
	Save();
}

void UDVProfileSubsystem::Save()
{
	UDVCommanderSave* Data = Cast<UDVCommanderSave>(UGameplayStatics::CreateSaveGameObject(UDVCommanderSave::StaticClass()));
	Data->Look = Look;
	Data->XP = XP;
	Data->Reputation = Reputation;
	Data->ActiveContract = ActiveContract;
	Data->ContractProgress = ContractProgress;
	UGameplayStatics::SaveGameToSlot(Data, CommanderSlot, 0);
}

int32 UDVProfileSubsystem::NumPaints() { return UE_ARRAY_COUNT(CommanderPaints); }
int32 UDVProfileSubsystem::NumOptics() { return UE_ARRAY_COUNT(CommanderOptics); }
int32 UDVProfileSubsystem::NumCallsigns() { return UE_ARRAY_COUNT(CommanderCallsigns); }
FLinearColor UDVProfileSubsystem::PaintColor(int32 I) { return CommanderPaints[FMath::Clamp(I, 0, NumPaints() - 1)].Color; }
FString UDVProfileSubsystem::PaintName(int32 I) { return CommanderPaints[FMath::Clamp(I, 0, NumPaints() - 1)].Name; }
FLinearColor UDVProfileSubsystem::OpticsColor(int32 I) { return CommanderOptics[FMath::Clamp(I, 0, NumOptics() - 1)].Color; }
FString UDVProfileSubsystem::OpticsName(int32 I) { return CommanderOptics[FMath::Clamp(I, 0, NumOptics() - 1)].Name; }
FString UDVProfileSubsystem::CallsignName(int32 I) { return CommanderCallsigns[FMath::Clamp(I, 0, NumCallsigns() - 1)]; }

int32 UDVProfileSubsystem::XPForLevel(int32 Level) const
{
	// 500, 1100, 1800, 2600 ... each level a little longer than the last.
	int32 Total = 0;
	for (int32 L = 2; L <= Level; ++L)
	{
		Total += 400 + 100 * (L - 1);
	}
	return Total;
}

int32 UDVProfileSubsystem::GetLevel() const
{
	int32 Level = 1;
	while (Level < DVPerks::MaxLevel && XP >= XPForLevel(Level + 1))
	{
		++Level;
	}
	return Level;
}

void UDVProfileSubsystem::AddXP(int32 Amount, const FString& Reason)
{
	const int32 Before = GetLevel();
	XP += Amount;
	const double Now = GetGameInstance() && GetGameInstance()->GetWorld() ? GetGameInstance()->GetWorld()->GetRealTimeSeconds() : 0.0;
	LastAward = FString::Printf(TEXT("+%d XP  %s"), Amount, *Reason);
	LastAwardTime = Now;
	if (GetLevel() > Before)
	{
		LastLevelUp = GetLevel();
		LastLevelUpTime = Now;
	}
	Save();
}

FString UDVProfileSubsystem::PerkName(int32 Level)
{
	switch (Level)
	{
	case DVPerks::ExtraThruster: return TEXT("Extra thruster charge");
	case DVPerks::FieldMedic: return TEXT("Field medic: revive twice as fast");
	case DVPerks::Hardened: return TEXT("Hardened: +25 max health");
	case DVPerks::PlateCarrier: return TEXT("Plate carrier: start with 40 armour");
	case DVPerks::KestrelFavour: return TEXT("Kestrel's favour: call-ins recharge faster");
	default: return Level > DVPerks::KestrelFavour ? TEXT("Service ribbon") : TEXT("");
	}
}

void UDVProfileSubsystem::AddRep(int32 Faction, int32 Delta, const FString& Reason)
{
	if (!Reputation.IsValidIndex(Faction) || Delta == 0)
	{
		return;
	}
	const FString Before = TierName(Reputation[Faction]);
	Reputation[Faction] = FMath::Clamp(Reputation[Faction] + Delta, -100, 100);
	const FString After = TierName(Reputation[Faction]);
	const FString Line = FString::Printf(TEXT("%s%d %s  -  %s%s"), Delta > 0 ? TEXT("+") : TEXT(""), Delta, *FactionName(Faction).ToUpper(), *Reason,
		After != Before ? *FString::Printf(TEXT("  (now %s)"), *After) : TEXT(""));
	RecentRep.Add(Line);
	if (RecentRep.Num() > 3)
	{
		RecentRep.RemoveAt(0);
	}
	LastRepTime = GetGameInstance() && GetGameInstance()->GetWorld() ? GetGameInstance()->GetWorld()->GetRealTimeSeconds() : 0.0;
	Save();
}

FString UDVProfileSubsystem::FactionName(int32 Faction)
{
	switch (Faction)
	{
	case DVFactions::Accord: return TEXT("Accord Command");
	case DVFactions::Colonies: return TEXT("The Colonies");
	case DVFactions::FreePorts: return TEXT("The Free Ports");
	default: return TEXT("");
	}
}

FString UDVProfileSubsystem::TierName(int32 Value)
{
	return Value <= -50 ? TEXT("Hostile") : Value < -15 ? TEXT("Wary") : Value < DVFactions::Trusted ? TEXT("Neutral")
		: Value < DVFactions::Allied ? TEXT("Trusted") : TEXT("Allied");
}

double UDVProfileSubsystem::Now() const
{
	return GetGameInstance() && GetGameInstance()->GetWorld() ? GetGameInstance()->GetWorld()->GetRealTimeSeconds() : 0.0;
}

int32 UDVProfileSubsystem::NumContracts() { return DVContracts::Num; }
const FDVContract& UDVProfileSubsystem::Contract(int32 Kind) { return CommanderContracts[FMath::Clamp(Kind, 0, DVContracts::Num - 1)]; }

void UDVProfileSubsystem::TakeContract(int32 Kind)
{
	if (Kind == ActiveContract || Kind < 0 || Kind >= NumContracts())
	{
		return;
	}
	ActiveContract = Kind;
	ContractProgress = 0;
	ContractNote = FString::Printf(TEXT("CONTRACT TAKEN  -  %s"), Contract(Kind).Name);
	ContractNoteTime = Now();
	Save();
}

void UDVProfileSubsystem::ContractEvent(int32 Kind)
{
	if (Kind != ActiveContract || IsContractDone())
	{
		return;
	}
	++ContractProgress;
	const FDVContract& C = Contract(Kind);
	ContractNote = IsContractDone() ? FString::Printf(TEXT("CONTRACT COMPLETE  -  %s  (paid on a successful operation)"), C.Name)
		: FString::Printf(TEXT("%s  %d / %d %s"), C.Name, ContractProgress, C.Goal, C.Unit);
	ContractNoteTime = Now();
	Save();
}

int32 UDVProfileSubsystem::PayContract()
{
	if (!IsContractDone())
	{
		return 0;
	}
	const FDVContract& C = Contract(ActiveContract);
	ActiveContract = -1;
	ContractProgress = 0;
	ContractNote = FString::Printf(TEXT("CONTRACT PAID  -  %s"), C.Name);
	ContractNoteTime = Now();
	AddRep(C.Faction, C.Rep, FString::Printf(TEXT("Contract paid: %s"), C.Name));	// also saves
	return C.XP;
}
