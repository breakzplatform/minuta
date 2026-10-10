"""The Minuta M, shared by the Haiku vector icons and the Win98 pixel icons.

Letter space: x right, y up, about 22 x 24 units: two stems, diagonals that
meet low, and slab feet at the baseline.
"""
from shapely import affinity
from shapely.geometry import Polygon, box
from shapely.ops import unary_union


def m_letter():
    stems = [box(0, 0, 5, 24), box(15, 0, 20, 24)]
    diagonals = [Polygon([(0, 24), (5.4, 24), (12.4, 6.5), (7.6, 6.5)]),
                 Polygon([(20, 24), (14.6, 24), (7.6, 6.5), (12.4, 6.5)])]
    feet = [box(-1.2, 0, 6.6, 2.6), box(13.4, 0, 21.2, 2.6)]
    return unary_union(stems + diagonals + feet)


def m_mask(width, height):
    """The M rasterized on a width x height grid ('#' where a pixel is at
    least half covered), mirrored from its left half so it stays symmetric."""
    letter = m_letter()
    x0, y0, x1, y1 = letter.bounds
    sx, sy = width / (x1 - x0), height / (y1 - y0)
    letter = affinity.affine_transform(letter, [sx, 0, 0, -sy, -x0 * sx, height + y0 * sy])
    rows = []
    for y in range(height):
        row = "".join("#" if letter.intersection(box(x, y, x + 1, y + 1)).area >= 0.5 else "."
                      for x in range(width))
        left = row[:(width + 1) // 2]
        rows.append(left + left[:width // 2][::-1])
    return rows
