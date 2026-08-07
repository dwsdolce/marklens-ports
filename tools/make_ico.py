#!/usr/bin/env python3
"""Build ``shared/icon.ico`` from ``shared/icon.png``.

Windows needs an .ico for the executable resource, the Inno Setup wizard and
the uninstall entry; the artwork only exists as a 512x512 PNG (and an .icns for
macOS). Rather than commit a third copy that can drift, this generates one.

Pure standard library on purpose. Pillow would be one import, but it would also
be a build dependency for a repository whose C++ and Rust ports otherwise need
no Python at all, and the PNG here is the easiest case there is: 8-bit RGBA,
non-interlaced. Decoding it is zlib plus the five PNG filters.

Sizes follow what Windows actually asks for: 16 and 32 for the title bar and
taskbar, 48 for the desktop, 64/128/256 for the large icon views. Everything at
or below 128 is written as an uncompressed BGRA DIB, which every Windows
version reads; 256 is written as PNG, which is the convention for that size and
keeps the file small.

Usage:

    python tools/make_ico.py                    # shared/icon.png -> shared/icon.ico
    python tools/make_ico.py in.png out.ico
"""

import pathlib
import struct
import sys
import zlib

ROOT = pathlib.Path(__file__).resolve().parent.parent

SIZES = (16, 32, 48, 64, 128, 256)

#: At or below this size an entry is stored as a raw DIB; above it, as PNG.
PNG_THRESHOLD = 128


# ------------------------------------------------------------------ decode ---

def read_png(data: bytes) -> tuple[int, int, bytearray]:
    """Decode an 8-bit RGBA non-interlaced PNG to (width, height, RGBA rows)."""
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise SystemExit("make_ico: not a PNG")

    width = height = 0
    idat = bytearray()
    offset = 8
    while offset < len(data):
        (length,) = struct.unpack(">I", data[offset:offset + 4])
        kind = data[offset + 4:offset + 8]
        body = data[offset + 8:offset + 8 + length]
        if kind == b"IHDR":
            width, height, depth, colour, _, _, interlace = struct.unpack(">IIBBBBB", body)
            if (depth, colour, interlace) != (8, 6, 0):
                raise SystemExit(
                    f"make_ico: need an 8-bit RGBA non-interlaced PNG, got "
                    f"depth={depth} colour type={colour} interlace={interlace}"
                )
        elif kind == b"IDAT":
            idat += body
        elif kind == b"IEND":
            break
        offset += 12 + length

    return width, height, unfilter(zlib.decompress(bytes(idat)), width, height)


def unfilter(raw: bytes, width: int, height: int) -> bytearray:
    """Undo the per-scanline PNG filters. 4 bytes per pixel throughout."""
    stride = width * 4
    out = bytearray(stride * height)
    pos = 0
    for y in range(height):
        method = raw[pos]
        pos += 1
        line = bytearray(raw[pos:pos + stride])
        pos += stride
        above = y * stride - stride
        for x in range(stride):
            left = line[x - 4] if x >= 4 else 0
            up = out[above + x] if y else 0
            upleft = out[above + x - 4] if y and x >= 4 else 0
            if method == 0:
                value = line[x]
            elif method == 1:
                value = line[x] + left
            elif method == 2:
                value = line[x] + up
            elif method == 3:
                value = line[x] + (left + up) // 2
            elif method == 4:
                # Paeth: pick whichever neighbour the gradient predicts best.
                estimate = left + up - upleft
                da, db, dc = (abs(estimate - left), abs(estimate - up),
                              abs(estimate - upleft))
                value = line[x] + (left if da <= db and da <= dc else
                                   up if db <= dc else upleft)
            else:
                raise SystemExit(f"make_ico: unknown PNG filter {method}")
            line[x] = value & 0xFF
        out[y * stride:y * stride + stride] = line
    return out


# ------------------------------------------------------------------ resize ---

def resize(pixels: bytearray, width: int, height: int, size: int) -> bytearray:
    """Box-filter downscale to size x size, averaging in premultiplied alpha.

    Averaging straight RGBA would let the colour of fully transparent pixels
    bleed into the edges, which shows up as a dark fringe at 16x16.
    """
    if width == size and height == size:
        return pixels

    out = bytearray(size * size * 4)
    for oy in range(size):
        y0 = oy * height // size
        y1 = max(y0 + 1, (oy + 1) * height // size)
        for ox in range(size):
            x0 = ox * width // size
            x1 = max(x0 + 1, (ox + 1) * width // size)
            r = g = b = a = count = 0
            for y in range(y0, y1):
                row = y * width * 4
                for x in range(x0, x1):
                    i = row + x * 4
                    alpha = pixels[i + 3]
                    r += pixels[i] * alpha
                    g += pixels[i + 1] * alpha
                    b += pixels[i + 2] * alpha
                    a += alpha
                    count += 1
            o = (oy * size + ox) * 4
            if a:
                out[o], out[o + 1], out[o + 2] = r // a, g // a, b // a
            out[o + 3] = a // count
    return out


# ------------------------------------------------------------------ encode ---

def encode_png(pixels: bytearray, size: int) -> bytes:
    """Minimal RGBA PNG: one IHDR, one IDAT of filter-0 scanlines, one IEND."""
    def chunk(kind: bytes, body: bytes) -> bytes:
        return (struct.pack(">I", len(body)) + kind + body
                + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF))

    stride = size * 4
    raw = bytearray()
    for y in range(size):
        raw += b"\x00" + pixels[y * stride:(y + 1) * stride]

    return (b"\x89PNG\r\n\x1a\n"
            + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
            + chunk(b"IEND", b""))


def encode_dib(pixels: bytearray, size: int) -> bytes:
    """32-bit BGRA DIB: BITMAPINFOHEADER, bottom-up rows, then the AND mask.

    The AND mask is obsolete for 32-bit icons, but the header still declares
    twice the image height and some Windows code paths still read it, so an
    all-zero (fully opaque) mask is written out.
    """
    header = struct.pack("<IiiHHIIiiII",
                         40,          # biSize
                         size,        # biWidth
                         size * 2,    # biHeight: colour rows plus mask rows
                         1,           # biPlanes
                         32,          # biBitCount
                         0,           # biCompression: BI_RGB
                         0, 0, 0, 0, 0)

    body = bytearray()
    for y in range(size - 1, -1, -1):          # DIB rows run bottom-up
        row = y * size * 4
        for x in range(size):
            i = row + x * 4
            body += bytes((pixels[i + 2], pixels[i + 1], pixels[i], pixels[i + 3]))

    # AND mask: 1 bit per pixel, each row padded to a 4-byte boundary.
    mask_stride = ((size + 31) // 32) * 4
    body += bytes(mask_stride * size)

    return header + bytes(body)


def build_ico(source: pathlib.Path, target: pathlib.Path) -> None:
    width, height, pixels = read_png(source.read_bytes())

    entries = []
    for size in SIZES:
        if size > max(width, height):
            continue            # never upscale: a blurry 512 helps no one
        scaled = resize(pixels, width, height, size)
        entries.append((size, encode_png(scaled, size) if size > PNG_THRESHOLD
                        else encode_dib(scaled, size)))

    # ICONDIR, then one ICONDIRENTRY per image, then the images themselves.
    out = bytearray(struct.pack("<HHH", 0, 1, len(entries)))
    offset = 6 + 16 * len(entries)
    for size, blob in entries:
        out += struct.pack("<BBBBHHII",
                           0 if size >= 256 else size,   # 0 means 256
                           0 if size >= 256 else size,
                           0,        # palette colours: none, it is true colour
                           0,        # reserved
                           1,        # colour planes
                           32,       # bits per pixel
                           len(blob), offset)
        offset += len(blob)
    for _, blob in entries:
        out += blob

    target.write_bytes(bytes(out))
    print(f"make_ico: wrote {target} ({', '.join(str(s) for s, _ in entries)})")


def main() -> None:
    args = sys.argv[1:]
    if len(args) not in (0, 2):
        raise SystemExit("usage: make_ico.py [source.png target.ico]")
    source = pathlib.Path(args[0]) if args else ROOT / "shared" / "icon.png"
    target = pathlib.Path(args[1]) if args else ROOT / "shared" / "icon.ico"
    if not source.is_file():
        raise SystemExit(f"make_ico: {source} is missing")
    build_ico(source, target)


if __name__ == "__main__":
    main()
