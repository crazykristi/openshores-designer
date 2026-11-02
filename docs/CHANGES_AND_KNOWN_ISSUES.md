# Changes and known issues

## 2 November 2026: release 0.3.6-alpha

* **Version 0.3.6-alpha** (`src/version.h`, `RELEASE_NOTES.md`): everything below
  from 1 and 2 November that came after 0.3.5-alpha.
* **Tried in the game**: the user tried `alliance_battleship_fixes3` (sitting
  desks) and `alliance_battleship_standing2` (standing desks) and found them
  good. Notes below saying these fixes were "not yet tried" were true when
  written.
* **Still open**: a false floor in the Vehicle Bay (found by a scan, about 1 ft
  below eye height), and 5 "rooms outside the hull" the audit reports on that
  battleship, not looked into yet.

## 2 November 2026: the same plan builds the same blueprint

Building a plan twice gave different files: the battleship's hull came out
anywhere from 96,122 to 97,876 cubic meters. The engine keeps a ship's parts in
a `QHash` and takes them in that order when it cuts doors and windows, and Qt
chooses a new order for every run. Both programs now fix that order when they
start (`qSetGlobalQHashSeed(0)`), so the same plan gives the same file, from
`shipgen` or the designer. A hull model is simplified the same way each time;
the variation came from the cuts after it.

This holds for one build on one machine. Another version of Qt or of the tool
may give a different, equally repeatable, result.

The game itself still reads a ship's parts in a different order each session.
Where two room voids overlap, either may answer for gravity, which is why a
door's void has the same gravity as the rooms beside it. Hull voids do not take
part in that choice.

## 2 November 2026: sitting or standing, desk height, and fire control

* **Fire control** can run any of the ten modules the game's own station dialog
  offers, chosen on the Ship page. It was always an Energy Weapon Bay.
* **Integrated desks** can be for sitting or standing, at a height you choose,
  for the whole ship (Build page). Standing desks have no stool and are 4.2 ft
  high unless you say otherwise. **Known**: not yet tried in the game. The
  game's poses are fixed, so at unusual heights the hands may not meet the desk.

## 1 November 2026: after the second try in the game

* **Consoles.** The screen's image was mirrored: its lower left corner was on
  the operator's right, where every station of the Atlantis has it on the left.
  Desks and screens are bigger, nearer the proportions of the Atlantis's stations
  (a screen 3.4 ft wide, 1.9 ft tall), and the seat is further back: the screen's
  middle is 2.4 ft from it, up from 1.5 ft. They take more room: the battleship's
  17 ft deep bridge now holds six desks, and its seventh station is a swivel chair. The operator now sits fixed facing the
  screen, as in those stations, and can still look around.
* **Doorways turned people round.** A door's void had no gravity of its own, so
  it defaulted to natural gravity: near a planet that pulls towards the planet,
  whichever way the ship lies. Doorways now have the rooms' gravity.
* **Crew stuck at ladders.** The game counts a climber as arrived when their
  centre of gravity is within a foot of the ladder's end. Ladders ended at the
  floor, about 3 ft below that, so crew stood at the foot for ever. Ladders and
  walks now run 3.25 ft above the floor, as the Atlantis's ladders do, and a
  ladder goes on level to over the floor beside the hole, so nobody lets go
  over it. In the test battleship, routes that can be followed went from 80 of
  240 to all 240.
* **Phantom walls and ledges.** The space out to a flat window pane, and a rim
  round a join between decks, looked open but could not be walked into.
* **`shipgen crew-check`** follows every room-to-room route with the game's own
  route finding, and reports natural gravity in doorways and mirrored screens.
* **Known**: all this is measured from the built blueprint and the game's code;
  not yet tried in the game. Coming down a ladder from the top, crew take hold of
  its level piece and so may lie level for a moment.

## 1 November 2026: after trying the battleship in the game

* **Hull and bay door rows split up.** A door is stored from whichever cell was
  clicked, and only doors stored from the same side joined, so a row of ten
  came out as seven doors. They now join however they are named.
* **Door rows are the player's choice.** Dragging along a wall makes one door,
  a click makes a separate one, and dragging over doors joins them.
* **Crew could not get around.** Ladders stood at a cell's middle and walking
  paths ended in a triangle of it, so the two never met, and zero-G shafts had
  no path at all. A battleship's paths were 14 separate pieces; now they are
  one, and the build warns when they are not. Paths no longer lead into rooms
  with no floor.
* **Holes in shaft walls.** Joining two rooms across decks removed a foot of
  wall on each side of the join, wherever the join ran along a wall, and that
  showed the inside of the ship. The linings of these cuts are now textured too.
* **Windows.** Joined windows on a zigzag wall are one flat pane with no frame
  across the view (optional). Panes have no hull paint, as in the game's own
  designs. Frames always take a painted hull colour and end at the hull surface.
* **Ceiling windows** join into one pane whatever order the cells were clicked in.
* **Desks** are lower and slimmer, with a steeper screen further from the seat,
  painted in the plan's colours. The screen stands clear of the surface. The
  captain sits on the centre line behind the rows, and desks have wider gaps.
* **Textures** per room (walls, floor, ceiling) and per door, from the game's library.
* **Known**: tested by measuring the built blueprints; not yet tried in the game.
  The seated eye height (about 3.3 ft) is judged from a screenshot. A pane can
  stand up to about 2 ft proud of a deep notch in the hull.

## 1 November 2026: open rooms, contrail markers, parking outline

* **Open to Deck Below** now does what the name says. It was only a name
  before: the room got a floor like any other. The room below now reaches up
  into it wherever the two overlap, with gravity, so people fall to the lower
  floor (`docs/DESIGNER.md`). With no room under it the floor stays and the
  build warns.
* **Contrail emitters** are drawn where they end up on the hull, as a ball
  with an arrow the way the trail goes, in the plan and in the 3D view.
* **Parking spots**: an outline of the spot follows the mouse, coloured by
  whether it fits, and R turns it. The orientation chosen is now the one
  placed; the designer no longer tries the other one by itself.
* **Ceiling windows (experimental)**: a new room tool cuts a window straight
  up through the hull from a cell with no room above it.
* **Bridge styles**: a per-plan choice on the Build page. *Hazeron default*
  is unchanged; *Integrated desks (experimental)* puts stations on desks in
  curved rows round the captain.
* **Known**: the 3D view draws the contrail marker through the hull when it
  is on the far side. Parking spots turn in quarter turns only.
* **Known**: none of the experimental features has been tried in the game. In
  particular the screens on desks may show mirrored, the desks do not block
  walking, and a bridge over several decks is not staggered yet.

## 1 November 2026: a Mac download, built on GitHub

* **`.github/workflows/macos.yml`** builds a dmg on GitHub's Macs, for Apple
  and Intel processors, for every tag or by hand for an older one, and adds
  it to the release. `packaging/build-macos-app.sh` makes the app (Qt inside
  it, signed ad hoc, checked to need nothing from the build machine, and
  refusing to pack the private building designs).
* **Fixed on the first runs**: libraries `macdeployqt` leaves behind are brought
  in, because without brotli's partner the designer would not open on Apple's
  processors. An app that does not start is no longer published. Walk paths
  reported as blocked on those processors went away with `-ffp-contract=off`.
* **Known**: both builds pass the tests and start on GitHub's Macs, but the app
  has not been tried by a person on a real Mac.

## 1 November 2026: release 0.3.5-alpha

* **Version 0.3.5-alpha** (`src/version.h`, `RELEASE_NOTES.md`): everything
  below from 1 November, and the macOS build from upstream. There was no
  0.3.4.
* **Private building designs stay out of the repository and the downloads**;
  the program builds with or without them (`docs/AETHER_KIT.md`).

## 1 November 2026: zero-G shafts, the designer's task bar

* **Zero-G shafts**: the room type *Gravity Well* is now called *Zero-G
  Shaft* (old plans still open) and actually is one: its room void has no
  gravity (`G_Zero`). Zero-G shafts stacked on neighbouring decks are
  joined into one space: the ceiling and floor between them are cut away
  and a zero-G void fills the structure (`Zero-G joins` in the build
  report; `shipgen parts` marks such voids `zero-G`). Checked with the
  engine's own floor test; not yet tried in the game.
* **Redraw while drawing can be turned off** in the designer (bottom right,
  or the View menu; F5 redraws). The hull is now shaped in the background,
  so the window no longer freezes while it is.
* **A task bar** at the bottom of the designer replaces the progress window:
  the task, the numbered step, a progress bar with a percentage where the
  step can be counted, the seconds so far, and Stop when reading a model.
  Shaping a hull from a model now reports its steps too (reading and
  slicing the model when a plan is opened, filling the grid, finding the
  surface, placing the corners), and slicing reports every 1% instead of
  every 200,000 faces.
* **Paint reaches the whole model hull**: with the model as the hull (and
  the angular hull), a face took the paint of the plan triangle it lies in;
  the surface lies partly in triangles next to the drawn hull (the skin, a
  deck's top, model parts the grid missed, the grid's edge), which cannot be
  painted, so those faces kept the base coat. They now take the paint of the
  nearest hull triangle, straight out along x, y or z first so colour edges
  carry straight on (`HullPaintNear`). The LOGH battleship's four engine
  nozzles were speckled with base coat; now they are wholly painted. The
  paint edges are kept when the hull is simplified, so the hull comes out a
  little different (its volume by 2.5%).
* **macOS** (from upstream, pull request #1): native build with Homebrew Qt 5
  and the GUI tests on macOS.
* **A plan from a model is shaped once, not twice**: changing tabs shaped
  the hull again although nothing had changed. The Cyclops at 400 ft as its
  own hull: 314 s to 170 s, most of it taking the slivers out of 3.3
  million faces (about 100 s; now shown as its own step).

## 1 November 2026: the building generator

* **The building generator** fits a power plant's generator module, lays
  out fields, places medic and teleporter stations, and spacecraft berths
  (new brief field `spacecraftParking`). It also says why a building does
  not suit its industry, in the engine's words. See `docs/AETHER_KIT.md`.
* **Battery space works**: the building generator fits the battery module
  whenever a brief asks for battery space (before, the space was allocated
  with no module).
* **The stand-in industry table** has rows for more building types
  (guesses). The smelter now needs a workshop and cargo instead of a
  generator.
* **`SHIPGEN_DUMP_KIT=<dir>`** writes each kit blueprint's outside as an
  .obj file, for viewing (debugging only).

## 31 October 2026: release 0.3.3-alpha, hull styles

* **Version 0.3.3-alpha** (`src/version.h`, `RELEASE_NOTES.md`).
* **Hull styles** (`hullStyle` in the plan, *Hull style* on the Hull page:
  `smooth`, the default and the hull as before; `angular`; `classic`). The
  user found the smooth hull "bloated and crumpled" and asked for two other
  ways: the look of the old Shores of Hazeron designer, and one that "tries
  to make contiguous triangles as much as possible, as long as the slope
  does not change".
  * **Classic**: the prisms of the drawn triangles, not sloped, subdivided
    or smoothed, grown by the skin with mitred corners (`OffsetMitred`:
    each vertex moved to where the planes of its faces, each pushed out by
    the skin, meet, by least squares). The old designer's leftovers in the
    OpenShores client show how it worked (no converter or "hull wall
    skinner" code survives): a grid of 5.66 ft (the square root of 32)
    with diagonals to the centre, 8 ft decks (`DeckHeight`, `audef.h`), and
    room types for the skinner, roof pieces (gable, hip, round) and *open
    to the deck below* (`ROOMTYPE`, `audef.h`); its textures are in
    `resources/textures/ships/data/txSpacecraft0Lit.png`.
  * **Angular** (`AngularHullMesh`, `src/planbuild.cpp`): made from a field
    rather than from the prisms. Each deck gets the signed distance to its
    drawn outline in the plan; the hull is each deck's outline from floor
    to ceiling, and between two decks next to each other that both have
    hull, the outline blended from one to the other over each of the two
    decks' height (the ledge rises from the lower deck's edge to the top of
    the deck above, the underside falls to the bottom of the deck below, as
    `SlopeBetweenDecks`). Two straight edges, one above the other, blend
    into a plane. The surface is taken from a grid of cubes a quarter of a
    cell across, every corner put where the planes round it meet (dual
    contouring), and the flat areas merged. What made it work, each found
    on the user's X-wing or a test bar moving over a cell each deck:
    * The first version (the prisms, sloped by `SlopeBetweenDecks` and
      grown by `OffsetMitred`) came out as a sawtooth.
    * Each deck blended with the next deck's distance crumpled where one
      deck's edge lies over the other's inner edge (a strut moving over by
      its own width each deck: the two distances are equal and opposite).
      The blend is with the two decks' outline seen together, and in the
      part that is only the other deck, measured straight away from this
      deck to where the outline ends (the slope's width there; the nearest
      edge was the bar's end, and the slope came out as stairs).
    * Round distances twisted the slopes at corners; the distance is in
      the octagon norm (the plan's edges go only along the grid and its
      diagonals), so it is flat in pieces everywhere and corners come out
      cut off at 45 degrees.
    * Slopes only between edges within a cell and a half; further apart,
      the hull steps (without a limit, every part of a deck blended with
      every part of the next: curved fillets between a wing and the body).
    * The skin is taken off the field (grown afterwards by `OffsetMitred`,
      the many nearly flat faces crumpled it), and above or below a deck
      only where no slope goes on from it (a deck's box grown up by the
      skin stood out of the slope as a step at every deck).
    * An empty deck between two others stays a gap (the user: such gaps are
      meant).
  * The X-wing: 3,118 faces. Plans made from models draw shallow angles as
    staircases, which the angular style keeps (vertical fluting): a known
    limit; straightening such outlines is a possible next step.
* **The model as the hull, sharp** (`"sharp": true` in the plan's
  `"model"`, *Model hull* on the Hull page; `ModelHullMesh` with the
  model's triangles). The smooth model hull (blurred cube grid, surface
  nets) lost the shape of the user's reference models. Sharp leaves the
  grid unblurred and puts each corner of the cube surface where the planes
  round it meet: where the solid ends at the model, the model's own
  triangle, found by casting the edge between two cube centres through
  the model (looked for a cube and a half either way, as the sliced solid
  is not exactly the model), with its normal; where it ends at a room's
  wall, the wall found by halving along the grid. A corner may move up to
  one and a half cubes (half a cube left steps where the sliced solid and
  the model differ). Unlike drawing each point onto the nearest point of
  the model (tried before, crumpled), the planes of all the crossings
  round a corner are fitted together. On the user's Lantean Daedalus,
  4,178 faces, closed, and recognisably the model; Oltenia and the Vaygr
  battlecruiser follow their models closely; the Helios (thousands of
  loose overlapping panels) stays over the budget. New plans from a model
  are sharp; plans saved before keep smooth. The choice is the user's,
  apart from the hull style (the user: "keep the choice between angular
  style, model-as-hull, etc a user choice").
* **Hull doors** (`hullDoorExit` in the plan, *Hull doors* on the Rooms
  page): `auto` (a ramp or a ladder down, as before), `ladder` or `none`
  (a docking door: no way down and no clear space below). The constant
  hull void outside a hull door ends at the hull's real surface, found by
  sight lines out through the hull at three heights (before, at the
  boundary plus the skin, which on a sloped or model hull was inside or
  well outside it), and the clear space below reaches only as low as the
  ladder needs, stopping above any hull under the door (`AddHullDoor`'s
  `clearBelowZ`).
* **The game hung loading the Omega** (the user, OpenShores in Steam
  through Proton; the game sat idle, not busy). Its lowest decks were
  empty, so the hull began 17 ft above the design's origin, which the
  engine reports as "CG outside design extents below" (the origin has to be
  within the hull's extents on every axis). The builder now moves the
  design so that it is (`OriginShift`), straight after turning it to the
  game's axes and before the hull is measured: the engine measures the
  volume on a grid from the origin (the Omega: 17,817 cubic meters, 18,320
  moved), and the FTL drive and capacitor are sized by it. Moved at the
  end instead, the Omega's FTL drive and capacitor came out too small
  (errors the CG error had hidden, as it stops the engine's detailed
  analysis). The Omega now builds with no errors.
* **Hull edges with four faces**: where drawn cells met only at a corner,
  or a triangle met the one across its edge only on the deck above, two
  parts of the hull shared an edge (76 on the Omega). `ManifoldHull` fills
  such gaps (only adds, so every drawn triangle stays inside) and says how
  many triangles it added. Open edges after the door and window cuts are
  normal (the example has about 700) and were not the problem.
* `test.sh`: `testdata/hull_edge_cases.py` gives the example an empty deck
  below and a diagonal corner on top; it is moved, mended and accepted.
* `test.sh`: the model hull built sharp and smooth (206 checks).

## 31 October 2026: release 0.3.2-alpha, and a Windows build

* **Version 0.3.2-alpha** (`src/version.h`, `RELEASE_NOTES.md`): the
  sliced, smooth model hull and its simplification, flips, emitters, the
  room dialog and tooltips, all below.
* **The AppImage is built on Ubuntu 22.04** in a container
  (`packaging/build-appimage-portable.sh`), so it runs with glibc 2.35 and
  newer instead of only Fedora 44's. Tried in a bare Ubuntu 22.04
  container: the designer builds the example plan and `gui` starts.
  `package-appimage.sh` takes `BIN` and `OUT` and finds Qt's plugins with
  `qmake` too.
* **A Windows build, made on Linux**: `packaging/build-windows-cross.sh`
  (MinGW-w64 and Fedora's MinGW Qt 5 in a container; `build.sh` takes
  `CROSS=x86_64-w64-mingw32`) makes a zip with the four programs, 23 DLLs
  and the Qt plugins. Under Wine 11 the command line builds the example
  plan with 0 errors (the Linux `shipgen validate` agrees) and the designer
  opens, shapes and builds it. **Not yet run on Windows itself; the 3D view
  under Wine was not looked at** (screenshots do not show it).
* For the Windows compilers: `planbuild.cpp` and `interior.cpp` no longer
  use `IN`, `OUT`, `near`, `far` or `VOID` as names (the Windows headers
  define them as macros), and the engine's MSVC-only `operator[]` block in
  `third_party/soh/aumath/auscface.h` is kept to MSVC.

## 31 October 2026, after 0.3.1-alpha: flips, emitters and the designer's controls

* **Turning and flipping** (the user: a model may come in backwards). The
  whole ship turned around, mirrored or turned upside down, or one deck
  turned around or mirrored (Ship page and Edit menu; `shipgen
  plan-flip`). The model hull follows (`PlanModel` mirrors, saved in the
  plan). `test.sh`: the example turned around keeps its rooms, doors,
  windows, ladder and stations, and each flip done twice gives the same
  plan.
* **Contrail and weapon emitters** (the user missed them): two new room
  tools. A contrail emitter (`AuScContrailEmitter`, an icon facing aft)
  goes on the hull straight aft of the triangle clicked; a weapon emitter
  is the emit location of a fire control station (`SetEmitLoc`, as the
  game's designer sets it), one per station. Saved in the plan as
  `"emitters"`.
* **Adding a room** asks for its type and name in a dialog; before, the
  type box both chose the new room's type and changed the room picked in
  the list.
* **The 3D view** draws only the nearest layer of the see-through hull
  (its depth first): its far side showed through its near side.
* **Tooltips** on every control, and on each room tool.

## 31 October 2026, after 0.3.1-alpha: the model import slices

* **The model is sliced** (the user: complex models lost a lot of
  geometry; their idea, slices per deck). The inside of each thin slab is
  found by flooding the outside in round the marked surface
  (`SliceSolid`), so open panels, overlapping parts and missing tops
  count, and no repair or thinning is needed. The cause of the loss was
  that only closed pieces counted: the Nautilus has 47 percent of its
  faces in closed pieces, the Cyclops 65, and the thinning of big models
  opened more. The Truman (4 million faces): 111,503 hull triangles
  instead of 27, in a fifth of the time. Table in `DESIGNER.md`.
* **Decks and slices per deck** in the dialog and `--decks`, `--slices`,
  `--method slices|solid`, `--seal` on the command line; the recommended
  length also names its decks.
* `test.sh`: an open model (`testdata/openpanels.obj`, the test box as
  five panels with gaps and no top) fills like the closed box; the decks
  asked for set the length.
* **The hull is simplified to the face budget** (the user: much less
  complex geometry). `DecimateHull`: quadric-ordered edge collapses that
  keep the hull closed, the paint edges, and a wall round every room
  (`DESIGNER.md`, step 6). The Cyclops 52,948 faces to 5,000, the Nautilus
  21,128 to 5,000, both closed. `test.sh`: an ellipsoid (`testdata/
  ellipsoid.obj`) with rooms right up to its hull (`testdata/add_rooms.py`)
  comes down to 5,000 faces with every room inside.
* **The move limit grows with the ship**: when nothing is left to collapse
  within 2 ft, the limit grows up to 1 percent of the longest side. The
  Truman: 22,578 faces to 12,388.
* **The import is tidied**: specks under 12 triangles go, pinholes are
  filled (`--min-piece`, `--no-pinholes`). The Truman: 7,480 faces.
* **The model as the hull** (default for a new import; `--model-hull`,
  `--grid-hull`): the plan remembers its model and the hull follows the
  model's own slopes instead of deck steps. `test.sh` checks the open test
  box's hull is closed and built from the model.
* **The model hull is smooth** (the user: the Omega destroyer came out
  jagged). The cube grid is blurred and its surface's corners moved onto
  the half level (surface nets) instead of stepping; only the rooms (grown
  by the skin) are added to the model, not every drawn triangle, whose
  zigzag edges stood out as teeth. The Cyclops now reaches the budget
  (5,000 faces, was 7,908), in 7.9 s instead of 11.5.
* **Flat panels** (experimental, off): `--panels <n>` and *Flat panels*.
  Crumpled rather than clean plates for now; see `DESIGNER.md`.
* **Found:** triangles thinner than 0.05 make the engine's door and window
  cutting give a different hull from run to run; the limit stays at 0.05.
* **Known:** a detailed model loses much of its detail at 5,000 faces (a
  higher face budget keeps more), and the model is read again for each
  build in a new process (the designer keeps it).
* **Known:** reading a big obj is slow (66 s for the Truman's 451 MB).

## 31 October 2026, after 0.3.1-alpha: repair and length recommendation

* **The model import repairs like the mesh tool** (the user): `RepairMesh`
  (clean, windings, close holes) before and after thinning, kept only when
  it leaves fewer broken edges. The Warlock: 176 hull triangles instead of
  124; the Moogle is unchanged (the repair after thinning made it worse,
  so it was not kept, and the notes say so).
* **A recommended length** (the user: keep as much of the geometry as
  possible): how much of the model the hull holds is measured by points
  through its inside; below 85 percent longer lengths are tried and the
  shortest that holds 85 percent (or ten points more) is recommended, with
  a button in the designer to import again at it. The summary no longer
  gives a share of the volume (overfilled cells made it read over 100
  percent).

## 31 October 2026: release 0.3.1-alpha (bug fixes)

* **The model import kept no plan** (the user, with a Babylon 5 Warlock
  model: it "stopped loading when finishing the decks"). The import itself
  was fine (2 s); closing the designer's progress window emitted its
  `canceled` signal, which the designer took as Stop, so it discarded every
  finished plan. Stop now only counts while the work runs, and a finished
  plan is always kept. New `test.sh` check: the designer's own import
  (`--from-model`) keeps its plan.
* **Feedback:** a failed import says why in a message box, naming the
  file; a made plan with worries (not closed, many pieces, little came
  through) says so; the summary gives the share of the model's volume the
  hull triangles hold, with a hint below 40 percent.
* Version 0.3.1-alpha (`src/version.h`, `RELEASE_NOTES.md`).

## 31 October 2026: release 0.3.0-alpha

The first release on GitHub, published by the user as a pre-release
(<https://github.com/crazykristi/openshores-designer/releases/tag/v0.3.0-alpha>): an AppImage of the triangle designer with `shipgen-gui` and
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
