# Marklens — C++ / Qt port

A Markdown viewer: `QWebEngineView` renders the same HTML/CSS/JS the other ports
use; C++ does Markdown→HTML (via **md4c**), link routing, and file watching.

## Requirements

- Qt 6 **with the WebEngine module**, and md4c
- CMake ≥ 3.19, a C++17 compiler

macOS: `brew install qt md4c`. On Windows neither comes for free — the base Qt
installer leaves WebEngine out (and WebEngine in turn needs Qt Positioning and
Qt WebChannel, which the maintenance tool does not always pull in), and md4c has
no package at all. See [../packaging/README.md](../packaging/README.md) for the
exact commands; the short version is:

```bash
C:/Qt/MaintenanceTool.exe --accept-licenses --default-answer --confirm-command     install extensions.qtwebengine.6111.win64_msvc2022_64             qt.qt6.6111.addons.qtwebchannel qt.qt6.6111.addons.qtpositioning
git clone https://github.com/mity/md4c && cd md4c
cmake -B build -DCMAKE_INSTALL_PREFIX=<prefix> && cmake --build build --config Release
cmake --install build --config Release
```

## Build

```bash
cd cpp
cmake -B build -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt
cmake --build build
```

On Windows, `CMAKE_PREFIX_PATH` is a semicolon-separated list and needs both:
`"C:/Qt/6.11.1/msvc2022_64;<md4c prefix>"`. No generator is forced, so CMake
picks the newest Visual Studio it knows about and finds the compiler itself.

## Run

```bash
./build/marklens-cpp ../shared/spec/sample/index.md
```

## Test

```bash
ctest --test-dir build --output-on-failure   # all three, headless (offscreen)
```

Three tests: **core** (shared fixtures), **smoke** (render — image, mermaid,
table, highlight), **navigation** (clicks a link in the real MainWindow and
verifies it navigates without trapping). ctest sets the offscreen env for the
GUI ones. Run one directly with, e.g.,
`QT_QPA_PLATFORM=offscreen ./build/nav_smoke`.

## Package

```bash
export CMAKE_PREFIX_PATH=/opt/homebrew/opt/qt   # or C:/Qt/6.8.0/msvc2022_64
packaging/build_win        # Git Bash or Cygwin -> installer/Marklens_Cpp_V<ver>.exe
packaginguild_win.bat    # cmd, same output
packaging/build_mac        # -> Marklens_Cpp_V<ver>.dmg  (or `build_mac pkg`)
packaging/build_linux      # -> dist/Marklens-Cpp-<ver>-<arch>.AppImage
```

These configure into `build-packaging/` and stage into `dist/`, so the `build/`
tree above is left alone. The Qt runtime is deployed with `windeployqt`,
`macdeployqt` or `linuxdeploy` + its Qt plugin, and `shared/` is installed next
to the executable for `assets::sharedDir()` to find. See
[../packaging/README.md](../packaging/README.md) for the per-platform tools and
for signing.

## Notes

- `test_core` loads `../shared/spec/fixtures/*.json` — the same contract the
  Python port satisfies. 25/25 rows pass.
- **md4c matched the fixtures with no per-engine fixups** — strikethrough is
  `<del>` natively (the Python markdown-it-py port needed an override).
- No sandbox → no folder-grant machinery. Base URL is the document's folder, so
  relative images/links resolve natively.
- `MARKLENS_SHARED_DIR` is baked at configure time, and is only the fallback:
  `assets::sharedDir()` first looks for a `shared/` next to the executable
  (`Contents/Resources/shared` inside a macOS bundle), which is what the
  packaged builds ship. `MARKLENS_SHARED` overrides both.
- On macOS the target is named `Marklens C++`, so the dev build is
  `build/Marklens C++.app`; elsewhere it is `build/marklens-cpp`.
