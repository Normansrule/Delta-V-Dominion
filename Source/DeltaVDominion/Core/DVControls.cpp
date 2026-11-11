#include "Core/DVControls.h"
#include "Misc/ConfigCacheIni.h"

namespace DVControlsData	// named: unity builds merge anonymous namespaces
{
	struct FEntry
	{
		const TCHAR* Id;
		const TCHAR* Name;
		const TCHAR* Group;
		FKey Default;
	};

	const FEntry& Entry(EDVInput C)
	{
		static const FEntry Table[] = {
			{ TEXT("MoveForward"), TEXT("Move forward"), TEXT("Movement"), EKeys::W },
			{ TEXT("MoveBack"), TEXT("Move back"), TEXT("Movement"), EKeys::S },
			{ TEXT("MoveLeft"), TEXT("Move left"), TEXT("Movement"), EKeys::A },
			{ TEXT("MoveRight"), TEXT("Move right"), TEXT("Movement"), EKeys::D },
			{ TEXT("Jump"), TEXT("Jump / thrusters"), TEXT("Movement"), EKeys::SpaceBar },
			{ TEXT("Sprint"), TEXT("Sprint"), TEXT("Movement"), EKeys::LeftShift },
			{ TEXT("Crouch"), TEXT("Crouch / slide"), TEXT("Movement"), EKeys::LeftControl },
			{ TEXT("Dash"), TEXT("Dash"), TEXT("Movement"), EKeys::LeftAlt },
			{ TEXT("Fire"), TEXT("Fire"), TEXT("Combat"), EKeys::LeftMouseButton },
			{ TEXT("Aim"), TEXT("Aim down sights"), TEXT("Combat"), EKeys::RightMouseButton },
			{ TEXT("Reload"), TEXT("Reload"), TEXT("Combat"), EKeys::R },
			{ TEXT("Interact"), TEXT("Use / board / pick up"), TEXT("Combat"), EKeys::E },
			{ TEXT("Melee"), TEXT("Melee / silent takedown"), TEXT("Combat"), EKeys::F },
			{ TEXT("Grenade"), TEXT("Throw grenade"), TEXT("Combat"), EKeys::G },
			{ TEXT("LeanLeft"), TEXT("Lean left"), TEXT("Combat"), EKeys::Q },
			{ TEXT("LeanRight"), TEXT("Lean right (while aiming)"), TEXT("Combat"), EKeys::E },
			{ TEXT("OrderMove"), TEXT("Squad: move / attack there"), TEXT("Squad"), EKeys::MiddleMouseButton },
			{ TEXT("OrderHold"), TEXT("Squad: hold position"), TEXT("Squad"), EKeys::Z },
			{ TEXT("OrderRegroup"), TEXT("Squad: regroup on me"), TEXT("Squad"), EKeys::V },
			{ TEXT("OrderSuppress"), TEXT("Squad: suppress target"), TEXT("Squad"), EKeys::T },
			{ TEXT("OrderFlank"), TEXT("Squad: flank"), TEXT("Squad"), EKeys::X },
			{ TEXT("Orbital"), TEXT("Orbital strike"), TEXT("Fire support"), EKeys::B },
			{ TEXT("Supply"), TEXT("Supply drop"), TEXT("Fire support"), EKeys::N },
			{ TEXT("Strafe"), TEXT("Strafing run"), TEXT("Fire support"), EKeys::H },
			{ TEXT("FlyHome"), TEXT("Fly home to the Kestrel"), TEXT("General"), EKeys::K },
			{ TEXT("Map"), TEXT("Galaxy map"), TEXT("General"), EKeys::Tab },
			{ TEXT("Buy"), TEXT("Buy (quartermaster)"), TEXT("General"), EKeys::Enter },
			{ TEXT("VehicleUp"), TEXT("Vehicle: climb / handbrake"), TEXT("Vehicles"), EKeys::SpaceBar },
			{ TEXT("VehicleDown"), TEXT("Vehicle: descend"), TEXT("Vehicles"), EKeys::LeftControl },
			{ TEXT("VehicleMode"), TEXT("Raven: hover / jet"), TEXT("Vehicles"), EKeys::G },
			{ TEXT("VehicleCamera"), TEXT("Vehicle camera"), TEXT("Vehicles"), EKeys::C },
			{ TEXT("Flares"), TEXT("Countermeasures"), TEXT("Vehicles"), EKeys::X },
			{ TEXT("Boost"), TEXT("Boost"), TEXT("Vehicles"), EKeys::LeftShift },
		};
		static_assert(UE_ARRAY_COUNT(Table) == int32(EDVInput::Num), "one entry per control");
		return Table[int32(C)];
	}

	const TCHAR* Section = TEXT("DVControls");

	TArray<FKey>& Bound()
	{
		static TArray<FKey> Keys;
		if (Keys.Num() == 0)
		{
			for (int32 i = 0; i < int32(EDVInput::Num); ++i)
			{
				FString Saved;
				const FEntry& E = Entry(EDVInput(i));
				FKey K = E.Default;
				if (GConfig && GConfig->GetString(Section, E.Id, Saved, GGameUserSettingsIni) && FKey(*Saved).IsValid())
				{
					K = FKey(*Saved);
				}
				Keys.Add(K);
			}
		}
		return Keys;
	}

	int32 GRevision = 0;
}

FKey DVControls::Key(EDVInput Control)
{
	return DVControlsData::Bound()[int32(Control)];
}

FKey DVControls::DefaultKey(EDVInput Control)
{
	return DVControlsData::Entry(Control).Default;
}

void DVControls::SetKey(EDVInput Control, const FKey& NewKey)
{
	DVControlsData::Bound()[int32(Control)] = NewKey;
	if (GConfig)
	{
		GConfig->SetString(DVControlsData::Section, DVControlsData::Entry(Control).Id, *NewKey.ToString(), GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
	++DVControlsData::GRevision;
}

void DVControls::ResetAll()
{
	for (int32 i = 0; i < int32(EDVInput::Num); ++i)
	{
		SetKey(EDVInput(i), DVControlsData::Entry(EDVInput(i)).Default);
	}
}

FString DVControls::KeyLabel(const FKey& K)
{
	if (K == EKeys::LeftMouseButton) { return TEXT("LMB"); }
	if (K == EKeys::RightMouseButton) { return TEXT("RMB"); }
	if (K == EKeys::MiddleMouseButton) { return TEXT("MMB"); }
	if (K == EKeys::SpaceBar) { return TEXT("Space"); }
	if (K == EKeys::LeftShift || K == EKeys::RightShift) { return TEXT("Shift"); }
	if (K == EKeys::LeftControl || K == EKeys::RightControl) { return TEXT("Ctrl"); }
	if (K == EKeys::LeftAlt || K == EKeys::RightAlt) { return TEXT("Alt"); }
	if (K == EKeys::Enter) { return TEXT("Enter"); }
	if (K == EKeys::Tab) { return TEXT("Tab"); }
	if (K == EKeys::Escape) { return TEXT("Esc"); }
	const FString S = K.GetDisplayName(false).ToString();
	return S.Len() <= 5 ? S : S.Left(5);
}

FString DVControls::Label(EDVInput Control)
{
	return KeyLabel(Key(Control));
}

FString DVControls::Cap(EDVInput Control)
{
	return FString::Printf(TEXT("[%s]"), *Label(Control));
}

FString DVControls::Name(EDVInput Control)
{
	return DVControlsData::Entry(Control).Name;
}

FString DVControls::Group(EDVInput Control)
{
	return DVControlsData::Entry(Control).Group;
}

int32 DVControls::Revision()
{
	return DVControlsData::GRevision;
}
