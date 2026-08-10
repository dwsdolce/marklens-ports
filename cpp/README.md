# Marklens — C++ / Qt port

A Markdown viewer: `QWebEngineView` renders the same HTML/CSS/JS the other ports
use; C++ does Markdown→HTML (via **md4c**), link routing, and file watching.

## Setup

```bash
cd cpp
packaging/setup            # checks the environment, installs what it can
packaging/setup --check    # report only, change nothing
```

C++ has no manifest format that fetches dependencies the way `pyproject.toml`
does, so this script stands in for one. It sorts the requirements in two:

**Prerequisites — you install these.** A C++ toolchain, CMake ≥ 3.19, and Qt 6
with the WebEngine module. These are platform SDKs measured in gigabytes, often
behind an account or a package manager wanting root, so `setup` verifies them
and tells you exactly what is missing and where to get it, but never installs
them itself. They have the standing the Python interpreter has for the Python
port.

macOS: `brew install cmake qt`. Linux: your distribution's `qt6` and
`build-essential` packages. Windows: [Visual Studio](https://visualstudio.microsoft.com/downloads/)
with *Desktop development with C++*, and the
[Qt online installer](https://www.qt.io/download-qt-installer) — WebEngine is
**not** in the base install, and it needs Qt WebChannel and Qt Positioning
alongside it. If Qt is already there but those modules are not, `setup` adds
them with the maintenance tool.

**Dependencies — `setup` installs these.** Just md4c: small, MIT, CMake-based,
and with no Windows package at all. It is built into `third_party/`, which
`CMakeLists.txt` searches first — the nearest thing to a venv this port has. An
md4c already installed system-wide is used as-is.

Once `setup` reports **Ready**, nothing needs to be exported: `CMakeLists.txt`
locates Qt and md4c itself, and `CMAKE_PREFIX_PATH`, `QTDIR` and `MD4C_ROOT`
are overrides rather than requirements.

## Run

```bash
packaging/run_win ../shared/spec/sample/index.md      # run_win.bat in cmd
packaging/run_mac ../shared/spec/sample/index.md
packaging/run_linux ../shared/spec/sample/index.md
```

This is the tool for iterating on the code. It always runs the working tree,
configuring and building `build/` first if it needs it, then running the
result with Qt's DLLs on `PATH` — without which the executable exits
silently, with no window and no error.
Read the script if you want the details; there is nothing in it you could not
type yourself, which is rather the point.

It never runs anything under `dist/` — that is what **Package** is for.

## Build

`packaging/run_*` does this for you; these are the same two commands by hand.

```bash
cmake -B build
cmake --build build --config Release
```

Where the binary lands depends on the generator, which is easy to trip over:

| Platform | Generator | Executable |
|----------|-----------|------------|
| Windows  | Visual Studio (multi-config) | `build/Release/marklens-cpp.exe` |
| Linux    | Makefiles or Ninja | `build/marklens-cpp` |
| macOS    | Makefiles or Ninja | `build/Marklens C++.app` |

The macOS target is named for the port so all three can be installed side by
side; elsewhere the executable is `marklens-cpp`.

Point `CMAKE_PREFIX_PATH` at a Qt somewhere unusual, or `MD4C_ROOT` at an md4c
of your own, if the search does not find what you want. The two trees are kept
apart: `build/` is yours, and the packaging scripts use `build-packaging/`, so
neither clobbers the other.

## Test

```bash
ctest --test-dir build --output-on-failure
ctest --test-dir build --output-on-failure -C Release   # Windows: name the config
```

Three tests, all headless — ctest sets the offscreen platform and the
QtWebEngine sandbox flags for the GUI ones:

| Test | What it covers |
|------|----------------|
| `core` | The shared fixtures: 12 render cases and 11 link cases from `../shared/spec/fixtures/` |
| `smoke` | Rendering — image, mermaid, table, syntax highlighting |
| `navigation` | Clicks a link in a real `MainWindow` and verifies it navigates without trapping |

Run one directly with, e.g.,
`QT_QPA_PLATFORM=offscreen ./build/nav_smoke`.

## Package

```bash
packaging/build_win        # Git Bash or Cygwin -> installer/Marklens_Cpp_V<ver>.exe
packaging\build_win.bat    # cmd, same output
packaging/build_mac        # -> Marklens_Cpp_V<ver>.dmg  (or `build_mac pkg`)
packaging/build_linux      # -> dist/Marklens-Cpp-<ver>-<arch>.AppImage
```

These configure into `build-packaging/` and stage into `dist/`, so the `build/`
tree above is left alone, and `shared/` is installed beside the executable for
`assets::sharedDir()` to find.

### Deploying the Qt runtime

A Qt program cannot just be copied: it needs its libraries, its platform and
image-format plugins, and — because this uses QtWebEngine — the
`QtWebEngineProcess` helper plus Chromium's `.pak` and ICU data. Each platform
has a tool that works out what those are and copies them in. **Two of the three
come with Qt; the third does not, which is the entire source of the confusion:**

| Platform | Tool | Where it comes from |
|---|---|---|
| Windows | `windeployqt` | **Comes with Qt**, in `<kit>/bin`. Nothing to install. |
| macOS | `macdeployqt` | **Comes with Qt**, in `<kit>/bin`. Nothing to install. |
| Linux | `linuxdeploy` **+** `linuxdeploy-plugin-qt` | **Two separate downloads.** Not Qt products. |

`build_win` and `build_mac` need no help finding theirs: `PATH` first, then the
`bin/` of the Qt that CMake recorded in `CMakeCache.txt` (`build_win` also tries
each `CMAKE_PREFIX_PATH` root). Reading it out of the cache means the deploy
tool always matches the Qt that built the executable. If you have a Qt that can
build this port you already have the tool that deploys it — which is why
`packaging/setup` does not check for either.

Linux is the odd one out because **Qt ships no `linuxdeployqt`**. `linuxdeploy`
is a third-party tool, and Qt awareness is a *plugin* for it — hence two files,
not one.

**`packaging/setup` downloads both for you**, into `packaging/tools/` at the
repository root. They are single-file AppImages behind a plain HTTPS download —
no account, no root, no installer — which puts them in the same class as md4c,
and the same rule applies: anything installable without your say-so goes into
the checkout, gitignored, and disappears when the checkout does. There is
nothing to do by hand.

That also settles two things that otherwise bite. The release assets carry an
architecture suffix, and `setup` saves them under the bare names — which is
what `build_linux` looks for, and what `linuxdeploy` itself looks for when it
searches `PATH` for its Qt plugin. And `packaging/tools/` is inside the project,
so it works whether or not `~/bin` is on your `PATH`, which by default it is not
on a good many distributions.

`build_linux` looks in `$LINUXDEPLOY` / `$LINUXDEPLOY_PLUGIN_QT`, then `PATH`,
then `~/bin`, then `packaging/tools/` — so a copy you installed yourself still
wins, and nothing is downloaded on top of it. Running an AppImage needs FUSE —
on Ubuntu 22.04 and later, `sudo apt install libfuse2`.

The installer format on top is separate again: Windows needs
[Inno Setup 6](https://jrsoftware.org/isdl.php), macOS needs `create-dmg`
(`brew install create-dmg`), and Linux needs nothing more, since `linuxdeploy
--output appimage` produces the AppImage itself. `packaging/setup` reports all
of these. See [../packaging/README.md](../packaging/README.md) for signing.

Only the Windows packaging path has actually been run; see the repository
[README](../README.md) for what that means.

## Notes

- **Assets are found at runtime, not baked in.** `assets::sharedDir()` looks
  first for a `shared/` beside the executable — `Contents/Resources/shared`
  inside a macOS bundle — which is what the packaged builds ship. It falls back
  to `MARKLENS_SHARED_DIR`, the absolute path baked in at configure time, so a
  development build reads straight out of the repository. `MARKLENS_SHARED`
  overrides both, which is how you point a packaged build at a working copy.
- **md4c matched the fixtures with no per-engine fixups** — strikethrough is
  `<del>` natively, where the Python markdown-it-py port needed an override.
- No sandbox → no folder-grant machinery. The page's base URL is the document's
  folder, so relative images and links resolve natively.
- The app links as a GUI-subsystem binary on Windows (`WIN32_EXECUTABLE`).
  Without it, launching the viewer also opens a console window; the test
  executables deliberately stay console so ctest captures their output.
