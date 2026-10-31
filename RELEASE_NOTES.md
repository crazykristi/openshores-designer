# Shipgen Designer 0.3.2-alpha

The triangle designer for Shores of Hazeron and
OpenShores: draw a ship deck by deck with triangles, draw its rooms, doors
and windows on the same triangles, paint it, and build a blueprint the game
loads. The AppImage also holds `shipgen-gui` (the automatic generator's
window) and the `shipgen` command line.

0.3.2-alpha is a pre-release. Most of it is about ships made from 3D
models, which came out jagged or lost much of their shape in 0.3.1-alpha.

## New in 0.3.2-alpha

* **3D models keep their shape.** The model is cut into slices per deck
  and the inside of each slice is filled in, so open panels, overlapping
  parts and missing tops count too (before, only closed pieces did, and
  complex models lost a lot). Choose the decks and slices per deck in the
  import dialog.
* **The model is the hull, and it is smooth.** A new import keeps its
  model, and the hull follows the model's own slopes instead of stepping
  from deck to deck; jagged edges are gone. Every room keeps a wall
  between it and the outside.
* **The hull is simplified to the face budget** (5,000 faces by default),
  staying closed and keeping its paint edges. Tiny specks of the model are
  dropped and pinholes are filled.
* **A recommended length**: when the hull holds too little of the model,
  the import suggests a longer ship and offers to import again at it.
* **Turn and flip**, for a model that came in backwards or upside down:
  turn the whole ship around, mirror it or turn it upside down, or turn
  around or mirror one deck (*Ship* page and *Edit* menu).
* **Contrail and weapon emitters**: two new room tools. Click a hull
  triangle to put a contrail behind it (straight aft, at the hull's back),
  or the spot weapons fire from (one per fire control station).
* **Adding a room** asks for its type and name in a small dialog; before,
  the type box also changed the room picked in the list.
* **Tooltips** on every control explain what it does.
* **The 3D view** no longer shows the far side of the see-through hull
  through its near side.

## Running it

```
chmod +x shipgen-designer-0.3.2-alpha-x86_64.AppImage
./shipgen-designer-0.3.2-alpha-x86_64.AppImage                 # the designer
./shipgen-designer-0.3.2-alpha-x86_64.AppImage gui             # shipgen-gui
./shipgen-designer-0.3.2-alpha-x86_64.AppImage shipgen --help  # the command line
```

Needs: 64-bit Linux from about 2022 on (built on Ubuntu 22.04: glibc 2.35
or newer, so Ubuntu 22.04, Debian 12, Fedora 36 and later), and OpenGL 3.3
for the 3D view. Blueprints go in the game's design folder like any other.

**Windows (new, experimental):** unpack
`shipgen-designer-0.3.2-alpha-windows-x86_64.zip` and start
`shipgen-designer.exe` in the folder (`shipgen-gui.exe`, `shipgen-mesh.exe`
and `shipgen.exe` are beside it). 64-bit Windows 10 or 11. It is built on
Linux and was tried under Wine only, so please report what goes wrong on
Windows itself. Windows may warn about an unknown publisher (the programs
are not signed): *More info*, then *Run anyway*.

## What the designer does

* **Hull:** triangles on a grid of cells, deck by deck, shaped into a
  smoothed, closed hull within a face budget. New: *Slope the hull between
  decks*, so a narrower deck rises from the one below instead of stepping
  in (the drawn triangles are always the least the hull may be).
* **Rooms** of the engine's own types, **doors** (neighbouring door edges
  make one wide bay door, as tall as the room), **hull doors** with a
  ramp down to the ground (or a ladder, for doors over 15 ft up),
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
  3DS or OBJ file into decks and makes the model itself the hull, smoothed
  and simplified to the face budget, with a progress window.
* **Turn and flip** the ship or one deck; **contrail and weapon
  emitters**.
* Feet or meters (*View > Metric units*).

## Not yet tried in the game

Ramps at hull doors, stairs, the sloped hull, joined windows, the new
interior look, the bay door switches, the smooth model hull and the
contrail and weapon emitters were made after the last in-game test.
Please report how they behave.

## Known limits

* Rooms are one deck high; hull doors are one deck high.
* On big round hulls, rooms far from the skin get no windows.
* A very detailed model loses fine detail at 5,000 faces; a higher face
  budget keeps more (the engine copes with about 15,000).
* Big models take a while: the Truman carrier (4 million faces) takes about
  two minutes and 2.5 GB of memory.

## Licence

Includes source from Shores of Hazeron, copyright Hazeron Freehold LLC,
under the Shores of Hazeron licence (`third_party/soh/LICENSE`).
