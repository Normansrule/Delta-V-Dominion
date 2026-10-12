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
	}
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
