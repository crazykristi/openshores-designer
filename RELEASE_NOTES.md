# Shipgen Designer 0.3.7

The triangle designer for Shores of Hazeron and OpenShores: draw a ship deck
by deck with triangles, draw its rooms, doors and windows on the same
triangles, paint it, and build a blueprint the game loads.

0.3.7 is the first full release, after four release candidates. Since
0.3.7-beta-rc4-hf1 it adds expected stats for a chosen quality, a choice
between a gravity and a rocket maneuver drive, a building's site, tall doors,
walking room round curved hulls, surface effect previews and an MCP server so
that an AI assistant can design in the open window, with fixes to the crew's
walk paths, the blueprint size of big ships and the corner normals of the
built parts. Everything from the betas is below.

**Problems?** If something goes wrong, please either contact the author
(`crazykristi` on GitHub) or report an issue at
<https://github.com/crazykristi/openshores-designer/issues>. A screenshot, the
plan file (`.json`) and what you expected help most.

## Bug fixes since rc4-hf1

* **Crew could not walk between the pieces of a room.** A room drawn in
  separate pieces on one deck (cut in two by a ladder shaft, a lift or
  another room) had walk paths only in the piece holding its middle, so
  stairs, ladders and doors in the other pieces led nowhere. Each piece is
  now joined from its own middle. On a 21-deck land battleship this joined
  11 separate path networks into 2, and the crew routes that work went from
  235 to 704 of 812; the rest need a ladder the plan does not have, which
  the new warning below points out.
* **Walk paths no longer run through hull doors of rooms open to the deck
  below** (a vehicle bay's upper deck), where there is no floor to walk on.
* **Walk paths go straight across a room** instead of zigzagging from
  triangle to triangle, so a ship has a few hundred path parts instead of
  thousands.
* **Big ships went over the game's blueprint size limit** (28 MB). Rooms kept
  a corner at every cell along their floor and wall edges, so a long
  corridor had over 2,000 faces. Those corners are taken out where the edge
  runs straight; the rooms keep exactly their shape. A 600 m battleship went
  from 28.6 MB to under 8 MB.
* **The corner normals of the built parts pointed inwards.** The triangles
  themselves faced the right way (the game draws a triangle from the side
  its corners go round clockwise), but the normal kept at each corner was
  the opposite of the face's. Build blueprints again to get the fix.
* **Big angular hulls took very long to build** (one took over 12 minutes);
  the hull comes out the same, in a fraction of the time.
* **Turbo Lift was missing from the ship room types**; the room type list
  scrolls instead of reaching past the screen.
* **Effect faces got no effect texture coordinates**, so the game showed a
  single pixel of the effect picture.
* **The game's walking outlines of each part were saved from partway through
  the build**; they are now made at the very end, when every part is in
  place.

## New since rc4-hf1

* **Expected stats** (Build page): the game's own design report for a chosen
  quality, and the raw materials the build uses up, as a guide; the game can
  change, so check the design in the game before building it. Ships can set
  system volumes by hand, and the structure makes up the difference. A
  yellow box on the Build and Crew pages warns when the berths may be short
  for the stations. Also `shipgen stats`.
* **A choice of maneuver drive** (Ship page): a Gravity Drive Module, which
  runs on power, or a Rocket Drive Module, which burns fuel and needs no
  power. Opening a blueprint keeps its drive.
* **A building's site**: an area kind for buildings, the building's plot on
  every deck. Generated parking goes inside it; anything drawn outside it is
  shown in red and stops the build.
* **New ships** ask for the cell size, length and beam in cells, with the
  real size shown as you type. *New from a 3D model* links the length and
  the number of cells both ways, and warns when a ship is too long for the
  grid (400 cells).
* **Room borders**: bold two-tone lines between rooms, and between rooms and
  the outside, so two rooms of one colour can be told apart.
* **A compass in the 3D view**: fore, aft, port, starboard and up, turning
  with the camera. The *New* menu names say spaceship and building plainly.
* **Rooms the crew cannot reach** are a warning in the build report. The
  game's crew walk and climb; they never ride a turbo lift, so every deck
  needs ladders or stairs to the rest of the ship as well. A stair whose
  walk path cannot be joined to its room is a warning too.
* New commands: `shipgen path-check` (the separate networks of walk paths in
  a blueprint, and the rooms in each), `shipgen facing-check` (which way the
  hull triangles face, by the game's rule), `shipgen probe` (what stops
  someone standing at a point), `shipgen effects`, `shipgen stats`, and
  `shipgen inspect` now shows how much of the game's size limit each kind
  of part uses.

## Experimental since rc4-hf1

* **AI assistants in the designer (MCP).** `shipgen-designer --mcp` opens the
  designer as usual and is also an MCP server, so an AI app (Claude Desktop,
  Claude Code and others) can design in the open window while you watch. It
  has tools that work in cells and rooms by name: draw each deck's hull, add
  rooms, doors (also tall ones), windows, ladders and stairs, stations,
  berths, landing and vehicle spots and weapon emitters, paint the hull with
  colours, empire colours and the game's textures, check the plan, see a
  deck as a text map or a picture, build, expected stats, save and open.
  Every change is one undo step, and files stay in one folder
  (`--mcp-folder`). Redrawing the 3D view while drawing stays your choice.
  `shipgen plan-tool` runs the same tools on a plan file.
* **Tall doors**: a door into a room open to the deck below can reach up
  through the decks above, inside or in the hull, with one door the full
  height.
* **Walking room round the hull**: the game stops people at outlines of the
  hull that are bigger than its real shape, so they got stuck beside curved
  hulls, under wings and between engines. The build opens the ground up
  there (`build-plan --no-hull-clearance` leaves it out).
* **Surface effect previews**: the 3D view animates the game's surface
  effects, each only when the game would run it (shields up, weapons
  firing, FTL drives charged, ...), set under *View > Surface effects*.
  Effects that can never run on the design are tinted pink, with the reason.

## Hotfix 1 of rc4

* **The 3D view stayed empty with NVIDIA drivers on Windows**, with the
  message "The 3D view needs OpenGL 3.3" even though the driver gives 3.3 or
  newer. NVIDIA writes its OpenGL version with three numbers ("4.6.0
  NVIDIA ..."), which the check misread. Only the first two numbers are read
  now.

## New in rc4 (since rc3)

* **Updates from inside the designer**: *Help > Check for a new version...*
  shows the notes of a newer release and can download and install it (the
  AppImage is put next to the old one and started; on Windows the new zip is
  unpacked over the folder once the designer has closed; on macOS the disk
  image is downloaded and opened). The download is checked against the
  checksum GitHub gives. *Look for new versions at start* looks once a day
  and can be switched off in the Help menu. It works from this release on.
* **Drag and drop**: drop a plan (`.json`) or a blueprint (`.SoH`) on the
  window to open it, or an image on the Decals box to add it as a decal.
* **Open a blueprint as a plan**: *File > Open blueprint (.SoH)...* (and
  `shipgen plan-from-blueprint`) makes a plan from a blueprint, with its
  hull, rooms (also over several decks), doors, ladders and turbolifts.
* **Copy decks**: *Copy this deck...* stacks copies of a deck on top of it,
  with its rooms, doors, windows and the rest; *Copy the floor...* copies
  only the outline and paint. The Decks box is on the Hull page as well.
* **Fly through the 3D view** (*View*, F6): W A S D, Q and E, Shift for
  speed, drag to look round, Esc leaves.
* **Pictures of the game's textures** in the texture boxes, read from the
  game's own texture library when it is installed (*View > The game's
  texture pictures...*).
* **Turbolifts are checked while you draw**: a lift only works on one
  vertical shaft; the Rooms page marks shaft cells drawn wrong, and the build
  warns.
* **Which way a desk faces** shows on the Crew page (desk, screen edge, seat
  and an arrow), and in its tooltip.
* **Engineer stations placed by hand can run their own FTL drive.**
* **Window lining can be switched off** (Hull page).
* `shipgen window-check` counts where the sky shows through round windows;
  `shipgen decal` puts an image on any blueprint.
* **Building mode**: *File > New building...* (or from a 3D model) turns a
  drawn plan into a building blueprint with its site, industry and volumes,
  outdoor parking (also rocket and boat spots), ground areas (fields, lawns,
  paving, build zones) and trees, shrubs, lamps and posts.
* **Decals**: an image on the hull at a place you click in the 3D view, flat
  or wrapped round the hull, on one side or both, turned, mirrored, self-lit
  or see-through (at most 256 pixels). The Decals box is on the Build page.
* **Textured paint**: a colour on the Paint page can be one of the game's
  textures or an image of your own, wrapped per face, flat from above, the
  side or the front, round the length or round a ball, with the size of one
  repeat, a turn, and how much of the colour lies over it.
* **Doors in floors and ceilings**: two tools on the Rooms page; drag over
  cells for one door. A ladder can stand under it and is climbed while the
  door is open.
* **Doors that start open**: *Doors start* closed or open, per door row
  (right click). A door that starts open is a standard door, since an
  automatic one would close again.
* **Clearer hull windows**: windows that follow the hull now have a flat
  pane of glass the size of the window by default (upright or on the hull's
  slope), instead of the hull's own faces, which on a thinned model hull
  could be large shards reaching far past the window. *The hull's own faces*
  is still a choice, for the whole ship and per window.
* **Vehicle spots**: *Place vehicle spots* (Rooms page) puts in the game's
  vehicle spot for one vehicle (SUV, dirt bike, helicopter, space fighter,
  ...), drawn at the vehicle's size.
* **Space stations**: *Space station* on the first page; built in space,
  with no FTL drive.
* **Military weapons bases and shield bases**: two new building industries
  and templates. A weapons base gets a fire control station for every weapon
  bay (*Weapon bays* in the Building box, and one for each weapon emitter
  drawn on the Rooms page), with its bay volume, its weapon emitter on the
  hull and a battery for energy bays; what the bays are is the *Fire
  control* choice. A shield base gets its shields and a shield station.
  Sensor, shield, fire control, medic and transporter stations placed by
  hand on the Crew page are used in buildings as well.

## Experimental in rc4

* **A paint brush in the 3D view** (Paint page): paint straight on the hull
  with the colour or texture picked, a round or square brush of a size in
  feet. Only the first face in view under the brush is painted. The strokes
  are kept in the plan and stay where they were brushed when the hull is
  shaped again; *Rub out* goes back to the drawn paint.
* **Stairs and ramps in several versions** (Rooms page, the Stairs tool):
  stairs or a ramp, 1, 2 or 3 cells wide, steep, normal or gentle; the
  length and angle show under the choice, and a right click on a flight
  gives it the choice picked. A ramp is a smooth slope instead of steps.

## Changes and fixes in rc4

* **Maintenance consoles** no longer stand in doorways or stick out of the
  hull: they go only into an engine room, on a stretch of its own wall with
  no door or window, clear of ladders, stairs and furniture. A ship without
  an engine room gets none.
* **Zero-G shafts**: a ladder placed in a shaft is the crew's way up, its
  opening has no gravity any more, and a shaft without a ladder is a warning
  in the build report.
* **Holes round windows**: the game does not draw the hull from inside, so
  the sky showed through beside the glass. The hull seen from inside through
  a window is now covered (a *Window Lining* part).
* **Weapon bays chosen by hand** at a fire control station were overwritten
  by the weapon emitter's choice; the station's choice now wins.
* **Door buttons of long door rows**: buttons are found along the whole row,
  and outside buttons of hull doors are no longer left out where the wall
  lies on the hull.
* **Decals on buildings** reach the blueprint.
* **Generated parking no longer overlaps** the building, its berths or spots
  drawn by hand.
* **Building rules**: parks are ground only, platforms have no rooms and get
  a build zone, street lights a dimmed lamp; the build warns about known
  traps. New templates: power plant, farm, park, street light and three
  platforms.
* **Very big models** (over a thousand feet) no longer run out of memory when
  the hull is shaped.
* **The 3D view** shows images the right way up, and the hull no longer turns
  see-through after text or the brush outline is drawn over it.

## New in rc2 and rc3 (since rc1)

* **Door buttons go where you click** along the wall, no longer only at the
  middle of a cell edge.
* **`shipgen view-check`**: what the helm and the captain see from their
  seats (ahead, to the sides, up, down ahead): glass, hull, open sky, or what
  is in the way. A fighter's build report says how much of the view ahead the
  pilot has.
* `shipgen audit` lists parts that reach out of the hull.

## Experimental in rc2 and rc3

* **A finish for each colour** (Paint step): transparency, self-lit,
  shininess, reflectivity, glow and the game's animated surface effects, with
  ready finishes (matte, satin metal, polished metal, tinted glass, light,
  energy field).
* **Canopy**: a ceiling window shape for a cockpit under a bubble of glass.
  The hull's own faces over the cockpit become its glass, so the canopy ends
  where the model's does; it may reach over the hull ahead of the cockpit.
* **A button outside a hull door**: click beyond the door's wall, beside the
  door, for a button on the hull that opens it from outside.

## Changes and fixes in rc2 and rc3

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
chmod +x shipgen-designer-0.3.7-x86_64.AppImage
./shipgen-designer-0.3.7-x86_64.AppImage                 # the designer
./shipgen-designer-0.3.7-x86_64.AppImage gui             # shipgen-gui
./shipgen-designer-0.3.7-x86_64.AppImage shipgen --help  # the command line
```

Needs: 64-bit Linux from about 2022 on (built on Ubuntu 22.04: glibc 2.35
or newer, so Ubuntu 22.04, Debian 12, Fedora 36 and later), and OpenGL 3.3
for the 3D view. Blueprints go in the game's design folder like any other.

**Windows (experimental):** unpack
`shipgen-designer-0.3.7-windows-x86_64.zip` and start
`shipgen-designer.exe` in the folder (`shipgen-gui.exe`, `shipgen-mesh.exe`
and `shipgen.exe` are beside it). 64-bit Windows 10 or 11. It is built on
Linux and tried under Wine, so please report what goes wrong on Windows
itself. Windows may warn about an unknown publisher (the programs are not
signed): *More info*, then *Run anyway*.

**macOS (experimental):** use `shipgen-designer-0.3.7-macos-arm64.dmg`
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
* **Paint** in plain or empire colours, with a finish, a texture or a
  brush in the 3D view; **decals** on the hull.
* **Buildings** as well as ships, space stations and fighters.
* **From a 3D model:** *File > New plan from a 3D model* slices an STL,
  3DS or OBJ file into decks and makes the model itself the hull, sharp or
  smooth, simplified to the face budget, with its progress shown at the
  bottom of the window.
* **Turn and flip** the ship or one deck; **contrail and weapon
  emitters**.
* Feet or meters (*View > Metric units*).

## Known limits

* Window lining can reach a few feet out of the hull on a narrow upper deck
  with windows.

* The paint brush paints whole faces: the hull's own faces set how fine it
  can paint. Decals and image textures are at most 256 pixels a side.

* Fighter mode: no belly hatch yet; wings and other thin parts stay on the
  hull but get no cells, so a side door over a wing gets a ladder that runs
  into it (a low door gets a ramp instead).

* Rooms are one deck high; doors are one deck high except tall doors into
  rooms open to the deck below.
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
