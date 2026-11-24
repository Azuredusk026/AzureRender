#!/usr/bin/env python3
"""Tolerance-based RGB image comparison for visual regression gates."""

import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("reference", type=Path)
    parser.add_argument("candidate", type=Path)
    parser.add_argument("--max-mean-error", type=float, default=0.005)
    parser.add_argument("--max-changed-ratio", type=float, default=0.01)
    parser.add_argument("--pixel-threshold", type=float, default=2.0 / 255.0)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    reference = np.asarray(Image.open(args.reference).convert("RGB"), dtype=np.float32) / 255.0
    candidate = np.asarray(Image.open(args.candidate).convert("RGB"), dtype=np.float32) / 255.0
    if reference.shape != candidate.shape:
        result = {"status": "failed", "reason": "dimension-mismatch",
                  "reference_shape": reference.shape, "candidate_shape": candidate.shape}
    else:
        difference = np.abs(reference - candidate)
        mean_error = float(np.mean(difference))
        changed_ratio = float(np.mean(np.max(difference, axis=2) > args.pixel_threshold))
        passed = mean_error <= args.max_mean_error and changed_ratio <= args.max_changed_ratio
        result = {
            "schema_version": 1,
            "status": "passed" if passed else "failed",
            "mean_absolute_rgb_error": mean_error,
            "changed_pixel_ratio": changed_ratio,
            "pixel_threshold": args.pixel_threshold,
            "limits": {
                "mean_absolute_rgb_error": args.max_mean_error,
                "changed_pixel_ratio": args.max_changed_ratio,
            },
        }
    encoded = json.dumps(result, indent=2) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(encoded, encoding="utf-8")
    print(encoded, end="")
    return 0 if result["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
