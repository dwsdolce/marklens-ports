# PyInstaller spec — builds Marklens.app (macOS) / a onedir bundle elsewhere.
# Build:  .venv/bin/pyinstaller marklens.spec
#
# Bundles the shared assets (web/, help, icon) under "shared" so the frozen
# app's assets._shared_dir() finds them via sys._MEIPASS. QtWebEngine is pulled
# in by PyInstaller's PySide6 hooks (collect_all below makes that explicit).

from PyInstaller.utils.hooks import collect_all

pyside_datas, pyside_binaries, pyside_hidden = collect_all("PySide6")

a = Analysis(
    ["run.py"],
    pathex=["src"],
    binaries=pyside_binaries,
    datas=[("../shared", "shared"), *pyside_datas],
    hiddenimports=[*pyside_hidden, "marklens.app", "marklens.renderer", "marklens.links"],
    excludes=["tkinter"],
    noarchive=False,
)
pyz = PYZ(a.pure)

exe = EXE(
    pyz,
    a.scripts,
    [],
    exclude_binaries=True,
    name="Marklens",
    console=False,
    icon="../shared/icon.icns",
)
coll = COLLECT(exe, a.binaries, a.datas, name="Marklens")

app = BUNDLE(
    coll,
    name="Marklens.app",
    icon="../shared/icon.icns",
    bundle_identifier="solutions.ddj.marklens.py",
    info_plist={
        "CFBundleName": "Marklens",
        "CFBundleDisplayName": "Marklens",
        "NSHighResolutionCapable": True,
    },
)
