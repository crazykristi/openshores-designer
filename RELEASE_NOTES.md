# Shipgen Designer 0.3.7-beta-rc2

The triangle designer for Shores of Hazeron and OpenShores: draw a ship deck
by deck with triangles, draw its rooms, doors and windows on the same
triangles, paint it, and build a blueprint the game loads. The download
also holds `shipgen-gui` (the automatic generator's window) and the
`shipgen` command line.

0.3.7-beta-rc2 is the second **release candidate** of the first beta: please
try it and report what goes wrong before the final 0.3.7. Since rc1 it adds
a finish for each colour, canopies for fighters, less geometry sticking out
of the hull, a check of what the pilot and captain see, more freedom in
placing door buttons, and a fix for large hull doors. It also brings
everything from rc1, all since 0.3.6-alpha: three rounds of testing in the
game (doors, features, liveries), a new Crew step for placing stations, beds
and walk paths yourself, a choice of what gets built, and a first,
experimental fighter mode.

**Problems?** If something goes wrong, please either contact the author
(Kristiana Moretti, `crazykristi` on GitHub) or report an issue at
<https://github.com/crazykristi/openshores-designer/issues>. A screenshot, the
plan file (`.json`) and what you expected help most.

## New since rc1

* **Door buttons go where you click** along the wall, no longer only at the
  middle of a cell edge.
* **`shipgen view-check`**: what the helm and the captain see from their
  seats (ahead, to the sides, up, down ahead): glass, hull, open sky, or what
  is in the way. A fighter's build report says how much of the view ahead the
  pilot has.
* `shipgen audit` lists parts that reach out of the hull.

## Experimental since rc1

* **A finish for each colour** (Paint step): transparency, self-lit,
  shininess, reflectivity, glow and the game's animated surface effects, with
  ready finishes (matte, satin metal, polished metal, tinted glass, light,
  energy field).
* **Canopy**: a ceiling window shape for a cockpit under a bubble of glass.
  The hull's own faces over the cockpit become its glass, so the canopy ends
  where the model's does; it may reach over the hull ahead of the cockpit.
* **A button outside a hull door**: click beyond the door's wall, beside the
  door, for a button on the hull that opens it from outside.

## Changes and fixes since rc1

* **Less sticks out of the hull**: room corners, frames and door linings are
  moved back inside where the hull curves in over them, and service panels
  sit side by side on one wall (in a fighter they were stacked and stuck out).
* **A large hull door could hang the build** and the designer with it (a 79 ft
  bay door on a curved hull); it builds in seconds now.

## New in rc1

* **A Crew step (tab 4)** for placing stations, berths, transporter pads and
  walk paths by hand. It is optional: anything you do not place is still put
  in automatically, and you can switch the automatic part off per kind. A
  station can have a swivel chair or an integrated desk; an outline shows
  green where it fits in one room, R turns it, and the mirror places the
  other side too.
* **Choose what gets built.** The Build step lists every kind of thing in
  the plan (doors, door buttons, windows, ladders, ramps, parking, emitters,
  stations, berths, pads, systems, walk paths, ...). Untick a kind, or single
  items in it, to leave them out of the blueprint without deleting them. It
  is saved with the plan.
* **Turbolifts** for people and vehicles (a room of type *Turbo Lift*).
* **Landing spots for spacecraft**, on a deck or on top of the hull.
* **The owner's empire flag and name** painted on the hull.
* **Door buttons**: after placing a door, click the walls beside it.
* **Windows between rooms and balconies**, each its own tool.
* **Windows that follow the hull**: the hull's own triangles become the
  glass, for model hulls. Now chosen per window (*Window frame* or *Follow
  the hull*) as well as for the whole ship.
* **Dragged window rows** that build as one window, and **ceiling windows**
  per triangle, following the hull or flat; they follow the brush size.
* **Liveries**: mark a colour to keep its edges sharp on a model or angular
  hull (stripes, insignia, pixel art).
* **Who may enter a room** (officers, crew, passengers, troops).
* **Several weapon bays**, each of its own type, and **ship-wide textures**
  for walls, floors, ceilings and doors.
* **Each hull door or row has its own way down** (ramp, ladder or none) and
  ramp length.
* **Quarter turns**: turn the whole ship or one deck 90° either way.
* **FBX, glTF, Collada, PLY, Blender and more** 3D models can be read (on
  Linux and macOS; the Windows build still reads STL, OBJ and 3DS).
* **A check on start**: when libraries the 3D view needs are missing, a clear
  warning with the issue tracker's address, and you may go on at your own
  risk.
* **Help > About** shows the version, author and build time.
* New commands: `shipgen walk`, `shipgen faces --where`, `plan-flip
  --clockwise/--anticlockwise`, `plan-from-model --fighter`.

## Experimental in rc1

Marked as experimental in the program as well; please try them in the game
before relying on them, and tell us how they behave.

* **Fighter mode.** *File > New fighter* or *New fighter from a 3D model*
  (or *Fighter mode* on the Ship page): small cells (2.5 ft) and low decks
  (4.5 ft), the model itself as the hull, one pilot at the helm, no FTL drive,
  windows that follow the hull for the canopy. Draw the cockpit, a hull door
  on its side and the canopy, and place the seat in the Crew step. Not there
  yet: a hatch in the belly, an opening canopy.
* **Turbolifts**, **landing spots**, **liveries**, **ceiling windows** and
  **windows that follow the hull** have not all been tried in every
  combination in the game yet.
* **Integrated desks** (from 0.3.6-alpha) and the **Windows** and **macOS**
  downloads (built and tested automatically, little tried on real machines).

## Changes in rc1

* The steps are now 1 Ship, 2 Hull, 3 Rooms, **4 Crew**, 5 Paint, 6 Build
  (`--step` counts that way).
* Room types now matter in the game: sick bays get the medic station,
  transporter rooms their transporter, and so on.
* Every ship has a cargo hold of at least 2% of its hull; up to 12
  transporter pads per room.
* The AppImage uses the system's own Wayland libraries, and asks for OpenGL
  3.3 where a driver only gives it on request.
* Small craft get a capacitor big enough for their idle power, and without an
  engineer station their power plant and drive stay under 100 m³, so the
  engine's two power warnings are gone.

## Bug fixes in rc1

* **The 3D view stayed empty in the AppImage on some PCs** (KDE Plasma on
  Wayland: "EGL not available").
* **Hull doors above the ground could not be reached from outside**, and
  ladders at hull doors could not be taken hold of from the ground.
* **Inside ladders threw you off at the top**; **a ledge and an invisible
  strip at the foot of ramps** are gone.
* **A door could not be taken away right after placing its first button.**
* **Swivel chairs could stick out of small rooms.**
* **Liveries and pixel art on rising roofs came out in pieces** (the MCRN
  logo).
* **Window frames ran through doorways**; windows on stepped walls blocked
  each other; the shaft of a hull-following ceiling window had no walls; door
  buttons showed through the wall.
* **Binary STL files with colours** could not be read.
* "Only while the door is open" ramps did not work in the game and are
  retired (old plans keep loading).

## Running it

**Linux:**

```
chmod +x shipgen-designer-0.3.7-beta-rc2-x86_64.AppImage
./shipgen-designer-0.3.7-beta-rc2-x86_64.AppImage                 # the designer
./shipgen-designer-0.3.7-beta-rc2-x86_64.AppImage gui             # shipgen-gui
./shipgen-designer-0.3.7-beta-rc2-x86_64.AppImage shipgen --help  # the command line
```

Needs: 64-bit Linux from about 2022 on (built on Ubuntu 22.04: glibc 2.35
or newer, so Ubuntu 22.04, Debian 12, Fedora 36 and later), and OpenGL 3.3
for the 3D view. Blueprints go in the game's design folder like any other.

**Windows (experimental):** unpack
`shipgen-designer-0.3.7-beta-rc2-windows-x86_64.zip` and start
`shipgen-designer.exe` in the folder (`shipgen-gui.exe`, `shipgen-mesh.exe`
and `shipgen.exe` are beside it). 64-bit Windows 10 or 11. It is built on
Linux and tried under Wine, so please report what goes wrong on Windows
itself. Windows may warn about an unknown publisher (the programs are not
signed): *More info*, then *Run anyway*.

**macOS (experimental):** use `shipgen-designer-0.3.7-beta-rc2-macos-arm64.dmg`
for Apple processors or `...-macos-x86_64.dmg` for Intel. The app is not signed
with an Apple Developer account, so macOS blocks it the first time: try to open
it, then choose *Open Anyway* under *System Settings > Privacy & Security*. The
dmg holds a note with the details. The dmgs are added a little after the other
downloads, once GitHub has built them.

## What the designer does

* **Hull:** triangles on a grid of cells, deck by deck, shaped into a
  closed hull within a face budget: smooth, angular or classic. *Slope the
  hull between decks* lets a smooth hull rise from a wider deck instead of
  stepping in (the drawn triangles are always the least the hull may be).
* **Rooms** of the engine's own types, **doors** (neighbouring door edges
  make one wide bay door, as tall as the room), **hull doors** with a
  ramp down to the ground (or a ladder, for doors over 15 ft up, or none),
  **ladders** and **stairs** between decks, **zero-G shafts**, **windows** on cell edges and
  diagonals (adjustable height, optionally joined into long windows),
  **vehicle parking spots**.
* **Inside:** walk paths for the crew, floors, walls and ceilings told
  apart (walls tinted by the room's colour), doors painted to stand out,
  swivel chairs facing the front, at the bridge's front windows.
* **Furnishing** from a ship template (stations, berths, systems sized to
  the hull, 3.5 times the maneuver drive), with shields, weapon bay and
  transporter optional.
* **Paint** in plain or empire colours.
* **From a 3D model:** *File > New plan from a 3D model* slices an STL,
  3DS or OBJ file into decks and makes the model itself the hull, sharp or
  smooth, simplified to the face budget, with its progress shown at the
  bottom of the window.
* **Turn and flip** the ship or one deck; **contrail and weapon
  emitters**.
* Feet or meters (*View > Metric units*).

## Tried in the game, and not yet

Tried in the game for this release: door rows, ramps and ladders at hull doors,
door buttons, windows that follow the hull, ceiling windows, liveries (the MCRN
logo, fixed after the test), a cockpit only 4 ft high. Not yet tried: the Crew
step's hand-placed items, leaving things out of the build, turbolifts, landing
spots, flags and names, quarter turns, FBX models, and fighter mode.

## Known limits

* Fighter mode: no belly hatch yet; wings and other thin parts stay on the
  hull but get no cells, so a side door over a wing gets a ladder that runs
  into it (a low door gets a ramp instead).

* Rooms are one deck high; hull doors are one deck high.
* Coming down a ladder from the top, crew may lie level for a moment.
* Desks do not block walking, and at unusual heights a seated or standing
  character's hands may not meet the desk. A bridge over several decks is not
  staggered yet.
* On big round hulls, rooms far from the skin get no windows.
* A very detailed model loses fine detail at 5,000 faces; a higher face
  budget keeps more (the engine copes with about 15,000). The model hull is
  built from cubes a quarter of a cell across, so details smaller than that
  (about 1.25 ft) go.
* The angular style follows the drawing exactly: an outline drawn as a
  staircase (as the import draws shallow angles) comes out as vertical
  fluting. Use the diagonals for slanted walls, or the model as the hull.
* Big models take a while: the Truman carrier (4 million faces) takes about
  two minutes and 2.5 GB of memory. A big model used as the hull also
  takes a while to shape (a 400 ft ship from a 95,000-face model: about
  three minutes, mostly tidying up the surface); the bar at the bottom
  shows where it is.

## Licence

Includes source from Shores of Hazeron, copyright Hazeron Freehold LLC,
under the Shores of Hazeron licence (`third_party/soh/LICENSE`).
