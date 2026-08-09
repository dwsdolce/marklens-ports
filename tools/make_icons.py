#!/usr/bin/env python3
"""Generate the per-port application icons from ``shared/icon.svg``.

The three ports install side by side, so their icons need to be tellable apart
in a taskbar. Each gets the shared Marklens plate with a small badge in the
corner naming its language.

The badges are typographic rather than the official C++, Python and Rust logos.
Two reasons, and the second is the one that decides it:

* All three marks are trademarks whose policies permit saying "written in X"
  but specifically carve out putting the mark inside another mark, which is
  what a badged application icon is. C++'s terms ask for the logo "positioned
  beside other artwork without overlapping"; Python asks you to ask first about
  derived versions; Rust requires written permission and mentions a fee.
* An icon has to survive 16x16 in a title bar, where a detailed logo is mush.
  Two or three letterforms and a distinct badge colour still read.

Rendering is a two-step because neither tool alone does the job. Qt's SVG
support is Tiny 1.2 and ignores the ``filter`` element the artwork uses for the
glyph's drop shadow, so re-rendering the whole icon through Qt would silently
lose it. Inkscape renders the plate properly; QPainter then draws the badge on
top, which is also the only one of the two that can set type.

For each port it writes three files, because the three platforms want three
containers: a PNG (Linux, and the in-app window icon), an ICO (the Windows
executable resource and the installer) and an ICNS (the macOS bundle). All are
committed, so building a port needs neither Inkscape nor PySide6 - only editing
the artwork does.

Usage:

    python tools/make_icons.py            # write shared/icon-<port>.{png,ico,icns}
    python tools/make_icons.py --check    # report what would change

Needs Inkscape (for the plate) and PySide6 (for the badge). Both are already
required elsewhere: Inkscape produced the committed ``icon.png``, and PySide6
is the Python port's toolkit. The outputs are committed, so this only has to
run when the artwork or a badge changes.
"""

import argparse
import os
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SHARED = ROOT / "shared"
SOURCE = SHARED / "icon.svg"

#: Rendered at this size and downscaled from there; macOS wants 1024.
SIZE = 1024

#: label, badge fill, text colour. The fills are chosen to sit against the
#: plate's blue rather than to match any project's branding: a blue badge on a
#: blue plate would not read at all.
PORTS = {
    "cpp":  ("C++", "#2F3B45", "#FFFFFF"),
    "py":   ("Py",  "#FFD43B", "#20416B"),
    "rust": ("Rs",  "#CE422B", "#FFFFFF"),
}

#: Tried in order; the first one Qt actually has wins. Checked up front rather
#: than left to Qt's substitution, which silently yields boxes for every glyph.
FONT_STACK = ("Segoe UI", "Helvetica Neue", "Arial", "DejaVu Sans",
              "Liberation Sans")

#: Where Inkscape hides on Windows when it is not on PATH.
INKSCAPE_FALLBACKS = (
    r"C:\Program Files\Inkscape\bin\inkscape.exe",
    r"C:\Program Files (x86)\Inkscape\bin\inkscape.exe",
    "/Applications/Inkscape.app/Contents/MacOS/inkscape",
)


def find_inkscape() -> str:
    found = shutil.which("inkscape")
    if found:
        return found
    for candidate in INKSCAPE_FALLBACKS:
        if os.path.isfile(candidate):
            return candidate
    sys.exit("make_icons: inkscape not found. Install it, or put it on PATH.\n"
             "  https://inkscape.org/release/  (brew install --cask inkscape)")


def render_plate(target: pathlib.Path) -> None:
    """Rasterise the shared SVG at full size, drop shadow and all."""
    inkscape = find_inkscape()
    target.parent.mkdir(parents=True, exist_ok=True)
    result = subprocess.run(
        [inkscape, str(SOURCE), "--export-type=png", f"--export-filename={target}",
         f"--export-width={SIZE}", f"--export-height={SIZE}"],
        capture_output=True, text=True, check=False,
    )
    if result.returncode != 0 or not target.is_file():
        sys.exit(f"make_icons: inkscape failed\n{result.stdout}\n{result.stderr}")


def badge(plate: "QImage", label: str, fill: str, ink: str, family: str) -> "QImage":
    """Draw the language badge over a copy of the plate."""
    from PySide6.QtCore import QPointF, QRectF, Qt
    from PySide6.QtGui import QColor, QFont, QFontMetricsF, QImage, QPainter

    out = QImage(plate)
    n = out.width()

    # Bottom-right, overlapping the plate's corner. Sized so the disc is still
    # a recognisable blob of colour once the whole icon is 16 pixels across.
    diameter = n * 0.40
    centre = QPointF(n * 0.725, n * 0.725)
    circle = QRectF(centre.x() - diameter / 2, centre.y() - diameter / 2,
                    diameter, diameter)

    painter = QPainter(out)
    painter.setRenderHint(QPainter.RenderHint.Antialiasing)
    painter.setRenderHint(QPainter.RenderHint.TextAntialiasing)

    # A ring of the plate's own shadow tone, so the badge reads as sitting on
    # top rather than being a hole cut in the artwork.
    ring = QRectF(circle).adjusted(-n * 0.018, -n * 0.018, n * 0.018, n * 0.018)
    painter.setPen(Qt.PenStyle.NoPen)
    painter.setBrush(QColor(0, 0, 0, 46))
    painter.drawEllipse(ring)
    painter.setBrush(QColor("#FFFFFF"))
    painter.drawEllipse(QRectF(circle).adjusted(-n * 0.010, -n * 0.010,
                                                n * 0.010, n * 0.010))
    painter.setBrush(QColor(fill))
    painter.drawEllipse(circle)

    # Type: the largest point size that fits inside the disc with a margin.
    font = QFont(family)
    font.setWeight(QFont.Weight.Black)
    usable = diameter * 0.74
    size = int(diameter * 0.5)
    font.setPixelSize(size)
    while size > 8:
        font.setPixelSize(size)
        fm = QFontMetricsF(font)
        if fm.horizontalAdvance(label) <= usable and fm.height() <= usable:
            break
        size -= 2

    painter.setFont(font)
    painter.setPen(QColor(ink))
    painter.drawText(circle, Qt.AlignmentFlag.AlignCenter, label)
    painter.end()
    return out


#: ICNS entry types and the pixel size each one holds. The @2x names are what
#: macOS calls them; what goes in the file is just a PNG of that many pixels.
#: This is the same set the hand-made shared/icon.icns carries, minus ic04 and
#: ic05, which are raw ARGB rather than PNG and which macOS has not needed
#: since 10.7.
ICNS_ENTRIES = (
    (b"ic11", 32),    # 16x16@2x
    (b"ic12", 64),    # 32x32@2x
    (b"ic07", 128),   # 128x128
    (b"ic13", 256),   # 128x128@2x
    (b"ic08", 256),   # 256x256
    (b"ic14", 512),   # 256x256@2x
    (b"ic09", 512),   # 512x512
    (b"ic10", 1024),  # 512x512@2x
)


def png_bytes(image: "QImage", size: int) -> bytes:
    """The image scaled to size, encoded as PNG, without touching the disk."""
    from PySide6.QtCore import QBuffer, QIODevice, Qt

    scaled = image.scaled(size, size, Qt.AspectRatioMode.IgnoreAspectRatio,
                          Qt.TransformationMode.SmoothTransformation)
    buffer = QBuffer()
    buffer.open(QIODevice.OpenModeFlag.WriteOnly)
    if not scaled.save(buffer, "PNG"):
        sys.exit(f"make_icons: could not encode a {size}px PNG")
    return bytes(buffer.data())


def write_icns(image: "QImage", target: pathlib.Path) -> None:
    """Write a PNG-based ICNS.

    The format is a header and a flat list of typed entries, each of which is
    its own 4-byte type, a big-endian length *including* those 8 header bytes,
    and the payload. macOS has read PNG payloads since 10.7, so no ARGB or
    RLE encoders are needed here.
    """
    import struct

    entries = b"".join(
        kind + struct.pack(">I", len(data) + 8) + data
        for kind, data in ((k, png_bytes(image, size)) for k, size in ICNS_ENTRIES)
    )
    target.write_bytes(b"icns" + struct.pack(">I", len(entries) + 8) + entries)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true",
                        help="report what would change, write nothing")
    args = parser.parse_args()

    if not SOURCE.is_file():
        sys.exit(f"make_icons: {SOURCE} is missing")

    # QPainter needs a QGuiApplication for font handling. Deliberately NOT the
    # offscreen platform, which loads no font database at all: every glyph comes
    # out as a tofu box, and a badge full of empty rectangles looks deliberate
    # enough to ship unnoticed. Drawing onto a QImage needs no window anyway.
    try:
        from PySide6.QtGui import QFontDatabase, QGuiApplication, QImage
    except ImportError:
        sys.exit("make_icons: PySide6 is needed for the badge.\n"
                 "  pip install -e 'python[dev]'   (or just: pip install PySide6)")
    app = QGuiApplication.instance() or QGuiApplication([])

    # Checked up front rather than left to Qt's font substitution, which fails
    # by drawing boxes rather than by complaining.
    available = set(QFontDatabase.families())
    family = next((f for f in FONT_STACK if f in available), None)
    if family is None:
        sys.exit(
            "make_icons: none of the expected fonts are available "
            f"({', '.join(FONT_STACK)}).\n"
            f"  Qt reports {len(available)} families on the "
            f"'{app.platformName()}' platform.\n"
            "  If that is 0, QT_QPA_PLATFORM is forcing offscreen, which has "
            "no fonts - unset it and try again."
        )
    print(f"make_icons: badge type in {family}")

    plate_png = ROOT / "build" / f"icon-plate-{SIZE}.png"
    render_plate(plate_png)
    plate = QImage(str(plate_png))
    if plate.isNull():
        sys.exit(f"make_icons: could not read {plate_png}")
    print(f"make_icons: plate {plate.width()}x{plate.height()} from {SOURCE.name}")

    # make_ico.py owns the ICO encoder; it is pure standard library, so the
    # Windows resource can also be rebuilt without Qt if it ever needs to be.
    sys.path.insert(0, str(ROOT / "tools"))
    from make_ico import build_ico

    for port, (label, fill, ink) in PORTS.items():
        png = SHARED / f"icon-{port}.png"
        ico = SHARED / f"icon-{port}.ico"
        icns = SHARED / f"icon-{port}.icns"

        if args.check:
            state = "would update" if png.is_file() else "would create"
            print(f"  {state} icon-{port}.{{png,ico,icns}}  [{label}]")
            continue

        image = badge(plate, label, fill, ink, family)
        if not image.save(str(png), "PNG"):
            sys.exit(f"make_icons: could not write {png}")
        build_ico(png, ico)
        write_icns(image, icns)
        print(f"  wrote icon-{port}.{{png,ico,icns}}  [{label}] {fill}")

    del app


if __name__ == "__main__":
    main()
