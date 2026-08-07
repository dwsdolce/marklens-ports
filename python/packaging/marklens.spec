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

from PyInstaller.utils.hooks import collect_all

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
    (os.path.join(shared, "icon.png"), "shared"),
]
for name in ("help.html", "help_default_macos.html", "help_default_windows.html",
             "help_default_linux.html"):
    datas.append((os.path.join(shared, name), "shared"))

# PySide6 is collected wholesale rather than left to the automatic hooks:
# QtWebEngine drags in a Chromium process, its .pak resources and its ICU data,
# and a missed one fails at runtime rather than at build time. It costs bundle
# size, which is the right trade for a viewer that is useless without a working
# web engine.
pyside_datas, pyside_binaries, pyside_hidden = collect_all("PySide6")
datas += pyside_datas

icon_ico = os.path.join(shared, "icon.ico")     # tools/make_ico.py
icon_icns = os.path.join(shared, "icon.icns")

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
    [os.path.join(project_root, "run.py")],
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
        print("WARNING: shared/icon.ico is missing - run tools/make_ico.py")

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
