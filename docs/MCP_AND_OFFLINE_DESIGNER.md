# The plan: MCP first, then the offline designer

Status (30 October 2026): **the MCP server has not been started.** Of the
offline designer, a different first piece exists: `shipgen-designer`
(`DESIGNER.md`) draws *new* ships on a triangle grid and builds them. It
does not open or edit existing blueprints, which is what section 4 below is
about, so section 4 still stands as written. Two pieces this plan asks for
now exist in the core and should be reused: the room, door, void and path
construction (`src/interior.h`) and the furnishing of rooms that are already
placed (`FurnishRooms` in `src/generate.h`). Reading blueprints from newer
game builds (`src/sohfile.h`) is what `design_open` should use.

The rest of this document is the specification as agreed.

The decision: build the **MCP server first** and the **offline designer
second**. The MCP server is roughly a week of work and it is immediately
useful. It also tells us which designer operations actually matter, so the
designer is built against a real list of needs rather than a guess.

---

## 1. Why this order

The game's designer cannot simply be lifted out of the client:

* `audesigner/` has 227 `.cpp` files. **182** include `auapp.h`,
  `auclientglobal.h`, `auframe.h` or `audesignerapp.h` and cannot be built
  without the whole game client.
* `audesigner/audesignerview.cpp` is 15,930 lines and
  `audesigner/audesignerframe.cpp` is 5,578. Both are welded to
  `auApp->Frame()` and to the atom layer: `DaDesigner` is a `DaAtom`, so it
  wants a `DaUniverse` to live in.
* `audesigner/reqt.opt` links `-lAuClient`, and the root `Makefile:436` only
  builds the designer after `libAuClient_debug.a`.

But the substance is not in the UI. All of it is in `audata`, which the
existing tool already links:

| What | Where |
|---|---|
| Every editing operation (`MakeHull`, `MakeRoom`, `MakeDoor`, `MakeWindow`, `MakeTurboLift`, `MakeTurret`, `FuseFaces`, `DivideFaces`, `JoinParts`, `DeletePart`, `AddPartObjects`, `Transform`, `MoveDoorSwitches`) | `audata/auscparts.h:453-792` |
| Picking (`IntersectRay`, `SelectFaceAt`, `Sight`) | `aumath/auscface.h:781, 1032`, `audata/auscparts.h:2832` |
| The selection model (`AuScSelection`, `AuScSelectInfo`) | `aumath/auscface.h:156, 305` |
| The file format | `audata/audesign.cpp:429` |
| Validation and the report | `audata/auscreport.cpp`, `audata/aubdreport.cpp` |
| The 46 designer files that need no client, among them 29 pure-Qt dialogs | `audesigner/*.cpp` |

The `ddsc*` command classes are thin: of the calls they make into the
client, 78 are `auApp->DesignerPartAdd` ("put this part in the design") and
74 are `auApp->Frame` ("show a dialog"). The real work happens inside
`AuScParts`.

So both halves of this plan are the same job: put a front end on the data
model. The MCP server is a scriptable front end; the designer is a graphical
one. They should share one core.

---

## 2. The shape of the thing

```
                    libShipgenCore  (C++, audata + aumath + augl + auglobal)
                    ┌──────────────────────────────────────────────────────┐
                    │  Document      a design + selection + undo stack      │
                    │  Generator     brief -> candidate layouts            │
                    │  Validator     the engine's own report               │
                    │  Vocabularies  the names the game itself uses        │
                    └──────────────────────────────────────────────────────┘
                          │                              │
              shipgen (CLI)│                    shipgen-gui (Qt)
                          │                              │
                          └── shipgen exec --json  ───────┤
                                 (one JSON request        │
                                  in, one JSON reply out)  │
                                      │                   │
                                      └──── MCP server ────┘
                                            (Python)
```

The CLI's human-readable commands and the MCP tools both become thin wrappers
over the same core functions. There is exactly one implementation of
"add a room" and it is the C++ one.

### Why the JSON request layer

A plain `shipgen exec --json '{...}'` command keeps **all** the logic in C++
and makes the Python server completely mechanical: one MCP tool maps to one
JSON request. The alternative, letting the Python server drive the CLI through
human-readable output, would put the logic in two places and make the output
depend on parsing English.

The request and the reply:

```json
// request
{ "op": "add_room", "args": { "type": "Bridge", "label": "Bridge",
                              "size": [44, 34, 13], "position": [0, 0, 4] } }

// reply
{ "ok": true, "result": { "partId": 12, "voidPartId": 11 },
  "warnings": ["the room is outside the hull"] }
```

Rules for the layer:

* **Never raise.** A failed operation comes back as `ok: false` with a
  message. An AI has to be able to recover from a refusal.
* **Every reply is JSON**, including errors. No prose on stdout.
* **Part ids are the currency.** Everything that refers to something in a
  design uses the part id the engine gives it.
* The layer is the *only* thing the MCP server and the GUI both call.

---

## 3. The MCP server

### Shape

* Python, using the `mcp` SDK (FastMCP). `pip` works on this machine and
  PyPI is reachable. There is no node on this box, so the Python SDK is the
  route.
* Transport: **stdio**. It runs next to the tool, fully offline, and needs no
  port or configuration. An HTTP transport can be added later for a shared
  server.
* The server is a *thin* client. It holds no design state of its own; every
  tool is a request to the core.

### The tools

**Vocabulary** — the AI must never invent a name the engine will not take.

| Tool | Returns |
|---|---|
| `room_types()` | The room types the game uses, for spacecraft and for buildings. Spacecraft names come from the engine's `ROOMTYPE` list (`auutil/audef.h:605`, `RoomTypeNameInSpacecraft()`); building names from the room names the game's own building blueprints use. |
| `station_types()` | The station types, which are the engine's console types. |
| `blueprint_templates(kind)` | The ship and building templates with their rooms, stations, berths and volumes. |
| `blueprint_systems()` | The systems a blueprint can allocate hull volume to, and what each one needs. |
| `shape_presets()` | The named looks a model can be put into, with the numbers behind each one. |
| `shape_parameters()` | What every shape parameter does, its range and its default. |
| `base_shapes()` | The primitive base shapes (`none`, `rectangle`, `circle`, `hexagon`), what each one is for, and how models are placed on it. |
| `building_systems()` | The systems a building can allocate its hull volume to (`offices`, `shops`, `store`, `lounge`, homes, `hold`, `powerPlant`, `lifeSupport`, `capacitor`, `sensors`, `shields`, `armor`, `skeleton`). |
| `simplify_methods()` | The two ways a model can be thinned out and what each one costs in quality. |

**Briefs**

| Tool | Notes |
|---|---|
| `brief_read(path)` | Reads a brief file. |
| `brief_write(path, spec)` | Writes one. The spec is the same shape as the JSON on disk. |
| `brief_from_template(kind, key)` | A brief pre-filled from a template, to be edited. |

**The design itself**

| Tool | Notes |
|---|---|
| `design_new(name, kind)` | A design of kind `ship` or `building`. |
| `design_open(path)` / `design_save(path)` | `.SoH` files. |
| `design_describe()` | The structured summary: hull extents and volume, every room with its type, size and position, every station, the berths, the site, the industry, and the system volumes. |
| `design_validate()` | The engine's own report, split into errors and warnings. |
| `design_render(path, width, height)` | A PNG of the model, hull drawn see through. **This is the one that makes the AI useful**: it can look at what it built and go round again. The OpenGL preview already exists in `shipgen-gui`; rendering it headless is a small addition. |

**Editing**

| Tool | Notes |
|---|---|
| `design_add_room(type, label, size, position)` | Adds the room mesh and its void, the way the generator does. |
| `design_add_station(type, room_type)` | Adds a console and gives it the right module. |
| `design_move(part_id, delta)` / `design_rotate(part_id, axis, degrees)` | |
| `design_delete(part_id)` | |
| `design_set_volume(system, cubic_metres)` | The hull volume the system gets. The remainder goes to the skeleton. |
| `design_add_parking(kind, count)` | Ground and space vehicle parking spots, which a building needs. |
| `design_set_industry(industry)` | The industry the building is for. The engine checks that the building can actually do it, so the tool has to say when it cannot. |
| `design_undo()` / `design_redo()` | The undo stack is part of the core document. |

**Generation**

| Tool | Notes |
|---|---|
| `generate(kind, brief, hull, options)` | Runs the generator and returns the candidates with their validation. `hull` is a model file, a list of model files, an existing blueprint, or a primitive base. `options` covers the scale, the fit, the layout strategies, the wall and deck thickness, the decimation and the face budget. |
| `generate_from_mesh(mesh, options)` | Imports one model as the hull of a design and reports the repairs, the final face count and the volume. |
| `building_set(set)` | Makes a set of buildings from one model: a list of entries, each with its own template and its own shape. This is how one model gives a set of buildings that look like they belong together. |
| `options.base` | A primitive base to build on: `none`, `rectangle`, `circle` or `hexagon`, with `width`, `depth`, `height` and `separate_models`. With a base, each model file is placed in a grid on it. With `none`, the model files are treated as parts of one model and the first file is the anchor. |
| `options.faces` | The face budget. The game copes with about 15,000 faces, is comfortable with about 5,000 and is happy with about 2,000. The default is 5,000. |
| `options.shape` | The shape to put the model into: the taper, the twist, the copies around a circle, the copies stacked up, the roughness and the stretch. A named look from `shape_presets()` can be given instead. |
| `options.variations` | How many blueprints to make. Each one is the same look with slightly different proportions, so the AI can offer the user a choice. |

**Buildings**

A building is not just a ship with a different brief. It has a shell (hull
parts or a primitive base), a site, hull volume split between systems, ground
and space parking, and an industry. The rooms are optional.

| Tool | Notes |
|---|---|
| `building_set_footprint(width, depth, floors, floor_height)` | The shell the building is planned inside, in feet. |
| `building_set_volumes(systems)` | Splits the hull volume between `offices`, `shops`, `store`, `lounge`, homes, `hold`, `powerPlant`, `lifeSupport`, `capacitor`, `sensors`, `shields`, `armor` and the skeleton. The parts must add up to the hull volume; the remainder becomes structure. |
| `building_set_parking(ground, space)` | How many ground and space vehicle parking spots the site gets. |
| `building_set_industry(industry)` | The industry the building is for. The engine checks that the building can actually do it, so the tool has to say when it cannot. |
| `building_add_room(type, label, size, position)` | Adds a room to the interior. Rooms are optional for buildings. |
| `building_add_door(room, position)` | Adds a door from the outside into a room. |
| `building_describe()` | The shell, the site, the volumes, the parking, the industry, the rooms and the doors. |

### The face budget and the shapes

Two things about a model have to be part of the tool surface, because the AI
cannot guess them.

**The face budget.** A Shores of Hazeron blueprint may carry about 15,000
faces. Around 5,000 is comfortable and 2,000 is comfortable indeed. A model
from a 3D print site has half a million triangles, so it always has to be
thinned out. The tool does the thinning with the quadric error method, which
keeps the shape, and says how many faces it ended up with, so the AI can tell
the user what it made. `options.faces` sets the budget. The tool can also
thin a model on its own, for when the user wants to look at a model before it
goes into a blueprint.

**The shape.** The same model can be made into a different shape by moving
its vertices around, which keeps the material and the look of the original
while changing the silhouette. This is how one model gives a set of buildings
that look like they belong together: a spire, an obelisk, a spiral, a
cathedral of four spires, a tower, a rough crystal. The operations are:

| Operation | What it does |
|---|---|
| `taper` | The cross section is scaled with the height. |
| `twist` | The model turns about its vertical axis as it goes up. |
| `repeat` | Copies of the model around a circle. |
| `stack` | Copies of the model on top of each other, each one smaller. |
| `noise` | The surface is pushed out and in. |
| `stretch` | The model is taller or shorter. |

They are combined with the named looks, and `options.variations` makes a set
of blueprints that are all the same kind of thing but not quite the same
thing.

### Resources and prompts

Resources are read-only and are how the AI learns the rules without being
told:

* `hazeron://vocabulary/rooms`
* `hazeron://vocabulary/stations`
* `hazeron://vocabulary/base-shapes`
* `hazeron://templates/ships`
* `hazeron://templates/buildings`
* `hazeron://schema/brief`
* `hazeron://shapes/presets`
* `hazeron://limits/faces`
* `hazeron://shapes/sets`

One prompt, `design_a_ship`, is a playbook: ask what the ship is for, pick a
template, adjust the room and station lists, generate, validate, render, and
iterate on what the picture shows.

### What has to happen first

1. `--json` output on the existing commands, so the server has a stable
   machine-readable surface while the JSON request layer is built.
2. The JSON request layer in the core, with the operations above.
3. The Python server, one tool per operation.
4. The tool descriptions. These matter more than the code: an AI picks a
   tool by reading its description, so they have to say what the tool is for
   and what it returns.

### Tests

* Every tool gets a test that runs it against a known design and checks the
  reply.
* An end-to-end test: give the server a model and a brief, ask it to generate,
  validate and render, and check the blueprint that comes out.
* A test that the AI cannot break: a request with a bad part id, a room type
  that is not in the vocabulary, a model that is not there. Each has to come
  back as `ok: false` with a message, never as a crash.

---

## 4. The offline designer, afterwards

### What it is

A standalone program that opens a `.SoH` blueprint and lets it be edited by
hand, with the generator built in. No game server, no database, no client.

### What has to be written

| Piece | Work | Notes |
|---|---|---|
| The view shell | camera, orbit, zoom, grid, axes | The camera and the GL preview already exist in `src/gui/preview.cpp`. |
| Picking | ~400 lines | Turn the mouse into a ray, `IntersectRay` into `AuScFaceList`, then `SelectFaceAt` for the vertex or edge. All in the data model. |
| Selection and highlight | ~300 lines | `AuScSelection` and `AuScSelectInfo` are already there. |
| Move, rotate, scale | ~1,000 lines | The operations are in `AuScParts`. The gizmo and the mouse handling are not. |
| Undo and redo | ~150 lines | `AuDesignUndoRedoTransaction` (`auatom/dadesigner.h:79`) is a snapshot of the parts added, changed and deleted, which `AuScParts` already hands out. Reimplement it over the core document rather than pulling in the atom layer. |
| The command palette | ~1,200 lines | The 102 `ddsc*` files are thin wrappers over `AuScParts`. Only a focused subset is needed: make hull, make room, make door, make window, cut, fuse, divide, join, delete, copy, paste, transform, texture. |
| Property dialogs | mostly reuse | 29 of the game's dialogs are pure Qt and can be lifted as they are. |
| Texture painting | ~500 lines | The effects live in `AuScFaceList` (`EffectCylindrical` and friends). The unwrapper UI is the work. |
| Building tools | ~400 lines | The site, the build zone, fences, trees and shrubs. They are ordinary parts. |
| Ship and building modes | ~300 lines | Pick which kind of blueprint is being edited and which rules apply. |

Total: roughly **4,000-5,000 lines**. The 3D interaction is the long pole, not
the line count.

### How it shares with the MCP server

Both are front ends over one `Document`:

```cpp
class Document
{
    AuScDesign  m_ship;        // or AuBdDesign
    AuScSelection m_selection;
    UndoStack    m_undo;
public:
    // The same operations the MCP server calls.
    bool AddRoom(const QString& type, const AuPointArray& size, ...);
    bool MovePart(quint16 partId, const AuPoint& delta);
    ...
};
```

The GUI calls `Document` directly. The JSON request layer calls `Document`
too. The MCP server calls the JSON layer. Nothing is implemented twice.

### Phases

| Phase | What | Done when |
|---|---|---|
| 1 | The view shell: open a blueprint, orbit, see it | The existing preview, with a file menu |
| 2 | Picking and selection | Click a face and it highlights; shift-click adds |
| 3 | Undo and redo | Every change is undoable, all the way back to opening the file |
| 4 | Move, rotate, scale | A room can be dragged and the blueprint stays valid |
| 5 | The command palette | Hull, room, door, window, cut, fuse, delete, duplicate |
| 6 | Property dialogs | The lifted dialogs work |
| 7 | Texture painting | A face can be given the hull texture and a room can be painted |
| 8 | Building tools | A site, fences and trees can be placed |
| 9 | The generator inside the designer | The wizard is a panel, not a separate program |

Each phase leaves a usable program. The generator is worth having in the
designer from phase 1, because it is what makes the blueprints worth
editing.

---

## 5. Open questions

* **The industry table.** The real one is in the game database, so a
  standalone tool uses the stand-in table in `src/building.cpp`. Buildings
  validate against it, but the numbers are not the game's. Worth asking
  whether the table can be exported from a game install.
* **How much of the game client is worth pulling in.** The 46 client-free
  designer files are worth lifting. The rest is not, and the atom layer should
  stay out of it.
* **Whether the MCP server should hold the design or the core should.** The
  plan is that the core holds it and the server is stateless, so several
  clients can share one document. That is a small design change if it turns
  out to be wrong.
* **Rendering without a display.** The MCP server needs `design_render` to
  work headless. The preview needs OpenGL 3.3; a machine without it gets no
  picture. A software rasteriser would fix that. Worth knowing how often that
  matters.

---

## 6. Where the code is now

| File | What it holds |
|---|---|
| `src/generate.cpp` | Hull analysis, the layout strategies, model import, the ship validation |
| `src/building.cpp` | The building generator, the building templates, the industry table, the "largest rectangle inside the model" search |
| `src/names.cpp` | The names the game itself uses |
| `src/gui/` | The graphical tool, and the OpenGL preview that `design_render` would reuse |
| `test.sh` | The matrix test, 94 blueprints with no errors |
| `HANDOFF.md` | The state of the work |

The pieces the plan needs that do not exist yet:

* `Document` in the core, with a selection and an undo stack.
* The JSON request layer.
* The Python MCP server.
* The tool descriptions.
