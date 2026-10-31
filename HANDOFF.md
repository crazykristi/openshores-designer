# Handoff state

Updated 31 October 2026 after the user's second test in the game. Start at
`docs/README.md`; what changed is in `docs/CHANGES_AND_KNOWN_ISSUES.md`.

**31 October** (notes in `work/ingame2/ingame-test2-observations.md`):
console chairs now lie on the floor facing fore; walk paths lie on the
floors; doors are flush with the floor; a ladder path leads down from each
hull door to below the hull; 3.5 times more maneuver drive; kit looks
softened (glass domes, fainter big fields, smaller bronze wheels on the
Brass Cruiser). Test files for the third test: `work/ingame3/`.

**Later on 31 October**, after the third test
(`work/ingame3/ingame-test3-observations.md`): the hull door's ladder moved
in so that no jump is needed; ladders between decks get an opening and a
visible ladder; generated ships get windows and the bridge is at the bow;
the designer has bay doors, vehicle parking spots, a sturdier window tool
and *New plan from a 3D model* (`shipgen plan-from-model`). Transporter pads
are understood (`docs/SOH_FORMAT.md`). Test files: `work/ingame4/`
(`WHAT_TO_TEST.md`).

**After the Wedge v4 production test** (it works fully, crew loading
included): rooms are sunlit (they were very dark), the hull doorway has a
floor plate, chairs sit at the windows; in the designer, windows on
diagonals, metric units, and the importer thins big models and copes with
overlapping pieces. Stairs and ramps are understood from the user's
aircraft factory (`docs/SOH_FORMAT.md`, "Stairs and ramps"); stairwells and
ramps are next on the designer list (`docs/BUILDING_TODO.md`). Test files:
`work/ingame5/`.

**Ramps and stairs** (the user: "ramp first, then stairwells"): hull doors
up to 15 ft above the ground get a ramp instead of the ladder (designer and
generator, `AddDoorRamp`; `--no-ramps` for the ladder); the designer has
*Place stairs* (`AddStairFlight`, `AddRail`). Not yet tried in the game; test
files `work/ingame6/`. The OpenShores source is in `third_party/SoH-source/`
for reference (the build still uses `third_party/soh/`). The deployable ramp
waits on how the fixed one does.

**After the user's sixth test** (`work/ingame6/ingame-test6-observations.md`):
room vertex normals now point into the room (they pointed every way; rooms
were still dark); window sill and top and joined windows in the designer;
hull door switches set within reach (the bay door could not be opened).
Asked: which Wedge was tested (the ramp was not noticed). Multi-deck rooms
and hull doors are on the list.

**After the Wedge v6 test** (lighting works now): the ramp is solid (it is a
hull part, so the engine looks for its barrier at all), chairs face fore,
the Wedge's bridge has front windows, room surfaces and doors are told
apart by texture and colour, and Wedges leave out shields, weapons and the
transporter. Test files `work/ingame7/`. Asked the user about a sloped
hull between decks.

**Later on 31 October:** the code is on GitHub
(https://github.com/crazykristi/openshores-designer; the user pushes).
The triangle designer is the mainline application and main release item
(an AppImage, `./package-appimage.sh`, holding shipgen-gui and the command
line too); `shipgen-gui` is secondary. In the long run the designer becomes
an offline designer in the OpenShores client. New: sloped
hull between decks, background model import with progress (and 9 s instead
of 86 s for half a million faces), an audit that copes with slanted rooms.

**Next work:** the building todo list agreed with the user,
`docs/BUILDING_TODO.md` (power plants first).

**What the first half did:** blueprints from newer game builds are read
completely (`src/sohfile.h`); doors and voids fixed (rooms had only existed
while a door was open); the triangle designer, `shipgen-designer`.

**What the second half did**, after the user's first test in the game
(notes and readout in `work/ingame/wedge-ingame-readout.md`, four working
blueprints the user exported in `exported/ships/`):

1. **Winding.** The hull was see-through in the game: every hull and room we
   made was wound the opposite way to the game's. Fixed for the generator,
   the building generator and the designer (`OrientAsHull` and friends,
   `src/interior.h`).
2. **Fore is +Y.** The designer turns its blueprints, the generator turns
   models that are longer along X (`--keep-axes`).
3. **Engineering.** One engineer runs one FTL drive, chosen in the brief
   (`"ftl"`), with `--ftl`, or in the designer; default the P wormhole drive.
   The game fits the maneuver drive itself.
4. **Doors and windows are cut** through the walls and the hull with the
   engine's own jig cut (`JigCut`), on the faces near the opening only, with
   a clean frame. Hull doors get a constant hull void in front, from the
   ground up, and a walk path to the ground; side doors are preferred. The
   designer has a window tool.
5. **The audit** accepts hulls with door and window openings ("closed but
   for openings").
6. **New commands:** `parts`, `faces`, `cut`, `kit`.
7. **The Aether kit**: seven blueprints (three ships, four buildings) in the
   style of the user's Aetherite Capitol. `docs/AETHER_KIT.md`.

**The next work is decided by the user.** The MCP server
(`docs/MCP_AND_OFFLINE_DESIGNER.md`) has still not been started.

## Questions for the user

Answered (30 October 2026):

* **White Star:** saved by the newest game build; its source is not
  available; the format is closely based on the published source and partly
  on the old pre-2016 ship designer. It has no hull void, which is why
  players get stuck at its door.
* **Designer defaults** (5 ft cells, 10 ft decks, 2 ft skin, doors one cell
  wide): fine.
* **Doors:** side doors; belly doors are buggy in the current game build.
* **FTL:** a P wormhole drive for testing; selectable in the end.

Open:

1. **The third test in the game**: `work/ingame3/` (`Wedge.SoH`,
   `Military frigate_decks.SoH`, the seven kit blueprints). Do the chairs
   sit right, can the hull door be walked into from the ground by the
   ladder, does pathing inside work now? Do the softened kit looks work?
2. Answered 2026-10-31, recorded in `docs/BUILDING_TODO.md`: minimal and
   expanded variants, the power plant list, the style (Aether mixed with
   high-tech and cyberpunk), the ships to make, decorations may stay hull,
   the Windows build is postponed. Still open: windows on generated ships,
   which 3D models go with which building. Teleporter pads are understood (`docs/SOH_FORMAT.md`).

## What exists and works

`/home/crazykristi/Documents/hazeron-shipgen/`

| Piece | State |
|---|---|
| `build.sh` | Builds the engine subset (AuUtil, AuMath, AuGL, AuGlobal, AuData) + `build/bin/shipgen` (CLI) + `build/bin/shipgen-gui` (GUI) + `build/bin/shipgen-mesh`. On MSYS2 it uses `build-windows/` and `.exe` files. |
| `build-windows.ps1`, `deploy-windows.sh` | The Windows entry point and the Qt/OpenAL/MinGW deployment step. |
| `docs/WINDOWS.md` | The Windows build, deployment and troubleshooting guide. |
| `src/shipgen.h`, `src/brief.cpp` | The brief model, the ship templates, brief JSON I/O. |
| `src/names.h`, `src/names.cpp` | The names the game itself uses. The spacecraft room names come from the engine's own `ROOMTYPE` list, the building room names from the game's own blueprints. |
| `src/generate.h`, `src/generate.cpp` | Hull analysis, the three layout strategies, model import, room containment and fitting, doors, part building, ship validation. |
| `src/paint.h`, `src/paint.cpp` | Texture loading, colour tiles, palette slots, UV projection, painting every hull part. |
| `src/logging.h`, `src/logging.cpp` | One Qt message handler per tool, dropping the engine's non-applicable obstruction-cache warning. |
| `src/building.h`, `src/building.cpp` | Building generator, building templates, `AuBdReport` validation, the stand-in industry table, the "largest rectangle inside the model" search. |
| `src/names.h`, `src/names.cpp` | The names the game itself uses: the room types, the station types, the industries. |
| `src/gui/` | The graphical tool: main window, generation wizard, OpenGL preview. |
| `src/main.cpp` | The command line tool. |
| `test.sh` | The matrix test. |
| `testdata/` | Test models. `extract_models.sh` unpacks the zip files from the `3d models` folder. |
| `tests/probe.cpp` | A small program that checks how the engine's geometry primitives behave. |
| `docs/MCP_AND_OFFLINE_DESIGNER.md` | The plan for the MCP server and the offline designer. |

Last full test run: **330 checks passed, 115 blueprints validated cleanly,
0 with errors** - spacecraft, buildings, the three primitive base shapes, two
models on one base, painting (a single hull and a model in parts), the
graphical tool, and the real models. One of the tests checks that the
generated Aetherite Capitol matches the reference blueprint in
`testdata/aetherite_capitol_reference.SoH`, and another checks that no
blueprint goes over the face budget. Every generated ship is also checked with
`shipgen audit`, which tests exactly whether every room box is inside a closed
hull solid, alongside the older silhouette inspection for rooms outside,
overlapping rooms, and doors without the room or hull voids that make them
work.

The local building corpus at `~/Documents/designs/examplebuildings` has 13
more blueprints from the game, including houses, farms, factories, an airport
and power plants. `test.sh` reads every one of them through `inspect`. They
are not all expected to validate: `aetherite.nucplant.SoH` is over the 14 MB
limit, and a few were saved for a purpose the engine no longer accepts. They
are useful as a check that the reader understands real game files, and as a
reference for the part types and industry ids.

The reference Capitol has 15,156 faces, which is right at the limit. The
generated one has about 4,200, which is much better.

* **Changing the shape of a model.** `src/shape.cpp` moves the vertices of an
  imported model around before it becomes the hull: a taper with the height,
  a twist about the axis, copies around a circle, copies stacked up, noise
  pushed along the surface, and a stretch. All of it keeps the material and
  the look of the original and changes the silhouette. There are named looks
  (`ShapePresets()`) that the graphical tool offers in a panel and the
  command line offers with `--preset`, and `--vary` makes a set of blueprints
  that are the same look with slightly different proportions.

  The operations all work on the triangle list, so they happen before the
  model is thinned out, and the copies of a model keep their own parts. The
  shape is applied after the model has been scaled and centred, so a taper
  really does make the building smaller; use `--fit` to make it bigger.

  A building that is too small for its industry says so: "the building does
  not have enough of something for a capitol", and the engine's own check
  then fails it. That is the honest answer, not a bug.

* **The mesh tool.** `src/mesh.cpp` reads 3ds, stl and obj files, and has
  two ways of taking a model down to a number of faces. The good one is the
  quadric error method from Garland and Heckbert: the edges are collapsed
  cheapest first, each corner is put where the surface around it wants it to
  be, and the edges of the model get a plane standing up through them so that
  the silhouette stays where it is. On the Aetherite, 670,000 faces become
  2,000 with 98.7% of the volume and a surface that grows by 5%. The other
  way is the clustering that was used before, which is much quicker but makes
  the surface blocky.

  `src/meshapp/` is a small program of its own, `shipgen-mesh`. It loads a
  model, takes it down to the number of faces that is asked for, and shows
  the original over the top of the reduced one so that the difference can be
  seen. It saves as a binary stl or an obj file.

  The **Simplify** and **Reset** buttons were not connected to their slots at
  all; only the `-f` command line path worked. They are connected now. The
  model list keeps the triangles of each file separately, so **Remove** takes
  that model out of the assembly and puts the remaining ones together again,
  instead of only hiding the row. **Clear** empties the geometry as well.

* **A primitive base shape.** `src/base.h` and `src/base.cpp` make a closed
  rectangle, circle or hexagon solid, standing on the ground and centred on
  the origin. `ModelImportOptions::base` carries it into `ImportHullModel()`.
  With a base, each model file is scaled and centred on its own and placed in
  a grid that follows the base (the grid gets as square as the base's aspect
  allows, with a tenth of a cell between models). Without a base the files
  stay parts of one model, and the first file is the anchor: the group is
  centred on the first model's centre rather than on the whole assembly.
  `separateModels` (on by default for a base) switches between the two.

  The base is added as its own closed hull part after the models are thinned,
  so its few faces are never rationed away by the face budget. A base on its
  own is a valid blueprint, which is what makes a primitive building possible.
  The wizard has a `BaseShapePanel` at the top of both source pages; it says
  what the shape, size and height are and whether the models are separate.

  The blueprint generator now uses the quadric method as well, so the shells
  it makes keep their shape. Each part of a model gets a share of the face
  budget in proportion to how many faces it has.

* **A model has to be closed.** A model with holes in it cannot be measured,
  and the game cannot tell what is inside it, so the volume comes out wrong
  and the rooms do not fit. The generator therefore puts the model right
  before it uses it: `FixWindings()` turns the triangles that wind the wrong
  way round, and `FillHoles()` puts a lid over the holes. It says what it
  found, and `--no-repair` (or the "Put the model right" box in the tool)
  turns it off.

  The Moogle Warrior is the hard case: its boundary is not a set of neat
  holes but 167 separate paths, and at 48 corners two sheets of the surface
  meet at a single point. The lid closes the edges along each path, but those
  corners cannot be closed without putting a thin wall across the model,
  which would make the volume wrong in a different way. They are left open and
  reported: "Most of them have been closed, but 277 are still open where the
  model is pinched." The effect on the volume is small — 29,498 m³ unrepaired
  against 30,210 repaired, about 2%.

  The Aetherite, Graha, Uriangier and Mithra are closed already, so nothing
  needs doing to them.

* **The build script watched only sources.** `compile_tree` rebuilt an object
  when its `.cpp` was newer, but not when a header beside it had changed. Adding
  a field to `ModelImportOptions` therefore left `building.cpp` compiled against
  the old layout, and `generate-set` read the new `base` and `notes` fields out
  of nowhere and crashed. A header that is newer than the object now rebuilds
  it too.

* **The stl writer had a bug.** `writeRawData` and the stream's own writing
  were mixed together, and the stream holds on to what it is given until it
  is told to write it, so the parts of the file ended up in the wrong order.
  Every stl file the tool wrote was unreadable. It is all raw writing now.

* **Tidying a model up.** `InspectMesh()`, `CleanMesh()`, `FixWindings()`,
  `GroundMesh()`, `MirrorMesh()` and `SmoothMesh()` in `src/mesh.cpp`. Every
  one of the models from the internet has triangles that wind the wrong way
  round, which is what makes parts of a model look inside out; the Moogle
  Warrior also has 555 loose edges and 5 triangles that are there twice, and
  Graha has 172 duplicates. `FixWindings()` walks over the model and turns
  the triangles that need it over, so that neighbours on a shared edge go
  round it in opposite ways.

* **The model picker is a list.** `FileList` in `src/gui/filelist.h` is a
  list of files with Add, Remove and Clear. The wizard's ship page and
  building page both use it, so a model that is in several files can be picked
  in one go and any of them can be taken away again. The Project panel shows
  one row for each model file. `GenerateFromBriefFile()` takes the whole list
  now, not just the first file.

* **The windows scroll.** The mesh tool's panel is in a scroll area, and so
  is every page of the wizard in the graphical tool, so both work on a screen
  that is not very tall. Every button says what it does in its tooltip and in
  the bar at the bottom.

* **A set of buildings.** `BuildingSet` in `src/building.h` is a list of
  buildings made from the same model, each with its own template and its own
  shape. `shipgen generate-set --set <file.json>` makes them all;
  `shipgen set-brief <file.json>` writes an example to look at. The graphical
  tool has the same thing as a table on the building page, with a row for
  each building: which type it is, what look the model is put into, its name
  and how many versions of it to make.

  The set is what answers "the same aesthetics, but for different buildings":
  the Aetherite becomes a cathedral of four spires for the capitol, a twisted
  spire for the arena and a tower of three for the office block, and they all
  look like they came from the same workshop.

## Building and running

```bash
cd ~/Documents/hazeron-shipgen
./build.sh          # about 3 minutes
./test.sh           # about 6 minutes
```

The toolchain (Qt 5 headers, `moc`, a C++ driver and `Xvfb`) lives in
`~/.local/hazeron-toolchain`. The engine source lives in `third_party/soh`
(a trimmed 15 MB copy of the upstream repository).

To look at the graphical tool on a machine with no display:

```bash
$TC/x11/Xvfb :99 -screen 0 1280x900x24 &
DISPLAY=:99 LIBGL_ALWAYS_SOFTWARE=1 ./build/bin/shipgen-gui
```

`shipgen-gui` also takes `--open`, `--screenshot`, `--screenshot-preview`,
`--wizard`, `--wizard-page`, `--wizard-template`, `--generate`,
`--generate-building`, `--model` and `--fit`, which makes it possible to drive
it from a script. `--screenshot-preview` saves the 3D view on its own; the
test uses it under Xvfb to check that the textured preview really draws,
because the offscreen platform does not give the widget an OpenGL context.

### Windows

The GUI and mesh tool are ported through MSYS2/MinGW-w64 + Qt 5. The same
`build.sh` detects an MSYS2 shell and writes `build-windows/bin/*.exe`.
`deploy-windows.sh` uses `windeployqt` and then copies the OpenAL and MinGW
runtime DLLs into `build-windows/dist/`. `build-windows.ps1` is a wrapper
that starts the MINGW64 shell for you. The full instructions, package list
and troubleshooting notes are in `docs/WINDOWS.md`.

**This has not been run on a Windows machine yet.** The Linux build and the
330-check matrix still pass. The Windows branch needs a first real build,
then the smoke test in `docs/WINDOWS.md`. The known platform assumptions are:

* x86_64 MSYS2, Qt 5.15, OpenAL Soft and the MinGW-w64 g++ toolchain;
* `qmake-qt5 -query` finds Qt; no hardcoded `~/.local` path is used on Windows;
* OpenAL's `<al.h>`/`<alc.h>` are found through `pkg-config` or
  `/mingw64/include/AL`;
* the engine links `opengl32` instead of `-lGL`;
* the GUI programs use Qt's `qtmain` with `-mwindows` when it is available,
  and fall back to a console subsystem if it is not;
* the preview asks for a desktop OpenGL 3.3 core context on Windows and uses
  a VAO, which core profiles require;
* `windeployqt` provides `platforms/qwindows.dll` and the Qt DLLs; the
  deployment script adds OpenAL and the MinGW runtime.

If a Windows build fails, the first useful output is the failing compiler or
linker line plus `g++ --version` and `qmake-qt5 -query QT_VERSION`.

## What a building blueprint is

This came out of the reference blueprint the user supplied, and it is worth
writing down because it is not what the first version of the generator did.

A building is:

* a **shell** - one or more hull parts. A model that comes in several files
  gives one hull part per file, and the parts keep the positions they have in
  relation to each other.
* a **site** - a flat polygon on the ground that covers the shell and the
  parking.
* the **hull volume shared out between the systems**. The brief asks for the
  volumes in cubic metres and they are used as they are; what is left over
  goes to the structure, so the systems always add up to the size of the
  building. If the brief asks for more than the building can hold, everything
  is scaled down.
* **parking** - ground vehicle parking spots in a row in front of the
  building, and space vehicle landing pads just behind them.
* the **industry** it serves.

An interior is **optional**. The reference Capitol has no rooms at all, because
the Aetherite is a solid object with nothing inside it. The room machinery is
still there for a brief that asks for an interior.

The reference:

| | |
|---|---|
| Shell | 3 hull parts, the three Aetherite files |
| Site | 350 x 350 feet |
| Parking | 5 ground vehicle spots, 2 space vehicle pads |
| Volumes | offices 500, store 500, hold 500, auditorium 2000, structure 20745 |
| Industry | 123, the Capitol |
| Rooms | none |
| Validation | 0 errors, 0 warnings |

The volume thresholds that decide whether a building can serve an industry
are in `audata/aubddesign.cpp:2661-2726`: arena 5000, auditorium 1132,
generator 2265, home small 57, home medium 114, home large 228, lounge 637,
office 34, shop 91, store 566.

## The models

The zip files in `~/Documents/3d models` are unpacked by
`testdata/extract_models.sh` into `testdata/models` with these names:

| File | What it is |
|---|---|
| `aetherite_bottom.stl`, `aetherite_ring.stl`, `aetherite_top.stl` | The Final Fantasy XIV Aetherite, in three parts. Used as a **capitol**. |
| `moogle_warrior.stl` | The Moogle Warrior. Used as an **arena**. |
| `graha.stl`, `uriangier.stl`, `mithra.stl`, `shtola.stl` | Other miniatures. |
| `moogle_warrior_unsupported.stl` | The Moogle Warrior without the fixing. |

They are in millimetres and they are solid objects, so `--fit` is used to
scale them and the generator has to find the room inside the shape.

## The important findings about the engine

* Design files are `.SoH`: `0x24 0x02` then a zlib compressed, XOR encrypted
  `QDataStream` (`audata/audesign.cpp:429`). There is no text format and no
  `.building` format (`BUILDING_FILE_TYPE` is dead code).
* `SHIP_FILE_TYPE` (`0x10`, the legacy `.ship`) is defined but referenced
  nowhere, so the user's twelve `*.ship` files cannot be read.
* Rooms are an `AuScRoom` mesh plus an `AuScRoomVoid`; the void's group name
  is the room's identity. Stations are `AuScConsole*` parts and the console
  type decides which system it controls (`AuScPart::ModuleSystem`).
* The engine's own room type list is `ROOMTYPE` in `auutil/audef.h`, with
  `RoomTypeNameInSpacecraft()` giving the names.
* `AuDesign`'s constructor asserts that the design id is not zero. Use 1.
* A ship may be 28 MB and a building 14 MB, so a detailed model has to be
  thinned out before it can be used.
* **The face budget.** The game copes with up to about 15,000 faces in a
  blueprint, is comfortable with about 5,000, and is happy with about 2,000.
  There is no check for this in the engine; it is a practical limit. The
  importer thins a model out to the budget (5,000 by default, `--faces` to
  change it) and reports the number of faces it ended up with. The thinning
  snaps the vertices onto a grid and drops the triangles that collapse, and the
  grid size is worked out from the face count rather than searched for, which
  keeps it to a few passes over the model.
* A small hull cannot carry an FTL drive: the drive and the capacitor that
  feeds it need more room than the hull has. The generator leaves the drive
  out and warns about it.

* **Rooms must stay inside the hull.** The engine's obstruction model absorbs
  the faces of a room that touches the hull, so a room that pokes out can make
  the engine's own silhouette grow around itself and pass validation. The
  generator therefore tests rooms against **hull parts only**, with the exact
  closed-solid test in `src/audit.cpp` (see the next bullet). When no closed
  solid is available it samples every corner, edge midpoint, face point and
  3x3x3 grid point, but that is only a fallback and is reported as such. It
  shrinks the room uniformly and then per axis, moves it through the usable
  interior, and refuses to go below 6 feet across and 7 feet high, at which
  point the room is left out and reported. Rooms that would overlap an
  already placed room are rejected. `inspect` prints the fit for every room,
  and a final pass deletes any room that is still outside.
  `AuScSilhouette3d` rasterises at 0.5 m/pixel, and the hull-only silhouette
  uses a margin of 0 rather than the engine's 2 feet. That is stricter than
  the engine, on purpose.

* **The silhouette test was not proof, so `audit` is exact.** Measuring rooms
  against the silhouette alone was not sound: a run over 214 real designs
  found 228 rooms actually outside the hull and 984 that could not be checked.
  `src/audit.cpp` now builds a closed-triangle `SolidMesh` with a BVH and tests
  the room box exactly (`SolidMesh::Contains` for a 3x3x3 sample and a
  triangle/box overlap test). `shipgen audit <design.SoH>` reports the hull's
  solidity and whether every room box is inside; `inspect` prints exact
  room-fit lines and an "outside the hull" count. Rooms are only trusted when
  the hull part is a closed solid. An open hull is refused for generation
  unless `--allow-open-hull` is given, in which case the tool tries
  `SolidifyMesh()` and falls back to the silhouette with a warning. The test
  suite now runs `audit` on every generated design.

* **Hull measurements were nondeterministic.** `AuScPart::Faces()` **replaces**
  the list it is given, and `AnalyseHull()` was accumulating every hull part's
  faces into one shared list, so only the last part in `QHash` order was
  measured. Hull extents jumped between runs (Mithra once measured
  `200 x 315 x 33 ft` instead of `275 x 442 x 866 ft`). `CollectHullFaces()`
  had the same bug. Each part is now collected locally and added, so extents
  and room fitting are repeatable. This bug was also hiding the room-leak
  result above.

* **Tall hulls put rooms in separate lobes.** When the hull is more than 60
  feet tall, the layout is confined to the horizontal band with the most solid
  interior (at most 40 feet high), and the per-room fallback searches only
  that band. Rooms are placed to share a wall with an already placed room
  (`SharedWallArea`), then snapped flush to a neighbour, including lining up
  its floor. That is what gives 10-12 doors and no rooms without a door on the
  Mithra frigate. On a narrow hull the spine corridor gives way and a 6-foot
  room can still take an interior door: the door needs only a 5-foot shared
  wall, not 6.5.

* **The obstruction cache is built outside the game.** `RefreshObstructionsCache()`
  is what fills the cache that `AuScReport` and the hull volume read, but the
  engine prints `error: AuScParts::RefreshObstructionsCache must only be
  called on scene client.` when `g_Au.IsSceneApp()` is false, which it always
  is in an offline tool. `src/logging.cpp` installs one Qt message handler in
  each tool that drops exactly that known warning and passes every other
  message, including real engine errors, through. The cache is correct; the
  engine only dislikes the caller.

* **Doors work by association.** A door is a part with a `Closed` and an
  `Open` state. A room void (or a hull void) points at the door and at the
  state it belongs to; a door with no such void on its open state opens onto
  nothing. A void has one association slot. **A void that is associated
  comes and goes with the door**, so it must be a small doorway void, never
  the room's own void (corrected 30 October 2026; before that every room only
  existed while one of its doors was open). `src/interior.h` builds rooms,
  doors, hull doors, ladders and walk paths this way, and
  `docs/SOH_FORMAT.md` section 5 has the details. `inspect` lists every
  door, its states and its open-state room and hull voids.

* **Textures: the tool assigns them per hull part; the faces could carry
  more.** Every face has its own texture slot, paint colour and colour theme
  (`AuScFace`, `aumath/auscface.h`); the designer paints faces directly. The design palette has 14
  usable user slots (`SCTEXI_User0`, `SCTEXI_User2`..`SCTEXI_User13`;
  `SCTEXI_Material` is the engine's). `src/paint.cpp` loads images or turns
  `#rrggbb` into a 64x64 panel tile, fills the palette, gives each hull part
  the next slot and runs one of the engine's own projections
  (`ByFace`/`Planar`/`Cylindrical`/`Spherical`/`Keep`). The engine's
  `TextureProject` and friends test `selInfo.m_Faces.contains(index)`, so an
  empty selection means **nothing**; the painter fills every face index.
  `AuScFaceList` deep-copies faces and `AuScPart::SetFaces()` puts the
  changed faces back. The import applies paint last, after the base and the
  thinning, and `CopyHull()` carries the palette across. The preview groups
  vertices by texture slot and binds one texture per draw range; a face with
  no texture coordinates is drawn without a texture rather than sampling the
  corner of slot 0.

* `whitestar-notele.SoH` in `~/Documents/designs` was believed to be
  defective. It is not: it was saved by a newer game build, with a part of
  type 90 that the engine source does not know, and the engine's reader
  stopped there. Read with `src/sohfile.h` it has 88 parts: six hull parts
  (open shells, one per deck), 44 rooms, 9 doors, stations and berths, but no
  room voids, hull voids or walk paths of the known types. See
  `docs/SOH_FORMAT.md` section 4.
* Buildings use the **same** part model as ships. What they add is a site
  (a flat polygon on the ground), an industry and a `BUILDING_STRUCTURE`.
* The industry table lives in the game database, so the tool installs a small
  stand-in table (`InstallIndustryTable`) to make `AuBdReport` work.

## What is still to do

The user decides the next work. Known loose ends, in the order they would be
worth doing:

* The second test in the game (question 1 above).
* The Windows build has not been run on Windows; `docs/WINDOWS.md` has the
  smoke test. It now builds `shipgen-designer.exe` too.
* The designer: open existing blueprints, turrets, choosing where stations
  go, rooms taller than a deck, the kit's looks in the paint step (end of
  `docs/DESIGNER.md`).
* Windows for the generator's ships and for buildings (the designer has
  them; `JigCut` is ready for it).
* The game's library texture images are in its `libAuTexture.dll`
  (Steam/Proton prefix, `AppData/Local/OpenShores`); extracting them would
  let our 3D views show the real materials.
* Read and write part type 90 once its layout is known.
* The import of a very detailed model takes about ten seconds.
* A building made from a model has no windows.
* The stand-in industry table has a fixed set of industries.
