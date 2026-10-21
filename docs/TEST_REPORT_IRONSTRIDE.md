# Test report: the Ironstride walker

**Slice:** a 30 m Hearthbound walker strides a loop across Arcadia Planitia, Mars. To bring it down, you board it
when it kneels, ride it, and overload three power couplings on its back. It collapses, and you extract on a Raven.

**References** (from the inspiration atlas):
- **Gameplay: Shadow of the Colossus**, for scale and the idea that the enemy is also the terrain you climb. We take
  only that principle. Our walker is a machine with a deck, a ramp and couplings, not a creature with fur to climb,
  and there's no grip or stamina system.
- **Prototype pattern: the atlas's "giant repair machine"**: observe a moving mechanism, board it safely, act on a
  fault, and return. Our fault is sabotage rather than repair, which suits the war.
- **Visual: the project's own kit-bashed industrial set** (gunmetal, navy, rust, glow strips), so the walker reads
  as part of the same Hearthbound family as the war-frames.

## How it plays

- **Watch it move.** The walker strides a loop with four legs that really step: diagonal pairs, feet planted on the
  terrain, knees solved as two-bone legs. The body heaves with each step.
- **Board.** At two points on its loop it kneels for 18 s and drops a rear ramp. The HUD says what it's doing: "The
  walker is striding. Wait for it to kneel", then "The rear ramp is down: board now". It won't raise the ramp while
  someone is standing on it, for up to 10 s more.
- **Ride.** The deck is a moving floor. You stay on it as it stands up, walks and turns.
- **Overload.** Stand next to the glowing coupling and it overloads in 4 s. The coupling flickers faster, the HUD
  shows the percentage, and the radio calls each one. Walk away and it slowly re-stabilises.
- **Its cannon.** Every 7 s it marks a red ring on the ground under the nearest player who isn't aboard. The shell
  lands 2.2 s later.
- **Collapse, then extract.** When the third coupling blows, the legs give, the walker drops 20 m with you on it,
  and Raven Two-One comes in to the landing zone.
- **Recoverable failure.** Miss the kneel, or fall off, and the walker comes around and kneels again. There's no fall
  damage, and thrusters cushion the landing.

## Test runs (observed)

Platform: Windows 11, Unreal Engine 5.8, Development editor, off-screen renderer at 1280×720. The base revision was 0b9bce6.

Boarding was tested with the `DVBoardTest` command. It drives the soldier with real movement input every frame, the
same as holding a key. It doesn't teleport. `DVWalker 3` first clears the hostiles and silences the cannon, so the
run measures the walker rather than the firefight.

1. **Board, ride and bring it down.**
   - Arrived at the ramp, but it rose before the soldier reached it: retried.
   - Second kneel: **aboard after 84 s**.
   - Rode the deck as it stood up: the soldier's height went from 9 m to 17 m, staying on the deck.
   - Overloaded all three couplings. The walker went to state Down.
   - HUD and radio afterwards: "It's down. Nine hundred tonnes of it. Raven Two-One is inbound", then "Get to the
     landing zone for extraction" and "RAVEN INBOUND".
2. **Fall recovery.**
   - Aboard at 84 s, rode it up, then walked off the stern **27 m above the ground**.
   - Logged as fallen, walked back, and boarded again at the next kneel (**156 s**).
   - Then overloaded all three couplings and it collapsed.
3. **Bug found and fixed during testing.**
   - A soldier standing on the top of the ramp counted as aboard. When the ramp retracted, they were knocked off.
   - Fixes:
     - "Aboard" now means the hull only.
     - The walker keeps the ramp down while anyone is on it, for up to 10 s.
   - Re-run: no accidental falls.
4. **Bug found and fixed.** The deck guards were spawned without an AI controller, so they floated where they
   spawned. They now get one and ride the deck.
5. **With the full garrison and the cannon active** (no cheats), the test soldier, who doesn't dodge or shoot, was
   downed several times by the cannon and the garrison before reaching the ramp. Real play needs you to clear or
   avoid the ground fire and step out of the red ring. That's intended, but the difficulty hasn't been tuned with a
   human player yet.

6. **Packaged build** (557 MB zip): Ironstride loads in the packaged game. The walker strides with its legs
   stepping, and the HUD guidance and objective work. Screenshot: `Media/Screenshots/ironstride_packaged_build.png`.
   The scripted `DVBoardTest` run did **not** start in the packaged build: no test log lines appeared, so boarding
   is verified in the editor's game mode only. The cause hasn't been investigated yet.

## Implemented, partial, planned

- **Implemented:**
  - the walker: legs, gait, deck, railings, ramp, couplings, head and cannon;
  - kneel schedule, ramp hold, overload, collapse;
  - the telegraphed cannon;
  - two deck guards;
  - the Ironstride operation (objective, HUD guidance, radio, extraction);
  - the map, its galaxy entry and its packaging entry;
  - dev and test commands `DVWalker 0|1|2|3` and `DVBoardTest [Delay] [JumpOff]`.
- **Partial:**
  - The squad AI doesn't board the walker. There's no navmesh on a moving deck, so they fight from the ground.
  - Legs have no collision; you can walk through a foot.
  - The guards don't use cover on the deck.
- **Planned:**
  - climbing a leg as a second way aboard;
  - a weak point on the belly for the Raven;
  - walker variants for later worlds.

## Known issues

- The first-person camera can sit very close to a coupling while you overload it, so the glow fills much of the screen.
- The walker's loop is fixed for this map.

## Asset manifest

The walker, ramp, couplings and ring are all built at runtime from the project's own kit meshes and materials
(`DVKit`). The map comes from `Scripts/build_planet_map.py -biome=ironstride`. There are no imported assets.
