# Shipgen documentation

Tools that make Shores of Hazeron blueprints (`.SoH`) offline, linked
against the game's own engine source so that the engine's own report checks
everything they write.

| Program | What it is for |
|---|---|
| `shipgen-designer` | **The main program.** Draw a ship with triangles: hull, rooms, doors, windows, ladders, stairs, ramps, paint; build the blueprint. |
| `shipgen-gui` | Secondary: the generator as a wizard, with a 3D view and the engine's report. |
| `shipgen` | The command line: inspect, audit and validate blueprints; generate ships and buildings from a 3D model, an existing hull or a primitive base; thin out models; build designer plans. |
| `shipgen-mesh` | Load a model, thin it to a number of faces, compare, tidy up, save. |

Releases are an AppImage of the designer holding `shipgen-gui` and the
command line too. The code is at
<https://github.com/crazykristi/openshores-designer>.

## The pages

| Page | Read it for |
|---|---|
| [DESIGNER.md](DESIGNER.md) | The triangle designer: how to use it, how a plan becomes a blueprint, the plan file. |
| [GIT_AND_RELEASES.md](GIT_AND_RELEASES.md) | Getting the code, working with git, and making a release (version, AppImage, GitHub). |
| [BUILDING_TODO.md](BUILDING_TODO.md) | The work list agreed with the user: buildings, ships, the designer's open items. |
| [CHANGES_AND_KNOWN_ISSUES.md](CHANGES_AND_KNOWN_ISSUES.md) | What changed, day by day, after each test in the game, and what is still open. |
| [SOH_FORMAT.md](SOH_FORMAT.md) | What a blueprint file contains, part types, how rooms, voids, doors, paths, stairs, ramps and lighting work, and what newer game files hold. |
| [AETHER_KIT.md](AETHER_KIT.md) | The Aether kit: ships and buildings in a high-tech and magic style, and how their look is made. |
| [TEXTURE_LIBRARY.md](TEXTURE_LIBRARY.md) | The game's library textures by id, and how example blueprints make glowing, see-through, animated surfaces. |
| [COMMAND_REFERENCE.md](COMMAND_REFERENCE.md) | Every command and option of every program, with defaults. |
| [SOURCE_CODE_REFERENCE.md](SOURCE_CODE_REFERENCE.md) | Where things are in `src/` and in the engine source. |
| [TROUBLESHOOTING.md](TROUBLESHOOTING.md) | Build, run and blueprint problems. |
| [WINDOWS.md](WINDOWS.md) | Building on Windows with MSYS2 (written, not tried on Windows; postponed by the user). |
| [MCP_AND_OFFLINE_DESIGNER.md](MCP_AND_OFFLINE_DESIGNER.md) | The idea of an MCP server and a full offline designer (not started; waits on the user). |

The root `README.md` describes the generator in depth (briefs, templates,
buildings, painting, shapes, the mesh tool); `HANDOFF.md` is the state of the
work for whoever picks it up next; `RELEASE_NOTES.md` is the current
release's notes.

## Start here

```bash
git clone https://github.com/crazykristi/openshores-designer.git
cd openshores-designer
./build.sh                                   # about 3 minutes
./build/bin/shipgen-designer                 # draw a ship

# Look at a blueprint.
./build/bin/shipgen inspect ~/Documents/designs/whitestar-notele.SoH

# Generate a frigate from a model.
./build/bin/shipgen brief military_frigate frigate.json
./build/bin/shipgen generate --model testdata/testhull.stl --brief frigate.json --out out/

./test.sh                                    # about 6 minutes
./package-appimage.sh                        # the release AppImage, in build/
```

The current state, and what changed after each test in the game, is in
[CHANGES_AND_KNOWN_ISSUES.md](CHANGES_AND_KNOWN_ISSUES.md) and `HANDOFF.md`.
