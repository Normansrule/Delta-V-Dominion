#include "Characters/DVArmourKit.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace DVArmourParts	// named: unity builds merge anonymous namespaces
{
	const FName OpticTag(TEXT("DVOptic"));

	struct FCtx
	{
		AActor* Owner;
		USkeletalMeshComponent* Body;
		const FDVArmourMeshes& M;
		TArray<UStaticMeshComponent*>& Parts;
		bool bHideHead;
		FVector Fwd, Right, Up;	// the wearer's frame
		TMap<FName, FTransform> RefWorld;	// every bone in the skeleton's rest pose, placed where the body stands
	};

	/** Rest-pose world transforms of every bone, so the kit fits the same whatever pose the body is in right now. */
	void BuildRefPose(FCtx& C)
	{
		const FReferenceSkeleton& Ref = C.Body->GetSkeletalMeshAsset()->GetRefSkeleton();
		const TArray<FTransform>& Local = Ref.GetRefBonePose();
		TArray<FTransform> Space;
		Space.SetNum(Local.Num());
		for (int32 i = 0; i < Local.Num(); ++i)
		{
			const int32 Parent = Ref.GetParentIndex(i);
			Space[i] = Parent == INDEX_NONE ? Local[i] : Local[i] * Space[Parent];
			C.RefWorld.Add(Ref.GetBoneName(i), Space[i] * C.Body->GetComponentTransform());
		}
	}

	FVector Bone(const FCtx& C, const TCHAR* Name)
	{
		const FTransform* T = C.RefWorld.Find(FName(Name));
		return T ? T->GetLocation() : C.Body->GetSocketLocation(FName(Name));
	}

	/** One piece: a mesh sized SizeCm (bounding box), centred at World, rotated Rot (world), attached to BoneName. */
	UStaticMeshComponent* Piece(FCtx& C, UStaticMesh* Mesh, const TCHAR* BoneName, const FVector& World, const FQuat& Rot, const FVector& SizeCm,
		const FLinearColor& Color, float Emissive = 0.f, bool bHead = false)
	{
		if (!Mesh)
		{
			return nullptr;
		}
		UStaticMeshComponent* P = NewObject<UStaticMeshComponent>(C.Owner);
		P->SetStaticMesh(Mesh);
		P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		P->SetCanEverAffectNavigation(false);
		P->SetGenerateOverlapEvents(false);
		P->SetCastShadow(true);
		const FVector Extent = Mesh->GetBounds().BoxExtent * 2.f;
		P->SetWorldScale3D(FVector(SizeCm.X / FMath::Max(Extent.X, 1.f), SizeCm.Y / FMath::Max(Extent.Y, 1.f), SizeCm.Z / FMath::Max(Extent.Z, 1.f)));
		// Meshes may not be centred on their pivot: shift so the bounds centre lands on World.
		const FVector Scale = P->GetComponentScale();
		const FVector PivotOffset = Rot.RotateVector(Mesh->GetBounds().Origin * Scale);
		// Where the piece sits on the rest pose, expressed relative to its bone there; attached with that offset it
		// follows the bone through every animation.
		const FTransform PieceWorld(Rot, World - PivotOffset, Scale);
		const FTransform* BoneWorld = C.RefWorld.Find(FName(BoneName));
		P->RegisterComponent();
		P->AttachToComponent(C.Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, FName(BoneName));
		if (BoneWorld)
		{
			P->SetRelativeTransform(PieceWorld.GetRelativeTransform(*BoneWorld));
		}
		if (UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(C.M.Base, P))
		{
			Mat->SetVectorParameterValue(TEXT("Color"), Color);
			Mat->SetVectorParameterValue(TEXT("Emissive"), Color * Emissive);
			P->SetMaterial(0, Mat);
		}
		if (Emissive > 0.f)
		{
			P->ComponentTags.Add(OpticTag);
		}
		if (bHead)
		{
			P->ComponentTags.Add(TEXT("DVHead"));
			P->SetOwnerNoSee(C.bHideHead);
		}
		C.Parts.Add(P);
		return P;
	}

	/** Rotation whose X runs along the limb from A to B, with Z kept facing the wearer's front/up as far as it can. */
	FQuat Along(const FCtx& C, const FVector& A, const FVector& B)
	{
		const FVector X = (B - A).GetSafeNormal();
		return FRotationMatrix::MakeFromXZ(X, C.Fwd).ToQuat();
	}

	FQuat Facing(const FCtx& C)
	{
		return FRotationMatrix::MakeFromXZ(C.Fwd, C.Up).ToQuat();
	}

	/** A plate on a limb segment, offset toward the front (or outward) of the limb. */
	void LimbPlate(FCtx& C, const TCHAR* From, const TCHAR* To, float Along01, float Length, float Width, float Thick, const FVector& Offset, const FLinearColor& Color)
	{
		const FVector A = Bone(C, From), B = Bone(C, To);
		const FVector Mid = FMath::Lerp(A, B, Along01) + Offset;
		Piece(C, C.M.Box, From, Mid, Along(C, A, B), FVector(Length, Thick, Width), Color);
	}
}

void DVArmour::Build(AActor* Owner, USkeletalMeshComponent* Body, EDVArmourStyle Style, const FLinearColor& Paint,
	const FDVArmourMeshes& Meshes, TArray<UStaticMeshComponent*>& Parts, bool bHideHead)
{
	using namespace DVArmourParts;
	for (UStaticMeshComponent* P : Parts)
	{
		if (P)
		{
			P->DestroyComponent();
		}
	}
	Parts.Reset();
	if (!Owner || !Body || !Body->GetSkeletalMeshAsset() || !Meshes.Box || !Meshes.Base)
	{
		return;
	}
	FCtx C{ Owner, Body, Meshes, Parts, bHideHead, Owner->GetActorForwardVector().GetSafeNormal2D(), Owner->GetActorRightVector().GetSafeNormal2D(), FVector::UpVector, {} };
	BuildRefPose(C);
	const FQuat Face = Facing(C);
	const FVector Head = Bone(C, TEXT("head")), Neck = Bone(C, TEXT("neck_01"));
	const FVector Chest = Bone(C, TEXT("spine_05")), Belly = Bone(C, TEXT("spine_03")), Pelvis = Bone(C, TEXT("pelvis"));
	const FVector HeadTop = Head + C.Up * 6.f;	// the middle of the skull
	const FLinearColor Dark(0.03f, 0.033f, 0.036f);

	switch (Style)
	{
	case EDVArmourStyle::Marine:
	{
		const FLinearColor Plate = Paint;
		const FLinearColor Webbing(0.06f, 0.06f, 0.05f);
		// Helmet: a rounded shell, a brow ridge, a cyan visor band, ear guards and a little comms boom.
		Piece(C, Meshes.Sphere, TEXT("head"), HeadTop + C.Fwd * 1.5f, Face, FVector(27.f, 25.f, 24.f), Plate, 0.f, true);
		Piece(C, Meshes.Box, TEXT("head"), HeadTop + C.Fwd * 11.f + C.Up * 2.f, Face, FVector(6.f, 22.f, 4.f), Plate * 0.8f, 0.f, true);
		Piece(C, Meshes.Box, TEXT("head"), HeadTop + C.Fwd * 11.f - C.Up * 4.f, Face, FVector(3.f, 19.f, 6.f), FLinearColor(0.15f, 0.75f, 1.f), 6.f, true);
		for (const float S : { -1.f, 1.f })
		{
			Piece(C, Meshes.Box, TEXT("head"), HeadTop + C.Right * 12.5f * S - C.Up * 4.f, Face, FVector(10.f, 3.f, 9.f), Dark, 0.f, true);
		}
		Piece(C, Meshes.Cylinder, TEXT("head"), HeadTop + C.Right * 13.f + C.Up * 8.f - C.Fwd * 3.f, FQuat::Identity, FVector(1.f, 1.f, 16.f), Dark, 0.f, true);
		// Plate carrier: chest and back plates, a collar, three magazine pouches and a cyan unit light.
		Piece(C, Meshes.Box, TEXT("spine_05"), FMath::Lerp(Chest, Belly, 0.35f) + C.Fwd * 13.f, Face, FVector(8.f, 34.f, 34.f), Plate);
		Piece(C, Meshes.Box, TEXT("spine_05"), FMath::Lerp(Chest, Belly, 0.35f) - C.Fwd * 13.f, Face, FVector(8.f, 32.f, 32.f), Plate * 0.9f);
		Piece(C, Meshes.Box, TEXT("spine_05"), Neck - C.Up * 2.f, Face, FVector(18.f, 26.f, 6.f), Webbing);
		for (int32 i = -1; i <= 1; ++i)
		{
			Piece(C, Meshes.Box, TEXT("spine_03"), Belly + C.Fwd * 17.f + C.Right * 9.f * i - C.Up * 2.f, Face, FVector(6.f, 8.f, 11.f), Webbing);
		}
		Piece(C, Meshes.Box, TEXT("spine_05"), Chest + C.Fwd * 17.5f + C.Right * 9.f, Face, FVector(1.f, 4.f, 2.f), FLinearColor(0.2f, 0.7f, 1.f), 8.f);
		// Pack with a whip antenna.
		Piece(C, Meshes.Box, TEXT("spine_04"), Belly + C.Up * 8.f - C.Fwd * 25.f, Face, FVector(14.f, 28.f, 34.f), Webbing);
		Piece(C, Meshes.Cylinder, TEXT("spine_04"), Belly + C.Up * 40.f - C.Fwd * 27.f + C.Right * 10.f, FQuat::Identity, FVector(0.8f, 0.8f, 50.f), Dark);
		// Shoulders, belt, thigh and shin plates, knee pads.
		for (const TCHAR* Side : { TEXT("_l"), TEXT("_r") })
		{
			const FString U = FString(TEXT("upperarm")) + Side, L = FString(TEXT("lowerarm")) + Side, T = FString(TEXT("thigh")) + Side, K = FString(TEXT("calf")) + Side, F = FString(TEXT("foot")) + Side;
			const FVector Up = Bone(C, *U), Low = Bone(C, *L);
			Piece(C, Meshes.Box, *U, FMath::Lerp(Up, Low, 0.15f) + C.Up * 5.f, Along(C, Up, Low), FVector(13.f, 12.f, 8.f), Plate, 0.f, true);
			LimbPlate(C, *L, *(FString(TEXT("hand")) + Side), 0.45f, 18.f, 9.f, 9.f, FVector::ZeroVector, Webbing);
			LimbPlate(C, *T, *K, 0.42f, 20.f, 15.f, 15.f, C.Fwd * 2.f, Plate);
			const FVector Knee = Bone(C, *K);
			Piece(C, Meshes.Sphere, *K, Knee + C.Fwd * 7.f, Face, FVector(9.f, 9.f, 8.f), Dark);
			LimbPlate(C, *K, *F, 0.45f, 22.f, 11.f, 12.f, C.Fwd * 1.5f, Plate);
		}
		Piece(C, Meshes.Box, TEXT("pelvis"), Pelvis + C.Up * 6.f, Face, FVector(30.f, 34.f, 7.f), Webbing);
		break;
	}
	case EDVArmourStyle::WarFrame:
	{
		const FLinearColor Chrome(0.02f, 0.02f, 0.022f);
		const FLinearColor Red(1.f, 0.06f, 0.02f);
		// Helm: angular, with a wide red optic slit, a crest and two sensor spikes.
		Piece(C, Meshes.Box, TEXT("head"), HeadTop + C.Fwd * 1.f, Face, FVector(26.f, 22.f, 26.f), Chrome, 0.f, true);
		Piece(C, Meshes.Box, TEXT("head"), HeadTop + C.Fwd * 13.5f + C.Up * 1.f, Face, FVector(2.f, 20.f, 3.5f), Red, 30.f, true);
		Piece(C, Meshes.Box, TEXT("head"), HeadTop + C.Up * 14.f, Face, FVector(22.f, 4.f, 6.f), Chrome, 0.f, true);
		for (const float S : { -1.f, 1.f })
		{
			Piece(C, Meshes.Cylinder, TEXT("head"), HeadTop + C.Right * 12.f * S + C.Up * 12.f - C.Fwd * 4.f,
				FQuat(C.Fwd, FMath::DegreesToRadians(20.f * S)), FVector(1.2f, 1.2f, 22.f), Chrome, 0.f, true);
		}
		// Heavy chest with a glowing core, armoured abdomen ribs, twin exhaust vents on the back.
		Piece(C, Meshes.Box, TEXT("spine_05"), FMath::Lerp(Chest, Belly, 0.3f) + C.Fwd * 12.f, Face, FVector(12.f, 40.f, 36.f), Chrome);
		Piece(C, Meshes.Sphere, TEXT("spine_05"), FMath::Lerp(Chest, Belly, 0.3f) + C.Fwd * 18.f, Face, FVector(7.f, 7.f, 7.f), Red, 25.f);
		for (int32 i = 0; i < 3; ++i)
		{
			Piece(C, Meshes.Box, TEXT("spine_03"), Belly + C.Fwd * 13.f - C.Up * (6.f + i * 7.f), Face, FVector(5.f, 26.f - i * 3.f, 4.f), Chrome);
		}
		Piece(C, Meshes.Box, TEXT("spine_05"), FMath::Lerp(Chest, Belly, 0.3f) - C.Fwd * 14.f, Face, FVector(12.f, 36.f, 34.f), Chrome);
		for (const float S : { -1.f, 1.f })
		{
			Piece(C, Meshes.Cylinder, TEXT("spine_05"), Chest - C.Fwd * 22.f + C.Right * 9.f * S + C.Up * 6.f, FQuat::Identity, FVector(7.f, 7.f, 26.f), Dark);
			Piece(C, Meshes.Cylinder, TEXT("spine_05"), Chest - C.Fwd * 22.f + C.Right * 9.f * S + C.Up * 19.5f, FQuat::Identity, FVector(5.f, 5.f, 1.f), Red, 12.f);
		}
		for (const TCHAR* Side : { TEXT("_l"), TEXT("_r") })
		{
			const FString U = FString(TEXT("upperarm")) + Side, L = FString(TEXT("lowerarm")) + Side, T = FString(TEXT("thigh")) + Side, K = FString(TEXT("calf")) + Side, F = FString(TEXT("foot")) + Side;
			const FVector Up = Bone(C, *U), Low = Bone(C, *L);
			// Big angular pauldrons with a red seam, forearm guards, armoured thighs and shins.
			Piece(C, Meshes.Box, *U, FMath::Lerp(Up, Low, 0.15f) + C.Up * 6.f, Along(C, Up, Low), FVector(17.f, 15.f, 11.f), Chrome, 0.f, true);
			Piece(C, Meshes.Box, *U, FMath::Lerp(Up, Low, 0.15f) + C.Up * 12.f, Along(C, Up, Low), FVector(15.f, 1.5f, 1.f), Red, 20.f, true);
			LimbPlate(C, *L, *(FString(TEXT("hand")) + Side), 0.45f, 20.f, 11.f, 11.f, FVector::ZeroVector, Chrome);
			LimbPlate(C, *T, *K, 0.45f, 24.f, 17.f, 17.f, C.Fwd * 2.f, Chrome);
			LimbPlate(C, *K, *F, 0.45f, 26.f, 13.f, 14.f, C.Fwd * 1.5f, Chrome);
		}
		break;
	}
	case EDVArmourStyle::Clockwork:
	{
		const FLinearColor Brass(0.55f, 0.36f, 0.12f), Amber(1.f, 0.55f, 0.12f);
		// A bell-shaped brass helmet with a brim, twin amber lenses and a little smokestack.
		Piece(C, Meshes.Cylinder, TEXT("head"), HeadTop + C.Up * 2.f, FQuat::Identity, FVector(24.f, 24.f, 24.f), Brass, 0.f, true);
		Piece(C, Meshes.Cylinder, TEXT("head"), HeadTop - C.Up * 9.f, FQuat::Identity, FVector(32.f, 32.f, 2.f), Brass * 0.8f, 0.f, true);
		for (const float S : { -1.f, 1.f })
		{
			Piece(C, Meshes.Sphere, TEXT("head"), HeadTop + C.Fwd * 12.f + C.Right * 5.f * S, Face, FVector(4.f, 6.f, 6.f), Amber, 12.f, true);
		}
		Piece(C, Meshes.Cylinder, TEXT("head"), HeadTop + C.Up * 18.f - C.Fwd * 5.f, FQuat::Identity, FVector(4.f, 4.f, 12.f), Dark, 0.f, true);
		Piece(C, Meshes.Box, TEXT("spine_05"), FMath::Lerp(Chest, Belly, 0.35f) + C.Fwd * 12.f, Face, FVector(8.f, 32.f, 34.f), Brass);
		Piece(C, Meshes.Cylinder, TEXT("spine_04"), Belly + C.Up * 6.f - C.Fwd * 22.f, FQuat::Identity, FVector(22.f, 22.f, 36.f), Brass * 0.7f);
		break;
	}
	case EDVArmourStyle::Helper:
	{
		const FLinearColor Sage(0.3f, 0.55f, 0.42f), Rose(1.f, 0.42f, 0.7f), Cream(0.85f, 0.8f, 0.68f);
		Piece(C, Meshes.Sphere, TEXT("head"), HeadTop, Face, FVector(30.f, 30.f, 28.f), Sage, 0.f, true);
		for (const float S : { -1.f, 1.f })
		{
			Piece(C, Meshes.Sphere, TEXT("head"), HeadTop + C.Fwd * 13.f + C.Right * 6.f * S + C.Up * 1.f, Face, FVector(3.f, 5.f, 5.f), Rose, 8.f, true);
		}
		Piece(C, Meshes.Box, TEXT("spine_03"), Belly + C.Fwd * 14.f - C.Up * 4.f, Face, FVector(3.f, 30.f, 36.f), Cream);
		break;
	}
	}
}

void DVArmour::SetOptics(TArray<UStaticMeshComponent*>& Parts, bool bOn)
{
	for (UStaticMeshComponent* P : Parts)
	{
		if (P && P->ComponentHasTag(DVArmourParts::OpticTag))
		{
			if (UMaterialInstanceDynamic* Mat = Cast<UMaterialInstanceDynamic>(P->GetMaterial(0)))
			{
				FLinearColor Color;
				Mat->GetVectorParameterValue(FHashedMaterialParameterInfo(TEXT("Color")), Color);
				Mat->SetVectorParameterValue(TEXT("Emissive"), bOn ? Color * 20.f : FLinearColor::Black);
			}
		}
	}
}
