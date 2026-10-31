# Shipgen Designer 0.3.3-alpha

The triangle designer for Shores of Hazeron and
OpenShores: draw a ship deck by deck with triangles, draw its rooms, doors
and windows on the same triangles, paint it, and build a blueprint the game
loads. The AppImage also holds `shipgen-gui` (the automatic generator's
window) and the `shipgen` command line.

0.3.3-alpha is a pre-release. It is about how the hull looks: two new hull
styles for drawn ships, a sharp hull for ships made from 3D models, and a
fix for ships that hung the game.

## New in 0.3.3-alpha

* **Hull styles** (*Hull style* on the Hull page). Choose how the drawn
  triangles become the hull:
  * **Smooth**, as before: pushed out by the skin and rounded.
  * **Angular** (new, experimental): the hull slopes from deck to deck in
    straight, flat panels and keeps sharp edges. A slope that does not
    change comes out as one flat face, not a sawtooth, and corners are cut
    off at 45 degrees. Decks only slope into each other where their edges
    are within a cell and a half; further apart, the hull steps. An empty
    deck between two others stays a gap.
  * **Classic** (new): like the old Shores of Hazeron designer, straight
    walls up from each deck's outline and flat tops and bottoms.
* **Sharp hulls from 3D models** (*Model hull* on the Hull page, when the
  model is the hull). *Sharp* follows the model's own edges and flat faces,
  so the hull looks like the model; *Smooth* rounds it as in 0.3.2. New
  plans from a model start on Sharp; plans saved before keep Smooth.
* **Hull doors: choose the way down** (*Hull doors* on the Rooms page): a
  ramp or a ladder down (as before), always a ladder, or none (a docking
  door). The space kept clear outside a hull door now ends at the hull's
  real surface and reaches no lower than the way down needs.
* **Fixed: a ship whose lowest decks are empty hung the game** when it was
  loaded (its hull did not reach the design's origin, which the game
  needs). The design is now moved to fit, and the FTL drive and capacitor
  are sized after the move. Hull cells that met only at a corner are joined
  up too.

## Running it

```
chmod +x shipgen-designer-0.3.3-alpha-x86_64.AppImage
./shipgen-designer-0.3.3-alpha-x86_64.AppImage                 # the designer
./shipgen-designer-0.3.3-alpha-x86_64.AppImage gui             # shipgen-gui
./shipgen-designer-0.3.3-alpha-x86_64.AppImage shipgen --help  # the command line
```

Needs: 64-bit Linux from about 2022 on (built on Ubuntu 22.04: glibc 2.35
or newer, so Ubuntu 22.04, Debian 12, Fedora 36 and later), and OpenGL 3.3
for the 3D view. Blueprints go in the game's design folder like any other.

**Windows (new, experimental):** unpack
`shipgen-designer-0.3.3-alpha-windows-x86_64.zip` and start
`shipgen-designer.exe` in the folder (`shipgen-gui.exe`, `shipgen-mesh.exe`
and `shipgen.exe` are beside it). 64-bit Windows 10 or 11. It is built on
Linux and was tried under Wine only, so please report what goes wrong on
Windows itself. Windows may warn about an unknown publisher (the programs
are not signed): *More info*, then *Run anyway*.

## What the designer does

* **Hull:** triangles on a grid of cells, deck by deck, shaped into a
  closed hull within a face budget: smooth, angular or classic. *Slope the
  hull between decks* lets a smooth hull rise from a wider deck instead of
  stepping in (the drawn triangles are always the least the hull may be).
* **Rooms** of the engine's own types, **doors** (neighbouring door edges
  make one wide bay door, as tall as the room), **hull doors** with a
  ramp down to the ground (or a ladder, for doors over 15 ft up, or none),
  **ladders** and **stairs** between decks, **windows** on cell edges and
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
  smooth, simplified to the face budget, with a progress window.
* **Turn and flip** the ship or one deck; **contrail and weapon
  emitters**.
* Feet or meters (*View > Metric units*).

## Not yet tried in the game

Ramps at hull doors, stairs, the sloped hull, joined windows, the new
interior look, the bay door switches, the smooth and sharp model hulls,
the angular and classic hull styles, the hull door choices and the
contrail and weapon emitters were made after the last in-game test.
Please report how they behave.

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
  two minutes and 2.5 GB of memory.

## Licence

Includes source from Shores of Hazeron, copyright Hazeron Freehold LLC,
under the Shores of Hazeron licence (`third_party/soh/LICENSE`).
