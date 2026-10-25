# Test report: Hollyhock Vale, Operation Lantern Night

**Slice:** the gentle world from Act IV. Hollyhock Vale (Tau Ceti f) was raised by a caretaker AI, Nana Bramble, who
never adopted CINDER's logic. A storm and the Static frightened her helper bots, broke the windpumps and scattered the
puffbots. Delta's rifles fire foam here. The job: calm the helpers, mend the pumps, bring the puffbots home, and stay
for the lantern festival. Hurting a villager twice ends the operation.

**References** (one principle each, nothing copied):
- **Herding: sheepdog trials and farm games.** Animals respond to how you approach. We reversed it into something
  gentler: walk up slowly and a puffbot follows you like a duckling; sprint at it and it scatters.
- **Non-lethal rules: the campaign's own rule set** in [CAMPAIGN.md](CAMPAIGN.md). It's the third rule book, after
  lethal and restrained.
- **Look: the art-direction table's "bright utopian city of trees and saucers"**, as round cottages, a maypole and
  pastel robots.

## Implemented, partial, planned

| Status | What |
|---|---|
| **Implemented** | The map: a festival square with a maypole and lantern strings, nine round cottages with flower boxes, stepped flower terraces, Nana's domed glasshouse, a red barn with a fenced puffbot pen, three windpumps, hay bales and rolling hills of trees |
| **Implemented** | Non-lethal rules: rifles fire pink foam streaks and puffs; helper bots are **calmed** (they slump, optics dark, and the kill feed says "calmed"); orbital strikes and strafing runs are refused; the first villager hit gets a warning, the second ends the operation |
| **Implemented** | `ADVRepairPoint`: stand beside a broken windpump for 5 s to mend it. The loose sail swings back, the rotor spins up and the lamp turns green |
| **Implemented** | `ADVPuffbot` and `ADVPen`: puffbots follow a soldier who walks up gently, scatter from one who sprints, ignore your AI squadmates, and trot into the pen by themselves once they're near the barn. Bullets pass through them |
| **Implemented** | The festival: when the last puffbot is home, lantern fireworks burst over the square, Nana thanks you, and the Raven comes in |
| **Implemented** | Hollyhock Vale as the fifth faction (+15 for the festival, +10 more if nobody was hurt, -15 if you're sent home); Tau Ceti on the war table |
| Partial | Foam is a rule on the existing rifles, not its own launcher model. Calmed helpers use the war-frame body painted sage, and the slump is a pose change |
| Planned | Hover-carts, the picnic-basket shuttle, riddles, a village hub with its own contracts |

## Test runs (observed)

Platform: Windows 11, Unreal Engine 5.8, Development editor, off-screen renderer at 1280×720. Driven by
`DVClearHostiles`, `DVRepairTest`, `DVHerdTest` and `DVSkipStage`, which play with real movement input (no teleports).

1. **Calm and mend.** All six helper bots were calmed and the stage moved to the pumps. `DVRepairTest` walked to
   each pump and stood beside it: **all three mended in 38.4 s**, and the stage moved to the puffbots.
2. **Herding, first design (puffbots flee and you push them).** It was too fiddly: the test soldier got **2 of 5 home
   in 5 minutes**. Problems found:
   - your AI squadmates kept spooking the puffbots;
   - villagers stopped in the soldier's path to chat;
   - one puffbot never moved at all because it had no AI controller;
   - puffbots crowded in the gateway without counting as home.
3. **Herding, final design (walk up gently and they follow).** After the fixes, **all five puffbots were penned in
   77.9 s**. The log shows the test soldier fetching one or two at a time, leading them to the gate, and standing back
   while they trotted in.
4. **The festival.** With the puffbots home, the objective changed to the landing zone, Nana's and Captain Vance's
   lines played, and fireworks burst over the square (see `holly_festival.gif`).
5. **Bugs found and fixed:** the four herding problems above; flower beds glowing like lamps; a mirrored disc on the
   square; puffbots spawned on the path slabs; extraction setting off a demolition blast (Brasshaven had the same
   bug).
6. **Not covered.** Playing it by hand with mouse and keyboard; co-op over two machines; the packaged build. The test
   soldier doesn't pathfind, so it sometimes walked into cottage walls on the way; that's the test driver, not the game.

## Media

`docs/media/hollyhock/`: `holly_square.png`, `holly_vale.png`, `holly_follow.png`, `holly_pen.png`,
`holly_windpump.png`, `holly_terraces.png`, `holly_festival.gif`.
