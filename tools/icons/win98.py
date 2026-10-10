"""Pixel-art Win98 icons for Minuta, drawn on each native grid (16/32/48).

Writes res/MINUTA.ICO (app), res/MINUTADOC.ICO (.md documents) and
res/MINUTAUN.ICO (uninstaller), plus PNG copies in build/icons/win98.
Run from the repository root: uv run --with pillow python tools/icons/win98.py
"""
import struct
from pathlib import Path
from PIL import Image

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

# Solid letter masks (serifs symmetric on both sides, or none) ('#'), one per size, drawn by hand.
M_MASKS = {
    "xs": [
        "##...##",
        "###.###",
        "##.#.##",
        "##...##",
        "##...##",
    ],
    # The app icon at 16x16 is the letter alone, so it reads apart from the
    # document icon (a sheet with a small M) in Explorer lists and menus.
    "app16": [
        "###......###",
        "####....####",
        "#####..#####",
        "###.####.###",
        "###..##..###",
        "###......###",
        "###......###",
        "###......###",
        "###......###",
        "###......###",
        "###......###",
    ],
    "s": [
        "###...###",
        "####.####",
        "##.###.##",
        "##..#..##",
        "##.....##",
        "##.....##",
        "##.....##",
    ],
    "m": [
        ".#####......#####.",
        ".#####......#####.",
        ".######....######.",
        ".######....######.",
        ".###.###..###.###.",
        ".###.###..###.###.",
        ".###..######..###.",
        ".###...####...###.",
        ".###....##....###.",
        ".###..........###.",
        ".###..........###.",
        ".###..........###.",
        ".###..........###.",
        "#####........#####",
        "#####........#####",
    ],
    "l": [
        ".########........########.",
        ".########........########.",
        ".########........########.",
        ".#########......#########.",
        ".#########......#########.",
        ".##########....##########.",
        ".####.#####....#####.####.",
        ".####..#####..#####..####.",
        ".####..#####..#####..####.",
        ".####...##########...####.",
        ".####...##########...####.",
        ".####....########....####.",
        ".####.....######.....####.",
        ".####......####......####.",
        ".####.......##.......####.",
        ".####................####.",
        ".####................####.",
        ".####................####.",
        ".####................####.",
        ".####................####.",
        "######..............######",
        "######..............######",
    ],
}


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


def app_icon(size):
    g = blank(size)
    if size == 48:
        lines = [(7 + 4 * i, 9, e, 2) for i, e in enumerate([29, 26, 33, 30, 17, 17, 17, 14])]
        draw_sheet(g, 4, 0, 41, 43, 10, lines)
        paste(g, shade_letter(M_MASKS["l"]), 20, 22)
    elif size == 32:
        lines = [(5 + 3 * i, 6, e, 1) for i, e in enumerate([20, 16, 21, 12, 11, 11, 11])]
        draw_sheet(g, 3, 0, 26, 27, 7, lines)
        paste(g, shade_letter(M_MASKS["m"]), 12, 14)
    else:
        paste(g, shade_letter(M_MASKS["app16"]), 1, 1)
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


def doc_icon(size):
    g = blank(size)
    if size == 48:
        lines = [(9 + 4 * i, 12, e, 2) for i, e in enumerate([30, 25, 32, 28, 22, 26, 18])]
        draw_sheet(g, 7, 1, 39, 46, 10, lines)
        paste(g, shade_letter(M_MASKS["s"], left_light=False), 27, 35)
    elif size == 32:
        lines = [(6 + 3 * i, 8, e, 1) for i, e in enumerate([20, 16, 21, 18, 14, 17])]
        draw_sheet(g, 5, 1, 26, 30, 7, lines)
        paste(g, shade_letter(M_MASKS["xs"], left_light=False), 17, 22)
    else:
        lines = [(4 + 2 * i, 5, e, 1) for i, e in enumerate([10, 8, 11])]
        draw_sheet(g, 2, 0, 13, 15, 4, lines)
        paste(g, shade_letter(M_MASKS["xs"], left_light=False), 6, 8)
    return g


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
