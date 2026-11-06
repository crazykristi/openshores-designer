# Building todo list

Agreed with the user on 2026-10-31. Items are in priority order; tick them off
as they are done.

**Every type comes in two variants:** **minimal** (lowest cost) and
**expanded** (bigger).

* **Minimal** (the user, 2026-10-31): one of each room the type needs, for
  example 1 workshop, 1 medium home and 1 office.
* **Expanded** (the user, 2026-10-31): many of each room, for example 20
  workshops and 50 to 200 large homes. This is the scale of a building
  complex, so the face budget and the engine's size limits will need
  checking.
* **One building or several** (the user): be flexible. In general one
  building, unless several fit the setting better.
* **Style** (the user): Aether (high-tech and magic) mixed with high-tech
  and cyberpunk.

### 0. Groundwork
- [x] How teleporter pads are associated with a teleporter station: the console's pad locations and pad effect number, and pad meshes drawn with that effect (`docs/SOH_FORMAT.md`, "Transporter pads"; example `exported/buildngs/airport (teleporter).SoH`).
- [ ] Give the generator's transporter stations pads (ships and buildings).
- [x] The list of power plant types (from the user, 2026-10-31; see section 1). What each needs, from the engine source: a generator, an office, and the generator module for its fuel, carried in the blueprint (2026-11-01). Full battery storage takes 3,900 to 10,300 m³.
- [x] Reusable building blocks: halls, towers, domes, chimneys, tanks, gears, fields of light (2026-11-01).
- [ ] Find the room counts for each type's expanded variant (the scale is settled above), and check the face budget and size limits at that scale.
- [x] A `test.sh` check that each type has its required parts: the kit check fails on the engine's "cannot be used for its intended purpose" (2026-11-01; against the stand-in industry table).

**Minimal versions of sections 1 to 6 exist as private designs (2026-11-01)**,
kept outside this repository. A ticked item below has one; the expanded
variants are still to do. Not yet seen in the game.

### 1. Power plants (top priority)
One building per type, each minimal and expanded:
- [x] Hydro-electric
- [x] Coal
- [x] Wind
- [x] Solar
- [x] Hydrogen (fusion)
- [x] Nuclear fission
- [x] Natural gas
- [x] Gasoline

### 2. Resources and basic industry
- [x] Farm
- [x] Orchard
- [x] Mine
- [x] Well
- [x] Smelter
- [x] Refinery

### 3. Housing and civic
- [x] House
- [x] Apartments
- [x] Capitol (the Aether Spire)
- [x] Police
- [x] Fire station
- [x] Hospital
- [x] University
- [x] Bank
- [x] Mass media
- [x] Street light

### 4. Commerce and social
- [x] Cantina
- [x] Church (possibly from the user's 3D models)
- [x] Grocery
- [x] Retailer
- [x] Broker
- [x] Design studio

### 5. Factories
- [x] Textile mill
- [x] Clothing factory
- [x] Explosives factory
- [x] Plastics factory
- [x] Electronics factory
- [x] Computer factory
- [x] Pharmaceutical lab
- [x] Munitions plant
- [x] Weapons factory
- [x] Armorer
- [x] Machine shop
- [x] Vehicle factory
- [x] Aircraft factory
- [x] Spacecraft factory

### 6. Airports and space
- [x] Airport, minimal, without teleporter
- [x] Airport, minimal, with teleporter (the station; the pads are still to do)
- [ ] Airport, expanded, without teleporter
- [ ] Airport, expanded, with teleporter
- [x] Airport radar
- [x] Airport repair shop
- [ ] Space station (a different structure; not started)

### 7. Ships (after the buildings)
All with the P wormhole drive unless noted. Combat ships may take ideas from
science fiction series (The Expanse, Babylon 5, Star Wars) but must be
clearly different designs.
- [ ] A basic early game ship with a rocket drive
- [ ] An early wormhole explorer (P wormhole drive, no shields)
- [ ] An early combat ship with a ballistic weapon bay, no shields or teleporters
- [ ] Combat ships in various layouts and sizes

Settled for ships: decorations may stay hull parts (the user: more volume
means more capabilities).

### Postponed
- The Windows build (no Windows machine for now).

### 3D models (agreed 2026-10-31)
Thinned to a few hundred faces with `shipgen reduce`, on a pedestal or as a
centrepiece:
- [ ] FFXIV Aetherite crystal: the Capitol's centrepiece (Aether Spire top or forecourt), or the core of the fusion plant
- [ ] Arc Angel Mithra: statue in the church
- [ ] Y'shtola: statue at the hospital entrance
- [ ] Urianger: statue in the university courtyard (or the orrery observatory)
- [ ] G'raha: statue in a university library, or at the Capitol
- [ ] Moogle warrior: mascot at the cantina, or a sign at the grocery or retailer

### Generator windows (agreed 2026-10-31)
- [x] Windows wherever a room's outer wall is against the hull skin: the
  bridge sees out from as many angles as possible; crew quarters, lounges
  and medical get one every 10 to 15 ft; none in engine rooms, the reactor,
  stores or weapon rooms. Done 2026-10-31 (`PlaceWindows` in
  `src/generate.cpp`): a band on the bridge (4 ft wide every 5.5 ft, the
  skin up to 10 ft away), one every 12 ft in crew quarters, barracks, the
  galley, halls and the sick bay (skin up to 7 ft away), the tunnel to the
  skin lined with the frame. Not yet seen in the game.
- [ ] On big round hulls (the kit ships) most rooms are too far inside the
  hull for windows: the Brass Cruiser's bridge is 11 to 20 ft from the
  skin. Putting the bridge against the skin needs a change to the layout.

### Designer
- [x] Start a plan from a 3D model (`shipgen plan-from-model`, *File > New
  plan from a 3D model*), 2026-10-31.
- [ ] Show the model faintly over the grid, to see where the fit is off.
- [x] Bay doors (neighbouring door edges make one wide door) and vehicle
  parking spots, 2026-10-31.
- [x] How the window tool failed for the user: the windows were on diagonal
  edges, which the tool did not take.
- [x] Windows on diagonal edges (the user, 2026-10-31): done; the hull and room are turned to the wall for the cut, so the window gets the same clean frame.
- [x] Metric units in the designer as well as feet (the user): *View > Metric units*.
- [x] Stairwells between decks, as well as ladders (the user): done
  2026-10-31, *Place stairs* in the designer: steps, a sloped Barrier, the
  opening with a void, a walk path up and a rail round the opening (as the
  user's aircraft factory, plus the walk path). Not yet tried in the game.
- [ ] Multi-cell hull doors (the user). Bay doors from neighbouring door
  edges are done; check this is what was meant.
- [ ] Deployable ramps at hull doors (the user: "would be nice, not sure if
  possible"). Possible: a door keeps a mesh for each state (`AuScStatePart`:
  Closed, Open10 to Open90, Open), so its open state can be the ramp lowered
  to the ground, with the steps in between as the animation, and a sloped
  Barrier and a walk path down it tied to the open state. A fixed ramp (a
  sloped barrier from the doorway to the ground, as at the factory's door)
  is the simpler first step.
  - [x] Fixed ramps at hull doors, in the designer and the generator, done
    2026-10-31 (`AddDoorRamp`); doors over 15 ft up keep the ladder. Not
    yet tried in the game.
  - [x] A way down per hull door, and tied to the door's state, done
    2026-11-02 (`exit`, `exitOpenOnly`, `rampRun`); not yet tried in the game.
  - [ ] The deployable version that lowers into sight, if the tied ramp works: the
    door's state meshes for what is seen. Doors do not count for the hull's
    extents, so the ramp's foot needs a small hull part as well.
  - [ ] Whether ground vehicles can drive up a ramp.
- [x] Windows at an adjustable height (the user: the windows suit tall
  characters): `windowSill`, `windowTop`, 2026-10-31.
- [x] Windows side by side join into one (the user: an option),
  `joinWindows`, 2026-10-31; straight runs only, not round a corner.
- [x] Bay door switches in reach (the user could not open the bay door),
  2026-10-31.
- [x] Chairs face the front; the Wedge's bridge has front windows; floors,
  walls and ceilings differ and walls are tinted per room; doors painted
  to stand out (the user's Wedge v6 test), 2026-10-31.
- [x] A hull option where the hull between decks slopes from one deck's
  outline to the next instead of stepping (the user, 2026-10-31: sleeker,
  fewer faces; the drawn hull is the minimum): `slopedDecks`, 2026-10-31.
- [x] Progress and a stop button when reading a big 3D model, and quicker
  thinning (86 s to 9 s for half a million faces), 2026-10-31.
- [ ] Rooms of more than one deck, and hull doors over more than one deck
  (the user: "would be awesome").
- [x] The 3D model importer should thin and repair the model itself, as
  `shipgen reduce` does (the user): done, above 20,000 faces; it also
  copes with models of several overlapping pieces.

### Open questions to settle as we go
