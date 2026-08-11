#!/usr/bin/env python3
"""Build a Reference / Current / Isolation review sheet from a JSON layout."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


def font(size: int) -> ImageFont.ImageFont:
    for name in ("arial.ttf", "segoeui.ttf", "DejaVuSans.ttf"):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            pass
    return ImageFont.load_default()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("layout", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--cell-width", type=int, default=520)
    args = parser.parse_args()

    layout_path = args.layout.resolve()
    layout = json.loads(layout_path.read_text(encoding="utf-8-sig"))
    rows = layout.get("rows", [])
    columns = ("reference", "current", "isolation")
    if not rows:
        raise SystemExit("Comparison layout contains no rows")

    sources: list[list[Image.Image]] = []
    for row in rows:
        row_sources = []
        for column in columns:
            path = (layout_path.parent / row[column]).resolve()
            if not path.is_file():
                raise SystemExit(f"Missing comparison image: {path}")
            row_sources.append(Image.open(path).convert("RGB"))
        sources.append(row_sources)

    cell_width = args.cell_width
    cell_height = round(cell_width * 9 / 16)
    gutter = 12
    header_height = 54
    row_label_height = 42
    sheet = Image.new(
        "RGB",
        (
            gutter + 3 * (cell_width + gutter),
            gutter + header_height + len(rows) * (cell_height + row_label_height + gutter),
        ),
        (18, 23, 33),
    )
    draw = ImageDraw.Draw(sheet)
    header_font = font(26)
    label_font = font(20)
    for column_index, column in enumerate(columns):
        x = gutter + column_index * (cell_width + gutter)
        draw.text((x + 8, gutter + 10), column.upper(), fill=(115, 220, 240), font=header_font)

    for row_index, (row, row_sources) in enumerate(zip(rows, sources)):
        y = gutter + header_height + row_index * (cell_height + row_label_height + gutter)
        for column_index, source in enumerate(row_sources):
            x = gutter + column_index * (cell_width + gutter)
            source.thumbnail((cell_width, cell_height), Image.Resampling.LANCZOS)
            tile = Image.new("RGB", (cell_width, cell_height), (7, 10, 16))
            tile.paste(
                source,
                ((cell_width - source.width) // 2, (cell_height - source.height) // 2),
            )
            sheet.paste(tile, (x, y))
            draw.text(
                (x + 8, y + cell_height + 8),
                row["label"],
                fill=(225, 235, 245),
                font=label_font,
            )

    args.output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(args.output, optimize=True)
    print(f"Comparison sheet: {args.output.resolve()}")


if __name__ == "__main__":
    main()
