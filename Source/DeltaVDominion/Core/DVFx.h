#pragma once

#include "CoreMinimal.h"

class UWorld;
class AActor;
class ADVSoldierCharacter;

/**
 * Asset-free combat cosmetics (emissive meshes and lights spawned on short-lived host actors) and
 * shared server-side damage helpers. Used by weapons, vehicles, the Kestrel and orbital strikes.
 * Real Niagara VFX can replace these per call site in the art pass.
 */
namespace DVFx
{
	/** Fireball, smoke column, flash and (for nearby local players) a view kick. Scale 1 = orbital strike. */
	DELTAVDOMINION_API void Explosion(UWorld* World, const FVector& Location, float Scale = 1.f);

	/** A glowing streak from From to To that lasts Duration seconds. */
	DELTAVDOMINION_API void Beam(UWorld* World, const FVector& From, const FVector& To, const FLinearColor& Emissive, float Width = 10.f, float Duration = 0.06f);

	/** Brief muzzle flash light. */
	DELTAVDOMINION_API void Flash(UWorld* World, const FVector& Location, const FLinearColor& Color, float Candelas = 2000.f, float Radius = 900.f, float Duration = 0.05f);
}

namespace DVCombat
{
	/** Server: one hit of Damage from Shooter's ability system (kill credit goes to Shooter). Ignores friendlies unless bFriendlyFire. */
	DELTAVDOMINION_API void ApplyDamage(ADVSoldierCharacter* Shooter, AActor* Target, float Damage, bool bFriendlyFire = false);

	/** Server: damage with linear falloff (to 30% at the edge) to every soldier inside Radius. */
	DELTAVDOMINION_API void RadialDamage(ADVSoldierCharacter* Shooter, const FVector& Center, float Radius, float Damage, bool bFriendlyFire);
}
