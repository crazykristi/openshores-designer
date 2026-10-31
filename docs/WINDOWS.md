# Building on Windows

The graphical tool and the mesh tool build on Windows with **MSYS2**, the
**MinGW-w64** compiler and **Qt 5**. The command line tool is built by the
same script and can be copied with them.

> **Status.** This Windows path has not been run on a Windows machine in the
> development workspace. The Linux build and the full matrix test still pass,
> and the Windows-specific parts have been reviewed, but expect to fix a
> platform detail or two the first time. The error messages below are the
> places to start.

The Linux build is unchanged: `./build.sh` still writes to `build/`. In an
MSYS2 shell the same script detects Windows and writes to `build-windows/`
with `.exe` suffixes.

---

## 1. Install MSYS2 and the packages

Install [MSYS2](https://www.msys2.org/) for 64-bit Windows, normally at
`C:\msys64`. Open the **MSYS2 MINGW64** shell, not the plain MSYS2 shell.

Update the packages first. If `pacman` asks to close the terminal, do so and
open the MINGW64 shell again:

```bash
pacman -Syu
```

Then install the compiler, Qt 5 and OpenAL Soft:

```bash
pacman -S --needed \
    base-devel git zip \
    mingw-w64-x86_64-toolchain \
    mingw-w64-x86_64-qt5-base \
    mingw-w64-x86_64-qt5-tools \
    mingw-w64-x86_64-openal \
    mingw-w64-x86_64-pkgconf
```

The important pieces are:

| Piece | Used for |
|---|---|
| `mingw-w64-x86_64-toolchain` | `g++`, `ar`, `ranlib`, the MinGW runtime |
| `mingw-w64-x86_64-qt5-base` | Qt Core, Gui, Widgets, Network, Sql and OpenGL; also `moc` and `rcc` |
| `mingw-w64-x86_64-qt5-tools` | `windeployqt` for the deployment step |
| `mingw-w64-x86_64-openal` | OpenAL Soft headers and `libopenal-1.dll` |
| `mingw-w64-x86_64-pkgconf` | optional; finds the OpenAL include directory |

Check that the shell is the right one:

```bash
which g++
which qmake-qt5
qmake-qt5 -query QT_VERSION
```

`g++` should be under `/mingw64/bin` and Qt should report a 5.x version.
The plain MSYS2 shell has a different `PATH` and is not the one to use.
The `build.sh` script also accepts a plain `qmake` on systems where that is
the Qt 5 tool. Here the MSYS2 package calls it `qmake-qt5`.

---

## 2. Build

From the MINGW64 shell:

```bash
cd /c/Users/you/Documents/hazeron-shipgen
./build.sh -j8
```

Use `-j 8` if you prefer. The project path must not contain spaces; the
build script checks this and stops with a clear message. Put the checkout
somewhere like `C:\hazeron-shipgen` if your user name has a space in it.

The script downloads the engine into `third_party/soh` if that folder is
missing, then builds:

```text
build-windows/bin/shipgen.exe
build-windows/bin/shipgen-gui.exe
build-windows/bin/shipgen-mesh.exe
build-windows/bin/shipgen-designer.exe
```

The first build takes a few minutes. Later builds only recompile what
changed. `./build.sh clean` removes `build-windows/`; it does not touch the
Linux `build/` folder.

The script uses the Qt that `qmake` reports, so Qt does not have to be in a
fixed place. To use a different compiler or qmake:

```bash
CXX=g++ QMAKE=/mingw64/bin/qmake-qt5.exe ./build.sh -j8
```

### Starting from PowerShell

If you would rather not open the MSYS2 shell yourself, run the wrapper from
PowerShell in the project folder:

```powershell
.\build-windows.ps1
.\build-windows.ps1 -Jobs 8
.\build-windows.ps1 -Jobs 8 -Deploy
```

It starts the MSYS2 MINGW64 environment and runs `build.sh`, and with
`-Deploy` runs `deploy-windows.sh` as well. It expects MSYS2 at `C:\msys64`;
pass `-Msys2Root` for another location. Execution policy may block a local
script; in that case use
`powershell -ExecutionPolicy Bypass -File .\build-windows.ps1` or run the
commands in the MSYS2 shell directly.

---

## 3. Make a folder that runs on another PC

The build tree uses DLLs from `/mingw64/bin`, which will not be on a normal
Windows machine. Run the deployment script in the same MINGW64 shell:

```bash
./deploy-windows.sh
```

It creates:

```text
build-windows/dist/
    shipgen.exe
    shipgen-gui.exe
    shipgen-mesh.exe
    shipgen-designer.exe
    Qt5Core.dll, Qt5Gui.dll, Qt5Widgets.dll, ...
    platforms/qwindows.dll
    OpenAL32.dll (or libopenal-1.dll)
    libwinpthread-1.dll, libstdc++-6.dll, libgcc_s_seh-1.dll
```

Copy the **whole `dist` folder** to the Windows machine, not just the `.exe`
files. Windows finds the DLLs and the `platforms` plugin next to the
program. The folder can be zipped:

```bash
cd build-windows
zip -r shipgen-windows-x64.zip dist
```

`deploy-windows.sh` uses Qt's `windeployqt` for the Qt DLLs and plugins, and
then copies the MinGW and OpenAL runtime next to the programs. If a Qt or
OpenAL DLL is still reported as missing at launch, the error usually names
it; copy it from `C:\msys64\mingw64\bin` into `dist`.

### A note on the GUI subsystem

`shipgen-gui.exe`, `shipgen-mesh.exe` and `shipgen-designer.exe` are built as Windows GUI programs,
so they do not open a console window. The command line tool
`shipgen.exe` is a console program. The GUI tools still accept their
command line switches, but their error messages are not visible in a console;
use the command line tool or a debugger for diagnostics.

---

## 4. Smoke test

These are the first things to try after building. They exercise the engine,
the model reader and the Qt windows. Run them from the MINGW64 shell.

```bash
cd /c/Users/you/Documents/hazeron-shipgen

# The CLI links the engine and Qt.
./build-windows/bin/shipgen.exe templates

# A building brief and a blueprint made from a primitive base, painted.
./build-windows/bin/shipgen.exe building-brief office_block "$HOME/office.json"
mkdir -p "$HOME/shipgen-smoke"
./build-windows/bin/shipgen.exe generate-building \
    --base rectangle --brief "$HOME/office.json" \
    --texture '#3a6ea5' --paint by-face \
    --out "$HOME/shipgen-smoke" --layout grid

# The engine's own report should read it back.
for f in "$HOME/shipgen-smoke"/*.SoH; do
    ./build-windows/bin/shipgen.exe validate "$f"
    ./build-windows/bin/shipgen.exe inspect "$f" | grep -E 'slot |UVs on'
done

# The mesh tool opens the model, thins it to 2,000 faces, saves a picture
# and exits. On a normal desktop it can also be started directly.
./build-windows/bin/shipgen-mesh.exe \
    --open testdata/models/aetherite_top.stl \
    --faces 2000 --screenshot "$HOME/mesh.png"

# The graphical tool can save a screenshot of its own window.
./build-windows/bin/shipgen-gui.exe \
    --wizard-kind building \
    --screenshot "$HOME/gui.png"
```

If the smoke test works, run the deployed copies:

```text
build-windows\dist\shipgen-gui.exe
build-windows\dist\shipgen-mesh.exe
```

To check that a model is closed and can be used as a volume, open it in the
mesh tool, use **Clean up**, **Close the holes** and **Fix windings**, then
check the volume and area figures. A model with open edges is reported rather
than silently accepted.

---

## 5. Troubleshooting

### `qmake: command not found`

You are in the wrong MSYS2 shell. Close it, open **MSYS2 MINGW64**, and run
`which g++` to confirm it is `/mingw64/bin/g++`. The Qt 5 package may call the
tool `qmake-qt5` rather than `qmake`; the build script tries both.

### `al.h: No such file or directory`

The OpenAL package is missing or its headers are not in the path that the
engine expects. The engine uses `<al.h>` on Windows, while OpenAL Soft
installs `AL/al.h`. `build.sh` adds `-I/mingw64/include/AL` when it can find
it. Check:

```bash
ls /mingw64/include/AL/al.h
pkg-config --cflags openal
```

If OpenAL is installed somewhere unusual:

```bash
OPENAL_CFLAGS="-I/path/to/include/AL" \
OPENAL_LIBS="-L/path/to/lib -lopenal" \
./build.sh -j8
```

### `undefined reference to ...`

Usually a Qt module or OpenAL is missing from the link. Check that
`qmake-qt5 -query QT_INSTALL_LIBS` points at the MINGW64 Qt and that
`ls "$(qmake-qt5 -query QT_INSTALL_LIBS)"` contains `libQt5OpenGL.dll.a` and
`libQt5Sql.dll.a`. The build script links Qt Core, Gui, Widgets, Network, Sql
and OpenGL; all of those are needed by the engine subset.

### The program starts and immediately exits

Run `shipgen.exe` from a console first. If the GUI program has a missing
DLL, Windows normally shows a dialog; on a machine with no display, check
the Windows event log. Keep `dist` intact instead of copying a single
`.exe`. `windeployqt` must be run on Windows; it cannot be run on Linux.

### The preview area is blank, or says there is no OpenGL

The preview needs a desktop OpenGL 3.3 context. The tool asks for one on
Windows. Remote Desktop, very old Intel drivers and virtual machines without
3D acceleration may only offer an older or OpenGL ES context. The rest of
the graphical tool still works: the wizard, validation, parts list and
saving do not need the preview. Updating the graphics driver is the usual
fix. The mesh tool can still simplify and save models without the preview.

### `0xc000007b` or a "bad image" error

A 32-bit and 64-bit DLL have been mixed. The build uses the x86_64 MINGW64
packages; do not copy 32-bit Qt or OpenAL DLLs into `dist`.

### Where are the log files?

The engine writes `SoH stdout.txt` and `SoH stderr.txt` in the current
working directory on Windows. Start the program from a writable folder if
you need the logs.

### The build uses the Linux engine copy

The Windows build uses the same `third_party/soh` sources. The engine already
has `Q_OS_WIN` branches for logging, fonts, symbols and OpenAL; the build
script only changes compiler flags, Qt discovery and the OpenGL/OpenAL
libraries.

---

## 6. What is and is not ported

Ported and built by the same sources:

* the command line generator, inspector and validator;
* the graphical wizard, preview, validation panel, parts tree and file list;
* the mesh tool: model list, simplify/reset, repair, view modes and saving;
* primitive bases, model import, buildings and aircraft-style layouts.

Not included:

* an installer, code signing or an auto-updater;
* a Windows-specific test suite. `test.sh` is the Linux regression matrix.
  The smoke test above is the starting point for a Windows one;
* 32-bit Windows. The instructions use x86_64 MSYS2 only.

The GUI and mesh tool keep the same features as the Linux versions. If you
find a Windows-only failure, the first useful facts are the exact command,
the compiler and Qt versions (`g++ --version`, `qmake-qt5 -query QT_VERSION`) and
the first ten lines of the build output.
