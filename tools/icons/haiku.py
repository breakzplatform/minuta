"""Haiku vector icon for Minuta: one geometry, emitted as SVG and HVIF.

Icon space is 64x64 units, y down (same as Icon-O-Matic). Perspective follows
the Haiku icon guidelines: a sheet lying on the floor (left-front edge 2:1 down,
right-front edge 1:1 up), an upright extruded "M", light from the top left and
a shadow cast straight to the right.

Writes src/haiku/icons/*.hvif and *.svg and src/haiku/Icons.rdef, plus PNG
renders in build/icons/haiku. Run from the repository root:
  DYLD_FALLBACK_LIBRARY_PATH=/opt/homebrew/lib uv run --with shapely \
    --with cairosvg --with pillow python tools/icons/haiku.py
"""
import re
import struct
from pathlib import Path

from shapely import affinity
from shapely.geometry import MultiPolygon, Polygon, box
from shapely.geometry.polygon import orient
from shapely.ops import unary_union

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "src" / "haiku" / "icons"
RDEF = ROOT / "src" / "haiku" / "Icons.rdef"
PNG = ROOT / "build" / "icons" / "haiku"


def rgba(hex_color, alpha=255):
    h = hex_color.lstrip("#")
    return (int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16), alpha)


def solid(hex_color, alpha=255):
    return ("solid", rgba(hex_color, alpha))


def linear(p0, p1, *stops):
    return ("linear", p0, p1, [(o, rgba(*c) if isinstance(c, tuple) else rgba(c)) for o, c in stops])


def translate(geom, dx, dy):
    return affinity.translate(geom, dx, dy)


def sweep(geom, dx, dy, steps=12):
    """Minkowski sum of geom with the segment (0,0)-(dx,dy)."""
    parts = [translate(geom, dx * i / steps, dy * i / steps) for i in range(steps + 1)]
    return unary_union(parts).buffer(0.05, join_style=2).buffer(-0.05, join_style=2)


# ---------------------------------------------------------------- sheet

LEFT = (2.0, 30.0)   # page bottom-left corner (leftmost point)
UP = (26.0, -26.0)   # bottom -> top of the page (right-front direction, 1:1 up)
ACROSS = (30.0, 15.0)  # left -> right, the text direction (left-front direction, 2:1 down)


def page_point(x, y):
    """x in [0, 1] across the page (text direction), y in [0, 1] from the top edge down."""
    return (LEFT[0] + (1 - y) * UP[0] + x * ACROSS[0], LEFT[1] + (1 - y) * UP[1] + x * ACROSS[1])


def page_quad(x0, y0, x1, y1):
    return Polygon([page_point(x0, y0), page_point(x1, y0), page_point(x1, y1), page_point(x0, y1)])


FOLD_X, FOLD_Y = 0.24, 0.2

# Alternate lines only appear from 64px up (HVIF level-of-detail); none at 16px.
LOD_SOME, LOD_ALL = (0.45, 4.0), (0.9, 4.0)


def sheet_shapes(text_lines):
    corner = page_point(1, 0)
    a = page_point(1 - FOLD_X, 0)
    b = page_point(1, FOLD_Y)
    page = Polygon([page_point(0, 0), a, b, page_point(1, 1), page_point(0, 1)])
    flap = Polygon([a, b, (a[0] + b[0] - corner[0], a[1] + b[1] - corner[1])])
    thickness = sweep(page, 0, 1.6)
    outline = thickness.buffer(1.4, join_style=2)
    shadow = sweep(outline, 4.0, 0).difference(outline)
    shapes = [
        ("sheet-shadow", shadow, linear((outline.bounds[2] - 8, 0), (outline.bounds[2] + 4, 0),
                                        (0, ("#0c1830", 120)), (255, ("#0c1830", 0)))),
        ("sheet-outline", outline, solid("#2b2412")),
        ("sheet-edge", thickness, linear(page_point(0, 1), page_point(1, 1),
                                         (0, "#e3cf85"), (255, "#a88f3e"))),
        ("sheet", page, linear(page_point(0, 0.1), page_point(0.9, 1),
                               (0, "#fffef6"), (140, "#fbf2c6"), (255, "#efdc92"))),
    ]
    for i, (y, x_end) in enumerate(text_lines):
        line = page_quad(0.13, y, x_end, y + 0.035)
        shapes.append((f"text-{i}", line, solid("#6b5d35", 235), LOD_SOME if i % 2 == 0 else LOD_ALL))
    shapes += [
        ("fold-shadow", flap.buffer(1.6, join_style=2).intersection(page), solid("#8a7430", 90)),
        ("fold", flap, linear(b, a, (0, "#fffbe8"), (255, "#e2cf86"))),
    ]
    return page, shapes


# ---------------------------------------------------------------- the "M"

def m_letter():
    """Upright slab-serif M in letter space (x right, y up), about 22 x 24."""
    stems = [box(0, 0, 5, 24), box(15, 0, 20, 24)]
    diagonals = [Polygon([(0, 24), (5.4, 24), (12.4, 6.5), (7.6, 6.5)]),
                 Polygon([(20, 24), (14.6, 24), (7.6, 6.5), (12.4, 6.5)])]
    feet = [box(-1.2, 0, 6.6, 2.6), box(13.4, 0, 21.2, 2.6)]
    return unary_union(stems + diagonals + feet)


def to_screen(letter, base, scale):
    """Letter space -> screen: baseline runs 2:1 down (left-front face), verticals stay vertical."""
    return affinity.affine_transform(letter, [scale, 0, 0.5 * scale, -scale, base[0], base[1]])


def m_shapes(base=(30.5, 50.0), scale=1.0, depth=(3.6, -3.6), lod=None, prefix="m"):
    face = to_screen(m_letter(), base, scale)
    dx, dy = depth
    back = sweep(face, dx, dy)
    silhouette = unary_union([face, back])
    # split the extrusion into top-facing and right-facing surfaces
    tops, sides = [], []
    for ring in [face.exterior] if face.geom_type == "Polygon" else [p.exterior for p in face.geoms]:
        coords = list(orient(Polygon(ring)).exterior.coords)
        for (x0, y0), (x1, y1) in zip(coords, coords[1:]):
            ex, ey = x1 - x0, y1 - y0
            nx, ny = ey, -ex  # outward normal for a CCW (y-up) ring == CW on screen
            quad = Polygon([(x0, y0), (x1, y1), (x1 + dx, y1 + dy), (x0 + dx, y0 + dy)])
            facing_viewer = nx * dx + ny * dy > 0
            if not facing_viewer or not quad.is_valid or quad.area < 1e-3:
                continue
            (tops if ny < -abs(nx) * 0.3 else sides).append(quad)
    top_geom = unary_union(tops).difference(face) if tops else None
    side_geom = unary_union(sides).difference(face) if sides else None
    ymin, ymax = face.bounds[1], face.bounds[3]
    xmin, xmax = face.bounds[0], face.bounds[2]
    inner = face.buffer(-1.25 * scale, join_style=2)
    shadow = sweep(silhouette, 6.5 * scale, 0).difference(silhouette)

    def add(name, geom, style):
        entry = (f"{prefix}-{name}", geom, style)
        return entry + ((lod,) if lod else ())

    shapes = [
        add("shadow", shadow, linear((xmax - 2, 0), (xmax + 7 * scale, 0),
                                     (0, ("#101430", 110)), (255, ("#101430", 0)))),
        add("outline", silhouette.buffer(1.5 * scale, join_style=2), solid("#07103a")),
        add("side", side_geom, linear((xmax, ymin), (xmax + dx, ymax), (0, "#0b2f9c"), (255, "#061a66"))),
        add("top", top_geom, linear((xmin, ymin), (xmax + dx, ymin), (0, "#bfe0ff"), (255, "#5b9cf0"))),
        add("bevel", face, linear((xmin, ymin), (xmax, ymax),
                                  (0, "#a8d6ff"), (110, "#4c8dff"), (255, "#0d3fc4"))),
        add("face", inner, linear((xmin, ymin), (xmin + 4, ymax),
                                  (0, "#4a95ff"), (255, "#1447d6"))),
    ]
    return [s for s in shapes if s[1] is not None and not s[1].is_empty]


# Every shape, its outline and its shadow must stay inside the 64x64 icon
# square: Haiku clips anything outside it, which cut off the M's right foot
# and shadow at every size.
CANVAS = (1.0, 1.0, 63.0, 63.0)
BIG, SMALL = (0.3, 4.0), (0.0, 0.3)  # level-of-detail: 32px and up, 16px


def transform_style(style, matrix):
    if style[0] != "linear":
        return style
    a, b, d, e, xoff, yoff = matrix
    move = lambda p: (a * p[0] + b * p[1] + xoff, d * p[0] + e * p[1] + yoff)
    return ("linear", move(style[1]), move(style[2]), style[3])


def fit(shapes, lod):
    """Scale and center a set of shapes into CANVAS, and limit them to lod."""
    union = unary_union([shape[1] for shape in shapes])
    x0, y0, x1, y1 = union.bounds
    left, top, right, bottom = CANVAS
    scale = min((right - left) / (x1 - x0), (bottom - top) / (y1 - y0))
    dx = left + ((right - left) - scale * (x1 - x0)) / 2 - scale * x0
    dy = top + ((bottom - top) - scale * (y1 - y0)) / 2 - scale * y0
    matrix = (scale, 0, 0, scale, dx, dy)
    fitted = []
    for shape in shapes:
        name, geom, style = shape[:3]
        own = shape[3] if len(shape) > 3 else lod
        shown = (max(own[0], lod[0]), min(own[1], lod[1]))
        if shown[0] >= shown[1]:
            continue
        fitted.append((name, affinity.affine_transform(geom, matrix), transform_style(style, matrix), shown))
    return fitted


def app_shapes():
    _, sheet = sheet_shapes([(0.16, 0.62), (0.28, 0.8), (0.40, 0.72), (0.52, 0.55),
                             (0.64, 0.5), (0.76, 0.4)])
    # At 16px the sheet turns into a smudge behind the letter, so the app is
    # the M alone there, which also tells it apart from the document icon.
    return (fit(sheet + m_shapes(), BIG)
            + fit(m_shapes(base=(23.0, 56.0), scale=1.3, depth=(4.4, -4.4), prefix="m16"), SMALL))


def doc_shapes():
    _, sheet = sheet_shapes([(0.16, 0.62), (0.28, 0.84), (0.40, 0.75), (0.52, 0.86),
                             (0.64, 0.6), (0.76, 0.8), (0.88, 0.5)])
    plain_sheet = [s for s in sheet if not s[0].startswith("text-")]
    return (fit(sheet + m_shapes(base=(41.5, 50.5), scale=0.52, depth=(2.2, -2.2), prefix="badge"), BIG)
            + fit(plain_sheet + m_shapes(base=(33.0, 54.0), scale=0.85, depth=(3.2, -3.2), prefix="badge16"),
                  SMALL))


# ---------------------------------------------------------------- output


def rings(geom):
    geom = geom.simplify(0.08, preserve_topology=True)
    polys = [geom] if geom.geom_type == "Polygon" else [g for g in getattr(geom, "geoms", []) if g.geom_type == "Polygon"]
    out = []
    for poly in polys:
        poly = orient(poly, sign=1.0)
        out.append(list(poly.exterior.coords)[:-1])
        out += [list(r.coords)[:-1] for r in poly.interiors]
    return out


def gradient_matrix(p0, p1):
    """Affine (sx, shy, shx, sy, tx, ty) mapping gradient space x in [-64, 64] onto p0..p1."""
    dx, dy = (p1[0] - p0[0]) / 128.0, (p1[1] - p0[1]) / 128.0
    return (dx, dy, -dy, dx, (p0[0] + p1[0]) / 2, (p0[1] + p1[1]) / 2)


def svg_document(shapes, size=64):
    """SVG of the shapes visible at 64px (SVG has no level-of-detail, so the 16px-only "M" is left out)."""
    defs, body = [], []
    for i, shape in enumerate(shapes):
        name, geom, style = shape[:3]
        if len(shape) > 3 and not shape[3][0] <= 1.0 <= shape[3][1]:
            continue
        d = " ".join("M" + " L".join(f"{x:.2f} {y:.2f}" for x, y in ring) + " Z" for ring in rings(geom))
        if style[0] == "solid":
            r, g, b, a = style[1]
            fill = f'fill="#{r:02x}{g:02x}{b:02x}"' + (f' fill-opacity="{a / 255:.3f}"' if a < 255 else "")
        else:
            _, p0, p1, stops = style
            m = gradient_matrix(p0, p1)
            stop_xml = "".join(
                f'<stop offset="{o / 255:.3f}" stop-color="#{c[0]:02x}{c[1]:02x}{c[2]:02x}"'
                + (f' stop-opacity="{c[3] / 255:.3f}"' if c[3] < 255 else "") + "/>"
                for o, c in stops)
            defs.append(
                f'<linearGradient id="g{i}" gradientUnits="userSpaceOnUse" x1="-64" y1="0" x2="64" y2="0" '
                f'gradientTransform="matrix({" ".join(f"{v:.5f}" for v in m)})">{stop_xml}</linearGradient>')
            fill = f'fill="url(#g{i})"'
        body.append(f'<path id="{name}" {fill} fill-rule="evenodd" d="{d}"/>')
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{size}" height="{size}" viewBox="0 0 64 64">\n'
            f'<defs>{"".join(defs)}</defs>\n' + "\n".join(body) + "\n</svg>\n")


def coord(value):
    value = max(-128.0, min(192.0, value))
    if value == int(value) and -32 <= value <= 95:
        return bytes([int(value + 32)])
    raw = int(round((value + 128.0) * 102.0)) | 0x8000
    return bytes([raw >> 8, raw & 0xFF])


def float24(value):
    if value == 0:
        return b"\0\0\0"
    bits = struct.unpack("<I", struct.pack("<f", value))[0]
    sign = bits >> 31
    exponent = ((bits >> 23) & 0xFF) - 127
    mantissa = bits & 0x7FFFFF
    if not -32 <= exponent < 32:
        return b"\0\0\0"
    short = (sign << 23) | ((exponent + 32) << 17) | (mantissa >> 6)
    return bytes([short >> 16, (short >> 8) & 0xFF, short & 0xFF])


def hvif(shapes):
    styles, style_index, paths, shape_records = [], {}, [], []
    for shape in shapes:
        name, geom, style = shape[:3]
        lod = shape[3] if len(shape) > 3 else None
        key = repr(style)
        if key not in style_index:
            style_index[key] = len(styles)
            if style[0] == "solid":
                styles.append(bytes([1]) + bytes(style[1]))
            else:
                _, p0, p1, stops = style
                blob = bytes([2, 0, 1 << 1, len(stops)])
                blob += b"".join(float24(v) for v in gradient_matrix(p0, p1))
                blob += b"".join(bytes([o]) + bytes(c) for o, c in stops)
                styles.append(blob)
        path_ids = []
        for ring in rings(geom):
            if len(ring) > 255:
                raise ValueError(f"{name}: ring has {len(ring)} points")
            data = b"".join(coord(round(x, 2)) + coord(round(y, 2)) for x, y in ring)
            path_ids.append(len(paths))
            paths.append(bytes([(1 << 1) | (1 << 3), len(ring)]) + data)
        flags = 0
        extra = b""
        if lod:
            flags |= 1 << 3
            extra = bytes([int(lod[0] * 63.75), min(255, int(lod[1] * 63.75))])
        shape_records.append(bytes([10, style_index[key], len(path_ids)]) + bytes(path_ids)
                             + bytes([flags]) + extra)
    for label, items in (("styles", styles), ("paths", paths), ("shapes", shape_records)):
        if len(items) > 255:
            raise ValueError(f"too many {label}: {len(items)}")
    return (b"ncif" + bytes([len(styles)]) + b"".join(styles) + bytes([len(paths)]) + b"".join(paths)
            + bytes([len(shape_records)]) + b"".join(shape_records))


# ---------------------------------------------------------------- HVIF reader (verification)


class Reader:
    def __init__(self, data):
        self.data, self.pos = data, 0

    def u8(self):
        self.pos += 1
        return self.data[self.pos - 1]

    def coord(self):
        v = self.u8()
        if v & 128:
            return (((v & 127) << 8) | self.u8()) / 102.0 - 128.0
        return v - 32.0

    def f24(self):
        b = [self.u8() for _ in range(3)]
        short = (b[0] << 16) | (b[1] << 8) | b[2]
        if short == 0:
            return 0.0
        sign = (short & 0x800000) >> 23
        exponent = ((short & 0x7E0000) >> 17) - 32
        mantissa = (short & 0x01FFFF) << 6
        return struct.unpack("<f", struct.pack("<I", (sign << 31) | ((exponent + 127) << 23) | mantissa))[0]


def hvif_to_svg(data, render_size):
    """Decode HVIF (the subset Icon-O-Matic uses here) into SVG, honouring LOD at render_size."""
    r = Reader(data)
    assert data[:4] == b"ncif"
    r.pos = 4
    styles = []
    for _ in range(r.u8()):
        kind = r.u8()
        if kind == 1:
            styles.append(("solid", tuple(r.u8() for _ in range(4))))
        elif kind == 2:
            gtype, gflags, count = r.u8(), r.u8(), r.u8()
            assert gtype == 0
            m = tuple(r.f24() for _ in range(6)) if gflags & 2 else (1, 0, 0, 1, 0, 0)
            stops = [(r.u8(), tuple(r.u8() for _ in range(4))) for _ in range(count)]
            styles.append(("matrix", m, stops))
        else:
            raise ValueError(kind)
    paths = []
    for _ in range(r.u8()):
        flags, count = r.u8(), r.u8()
        assert flags & 8
        paths.append([(r.coord(), r.coord()) for _ in range(count)])
    scale = render_size / 64.0
    defs, body = [], []
    for i in range(r.u8()):
        assert r.u8() == 10
        style = styles[r.u8()]
        ids = [r.u8() for _ in range(r.u8())]
        flags = r.u8()
        visible = True
        if flags & 8:
            lo, hi = r.u8() / 63.75, r.u8() / 63.75
            visible = lo <= scale <= hi
        if not visible:
            continue
        d = " ".join("M" + " L".join(f"{x:.3f} {y:.3f}" for x, y in paths[p]) + " Z" for p in ids)
        if style[0] == "solid":
            c = style[1]
            fill = f'fill="rgb({c[0]},{c[1]},{c[2]})" fill-opacity="{c[3] / 255:.3f}"'
        else:
            stops = "".join(f'<stop offset="{o / 255:.3f}" stop-color="rgb({c[0]},{c[1]},{c[2]})" '
                            f'stop-opacity="{c[3] / 255:.3f}"/>' for o, c in style[2])
            defs.append(f'<linearGradient id="h{i}" gradientUnits="userSpaceOnUse" x1="-64" y1="0" x2="64" y2="0" '
                        f'gradientTransform="matrix({" ".join(f"{v:.5f}" for v in style[1])})">{stops}</linearGradient>')
            fill = f'fill="url(#h{i})"'
        body.append(f'<path {fill} fill-rule="evenodd" d="{d}"/>')
    assert r.pos == len(data), (r.pos, len(data))
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{render_size}" height="{render_size}" '
            f'viewBox="0 0 64 64"><defs>{"".join(defs)}</defs>{"".join(body)}</svg>')


# ---------------------------------------------------------------- legacy CMAP8 bitmaps


def beos_palette():
    text = (Path(__file__).parent / "beos_palette.h").read_text()
    values = [tuple(map(int, m)) for m in re.findall(r"\{\s*(\d+),\s*(\d+),\s*(\d+),\s*\d+\s*\}", text)]
    assert len(values) == 256
    return values


def cmap8(image, palette):
    """Quantise an RGBA PIL image to the BeOS system palette; 0xFF is B_TRANSPARENT_MAGIC_CMAP8."""
    out = bytearray()
    for r_, g_, b_, a_ in image.get_flattened_data():
        if a_ < 128:
            out.append(255)
            continue
        best, best_d = 0, None
        for idx, (pr, pg, pb) in enumerate(palette[:255]):
            rmean = (r_ + pr) // 2
            dist = (((512 + rmean) * (r_ - pr) ** 2) >> 8) + 4 * (g_ - pg) ** 2 + (((767 - rmean) * (b_ - pb) ** 2) >> 8)
            if best_d is None or dist < best_d:
                best, best_d = idx, dist
        out.append(best)
    return bytes(out)


def hexlines(data, width=32):
    return "\n".join(f'\t$"{data[i:i + width].hex().upper()}"' for i in range(0, len(data), width))


def main():
    import cairosvg
    from io import BytesIO
    from PIL import Image

    OUT.mkdir(exist_ok=True)
    PNG.mkdir(parents=True, exist_ok=True)
    palette = beos_palette()
    rdef = ["// Generated by tools/icons/haiku.py -- Minuta icons for Haiku.", ""]
    for name, maker in (("minuta", app_shapes), ("minuta-doc", doc_shapes)):
        shapes = maker()
        (OUT / f"{name}.svg").write_text(svg_document(shapes))
        data = hvif(shapes)
        (OUT / f"{name}.hvif").write_bytes(data)
        renders = {}
        for size in (16, 32, 48, 64, 128):
            png = cairosvg.svg2png(bytestring=hvif_to_svg(data, size).encode(), output_width=size,
                                   output_height=size)
            renders[size] = Image.open(BytesIO(png)).convert("RGBA")
            if size in (16, 32, 64, 128):
                renders[size].save(PNG / f"{name}-{size}.png")
        large, mini = cmap8(renders[32], palette), cmap8(renders[16], palette)
        for size, raw in ((32, large), (16, mini)):
            img = Image.new("RGBA", (size, size))
            img.putdata([(0, 0, 0, 0) if i == 255 else palette[i] + (255,) for i in raw])
            img.save(PNG / f"{name}-{size}-legacy.png")
        if name == "minuta":
            rdef += ["resource vector_icon {", hexlines(data), "};", "",
                     "resource large_icon array {", hexlines(large), "};", "",
                     "resource mini_icon array {", hexlines(mini), "};", ""]
        else:
            rdef += ['// Icon Tracker shows for text/markdown files when Minuta is the preferred app.',
                     'resource(0, "BEOS:text/markdown") #\'VICN\' array {', hexlines(data), "};", "",
                     'resource(0, "BEOS:L:text/markdown") #\'ICON\' array {', hexlines(large), "};", "",
                     'resource(0, "BEOS:M:text/markdown") #\'MICN\' array {', hexlines(mini), "};", ""]
        print(name, "hvif bytes:", len(data), "shapes:", len(shapes))
    RDEF.write_text("\n".join(rdef))


if __name__ == "__main__":
    main()
