#!/usr/bin/env python3
"""Rebuild the runtime LOCUS icon and Home wordmark from canonical SVG sources.

Requires: pip install cairosvg Pillow
Run: python design/ui/v1/render_brand.py
"""
from pathlib import Path
import io

import cairosvg
from PIL import Image

ROOT = Path(__file__).resolve().parents[3]
SVG_DIR = ROOT / "design" / "ui" / "v1"
PNG_DIR = ROOT / "assets" / "ui"

ASSETS = (
    ("app-icon", (256, 256)),
    ("wordmark", (460, 136)),
)


def main() -> None:
    PNG_DIR.mkdir(parents=True, exist_ok=True)
    for name, size in ASSETS:
        svg = SVG_DIR / f"{name}.svg"
        png = PNG_DIR / f"{name}.png"
        raster = cairosvg.svg2png(url=str(svg), output_width=size[0],
                                  output_height=size[1])
        image = Image.open(io.BytesIO(raster)).convert("RGBA")
        image = image.quantize(colors=32, method=Image.Quantize.FASTOCTREE,
                               dither=Image.Dither.NONE)
        image.save(png, format="PNG", optimize=True, compress_level=9)
        print(f"{svg.relative_to(ROOT)} -> {png.relative_to(ROOT)} ({size[0]}x{size[1]})")


if __name__ == "__main__":
    main()
