# Git and releases

The code is on GitHub: <https://github.com/crazykristi/openshores-designer>.
The main program is the triangle designer (`shipgen-designer`); releases are
an AppImage that also holds `shipgen-gui` and the `shipgen` command line.

## What is in the repository, and what is not

| In git | Not in git (`.gitignore`) |
|---|---|
| `src/`, `docs/`, `tests/`, `test.sh`, the build and packaging scripts | `build/`: the build output, the AppImage, `build/tools/appimagetool` |
| `third_party/soh/`: the engine source the build uses (Shores of Hazeron licence, `third_party/soh/LICENSE`) | `third_party/SoH-source/`: the OpenShores source, a git repository of its own (886 MB), for reference only |
| `testdata/` (the small test hull, the Aetherite capitol reference) | `testdata/models/`: the big 3D models (over 25 MB each); the tests that need them skip without them, and `testdata/extract_models.sh` puts them back from the "3d models" folder |
| `exported/`: the user's exported reference ships and buildings | `work/`: generated test output and the in-game test rounds, rebuilt from the plans |
| `packaging/`: the AppImage icon | |

To get the OpenShores source back after a fresh clone:

```bash
git clone --depth 1 https://github.com/Celarious/SoH-source.git third_party/SoH-source
```

## Getting the code and building

```bash
git clone https://github.com/crazykristi/openshores-designer.git
cd openshores-designer
./build.sh            # about 3 minutes; programs in build/bin/
./test.sh             # about 6 minutes; should end "... 0 with errors."
```

The build needs a C++ compiler and the Qt 5 headers and `moc` (the README's
*Building* section; the toolchain on the user's machine is in
`~/.local/hazeron-toolchain`).

## Day to day

```bash
git status                       # what changed
git add -A                       # everything not ignored
git commit -m "What and why"     # one change per commit, a short first line
git push                         # to GitHub
```

* Run `./test.sh` before committing a change to the code. A commit that
  breaks the tests should say so.
* Bigger or riskier work can go on a branch: `git switch -c sloped-hull`,
  commit there, then `git switch main && git merge sloped-hull`.
* GitHub asks for your user name and, as the password, a **personal access
  token** (*Settings > Developer settings > Personal access tokens*, scope
  `repo`), not your account password. KDE's password window hides the user
  name as you type it too; that is normal. To be asked less often:
  `git config --global credential.helper "cache --timeout=86400"` (a day),
  or the GitHub CLI: `sudo dnf install gh`, `gh auth login`,
  `gh auth setup-git`.
* Never commit `build/`, `work/` or the big models; `.gitignore` keeps them
  out. Check `git status` before `git add -A` if a new kind of file
  appears.

## Making a release

Versions are `MAJOR.MINOR.PATCH`, with `-alpha` or `-beta` for test
releases. The releases so far:

| Version | Date | |
|---|---|---|
| [0.3.0-alpha](https://github.com/crazykristi/openshores-designer/releases/tag/v0.3.0-alpha) | 31 October 2026 | The first on GitHub, a pre-release. (The designer test builds before it were called alpha1 and alpha2.) |
| [0.3.1-alpha](https://github.com/crazykristi/openshores-designer/releases/tag/v0.3.1-alpha) | 31 October 2026 | A pre-release with bug fixes: the model import kept no plan; clear feedback on why an import went wrong. |
| [0.3.2-alpha](https://github.com/crazykristi/openshores-designer/releases/tag/v0.3.2-alpha) | 31 October 2026 | A pre-release: 3D models sliced and made the smooth hull, simplified to the face budget; turn and flip; contrail and weapon emitters; tooltips. The AppImage is built on Ubuntu 22.04. |
| [0.3.3-alpha](https://github.com/crazykristi/openshores-designer/releases/tag/v0.3.3-alpha) | 31 October 2026 | A pre-release: angular and classic hull styles; sharp hulls from 3D models; a choice of way down at hull doors; fixed ships with empty lowest decks hanging the game. |
| [0.3.5-alpha](https://github.com/crazykristi/openshores-designer/releases/tag/v0.3.5-alpha) | 1 November 2026 | A pre-release: zero-G shafts; the designer's task bar, background shaping and a switch for redrawing while drawing; paint over the whole model hull; better generated buildings; macOS builds from source. There was no 0.3.4. |
| [0.3.6-alpha](https://github.com/crazykristi/openshores-designer/releases/tag/v0.3.6-alpha) | 2 November 2026 | A pre-release: fixes from trying a battleship in the game (crew routes, ladders, doorways, door rows), integrated desks for sitting or standing, fire control choice, repeatable builds, a Mac download. |

1. **Set the version** in `src/version.h` (`SHIPGEN_VERSION`). The
   designer's window title, `--version` of all three programs and the
   AppImage's file name follow it.
2. **Write the release notes** in `RELEASE_NOTES.md`: what the designer
   does, what is new, what has not been tried in the game, known limits.
   Move the details into `docs/CHANGES_AND_KNOWN_ISSUES.md` as usual.
3. **Build and test:** `./build.sh && ./test.sh`, which should end
   "... 0 with errors."
   For a bug-fix release, put a *Bug fixes* section first, naming what was
   wrong in the version before and what changed (as 0.3.1-alpha does).
4. **Package:** `packaging/build-appimage-portable.sh` makes
   `build/shipgen-designer-<version>-x86_64.AppImage` on Ubuntu 22.04 in a
   container (`./package-appimage.sh` makes it from this machine's own
   build, for this machine's glibc and newer), and
   `packaging/build-windows-cross.sh` makes
   `build/shipgen-designer-<version>-windows-x86_64.zip`. Try it:
   `build/shipgen-designer-<version>-x86_64.AppImage`, then
   `... gui` and `... shipgen --version`.
5. **Commit and tag:**
   ```bash
   git add -A
   git commit -m "Release 0.3.0-alpha"
   git tag -a v0.3.0-alpha -m "Shipgen Designer 0.3.0-alpha"
   git push origin main --tags
   ```
6. **Publish on GitHub:** *Releases > Draft a new release*, choose the tag,
   title it "Shipgen Designer 0.3.0-alpha", paste `RELEASE_NOTES.md`,
   attach the AppImage and the Windows zip, tick *Set as a pre-release* for an alpha or beta,
   and *Publish*. With the GitHub CLI instead:
   ```bash
   gh release create v0.3.0-alpha build/shipgen-designer-0.3.0-alpha-x86_64.AppImage \
       build/shipgen-designer-0.3.0-alpha-windows-x86_64.zip \
       --title "Shipgen Designer 0.3.0-alpha" --notes-file RELEASE_NOTES.md --prerelease
   ```

7. **The Mac download** comes from GitHub, since it has to be built on a Mac:
   pushing the tag starts *Actions > macOS build*. If the release was not
   published by the time the build ended, the dmg stays with the run; run the
   workflow by hand (*Run workflow*, with the tag filled in) to add it to the
   release. The same builds any older tag. Mention the dmg in the release text
   once it has been tried on a real Mac.

### What the AppImage needs

It bundles Qt 5 and its X11 and Wayland plugins, OpenAL and the other
libraries; the C library, the OpenGL drivers, X11 and the font libraries
come from the system it runs on. It therefore runs only where the C library
is at least as new as on the machine that built it (Fedora 44 for
0.3.0-alpha and 0.3.1-alpha). Since 0.3.2-alpha the release AppImage is
built on Ubuntu 22.04 in a container, `packaging/build-appimage-portable.sh`
(podman or docker; the image is `packaging/containers/Containerfile.linux`),
so it runs with glibc 2.35 and newer. It was tried in a bare Ubuntu 22.04
container with only the desktop's usual libraries added.

### The Windows zip

`packaging/build-windows-cross.sh` builds the Windows programs on Linux,
with the MinGW-w64 cross compiler and Qt 5 from Fedora's packages in a
container (`packaging/containers/Containerfile.windows`), and packs them
with their DLLs and Qt plugins into
`build/shipgen-designer-<version>-windows-x86_64.zip`. No Windows machine
is needed to make it; Wine (which Proton is built on) is enough to try it:

```bash
wine build/windows-dist/shipgen-designer-<version>/shipgen-designer.exe
wine build/windows-dist/shipgen-designer-<version>/shipgen.exe --version
```

0.3.2-alpha's zip was tried under Wine 11: the command line builds the
example plan with no engine errors, and the designer opens, shapes and
builds it. It has not been run on Windows itself yet.

The licence notice of the engine source (`third_party/soh/LICENSE`) is
packed into the AppImage (`usr/share/doc/shipgen/`), as the licence asks.
It also asks for a royalty if a fee is charged for the software, so the
releases are free.
