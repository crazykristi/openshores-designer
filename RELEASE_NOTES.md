# Shipgen Designer 0.3.1-alpha

The triangle designer for Shores of Hazeron and
OpenShores: draw a ship deck by deck with triangles, draw its rooms, doors
and windows on the same triangles, paint it, and build a blueprint the game
loads. The AppImage also holds `shipgen-gui` (the automatic generator's
window) and the `shipgen` command line.

0.3.1-alpha is a pre-release that fixes bugs in 0.3.0-alpha.

## Bug fixes in 0.3.1-alpha

* **New plan from a 3D model threw the plan away.** In 0.3.0-alpha the
  import ran to the end ("Filling in deck 3 of 3") and then nothing
  happened: closing the progress window counted as pressing its Stop
  button, so every finished plan was discarded. Fixed: Stop only counts
  while the import is still running, and a finished plan is always kept.
* **No word on why an import went wrong.** When a model could not be used,
  or came through only partly, the reasons were at most a line in the
  status bar. Now:
  * a plan that could not be made says why in a message box, naming the
    file;
  * a plan that was made but needs looking over (the model is not a closed
    surface, it is in many separate pieces, or little of it came through)
    says so in a message box;
  * the summary gives the share of the model's volume the hull triangles
    hold, and when it is low, suggests a longer length, a lower 'Inside at
    least' or smaller cells.

Also new: `shipgen-designer --from-model <model> [--length <ft>]` starts a
plan from a model from the command line (with `--screenshot` or `--export`
for scripts), which the test suite now uses to check the import.

## Running it

```
chmod +x shipgen-designer-0.3.1-alpha-x86_64.AppImage
./shipgen-designer-0.3.1-alpha-x86_64.AppImage                 # the designer
./shipgen-designer-0.3.1-alpha-x86_64.AppImage gui             # shipgen-gui
./shipgen-designer-0.3.1-alpha-x86_64.AppImage shipgen --help  # the command line
```

Needs: 64-bit Linux with a recent C library (glibc as new as Fedora 44's;
older distributions are not supported by this build), and OpenGL 3.3 for
the 3D view. Blueprints go in the game's design folder like any other.

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
* **From a 3D model:** *File > New plan from a 3D model* fills the hull in
  from an STL, 3DS or OBJ file, thinning big models first (half a million
  faces in about 9 seconds) with a progress window.
* Feet or meters (*View > Metric units*).

## Not yet tried in the game

Ramps at hull doors, stairs, the sloped hull, joined windows, the new
interior look and the bay door switches were made after the last in-game
test. Please report how they behave.

## Known limits

* Rooms are one deck high; hull doors are one deck high.
* On big round hulls, rooms far from the skin get no windows.
* Built on Fedora 44: it will not start on distributions with an older C
  library.

## Licence

Includes source from Shores of Hazeron, copyright Hazeron Freehold LLC,
under the Shores of Hazeron licence (`third_party/soh/LICENSE`).
