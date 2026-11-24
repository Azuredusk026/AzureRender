"""Verify a native Player capture of the authored desktop loading loop."""

import argparse
from hashlib import sha256
import json
from pathlib import Path

from PIL import Image

from tools.npr_room.startup_splash import BOOT, desktop_elements
from tools.npr_room.skinned_mesh_export import UnityScene


def verify(source, capture, report):
    manifest = json.loads((capture / "capture_manifest.json").read_bytes())
    state = json.loads(report.read_bytes())
    width, height = manifest["width"], manifest["height"]
    frames = sorted(capture.glob("frame_*.png"))
    if manifest["fps"] != 60 or len(frames) < 91 or len(frames) != manifest["capturedFrames"]:
        raise ValueError("Capture must include one complete 60 Hz loading loop")
    if state["scriptErrors"] or state["presentationErrors"] or state["uiDrawCalls"] < len(frames):
        raise ValueError("Loading capture contains runtime errors or absent UI draws")

    _, background = desktop_elements(UnityScene(source / BOOT))["Image"]
    expected = tuple(round(background["m_Color"][key] * 255) for key in "rgba")
    area = (width * 7 // 20, height // 5, width * 13 // 20, height * 3 // 4)
    phases = set()
    for frame in frames:
        with Image.open(frame) as image:
            rgba = image.convert("RGBA")
            actual = rgba.getpixel((10, 10))
            if rgba.size != (width, height) or actual != expected:
                raise ValueError(f"Loading image mismatch at {frame.name}: {actual}, expected {expected}")
            phases.add(sha256(rgba.crop(area).tobytes()).hexdigest())
    if len(phases) < 2:
        raise ValueError("Loading sprites did not animate in the captured frames")
    return {"frames": len(frames), "distinctCenterRegions": len(phases), "size": [width, height],
            "backgroundRGBA": expected, "uiDrawCalls": state["uiDrawCalls"]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for argument in ("source", "capture", "report"):
        parser.add_argument("--" + argument, type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(verify(args.source, args.capture, args.report), sort_keys=True))


if __name__ == "__main__":
    main()
