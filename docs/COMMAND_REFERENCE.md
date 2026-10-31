# Command reference

Every option here is taken from the source (`src/main.cpp`,
`src/gui/main.cpp`, `src/meshapp/main.cpp`, `src/designer/main.cpp`).
`./build/bin/shipgen` with no arguments prints the same summary.

The programs need the toolchain's libraries on Linux:

```bash
export LD_LIBRARY_PATH=$HOME/.local/hazeron-toolchain/usr/lib64
```

## shipgen (the command line)

### Looking at blueprints

| Command | What it does |
|---|---|
| `shipgen inspect <design.SoH>` | Parts by type, hull parts and whether each is a closed solid, textures, room voids (doorways marked), room fit against the hull, overlaps, doors with their open-state voids, rooms that cannot be reached through a door, rooms that only exist while a door is open, stations. Part types this engine build does not know are reported as a `Note:` and stepped over. |
| `shipgen audit <design.SoH>` | The exact test: every room box against the hull triangles, for each closed hull part. |
| `shipgen validate <design.SoH>` | The engine's own report (`AuScReport` / `AuBdReport`), as `Errors (n):` and `Warnings (n):`. |
| `shipgen parts <design.SoH> [<type>] [--open-edges]` | Every part on one line: id, type, group, door tie (`tied to <door> state <n>`), module, a transporter's pad effect and pad locations, triangles, signed volume (hulls and voids negative, room meshes positive in a blueprint that works in the game), bounds. A type name part filters the list. `--open-edges` adds where a mesh's open edges are. |
| `shipgen faces <design.SoH> [<part>]` | The texture palette (library texture ids, see [TEXTURE_LIBRARY.md](TEXTURE_LIBRARY.md), or images) and, per part, how its faces look: texture slot, colour theme, paint, opacity, self-illumination, shine, reflectivity, surface effect, vertex glow. |
| `shipgen cut <in.SoH> <out.SoH> window\|door <hull part> <room part\|0> x0 y0 z0 x1 y1 z1` | Cuts a window or door through a hull part (and a room mesh) with a box-shaped jig, using the engine's own `JigCutParts`, as the game's designer does. The new parts are listed. |
| `shipgen resave <in.SoH> <out.SoH>` | Reads a blueprint and writes it back. Parts of types this engine build does not know are kept byte for byte. Refuses to write a design that could not be read completely. |
| `shipgen templates` | The ship and building templates. |

### Briefs

| Command | What it does |
|---|---|
| `shipgen brief <template> <file.json>` | Writes a ship brief from a template, to be edited. |
| `shipgen building-brief <template> <file.json>` | The same for a building. |
| `shipgen set-brief <file.json>` | Writes an example building set. |

Ship templates: `military_frigate`, `cargo_hauler`, `science_lab`,
`colony_transport`, `patrol_cutter`, `shuttle`. Building templates include
`office_block`, `residential`, `barracks`, `command_post`, `warehouse`,
`factory`, `biodome`, `capitol`, `arena` (`shipgen templates` has the full
list).

### Generating a ship

```
shipgen generate  --model <file> [--model <file>]... | --hull <design.SoH> | --base <shape>
                  --brief <file.json> [options]
```

One of `--model`, `--hull` or `--base` is required, and `--brief`.

| Option | Default | What it does |
|---|---|---|
| `--model <file>` | | A 3ds, stl or obj model for the hull. Several files are parts of one model, or separate models on a base. |
| `--hull <design.SoH>` | | Reuse the hull parts of an existing blueprint. The hull must be a closed solid. |
| `--ftl none\|warp1..warp9\|wormhole-p\|wormhole-pn\|wormhole-pnn` | the brief's `ftl`, else `wormhole-p` | The FTL drive. One engineer station runs it; the game fits the maneuver drive itself. |
| `--keep-axes` | off | Do not turn the model. Without it, a model that is longer along X than along Y is turned a quarter turn, because the game takes +Y as fore. |
| `--allow-open-hull` | off | Place rooms in a hull that is not closed. Rooms are then only checked against the silhouette (unverified). |
| `--out <dir>` | current directory | Where the blueprints go. |
| `--name <name>` | the brief's | |
| `--layout decks\|spine\|blocks\|auto` | the brief's, else all | One blueprint per strategy. |
| `--candidates <n>` | all | Only the first n strategies. |
| `--scale <n>` | 1 | Feet per model unit. |
| `--fit <feet>` | | Scale the model so its largest side is this long (for models in millimetres). |
| `--wall <feet>` | 8 | Hull left between the interior and the hull surface. |
| `--deck <feet>` | 4 | Structure between decks. |
| `--decimate <feet>` | | Snap vertices to a grid of this size. |
| `--faces <n>` | 5000 | Face budget for the hull. |
| `--no-repair` | | Do not fix windings or close holes. |
| `--texture <file.png\|#rrggbb>` | | A texture or plain colour; give it more than once for a model in parts. |
| `--paint by-face\|planar\|cylindrical\|spherical\|keep` | by-face | How textures are wrapped. |
| `--paint-scale <feet>` | | How many feet one repeat of the texture covers. |
| `--paint-per-part` / `--paint-single` | per part | Each hull part the next texture, or the first texture on all. |
| `--base none\|rectangle\|circle\|hexagon` | none | A closed primitive to build on, or to be the whole hull. |
| `--base-size <w>x<d>` | | Its footprint in feet, e.g. `240x120`. |
| `--base-height <feet>` | | Its height. |
| `--base-separate` / `--base-together` | separate with a base | Each model file its own object in a grid, or parts of one model. |
| `--preset <name>` | | A named look: `as-is spire obelisk column flared spiral spiral-cluster cathedral ring tower rough wide`. |
| `--taper <n>`, `--taper-bottom <n>` | | Width at the top / bottom, as a part of the width. |
| `--twist <degrees>` | | Turn about the vertical axis from bottom to top. |
| `--repeat <n>`, `--repeat-radius <n>` | | Copies around a circle, and how far apart. |
| `--stack <n>` | | Copies stacked up, each smaller. |
| `--noise <n>`, `--noise-scale <n>`, `--seed <n>` | | Surface roughness. |
| `--stretch <n>` | | Taller or shorter. |
| `--vary <n>` | 1 | This many blueprints of the same look with slightly different proportions. |

Rooms are never made smaller than 6 ft across and 7 ft high
(`SHIPGEN_MIN_ROOM_FEET`, `SHIPGEN_MIN_ROOM_HEIGHT` in `src/generate.h`); a
room that does not fit is left out and reported.

### Generating buildings

```
shipgen generate-building --brief <file.json> [--out <dir>] [--name <name>]
                  [--layout grid|corridor|auto] [--model <file>]... [--fit <feet>] [--scale <n>]
                  [--faces <n>] [--texture ...] [--paint <mode>] [--paint-scale <feet>]
                  [--base ...] [--base-size <w>x<d>] [--base-height <feet>]
                  [--base-together|--base-separate] [the shape options] [--vary <n>]
shipgen generate-set --set <file.json> [--out <dir>]
```

### Models

| Command | What it does |
|---|---|
| `shipgen mesh-info --model <file>` | Triangles, vertices, and what is wrong with the model. |
| `shipgen reduce --model <file> --faces <n> --out <file> [--method quadric\|cluster] [--no-repair]` | Thins a model out and writes a binary stl or an obj file. |

### Drawn ships

| Command | What it does |
|---|---|
| `shipgen kit list \| <key> \| all [--out <dir>]` | The Aether kit ([AETHER_KIT.md](AETHER_KIT.md)): builds the blueprints and prints the engine's report. Exit code 3 if one has errors. |
| `shipgen plan-example <plan.json>` | Writes the designer's example plan. |
| `shipgen plan-flip <plan.json> <out.json> [--turn] [--mirror] [--fore-aft] [--upside-down] [--deck <n>]` | Turns a plan around (`--turn`: fore and aft, and port and starboard, a half turn), mirrors it (`--mirror`: port and starboard), swaps only fore and aft (`--fore-aft`) or turns it upside down (`--upside-down`, the whole plan only); with `--deck` only that deck (from 1). The rooms, doors, windows, ladders, stairs, parking, emitters, paint and the plan's 3D model go with it. |
| `shipgen plan-from-model <model.stl\|3ds\|obj> <plan.json> [--length <ft>] [--threshold <0..1>] [--decks <n>] [--slices <per deck>] [--method slices\|solid] [--seal <ft>] [--min-piece <n>] [--no-pinholes] [--grid-hull] [--cell <ft>] [--deck <ft>] [--flip] [--y-up]` | A plan whose hull triangles are filled in from a 3D model, to draw rooms on. The longest horizontal side becomes fore; `--length` is that length (default 150 ft), or `--decks` the number of decks (slices) the model is cut into, which sets the length; `--slices` the heights per deck each triangle is judged at (default 3 for 10 ft decks); `--method solid` counts only the model's closed pieces (the old way; default `slices`); `--seal` closes gaps narrower than that (default 1 ft); `--min-piece <n>` takes away pieces of fewer triangles (default 12) and `--no-pinholes` leaves single missing triangles unfilled; `--grid-hull` makes the plan's hull from the drawn triangles instead of the model; `--threshold` is how much of a triangle has to be inside the model (default 0.5; lower is fuller); `--flip` turns it round; `--y-up` for models made with y up. See `DESIGNER.md`. |
| `shipgen build-plan <plan.json> [--out <file.SoH>] [--no-furnish] [--no-paths] [--no-ramps] [--faces <n>] [--panels <n>] [--model-hull\|--grid-hull]` | Turns a triangle plan into a blueprint and prints the engine's report. `--faces` overrides the plan's face budget; `--panels` tries the experimental flat panels; `--model-hull`/`--grid-hull` build the hull from the plan's 3D model or from the drawn triangles. `--no-ramps` gives every hull door a ladder down instead of a ramp. Without `--out` the blueprint goes next to the plan, named after it. Exit code 0 when the engine accepts it, 3 when it has errors. |

See `DESIGNER.md`.

## shipgen-gui (the generator's graphical tool)

A wizard for everything `generate`, `generate-building` and `generate-set`
do, with a 3D preview and the engine's report. For scripts:

| Option | What it does |
|---|---|
| `--open <design.SoH>` | Open a blueprint. |
| `--wizard`, `--wizard-page <n>`, `--wizard-kind ship\|building`, `--wizard-template <key>` | Start in the wizard. |
| `--generate <brief.json>`, `--generate-building <brief.json>` | Generate straight away. |
| `--hull <design.SoH>`, `--model <file>`, `--fit <feet>` | The hull for that. |
| `--screenshot <file.png>`, `--screenshot-preview <file.png>` | Save a picture of the window, or of the 3D view alone, and quit. |

## shipgen-mesh (the mesh tool)

| Option | What it does |
|---|---|
| `-o, --open <file>` (or the file as an argument) | The model to open. |
| `-f, --faces <n>` | The number of faces to aim for. |
| `-m, --method quadric\|cluster` | How to thin it out. |
| `-v, --view reduced\|original\|both` | What to show. |
| `-z, --size <w>x<h>` | The window size. |
| `-s, --screenshot <file.png>` | Save a picture and quit. |

## shipgen-designer (the triangle designer)

| Option | What it does |
|---|---|
| `-o, --open <plan.json>` | Open a plan. Without it the example plan is shown. |
| `--step <1-5>` | The step to show: 1 ship, 2 hull, 3 rooms, 4 paint, 5 build. |
| `--deck <n>` | The deck to show, from 1. |
| `--build` | Build the blueprint once the window is up. |
| `--export <file.SoH>` | Build, save and quit. Exit code 0 when the engine accepts it, 3 when not, 1 when it could not be written. |
| `--screenshot <file.png>` | Save a picture of the window and quit. |
| `--from-model <model>` | Start a new plan from a 3D model (STL, 3DS or OBJ), as *File > New plan from a 3D model* does, with a progress window. Prints "plan made from the model" or "no plan from the model". With `--screenshot` or `--export` nothing is asked and a failure exits with 1; without them, what went wrong is shown in a message box. |
| `--length <ft>` | With `--from-model`: the length of the ship (default 150 ft). |
| `--version` | The version (0.3.3-alpha). The window title shows it too. |

## build.sh and test.sh

```bash
./build.sh          # the engine libraries, the core library and the four programs (about 3 minutes)
./build.sh clean    # removes build/
./test.sh           # the whole matrix (about 6 minutes)
```

`build.sh` takes no other options. It rebuilds an object when its source or
any header beside it is newer. On MSYS2 it builds into `build-windows/` (see
`WINDOWS.md`).

`test.sh` reads `DESIGNS` (default `~/Documents/designs`),
`EXAMPLE_BUILDINGS` (default `~/Documents/designs/examplebuildings`) and
`TOOLCHAIN` from the environment. It has no options; it runs everything and
ends with `<n> checks passed, <m> blueprints validated cleanly, <k> with
errors.`
