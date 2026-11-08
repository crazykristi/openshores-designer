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
- [x] Give the generator's transporter stations pads (ships and buildings). Ships done 2026-11-07; buildings 2026-11-07 (the transporter goes in a transporter or teleporter room when there is one, and gets its pads there).
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
- [x] Airport, minimal, with teleporter (the station and its pads)
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
  - [x] A way down per hull door, done 2026-11-02 (`exit`, `rampRun`).
    Tying it to the door's state (`exitOpenOnly`) did nothing in the game and
    was retired on 2026-11-06.
  - [ ] The deployable version that lowers into sight, if the tied ramp works: the
    door's state meshes for what is seen. Doors do not count for the hull's
    extents, so the ramp's foot needs a small hull part as well.
  - [x] Whether ground vehicles can drive up a ramp: no (the user's game
    test, 2026-11-06). Vehicles change decks by turbolift instead (track 1a).
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

### Parallel tracks (from the user, 2026-11-06)
Separate tracks; each can be picked up on its own. The research, the design
and the order are in `TRACKS_PLAN.md`.
- [ ] **1. How the game's own designer places these** (from the original
  game's source code):
  - [x] Research done (2026-11-06, `TRACKS_PLAN.md`).
  - [ ] 1a. Turbolifts.
    - [x] Done 2026-11-07: a *Turbo Lift* room drawn on several decks is one
      car (`AuScLift`) with a stop and a lift door per deck with a door; car
      height for vehicles; wide doors. Not yet tried in the game.
    - [ ] A stop at ground level below the hull (vehicles drive straight in).
    - [ ] A door taller than one deck, into an Open to Deck Below room.
    - [ ] **Vehicle lifts** (the user, 2026-11-06): vehicles cannot drive up
      ramps, so they change decks by lift. A bigger car, a height of its own
      (taller than a deck where needed), a wide door per stop, the car floor
      level with each deck, optionally a bottom door straight onto the
      ground. The user confirmed in the game: vehicles ride along if they
      fit, no limit on door width, ground-level lift doors and tall cars work.
  - [x] 1b. The empire flag. Done 2026-11-07: flag or name on a hull
    triangle, upright on the nearest wall; not yet tried in the game.
  - [x] 1c. Landing spots for spacecraft and starships (as at an airport).
    Done 2026-11-07: a tool on the Rooms page, kind `spacecraft` in a plan.
- [ ] **2. Weapon bay flow.** In the game a weapon bay always has a bay type
  and a fire control station, and may have a weapon emitter. We already have
  these types; let the user pick the bay type when placing an emitter. Support
  several weapon bays per ship.
  - [x] Done 2026-11-07: a bay type per weapon emitter, a fire control station
    per emitter, right click to change; not yet tried in the game.
- [ ] **3. Hull voids still trap people in front of the door.** A design
  where nobody could get in during the game: the user's arness-mcrn ship
  (private, outside this repository). Check whether hull voids are switched
  off when a ramp is closed or open.
  - [x] Ramp foot ledge, ladders at hull doors that could not be grabbed, the
    inside ladder that threw the player off; "only while open" retired
    (2026-11-06, not committed).
  - [ ] The user's game test of these fixes (`door_exit_test3.SoH`,
    `arness_plain_ramps2.SoH`): ladders A and B, the inside ladder.
  - [ ] The arness-mcrn ship: 52 of 240 crew routes cannot be followed (its
    vehicle bays and zero-G shafts; there before these fixes).
  - [ ] A `door-check` walk from the ground in through each door in `test.sh`.
- [ ] **4. A second window style: one unified window** where possible (the
  old style stays; windows can look garbled now). Needs windows that can be
  dragged out, as doors are. On a model hull, the glass follows the model's
  own surface and edges, so it blends in (the user, 2026-11-06).
  - [x] Dragged window runs: one cut and one pane per run. Done 2026-11-07
    (windows share a `row`; a row joins as `joinWindows` joins).
  - [x] Windows on a model hull: the model's own triangles become the
    glass (cut to the window's outline), no frame outside. Done 2026-11-07
    (`windowStyle` "hull", `HullWindow` in planbuild.cpp).
  - [ ] Keep the model's detail round windows when it is thinned.
  - [x] Ceiling windows (skylights) too: a choice per window between following
    the hull's shape and a flat cutout (the user, 2026-11-07). Done
    2026-11-07 (`shape`, `HullSkylight`).
  - [x] Ceiling windows placed per triangle, not per whole cell (the user,
    2026-11-07). Done 2026-11-07 (`k`; Shift+click for the whole cell).
- [ ] **5. Hull geometry clips through doors.** The door is its own geometry
  and takes precedence: cut the hull away inside the doorway.
  - [x] 2026-11-07: window frames (and their tunnel linings) are cut away in
    interior doorways; found in the arness (two doors on deck 10).
  - [ ] Ask the user which door they saw it at, in case it was something else.
- [x] **6. Hull-wide default textures** for walls, doors, ceilings and floors,
  so they do not have to be picked again for each new room. Done 2026-11-07.
- [x] **7. Liveries** (working name; also possible: markings, insignia). A
  painted pattern must stay clearly visible: let the user mark a shape as a
  livery so the designer keeps as much of its geometry as it can (no thinning
  or merging across it). Done 2026-11-07: a livery colour (`livery` on a
  palette colour); on model and angular hulls the hull is cut along its
  outline, and its edges are held still in thinning (`CutLiveries`).
  - [ ] Ask the user which painted design looked wrong, and how.
- [ ] **8. Balconies:** a wall a third of the room's height between rooms.
  - [x] Done 2026-11-07 (the door tool's *balcony*); not yet tried in the game:
    can people climb over the parapet?
- [ ] **9. Windows between rooms**, if the engine supports them.
  - [x] Done 2026-11-07 (the door tool's *window between rooms*); not yet tried
    in the game: does the game draw the other room through the pane?
- [ ] **10. Audit the room types:** which ones actually do something in the
  game. Rooms and room voids can carry properties; the list has grown very
  long. Found: every room is a cargo hold in the game now. Keep at least
  (the user): Hold, Bridge, Open to deck below, Medical bay (surgery
  station), Teleporter room (control station and pads, each pad at least
  0.5 m from the others).
  - [x] Each room gets its type's hold access; sick bays their medic,
    transporter rooms their transporter and pads; the designer's list grouped
    with tips (2026-11-07).
  - [ ] Who may enter (personnel access per room): to ask the user.
  - [ ] Buildings: read `DaBd::InsideRoomType` before changing anything.
- [ ] **11. Door buttons** (the user, 2026-11-06): after dragging out a
  door, place a button on a wall nearby that opens and closes it, raised a
  little from the wall in another colour; Esc skips it. Possible: each door
  has two switch locations the game lets us place.
  - [x] Done 2026-11-07: the designer step after a door, the button mesh, the
    switches on it, manual doors; not yet tried in the game.
- [x] **12. The flaky `test.sh` check** "room and door textures" (fails
  about one run in two, since before 2026-11-06). Fixed 2026-11-07
  (`grep -q` under `pipefail`).

### Open questions to settle as we go
