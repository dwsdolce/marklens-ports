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

## Build

There are two ways to build, and which you want depends on what you are doing.

### The whole thing, ready to run

```bash
packaging/build_win        # Git Bash or Cygwin
packaging\build_win.bat    # cmd
packaging/build_mac        # macOS
packaging/build_linux      # Linux
```

This is the one to start with, especially on Windows. It configures, builds,
stages the app with its assets into `dist/`, copies the Qt runtime in beside it,
and produces an installer. Add `app` to stop after the deploy and skip the
installer, which is the faster loop while working:

```bash
packaging/build_win app
```

Either way the result runs straight off the disk with nothing on `PATH`.

### Just the compile, for iterating

```bash
cmake -B build
cmake --build build --config Release
```

Faster, and what you want while changing code — but it only compiles. Nothing
is deployed, so on Windows the executable cannot find Qt's DLLs and **exits
silently, with no window and no error**. Put `<Qt>/6.11.1/msvc2022_64/bin` on
`PATH` before running it:

Rather than doing that by hand, use the run script, which finds whichever
build exists and sorts out `PATH` from `CMakeCache.txt`:

```bash
packaging/run_win ../shared/spec/sample/index.md      # run_win.bat in cmd
packaging/run_mac ../shared/spec/sample/index.md
packaging/run_linux ../shared/spec/sample/index.md
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
tree above is left alone. The Qt runtime is deployed with `windeployqt`,
`macdeployqt` or `linuxdeploy` + its Qt plugin, and `shared/` is installed
beside the executable for `assets::sharedDir()` to find. See
[../packaging/README.md](../packaging/README.md) for the per-platform tools and
for signing.

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
