# Test report: Brasshaven, Operation Steamheart

**Slice:** the first of the three "cure" worlds from Act IV. Brasshaven (Wolf 359 b) lost the Lattice eighty years ago
and rebuilt itself on steam, brass and tube-mail. Someone has fed forged punch-cards to the Guild's clockwork
constables, and the Steamheart, the one foundry the Static can't touch, is running toward red. Delta has to wind the
constables down without scrapping them, balance the Steamheart's pressure by hand, and hold the street while it
relights, all without hurting the townsfolk.

**References** (one principle each, nothing copied):
- **Engineering puzzle: physical valve-and-gauge boards** from steam plants. You read the pipes to learn which
  valve moves which gauge. No minigame screen; the puzzle is the machine in the world.
- **Restrained rules of engagement: the campaign's own rule set** in [CAMPAIGN.md](CAMPAIGN.md). Civilians are
  present, so fire from the sky is refused and hurting people costs you.
- **Look: the art-direction table's "dark steampunk gear-and-sail airship over a smoky canyon city"**, built from the
  project's kit-bash geometry: brick, brass, copper, soot and gas light.

## Implemented, partial, planned

| Status | What |
|---|---|
| **Implemented** | The Brasshaven map: a cobbled street of brick townhouses with pitched roofs and chimneys, gas lamps, tube-mail pipes and junction drums, a clock tower, the Steamheart foundry (a gear over its doors, three stacks, a furnace glow), a moored Guild airship and its mast, a second airship in the haze, canyon walls |
| **Implemented** | `ADVPressureManifold`: three valves, three gauges, shared pipes, safety-valve venting, a beacon that goes from red to green. Replicated, server-checked (a client can only turn the wheel it's standing at) |
| **Implemented** | Clockwork constables (`bClockwork` on the soldier): brass paint, amber optics, "Constable No. 412" names. Taken out of the fight, they **wind down**: they slump where they stand, their optics go dark, they stay for the Guild to collect, and the kill feed says "wound down" |
| **Implemented** | Restrained rules of engagement: orbital strikes and strafing runs are refused on the radio and shown as not cleared. Shots or blasts from Delta that reach a townsperson make them run, cost Guild standing (-4) and are counted |
| **Implemented** | The Guild Council as the fourth faction: +15 when the Steamheart relights, +10 more if no citizen was hurt, -10 on a failure. Shown on the standing panel |
| **Implemented** | The operation: wind down the constables, balance the manifold, hold 45 s against reserve constables, extract. Radio from Guildmaster Ottilie Cogsworth-Vane, hints after 12 and 24 turns, an after-action line for civilians |
| **Implemented** | War table: Wolf 359 in the Local Bubble with Operation Steamheart; the map is in the package list |
| Partial | The wind-down is a pose and lighting change on the mannequin, not an animation. The constables use the war-frame body painted brass, not their own model |
| Partial | Townsfolk who are shot run and are never killed. There's no wounded civilian state or medic escort yet |
| Planned | Airship convoy escort, steam crawlers, duels of honour, the Guild Council town hub and its contracts, rewinding constables to fight for you |

## How the puzzle works

Each valve adds one step of pressure to every gauge it feeds: A feeds gauges 1 and 2, B feeds all three, C feeds 2
and 3. A gauge has five steps; one more past red trips its safety valve and it vents to empty. The goal is all three
in the green (the middle step).

Because the coupling is invertible (mod 5), every starting state has exactly one answer, counting turns 0 to 4 per
valve. Each operation scrambles a start that needs at least five turns. The HUD readout beside the crosshair shows the
three gauges and which ones the valve in front of you feeds.

## Test runs (observed)

Platform: Windows 11, Unreal Engine 5.8, Development editor, off-screen renderer at 1280×720. The base revision was
ce0eb78. Your saves were backed up before the runs and restored afterwards (checked by checksum).

1. **Map build.** `build_planet_map.py -biome=brasshaven` built 581 actors and saved the map; the galaxy map rebuilt
   with Wolf 359 (26 actors).
2. **The whole operation**, driven with `DVClearHostiles 3 0.5`, `DVValve -1`, `DVCivilianTest`,
   `DVOrbitalStrike`, `DVSkipStage` and `DVEndMission 1`:
   - The 8 constables wound down (plus a Bulwark that shouldn't have been there, fixed below); the kill feed read "Constable No. 747 (wound down)". The stage moved on to the
     manifold, and the Guildmaster's radio line played.
   - Log: `Steamheart manifold: gauges 3 1 3 (green is 2); answer A x2, B x2, C x2`. `DVValve -1` walked to each
     wheel and pressed Z through the normal input path (`OnInteractPressed`, then `ServerTurnValve`). All three
     gauges reached the green, the beacon turned green, and the stage moved to "Hold the Valve Quarter while the
     Steamheart relights" with a 45 s RELIGHT timer and reserve constables arriving.
   - Firing on the nearest citizen (the Guildmaster, 5 m away): the radio said "Those are my people, Commander!", the
     standing toast showed -4 Guild Council, and the after-action report read "Civilians fired on: 1".
   - `DVOrbitalStrike`: refused, with Captain Vance's "No fire from orbit" line, and nothing fired.
3. **Puzzle answer check by hand.** A second run started at gauges 1 2 0 and was solved by A x3, B x3, C x4. Working
   it through: gauge 1 is 1+3+3 = 7, gauge 2 is 2+3+3+4 = 12, gauge 3 is 0+3+4 = 7. All are 2 mod 5.
4. **Bugs found and fixed during testing.**
   - A Bulwark was spawning in Brasshaven. It's now skipped on the manifold map, like on Ironstride.
   - The kill feed said "(wound down) killed". It now says "wound down" only.
   - The gauge readout sat on top of the radio subtitles; it moved beside the crosshair.
   - The restrained-rules line overlapped the call-in row; it's shorter now.
   - The green beacon light flooded the whole yard; it's dimmer and shorter-ranged.
   - A stray valve stem sat at the manifold's feet; removed. The back panel was a brass mirror that hid the gauges;
     it's dark iron now.
5. **Not covered.** Playing it through by hand with a mouse and keyboard; co-op over two machines; the packaged
   build (not rebuilt in this pass). In the hold, the test soldier stood still at the manifold and was downed by the
   reserve wave, which is expected for a soldier who doesn't fight back.

## Media

`docs/media/brasshaven/`: `brass_street.png` and `.gif`, `brass_manifold.png`, `brass_valve_hud.png`,
`brass_wound_down.png`, `brass_relight_hold.png`, `brass_airship.png`, `brass_foundry.png`, `brass_after_action.png`.
