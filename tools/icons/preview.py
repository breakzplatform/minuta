"""Contact sheets: Win98 icons on teal/white at 1x and 4x, Haiku icons on the Haiku desktop blue.

Reads the PNGs that win98.py and haiku.py leave in build/icons and writes
build/icons/{win98,haiku,preview}.png.
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

OUT = Path(__file__).resolve().parents[2] / "build" / "icons"
WIN = OUT / "win98"
HAIKU = OUT / "haiku"

TEAL, WHITE, HAIKU_BLUE = (0, 128, 128), (255, 255, 255), (51, 102, 152)
FONT = ImageFont.load_default(size=14)


def win_panel(background, depth):
    width, height = 1500, 520
    panel = Image.new("RGBA", (width, height), background + (255,))
    draw = ImageDraw.Draw(panel)
    ink = (255, 255, 255) if background == TEAL else (0, 0, 0)
    draw.text((12, 8), f"Win98 {depth}-bit  {'#008080' if background == TEAL else 'white'}", fill=ink, font=FONT)
    x = 12
    for name in ("minuta", "minutadoc", "minutaun"):
        for size in (48, 32, 16):
            icon = Image.open(WIN / f"{name}-{size}-{depth}bit.png")
            panel.alpha_composite(icon, (x, 40))
            big = icon.resize((size * 4, size * 4), Image.NEAREST)
            panel.alpha_composite(big, (x, 100))
            draw.text((x, 100 + size * 4 + 6), f"{size}x{size}", fill=ink, font=FONT)
            x += size * 4 + 14
        x += 10
    return panel.crop((0, 0, x, 330))


def haiku_panel():
    panel = Image.new("RGBA", (1000, 470), HAIKU_BLUE + (255,))
    draw = ImageDraw.Draw(panel)
    draw.text((12, 8), "Haiku  #336698  (HVIF render at native size; right: legacy CMAP8 32/16)",
              fill=(255, 255, 255), font=FONT)
    y = 36
    for name in ("minuta", "minuta-doc"):
        x = 12
        for size in (128, 64, 32, 16):
            panel.alpha_composite(Image.open(HAIKU / f"{name}-{size}.png"), (x, y + 128 - size))
            draw.text((x, y + 134), str(size), fill=(255, 255, 255), font=FONT)
            x += size + 24
        for size in (32, 16):
            panel.alpha_composite(Image.open(HAIKU / f"{name}-{size}-legacy.png"), (x, y + 128 - size))
            draw.text((x, y + 134), f"{size}L", fill=(255, 255, 255), font=FONT)
            x += size + 24
        x += 20
        for size in (64, 32, 16):
            big = Image.open(HAIKU / f"{name}-{size}.png").resize((128, 128), Image.NEAREST)
            panel.alpha_composite(big, (x, y))
            draw.text((x, y + 134), f"{size} @{128 // size}x", fill=(255, 255, 255), font=FONT)
            x += 142
        y += 210
    return panel.crop((0, 0, 950, y))


def stack(images, gap=10):
    width = max(i.width for i in images)
    sheet = Image.new("RGBA", (width, sum(i.height for i in images) + gap * (len(images) - 1)), (40, 40, 40, 255))
    y = 0
    for img in images:
        sheet.alpha_composite(img, (0, y))
        y += img.height + gap
    return sheet


def main():
    OUT.mkdir(exist_ok=True)
    win = stack([win_panel(TEAL, 8), win_panel(WHITE, 8), win_panel(TEAL, 4), win_panel(WHITE, 4)])
    haiku = haiku_panel()
    win.save(OUT / "win98.png")
    haiku.save(OUT / "haiku.png")
    stack([win, haiku]).save(OUT / "preview.png")


if __name__ == "__main__":
    main()
