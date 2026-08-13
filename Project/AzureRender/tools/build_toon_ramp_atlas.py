#!/usr/bin/env python3
"""Build or validate the renderer-owned linear PPM toon-ramp atlas."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


def validate(document: dict) -> tuple[int, list[dict]]:
    if document.get("format") != "AzureRender Toon Ramp Profiles v1":
        raise ValueError("unsupported toon-ramp profile format")
    if document.get("version") != 1:
        raise ValueError("toon-ramp profile version must be 1")
    width = document.get("width")
    if not isinstance(width, int) or width < 8 or width > 4096:
        raise ValueError("toon-ramp width must be an integer in [8, 4096]")
    classes = document.get("classes")
    if not isinstance(classes, list) or len(classes) != 10:
        raise ValueError("toon-ramp profiles must define exactly 10 classes")
    for expected_id, profile in enumerate(classes):
        if profile.get("id") != expected_id:
            raise ValueError(f"expected class id {expected_id}")
        if profile.get("interpolation") not in ("linear", "step"):
            raise ValueError(f"class {expected_id} interpolation is invalid")
        stops = profile.get("stops")
        if not isinstance(stops, list) or len(stops) < 2:
            raise ValueError(f"class {expected_id} requires at least two stops")
        previous_position = -1.0
        for stop in stops:
            if not isinstance(stop, list) or len(stop) != 2:
                raise ValueError(f"class {expected_id} has an invalid stop")
            position, color = stop
            if not isinstance(position, (int, float)) or not 0.0 <= position <= 1.0:
                raise ValueError(f"class {expected_id} stop position is invalid")
            if position <= previous_position:
                raise ValueError(f"class {expected_id} stops must be increasing")
            if not isinstance(color, list) or len(color) != 3:
                raise ValueError(f"class {expected_id} stop color is invalid")
            if not all(isinstance(value, (int, float)) and 0.0 <= value <= 1.0 for value in color):
                raise ValueError(f"class {expected_id} stop color is outside [0, 1]")
            previous_position = float(position)
        if stops[0][0] != 0.0 or stops[-1][0] != 1.0:
            raise ValueError(f"class {expected_id} stops must cover [0, 1]")
    return width, classes


def sample(stops: list, position: float, interpolation: str) -> list[float]:
    if interpolation == "step":
        result = list(stops[0][1])
        for stop_position, color in stops[1:]:
            if position < stop_position:
                break
            result = list(color)
        return result
    for index in range(1, len(stops)):
        end_position, end_color = stops[index]
        if position <= end_position:
            start_position, start_color = stops[index - 1]
            factor = (position - start_position) / (end_position - start_position)
            return [
                start_color[channel]
                + (end_color[channel] - start_color[channel]) * factor
                for channel in range(3)
            ]
    return list(stops[-1][1])


def build_ppm(document: dict) -> bytes:
    width, classes = validate(document)
    lines = ["P3", f"{width} {len(classes)}", "255"]
    for profile in classes:
        pixels: list[str] = []
        for x in range(width):
            position = x / (width - 1)
            color = sample(
                profile["stops"], position, profile["interpolation"]
            )
            pixels.extend(str(round(min(value, 1.0) * 255.0)) for value in color)
        lines.append(" ".join(pixels))
    return ("\n".join(lines) + "\n").encode("ascii")


def main() -> None:
    parser = argparse.ArgumentParser()
    root = Path(__file__).resolve().parents[1]
    parser.add_argument(
        "--profiles",
        type=Path,
        default=root / "assets_public" / "toon_ramp_profiles.json",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=root / "assets_public" / "toon_ramp_atlas.ppm",
    )
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()

    document = json.loads(args.profiles.read_text(encoding="utf-8-sig"))
    expected = build_ppm(document)
    if args.check:
        if not args.output.is_file() or args.output.read_bytes() != expected:
            raise SystemExit(f"toon-ramp atlas is stale: {args.output}")
        print(f"Validated toon-ramp atlas: {args.output}")
        return
    args.output.write_bytes(expected)
    print(f"Wrote {args.output} ({len(expected)} bytes)")


if __name__ == "__main__":
    main()
