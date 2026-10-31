# Changes and known issues

## 31 October 2026: release 0.3.0-alpha

The first release on GitHub (<https://github.com/crazykristi/openshores-designer>),
a pre-release: an AppImage of the triangle designer with `shipgen-gui` and
the command line (`package-appimage.sh`, `RELEASE_NOTES.md`). The version
is in `src/version.h`; the designer's window title and `--version` show it.
The documentation gained `docs/GIT_AND_RELEASES.md` (git, the AppImage,
release steps); the stale status sections in `docs/README.md` and the
history in `HANDOFF.md` were dropped in favour of this page.

## 31 October 2026: ramps and stairs

The user asked for the ramp first, then stairwells. Both are made the way
the user's aircraft factory makes them (`docs/SOH_FORMAT.md`, "Stairs and
ramps"); neither has been tried in the game yet (`work/ingame6/`).

* **Ramps at hull doors**, in the designer and the generator: a level
  landing over the threshold, then a 34 degree slope to half a foot below
  the hull, as an invisible Barrier with a mesh to see, a hull void over it
  and a walk path down it. They take the place of the ladder for doors up
  to 15 ft above the ground. `build-plan --no-ramps` brings the ladder back.
* **Stairs in the designer** (*Place stairs*, `"stairs"` in the plan file):
  three cells for 10 ft decks, with the walk path up them that the factory's
  stairs lack, and a rail round the opening above. Walk paths across the
  rooms keep off the opening and the run.
* **After the user's next test** (`work/ingame6/ingame-test6-observations.md`):
  * Still dark inside. Our room meshes' vertex normals pointed every way
    (most straight up, along the walls); they now point into the room, as
    in the user's scout. The engine also lights a room only while the
    capacitor has charge (`DaSc::InteriorAmbientLight` in the OpenShores
    source); not yet confirmed which of the two it was.
  * Windows can be lower or higher: `windowSill` and `windowTop` in the
    plan (3 and 7.5 ft above the floor by default), *Window sill* and
    *Window top* in the designer.
  * Windows side by side on one straight wall of a room can join into one
    (`joinWindows`, *Join windows side by side*). Windows round a corner do
    not join.
  * The bay door could not be opened: with no switch locations set, the
    game puts them at the middle of the closed door, which for a door made
    of the room wall and the hull 10 ft apart is inside the hull. Hull
    doors now have their switches set, inside the room and just outside
    the hull, 4 ft up (as the Atlantis' hull door has them).
  * The ramp was not noticed; which file was tested is still to be asked.
  * Rooms and hull doors over more than one deck: on the list
    (`docs/BUILDING_TODO.md`).
* **After the Wedge v6 test** (same notes file; lighting now works):
  * The ramp was a ghost: `AuScParts::IsPathBlocked` only looks at
    barriers inside the extents of the hull parts (plus 2 ft), and the
    ramp's mesh did not count. The visible ramp is now a hull part in the
    "Frame" group, and its foot is level with the bottom of the hull (the
    ship rests on it).
  * Chairs faced the side windows. Only fore windows draw chairs now; the
    rest stand in rows facing fore. The Wedge's bridge reaches into the bow
    taper with windows round its front.
  * Rooms were one texture throughout: floors are now Tile Metal (slot 5),
    ceilings Plate Metal Box (slot 1) painted light, walls Wall Metal
    (slot 0) tinted by room (`RoomTint`, or the room's plan colour).
  * Doors could not be told from walls: hull doors are painted yellow and
    interior doors blue, on Wall Cargo (slot 2), in every door state.
  * Wedges are built without shields, weapon bay and transporter
    (`leaveOut` in the plan, the *No shields / No weapon bay / No
    transporter* boxes in the designer).
* **Sloped hull between decks** (the user: sleeker, fewer faces; the drawn
  hull is the minimum): `slopedDecks`, *Slope the hull between decks*.
* **The 3D model importer froze the designer** (the user's 510,000-face
  Moogle): it was working, not crashed, for 86 s on the window's own
  thread. It now runs in the background with a progress window and a stop
  button, and thins with a quick clustering pass first: 9 s.
* **The audit** tested each room's bounding box, which for a room with
  slanted walls (the Wedge's new bridge) reaches outside a tapering hull.
  A room that is not a box and fails the box test is now tested by points
  through its inside, allowing 3% failures by windows (rays leave through
  them).
* `third_party/SoH-source/`: the OpenShores source (github.com/Celarious/
  SoH-source, cloned 2026-10-31 at 849c43b), kept beside `third_party/soh/`
  for reference; the build still uses `third_party/soh/`. It is laid out
  differently (`src/lib/data/designs/auscparts.h` for `audata/auscparts.h`)
  and adds part types, among them `Part_ItemSpot = 92` and
  `Part_VehicleSpot`: blueprints saved by OpenShores builds may carry parts
  that this build skips (`src/sohfile.h`).

## 31 October 2026: after the second test in the game

The user tested `work/ingame2/` (notes in
`work/ingame2/ingame-test2-observations.md`). The hull is solid from outside
now, the ships face the right way, windows work, and the hull door can be
opened. What was still wrong, and what changed:

* **The control chairs were stuck on the walls, turned 90 degrees.** A
  `Console*` part is the game's swivel chair: a flat icon that lies on the
  floor (`docs/SOH_FORMAT.md` section 3). The generator stood them up on a
  wall. They now lie on the floor facing fore, in rows from the room's fore
  wall, counted per room (two kinds of console in one room used to end up in
  the same place).
* **The hull door could only be entered by flying, and pathing inside was
  odd.** Three causes, all from comparing with the working designs: walk
  paths floated a foot above the floor (so they also missed the ends of the
  ladders between decks); the way to the ground was a sloping path ending a
  foot above the hull bottom; and the generator's doors started half a foot
  above the floor. Paths now lie on the floor, doors are flush with it, and
  a ladder path goes straight down from outside the hull door to 2.5 feet
  below the hull, inside the constant hull void, as in the hulk and the
  scout.
* **Ships need 3 to 4 times more maneuver drive.** The drive now gets 3.5
  times its share, from the structure (which keeps at least half of its
  own); the minimum is raised the same way.
* **Aether kit looks** (`docs/AETHER_KIT.md`): smaller Brass Cruiser wheels
  set into the hull; plain tinted glass instead of stained glass on the
  domes; a fainter field (*aether-soft*) for the big sheets of light; worn
  bronze (*cog*) for gears.

* **Windows on generated ships** (asked for by the user): a band of
  windows round the bridge, and one every 12 ft in quarters, the galley,
  halls and the sick bay, wherever the wall is within 10 ft (bridge) or
  7 ft (others) of the hull skin. They are cut with the engine's window jig;
  the frame now reaches back to the room wall (`JigCut`'s sleeve), lining
  the tunnel. `HullPartAt` measured the distance to triangle centres and so
  picked a nearby frame over a hull of big triangles; it now measures to the
  triangles themselves and skips frames. The audit allows 40 open edges per
  door or window, so a hull of a few big triangles with openings is still
  "closed but for openings".
* **The bridge was at the stern.** The layout fills the hull from its low
  end, and with fore now +Y that was the aft. The layout is mirrored, so the
  bridge is at the bow.
* **Transporter pads** are understood (`docs/SOH_FORMAT.md`), from the
  user's airport; `shipgen parts` shows them. The generator's transporter
  stations do not have pads yet.

Then, from the user's third test (`work/ingame3/ingame-test3-observations.md`):
the hull door could be entered but needed a small jump, the ladder between
decks could not be seen, and in the designer the doors had one size, there
were no vehicle parking spots, and the window tool did not work for the user.

* **The jump at the hull door.** The ladder up to it stood 3 ft out, past the
  end of the doorway's voids (1 ft out), so there was nothing to stand on in
  between. The ladder now stands 0.75 ft out and the voids reach 1.5 ft out.
* **The ladder can be seen.** The opening is cut through the ceiling and the
  floor with the engine's cutting jig, and a ladder of rails and rungs
  stands in the shaft.
* **Bay doors.** In the designer, doors on neighbouring edges of one wall
  make one wide door as tall as the room.
* **Vehicle parking spots** in the designer (ground and space vehicle, the
  game's own parts, as in the Atlantis' hold).
* **The window tool** now also takes a click on any triangle of a room cell
  that has one outer wall, and the status bar keeps saying why a click did
  nothing (the hover text used to replace it at once). The mirror of a door
  or window could be undone by the same click when the other side already
  had one; fixed.
* **The layout on hulls that differ fore and aft** (the cargo hauler on the
  Mithra station): putting the bridge at the bow left a room with no wall in
  common with any other. The layout is now fitted both ways round and the
  bow is used unless the other way keeps more rooms or joins them better.

None of this has been seen in the game yet. New test files are in
`work/ingame3/` (the first round) and `work/ingame4/` (this round).

Then, from the user's live production test of the Wedge v4 (crew loading,
the ladder to the bridge and the whole ship work):

* **Very dark inside.** Rooms were made not sunlit; the working designs'
  rooms are bright and sunlit, the engine's default. Fixed.
* **Still a small jump at the hull door.** Cutting the doorway leaves no
  floor in it, and the constant hull void outside started inside the hull
  surface and reached below the floor. The doorway now gets a floor plate
  flush with the room floor, and the void starts just beyond it.
* **The chairs were not at the windows.** In a room with windows the chairs
  now sit in front of them, looking out: the fore windows first (the helm),
  then the sides in turn, never on top of each other.
* **The designer:** windows on diagonal edges (why the window tool failed
  for the user), metric units, and the model importer thins big models and
  copes with models of several overlapping pieces.

## 30 October 2026, evening: after the first test in the game

The user loaded the designer's wedge in the game (`work/ingame/` has the
readout, the notes and the comparison) and exported four blueprints that
work in the game to `exported/ships/`. What was wrong, and what changed:

* **The hull was see-through from outside, and the rooms inside out.** Every
  hull and room mesh we made was wound the opposite way to the game's. The
  working designs have hulls with a negative signed volume and rooms with a
  positive one. `OrientAsHull` / `OrientAsRoom` / `OrientAsVoid` in
  `src/interior.h` now wind every hull and room the generator, the building
  generator and the designer make.
* **The ships lay sideways.** The game takes +Y as fore; the wedge read 26 m
  long and 56 m wide. The designer now turns its blueprint a quarter turn
  (before the systems are sized, because the engine's hull volume depends on
  the orientation); the generator turns a model that is longer along X
  (`--keep-axes` to leave it).
* **The second engineer station was given the maneuver drive.** The game
  fits a maneuver drive itself. There is now one engineer, running the FTL
  drive chosen in the brief (`"ftl"`), on the command line (`--ftl`) or in
  the designer: none, warp 1-9, or a P, PN or PNN wormhole drive. The default
  is the P wormhole drive.
* **The hull door was hard to get through, and a long tunnel.** Hull doors
  are now cut through the room wall and the hull with the engine's own door
  jig (`AuScParts::JigCutParts`), so the hull has a real opening; the voids
  tied to the door reach only just past the hull; a constant hull void
  clears the outside in front of the door from the ground up, as in every
  working design; and a walk path leads down to the ground. Side doors are
  preferred; the generator only uses a fore or aft door when no room reaches
  a side. Interior doors are cut through both walls the same way.
* **Windows.** The designer has a window tool (step 3); windows are cut with
  the engine's window jig, as the game's designer does
  (`docs/SOH_FORMAT.md` section 5a).
* New commands: `shipgen parts` (every part with its door tie, module,
  signed volume and bounds), `shipgen faces` (how faces look: texture slot,
  paint, opacity, light, effect, glow) and `shipgen cut` (cut a window or
  door into a blueprint). `docs/TEXTURE_LIBRARY.md` lists the game's library
  textures and how the example blueprints make glowing, see-through,
  animated surfaces.

Cutting leaves holes in the hull, as in any blueprint from the game's
designer. The audit now tells a hull that is "closed but for openings" (no
folded edges, at most 8% of the edges open) from a broken one, and tests
rooms in it by a vote of seven rays, allowing up to three of the 27 sample
points right by an opening to fail. On a curved hull the engine's own lining
of a cut hole comes out as overlapping fragments, so `JigCut` cuts only the
faces near the opening, takes the lining out and puts a clean frame round
the opening instead.

* **The Aether kit** (`shipgen kit`, `docs/AETHER_KIT.md`): three ships and
  four buildings in a high-tech and magic style, after the user's Aetherite
  Capitol, made with the game's library textures, animated surface effects,
  glow and see-through fields of light. All are accepted by the engine; none
  has been seen in the game yet.

## 30 October 2026

### The documentation

The `docs/` folder had been filled by another tool with pages that did not
match the code or the engine: invented file names (`shipsystem.cpp`,
`scutiling.cpp`), invented classes (`class Generator`), invented build steps
(`qmake build.pro`, a `setup.sh`), wrong defaults (a 2 ft wall; it is 8 ft),
and thirteen `SOH_*.md` pages about the file format whose contents were not
true of the format at all. Those pages are kept in `work/old-docs/` and the
folder has been rewritten from the source: `README.md`, `SOH_FORMAT.md`,
`COMMAND_REFERENCE.md`, `SOURCE_CODE_REFERENCE.md`, `DESIGNER.md`,
`TROUBLESHOOTING.md` and this page. `WINDOWS.md` and
`MCP_AND_OFFLINE_DESIGNER.md` were accurate and are kept, with their status
brought up to date.

### Blueprints from newer game builds were read wrongly

`whitestar-notele.SoH` was believed to be defective (no hull, no voids). It
is not. It has 88 parts, and part 201 is of type 90, which the published
engine source does not know. The engine's reader stops at an unknown type
without saying so to the caller, so it returned 33 parts and an empty hull.
`src/sohfile.h` now walks the part list itself, steps over unknown parts,
keeps their bytes and writes them back on save. Every tool loads through it.
`shipgen resave` round-trips a file. See `SOH_FORMAT.md` section 4.

Side effect: `test.sh` used to "generate" ships from the White Star's empty
hull; it now finds its real hull, which is six open shells, and the
generator refuses it as not closed, which is right.

### Rooms only existed while a door was open

The generator associated each room's **own** room void with the open state
of one of its doors. The engine treats an associated void as one that comes
and goes with the door (`AuScParts::ListRoomNames` skips it), so in the game
the room only existed while that door was open. Every generated ship had
this. Doors now get a small doorway void of their own, associated with the
open state, and the room voids stay constant; hull doors get a doorway void
and a generous hull void. `src/interior.h` does this for the generator, the
building generator and the designer.

Also fixed on the way:

* The building generator's entrance door never got a void: it read the room
  void's id from a copy that was never filled in.
* Walk paths ran in straight lines from one room's middle to the next, through
  walls. They now go through the doors, and are associated with them.
* Doors were placed only while "neither room had a door yet", which could
  leave groups of rooms that could not reach each other. They are now placed
  as a spanning tree over the shared walls, widest first.
* `inspect` and `audit` treated doorway voids as rooms (they reach through
  walls and out of the hull on purpose). They now skip them, report rooms that
  only exist while a door is open, and count rooms joined by a ladder shaft as
  reachable. `test.sh` fails on a room whose only void is tied to a door.

### A closed solid may pinch

`MeshSolidity` counted an edge shared by four faces, as where two solids
touch along an edge, as non-manifold, and refused the exact test. If the
faces go round the edge in equal numbers each way, the inside is still well
defined, so such "pinched" edges now count as closed.

### The triangle designer

A third program, `shipgen-designer`, and `shipgen plan-example` /
`shipgen build-plan`: draw the hull deck by deck on a grid of cells cut into
four triangles, let it be shaped into a smoothed closed hull, draw the rooms,
doors and ladders on the same triangles, paint it, and build the blueprint.
See `DESIGNER.md`. The generator's furnishing (stations, berths, systems) is
now `FurnishRooms` in `src/generate.h`, so that rooms placed by anything can
be furnished.

### Smaller

* The 3D view (`src/gui/preview.cpp`) draws painted faces in their paint, can
  be told how opaque the hull is, can be given a design without reframing,
  and frames a tall, narrow view properly.
* `shipgen --help` lists `resave`, `plan-example` and `build-plan`.

Test matrix after these changes: see the last line of `work/test-run*.log`.

## Earlier sessions

The history before this session is in `HANDOFF.md` and the root
`README.md`. The main points:

* Hull measurements were nondeterministic: `AuScPart::Faces()` replaces the
  list it is given, and every hull part was collected into one list, so only
  the last part in `QHash` order was measured.
* Rooms are tested against the hull triangles exactly (`src/audit.cpp`), not
  only against the engine's silhouettes, which accepted rooms in the open air
  of a concave hull.
* `build.sh` did not rebuild an object when a header changed; it does now.
* The stl writer mixed two ways of writing and produced unreadable files.
* The engine's obstruction-cache warning is dropped by one message handler
  (`src/logging.cpp`); it does not apply to an offline tool.

## Known issues and open questions

* **Nothing made by these tools has been tried in the game this session.**
  The door and void model follows the engine source exactly, but the White
  Star shows that the live game is newer than the source and walks without
  any room voids. Whether the live game still honours room voids, hull voids
  and walk paths the way the source does is the most important open
  question.
* **Part type 90** is only partly understood (`SOH_FORMAT.md` section 4).
  Writing one is not possible yet.
* **The Windows build** is source-complete but has not been built on a
  Windows machine (`WINDOWS.md`). The designer is built by the same script
  and should come along, but is equally untested there.
* **The industry table** is a stand-in; the real one is in the game database.
* **Models** take about ten seconds to import when they are very detailed;
  most of that is the engine's stl reader.
* **A building made from a model** has no windows, and a solid model with no
  rooms has no door.
* **The designer** draws new spacecraft only; see the end of `DESIGNER.md`.
* **The White Star's hull** is six open shells, so the generator will not put
  rooms in it, and `audit` cannot check its rooms.
