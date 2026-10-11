#include "Combat/DVLoadouts.h"
#include "Combat/DVWeaponData.h"
#include "Engine/SkeletalMesh.h"
#include "UObject/Package.h"

namespace
{
	TArray<UDVWeaponData*>& Profiles()
	{
		static TArray<UDVWeaponData*> List;
		if (List.Num() > 0)
		{
			return List;
		}
		USkeletalMesh* Pistol = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Weapons/Pistol/Meshes/SKM_Pistol.SKM_Pistol"));
		USkeletalMesh* Launcher = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Weapons/GrenadeLauncher/Meshes/SKM_GrenadeLauncher.SKM_GrenadeLauncher"));
		auto Make = [](const TCHAR* Name, const TCHAR* Label)
		{
			UDVWeaponData* D = NewObject<UDVWeaponData>(GetTransientPackage(), FName(Name));
			D->AddToRoot();	// lives for the whole session, shared by every soldier that picks it
			D->DisplayName = FText::FromString(Label);
			List.Add(D);
			return D;
		};

		Make(TEXT("DV_M9Bastion"), TEXT("M9 Bastion"));	// class defaults: 600 rpm, 30 rounds

		UDVWeaponData* Kite = Make(TEXT("DV_KiteSMG"), TEXT("Kite SMG"));
		Kite->RoundsPerMinute = 920.f;
		Kite->Damage = 17.f;
		Kite->MaxRange = 6000.f;
		Kite->HipSpread = 1.8f;
		Kite->AdsSpread = 0.7f;
		Kite->SpreadPerShot = 0.15f;
		Kite->MagazineSize = 40;
		Kite->StartingReserve = 280;
		Kite->ReloadTime = 1.7f;
		Kite->RecoilPattern = { {0.25f, 0.1f}, {0.3f, -0.12f}, {0.28f, 0.15f}, {0.3f, -0.18f} };
		Kite->AdsFOV = 72.f;
		if (Pistol) { Kite->Mesh = Pistol; }

		UDVWeaponData* Halberd = Make(TEXT("DV_Halberd"), TEXT("Halberd Breacher"));
		Halberd->FireMode = EDVFireMode::Semi;
		Halberd->RoundsPerMinute = 85.f;
		Halberd->PelletsPerShot = 8;
		Halberd->Damage = 15.f;
		Halberd->MaxRange = 3000.f;
		Halberd->HipSpread = 4.5f;
		Halberd->AdsSpread = 3.f;
		Halberd->MagazineSize = 6;
		Halberd->StartingReserve = 42;
		Halberd->ReloadTime = 2.8f;
		Halberd->RecoilPattern = { {2.2f, 0.3f}, {2.f, -0.4f} };
		Halberd->SuppressionPerShot = 30.f;
		if (Launcher) { Halberd->Mesh = Launcher; }

		UDVWeaponData* Longreach = Make(TEXT("DV_Longreach"), TEXT("Longreach DMR"));
		Longreach->FireMode = EDVFireMode::Semi;
		Longreach->RoundsPerMinute = 240.f;
		Longreach->Damage = 72.f;
		Longreach->MaxRange = 30000.f;
		Longreach->HipSpread = 3.f;
		Longreach->AdsSpread = 0.05f;
		Longreach->SpreadPerShot = 0.6f;
		Longreach->MagazineSize = 10;
		Longreach->StartingReserve = 70;
		Longreach->ReloadTime = 2.6f;
		Longreach->RecoilPattern = { {1.4f, 0.1f}, {1.3f, -0.15f} };
		Longreach->AdsFOV = 38.f;
		Longreach->SuppressionPerShot = 18.f;

		UDVWeaponData* Pulse = Make(TEXT("DV_PulseCarbine"), TEXT("Pulse Carbine"));
		Pulse->RoundsPerMinute = 720.f;
		Pulse->Damage = 24.f;
		Pulse->bUsesHeat = true;
		Pulse->HeatPerShot = 0.045f;
		Pulse->CooldownPerSecond = 0.45f;
		Pulse->HipSpread = 1.6f;
		Pulse->AdsSpread = 0.25f;
		Pulse->RecoilPattern = { {0.2f, 0.04f}, {0.22f, -0.05f} };
		return List;
	}
}

int32 DVLoadouts::Num()
{
	return Profiles().Num();
}

UDVWeaponData* DVLoadouts::Get(int32 Index)
{
	TArray<UDVWeaponData*>& List = Profiles();
	return List.IsValidIndex(Index) ? List[Index] : nullptr;
}

UDVWeaponData* DVLoadouts::BulwarkCannon()
{
	static UDVWeaponData* Cannon = nullptr;
	if (!Cannon)
	{
		Cannon = NewObject<UDVWeaponData>(GetTransientPackage(), TEXT("DV_Hammerfall"));
		Cannon->AddToRoot();
		Cannon->DisplayName = FText::FromString(TEXT("Hammerfall Rotary"));
		Cannon->RoundsPerMinute = 720.f;
		Cannon->Damage = 8.f;
		Cannon->MaxRange = 9000.f;
		Cannon->HipSpread = 3.8f;
		Cannon->AdsSpread = 3.f;
		Cannon->SpreadPerShot = 0.05f;
		Cannon->MagazineSize = 150;
		Cannon->StartingReserve = 1500;
		Cannon->ReloadTime = 4.f;
		Cannon->SuppressionRadius = 450.f;
		Cannon->SuppressionPerShot = 14.f;
		Cannon->RecoilPattern = { {0.1f, 0.05f}, {0.1f, -0.05f} };
		if (USkeletalMesh* Launcher = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Weapons/GrenadeLauncher/Meshes/SKM_GrenadeLauncher.SKM_GrenadeLauncher")))
		{
			Cannon->Mesh = Launcher;
		}
	}
	return Cannon;
}

FString DVLoadouts::Describe(const UDVWeaponData* Data)
{
	if (!Data)
	{
		return FString();
	}
	const FString Feed = Data->bUsesHeat ? FString(TEXT("no reloads, overheats")) : FString::Printf(TEXT("%d-round mag"), Data->MagazineSize);
	const FString Dmg = Data->PelletsPerShot > 1 ? FString::Printf(TEXT("%d x %.0f dmg"), Data->PelletsPerShot, Data->Damage) : FString::Printf(TEXT("%.0f dmg"), Data->Damage);
	const TCHAR* Mode = Data->FireMode == EDVFireMode::Auto ? TEXT("auto") : Data->FireMode == EDVFireMode::Burst ? TEXT("burst") : TEXT("semi");
	return FString::Printf(TEXT("%s  |  %.0f rpm %s  |  %s  |  %.0f m"), *Dmg, Data->RoundsPerMinute, Mode, *Feed, Data->MaxRange / 100.f);
}
