# Test report: The Kestrel remembers

**Slice:** the TAS Kestrel's hangar deck changes with what happened on your operations, and the change survives quitting and reloading.

**Reference stack** (from the inspiration atlas, Delta-V route):
- **Gameplay reference: Red Dead Redemption 2's camp.** A home base that reacts to what you did. We borrow only the principle that the hub's people and objects show the consequences of your missions. Our version is a military carrier deck with soldiers, a medbay and a memorial. We don't model camp chores, upkeep or companions.
- **Visual reference: Cyberpunk 2077's industrial props.** A small set of props that clearly belong together. Every added object is built from the project's own kit materials (gunmetal, navy, rust, glow strips). No outside assets are used.

## What changes on the deck

Everything below is rebuilt from saved data by `ADVKestrelMemory`.

| Event in an operation | What you see aboard the Kestrel |
|---|---|
| A soldier is killed in action | A name plate on the memorial wall and a candle at its foot. Their locker is sealed with a red seal and has a folded flag on top. The operations board gets a white mark under that op. |
| A soldier is badly hurt (50% or more of their health lost) | They lie in a medbay bed. Walk up and they tell you how many missions until they're back. Doc Pell's line changes with the number of patients. |
| The operation failed, or cost lives | A repair scaffold with a welder's flicker goes up around the Raven on pad one. Deck crew name the operation that damaged it. |
| A Bulwark war-frame is destroyed | Its head goes on the trophy shelf between the armoury and the lockers. |
| Operation Breakwater is won | Port Halden families are bedded down on the deck, with their own lines. Winning Proxima puts New Albion colonists there instead. |
| Any operation ends | A green (won) or red (lost) light is added to the operations board. |
| You come aboard | The crew's radio greeting names the last operation and its losses. |

## Test runs

Platform: Windows 11, Unreal Engine 5.8, Development editor build, off-screen renderer at 1280×720. The base revision was 0d7e580. This work is committed on top of it.

1. **Operation with consequences.**
   - Setup: a new campaign, then Breakwater with dev commands to cause the events: `DVCasualty 0` (one squadmate KIA), `DVCasualty 1` (one badly hurt), `DVCasualty 2` (Bulwark destroyed). The mission was ended as a win, then flown home with K.
   - Log observed: `KIA: Private First Class Dmitri "Crow" Jaramillo`. The streamed hangar then reported `DVKestrelMemory: 1 ops, 1 fallen, 1 wounded, 1 trophies`, and the Raven landed with 2 squadmates.
   - Save files observed: `Campaign0.sav` and the new `Campaign0_OpLog.sav`.
2. **Reload.**
   - Setup: new processes launched straight into `/Game/Maps/Kestrel` (no mission played), eight separate launches.
   - Every launch logged `1 ops, 1 fallen, 1 wounded, 1 trophies`.
   - Observed on screen:
     - the name plate on the memorial;
     - the sealed locker with its red seal;
     - the patient lying in bed one, and Doc Pell;
     - the scaffold and crew around the pad-one Raven;
     - one green light on the operations board;
     - the Bulwark head on the trophy shelf;
     - four Port Halden evacuees;
     - Halloran's greeting: "We got it done on Breakwater. 1 of ours didn't come home…"
   - Screenshots are in `Docs/media/kestrel_remembers_*.png`.
3. **Packaged build.**
   - The game was packaged after this change (550 MB zip).
   - In the packaged game, Breakwater with `DVCasualty 0` and `DVCasualty 1`, a win, then K to fly home. The streamed hangar logged `1 ops, 1 fallen, 1 wounded, 0 trophies`, and the Raven landed with 2 squadmates.
   - On screen aboard: the KIA marker in the squad list, the repair scaffold, and Halloran's greeting naming Breakwater and the loss.
   - Unreal logged a harmless material warning ("Had to pass SMU back to game thread … usage flag Nanite") for the runtime glow parts.
   - The test saves were then deleted from the staged folder.

My own campaign test saves were removed afterwards, and your original saves were restored.

## Implemented, partial, planned

- **Implemented:**
  - operations log (its own save slot, cleared by a new campaign);
  - memorial plates, candles, sealed lockers;
  - medbay beds and lying patients with their own lines, and the doctor;
  - repair scaffold and deck crew after a costly or failed op;
  - Bulwark trophy shelf;
  - evacuees after Breakwater or Proxima;
  - operations board;
  - crew greeting based on the last op;
  - the dev command `DVCasualty 0|1|2`.
- **Partial:**
  - In co-op, only the host sees the memorial plates, lockers, board and shelf. The roster lives on the host, and those props are local. People (patients, crew, evacuees) are spawned by the server and replicate.
  - Plates hold a name line but no readable text. The memorial screen at the war table lists the names.
  - Patients use the idle animation lying down. It reads as breathing but isn't a dedicated pose.
- **Planned:**
  - a huge moving machine (a walker) to board or extract around;
  - funerals or visits that change crew morale;
  - repairs that finish after the next operation;
  - names engraved as real text.

## Known issues

- The hangar's random crates now keep clear of the memorial, board, medbay and shelf. If you have an older copy of the Kestrel map, rebuild it with `Scripts/build_kestrel_hub.py`.
- After the flight home, the objective panel still shows the old mission's enemy counter.

## Asset manifest

All new geometry is made at runtime from the project's own kit meshes and materials (`DVKit` box, cylinder and sphere; Gunmetal, Navy, Olive, Rust, Concrete and Glow). There are no new imported assets. People use the UE5 template mannequins that were already in the project.
