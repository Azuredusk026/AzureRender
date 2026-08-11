#!/usr/bin/env python3
"""Build a labelled contact sheet from an AzureRender CQ-0 QA index."""

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


def load_font(size: int) -> ImageFont.ImageFont:
    for candidate in ("arial.ttf", "segoeui.ttf", "DejaVuSans.ttf"):
        try:
            return ImageFont.truetype(candidate, size)
        except OSError:
            pass
    return ImageFont.load_default()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("index", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--columns", type=int, default=3)
    parser.add_argument("--cell-width", type=int, default=480)
    args = parser.parse_args()

    if args.columns < 1 or args.cell_width < 160:
        raise SystemExit("columns must be >= 1 and cell-width must be >= 160")

    index_path = args.index.resolve()
    data = json.loads(index_path.read_text(encoding="utf-8-sig"))
    cases = data.get("cases", [])
    if not cases:
        raise SystemExit(f"No QA cases in {index_path}")

    source_images: list[tuple[str, Image.Image]] = []
    for case in cases:
        frame = index_path.parent / case["directory"] / "frame_000000.png"
        if not frame.is_file():
            raise SystemExit(f"Missing QA frame: {frame}")
        source_images.append((case["name"], Image.open(frame).convert("RGB")))

    source_width, source_height = source_images[0][1].size
    image_height = round(args.cell_width * source_height / source_width)
    label_height = 44
    gutter = 12
    rows = math.ceil(len(source_images) / args.columns)
    sheet_width = gutter + args.columns * (args.cell_width + gutter)
    sheet_height = gutter + rows * (image_height + label_height + gutter)
    sheet = Image.new("RGB", (sheet_width, sheet_height), (18, 23, 33))
    draw = ImageDraw.Draw(sheet)
    font = load_font(20)

    for index, (name, source) in enumerate(source_images):
        column = index % args.columns
        row = index // args.columns
        x = gutter + column * (args.cell_width + gutter)
        y = gutter + row * (image_height + label_height + gutter)
        resized = source.resize((args.cell_width, image_height), Image.Resampling.LANCZOS)
        sheet.paste(resized, (x, y))
        draw.text((x + 8, y + image_height + 10), name, fill=(225, 235, 245), font=font)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(args.output, optimize=True)
    print(f"Contact sheet: {args.output.resolve()}")


if __name__ == "__main__":
    main()
