# Test report: things to do on every map

**Slice:** the worlds were too open-ended. Each operation had one objective and a lot of empty ground. This pass
gives every operation map things worth going out of your way for, a reason to replay it, and something to spend what
you earn on, without microtransactions and without retention tricks.

## What the reference repos taught, and where it went

| Repo | Principle taken | Where it is |
|---|---|---|
| Azgaar/Fantasy-Map-Generator | A strict pipeline: settings, then generators, then world data, then a renderer, each step replaceable | `DVWorldGen` is written in exactly those four steps |
| redblobgames/mapgen4 | Poisson-disk points, not a grid or pure randomness, so features feel scattered but evenly spaced | Bridson sampling over the arena oval |
| mxgmn/WaveFunctionCollapse | Placement as constraint solving: a candidate is kept only if it satisfies every local rule | Each sample must be flat, on the navmesh, reachable from the landing zone, and clear of objectives, enemies and vehicles |
| BenPyton/ProceduralDungeon | Generate at runtime in UE5 from rules, deterministically from a seed | Seeded by the map's name, so co-op players and replays get the same layout |
| princeton-vl/infinigen, Terrain3D, godot_voxel, 3DWorld, THREE.Terrain, cesium-unreal | Terrain and photoreal generation at a scale beyond this project's kit-bash art | Noted for the art pass; not used here |

## Implemented, partial, planned

| Status | What |
|---|---|
| **Implemented** | `DVWorldGen`: points of interest generated when an operation starts. Eight per map: three intel caches spread by farthest-point selection, a survivor as far from the landing zone as possible, two supply caches about 45 m out, two wrecks. A cramped map (the harbour) gets up to two tighter passes |
| **Implemented** | `ADVPointOfInterest`: intel (walk over it), a survivor in a crashed pod (stay beside it for 4 s), supply caches (full ammo, health and armour), and wrecks (requisition). Markers appear within 45 m, so you find them by exploring. A line under the objective shows what's left |
| **Implemented** | Challenges per operation: recover all intel, get the survivor out, bring everyone home, win in under nine minutes, and, on restrained worlds, hurt no civilians. The first completion pays XP and requisition; repeating one pays a fifth, so a replay is worth something without becoming a grind. The after-action report lists them |
| **Implemented** | Requisition: earned only in play (operations won, challenges, intel, survivors, wrecks). There is no real-money anything, by design |
| **Implemented** | The quartermaster on the Kestrel: eight paint schemes, each named after a world (Kestrel Blue, Ironstride Rust, Guild Brass, Hollyhock Sage, Lantern Rose, Verge Neon, Europa Ice, Memorial Brass), 250 to 600 requisition. Z puts the next one on the counter, Y buys it and you wear it at once; the locker only offers schemes you own |
| **Implemented** | PLAY buttons: every playable operation on the galaxy map has one, with how many of its challenges you've met. The website's world cards and the README have one for every world |
| Healthy by design | No daily logins, streaks, timers or loot boxes. Rewards come from finishing things, and everything that can be earned can be earned in a few evenings |
| Partial | Points of interest are reachable on foot but not hand-composed; the survivor waits at the pod rather than walking to the LZ |
| Planned | More kinds (data terminals to hack, hostages under guard, sniper nests), weapon and optics cosmetics at the quartermaster, a challenges board on the Kestrel |

## Test runs (observed)

Platform: Windows 11, Unreal Engine 5.8, Development editor, off-screen renderer at 1280×720.

1. **An editor-time placement script, abandoned.** Line traces in the editor missed the generated terrain on most maps
   (0 hits outside Red Tide and Neon Verge), so placement moved into the game, where collision and the navmesh are
   both real. The two maps the script had touched were cleared.
2. **Placement on every map** (runtime, logged): Outpost, Europa, Io, Proxima, Ceres, Gaia, Ironstride, Brasshaven,
   Hollyhock and Neon Verge placed **8**. Breakwater placed **0** on the first pass, so tighter fallback passes were
   added; it now places **5**.
3. **Points and challenges in Red Tide.** `DVPointsTest` walked to all eight in turn: three intel caches ("Intel
   recovered. That's 1 of 3"), the survivor, both caches and both wrecks, each paying requisition. `DVEndMission 1`:
   the after-action report showed all four challenges met with their requisition, plus "+60 Red Tide complete".
4. **The quartermaster.** At the counter with 750 requisition: Z showed Kestrel Blue (250); Y bought it ("Kestrel
   Blue is yours"), leaving 500, and the counter moved on to Ironstride Rust.
5. **Your saves.** These tests write to the commander profile, so your saves were backed up first and restored
   afterwards (checksums match). Earlier test runs, before this pass, had added some XP to the commander profile
   without restoring it.
6. **Not covered.** Playing by hand; co-op; the packaged build.
