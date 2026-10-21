#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "DVTownsfolk.generated.h"

/**
 * A resident of a hub town (Rook's Rest): wanders the market on the navmesh, stops to face a commander who walks up,
 * and says something that depends on how the Free Ports feel about Delta. Never a combatant; the squad AI ignores them.
 */
UCLASS()
class DELTAVDOMINION_API ADVTownsfolk : public ACharacter
{
	GENERATED_BODY()

public:
	ADVTownsfolk();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Town") FString DisplayName = TEXT("Dockhand");
	/** 0 trader, 1 dockhand, 2 drifter, 3 mechanic, 4 colony kid: picks what they talk about (named Trade: AActor already has Role). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Town") int32 Trade = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Town") bool bWanders = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Town") float WanderRadius = 1400.f;
	/** Says this instead of the trade's lines (crew, patients and evacuees aboard the Kestrel). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Town") FString CustomLine;
	/** Lying on a bed (medbay patients): no walking, no turning to face visitors. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Town") bool bLyingDown = false;

	static constexpr float TalkRange = 260.f;

	/** The townsperson the player is close enough to hear, if any. */
	static ADVTownsfolk* FindNear(const AActor* Listener);
	/** What they say to the local commander right now (depends on Free Ports standing). */
	FString Line(int32 FreePortsRep) const;

	/** Server: a shot or blast from Shooter hit or landed beside them. They run, and the game mode counts a civilian hurt
	 *  (restrained rules of engagement: see ADVGameMode::NotifyCivilianHurt). Never fatal. */
	void OnShot(AActor* Shooter);
	/** Server: every civilian within Radius of Center reacts to a blast that Shooter set off. */
	static void BlastNear(UWorld* World, const FVector& Center, float Radius, AActor* Shooter);

protected:
	void Wander();
	UPROPERTY() TObjectPtr<USkeletalMesh> AltBody;
	FVector Home = FVector::ZeroVector;
	double NextWander = 0.0;
	int32 Seed = 0;
	double LastShot = -100.0;
	double FleeUntil = -100.0;
};
