# shipgen — Shores of Hazeron internal layout generator

`shipgen` turns a 3D model of a ship and a short requirements brief into
**finished Shores of Hazeron `.SoH` blueprints** with a complete internal
layout: rooms, room voids, crew stations, berths, service panels and the hull
volume that goes with them. It does spacecraft and buildings.

It links directly against the Shores of Hazeron engine libraries, so every
blueprint it writes is validated by the engine's own report engine and opens
directly in the in-game designer.

```
shipgen generate --model ship.3ds --brief frigate.json --out designs/
```

There are four programs:

* `build/bin/shipgen` - the command line tool.
* `build/bin/shipgen-gui` - the graphical tool: a wizard that walks you
  through the choices, a 3D preview of the model with the hull drawn see
  through, the validation report and a list of the generated blueprints.
* `build/bin/shipgen-mesh` - the mesh tool (see below).
* `build/bin/shipgen-designer` - the triangle designer: draw the hull, the
  rooms, the doors and the paint on a grid of triangles, and build the
  blueprint. See `docs/DESIGNER.md`.

The full documentation is in `docs/`, starting at `docs/README.md`.

---

## What it does

| Input | What happens |
|---|---|
| `--model ship.3ds` / `ship.stl` | The model is imported and becomes the hull of the design. The option can be given more than once for a model that is in several files. |
| `--base none\|rectangle\|circle\|hexagon` | Start from a closed primitive instead of a model. Models can be put on it, or it can be the whole hull. |
| `--base-size <w>x<d>` | The footprint of the base in feet, for example `240x120`. |
| `--base-height <feet>` | How tall the base solid is. |
| `--base-separate` / `--base-together` | With a base: place every model file as its own object in a grid, or treat the files as parts of one model. |
| `--hull design.SoH` | The hull of an existing design is reused instead of importing a model. |
| `--texture <file.png\|#rrggbb>` | A texture for the hull. Give it more than once to paint a model that is in parts. Plain colours such as `#3a6ea5` become a painted panel. |
| `--paint by-face\|planar\|cylindrical\|spherical\|keep` | How the textures are wrapped onto the hull. The default, `by-face`, keeps the texture the same size on every face. |
| `--paint-scale <feet>` | How many feet one repeat of the texture covers. Smaller makes the pattern finer. |
| `--paint-per-part` / `--paint-single` | Give each hull part the next texture (the default), or put the first texture on all of them. |
| `--brief brief.json` | The ship's purpose and the rooms and stations it must have. |
| `--layout decks\|spine\|blocks\|auto` | Which layout strategies to run. |
| `--fit <feet>` | Scale the model so that its largest side is this many feet. Handy for models that are in millimetres. |
| `--faces <n>` | How many faces the model may have at most. See below. |
| `--preset <name>` | Put the model into one of the named looks. See below. |
| `--taper <n>` | How wide the model is at the top, as a part of how wide it is at the bottom. |
| `--taper-bottom <n>` | How wide the model is at the bottom. |
| `--twist <degrees>` | How far the model turns about its axis from the bottom to the top. |
| `--repeat <n>` | How many copies of the model are put around a circle. |
| `--repeat-radius <n>` | How far apart the copies are, as a part of the width of the model. |
| `--stack <n>` | How many copies of the model are put on top of each other. |
| `--noise <n>` | How much the surface is pushed out and in. |
| `--stretch <n>` | How tall the model is. |
| `--seed <n>` | The seed for the roughness. The same seed always gives the same shape. |
| `--vary <n>` | Make this many blueprints that are all the same look but slightly different. |
| `--no-repair` | Do not close the holes in the model or put the windings right. |

There is another program as well:

```
shipgen-mesh                     # the mesh tool
shipgen mesh-info --model top.stl
```

It loads a **stl**, **3ds** or **obj** model, takes it down to a number of
faces you choose, and shows you what is left so that you can judge whether the
shape has survived. It can save the result as a binary stl or an obj file.

For every layout strategy the tool produces one `.SoH` file, so a single run
gives you several viable internal geometries to choose from:

* **decks** — horizontal decks stacked on top of each other, rooms side by side
  along the length of the ship.
* **spine** — a corridor down the middle of the ship with rooms alternating
  along the port and starboard sides.
* **blocks** — rooms packed into a two dimensional grid inside each deck.

The tool prints what it managed to fit, what it had to leave out, and what the
engine's own report says about the result.

---

## Commands

```
shipgen templates                       list the ship templates
shipgen brief <template> <file.json>    write a starting brief
shipgen generate  --model <file.3ds|file.stl|file.obj> [--model <file> ...] | --hull <design.SoH>
                   --brief <file.json> [--out <dir>]
                   [--texture <file.png|#rrggbb>]... [--paint by-face|planar|cylindrical|spherical|keep]
                   [--paint-scale <feet>] [--paint-per-part|--paint-single]
                   [--name <name>] [--layout decks|spine|blocks|auto]
                   [--scale <n>] [--fit <feet>] [--wall <feet>] [--deck <feet>] [--decimate <feet>]
                   [--faces <n>] [--base <shape>] [--base-size <w>x<d>] [--base-height <feet>]
shipgen inspect  <design.SoH>           show what is inside a design
shipgen audit    <design.SoH>           exact check: is the hull solid and is every room inside it
shipgen validate <design.SoH>           run the engine report on a design
shipgen generate-building --brief <file.json> [--out <dir>]
                   [--model <file>] [--fit <feet>] [--faces <n>]
                   [--texture <file.png|#rrggbb>]... [--paint <mode>] [--paint-scale <feet>]
                   [--base <shape>] [--base-size <w>x<d>] [--base-height <feet>]
shipgen building-brief <template> <file.json>
```

`--scale` means *feet per model unit* for a 3ds file and a plain multiplier for
an stl file. `--wall` is how much hull to leave between the interior and the
hull surface, in feet. `--deck` is the structure thickness between decks.
`--decimate` thins the imported model out by snapping its vertices onto a grid
of that many feet.

The engine refuses designs that are bigger than 28 MB, and a building 14 MB, so
a very detailed model has to be thinned out. The tool does that by itself when
it has to.

**The face budget.** The game copes with up to about 15,000 faces in a
blueprint. Around 5,000 is as much as it is comfortable with and 2,000 is
comfortable indeed. A model that comes in with half a million triangles is
thinned out to fit, and the tool says how many faces it ended up with. The
budget is set with `--faces`; it is 5,000 by default.

**Changing the shape of a model.** A model from a 3D print site is one shape.
The same model can be made into a different shape by moving its vertices
around, which keeps the material and the look of the original while changing
the silhouette. That is how one model gives a set of buildings that look like
they belong together. The graphical tool has a panel for it with a list of
named looks and the numbers behind them; the command line has the same
numbers as options.

| Look | What it gives |
|---|---|
| As it is | The model as it came. |
| Spire | A slender spire. |
| Obelisk | A very slim tower. |
| Column | Almost the same, a little slimmer at the top. |
| Flared | Wide at the top and narrow at the bottom. |
| Spiral | A twisted crystal. |
| Spiral cluster | Three twisted spires around a circle. |
| Cathedral | Four spires around a circle. |
| Ring | Six spires in a wide ring. |
| Tower | Three spires on top of each other, each one smaller. |
| Rough crystal | The surface pushed out and in. |
| Wide hall | A low, wide building. |

`--vary` makes a set of blueprints that are the same look with slightly
different proportions, so a set of buildings can be offered to choose from.
The numbers behind a look can be changed by hand, in the tool or on the
command line.

### Painting a model

A blueprint uses the engine's own texture palette. A face does not carry an
image of its own; it carries a **slot number**, and the palette holds the
image for that slot. The engine has 14 usable user slots, so a model can wear
that many different textures.

The tool fills those slots for you and works out **texture coordinates** for
every hull face, which is the part that is slow and fiddly by hand:

| Option | What it does |
|---|---|
| `--texture <file>` | Adds an image to the next palette slot. A PNG, JPEG, BMP or TGA works. |
| `--texture '#rrggbb'` | Adds a plain colour instead of an image. It becomes a small painted panel tile rather than a flat wash. |
| `--paint by-face` | Lays every face flat on its own and aligns it to the longest axis, so the texture is the same size everywhere and nothing is stretched. This is the default, and it is the one to use when the model is not a simple shape. |
| `--paint planar` | One flat projection, from above. Good for decks and sites. |
| `--paint cylindrical` | Wraps around the model's long axis. Good for fuselages and towers. |
| `--paint spherical` | Wraps around a ball. Good for round hulls. |
| `--paint keep` | Leaves the coordinates that are already there and only sets the slot. |
| `--paint-scale <feet>` | How many feet one repeat of the texture covers. Smaller is finer. |
| `--paint-per-part` / `--paint-single` | One texture per hull part (default), or the first texture on every part. |

The useful unit is the **hull part**, not the face. A model that is already in
parts — the Aetherite is a base, a ring and a top — gets a different texture
for each part, so the ring can be one colour and the top another. A model that
is one piece gets one texture; the tool says so and leaves the others unused
rather than pretending to have used them.

The graphical tool has the same controls in a **Paint** panel on the hull page:
a removable list of textures, **Add a colour...**, the wrapping mode, the
texture size and the per-part checkbox. The 3D preview shows the painted
result, with the hull see-through so that the rooms remain visible.

`shipgen inspect design.SoH` reports the palette and which slots each hull part
uses, and how many of its vertices have texture coordinates. A palette entry on
its own does nothing; the faces have to be given the slot and the coordinates,
and both are checked.

### Taking a model down to a number of faces

A model from a 3D print site has half a million triangles, and the game only
copes with about 15,000 faces. The **mesh tool** does the taking down:

```
shipgen-mesh                         # open it and pick a model
shipgen reduce --model top.stl --faces 2000 --out top_small.stl
```

There are two ways of doing it, and the tool shows both so they can be
compared:

| Method | What it does | How it comes out |
|---|---|---|
| **Quadric error** | The edges are collapsed one at a time, cheapest first, and each corner is put where the surface around it wants it to be. The edge of the model is held in place. | Slow, but the shape survives. On the Aetherite, 670,000 faces become 2,000 with **98.7% of the volume** and a surface that grows by 5%. |
| **Clustering** | The corners are snapped onto a grid and the triangles that go flat are dropped. | Very quick, but the surface goes blocky. The same model comes out with **47% more surface**. |

The tool shows the volume and the surface area before and after, so the damage
can be seen rather than guessed at.

The graphical tool and the blueprint generator use the quadric method too, so
the shells they make keep their shape.

### Tidying a model up

Models that come from the internet are often in a mess, and the mesh tool can
put them right. **Every one of your models has triangles that wind the wrong
way round** — it is what makes parts of a model look inside out — and some of
them have the same triangle twice or triangles with no area at all.

| Button | What it does |
|---|---|
| **Clean up** | Takes away the triangles that have no area and the ones that are there twice. |
| **Close the holes** | Puts a lid over every hole, so that the model is solid from the outside. |
| **Fix windings** | Turns the triangles that wind the wrong way round the other way round, so the model is solid from the outside. |
| **Put on the ground** | Puts the bottom of the model on the ground and the middle of it in the middle, which is where the blueprint generator puts it. |
| **Turn over** | Mirrors the model, for when it has to face the other way. |
| **Smooth shading** | Gives every corner the average of the normals around it, so the model looks smooth rather than faceted. It changes the lighting, not the shape. |

**The model has to be closed.** A model that has holes in it cannot be
measured, and the game cannot tell what is inside it, so the volume comes out
wrong and the rooms do not fit. The blueprint generator therefore puts the
model right before it uses it: it turns the triangles that wind the wrong way
round, and it puts a lid over the holes. It says what it found:

```
note : the model had triangles that were the wrong way round, so the shape was
       not solid from the outside. They have been put right.
note : the model had 555 hole(s) in it, so the volume could not be measured
       properly. Most of them have been closed, but 277 are still open where
       the model is pinched.
```

The repair can be turned off with `--no-repair` on the command line, or with
the "Put the model right" box in the graphical tool.

**Where a model is pinched** — where two sheets of the surface meet at a single
corner — the hole cannot be closed cleanly. Closing those would mean putting a
thin wall right across the model, which would make the volume wrong in a
different way, so they are left open and reported. A model that is closed all
the way round is best made in a modeller rather than here.

The panel on the left also says what it found: the size of the model, how many
corners and edges it has, how many holes it has, how much junk, and whether
the triangles all wind the same way round. `shipgen mesh-info` says the same
thing from the command line.

### A set of buildings

The same model can be made into a set of buildings that all have the same look
but are for different things: a capitol, an arena, an office block. The
graphical tool has a table for it on the building page; the command line
reads a small JSON file.

```
shipgen set-brief      set.json          # writes an example to look at
shipgen generate-set   --set set.json [--out dir]
```

```json
{
    "name": "Aetherite Buildings",
    "fit": 397,
    "faces": 5000,
    "model": [ "bottom.stl", "ring.stl", "top.stl" ],
    "buildings": [
        { "template": "capitol",     "shape": "cathedral", "name": "Crystal Capitol" },
        { "template": "arena",       "shape": "spiral",    "name": "Spiral Arena" },
        { "template": "office_block", "shape": "tower",    "name": "Crystal Tower",
          "variations": 2 }
    ]
}
```

`model` and `fit` can be left out of a building to use the ones at the top.
`variations` makes more than one version of that building.

---

## The brief

A brief is a small JSON file. It can start from a template and then be edited
freely.

```json
{
    "name": "Ironclad",
    "template": "military_frigate",
    "crew": 60,
    "rooms": [
        { "type": "bridge", "label": "Bridge", "count": 1, "size": [44, 34, 13] },
        { "type": "engineering", "label": "Engineering", "count": 2, "size": [54, 34, 15] }
    ],
    "stations": [
        { "type": "helm", "count": 1, "room": "bridge" },
        { "type": "engineer", "count": 2, "room": "engineering" }
    ],
    "berths": { "capt": 1, "officer": 6, "crew": 54, "passenger": 0, "troop": 0 },
    "modules": { "powerPlant": 120, "maneuverDrive": 90 }
}
```

* Room `size` is `[length, beam, height]` in **feet** (design units).
* `priority` (1 is most important) decides which rooms are dropped when the
  hull is too small for all of them. It defaults from the room type.
* Station `room` names the room type the station belongs in. If it is missing,
  the station goes into any room that is free.
* `modules` are hull volume allocations in **cubic meters**. Leave them out and
  the tool works them out from the size of the hull.

### Templates

Spacecraft: `military_frigate`, `cargo_hauler`, `science_lab`,
`colony_transport`, `patrol_cutter`, `shuttle`.

Buildings: `office_block`, `residential`, `barracks`, `command_post`,
`warehouse`, `factory`, `biodome`.

## Buildings

A building blueprint is a **shell**, a **site**, the **hull volume shared out
between the systems**, some **parking** and the **industry** it serves. An
interior is optional: a monument made from a model has no rooms at all.

That is the shape the game wants, and it is the shape of the reference
blueprint in `testdata/aetherite_capitol_reference.SoH`: the Aetherite model as
three hull parts, a site, five ground vehicle parking spots, two space vehicle
landing pads, fourteen offices, an auditorium and the Capitol industry, with
no interior rooms at all.

The brief is the same kind of JSON file:

```json
{
    "name": "Aetherite Capitol",
    "industry": "capitol",
    "groundParking": 5,
    "spaceParking": 2,
    "volumes": {
        "offices": 500, "store": 500, "hold": 500, "auditorium": 2000
    }
}
```

The volumes are in cubic metres and are used as they are asked for. Whatever is
left of the hull volume goes to the structure, so the systems always add up to
the size of the building. If the brief asks for more than the building can
hold, it is scaled down and the tool says so.

The game keeps its industry table in its database, which a standalone tool
cannot reach, so the tool installs a small stand-in table to work out whether
a building is useful for anything. When the building cannot serve the
industry it was given, the tool says so.

Building templates: `capitol`, `arena`, `command_post`, `office_block`,
`residential`, `barracks`, `warehouse`, `factory`, `biodome`.

## Buildings made from a model

A building can be made from a 3D model as well. The model becomes the shell of
the building and the generator finds the room inside it:

```bash
shipgen generate-building \
    --model aetherite_bottom.stl --model aetherite_ring.stl --model aetherite_top.stl \
    --fit 120 --brief capitol.json --out designs/
```

Models that come from a 3D print site are solid objects rather than hollow
shells, so there is no interior to speak of, and a building does not need one.
The model becomes the shell, the site is drawn around it, the parking goes in
front of it and the hull volume is shared out between the systems.

The Aetherite is three files that belong together, so `--model` is given
three times. They are in millimetres, so `--fit` scales the whole assembly.
The three parts keep the positions they have in relation to each other.

The models in `testdata/models` come from the zip files in your `3d models`
folder. `testdata/extract_models.sh` takes them out and gives them sensible
names. `test.sh` also reads every blueprint in
`~/Documents/designs/examplebuildings`, if that folder exists, so that real
building files are exercised as well.

## A building from a base shape

A building does not need a model at all. Give it a primitive instead:

```bash
shipgen generate-building \
    --base hexagon --base-size 160x140 --base-height 28 \
    --brief office_block.json --out designs/
```

The base is a closed solid, so the engine sees a volume and the building is
valid on its own. Models can be added to it one at a time:

```bash
shipgen generate-building \
    --base circle --base-size 400x400 --base-height 8 \
    --model statue.stl --model fountain.stl --fit 120 \
    --brief arena.json --out designs/
```

With a base, every model file is a separate object and they are laid out in a
grid that follows the shape of the base. Without a base, the files are parts
of one model and the first file is the anchor that the others keep their
places around. `--base-together` asks for that parts behaviour even when a base
is given.

In the GUI, the primitive base is the first thing on the building page. The
model list builds up as files are added, and every row can be removed again.

The mesh tool now has working **Simplify** and **Reset** buttons. Removing a
row from its model list removes that model from the assembly as well, and the
remaining models are put together again.

---

## What comes next

The plan for the MCP server and the offline designer is in
`docs/MCP_AND_OFFLINE_DESIGNER.md`. The MCP server lets an AI drive the same
operations this tool does, over a small JSON interface, with no game and no
network; it has not been started. The triangle designer
(`docs/DESIGNER.md`) is the first piece of an offline designer.

## Building

The tool needs a C++ compiler and Qt 5 (Core, Gui, Widgets, Network, Sql and
OpenGL headers). It builds the headless part of the Shores of Hazeron engine
from source: `AuUtil`, `AuMath`, `AuGL`, `AuGlobal` and `AuData`. The game
client, the servers and the database are **not** needed.

```bash
./build.sh            # build everything
./build.sh clean      # remove the build directory
./build.sh -j8        # build with 8 parallel jobs
```

`build.sh` downloads the engine into `third_party/soh` if it is not there yet
and writes the command line tool, the graphical tool and the mesh tool to
`build/bin/`.

The toolchain used on this machine lives in `~/.local/hazeron-toolchain`; set
`TOOLCHAIN` to point somewhere else if needed. The Qt headers, `moc` and a
C++ compiler are all that is required — nothing needs to be installed system
wide.

### Windows

The graphical tool and the mesh tool build on Windows with MSYS2, MinGW-w64
and Qt 5. The same `build.sh` detects the MSYS2 shell and writes
`build-windows/bin/*.exe`; a separate deployment script copies the Qt,
OpenAL and MinGW DLLs into a folder that can be moved to another PC.

See [docs/WINDOWS.md](docs/WINDOWS.md) for the package list, build commands
and the first things to check. That path has not been run on a Windows
machine in this workspace, so it is documented rather than claimed as
tested.

---

## Release: the AppImage

`shipgen-gui` is the main program, released as an AppImage that also holds
the triangle designer and the command line:

```
./build.sh && ./package-appimage.sh      # build/shipgen-gui-x86_64.AppImage
./shipgen-gui-x86_64.AppImage                  # the generator's window
./shipgen-gui-x86_64.AppImage designer         # the triangle designer
./shipgen-gui-x86_64.AppImage shipgen <command> ...
```

It bundles Qt 5 with its X11 and Wayland plugins, OpenAL and the other
libraries, but not the C library, the OpenGL drivers, X11/xcb or the font
libraries, which come from the system (the AppImage project's rules). It
runs on systems with a glibc at least as new as the one it was built on,
so for the widest reach build it on an older distribution. The script
downloads `appimagetool` into `build/tools/` the first time;
`APPDIR_ONLY=1 ./package-appimage.sh` makes only the folder
`build/AppDir`, which runs as `build/AppDir/AppRun`.

## The graphical tool

`build/bin/shipgen-gui` has a main window with the source files on the left,
a 3D preview in the middle, the validation report on the right and a list of
the parts in the blueprint along the bottom. *Generate* opens the wizard:

* **Hull** - the ship model, or an existing blueprint to take the hull from,
  the scale and how much to simplify the model, and the **Paint** panel for
  textures or colours.
* **Purpose** - the type of ship, its name, its crew and a description.
* **Rooms** - a table of the rooms it has to have, with their sizes in feet.
* **Stations** - the stations that have to be staffed and which room they
  belong in.
* **Crew and systems** - the accommodation and the hull volume per system.
* **Layout** - which layouts to produce and how thick the walls and decks are.

The same wizard with different pages makes a building blueprint.

The preview draws the hull and the site see through so that the interior can
be seen, and it shows the palette textures on the hull. Drag to turn the
model, use the wheel to zoom.

The model is picked with a **list**, not a single box. A model is often in more
than one file — the Aetherite is in three — so the picker takes several files
at once and keeps them in a list, and any of them can be taken away again
with **Remove**, or all of them with **Clear**. The list of models is shown in
the Project panel on the left, one row for each file.

## Notes and limits

* The engine stores a design as a binary blob: a byte of file type, a byte of
  version and then a zlib compressed, XOR encrypted `QDataStream`. There is no
  text format, so `shipgen` speaks that format directly.
* Textures are limited by the engine to **256 × 256** pixels, and the design
  palette has **14 user slots** (the material slot in the middle belongs to the
  engine). The tool puts images in the user slots and gives the hull faces the
  matching slot and coordinates. See **Painting a model** above.
* **3ds**, **stl** and **obj** models can be imported. The mesh tool reads the
  texture coordinates of an obj file and writes them back out; the blueprint
  importer keeps its own coordinates until the painter replaces them.
* Room sizes are scaled down automatically when the hull is too small, and
  rooms are moved around inside the hull until every sampled corner, edge and
  face is inside **one** hull part. Rooms that cannot be made to fit, or that
  would overlap another room, are moved or left out and reported. The test
  suite checks every generated ship for rooms outside the hull and for rooms
  that share space.
* **`shipgen audit` is the exact check.** It builds a closed triangle solid
  for each hull part and tests each room box against it, rather than measuring
  a silhouette. `shipgen inspect` prints the same exact room-fit lines, the
  solidity of each hull part, rooms outside the hull, rooms that could not be
  checked, and rooms without a door. A hull must be a **closed solid** to be
  used as a volume. An open or non-manifold source design is refused; pass
  `--allow-open-hull` to try `SolidifyMesh()` and fall back to the silhouette
  with a warning, which is not verified the same way.
* A generated spacecraft gets doors between rooms that share a wall (enough
  of them to join every room to every other), and one hull door. Each room
  has a constant room void; each door gets a small doorway void of its own,
  tied to its open state, and a hull door a hull void as well. A room's own
  void is never tied to a door: the engine treats a tied void as one that
  comes and goes with the door, so the room would only exist while the door
  was open. Blueprints generated before 30 October 2026 have exactly that
  fault; generate them again. See `docs/SOH_FORMAT.md` section 5. The
  building generator adds its entrance door the same way. `shipgen inspect` lists every
  door and the voids associated with it, so an unassociated door is easy to
  spot. On a deep hull the layout stays in the horizontal band with the most
  solid interior, and each room is snapped flush to a neighbour, so tall and
  narrow hulls still get connected rooms.
* The 3D preview needs OpenGL 3.3. Without it the rest of the graphical tool
  still works, it just cannot show the model. The preview draws the palette
  textures; a face with no texture coordinates is drawn without one.
* The room and station pickers only offer the names the game itself uses. The
  spacecraft room names come straight from the engine's own room type list, so
  they always match.
* Crew walk paths go from each room's middle to its doorway, through the
  doorway (only while the door is open), and on to the next room's middle.

---

## Testing

```bash
./test.sh
```

Runs the generator over every template and every hull it can find and checks
each design it produces with the engine's report. Every generated design is
also run through `shipgen audit`, which checks that the hull is a closed solid
and every room box is genuinely inside it. The suite inspects every generated
ship for rooms outside the hull, rooms that share space, and doors without the
voids that make them open; paints a single hull and a model in parts and checks
the texture coordinates; and starts both graphical tools, drawing the 3D
preview under Xvfb when it is available. A last run on this machine passed
330 checks over 115 blueprints with no errors.

---

## Layout of the source

```
src/shipgen.h     the brief, the room and station model
src/brief.cpp     templates, brief parsing and writing
src/generate.h    the hull analysis and the layout generator interface
src/generate.cpp  hull analysis, room placement, doors, part building, validation
src/paint.h       the painting options and the public paint API
src/paint.cpp     texture loading, colour tiles, palette slots, UV projection
src/logging.h     the shared message handler
src/logging.cpp   drops the engine's non-applicable obstruction warning
src/base.h        the primitive base shapes
src/base.cpp      closed rectangle, circle and hexagon solids
src/building.h    the building brief and the building generator interface
src/building.cpp  building templates, the building generator, the industry
                  table and the building validation
src/interior.*    rooms, doors, doorway and hull voids, ladders, walk paths
src/sohfile.*     reading blueprints from newer game builds
src/triplan.*     the designer's triangle plan
src/planbuild.*   a triangle plan into a blueprint
src/audit.*       the exact solid and containment tests
src/mesh.*, src/shape.*, src/names.*   models, shapes, the game's names
src/main.cpp      the command line interface
src/gui/          the graphical tool
    mainwindow.*    the main window, the docks and the file list
    wizard.*        the generation wizard and its pages
    preview.*       the 3D preview (also used by the mesh tool and the designer)
src/meshapp/      the mesh tool
src/designer/     the triangle designer
tests/probe.cpp   a small program that checks how the engine's geometry
                  primitives behave
build.sh          builds the engine libraries, the CLI and the three Qt tools
build-windows.ps1 convenience wrapper that runs build.sh in MSYS2
deploy-windows.sh collects the Qt/OpenAL/MinGW DLLs for a Windows folder
test.sh           runs the generator over a matrix of templates and hulls
HANDOFF.md        the state of the work and what is left to do
docs/             the documentation (docs/README.md is the index)
```
