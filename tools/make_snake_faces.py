"""Rebuild Snake's CC0 portrait crops from the adjacent original JPEGs.

Development-only Pillow import. Source and license records live in
assets/snake/CREDITS.md and the per-object JSON files. No network is used.
"""
from pathlib import Path
from PIL import Image, ImageOps, ImageEnhance

ASSETS = Path(__file__).resolve().parent.parent / "assets" / "snake"
CROPS = {
    436532: (.25, .32, .75, .79),
    436323: (.40, .20, .66, .47),
    437397: (.30, .22, .66, .58),
}

for object_id, crop in CROPS.items():
    image = Image.open(ASSETS / f"{object_id}.jpg")
    bounds = tuple(int(part * size) for part, size in
                   zip(crop, (image.width, image.height) * 2))
    image = image.crop(bounds).resize((128, 128), Image.Resampling.LANCZOS)
    image = ImageOps.autocontrast(image.convert("L"), cutoff=1)
    image = ImageEnhance.Contrast(image).enhance(1.15)
    image.save(ASSETS / f"{object_id}-face.bmp")
