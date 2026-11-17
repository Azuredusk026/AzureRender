"""Prepare and verify the original Windows opening noise capture."""

import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image

from tools.npr_room.build_city_window import verified_path
from tools.npr_room.reference_inventory import digest


PROGRAM = "windows-opening-effects-programs.json"
BINDINGS = "windows-opening-effects-source-lock.json"
BACKGROUND = np.array([.25, .5, .75, 1], dtype=np.float32)


def prepare(effects, catalog):
    bindings = json.loads((effects / BINDINGS).read_bytes())
    program_path = effects / PROGRAM
    if bindings["programsSHA256"] != digest(program_path):
        raise ValueError("Direct3D opening program receipt moved away from its source lock")
    programs = json.loads(program_path.read_bytes())["programs"]
    for stage in programs["noise"]["stages"].values():
        if digest(effects / stage["path"]) != stage["sha256"]:
            raise ValueError("Original noise bytecode differs")
    native, = (item for item in bindings["renderers"]["High"]["passes"] if item["shaderProgram"] == "noise")
    expected = {"srcBlend": 5, "destBlend": 10, "srcBlendAlpha": 1, "destBlendAlpha": 10,
                "blendOp": 0, "blendOpAlpha": 0, "colMask": 15}
    if native["renderState"]["blend"] != expected or native["floats"]["_Unstable"] != 0:
        raise ValueError("Original noise requires its authored blend and initial parameters")
    timeline, = native["textures"].values()
    if not timeline["srgb"] or timeline["binding"] != 0:
        raise ValueError("Original noise requires the sRGB texture at slot zero")
    png = verified_path(catalog, timeline)
    with Image.open(png) as image:
        if image.size != (64, 1):
            raise ValueError("The original noise timeline requires 64 color samples")
        with (effects / "noise-timeline.rgba").open("xb") as output:
            output.write(image.convert("RGBA").tobytes())


def verify(first, repeat):
    checksum = digest(first)
    if checksum != digest(repeat):
        raise ValueError("Original noise captures differ under identical inputs")
    frames = np.fromfile(first, dtype="<f4").reshape(3, 180, 320, 4)
    if not np.array_equal(frames[0], np.broadcast_to(BACKGROUND, frames[0].shape)):
        raise ValueError("Zero original noise changed the control frame")
    if not np.isfinite(frames).all() or not np.allclose(frames[:, :, :, 3], 1, atol=1e-7, rtol=0):
        raise ValueError("Original noise changed the target alpha or produced a non-finite value")
    expected = np.array([[.2375, .475, .7125], [.2875, .525, .7625]], dtype=np.float32)
    count = []
    for frame in frames[1:]:
        unique, pixels = np.unique(frame[..., :3].reshape(-1, 3), axis=0, return_counts=True)
        if unique.shape != expected.shape or not np.allclose(unique, expected, atol=2e-7, rtol=0):
            raise ValueError("Original noise was not composited at the specified 0.05 ratio")
        count.append([int(value) for value in pixels])
    moving = int(np.count_nonzero(np.any(frames[1] != frames[2], axis=2)))
    if moving < 0.2 * 320 * 180:
        raise ValueError("Original time input failed to advance the noise pattern")
    return {"size": [320, 180], "frames": [[9.302, 0], [9.302, .05], [9.802, .05]],
            "pixelCounts": count, "timeChangedPixels": moving, "captureSHA256": checksum}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="operation", required=True)
    options = {"prepare": ("effects", "catalog"), "verify": ("first", "repeat", "output")}
    for operation, paths in options.items():
        command = subparsers.add_parser(operation)
        for path in paths:
            command.add_argument("--" + path, type=Path, required=True)
    args = parser.parse_args()
    if args.operation == "prepare":
        prepare(args.effects, args.catalog)
    else:
        report = verify(args.first, args.repeat)
        with args.output.open("x", encoding="utf-8", newline="\n") as output:
            json.dump(report, output, sort_keys=True)
            output.write("\n")
        print(json.dumps(report))
