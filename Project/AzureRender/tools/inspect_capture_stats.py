#!/usr/bin/env python3
"""Reports basic pixel statistics for captured frames.

Used to confirm that an isolation view actually produces distinguishable
output before it is promoted to a visual-regression baseline.
"""

import argparse
import hashlib
from pathlib import Path

import numpy as np
from PIL import Image


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("images", type=Path, nargs="+")
    args = parser.parse_args()

    for path in args.images:
        if not path.is_file():
            print(f"{path}: MISSING")
            continue
        pixels = np.asarray(Image.open(path).convert("RGB"), dtype=np.uint8)
        digest = hashlib.sha256(path.read_bytes()).hexdigest()[:16]
        print(
            f"{str(path):58s} "
            f"min={pixels.min():3d} max={pixels.max():3d} "
            f"mean={pixels.mean():7.3f} nonzero={np.count_nonzero(pixels):8d} "
            f"sha={digest}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
