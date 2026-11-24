#!/usr/bin/env python3
"""Checks that clustered point lights change the public character capture."""

import argparse
import json
import re
import shutil
from pathlib import Path

import numpy as np
from PIL import Image

from run_visual_regression import run_renderer

PROJECT_ROOT = Path(__file__).resolve().parent.parent


def capture(executable: Path, scene: Path, output: Path) -> Path:
    if output.exists():
        shutil.rmtree(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    completed = run_renderer([
        str(executable),
        "--scene", str(scene),
        "--width", "1280",
        "--height", "720",
        "--capture-dir", str(output),
        "--capture-frames", "1",
        "--capture-fps", "60",
    ])
    detail = completed.stderr.strip() or completed.stdout.strip()
    if completed.returncode != 0:
        if "No GPU" in detail or "No Vulkan" in detail:
            raise RuntimeError("SKIP: no usable Vulkan GPU")
        raise RuntimeError(f"render failed for {scene.name}: {detail}")
    if re.search(r"validation error|VUID-", detail, re.IGNORECASE):
        raise RuntimeError(f"Vulkan Validation reported an error for {scene.name}")
    frame = output / "frame_000000.png"
    if not frame.is_file():
        raise RuntimeError(f"render produced no capture for {scene.name}")
    return frame


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    empty_scene = PROJECT_ROOT / "assets_public/scenes/clustered_lights_empty.azscene"
    lights_scene = PROJECT_ROOT / "assets_public/scenes/clustered_lights_16.azscene"
    try:
        empty_frame = capture(
            args.executable, empty_scene, args.output_dir / "empty")
        lights_frame = capture(
            args.executable, lights_scene, args.output_dir / "lights")
    except RuntimeError as error:
        message = str(error)
        print(message)
        return 2 if message.startswith("SKIP:") else 1

    empty = np.asarray(Image.open(empty_frame).convert("RGB"), dtype=np.float32)
    lights = np.asarray(Image.open(lights_frame).convert("RGB"), dtype=np.float32)
    if empty.shape != lights.shape:
        print("capture dimensions differ")
        return 1
    difference = np.abs(empty - lights) / 255.0
    mean_error = float(difference.mean())
    changed_ratio = float((difference.max(axis=2) > 1.0 / 255.0).mean())
    summary = {
        "schemaVersion": 1,
        "lightCount": 16,
        "meanAbsoluteRgbError": mean_error,
        "changedPixelRatio": changed_ratio,
    }
    args.output_dir.mkdir(parents=True, exist_ok=True)
    (args.output_dir / "summary.json").write_text(
        json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(
        f"16 clustered lights: mean={mean_error:.6f} "
        f"changed={changed_ratio:.6f}")
    if mean_error < 0.001 or changed_ratio < 0.01:
        print("clustered point lights did not produce the expected image change")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
