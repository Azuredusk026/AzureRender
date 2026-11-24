"""Compose source sky and cloud samples in linear light."""

import argparse
import gzip
from hashlib import sha256
import json
from pathlib import Path
import re

import numpy as np

from tools.npr_room.reference_inventory import digest
from tools.npr_room.windows_sky_environment import equirectangular, rgbe, write_hdr


def compose(sky, cloud):
    size = sky.shape[1]
    if not np.all(np.isfinite(sky)) or not np.all(sky[..., 3] == 1):
        raise ValueError("Sky has invalid radiance or incomplete coverage")
    alpha = cloud[..., 3:]
    if not np.all(np.isfinite(cloud)) or np.any((alpha < 0) | (alpha > 1)):
        raise ValueError("Cloud has invalid radiance or coverage")
    # Interpolate already associated RGB and coverage across the cloud grid.
    transport = np.concatenate((cloud[..., :3] * alpha, alpha), axis=-1)
    layer = equirectangular(transport, size)
    background = equirectangular(sky[..., :3], size)
    return layer[..., :3] + background * (1 - layer[..., 3:])


def load_cube(path, checksum, size):
    payload = gzip.decompress(path.read_bytes())
    if sha256(payload).hexdigest() != checksum:
        raise ValueError(f"Original program output differs: {path.name}")
    return np.frombuffer(payload, dtype="<f4").reshape(6, size, size, 4)


def convert(sky_file, sky_report, cloud_file, cloud_report, frame, output):
    sky = json.loads(sky_report.read_bytes())
    cloud = json.loads(cloud_report.read_bytes())
    cloud_frame = cloud["frames"][frame]
    snapshot = cloud_frame["inputs"]
    if (sky["mode"] != cloud["mode"] or sky["preparation"]["sourceSeconds"] != snapshot["seconds"]
            or sky["preparation"]["referenceSHA256"] != snapshot["referenceSHA256"]
            or cloud["program"] != "cozy-cloud"):
        raise ValueError("Sky and cloud samples must use the same weather and clock")
    captured = (sky["preparation"].get("input") == "captured-room-program-globals"
                or "frameSHA256" in sky["preparation"] or "capturedFrameSHA256" in snapshot)
    identity = None
    if captured:
        sky_frame = sky["preparation"].get("frameSHA256")
        cloud_frame_hash = snapshot.get("capturedFrameSHA256")
        if (not isinstance(sky_frame, str) or not re.fullmatch("[0-9a-f]{64}", sky_frame)
                or sky_frame != cloud_frame_hash):
            raise ValueError("Sky and cloud samples must use the same captured frame")
        identity = sky_frame
    sky_cube = load_cube(sky_file, sky["rawSHA256"], sky["resolution"][0])
    cloud_cube = load_cube(cloud_file, cloud_frame["rawSHA256"], snapshot["resolution"][0])
    linear = compose(sky_cube, cloud_cube)
    encoded = rgbe(linear)
    with output.open("xb") as stream:
        write_hdr(stream, encoded)
    return {"mode": sky["mode"], "seconds": snapshot["seconds"], "environmentSHA256": digest(output),
            "skySHA256": sky["rawSHA256"], "cloudSHA256": cloud_frame["rawSHA256"],
            "width": linear.shape[1], "height": linear.shape[0],
            **({"capturedFrameSHA256": identity} if identity is not None else {})}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("sky", "sky-report", "cloud", "cloud-report", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--frame", type=int, choices=(0, 1), required=True)
    args = parser.parse_args()
    print(json.dumps(convert(args.sky, args.sky_report, args.cloud, args.cloud_report, args.frame, args.output)))
