# Test report: launching from the Kestrel, and dropping into an operation

**Slice:** the other half of "no loading screen". Flying home already took a Raven from a battlefield into the
Kestrel's hangar. Now the hangar's war table takes you back out: a Raven lifts off its pad, slides out of the open
hangar mouth over the launch lip, burns away from the carrier toward Mars, and makes the Lattice jump. The galaxy map
opens out of the jump's white-out, pulling back from Sol.

## Implemented, partial, planned

| Status | What |
|---|---|
| **Implemented** | `ADVDeparture`: a server-side director, the reverse of `ADVAscent`. Lift (3 s), taxi out of the mouth (5 s), burn toward Mars in jet mode (7.5 s). The view follows where the Raven is flying; Ruiz and Vance call it on the radio |
| **Implemented** | The war table in the hangar boards you in the nearest free Raven (`ADVGameMode::BeginDeparture`). Elsewhere (a hangar streamed into a planet's Kestrel) it still opens the galaxy map directly. You can't climb out mid-flight |
| **Implemented** | The Lattice jump: the HUD washes to white over the last 1.3 s (`ADVGameState::JumpTime`, replicated), then the galaxy map loads with the `launch` option. It opens close on Sol inside the same white, which fades over 2.5 s while the camera pulls back to the local bubble |
| **Implemented** | Co-op aware: in a listen server the whole squad travels together (`ServerTravel`); standalone uses `OpenLevel` |
| Partial | The galaxy map load is a level change hidden by the white-out, not a streamed transition. There's no exterior model of the Kestrel to see behind you, because the camera faces forward the whole way |
| **Implemented** | The descent (`ADVDescent`): launching an operation from the galaxy map washes white and opens the map with the `descent` option. The commander starts aboard the LZ's Raven 90 m up; it drops in (11 s), flares over the pad (3.5 s) and settles (2.5 s) while the briefing plays. The squad climbs out, and the mission clock and call-in timers start at touchdown |

## Test runs (observed)

Platform: Windows 11, Unreal Engine 5.8, Development editor, off-screen renderer at 1280×720.

1. **Kestrel to galaxy map.** `DVLaunch` in the hangar: the commander boarded a Raven, which lifted off, turned for the
   mouth, flew out over the launch lip and burned toward Mars. The log then shows `LoadMap(/Game/Maps/Galaxy)`
   completing in 0.12 s.
2. **Bug found and fixed.** With a 900 m burn, the Raven out-ran the chase camera and shrank to a dot. The burn is now
   320 m on an ease-in, and the Raven stays framed all the way to the jump (`launch_departure.gif`).
3. **Arrival.** Opening `Galaxy?launch`: the white fades out over 2.5 s while the camera pulls back from Sol to the
   local bubble with the Sol panel open (`launch_arrival.gif`).
4. **The descent.** `Outpost?descent`: out of the white at 90 m, down over Tharsis Ridge, flare, touchdown; the log reads
   `DVDescent: landed at the LZ, 3 squadmates out`, and the title card and objective appear on landing (`descent.gif`).
5. **The whole chain.** On the galaxy map, `DVSelectSystem Sol` and `DVLaunchOperation 2` (Red Tide): the white-out, then
   `LoadMap: /Game/Maps/Outpost?descent` in 0.56 s, then the descent and landing as in run 4.
6. **Not covered.** Two players in co-op; the packaged build; maps without a Raven on the LZ (Boot Camp) start on the
   ground as before, by design.
