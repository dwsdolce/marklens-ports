# -*- mode: python ; coding: utf-8 -*-
"""PyInstaller spec for the Python/PySide6 port.

Build from the python/ directory:

    .venv/bin/pip install -e ".[packaging]"
    python ../tools/gen_version_build.py python     # stamps the build number
    .venv/bin/pyinstaller -y packaging/marklens.spec

Or just run packaging/build_win, build_mac or build_linux, which do all of that
and then produce the installer for the platform.

Produces dist/marklens-py/ everywhere, plus "dist/Marklens Python.app" on
macOS. The shared assets are bundled under "shared" so the frozen app's
assets._shared_dir() finds them via sys._MEIPASS.
"""

import os
import sys

spec_dir = os.path.abspath(SPECPATH)
project_root = os.path.dirname(spec_dir)                 # python/
repo_root = os.path.dirname(project_root)                # marklens-ports/
shared = os.path.join(repo_root, "shared")

from PyInstaller.utils.hooks import (
    collect_data_files, collect_dynamic_libs, collect_submodules,
)

# ---------------------------------------------------------------- version ----
# The base version lives in pyproject.toml; the build number is the git commit
# count, written to build/installer_version by tools/gen_version_build.py and
# not committed.
version_file = os.path.join(project_root, "build", "installer_version")
if os.path.isfile(version_file):
    with open(version_file, encoding="utf-8") as handle:
        package_version = handle.read().strip()
else:
    # A bare `pyinstaller` run without the build script. Not fatal, but the
    # Windows Properties tab would claim 0.0.0.0, so say so.
    print("WARNING: python/build/installer_version is missing - run "
          "tools/gen_version_build.py python; using 0.0.0.0")
    package_version = "0.0.0.0"
print(f"Creating build for version {package_version}")

# ------------------------------------------------------------------ datas ----
# Only what the app reads at runtime. shared/spec (fixtures and the sample
# document) is for the test suites, and has no business in a shipped bundle.
datas = [
    (os.path.join(shared, "web"), os.path.join("shared", "web")),
    (os.path.join(shared, "icons"), os.path.join("shared", "icons")),
    (os.path.join(shared, "icon-py.png"), "shared"),
]
for name in ("help.html", "help_default_macos.html", "help_default_windows.html",
             "help_default_linux.html"):
    datas.append((os.path.join(shared, name), "shared"))

# The build number, read back by marklens.__build__ for the About box. A frozen
# app has no git to ask, which is the whole reason this file exists.
version_stamp = os.path.join(project_root, "src", "marklens", "version_build")
if os.path.isfile(version_stamp):
    datas.append((version_stamp, "marklens"))
else:
    print("WARNING: src/marklens/version_build is missing - run "
          "tools/gen_version_build.py python; About will show no build number")

# PySide6 is collected wholesale rather than left to the automatic hooks:
# QtWebEngine drags in a Chromium process, its .pak resources and its ICU data,
# and a missed one fails at runtime rather than at build time. It costs bundle
# size, which is the right trade for a viewer that is useless without a working
# web engine.
#
# This is collect_all() split into its three parts so that PySide6.scripts can
# be skipped. That package is the pyside6-deploy / pyside6-project CLI, which
# nothing here imports, and its deploy_lib submodule imports its sibling
# project_lib as though it were top-level - which only resolves when the console
# script puts that directory on sys.path. Collecting it therefore fails every
# build with a ModuleNotFoundError warning and contributes nothing.
# PySide6.scripts is the pyside6-deploy / pyside6-project CLI, which nothing
# here imports. Its deploy_lib submodule imports its sibling project_lib as
# though it were top-level, which only resolves when the console script puts
# that directory on sys.path, so collecting it fails every build with a
# ModuleNotFoundError warning and contributes nothing.
def _wanted_module(name):
    return not name.startswith("PySide6.scripts")


pyside_hidden = collect_submodules("PySide6", filter=_wanted_module)
pyside_binaries = collect_dynamic_libs("PySide6")
pyside_datas = collect_data_files("PySide6")

datas += pyside_datas

# Two warnings survive every build and are not worth chasing:
#
#   "Library not found: could not resolve 'LIBPQ.dll' ..." (and OCI, fbclient,
#   MIMAPI64) - Qt's PostgreSQL, Oracle, Firebird and Mimer drivers, whose
#   vendor client libraries are not installed. PyInstaller's PySide6 hook
#   collects every plugin type the shipped Qt libraries reference, and the
#   wheel ships all of Qt, so the drivers arrive no matter what the spec
#   excludes - filtering them out of `binaries` here does not stop the hook
#   putting them back. They are inert: nothing imports QtSql.
#
#   "QML plugin binary ...assetdownloader...dll does not exist!" - the wheel
#   ships that plugin's .lib and .prl but not its .dll. An upstream packaging
#   gap in a QML module nothing here uses.
#
icon_ico = os.path.join(shared, "icon-py.ico")    # tools/make_icons.py
icon_icns = os.path.join(shared, "icon-py.icns")

# ------------------------------------------------------------ version info ---
# Windows only: puts the version on the exe's Properties tab, and gives Inno
# Setup something to compare when replacing an older install.
version_resource = None
if os.name == "nt":
    from PyInstaller.utils.win32.versioninfo import (
        FixedFileInfo, StringFileInfo, StringStruct, StringTable, VarFileInfo,
        VarStruct, VSVersionInfo,
    )

    # The Windows resource wants exactly four integers.
    numbers = tuple(int(part) for part in package_version.split("."))
    numbers += (0,) * (4 - len(numbers))

    version_resource = VSVersionInfo(
        ffi=FixedFileInfo(filevers=numbers, prodvers=numbers,
                          mask=0x3F, flags=0x0, OS=0x40004, fileType=0x1,
                          subtype=0x0, date=(0, 0)),
        kids=[
            StringFileInfo([StringTable("040904B0", [
                StringStruct("CompanyName", "Dolce Sfogato"),
                StringStruct("FileDescription", "Marklens - a fast Markdown viewer (Python/PySide6 port)"),
                StringStruct("FileVersion", package_version),
                StringStruct("InternalName", "marklens-py"),
                StringStruct("LegalCopyright", "Copyright (C) 2026 Marklens contributors."),
                StringStruct("OriginalFilename", "marklens-py.exe"),
                StringStruct("ProductName", "Marklens Python"),
                StringStruct("ProductVersion", package_version),
            ])]),
            # 0x0409 = US English, 1200 = Unicode.
            VarFileInfo([VarStruct("Translation", [0x0409, 1200])]),
        ],
    )

# ------------------------------------------------------------------ build ----
a = Analysis(
    [os.path.join(spec_dir, "pyinstaller_entry.py")],
    pathex=[os.path.join(project_root, "src")],
    binaries=pyside_binaries,
    datas=datas,
    hiddenimports=[*pyside_hidden, "marklens.app", "marklens.renderer",
                   "marklens.links", "marklens.assets"],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=["tkinter"],
    noarchive=False,
)

pyz = PYZ(a.pure)

if sys.platform == "darwin":
    icon = icon_icns if os.path.isfile(icon_icns) else None
else:
    icon = icon_ico if os.path.isfile(icon_ico) else None
    if icon is None:
        print("WARNING: shared/icon-py.ico is missing - run tools/make_icons.py")

exe = EXE(
    pyz,
    a.scripts,
    [],
    exclude_binaries=True,
    name="marklens-py",
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=False,
    console=False,  # GUI application: no console window
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    # Opt-in, and read from the environment rather than hardcoded: this is a
    # public repository and a Developer ID does not belong in it. See
    # packaging/build_mac.
    codesign_identity=os.environ.get("CODESIGN_IDENTITY") or None,
    entitlements_file=None,
    icon=icon,
    version=version_resource,
)

coll = COLLECT(
    exe,
    a.binaries,
    a.datas,
    strip=False,
    upx=False,
    upx_exclude=[],
    name="marklens-py",
)

if sys.platform == "darwin":
    app = BUNDLE(
        coll,
        # Named for the port, not just "Marklens": the whole point of this
        # repository is having all three installed side by side.
        name="Marklens Python.app",
        icon=icon_icns if os.path.isfile(icon_icns) else None,
        bundle_identifier="solutions.ddj.marklens.py",
        version=package_version,
        info_plist={
            "CFBundleName": "Marklens Python",
            "CFBundleDisplayName": "Marklens Python",
            "CFBundleVersion": package_version,
            "CFBundleShortVersionString": package_version,
            "NSPrincipalClass": "NSApplication",
            "NSAppleScriptEnabled": False,
            "NSRequiresAquaSystemAppearance": "No",
            "NSHighResolutionCapable": "True",
            # Lets the Finder offer this app under "Open With" for Markdown,
            # and makes drops onto the Dock icon arrive as arguments.
            "CFBundleDocumentTypes": [{
                "CFBundleTypeName": "Markdown Document",
                "CFBundleTypeRole": "Viewer",
                "LSHandlerRank": "Alternate",
                "LSItemContentTypes": ["net.daringfireball.markdown",
                                       "public.plain-text"],
            }],
        },
    )
