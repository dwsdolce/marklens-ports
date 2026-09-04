# Packaging

Each port builds its own installers, from its own directory, with the same
script names:

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
python/packaging/setup     # with the repository-root venv activated
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

Every port takes the same three verbs, on every platform:

```bash
<port>/packaging/build_win  [app|installer]     # build_win.bat in cmd
<port>/packaging/build_mac  [app|dmg|pkg]
<port>/packaging/build_linux [app|appimage|deb|rpm]
<port>/packaging/run_win                        # run_win.bat in cmd
<port>/packaging/run_mac
<port>/packaging/run_linux
<port>/packaging/test_win                       # test_win.bat in cmd
<port>/packaging/test_mac
<port>/packaging/test_linux
```

`app` stops once there is something runnable and skips the packaging step,
which is the fast loop while working. The packaging verb — the default —
carries on and produces the installer. `help` prints the modes.

On Linux the packaging verb is a comma-separated list of formats, and the
default is all three: `appimage,deb,rpm`. The AppImage runs anywhere without
being installed; the `.deb` and `.rpm` are for people who would rather their
package manager knew about it.

All three ports ship the same way in every format — self-contained, with their
own copy of Qt or the Python runtime — so which format you pick changes how it
is installed and nothing about what runs. The `.deb` and `.rpm` put that tree
under `/opt/<name>` with a launcher in `/usr/bin`; `packaging/make_linux_package`
builds both from a staged directory and explains the layout. The Rust port is
the exception only in mechanism: Tauri bundles all three formats itself, and it
depends on the system webview rather than carrying one.

Every Linux package carries its licence paperwork under `share/doc/<name>`:
the project's MIT licence, a NOTICE describing the bundled Qt, and the
LGPL-3.0 and GPL-3.0 texts it refers to, staged by
`packaging/collect_licenses` from `shared/licenses`. linuxdeploy separately
collects a copyright file for every library it takes from a distribution
package, which is why the C++ build prints a "Could not find copyright files"
warning for each file no package owns — our own executable, and all of Qt.
Those warnings are expected and worth keeping: they are how a library that
stopped being collected would show up.

Two tools are needed that the setup scripts cannot download for you, because
they are system packages rather than single-file releases: `dpkg-deb` for the
`.deb` (part of `dpkg`) and `rpmbuild` for the `.rpm` (`apt install rpm`, or
`dnf install rpm-build`). Setup reports each as a note rather than a
requirement, and a build with one missing still produces the other formats and
says which it skipped. The Rust port needs neither — Tauri writes both formats
itself.

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
| Python | `Marklens_Python_V<ver>.exe`  | `Marklens_Python_V<ver>.dmg` | `Marklens-Python-<ver>-<arch>.AppImage` + `.deb` + `.rpm` |
| C++    | `Marklens_Cpp_V<ver>.exe`     | `Marklens_Cpp_V<ver>.dmg`    | `Marklens-Cpp-<ver>-<arch>.AppImage` + `.deb` + `.rpm`    |
| Rust   | `Marklens_Rust_V<ver>.exe`    | `Marklens_Rust_V<ver>.dmg`   | `Marklens-Rust-<ver>-<arch>.AppImage` + `.deb` + `.rpm` |

Windows installers land in `<port>/installer/`, AppImages and Linux packages in
`<port>/dist/`, disk images in `<port>/`.

The three install **side by side** on purpose — that is what the repository is
for. Each has its own display name (`Marklens Python`, `Marklens C++`,
`Marklens Rust`), its own executable name, its own Windows AppId, and its own
macOS bundle identifier, so installing one never disturbs another.

## What has actually been run

These scripts were written on Windows and have since been exercised on Linux and
on macOS. What follows is what was actually observed, platform by platform, so a
reader can tell a tested path from a plausible one.

Exercised on Windows 11, with CMake 4.4, Visual Studio 2026 (MSVC 14.51),
Qt 6.11.1, Rust 1.97 and Inno Setup 6:

| Port   | Windows                                                    |
|--------|------------------------------------------------------------|
| Python | installer built, app launches from the frozen bundle       |
| C++    | installer built, `ctest` 5/5, app launches from `dist/`    |
| Rust   | installer built, fixtures 2/2                              |

Exercised on Linux Mint 22.3 (Ubuntu 24.04 base, Cinnamon), with CMake 3.28.3,
GCC 13.3, Qt 6.11.1, Rust 1.97, Python 3.14, WebKitGTK 2.52.3, dpkg 1.22.6 and
rpm 4.18.2:

| Port   | Linux                                                                       |
|--------|-----------------------------------------------------------------------------|
| Python | AppImage, `.deb` and `.rpm` built; installed from the `.deb`; 36 tests pass |
| C++    | AppImage, `.deb` and `.rpm` built; installed from the `.deb`; `ctest` 3/3; renders from the installed tree |
| Rust   | AppImage, `.deb` and `.rpm` built; installed from the `.deb`; fixtures pass; opens a `.md` from the file manager |

macOS packaging was run and proven on macOS during the month of development
there, across all three ports. The toolchain versions and the per-port evidence
are not written down yet — they need filling in from that machine, in the same
shape as the two tables above.

The `.rpm`s were built and their contents checked by extraction, but not
installed: this is a Debian-family machine, and `rpm -i` on one is not a test
of anything. An rpm distribution is still the untried case for that format.

**No packaging script has been run on macOS.** `build_mac`, `run_mac` and
`test_mac` are written from the documented behaviour of `macdeployqt` and
`create-dmg`, not from a working build. Treat the first run there as a
debugging session, not a release.

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

Tauri requires valid semver in the bundle, and `0.1.0.31` is not, so the Rust
Linux packages declare `0.1.0+31` — the build number as semver build metadata,
passed with `cargo tauri build --config` so `tauri.conf.json` keeps the plain
version it is meant to hold. It has to change between builds: dpkg and rpm
compare that field and nothing else, so a version that never moved made every
install a reinstall rather than an upgrade. The Windows installer still takes
the build number from the file name.

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

**Python** — one venv, at the repository root (`marklens-ports/.venv`), with the
dev extra installed, and **activated before you run anything**. The scripts use
the interpreter they are given rather than going looking for one, so the venv
you activate is the venv they use; `python/packaging/setup` is the only thing
that checks, and it treats a second venv at `python/.venv` as an error.
PyInstaller collects PySide6 wholesale, QtWebEngine included — which is why
this port needs no Qt deploy tool at all. Windows also needs
[Inno Setup 6](https://jrsoftware.org/isdl.php) and macOS `create-dmg`
(`brew install create-dmg`) — neither of which `setup` can install for you, so
both are reported rather than fetched. Linux needs `appimagetool`, which `setup`
*does* download, into `packaging/tools/` at the repository root; `build_linux`
takes `$APPIMAGETOOL`, then `PATH`, then `~/bin`, then `packaging/tools/`.

**C++** — CMake, Qt 6 with WebEngineWidgets, md4c, and Python 3. Set
`CMAKE_PREFIX_PATH` to the Qt installation (semicolon-separated if md4c lives
somewhere of its own). No generator or compiler is forced: CMake picks the
platform default, which must match the ABI of the Qt build you point it at —
MSVC for Qt's official Windows binaries, Apple Clang for Homebrew Qt, system
GCC on Linux. Packaging builds into `cpp/build-packaging/` and stages into
`cpp/dist/`, leaving the `cpp/build/` tree from `cpp/README.md` alone. For the
deploy tools, see **Deploying the Qt runtime** below.

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

## Deploying the Qt runtime

Only the **C++** port needs this. A Qt program cannot just be copied: it needs
its libraries, its platform and image-format plugins, and — with QtWebEngine —
the `QtWebEngineProcess` helper plus Chromium's `.pak` and ICU data. The Python
port sidesteps the whole question because PyInstaller has already collected
PySide6 and everything under it; the Rust port has no Qt at all, using the
platform webview.

**Two of the three tools come with Qt; the Linux one does not.** That asymmetry
is the only thing that makes this confusing.

| Platform | Tool | Where it comes from | Checked by `setup`? |
|---|---|---|---|
| Windows | `windeployqt` | Comes with Qt, in `<kit>/bin` | No — nothing to install |
| macOS | `macdeployqt` | Comes with Qt, in `<kit>/bin` | No — nothing to install |
| Linux | `linuxdeploy` **+** `linuxdeploy-plugin-qt` | Two separate GitHub downloads | Yes |

`build_win` and `build_mac` look on `PATH`, then in the `bin/` of the Qt that
CMake recorded in `CMakeCache.txt`, so the deploy tool always matches the Qt
that built the executable rather than whatever the environment happens to point
at; `build_win` tries each `CMAKE_PREFIX_PATH` root after that. A Qt good enough
to build the port already carries the tool that deploys it.

Qt ships no `linuxdeployqt`. `linuxdeploy` is a third-party tool and Qt support
is a *plugin* for it, so Linux needs two files rather than one — and **`setup`
downloads both**, into `packaging/tools/` at the repository root. Nothing to do
by hand.

This is the same rule md4c follows: a dependency that installs without an
account, without root and without an interactive installer belongs inside the
checkout, gitignored, gone when the checkout goes. `setup` saves them under the
bare names — the release assets carry an architecture suffix, and the bare name
is what `build_linux` looks for and what `linuxdeploy` searches `PATH` for when
it loads its Qt plugin. Being inside the project also means it does not matter
whether `~/bin` is on your `PATH`, which by default it is not on many
distributions.

The lookup order in both `setup` and `build_linux` is `$LINUXDEPLOY` /
`$LINUXDEPLOY_PLUGIN_QT`, then `PATH`, then `~/bin`, then `packaging/tools/`, so
a copy you installed yourself wins and is never downloaded over. `--check`
reports without downloading. Running an AppImage needs FUSE — on Ubuntu 22.04
and later, `sudo apt install libfuse2`.

The versions are upstream's rolling `continuous` builds, as md4c is cloned from
its default branch: nothing here pins a release.

No separate `appimagetool` is needed for the C++ port — `linuxdeploy --output
appimage` produces the AppImage. The Python port does need it, because it has no
linuxdeploy step to hide it behind, and `python/packaging/setup` fetches it the
same way into the same place.

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
