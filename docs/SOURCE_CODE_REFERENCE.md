# Source code reference

Where things are. Every file has a header comment that says what it is for;
this is the map.

## Layout

```
hazeron-shipgen/
├── build.sh, build-windows.ps1, deploy-windows.sh   the build (see WINDOWS.md for Windows)
├── build_probe.sh                                   builds one program from tests/
├── test.sh                                          the test matrix
├── src/                                             the core library (libShipgenCore) and the CLI
│   ├── gui/                                         shipgen-gui
│   ├── meshapp/                                     shipgen-mesh
│   └── designer/                                    shipgen-designer
├── tests/                                           small probes of engine behaviour
├── testdata/                                        test models, the reference Capitol
├── third_party/soh/                                 the engine source (a trimmed copy of v7b2)
└── work/                                            test output; work/old-docs has the replaced docs
```

`build.sh` compiles every `src/*.cpp` into `libShipgenCore.a`. The command
line tool links that library plus its own `main.cpp`; a module that must
exist only once (because it holds state or is large) is listed in the
`case` near "These live in the core library" in `build.sh`, and a new core
module belongs there too.

## The core library (`src/`)

| File | What it holds |
|---|---|
| `shipgen.h`, `brief.cpp` | The brief (rooms, stations, berths, system volumes), the ship templates, brief JSON. `SanitiseName`. |
| `names.h/.cpp` | The names the game uses: spacecraft room types (from `ROOMTYPE`), building room types, station types, industries. |
| `generate.h/.cpp` | `AnalyseHull` (extents, silhouettes, closed solids, volume, usable interior), `GenerateLayout` (the decks/spine/blocks strategies, room fitting, doors), `FurnishRooms` (stations, berths, panels, weapons, system volumes for rooms that are already there), `AllocateSystemVolumes` (size the systems again after the hull changed), `FtlDriveChoices`/`FtlDriveModule`, `QuarterTurnToFore`, `ValidateDesign` (the engine's report), `ImportHullModel` (models into hull parts: repair, shape, thinning, base, paint), `CopyHull`. |
| `interior.h/.cpp` | The parts that make an interior walkable: rooms (mesh + constant void), interior doors with doorway voids, hull doors with doorway and hull voids and the constant hull void in front, ladder shafts and ladder paths, walk paths. `OrientAsHull`/`OrientAsRoom`/`OrientAsVoid` (the winding the game wants), `JigCut` (cutting doors and windows with the engine's own `JigCutParts`, near faces only, with a clean frame), `HullPartAt`. The comment at the top says how the engine reads voids. Used by the generator, the building generator and the designer. |
| `aether.h/.cpp` | The Aether kit: looks (texture slot, paint, light, effect, glow), the kit palette of library textures, shapes (frustum, torus, gear, dome, crystal, halo, sheet), and the recipes of the seven blueprints. |
| `building.h/.cpp` | The building generator, building templates, building sets, the stand-in industry table (`InstallIndustryTable`), the largest-rectangle search. |
| `audit.h/.cpp` | `AuditSolidity` (open, thick, pinched and folded edges; `ClosedButForOpenings` for a hull with door and window holes), `SolidMesh` (exact point and box containment with a BVH; a vote of seven rays, and some slack by the openings, for a hull with holes), `AuditDesign`. |
| `mesh.h/.cpp` | Reading 3ds/stl/obj, writing stl/obj, quadric and clustering simplification, welding, cleaning, fixing windings, filling holes, grounding, mirroring, smoothing. |
| `shape.h/.cpp` | Taper, twist, repeat, stack, noise, stretch, and the named looks. |
| `base.h/.cpp` | The rectangle, circle and hexagon base solids. |
| `paint.h/.cpp` | Textures and colour tiles, palette slots, UV projections. |
| `sohfile.h/.cpp` | `LoadDesign`/`SaveDesign`: reading files written by newer game builds, keeping parts of unknown types. `ReadDesignBytes`. |
| `triplan.h/.cpp` | The designer's plan: the triangle grid per deck, rooms, doors, windows, ladders, palette, FTL drive; JSON; the example. |
| `planbuild.h/.cpp` | A plan into a blueprint: the hull surface (prisms, subdivision, skin, smoothing, flat merging), room meshes, doors, windows, ladders, paths, the quarter turn to fore +Y, furnishing. |
| `logging.h/.cpp` | One Qt message handler for every tool; it drops the engine's obstruction-cache warning that does not apply offline. |
| `main.cpp` | The `shipgen` command line. |

## The programs

| Directory | Program | Files |
|---|---|---|
| `src/gui/` | `shipgen-gui` | `mainwindow` (project panel, file list, report), `wizard` (the generation wizard), `preview` (the OpenGL 3D view, shared by all three GUI programs), `shapepanel`, `basepanel`, `filelist`. |
| `src/meshapp/` | `shipgen-mesh` | `meshwindow`: load, thin out, compare, tidy up, save. |
| `src/designer/` | `shipgen-designer` | `planview` (the triangle grid editor), `designerwindow` (the steps, undo, the 3D view, the report). |

## The engine (`third_party/soh/`)

Only these libraries are built: `auutil`, `aumath`, `augl`, `auglobal`,
`audata`. The rest of the game (client, server, designer UI) is not in the
copy. Where to look:

| What | Where |
|---|---|
| The file container | `audata/audesign.cpp:429` (`SaveToFile`, `LoadFromDataStream`) |
| A spacecraft design | `audata/auscdesign.h/.cpp` |
| A building design | `audata/aubddesign.h/.cpp` |
| Every part class, the part types, the part list | `audata/auscparts.h` (`PART_TYPE` at line 1228), `audata/auscparts.cpp` |
| The report (what makes a blueprint valid) | `audata/auscreport.cpp`, `audata/aubdreport.cpp` |
| Faces, triangles, selections | `aumath/auscface.h` (`SURFACE_EFFECT`, `COLOR_THEME`, per-face paint, opacity, light, glow) |
| Cutting doors and windows | `audata/auscparts.cpp:1800` (`AuScParts::JigCutParts`), `aumath/auscface.cpp:4244` (`AuScFaceList::JigCut`) |
| The texture library | `augl/autexturelib.h` (ids), `augl/augleffects.cpp` (`s_TextureLibrary`, names and resource files) |
| Where fore is, and the size limits | `audata/auscparts.h:7167` ("Fore = +Y"), `audata/auscreport.cpp:1841` (length along Y), `auscreport.cpp:690-735` (the hull must reach z 0 and the other extents) |
| Room types | `auutil/audef.h:605` |
| File types | `auutil/aufilever.h` |

The engine needs a global data object, which the game client normally
makes. Every program makes a bare one at the start of `main()`
(`gp_AuGlobal = new AuGlobal(...)` and `InstallIndustryTable`). A new
program that builds or validates designs must do the same, or it crashes in
the first validation.

## Conventions

* Comments say what and why in plain words; a header comment per file.
* Tabs for indentation, braces on their own lines, `m_` for members.
* Anything that iterates a `QHash` and adds up floating point numbers or
  makes choices must not depend on the order: Qt 5 seeds the hash per
  process. Use `QMap` or sort first (see `CollapseFlat` and the smoothing in
  `planbuild.cpp`, and the hull measurement bug in `CHANGES_AND_KNOWN_ISSUES.md`).
* `AuScPart::Faces()` *replaces* the list it is given; collect per part.
