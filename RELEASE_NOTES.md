# Shipgen Designer 0.3.6-alpha

The triangle designer for Shores of Hazeron and OpenShores: draw a ship deck
by deck with triangles, draw its rooms, doors and windows on the same
triangles, paint it, and build a blueprint the game loads. The download
also holds `shipgen-gui` (the automatic generator's window) and the
`shipgen` command line.

0.3.6-alpha is a pre-release. It comes from trying a large ship in the game:
crew can now get around it, doors and door rows behave, and bridges can have
real desks, for sitting or standing. There is also a Mac download now.

## New in 0.3.6-alpha

* **Integrated desks (experimental).** A choice on the Build page: stations on
  desks in curved rows round the captain instead of the game's swivel chairs.
  For the whole ship, operators can sit or stand, and you can set the desk's
  height.
* **Fire control** can run any of the ten modules the game's own station dialog
  offers, chosen on the Ship page.
* **Open to Deck Below** now does what it says: the room below reaches up into
  it, and people fall to the lower floor.
* **Ceiling windows (experimental):** a new room tool cuts a window straight up
  through the hull.
* **Textures per room and per door**, from the game's library.
* **Door rows are your choice:** dragging along a wall makes one wide door, and
  a click makes a separate one.
* **Parking spots** show an outline that follows the mouse, coloured by whether
  the spot fits, and R turns it. **Contrail emitters** are drawn where they end
  up on the hull.
* **`shipgen crew-check`** follows every room-to-room route with the game's own
  route finding, and reports natural gravity in doorways and mirrored screens.
  **`shipgen sight`** shows what is seen along a line of sight.
* **A Mac download**, for Apple and Intel processors, built on GitHub. It starts
  there and passes the tests, but nobody has tried it on a real Mac yet, so
  please report how it goes.

## Bug fixes

* **Crew stuck at ladders.** Ladders ended too close to the floor for the game
  to count the climb as finished. Crew routes on the test battleship that can
  be followed went from 80 of 240 to all 240.
* **Crew could not get around big ships.** Walking paths and ladders did not
  meet, and zero-G shafts had no path. The build now warns when a ship's paths
  fall into separate parts.
* **Doorways turned people round near a planet.** They now have the rooms'
  gravity.
* **Mirrored console screens.** The image on an integrated desk's screen is now
  the right way round.
* **Hull and bay door rows split up:** a row of ten could come out as seven
  doors.
* **Holes in shaft walls** where rooms join across decks, and **phantom walls
  and ledges** at flat window panes and joins between decks.
* **Windows:** joined windows on a zigzag wall are one flat pane, panes no
  longer take hull paint, and ceiling windows join whatever order the cells were
  clicked in.
* **The same plan now builds the same blueprint.** Two builds of one plan used
  to differ.

## Running it

**Linux:**

```
chmod +x shipgen-designer-0.3.6-alpha-x86_64.AppImage
./shipgen-designer-0.3.6-alpha-x86_64.AppImage                 # the designer
./shipgen-designer-0.3.6-alpha-x86_64.AppImage gui             # shipgen-gui
./shipgen-designer-0.3.6-alpha-x86_64.AppImage shipgen --help  # the command line
```

Needs: 64-bit Linux from about 2022 on (built on Ubuntu 22.04: glibc 2.35
or newer, so Ubuntu 22.04, Debian 12, Fedora 36 and later), and OpenGL 3.3
for the 3D view. Blueprints go in the game's design folder like any other.

**Windows (experimental):** unpack
`shipgen-designer-0.3.6-alpha-windows-x86_64.zip` and start
`shipgen-designer.exe` in the folder (`shipgen-gui.exe`, `shipgen-mesh.exe`
and `shipgen.exe` are beside it). 64-bit Windows 10 or 11. It is built on
Linux and tried under Wine, so please report what goes wrong on Windows
itself. Windows may warn about an unknown publisher (the programs are not
signed): *More info*, then *Run anyway*.

**macOS (experimental):** use `shipgen-designer-0.3.6-alpha-macos-arm64.dmg`
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

A test battleship was tried in the game for this release, with sitting and with
standing desks, and looked good. Everything else has only been measured from the
built blueprints and the game's code. New in this release and not yet tried in
the game: fire control modules other than the
Energy Weapon Bay, Open to Deck Below and ceiling windows. From before and still
untried: zero-G shafts, the paint fix on model hulls, the new parts of generated
buildings, ramps at hull doors, stairs, the sloped hull, the smooth and sharp
model hulls, the angular and classic hull styles, the hull door choices and the
contrail and weapon emitters. Please report how they behave.

## Known limits

* Rooms are one deck high; hull doors are one deck high.
* The way down from hull doors is one choice for the whole ship.
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
