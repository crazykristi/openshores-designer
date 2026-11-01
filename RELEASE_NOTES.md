# Shipgen Designer 0.3.5-alpha

The triangle designer for Shores of Hazeron and OpenShores: draw a ship deck
by deck with triangles, draw its rooms, doors and windows on the same
triangles, paint it, and build a blueprint the game loads. The download
also holds `shipgen-gui` (the automatic generator's window) and the
`shipgen` command line.

0.3.5-alpha is a pre-release. It makes the designer nicer to work with on
big ships, adds rooms without gravity, fixes paint on ships made from 3D
models, and improves the buildings the generator makes. It can also be
built on a Mac now.

## New in 0.3.5-alpha

* **Zero-G shafts.** A new room type (it was listed as *Gravity Well*
  before, but did nothing): a room without gravity, where your crew floats
  and can move up and down freely. Draw zero-G shafts on top of each other
  on neighbouring decks and they become one open shaft: the floor and
  ceiling between them are left out, so no ladder is needed. They work
  while the ship's gravity is on. Plans that used *Gravity Well* open as
  zero-G shafts.
* **See what the designer is doing.** A bar at the bottom of the window
  now shows anything that takes a while (reading a 3D model, shaping the
  hull, building the blueprint): what it is doing, which step it is on,
  how far along that step is, and how long it has taken. Reading a model
  can be stopped there. It replaces the old progress window.
* **Drawing no longer freezes on big ships.** The hull is shaped in the
  background while you keep drawing. And if redrawing the 3D view after
  every change is too slow for your ship, untick *Redraw 3D while drawing*
  (bottom right, or the View menu) and press **F5** when you want to see
  your changes. The designer remembers your choice.
* **Faster start from a 3D model.** A new plan from a model was shaped
  twice; now it is shaped once. On a large ship that roughly halves the
  wait.
* **Paint now covers the whole hull of ships made from 3D models.** Paint
  near the edges of the drawn hull did not always reach the model's
  surface, which left patches in the base colour (engine nozzles, for
  instance). The surface there now takes the colour of the nearest painted
  triangle. The angular hull style gets the same fix.
* **Better generated buildings** (`shipgen generate-building`): power
  plants get the generator for their fuel, and a building's brief can now
  ask for working battery space, fields for farms and orchards, medic and
  teleporter stations, and a landing berth for spacecraft
  (`spacecraftParking`). When a building does not suit its purpose, you are
  told why in the game's own words.
* **macOS** can now build and run the tools from source (with Qt 5 from
  Homebrew), thanks to a contribution from yamamushi. There is no ready
  Mac download yet.
* **Windows:** a build problem with the Windows version is fixed.

## Running it

**Linux:**

```
chmod +x shipgen-designer-0.3.5-alpha-x86_64.AppImage
./shipgen-designer-0.3.5-alpha-x86_64.AppImage                 # the designer
./shipgen-designer-0.3.5-alpha-x86_64.AppImage gui             # shipgen-gui
./shipgen-designer-0.3.5-alpha-x86_64.AppImage shipgen --help  # the command line
```

Needs: 64-bit Linux from about 2022 on (built on Ubuntu 22.04: glibc 2.35
or newer, so Ubuntu 22.04, Debian 12, Fedora 36 and later), and OpenGL 3.3
for the 3D view. Blueprints go in the game's design folder like any other.

**Windows (experimental):** unpack
`shipgen-designer-0.3.5-alpha-windows-x86_64.zip` and start
`shipgen-designer.exe` in the folder (`shipgen-gui.exe`, `shipgen-mesh.exe`
and `shipgen.exe` are beside it). 64-bit Windows 10 or 11. It is built on
Linux and tried under Wine, so please report what goes wrong on Windows
itself. Windows may warn about an unknown publisher (the programs are not
signed): *More info*, then *Run anyway*.

**macOS:** no download yet; build it from the source with `./build.sh`
(needs Homebrew's `qt@5`). See the README.

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

## Not yet tried in the game

New in this release and not yet tried in the game: zero-G shafts, the paint
fix on model hulls, and the new parts of generated buildings. From before and still
untried: ramps at hull doors, stairs, the sloped hull, joined windows, the
smooth and sharp model hulls, the angular and classic hull styles, the hull
door choices and the contrail and weapon emitters. Please report how they
behave.

## Known limits

* Rooms are one deck high; hull doors are one deck high.
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
