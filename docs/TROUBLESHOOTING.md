# Troubleshooting

## Building

**`error: could not find .../qglobal.h` or `moc`.** `build.sh` expects the
toolchain in `~/.local/hazeron-toolchain` (Qt 5 headers and libraries under
`usr/`, `moc` under `usr/lib64/qt5/bin`, a g++ under `bin/`). Set
`TOOLCHAIN=<dir>` to use another one. On Windows see `WINDOWS.md`.

**`third_party/soh` is missing.** `build.sh` clones
`https://github.com/Hazeron-Freehold-LLC/Shores-of-Hazeron.git` into it. The
copy in the project is a trimmed one; a fresh clone works too.

**A change to a header did not take.** `build.sh` rebuilds an object when its
own source or any header in the same directory is newer. A header in another
directory (an engine header, say) does not trigger it: `./build.sh clean`
and build again.

**A new `.cpp` in `src/` links twice.** Add it to the `case` near "These live
in the core library" in `build.sh` (see `SOURCE_CODE_REFERENCE.md`).

## Running

**`error while loading shared libraries: libQt5Core.so.5`.**
`export LD_LIBRARY_PATH=$HOME/.local/hazeron-toolchain/usr/lib64`.

**A new program crashes the first time it validates a design.** It has not
made the engine's global data object; copy the four lines from the start of
`main()` in `src/main.cpp`.

**The 3D view is blank on a machine with no display.** Use the toolchain's
Xvfb:

```bash
$HOME/.local/hazeron-toolchain/x11/Xvfb :99 -screen 0 1600x1000x24 &
DISPLAY=:99 LIBGL_ALWAYS_SOFTWARE=1 ./build/bin/shipgen-gui ...
```

The offscreen Qt platform gives the widget no OpenGL context, so it draws
nothing there.

## Blueprints

**`inspect` says `Note: Part N has type T, which this engine build does not
know`.** The file was saved by a newer game build. Everything else in it is
read; the unknown part is kept and written back by `resave` and by the
graphical tool's save. See `SOH_FORMAT.md` section 4.

**`generate --hull x.SoH` says the hull is not a closed solid.** Rooms can only
be checked against a closed hull. Many real blueprints are built from open
shells (the White Star is six of them). `--allow-open-hull` places rooms
against the silhouette instead, unverified.

**`inspect` lists "Rooms that are only there while a door is open".** The
room's only void is tied to a door's open state, so the game only has the
room while that door is open. Blueprints made by the generator before
30 October 2026 all have this; generate them again.

**"Rooms without a door".** No doorway void (and no ladder shaft leading to a
room that has one) touches the room's void.

**The engine says "Door N has no room void associated with its open
state".** The door opens onto nothing. Tie a doorway void to its open state
(`AddInteriorDoor` in `src/interior.h` shows how).

**A hull opens (open edges) after it has been saved.** The engine drops
triangles whose corners are on one line. Do not write slivers.

**A blueprint is too big for the game.** Keep the hull to about 5,000 faces
(`--faces`, or the designer's face budget). The packed size limit is 28 MB for
a ship and 14 MB for a building.

## The designer

**Room triangles are hatched red.** They are not inside the shaped hull:
the rounding has cut the corner off there. Draw the hull one triangle wider,
use less smoothing, or more skin.

**The hull is over the face budget.** The note in step 2 says so. Less
smoothing, a bigger cell, or a smaller ship bring it down; the budget can
also be raised to about 15,000.

**A door cannot be placed.** It has to stand on the edge between two
different rooms, or between a room and something that is not a room (that is
a hull door). Click the triangle that stands on that edge.

**A ladder cannot be placed.** The whole cell (all four triangles) has to be
in a room on this deck and in a room on the deck above.

## Tests

`./test.sh` runs everything and prints one line per check. A `FAIL` line is
followed by the first lines of what went wrong. The working files are in
`work/test/`; `work/test/interior.txt` and `work/test/audit.txt` hold the
last `inspect` and `audit` output.
