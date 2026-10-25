# Test report: Neon Verge, Operation Clean Signal

**Slice:** the third of the Act IV "cure" worlds. Neon Verge is an orbital megacity over a black lake, where data
brokers sell CINDER's secrets to anyone. Juno Kade holds the one clean sample of the Static and wants a favour for it.
Delta clears Halcyon's rented war-frames off the broker district, hacks three relay masts while response teams drop in,
then runs down the courier who bolts with the sample before he reaches the maglev.

**References** (one principle each, nothing copied):
- **The chase: the classic "catch the runner" objective** from action games. Our rule is about speed: he's faster than
  a walk and slower than a sprint, so you have to commit to running him down, and you can't shoot him because the sample
  is on him.
- **The hack: hold-the-point uploads under pressure.** Each relay calls a response team the moment you start.
- **Look: the art-direction table's "blue-and-orange cyberpunk megacity; neon spires on a reflective lake under a
  ringed planet"**, built from the project's kit-bash geometry, emissive bands and the harbour's water material.

## Implemented, partial, planned

| Status | What |
|---|---|
| **Implemented** | The map: a wet city deck on a black lake with a lit sea wall; six broker blocks with neon roof edges, ramps, skybridges and holo signs; street lights and kiosks; an elevated maglev line with a platform and a waiting car; thirty megaspires standing in the lake; a ringed planet overhead |
| **Implemented** | Relay masts (`ADVRepairPoint` with `bTerminal`): stand at the terminal for 7 s to upload; the screen and beacon go from cold blue and red to the brokers' orange. Starting each hack sends a two-frame response team toward it |
| **Implemented** | The courier (`ADVCourier`): runs a fixed route of six waypoints through the streets to the maglev at 590 (walk 420, sprint 680). Get within 2.4 m to catch him; if he reaches the platform, the sample is gone and the operation fails. Rounds pass through him |
| **Implemented** | The Verge Brokers as the sixth faction (+15 when the sample changes hands, -10 if it's lost); Luyten's Star on the war table with Operation Clean Signal; lethal rules (the Kestrel's fire support is available) |
| Partial | The roofs, ramps and skybridges are scenery: the navmesh doesn't connect the ramps to the roofs, so the relays were moved down to the street (see run 3). The courier uses the mannequin body in a broker's jacket |
| Planned | Grav-bikes and armoured transports; rescues from body-mod clinics; faction politics between brokers; climbing the towers |

## Test runs (observed)

Platform: Windows 11, Unreal Engine 5.8, Development editor, off-screen renderer at 1280×720. Driven by
`DVClearHostiles`, `DVRepairTest` (which now paths on the navmesh, the way a player finds the way) and `DVChaseTest`
(a real sprint after the courier).

1. **Look pass.** The first captures were blown out: exposure too high, neon at 45 and holo signs at 5, and a roof
   "rim" that was a full glowing slab. Exposure is now -0.8, neon 14, holo 2, the rims are four thin strips, and the
   spires stand further out.
2. **Map build.** 1,447 actors; the galaxy map rebuilt with Luyten's Star (27 actors).
3. **Relays on the roofs: failed.** The test soldier pathed up a ramp and stopped against the block wall: the navmesh
   doesn't connect ramp to roof. Turning the ramp the other way, opening the roof edge and overlapping the ramp onto the
   roof didn't fix it, so the masts moved to the street at the foot of three blocks.
4. **The whole operation.** With the relays in the street: the frames cleared, then **all three relays uploaded in
   37.4 s**, with Halcyon's response teams arriving at each one (the soldier's health fell to 25 before the test cleared
   them). The courier bolted and **`DVChaseTest` caught him**; the stage moved to extraction with Juno's and Captain
   Vance's lines.
5. **Not covered.** Playing by hand; the courier escaping (the code path exists but wasn't run); co-op; the packaged
   build.

## Media

`docs/media/neonverge/`: `neon_street.png`, `neon_lake.png`, `neon_district.png`, `neon_relay.png`, `neon_maglev.png`,
`neon_caught.png`, `neon_chase.gif`.
