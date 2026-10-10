# DELTA-V: DOMINION

Unreal Engine 5.6 C++ project. This first drop covers milestones 1 and 2 of the design brief:
the soldier, GAS weapons, squad AI with orders, and persistent roster attrition.

## Open it

1. Install UE 5.6 and Visual Studio 2022 (Game development with C++ workload).
2. Right-click `DeltaVDominion.uproject` > **Generate Visual Studio project files**.
3. Open `DeltaVDominion.sln`, set **Development Editor / Win64**, build, then launch.
4. Create a test map: a floor, some walls/crates for cover, a **NavMeshBoundsVolume**
   (press P to check the green navmesh), and a Player Start. Press Play.

The game mode, pawn and input all work with **no assets**: the soldier has a camera and a
code-built default input layout. You won't see arms or a gun until you assign meshes (below).

Optional: the Fab/Quixel/Lyra assets the brief lists are not included (they're licensed downloads).

## Controls (fallback layout)

| Key | Action | Key | Action |
|---|---|---|---|
| WASD / Mouse | Move / look | F | Squad: move to crosshair |
| LMB / RMB | Fire / aim down sights | G | Squad: hold position |
| R | Reload | V | Squad: regroup on me |
| Shift | Sprint | T | Squad: suppress target/area |
| C | Crouch | X | Squad: flank target |
| Q / E | Lean left / right | Space | Jump |

## Wiring up art (in the editor)

- **Soldier**: create `BP_Soldier` from `DVSoldierCharacter`. Set `Mesh` (third-person body with a
  physics asset, for per-bone hits and headshots), `FirstPersonMesh` (arms), anim blueprints, and
  optionally your own Input Mapping Context / Input Actions (they replace the fallback layout).
  Set it as `DefaultPawnClass` and `SquadmateClass` on a `BP_GameMode` child of `DVGameMode`.
- **Weapons**: Content Browser > Data Asset > `DVWeaponData`, one per weapon (M9 Bastion, Kite,
  Halberd: `PelletsPerShot` 8, Longreach, Pulse Carbine: `bUsesHeat`). Assign mesh, muzzle socket,
  Niagara muzzle/tracer/impact, sound, montages. Put it in the soldier's `DefaultWeapon`.
  With nothing assigned, the class defaults act as an M9-style auto rifle.
- **Enemies**: place `BP_Soldier` instances with `TeamId = 2` (Hearthbound). They hold where
  placed and fight from cover. Squadmates and the player are `TeamId = 1`.

## Source layout

```
Source/DeltaVDominion/
  DVGameplayTags        Native gameplay tags (abilities, states, SetByCaller)
  Core/                 DVGameInstance, DVGameMode, DVPlayerController,
                        DVRosterSubsystem, DVRosterTypes, DVSaveGame
  Characters/           DVSoldierCharacter, DVCharacterMovementComponent,
                        DVCommandComponent, DVSquadAIController, DVCoverQuery, DVSquadTypes
  Combat/               DVAttributeSet, DVCombatEffects (damage exec + suppression),
                        DVGameplayAbility, DVGA_FireWeapon, DVGA_Reload, DVWeaponBase, DVWeaponData
  Net/                  DVRateLimiter (token bucket for server RPCs)
  Vehicles/ Space/ Campaign/ Strategy/ Physics/ Narrative/ UI/   (placeholders for later milestones)
```

## How the systems work

**Soldier** (`DVSoldierCharacter`): first-person camera and arms for the local player, body for
everyone else. Owns its own AbilitySystemComponent, so AI squadmates and co-op takeovers keep
health, ammo and abilities with the pawn. Sprint and ADS speeds go through the custom movement
component's saved-move flags, so they're predicted and server-corrected rather than trusted.
Lean is a rate-limited server RPC.

**Weapons on GAS**: `DVGA_FireWeapon` is locally predicted. The shooter traces and plays effects at
once, then sends hits to the server as target data. The server checks fire rate, ammo/heat,
pellet count, shot origin, range, aim angle, line of sight (no shooting through walls) and a
ping-scaled target position tolerance before applying `DVDamageEffect`
(SetByCaller damage, x2.5 headshot, armour reduction). Rounds passing near enemies apply
suppression, which widens aim, slows AI and pins them. Recoil patterns, bloom, burst/auto/semi,
shotguns and heat-based weapons are data-driven in `DVWeaponData`.

**Squad AI** (`DVSquadAIController`): sight perception by team, runtime cover search on the
navmesh (low cover preferred, so bots pop up to fire and duck between bursts), and the orders
Follow, Move, Hold, Flank, Suppress and Regroup. On Flank, one bot suppresses while the rest
swing wide. Aim error comes from each soldier's roster Accuracy plus their current suppression.

**Roster and attrition** (`DVRosterSubsystem`): named soldiers with rank, traits, accuracy,
morale, XP, kills and portrait index. KIA is permanent and saved immediately (no undo by
quitting). `DVGameMode::EndMission` gives XP, promotions, morale changes, sends heavily damaged
survivors to the medbay for 2 missions, adds rewards, then recruits greener replacements with
manpower. When manpower runs out, emergency conscripts keep the campaign winnable and set the
`bDireStraits` flag (for the salvage mission). Saves are SHA-1 checksummed and rejected if
edited. In co-op the host's save is the only authority.

**Co-op**: the first player leads; later players take over a living AI squadmate.

## Not done yet (by design for this drop)

- Compiling: this was written without an Unreal install. Expect a few small fixes on first build.
- Lag-compensation rewind (the tolerance check is the placeholder), EOS/EAC wiring
  (config template is in `Config/DefaultEngine.ini`), HUD/command wheel UI, StateTree assets
  (the AI is C++; StateTree tasks can wrap its behaviours later), animation blueprints.
