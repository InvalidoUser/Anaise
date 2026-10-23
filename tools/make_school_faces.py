"""Offline import of entirely fictional adult college character sheets.

Run with Pillow in the workspace dependency runtime, not from the game.
The checked-in PNG masters contain pixels only; the game embeds luminance BMPs.
"""
from pathlib import Path
from PIL import Image, ImageEnhance, ImageOps

ROOT = Path(__file__).resolve().parent.parent
ASSETS = ROOT / "assets" / "school"
SIZE = 256


def build_atlases():
    for name in ("hana", "rei", "yumi", "mika"):
        sheet = Image.open(ASSETS / "masters" / f"{name}-expressions.png").convert("RGB")
        w, h = sheet.width // 3, sheet.height // 2
        # Neutral, happy, shy / hurt, afraid, dead. No real-person photos.
        cells = [sheet.crop((col * w, row * h, (col + 1) * w, (row + 1) * h))
                 for row in range(2) for col in range(3)]
        atlas = Image.new("L", (SIZE * 6, SIZE), 0)
        for expression, cell in enumerate(cells):
            cell = cell.resize((SIZE, SIZE), Image.Resampling.LANCZOS)
            luminance = ImageOps.autocontrast(cell.convert("L"), cutoff=1)
            luminance = ImageEnhance.Contrast(luminance).enhance(1.12)
            # Dither at the final destination resolution in the game: baking
            # dots before a projected resize creates distracting moire.
            atlas.paste(luminance, (expression * SIZE, 0))
        atlas.save(ASSETS / f"{name}-faces.bmp")


if __name__ == "__main__":
    build_atlases()
