#pragma once

#include "CoreMinimal.h"

class UWorld;

/**
 * Procedural points of interest for operation maps, generated when the operation starts.
 *
 * The pipeline follows the shape of Azgaar's Fantasy-Map-Generator (settings, then generators, then world data, then a
 * renderer) and samples the way Red Blob Games' mapgen4 does (Poisson-disk points, not a grid or pure randomness):
 *  1. Settings: how many of each kind, how far apart, what to keep clear of.
 *  2. Generator: Bridson Poisson-disk samples over the arena oval, each dropped onto the ground with a trace and kept
 *     only on flat ground that the navmesh can walk to from the player start, clear of objectives and enemies.
 *  3. World data: intel spread by farthest-point selection, the survivor far from where you land, supply caches
 *     about halfway out, wrecks in what's left.
 *  4. Renderer: one ADVPointOfInterest per entry (it builds its own look).
 * Deterministic per map: the seed is the map's name, so every player in a session, and every replay, gets the same layout.
 */
namespace DVWorldGen
{
	struct FSettings
	{
		int32 Intel = 3;
		int32 Survivors = 1;
		int32 Caches = 2;
		int32 Wrecks = 2;
		float Spacing = 1400.f;			// nothing closer together than this (cm)
		FVector2D Arena = FVector2D(5600.f, 4300.f);	// the oval every planet map shares
		float ArenaFill = 0.92f;
		float KeepClear = 1300.f;		// from objectives, enemies, vehicles, townsfolk
		float KeepClearStart = 2600.f;	// from the player start and landing zone
		float MinNormalZ = 0.9f;		// flatter than about 25 degrees
	};

	/** Server: places the points if the map has none already. Returns how many it placed (0 if the navmesh isn't ready). */
	DELTAVDOMINION_API int32 PlacePointsOfInterest(UWorld* World, const FString& MapKey, const FSettings& Settings = FSettings());
}
