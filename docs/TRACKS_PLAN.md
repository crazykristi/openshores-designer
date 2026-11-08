# Plan for the ten parallel tracks

Research and design for the tracks agreed with the user on 2026-11-06 (the
list is in `BUILDING_TODO.md`, "Parallel tracks"). Written 2026-11-06 from
the game's source (the OpenShores clone, `src/lib/data/designs/` unless
another path is given) and from probes of the user's designs. Nothing here is
built yet.

Each track has: **what the game does** (with where in the source), **the
design** for the designer and the generator, **open questions**, and a rough
**size** (S: an evening, M: a few sessions, L: a week or more).

---

## Order

1. **Track 3, hull voids at ramps** (a bug that keeps people out of a ship).
   Fixed, waiting for the user's next game test (below).
2. **Track 10, room types**, because it changes what every room *is* in the
   game, and the weapon bay flow (2), balconies (8) and the room list depend
   on it.
3. **Track 6, ship-wide textures** (small, independent; quick relief).
4. **Track 2, weapon bays.**
5. **Tracks 8 and 9 together, openings between rooms** (one mechanism).
6. **Track 4, unified windows** (needs the dragging that 8/9 also use).
7. **Track 5, hull through doors**, then **track 11, door buttons** (both
   about doors).
8. **Track 7, liveries.**
9. **Track 1, turbolifts, flag and landing spots** (the research is done; the
   flag and landing spots are small, the turbolift is the biggest piece).

---

## Track 1. How the game's designer places these

### 1a. Turbolifts

**What the game does.** A turbolift is one `AuScLift` part
(`auscparts.h:4916`), a state part with four meshes: `Car` (the room mesh),
`RoomVoid` (the void inside the car), `DoorJig` and `CarOriginal` (the last two
are only used while designing; `AuScLift::Finalize` removes them).
`AuScParts::MakeTurboLift` (`auscparts.cpp:3535`) builds it from a selected
room, room void and door jig, and takes the gravity from the room void.

* **Stops** are named positions of the car: `m_Stops` maps a name to a
  matrix, relative to the car's drawn position (`AddStop` records the identity;
  `SetStop` + `TransformLiftStop` moves the current stop). At most 63 stops.
* **Each stop needs a door** whose *stop name* is the stop's name. The game's
  designer names a door after the stop the car is at when the door is cut
  through it (`auscparts.cpp:2113`, `SetStopName`).
* **Each stop's door needs a walk path** tied to the door's Open state, with
  one end inside the car's room void at that stop. `Finalize` uses these to
  build the path segments between all the stops (`AuScPathSeg::m_LiftId`,
  `m_Stop1/2`), so the crew's routing goes through the lift.
* The report (`auscreport.cpp:1455-1580`) fails a lift with no stops, a stop
  with no door, and a lift door with no lift stop of its name.
* A lift door may be a hull door (`DOOR_HullLift`), for example a lift down to
  the ground.

**Design.** *Place turbolift* in the designer: pick a 1x1 or 2x2 cell shaft
and the decks it stops at; on each deck, which side the door is on. The build
makes the car at the lowest stop, one stop per deck (named after the deck's
room the door opens into, e.g. "Deck 3 Bridge"), and cuts each door with the
car moved to that stop. Then the walk path for each door. The shaft is drawn
as a room column, the way stairs are now. Turbolifts can stand in for ladders
in the crew routes, and `crew-check` should follow them.

**Vehicle lifts (the user, 2026-11-06).** In the game test of
`door_exit_test3.SoH`, vehicles could not drive up any ramp, so a lift is the
way to move vehicles between decks. The engine allows it:

* A lift carries everything whose room is the car (`DaSc::LockLiftContents`:
  every object inside the ship whose `ShipRoomId` is the lift), vehicles too.
* `MakeTurboLift` sets no limit on the car's size or shape: any room mesh and
  room void, so a car taller than one deck is possible.
* Being in a vehicle bay only matters for fuel, repairs and loading ordnance
  (`DaVehicle`, `AuAtomInterface`). A vehicle can stand in any room.

Design: a *vehicle lift* variant with a bigger footprint (2x2 to 4x4 cells), a
car height of its own (taller than a deck where vehicles need it), a wide door
at each stop, and the car floor exactly level with each deck's floor (vehicles
are stopped by even a small ledge). The lowest stop can be a hull lift door at
ground level (`DOOR_HullLift`), so vehicles drive straight in from the ground
with no ramp. The shaft is clear over the car's whole travel plus its height.
**The user's answers (2026-11-06), so no test ship is needed first:** a
vehicle rides along as long as it fits in the car; a lift door has no width
limit; a hull lift door at ground level works; a car taller than the deck
spacing works, as long as the door, the room and the hull voids fit it.

**Open questions.** Whether the generator can use the engine's own
`JigCutParts` with the car at a stop, or must name the doors itself. What the
shaft around the car must be (hull void? room void per deck?): check against
a lift in one of the game's own ships before building.

**Answered (2026-11-07, from the Atlantis).** The Atlantis has one car (part
250, 8 x 8 x 10 ft, Car mesh of 14 faces and a box RoomVoid) with eleven stops
all over the ship, each a full matrix (turned as well as moved). There is
**no shaft**: no void or room round the car's travel. Each stop's door is an
ordinary door part with access `DOOR_Lift`, its stop's name, Closed and Open
states, a room void tied to Open (grouped with the room it opens into) and a
walk path tied to Open from inside the car to outside the door. Finalize made
55 path segments, one per pair of stops. We cut each door with our own jig
cut, the car mesh moved to the stop, and name the doors ourselves
(`SetStopName`). Built: see `DESIGNER.md`, *Turbolifts*.
**Size:** L.

### 1b. The empire flag

**What the game does.** `AuScEmpireFlag` (`auscparts.h:10966`) is a screen
(`AuScScreen`): one flat quad. The designer's command
(`lib/designer/commands/spacecraft/appearance/ddscempireflag.cpp:168`) calls
`MakeScreen(corner1, corner2, designPlane)`, then `FlattenVertices`. The game
draws the owner's flag on it (`OwnTexture`, `DaSc::AnimateEmpireFlagAndName`)
and keeps the flag's aspect ratio inside the quad. `AuScEmpireName` works the
same way for the empire's name. We already make screens the same way (desk
screens, `generate.cpp:2125`).

**Design.** A *Flag* tool on a hull triangle, or on a run of hull edges like a
window: a rectangle on the hull surface, a little outside it so it does not
fight with the hull faces (as decals are). Size and height like the window
sill and top. The empire's name as a second choice of the same tool.
**Size:** S.

### 1c. Landing spots for spacecraft

**What the game does.** `AuScSpacecraftParkingSpot` (`auscparts.h:13396`) is
an icon part: a location and a rotation on the design plane
(`ddscparkspacecraft.cpp:150`, `MakeIcon(loc, rot, plane)`). Its size is fixed:
a square 2 x 119 ft (`GroundSpawnSizeLimit`, `lib/util/audef.h:1427`), the
largest hull that can spawn on the ground. The report counts them
(`m_ParkSpacecraft`); industries can require one (`DbIndustry::ParkSpacecraft`,
for example airports). We already place them in buildings (`building.cpp:871`)
but not from the plan designer, which only has ground and space vehicle spots.

**Design.** Add *spacecraft* to the designer's parking spot tool (it has
ground and space vehicles now), with the 238 ft square drawn, so the user can
see it fits on a deck or pad. Warn when the square leaves the drawn hull or
overlaps another spot. For buildings, also a landing pad shape from the kit.
**Size:** S.

---

## Track 2. The weapon bay flow

**What the game does.** A weapon bay is not a room: it is **a part that runs a
bay module**, with **its own volume**.

* The part is a fire control station or console (`AuScStationFireControl`,
  `AuScConsoleFireControl`), or a turret. Each has `m_Module`, the commodity it
  runs: Ballistic, Energy, Launcher or Deployment weapon bay, a harvester bay
  (condenser, collector, prospector, siphon, wildcatter) or a tractor beam.
* The design keeps a volume per bay: `AuScDesign::m_Bays`, keyed by the part's
  id, in cubic metres (`auscdesign.h:57`, `SyncBaysAndFTLDrives`).
* The **weapon emitter** is a location on that same station (`m_EmitLoc`,
  `auscparts.h:6576`): where the shot leaves the hull.
* So **a ship can have several weapon bays**: one station per bay, each with
  its own module, volume and emitter. The report (`auscreport.cpp:898-990`)
  needs sensors if there is any bay, a capacitor big enough for each energy
  or harvester bay, and some volume for each bay.

We have one `fireControlModule` per plan; the emitters go to the stations in
order (`triplan.h:157`, `planbuild.cpp:5159`).

**Design.** A **weapon bay** becomes a plan object: `weaponBays: [{module,
volumeShare, emitter, station}]`.

* Placing a **weapon emitter** on the hull asks for the bay type (the
  module). It makes a new bay or adds the emitter to a bay that has none.
* Each bay gets a fire control station. By default the furnishing places it
  (as now, in the weapon room or the bridge); the user can also place it.
* The bays share the weapons volume (equally by default, or by a share per
  bay).
* Old plans: `fireControlModule` and the emitters become one bay per emitter
  with that module (one bay if there is no emitter).
* The designer lists the bays (type, volume, station, emitter) in the Systems
  panel, and the report shows each bay's shots for the capacitor.

**Open questions.** Where a bay without an emitter fires from (the station's
default `m_EmitLoc`?): to check in the game's weapon code. Whether the
emitter must be on the hull surface or can be anywhere. Turrets as a later
step.
**Size:** M.

---

## Track 3. Hull voids that trap people at the door

**The question: are hull voids switched by the door?** Yes. A hull void tied
to a door state is only there in that state; one with no tie is always there.
At a hull door we make: the doorway's hull void (tied to Open), the hull void
in front of the door (always), and for a ramp with **only while open**
(`exitOpenOnly`) a hull void tied to **Closed** that takes the ramp away while
the door is closed, plus a hull void over the slope (always).

**The arness-mcrn ship** (the user's design, outside the repository): every
door in its plan has `exitOpenOnly` set, interior doors too. The hull is the 3D
model itself (`asHull`); the four bay doors open under the belly (14 ft of
clearance; nothing in the way). Probing door 57 with the game's
`IsPathBlocked` (a probe in `~/coding/soh-probes/rampwalk.cpp`) shows three
problems:

1. **A phantom lip at the foot of the ramp, closed or open.** 1 to 1.5 ft
   beyond the foot there is something solid at z -0.2, about 0.6 ft above the
   ground, which no face accounts for. It is the hull's margin round the ramp
   (the ramp is a hull part): the voids end 0.5 ft beyond the foot, the margin
   reaches further.
2. **While the door is closed, there is no ramp**, so whoever walks up stands
   on the ground, 1.4 ft below the doorway, right where the ramp appears.
   **When they open the door, the ramp appears round their legs.** This is the
   likeliest cause of "stuck before the door".
3. **The setting sticks in the designer.** `planview.cpp:547` and `:878` give
   every new door the current "only while open" setting, interior doors too,
   so it spreads to the whole ship without the user noticing.

**Design of the fix.**

* Extend both ramp voids beyond the foot by the hull margin
  (`SHIPGEN_HULL_VOID_MARGIN`), not 0.5 ft (`interior.cpp:989` and the
  over-slope prism).
* Make "only while open" a choice per door again: off for each new door, shown
  in the door's settings, never on interior doors. A plan that has it on every
  door gets a warning.
* Keep "only while open" experimental, as the user's choice, and say in the
  designer that someone standing in front of the door is caught when it opens.
  The plain ramp (always there, a Barrier to walk on) stays the default.
* **`door-check` walks the way in**: from the ground to the threshold, closed
  and open, as `rampwalk` does, and reports any step higher than the game's
  step height (to look up in the walk code) and anything solid where someone
  stands to reach the switch. `test.sh` runs it on the kit and the door test.
* Rebuild the arness-mcrn plan without `exitOpenOnly` for the user to try.

**The user's game test of `door_exit_test.SoH` (2026-11-06), and what was done.**

* **Ramps C and D could be walked; D's stayed walkable with the door closed.**
  The hull void tied to Closed did nothing in the game. **"Only while open" is
  retired**: the checkbox is gone, the plan key `exitOpenOnly` is ignored when
  read and no longer written, and every ramp and ladder is always there.
* **A ledge at the foot of every ramp** (C, D, F, G, H), which also stopped
  vehicles. The visible ramp was a hull part 1 ft thick at the foot, and the
  game sets a landed ship on its lowest hull part, so the ground was 1 ft below
  the end of the slope. The visible ramp is now a wedge that comes to an edge
  at the foot, and the Barrier walked on goes on 1 ft into the ground (a
  Barrier does not count for setting the ship down). The voids reach
  `SHIPGEN_HULL_VOID_MARGIN` past the foot.
* **Ladders A and B could not be taken hold of.** The game builds each hull
  part's silhouette 2 ft bigger than the hull (`d_ObstructMargin`,
  `AuScHull::IsPathBlocked`), so nobody outside can come within 2 ft of the
  hull, and the ladder stands 0.75 ft out. A hull void now goes round the
  ladder from the ground up to the doorway (`AddOutsideLadderClearance`), and
  the ladder can be seen: rails and rungs just off the hull, a hull part (the
  game rejects a mesh outside the hull) that stops at the bottom of the hull,
  so the ship is not set down on it. `door-check` now also tries to take hold
  from the ground, as a player does (1 to 3 ft out, and to the sides, with the
  game's eye check): 5 of 5, where it was 0.
* **The inside ladder threw the user's view round and dropped them.** At the
  top it went on as a level ladder over the floor, so as not to let go over
  the hole. The game turns a climber's "up" along the way a ladder runs
  (`AuScPathLadder::FindLadder`), so on a level stretch the player was on
  their side. The game's own level ladders are crawlways that face up or down
  (Rocket, Atlantis); its deck-to-deck ladders (Scout, BasicDesign) are plain
  vertical ones. Ours are now vertical only, at the edge of the hole facing the
  floor beside it (0.45 ft from the edge, inside the 0.6 ft crew-check allows),
  so the climber steps forward off the top. Ladders stacked in one cell stand
  in the same place. Crew routes on the door test: 156 of 156.
* Still open: the arness-mcrn ship has 52 of 240 crew routes that cannot be
  followed (its vehicle bays and zero-G shafts). This was already so before
  these changes.
**Size:** S to M.

---

## Track 4. Unified windows

**What the game does.** A window is an `AuScWindow` part made from faces, with
an opacity (`MakeWindow`, `auscparts.cpp`); the report has no rules about where
windows go. The game's designer cuts the hole with a window jig
(`JigCutParts`), as we do (`interior.cpp`, `JigCut`).

**Why windows look garbled now.** Each window is cut per cell edge, and
`joinWindows` joins straight runs only; round a corner or on a smoothed hull,
the cuts meet at angles and leave slivers and steps in the frames.

**Design.** A second window style, **unified**: the user **drags a window**
along hull edges, as doors are dragged into rows. The run becomes one jig and
one cut, with one pane: flat if the run is straight; if it turns, a pane per
straight stretch joined by a mitre, inside one frame. The sill and top as
now. The old per-cell window stays.

* Plan: `windowRuns: [{deck, cells: [edge, ...], sill, top, style}]`.
* Designer: drag as for doors; the run is drawn as one line with its ends.
* Build: the run's edges become one polyline; the jig is the polyline pushed
  out through the hull; one `JigCut` on the hull and the room.

**Windows that follow a model hull (the user, 2026-11-06).** On a ship whose
hull is a 3D model, windows should follow the outside of the model as much as
possible, so they blend in. Today the cut is a box pushed out through the hull
(`JigCut` with a window jig): the glass is the hull's own surface inside the
box, but the box's outline cuts across the model's triangles, adding slivers,
and a frame and lining go round it. That is most of what looks garbled.

Design, a **conformal** window style for model hulls:

* Find the hull triangles the window looks out at: those whose middles fall
  inside the window's outline, projected outward from the room wall onto the
  hull (rays as now).
* **Snap the outline to the model's own edges**: take whole triangles rather
  than cutting them, so the glass is exactly the model's surface and its edge
  follows lines already in the model. A tolerance decides how far the outline
  may move (perhaps half a foot); past it the triangle is cut as now.
* Those triangles become the window part (same vertices, glass), taken out of
  the hull part; no frame, no lining. The room wall gets its opening and a void
  out to the glass, as the flat pane does now.
* Keep the model's detail there: the thinning that brings a big model under the
  face budget should keep the triangles round the windows (as with liveries,
  track 7), or the glass shape changes when the model is thinned.
* Works with dragged window runs (above), and on a curved hull gives one smooth
  piece of glass instead of a pane per cell.

**Ceiling windows (the user, 2026-11-07).** Ceiling windows (`skylights`,
experimental) are cut today by turning the room and hull until the ceiling is
a wall and cutting as for a wall window; one whole cell each, or a rectangle of
cells with `joinWindows`.

* **A choice per ceiling window: follow the hull's shape, or a flat cutout.**
  Following the shape uses the conformal method above, looking up instead of
  out. The flat cutout is one level pane at a height of its own, as the flat
  wall pane does now. Plan: `"shape": "hull"` or `"flat"` on each skylight.
* **Per triangle, not per cell.** A skylight gets a triangle (`k`, as rooms
  and paint have) instead of only a cell; the designer paints them triangle by
  triangle. Neighbouring triangles of one room join into one outline (any
  shape the triangles make, not only rectangles), cut as one window. Plans
  without `k` keep meaning the whole cell.

**Open questions.** How far a curved hull may bow away from a flat pane before
it needs splitting (a tolerance, perhaps 0.5 ft). Which of the user's designs
show the garbling best, to test against.
**Size:** M.

---

## Track 5. Hull geometry through doors

**What happens.** The door is its own part, but hull faces that pass through
the doorway are still drawn there. With a model as the hull (`asHull`), or the
wall round the rooms that we add to it, the model's own faces can cross an
interior doorway as well.

**Design.** The door takes precedence: for every door, interior doors too,
find the hull faces that cross its doorway box and cut them away with the
door's jig (`JigCut`, near faces only), as hull doors are cut now. For an
interior door the hull is not meant to be there at all, so the cut leaves no
frame. The audit (`ClosedButForOpenings`) must still accept the hull after
these cuts.

**Open question.** An example: which design and which door showed it, to
confirm it is the model's faces and not the room's walls.
**Size:** M.

---

## Track 6. Ship-wide textures

**Now.** Each room has its own floor, wall and ceiling texture (`rooms[].
floorTexture` and so on), each door its own (`doors[].texture`).

**Design.** Plan-wide defaults: `textures: {floor, wall, ceiling, door}`. A room
or door with no texture of its own takes the default; new rooms and doors get
none of their own, so changing the default changes them all. In the designer:
a *Ship textures* box on the Rooms page, and per room a "use the ship's"
choice next to each texture. Old plans keep their textures.
**Size:** S.

---

## Track 7. Liveries (painted patterns kept sharp)

**Name.** *Livery*, the word for a ship's or aircraft's paint scheme. A colour
of the palette marked as a livery colour is one the designer must keep sharp.

**Now.** Paint is per drawn triangle. When a drawn hull is thinned and merged,
paint edges are already kept (`planbuild.cpp:461-503`, `656-902`), but
smoothing still lets a vertex slide along a paint edge, and the face budget
treats every colour the same. A hull made from a 3D model takes its paint by
another route (`ModelHullMesh`), still to read.

**Design.** A **livery** flag on palette colours (`palette[].livery`). Faces of
a livery colour:

* are never merged or thinned, and their edges never slide or round off;
* count first towards the face budget (the rest of the hull is thinned more);
* on a model hull, the livery's outline is cut into the model's faces, so the
  edge is as sharp as drawn, not snapped to the model's triangles.

The designer shows livery colours with an outline, and the report says how many
faces the liveries took.

**Open question.** Which painted design looked wrong in the game, and how (lost
corners, blurred edges, missing pieces), so the fix is aimed at the real
problem.
**Size:** M.

---

## Tracks 8 and 9. Openings between rooms: balconies and windows

**What the game does.** People only walk inside room voids; two rooms side by
side with no doorway between them are separate even with no wall. Windows can
go anywhere (no report rule), and block movement by their own shape
(`AuScWindow::IsPathBlocked`). So an opening between two rooms is mostly a
matter of cutting both rooms' walls.

**Design.** One new plan element on an edge between two rooms, `openings:
[{deck, cells, kind, sill, top}]`, dragged like a door row:

* **Window between rooms** (track 9): both walls cut from sill to top; a pane
  in the hole.
* **Balcony** (track 8): both walls cut above a parapet a third of the room's
  height (the sill), no pane. The parapet is the wall that is left. With the
  room below open to it (*open to deck below*), it is a gallery.

The two room voids stay separate, so nobody walks through; sight goes through.
`crew-check` must not take an opening for a doorway.

**Open questions.** Whether the game draws another room's walls and furniture
through an interior window (check with `shipgen sight` and in the game).
Whether someone can fall or jump over a balcony's parapet; if they can, a
Barrier up to head height in the opening.
**Size:** M (both together).

---

## Track 10. Room types: which ones do something

**What the game does.** In a ship, **the room's name and type do almost
nothing**. What counts is the room void's **hold access** (`m_Access`, low
nibble), its **personnel access** (high nibble) and its gravity.
`DaSc::InsideRoomType` (`lib/atom/entities/vehicles/dasc.cpp:5884`) turns the
hold access into the room type the rest of the game uses:

| Hold access | The game treats the room as | What it gives (`auaccesshold.cpp`) |
|---|---|---|
| Inaccessible | Corridor | nothing |
| Armory | Barracks | the armory; a sea chest for a troop in their berth |
| Bar | Lounge | the bar |
| Feeder | Brig | feeding prisoners |
| FuelCoupling | Engine Room | refuelling |
| VehicleBay | Vehicle Bay | vehicles, ordnance transfer |
| Hold | Hold | the cargo hold |
| Pantry | Galley | food |
| Pharmacy | Sick Bay | medicine |
| Wardrobe | Stateroom | clothes |

Personnel access: command, crew, passengers, troops, or everyone.

**We set every room void to Hold and everyone** (`interior.cpp:601`), so **in
the game every room of our ships is a cargo hold**, whatever its type.

The name still matters as a label: it is what the player sees, turbolift
stops are matched by name, and berths and crew routes may refer to rooms by
name.

**Design.**

* The room's **purpose** decides the hold access: Galley gets Pantry, Sick
  Bay Pharmacy, Engine Room FuelCoupling, Brig Feeder, Barracks and Armory
  Armory, Lounge and Bar Bar, cabins Wardrobe, Vehicle Bay VehicleBay, Cargo
  Hold, every other room Inaccessible.
* The designer's long list becomes **two choices**: the purpose and a free
  name (with the old type names offered as suggestions). Old plans map their
  type to a purpose.
* **The purposes** (the user, 2026-11-06): the ten hold accesses above, and
  besides them these room definitions, which decide what the designer puts in
  the room rather than a hold access (they get Inaccessible, or a hold access
  the user picks):
  * **Bridge**: the command, helm and other stations, and the windows that
    look out (as `PlaceWindows` does now).
  * **Open to deck below**: the floor left out over the room below, with a rail
    round the opening (as now).
  * **Medical bay**: the surgery (medic) station; Pharmacy as its hold access,
    so medicine is there too.
  * **Teleporter room**: the transporter (teleporter) station and its pads,
    with **at least 0.5 m between pads**. A pad is a 3 x 3 ft slab
    (`SOH_FORMAT.md`, "Transporter pads"), so the centres are at least 4.6 ft
    apart. The game's report has no spacing rule; this one is ours, and the
    build warns when the room is too small for the pads it asks for.
* **Who may enter**: everyone by default; command only for the bridge and the
  captain's cabin; crew only for the engine room; as a choice per room.
* For **buildings**, `DaBd::InsideRoomType` (`dabd.cpp:7119`) and the
  industry checks need the same reading before changing anything there: room
  names may matter more for buildings ("cannot be used for its intended
  purpose").

**Open question.** Whether the server's crew or NPC code looks rooms up by name
(`augotoroom.cpp` uses `ROOM_WeaponBay`): to read before the names are freed.
**Size:** M.

---

## Track 11. Door buttons (from the user, 2026-11-06)

**The wish.** After dragging out a door, the user can place a button on a
wall nearby that opens and closes it, and the button looks like one: raised a
little from the wall, in another colour. Esc skips it.

**What the game does.** Each door has two switch locations, one for each side
(`AuScDoor::DoorOpLocs`, `SetDoorOpLocs`; the game's designer moves them with
`MoveDoorSwitches`). With none set, the game puts them at the middle of the
closed door. They are kept in the door's state (`AuScParts::DoorStates`), and
a player works the door from near one. We already set them for hull doors (4
ft up, inside the room and just outside the hull). Nothing in the game's code
draws anything at a switch location, so the button that is seen has to be our
own part.

**Design.**

* In the designer, after a door is placed: a second step, "place the button
  on this side", then "on the other side", each clicked on a wall cell edge
  within reach of the door (a limit to find from the game's reach), or Esc for
  the usual place. Kept in the plan per door (`switch1`, `switch2`: an edge
  and a height).
* The build sets the door's switch locations there and adds a button: a small
  raised box (about 0.4 x 0.6 ft, 0.1 ft proud of the wall) in a colour of its
  own, as a mesh in the room so it is not cut away with the hull.
* **To check in the game**: how close a player has to be, and whether the
  switch has to be on the door's own side of the wall.

**Size.** Small to medium: the designer step is most of it.

## Probes and references

* Probes for this research: `~/coding/soh-probes/rampwalk.cpp` (floor heights
  through a door, closed and open), `ray.cpp` (faces along a line, now to 400
  ft).
* The game's designer commands, for the tools above:
  `lib/designer/commands/spacecraft/` (`interiors/ddscmaketurbolift.cpp`,
  `systems/ddscalignliftstop.cpp`, `appearance/ddscempireflag.cpp`,
  `systems/ddscparkspacecraft.cpp`, `systems/ddscweaponemitloc.cpp`,
  `systems/ddscstationfirecontrol.cpp`).
