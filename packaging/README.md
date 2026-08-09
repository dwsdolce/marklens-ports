# Packaging

Each port builds its own installers, from its own directory, with the same
three script names:

```
python/packaging/build_win  build_win.bat  build_mac  build_linux
cpp/packaging/build_win     build_win.bat  build_mac  build_linux
rust/packaging/build_win    build_win.bat  build_mac  build_linux
```

Run one from anywhere — each `cd`s to its port's root first:

```bash
python/packaging/build_win        # Git Bash or Cygwin
python\packaging\build_win.bat    # cmd
cpp/packaging/build_mac dmg
rust/packaging/build_linux
```

## Setup first

Each port has a setup step that leaves it ready to build, and the build scripts
need nothing exported afterwards:

```bash
python/packaging/setup     # or: pip install -e ".[packaging]" in a venv
cpp/packaging/setup        # cpp\packaging\setup.bat in cmd
rust/packaging/setup       # rust\packaging\setup.bat in cmd
```

Pass `--check` to any of them to report without changing anything.

They sort requirements into two kinds, and say which is which:

- **Prerequisites** — the C++ toolchain, CMake, Qt 6, the Rust toolchain, the
  Linux webview packages. Platform SDKs, often behind an account or a package
  manager wanting root. Setup verifies them and says exactly what is missing
  and where to get it; it never installs them. Qt's optional *modules* are the
  one exception: if Qt is present but WebEngine is not, the maintenance tool
  can add it without an interactive download, so setup offers to.
- **Dependencies** — md4c for C++, built into `cpp/third_party/`; the Tauri CLI
  for Rust, via `cargo install`. Python has neither category to worry about,
  because `pyproject.toml` covers everything once you have a venv.

The asymmetry is real rather than an oversight: `pyproject.toml` and
`Cargo.toml` fetch dependencies as a matter of course, and C++ has no
equivalent in the standard toolchain, so `packaging/setup` plus a `third_party/`
prefix stands in for one.

## Build and run

Every port takes the same two verbs, on every platform:

```bash
<port>/packaging/build_win  [app|installer]     # build_win.bat in cmd
<port>/packaging/build_mac  [app|dmg|pkg]
<port>/packaging/build_linux [app|appimage]     # rust also: deb, rpm
<port>/packaging/run_win                        # run_win.bat in cmd
<port>/packaging/run_mac
<port>/packaging/run_linux
```

`app` stops once there is something runnable and skips the packaging step,
which is the fast loop while working. The packaging verb — the default —
carries on and produces the installer. `help` prints the modes.

`run_*` is the development tool: it runs the **working tree**, building first
where that means anything, and never touches `dist/`. One answer per platform
to "how do I run this", rather than one per port:

| Port | What it does |
|------|--------------|
| Python | runs the source through the venv, as `python -m marklens` does — no build step exists |
| C++ | configures and builds `build/` if needed, then runs it with Qt's DLLs on `PATH` |
| Rust | `cargo run`, which rebuilds whatever changed |

The C++ case earns its keep on Windows: a plain `cmake --build` leaves an
executable that cannot find Qt's DLLs and exits silently, with no window and no
error. `run_win` reads `Qt6_DIR` out of `CMakeCache.txt` and fixes `PATH`, so
that failure mode never reaches you.

Build numbers come from `git rev-list --count HEAD` in all three ports, asked at
the point each one can: Python at runtime, C++ at configure time, Rust in
`build.rs`. A packaged build falls back to the number stamped into
`<port>/build/installer_version`, for a source tarball with no git to ask.

## What comes out

| Port   | Windows                       | macOS                        | Linux                                   |
|--------|-------------------------------|------------------------------|-----------------------------------------|
| Python | `Marklens_Python_V<ver>.exe`  | `Marklens_Python_V<ver>.dmg` | `Marklens-Python-<ver>-<arch>.AppImage` |
| C++    | `Marklens_Cpp_V<ver>.exe`     | `Marklens_Cpp_V<ver>.dmg`    | `Marklens-Cpp-<ver>-<arch>.AppImage`    |
| Rust   | `Marklens_Rust_V<ver>.exe`    | `Marklens_Rust_V<ver>.dmg`   | `Marklens-Rust-<ver>-<arch>.AppImage` + `.deb` |

Windows installers land in `<port>/installer/`, AppImages and Linux packages in
`<port>/dist/`, disk images in `<port>/`.

The three install **side by side** on purpose — that is what the repository is
for. Each has its own display name (`Marklens Python`, `Marklens C++`,
`Marklens Rust`), its own executable name, its own Windows AppId, and its own
macOS bundle identifier, so installing one never disturbs another.

## What has actually been run

These scripts were written on Windows and have only run there. The
applications they package are a separate matter — all three ports were
developed on macOS, and it is the packaging, not the ports, that is untried
elsewhere.

Exercised on Windows 11, with CMake 4.4, Visual Studio 2026 (MSVC 14.51),
Qt 6.11.1, Rust 1.97 and Inno Setup 6:

| Port   | Windows                                                    |
|--------|------------------------------------------------------------|
| Python | installer built, app launches from the frozen bundle       |
| C++    | installer built, `ctest` 3/3, app launches from `dist/`    |
| Rust   | installer built, fixtures 2/2                              |

**No packaging script has been run on macOS or Linux.** `setup`, `build_mac`,
`build_linux`, `run_mac` and `run_linux` are written from the documented
behaviour of `macdeployqt`, `create-dmg`, `linuxdeploy` and `appimagetool`, not
from a working build. Treat the first run on either as a debugging session, not
a release.

## Versions

`tools/gen_version_build.py <port>` produces a four-part version: the port's own
declared base version plus `git rev-list --count HEAD`.

```
python   python/pyproject.toml            project.version
cpp      cpp/CMakeLists.txt               project(... VERSION ...)
rust     rust/src-tauri/tauri.conf.json   version
```

It writes `<port>/build/installer_version`, which the Inno Setup scripts read.
A file rather than an ISCC `/D` argument because Git Bash rewrites any argument
that looks like a Unix path while Cygwin passes the `//` escape that fixes it
through literally; a file works in both, and in `cmd`, and in the Inno Setup
IDE.

The Rust bundle itself keeps the three-part version, because Tauri requires
valid semver there. The build number still appears in the installer file name.

## Icons

`tools/make_ico.py` generates `shared/icon.ico` from `shared/icon.png`. It is
generated rather than committed so there is no third copy of the artwork to
keep in step with the PNG and the `.icns`, and every Windows build script runs
it first. Pure standard library — no Pillow — so the C++ and Rust ports need
Python only for this and the version stamp.

## Shared pieces

`inno-existing-install.iss` and `inno-markdown-assoc.iss` are `#include`d by the
Python and C++ Inno Setup scripts. The first detects an earlier install of the
same AppId and offers to uninstall it, install elsewhere, or cancel. The second
adds the app to the **"Open with"** list for `.md`, `.markdown`, `.mdown` and
`.mkd` — deliberately not as the default handler, because three ports of one
viewer quietly fighting over every Markdown double-click is not a decision an
installer gets to make.

## Per-port requirements

**Python** — a venv with the packaging extra (`pip install -e ".[packaging]"`);
the scripts accept either `python/.venv` or a shared `.venv` at the repository
root. PyInstaller collects PySide6 wholesale, QtWebEngine included. Windows
also needs [Inno Setup 6](https://jrsoftware.org/isdl.php); macOS needs
`create-dmg`; Linux needs
[appimagetool](https://github.com/AppImage/AppImageKit/releases) at `~/bin` or
`$APPIMAGETOOL`.

**C++** — CMake, Qt 6 with WebEngineWidgets, md4c, and Python 3. Set
`CMAKE_PREFIX_PATH` to the Qt installation (semicolon-separated if md4c lives
somewhere of its own). No generator or compiler is forced: CMake picks the
platform default, which must match the ABI of the Qt build you point it at —
MSVC for Qt's official Windows binaries, Apple Clang for Homebrew Qt, system
GCC on Linux. The Qt runtime is deployed with `windeployqt` / `macdeployqt` /
`linuxdeploy` + its Qt plugin (at `~/bin` or `$LINUXDEPLOY` and
`$LINUXDEPLOY_PLUGIN_QT`). Packaging builds into `cpp/build-packaging/` and
stages into `cpp/dist/`, leaving the `cpp/build/` tree from `cpp/README.md`
alone.

Two Windows snags worth knowing. **Qt WebEngine is not part of the base Qt
install** — add it with the maintenance tool
(`MaintenanceTool.exe search qtwebengine` lists the package names, then
`MaintenanceTool.exe install extensions.qtwebengine.<ver>.<kit>`). And **md4c
has no Windows package**; either `vcpkg install md4c` and set
`CMAKE_TOOLCHAIN_FILE`, or build it from source, which is quicker:

```bash
git clone https://github.com/mity/md4c && cd md4c
cmake -B build -DCMAKE_INSTALL_PREFIX=C:/md4c
cmake --build build --config Release
cmake --install build --config Release
```

**Rust** — Rust, the Tauri CLI (`cargo install tauri-cli --version "^2.0"`) and
Python 3. Tauri's own bundler does the packaging rather than Inno Setup: it is
the only one of the three that installs the WebView2 runtime the app cannot
start without, and it builds the disk image and AppImage itself. Tauri also
registers the Markdown file types as a full association rather than an
"Open with" entry — the one place the three ports differ, and the reason to
install this one last if you want it to own `.md`.

## Signing

Off by default, and read from the environment — this repository is public and a
Developer ID does not belong in it. Unsigned builds work fine locally;
Gatekeeper warns on first launch and right-click → Open gets past it.

```bash
# Python and C++ (PyInstaller / macdeployqt / codesign):
export CODESIGN_IDENTITY="Developer ID Application: Your Name (TEAMID)"
export INSTALLER_IDENTITY="Developer ID Installer: Your Name (TEAMID)"   # pkg only
export NOTARY_PROFILE="notarytool-profile"

# Rust (Tauri reads its own names):
export APPLE_SIGNING_IDENTITY="Developer ID Application: Your Name (TEAMID)"
export APPLE_KEYCHAIN_PROFILE="notarytool-profile"
```

Windows code signing is not wired up in any of the three; add a `signtool` step
after the installer is produced if you need it.
