# Shipgen Designer

An offline ship and building designer for **Shores of Hazeron** and
**OpenShores**. Draw a ship deck by deck with triangles, draw its rooms, doors,
windows, ladders and stairs on the same triangles, paint it, and build a
blueprint (`.SoH`) the game loads. It is linked against the game's own engine
source, so every blueprint is checked by the engine's own report.

## Download

Get the latest release from
<https://github.com/crazykristi/openshores-designer/releases>:

* **Linux:** `shipgen-designer-<version>-x86_64.AppImage`. Make it executable
  and run it. It needs glibc 2.35 or newer (Ubuntu 22.04, Debian 12, Fedora 36
  and later) and OpenGL 3.3.
* **Windows (experimental):** unpack `...-windows-x86_64.zip` and start
  `shipgen-designer.exe`.
* **macOS (experimental):** `...-macos-arm64.dmg` (Apple processors) or
  `...-macos-x86_64.dmg` (Intel). The app is not signed: open it once, then
  choose *Open Anyway* under *System Settings > Privacy & Security*.

The designer can update itself: *Help > Check for a new version...*

## Quick start

1. *File > New spaceship...* (or *New building...*, or *New spaceship from a
   3D model...*).
2. **Hull:** draw each deck's outline with triangles.
3. **Rooms:** draw the rooms, then the doors (at least one to the outside),
   windows, ladders and stairs.
4. **Crew** (optional): place stations, berths and pads yourself; whatever you
   leave out is put in from the ship's template.
5. **Paint:** colours, the game's textures, a finish, decals.
6. **Build:** check the report and the expected stats, then save the
   blueprint into the game's design folder.

See [Designing a valid blueprint](https://github.com/crazykristi/openshores-designer/wiki/Designing-a-valid-blueprint)
for what a design needs to work in the game: rooms, stations, berths, hull
volume and the game's limits.

## What it does

* **Hulls** from triangles on a grid, shaped smooth, angular or classic within
  a face budget, or from a **3D model** (STL, OBJ, 3DS, FBX, glTF and more)
  used as the hull or only sliced into decks.
* **Rooms** of the game's types, doors (also tall and in floors), hull doors
  with a ramp or ladder, windows, ladders, stairs, ramps, turbolifts and
  zero-G shafts. Walk paths for the crew are made for you.
* **Furnishing** from a template: stations, berths and systems sized to the
  hull, with a choice of FTL and maneuver drive.
* **Paint**: plain or empire colours, the game's textures, finishes, surface
  effects, a brush in the 3D view and decals.
* **Buildings**, space stations and fighters (experimental) as well as ships.
* **AI assistants** (experimental): `shipgen-designer --mcp` lets an AI app
  such as Claude Desktop or Claude Code design in the open window while you
  watch. See [Designing with an AI assistant](https://github.com/crazykristi/openshores-designer/wiki/Designing-with-an-AI-assistant).

The release also contains `shipgen` (the command line: inspect, audit and
check blueprints, build plans), `shipgen-gui` (an automatic generator) and
`shipgen-mesh` (thin a 3D model).

## More

* [The wiki](https://github.com/crazykristi/openshores-designer/wiki): the
  command line, buildings, the generator, notes and limits, and building from
  source.
* [Release notes](RELEASE_NOTES.md)
* **Problems?** Open an issue at
  <https://github.com/crazykristi/openshores-designer/issues>. A screenshot, the
  plan file (`.json`) and what you expected help most.

## Building from source

```bash
./build.sh      # Qt 5 and a C++ compiler; programs in build/bin/
./test.sh       # should end "... 0 with errors."
```

Details are on the wiki page
[Building from source](https://github.com/crazykristi/openshores-designer/wiki/Building-from-source).

## Licence

Includes source from Shores of Hazeron, copyright Hazeron Freehold LLC, under
the Shores of Hazeron licence (`third_party/soh/LICENSE`). Releases are free.
