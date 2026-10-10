"""Pixel-art Win98 icons for Minuta, drawn on each native grid (16/32/48).

Writes res/MINUTA.ICO (app), res/MINUTADOC.ICO (.md documents) and
res/MINUTAUN.ICO (uninstaller), plus PNG copies in build/icons/win98.
Run from the repository root: uv run --with pillow python tools/icons/win98.py
"""
import os
import struct
import sys
from pathlib import Path
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
from glyph import m_mask  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "res"
PNG = ROOT / "build" / "icons" / "win98"

# Pixel keys -> (8-bit colour, 4-bit VGA colour). None = transparent.
COLORS = {
    "K": ((0x00, 0x00, 0x00), (0x00, 0x00, 0x00)),  # outline
    "W": ((0xFF, 0xFF, 0xFF), (0xFF, 0xFF, 0xFF)),  # paper
    "S": ((0xC0, 0xC0, 0xC0), (0xC0, 0xC0, 0xC0)),  # fold face / paper shade
    "G": ((0x80, 0x80, 0x80), (0x80, 0x80, 0x80)),  # fold crease
    "T": ((0x4D, 0x4D, 0x5A), (0x80, 0x80, 0x80)),  # text lines
    "H": ((0x66, 0xCC, 0xFF), (0x00, 0xFF, 0xFF)),  # M highlight
    "B": ((0x00, 0x66, 0xFF), (0x00, 0x00, 0xFF)),  # M face
    "D": ((0x00, 0x33, 0xCC), (0x00, 0x00, 0x80)),  # M shade
    "N": ((0x00, 0x00, 0x80), (0x00, 0x00, 0x80)),  # M deep shade
    "R": ((0xE0, 0x10, 0x10), (0xFF, 0x00, 0x00)),  # uninstall badge
    "r": ((0x90, 0x00, 0x00), (0x80, 0x00, 0x00)),  # uninstall badge shade
}

VGA16 = [
    (0, 0, 0), (128, 0, 0), (0, 128, 0), (128, 128, 0),
    (0, 0, 128), (128, 0, 128), (0, 128, 128), (192, 192, 192),
    (128, 128, 128), (255, 0, 0), (0, 255, 0), (255, 255, 0),
    (0, 0, 255), (255, 0, 255), (0, 255, 255), (255, 255, 255),
]

# The same M (glyph.py, also used by the Haiku icons) at every size: alone
# for the app and the uninstaller, smaller on documents.
APP_M = {16: (12, 11), 32: (26, 22), 48: (38, 34)}
CENTER_M = {16: (8, 7), 32: (14, 12), 48: (20, 18)}
CORNER_M = {16: (6, 5), 32: (10, 9), 48: (14, 12)}

# ".md" in a small pixel font, as modern systems label document icons.
LABEL = [
    "...........#",
    "...........#",
    "..####...###",
    "..#.#.#.#..#",
    "#.#.#.#..###",
]


def blank(size):
    return [[None] * size for _ in range(size)]


def shade_letter(mask, left_light=True):
    """Outline + bevel a solid mask; returns a grid with 1px border (None = empty)."""
    h, w = len(mask), len(mask[0])
    inside = lambda x, y: 0 <= y < h and 0 <= x < w and mask[y][x] == "#"
    grid = [[None] * (w + 2) for _ in range(h + 2)]
    for y in range(-1, h + 1):
        for x in range(-1, w + 1):
            if inside(x, y):
                top = not inside(x, y - 1)
                left = not inside(x - 1, y)
                bottom = not inside(x, y + 1)
                right = not inside(x + 1, y)
                if bottom:
                    key = "N"
                elif right:
                    key = "D"
                elif top or (left and left_light):
                    key = "H"
                else:
                    key = "B"
            elif any(inside(x + dx, y + dy) for dx in (-1, 0, 1) for dy in (-1, 0, 1)):
                key = "K"
            else:
                key = None
            grid[y + 1][x + 1] = key
    return grid


def draw_sheet(grid, x0, y0, x1, y1, fold, lines):
    """Paper sheet with folded top-right corner; `lines` = [(y, x_start, x_end, thickness)]."""
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            corner_cut = x - (x1 - fold) > y - y0  # above the fold diagonal
            if corner_cut:
                continue
            edge = x in (x0, x1) or y in (y0, y1) or x - (x1 - fold) == y - y0
            grid[y][x] = "K" if edge else "W"
    # inner shade along right and bottom edges
    for y in range(y0 + fold + 1, y1):
        grid[y][x1 - 1] = "S"
    for x in range(x0 + 1, x1):
        grid[y1 - 1][x] = "S"
    # fold flap: triangle under the diagonal, crease at its left/bottom
    fx = x1 - fold
    for y in range(y0 + 1, y0 + fold):
        for x in range(fx, fx + (y - y0)):
            grid[y][x] = "G" if x == fx else "S"
    for x in range(fx, x1):
        grid[y0 + fold][x] = "G" if x < x1 else grid[y0 + fold][x]
    grid[y0 + fold][fx] = "G"
    for y, xs, xe, t in lines:
        for dy in range(t):
            for x in range(xs, xe + 1):
                grid[y + dy][x] = "T"


def paste(grid, layer, ox, oy):
    for y, row in enumerate(layer):
        for x, key in enumerate(row):
            if key is not None:
                grid[oy + y][ox + x] = key


def letter(size_wh):
    return shade_letter(m_mask(*size_wh))


def app_icon(size):
    g = blank(size)
    m = letter(APP_M[size])
    paste(g, m, (size - len(m[0])) // 2, (size - len(m)) // 2)
    return g


# Red "remove" badges (a white bar on a red disc), one per size, drawn by
# hand: R = red, r = shade, W = the bar, K = outline.
BADGES = {
    16: [
        "..KKK..",
        ".KRRRK.",
        "KRRRRrK",
        "KWWWWWK",
        "KRRRRrK",
        ".KrrrK.",
        "..KKK..",
    ],
    32: [
        "...KKKKK...",
        "..KRRRRRK..",
        ".KRRRRRRrK.",
        "KRRRRRRRRrK",
        "KRRRRRRRRrK",
        "KRWWWWWWWrK",
        "KRWWWWWWWrK",
        "KRRRRRRRRrK",
        ".KRRRRRRrK.",
        "..KrrrrrK..",
        "...KKKKK...",
    ],
    48: [
        "....KKKKKKK....",
        "...KRRRRRRRK...",
        "..KRRRRRRRRRK..",
        ".KRRRRRRRRRRrK.",
        "KRRRRRRRRRRRRrK",
        "KRRRRRRRRRRRRrK",
        "KRRWWWWWWWWWRrK",
        "KRRWWWWWWWWWRrK",
        "KRRWWWWWWWWWRrK",
        "KRRRRRRRRRRRRrK",
        "KRRRRRRRRRRRRrK",
        ".KRRRRRRRRRRrK.",
        "..KrrrrrrrrrK..",
        "...KrrrrrrrK...",
        "....KKKKKKK....",
    ],
}


def uninstall_icon(size):
    """The app icon with a red remove badge in the bottom-right corner."""
    g = app_icon(size)
    badge = [[None if c == "." else c for c in row] for row in BADGES[size]]
    paste(g, badge, size - len(badge), size - len(badge))
    return g


def label(grid, x, y, scale):
    for dy, row in enumerate(LABEL):
        for dx, c in enumerate(row):
            if c == "#":
                for sy in range(scale):
                    for sx in range(scale):
                        grid[y + dy * scale + sy][x + dx * scale + sx] = "K"


def doc_icon_center(size):
    """A sheet with the M in the middle."""
    g = blank(size)
    if size == 48:
        draw_sheet(g, 6, 1, 41, 46, 10, [(9 + 4 * i, 11, e, 2) for i, e in enumerate([26, 22])])
        x0, x1, y0 = 6, 41, 19
    elif size == 32:
        draw_sheet(g, 4, 1, 27, 30, 7, [(6 + 3 * i, 7, e, 1) for i, e in enumerate([18, 15])])
        x0, x1, y0 = 4, 27, 13
    else:
        draw_sheet(g, 2, 0, 13, 15, 4, [])
        x0, x1, y0 = 2, 13, 4
    m = letter(CENTER_M[size])
    paste(g, m, x0 + (x1 - x0 + 1 - len(m[0])) // 2, y0)
    return g


def doc_icon_label(size):
    """A sheet with ".md" written on it and the M in the corner."""
    g = blank(size)
    if size == 48:
        draw_sheet(g, 6, 1, 39, 46, 10, [(9 + 4 * i, 11, e, 2) for i, e in enumerate([26, 22, 30])])
        label(g, 10, 24, 2)
        m = letter(CORNER_M[48])
        paste(g, m, 48 - len(m[0]), 48 - len(m))
    elif size == 32:
        draw_sheet(g, 4, 1, 26, 30, 7, [(6 + 3 * i, 7, e, 1) for i, e in enumerate([18, 15, 21])])
        label(g, 7, 15, 1)
        m = letter(CORNER_M[32])
        paste(g, m, 32 - len(m[0]), 32 - len(m))
    else:
        draw_sheet(g, 1, 0, 14, 15, 3, [])
        label(g, 2, 4, 1)
        m = letter(CORNER_M[16])
        paste(g, m, 16 - len(m[0]), 16 - len(m))
    return g


# MINUTA_DOC_ICON=center picks the other document design; "label" is the default.
doc_icon = doc_icon_center if os.environ.get("MINUTA_DOC_ICON") == "center" else doc_icon_label


def to_image(grid, depth):
    size = len(grid)
    im = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    for y, row in enumerate(grid):
        for x, key in enumerate(row):
            if key:
                rgb = COLORS[key][0 if depth == 8 else 1]
                im.putpixel((x, y), rgb + (255,))
    return im


def bmp_entry(grid, depth):
    size = len(grid)
    if depth == 4:
        palette = VGA16
    elif depth == 8:
        palette = []
        for key in COLORS:
            if COLORS[key][0] not in palette:
                palette.append(COLORS[key][0])
        palette = [(0, 0, 0)] + [c for c in palette if c != (0, 0, 0)]
        palette += [(0, 0, 0)] * (256 - len(palette))
    rows_xor, rows_and = [], []
    for row in reversed(grid):
        if depth == 32:
            data = b"".join(
                bytes((*COLORS[k][0][::-1], 255)) if k else b"\0\0\0\0" for k in row)
        else:
            idx = [palette.index(COLORS[k][0 if depth == 8 else 1]) if k else 0 for k in row]
            if depth == 8:
                data = bytes(idx)
            else:
                data = bytes((idx[i] << 4) | idx[i + 1] for i in range(0, size, 2))
        data += b"\0" * (-len(data) % 4)
        rows_xor.append(data)
        bits = 0
        mask = bytearray()
        for i, k in enumerate(row):
            bits = (bits << 1) | (0 if k else 1)
            if i % 8 == 7:
                mask.append(bits)
                bits = 0
        mask += b"\0" * (-len(mask) % 4)
        rows_and.append(bytes(mask))
    header = struct.pack("<IiiHHIIiiII", 40, size, size * 2, 1, depth, 0,
                         0, 0, 0, 0 if depth != 4 else 16, 0)
    pal = b""
    if depth in (4, 8):
        pal = b"".join(bytes((b, g, r, 0)) for r, g, b in palette)
    body = header + pal + b"".join(rows_xor) + b"".join(rows_and)
    return size, depth, body


def write_ico(path, entries):
    out = struct.pack("<HHH", 0, 1, len(entries))
    offset = 6 + 16 * len(entries)
    blobs = []
    for size, depth, body in entries:
        colors = 16 if depth == 4 else 0
        out += struct.pack("<BBBBHHII", size % 256, size % 256, colors, 0, 1, depth,
                           len(body), offset)
        offset += len(body)
        blobs.append(body)
    path.write_bytes(out + b"".join(blobs))


def build(name, maker):
    grids = {s: maker(s) for s in (16, 32, 48)}
    entries = []
    for depth in (4, 8):
        for s in (48, 32, 16):
            entries.append(bmp_entry(grids[s], depth))
    entries.append(bmp_entry(grids[48], 32))
    write_ico(OUT / f"{name}.ICO", entries)
    for s, grid in grids.items():
        for depth in (4, 8):
            to_image(grid, depth).save(PNG / f"{name.lower()}-{s}-{depth}bit.png")
    return grids


if __name__ == "__main__":
    PNG.mkdir(parents=True, exist_ok=True)
    build("MINUTA", app_icon)
    build("MINUTADOC", doc_icon)
    build("MINUTAUN", uninstall_icon)
    # Both document designs, for comparison only (not written to res/).
    for name, maker in (("doc-label", doc_icon_label), ("doc-center", doc_icon_center)):
        for s in (16, 32, 48):
            for depth in (4, 8):
                to_image(maker(s), depth).save(PNG / f"{name}-{s}-{depth}bit.png")
