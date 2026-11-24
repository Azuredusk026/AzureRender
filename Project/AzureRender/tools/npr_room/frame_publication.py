"""Publish a hash-bound image/depth frame before acknowledging its producer."""

import hashlib
import json
import re
import numpy as np
from PIL import Image
from io import BytesIO

from tools.npr_room.catalog_storage import publish_directory


def publish_ready_frame(source, target, stem):
    if not re.fullmatch(r"(Night|Cloudy)-(0[0-9]|1[01])", stem):
        raise ValueError("Unexpected original frame identity")
    source, target = source.resolve(), target.resolve()
    if source.anchor != target.anchor or source == target:
        raise ValueError("Frame publication requires distinct same-volume roots")
    marker = source / (stem + ".ready")
    if not marker.exists():
        return None
    ready = json.loads(marker.read_bytes())
    names = [stem + ".png", stem + "-depth.f32", stem + ".json"]
    inputs = [source / name for name in names]
    outputs = [target / name for name in names]
    if any(before.exists() and after.exists() for before, after in zip(inputs, outputs, strict=True)):
        raise FileExistsError("Frame publication target already contains an input")
    paths = [before if before.exists() else after for before, after in zip(inputs, outputs, strict=True)]
    if not all(path.is_file() for path in paths):
        raise ValueError("Ready frame is missing an image, depth or descriptor")
    data = [path.read_bytes() for path in paths]
    hashes = [hashlib.sha256(payload).hexdigest() for payload in data]
    if hashes != [ready[key] for key in ("colorSHA256", "depthSHA256", "metadataSHA256")]:
        raise ValueError("Ready frame payload fingerprint differs")
    record = json.loads(data[2])
    if (record["metadataOnly"] or record["frame"] != ready["frame"]
            or f'{record["weather"]}-{record["index"]:02d}' != stem
            or record["color"] != names[0] or record["depth"] != names[1]
            or record["depthGraphicsFormat"] != "R32_SFloat"):
        raise ValueError("Ready frame descriptor identity differs")
    width, height = record["width"], record["height"]
    if type(width) is not int or type(height) is not int or width <= 0 or height <= 0:
        raise ValueError("Ready frame requires a positive viewport")
    if len(data[1]) != width * height * 4:
        raise ValueError("Ready frame depth length differs from its viewport")
    depth = np.frombuffer(data[1], dtype="<f4")
    if not np.isfinite(depth).all() or np.any((depth < 0) | (depth > 1)):
        raise ValueError("Ready frame requires finite normalized depth")
    with Image.open(BytesIO(data[0])) as image:
        if image.format != "PNG" or image.size != (width, height):
            raise ValueError("Ready frame color viewport or encoding differs")
        image.verify()
    moved = []
    try:
        for before, after in zip(inputs, outputs, strict=True):
            if before.exists():
                publish_directory(before, after)
                moved.append((before, after))
    except Exception:
        for before, after in reversed(moved):
            publish_directory(after, before)
        raise
    marker.unlink()
    return {"metadata": names[2], "frame": record["frame"], "bytes": sum(map(len, data)),
            "colorSHA256": hashes[0], "depthSHA256": hashes[1], "metadataSHA256": hashes[2]}
