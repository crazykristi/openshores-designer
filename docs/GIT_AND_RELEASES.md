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

1. **Set the version** in `src/version.h` (`SHIPGEN_VERSION`). The
   designer's window title, `--version` of all three programs and the
   AppImage's file name follow it.
2. **Write the release notes** in `RELEASE_NOTES.md`: what the designer
   does, what is new, what has not been tried in the game, known limits.
   Move the details into `docs/CHANGES_AND_KNOWN_ISSUES.md` as usual.
3. **Build and test:** `./build.sh && ./test.sh`, which should end
   "... 0 with errors."
4. **Package:** `./package-appimage.sh` makes
   `build/shipgen-designer-<version>-x86_64.AppImage`. Try it:
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
   attach the AppImage, tick *Set as a pre-release* for an alpha or beta,
   and *Publish*. With the GitHub CLI instead:
   ```bash
   gh release create v0.3.0-alpha build/shipgen-designer-0.3.0-alpha-x86_64.AppImage \
       --title "Shipgen Designer 0.3.0-alpha" --notes-file RELEASE_NOTES.md --prerelease
   ```

### What the AppImage needs

It bundles Qt 5 and its X11 and Wayland plugins, OpenAL and the other
libraries; the C library, the OpenGL drivers, X11 and the font libraries
come from the system it runs on. It therefore runs only where the C library
is at least as new as on the machine that built it (Fedora 44 for
0.3.0-alpha). Building it on an older distribution (for example in a
container) would let it run on more systems; that is not set up yet.

The licence notice of the engine source (`third_party/soh/LICENSE`) is
packed into the AppImage (`usr/share/doc/shipgen/`), as the licence asks.
It also asks for a royalty if a fee is charged for the software, so the
releases are free.
