#!/usr/bin/env python3
"""Compares nonzero Morph output from Compute and vertex fallback paths."""

import argparse
import shutil
from pathlib import Path

import numpy as np
from PIL import Image

from run_visual_regression import run_renderer

PROJECT_ROOT = Path(__file__).resolve().parent.parent


def capture(
    executable: Path,
    output: Path,
    weights: tuple[float, float],
    disable_compute: bool,
) -> Path:
    if output.exists():
        shutil.rmtree(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    command = [
        str(executable),
        "--asset", str(PROJECT_ROOT / "assets_public/test_model.gltf"),
        "--qa-camera", "full-body-front",
        "--qa-light", "stylized-key",
        "--qa-morph-weights", str(weights[0]), str(weights[1]),
        "--width", "1280",
        "--height", "720",
        "--capture-dir", str(output),
        "--capture-frames", "16",
        "--capture-fps", "60",
    ]
    if disable_compute:
        command.append("--disable-compute-skinning")
    completed = run_renderer(command)
    detail = completed.stderr + "\n" + completed.stdout
    if completed.returncode != 0:
        if "No GPU" in detail or "No Vulkan" in detail:
            raise RuntimeError("SKIP: no usable Vulkan GPU")
        raise RuntimeError(detail.strip())
    if "1 Morph targets" not in detail:
        raise RuntimeError("the public validation asset has no Morph target")
    frame = output / "frame_000015.png"
    if not frame.is_file():
        raise RuntimeError("render produced no Morph capture")
    return frame


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    try:
        zero_frame = capture(
            args.executable, args.output_dir / "zero", (0.0, 0.0), False)
        compute_frame = capture(
            args.executable, args.output_dir / "compute", (0.75, 0.0), False)
        fallback_frame = capture(
            args.executable, args.output_dir / "fallback", (0.75, 0.0), True)
    except RuntimeError as error:
        message = str(error)
        print(message)
        return 2 if message.startswith("SKIP:") else 1

    zero = np.asarray(Image.open(zero_frame).convert("RGB"), dtype=np.float32)
    compute = np.asarray(Image.open(compute_frame).convert("RGB"), dtype=np.float32)
    fallback = np.asarray(Image.open(fallback_frame).convert("RGB"), dtype=np.float32)
    if zero.shape != compute.shape or compute.shape != fallback.shape:
        print("Morph capture dimensions differ")
        return 1

    morph_difference = np.abs(zero - compute) / 255.0
    path_difference = np.abs(compute - fallback) / 255.0
    morph_mean = float(morph_difference.mean())
    morph_changed = float((morph_difference.max(axis=2) > 1.0 / 255.0).mean())
    path_mean = float(path_difference.mean())
    path_changed = float((path_difference.max(axis=2) > 1.0 / 255.0).mean())
    print(
        f"Morph change: mean={morph_mean:.6f} changed={morph_changed:.6f}; "
        f"Compute/fallback: mean={path_mean:.6f} changed={path_changed:.6f}")
    if morph_mean < 0.0001 or morph_changed < 0.001:
        print("the nonzero Morph weight did not change the captured image")
        return 1
    if path_mean > 0.001 or path_changed > 0.005:
        print("Compute Morph differs from the vertex fallback")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
