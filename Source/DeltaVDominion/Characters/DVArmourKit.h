#pragma once

#include "CoreMinimal.h"

class USkeletalMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;

/** Who is wearing the kit: each faction's armour has its own silhouette, not just its own paint. */
enum class EDVArmourStyle : uint8
{
	Marine,		// Accord marines: rounded helmet with a cyan visor, plate carrier, pouches, pack with an antenna, pads
	WarFrame,	// CINDER war-frames: an angular helm with a red optic slit, heavy chest with a glowing core, back vents
	Clockwork,	// Brasshaven constables: a brass helmet like a bell, twin amber lenses, a boiler on the back
	Helper,		// Nana's helper-bots: a round dome head with two rosy eyes, a soft apron plate
};

struct FDVArmourMeshes
{
	UStaticMesh* Box = nullptr;
	UStaticMesh* Sphere = nullptr;
	UStaticMesh* Cylinder = nullptr;
	UMaterialInterface* Base = nullptr;	// a material with Color and Emissive parameters (M_GreyBox)
};

/**
 * Kit-bashed armour bolted to a mannequin's skeleton. Pieces are placed against the body's bones in the pose the
 * skeleton is in when the kit is built, then attached to those bones, so they follow every animation afterwards.
 * Limb plates are aligned along the limb (from one joint to the next), so they sit right whatever the pose was.
 */
namespace DVArmour
{
	/** Builds the kit on Body (owned by Owner), replacing any earlier kit in Parts. Paint tints the main plates.
	 *  bHideHead hides the helmet from the owning player's own first-person camera. */
	DELTAVDOMINION_API void Build(AActor* Owner, USkeletalMeshComponent* Body, EDVArmourStyle Style, const FLinearColor& Paint,
		const FDVArmourMeshes& Meshes, TArray<UStaticMeshComponent*>& Parts, bool bHideHead);

	/** Darkens the optics of a kit whose wearer is down or calmed. */
	DELTAVDOMINION_API void SetOptics(TArray<UStaticMeshComponent*>& Parts, bool bOn);
}
