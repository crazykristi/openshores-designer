# Shipgen documentation

Tools that make Shores of Hazeron blueprints (`.SoH`) offline, linked
against the game's own engine source so that the engine's own report checks
everything they write.

| Program | What it is for |
|---|---|
| `shipgen` | The command line: inspect, audit and validate blueprints; generate ships and buildings from a 3D model, an existing hull or a primitive base; thin out models; build drawn plans. |
| `shipgen-gui` | The generator as a wizard, with a 3D view and the engine's report. |
| `shipgen-mesh` | Load a model, thin it to a number of faces, compare, tidy up, save. |
| `shipgen-designer` | **New.** Draw a ship with triangles: hull, rooms, doors, ladders, paint; build the blueprint. |

## The pages

| Page | Read it for |
|---|---|
| [SOH_FORMAT.md](SOH_FORMAT.md) | What a blueprint file contains, part types, how rooms, voids, doors and paths work, and what newer game files hold that the source does not know. |
| [DESIGNER.md](DESIGNER.md) | The triangle designer: how to use it, how a plan becomes a blueprint, the plan file. |
| [AETHER_KIT.md](AETHER_KIT.md) | The Aether kit: ships and buildings in a high-tech and magic style, and how their look is made. |
| [TEXTURE_LIBRARY.md](TEXTURE_LIBRARY.md) | The game's library textures by id, and how example blueprints make glowing, see-through, animated surfaces. |
| [COMMAND_REFERENCE.md](COMMAND_REFERENCE.md) | Every command and option of every program, with defaults. |
| [SOURCE_CODE_REFERENCE.md](SOURCE_CODE_REFERENCE.md) | Where things are in `src/` and in the engine source. |
| [TROUBLESHOOTING.md](TROUBLESHOOTING.md) | Build, run and blueprint problems. |
| [CHANGES_AND_KNOWN_ISSUES.md](CHANGES_AND_KNOWN_ISSUES.md) | What changed, and what is still open. |
| [WINDOWS.md](WINDOWS.md) | Building on Windows with MSYS2 (written, not yet tried on a Windows machine). |
| [MCP_AND_OFFLINE_DESIGNER.md](MCP_AND_OFFLINE_DESIGNER.md) | The plan for an MCP server and a full offline designer. |

The root `README.md` describes the generator in depth (briefs, templates,
buildings, painting, shapes, the mesh tool); `HANDOFF.md` is the state of the
work for whoever picks it up next.

## Start here

```bash
cd ~/Documents/hazeron-shipgen
./build.sh                                   # about 3 minutes
export LD_LIBRARY_PATH=$HOME/.local/hazeron-toolchain/usr/lib64

# Look at a blueprint.
./build/bin/shipgen inspect ~/Documents/designs/whitestar-notele.SoH

# Generate a frigate from a model.
./build/bin/shipgen brief military_frigate frigate.json
./build/bin/shipgen generate --model testdata/testhull.stl --brief frigate.json --out out/

# Draw one instead.
./build/bin/shipgen-designer

./test.sh                                    # about 6 minutes
```

## Status (30 October 2026, evening)

* After the first test in the game: hulls and rooms are now wound the way
  the game wants (the hull was see-through), ships are turned fore to +Y,
  one engineer runs a chosen FTL drive, doors and windows are cut through
  the hull the way the game's designer does it, and hull doors get a clear
  way in from the ground. See CHANGES_AND_KNOWN_ISSUES.md.
* The Aether kit: seven blueprints in a high-tech and magic style.

## Status earlier on 30 October 2026

* The generator, the building generator, the mesh tool and the graphical
  tool work and are covered by `test.sh`.
* Doors and voids were wrong in every generated blueprint until this session
  (rooms only existed while a door was open); fixed. Regenerate anything made
  before.
* Blueprints from newer game builds are now read completely.
* The triangle designer is a working first version.
* Nothing has been tried in the game this session. The MCP server has not
  been started. The Windows build has not been run on Windows.
