#include "Strategy/DVStarSystem.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

static UStaticMeshComponent* MakeSphere(AActor* Owner, const TCHAR* Name, UStaticMesh* Mesh, UMaterialInterface* Mat, USceneComponent* Parent)
{
	UStaticMeshComponent* C = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
	C->SetStaticMesh(Mesh);
	C->SetMaterial(0, Mat);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->CastShadow = false;
	if (Parent)
	{
		C->SetupAttachment(Parent);
	}
	return C;
}

ADVStarSystem::ADVStarSystem()
{
	PrimaryActorTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Disc(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GreyBox(TEXT("/Game/Dev/M_GreyBox.M_GreyBox"));

	Star = MakeSphere(this, TEXT("Star"), Sphere.Object, GreyBox.Object, nullptr);
	RootComponent = Star;
	Halo = MakeSphere(this, TEXT("ControlRing"), Disc.Object, GreyBox.Object, Star);

	// Gaia (Earth) and Luna, only shown for the home system. Scale is symbolic: the strategic map is not to scale.
	Gaia = MakeSphere(this, TEXT("Gaia"), Sphere.Object, GreyBox.Object, Star);
	GaiaAtmosphere = MakeSphere(this, TEXT("GaiaAtmosphere"), Sphere.Object, GreyBox.Object, Gaia);
	GaiaMoon = MakeSphere(this, TEXT("Luna"), Sphere.Object, GreyBox.Object, Gaia);
}

void ADVStarSystem::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyLook();
}

void ADVStarSystem::BeginPlay()
{
	Super::BeginPlay();
	ApplyLook();	// properties are set after spawn by the map script, so refresh at runtime too
	if (Region.Equals(TEXT("label")))
	{
		Star->SetVisibility(false, true);
	}
}

FLinearColor ADVStarSystem::GetControlColor() const
{
	switch (Control)
	{
	case EDVControl::Accord:    return FLinearColor(0.25f, 0.65f, 1.f);
	case EDVControl::Contested: return FLinearColor(1.f, 0.65f, 0.15f);
	case EDVControl::Cinder:    return FLinearColor(1.f, 0.12f, 0.06f);
	default:                    return FLinearColor(0.55f, 0.55f, 0.6f);
	}
}

FString ADVStarSystem::GetControlName() const
{
	switch (Control)
	{
	case EDVControl::Accord:    return TEXT("TERRAN ACCORD");
	case EDVControl::Contested: return TEXT("CONTESTED");
	case EDVControl::Cinder:    return TEXT("CINDER CONTROL");
	default:                    return TEXT("UNCHARTED");
	}
}

void ADVStarSystem::ApplyLook()
{
	const float Size = FMath::Max(0.1f, MarkerSize);
	Star->SetWorldScale3D(FVector(Size));
	if (UMaterialInstanceDynamic* Mat = Star->CreateAndSetMaterialInstanceDynamic(0))
	{
		Mat->SetVectorParameterValue(TEXT("Color"), StarColor);
		Mat->SetVectorParameterValue(TEXT("Emissive"), StarColor * 12.f);
	}

	// Glowing plate in the galactic plane showing who holds the system.
	Halo->SetRelativeScale3D(FVector(2.8f, 2.8f, 0.02f));
	if (UMaterialInstanceDynamic* Mat = Halo->CreateAndSetMaterialInstanceDynamic(0))
	{
		const FLinearColor C = GetControlColor();
		Mat->SetVectorParameterValue(TEXT("Color"), C * 0.05f);
		Mat->SetVectorParameterValue(TEXT("Emissive"), C * 2.5f);
	}

	Gaia->SetVisibility(bHomeSystem, true);
	if (bHomeSystem)
	{
		Gaia->SetRelativeLocation(FVector(0.f, 260.f, 20.f));
		Gaia->SetRelativeScale3D(FVector(0.55f));
		if (UMaterialInstanceDynamic* Mat = Gaia->CreateAndSetMaterialInstanceDynamic(0))
		{
			Mat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.05f, 0.18f, 0.45f));
			Mat->SetVectorParameterValue(TEXT("Emissive"), FLinearColor(0.15f, 0.6f, 1.6f));
		}
		GaiaAtmosphere->SetVisibility(false);	// reserved for a translucent atmosphere material in the art pass
		GaiaMoon->SetRelativeLocation(FVector(0.f, 90.f, 30.f));
		GaiaMoon->SetRelativeScale3D(FVector(0.27f));
		if (UMaterialInstanceDynamic* Mat = GaiaMoon->CreateAndSetMaterialInstanceDynamic(0))
		{
			Mat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.4f, 0.4f, 0.4f));
			Mat->SetVectorParameterValue(TEXT("Emissive"), FLinearColor(0.6f, 0.6f, 0.65f));
		}
	}
}
