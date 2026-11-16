"""Compare all baked timer pose samples to the original Avatar evaluation."""

import argparse
import gzip
import json
import math
from pathlib import Path
from struct import unpack_from

from tools.build_face_sdf import parse_glb
from tools.npr_room.humanoid_clip_export import CHANNELS, bind_source_nodes
from tools.npr_room.pbr_materials import write
from tools.npr_room.reference_inventory import digest


def accessor(document, binary, index, width):
    record = document["accessors"][index]
    view = document["bufferViews"][record["bufferView"]]
    start = view.get("byteOffset", 0) + record.get("byteOffset", 0)
    return [unpack_from("<" + "f" * width, binary, start + 4 * width * row) for row in range(record["count"])]


def angle(first, second):
    norm_first, norm_second = math.hypot(*first), math.hypot(*second)
    first = [value / norm_first for value in first]
    second = [value / norm_second for value in second]
    if sum(x * y for x, y in zip(first, second)) < 0:
        second = [-value for value in second]
    return math.degrees(4 * math.atan2(math.dist(first, second), math.hypot(*(x + y for x, y in zip(first, second)))))


def verify(room, sample_root, rig_path):
    receipt_path = room / "timer-motion-receipt.json"
    receipt = json.loads(receipt_path.read_bytes())
    if digest(rig_path) != receipt["rigSHA256"]:
        raise ValueError("The timer pose comparison requires the original rig")
    rig = json.loads(rig_path.read_bytes())["nodes"]
    root = sample_root.resolve(strict=True)
    clips = {}
    for batch in receipt["sampleBatches"]:
        path = (root / batch["file"]).resolve(strict=True)
        if path.parent != root or digest(path) != batch["sha256"]:
            raise ValueError(f"Timer pose sample identity differs: {batch['file']}")
        for clip in json.loads(gzip.decompress(path.read_bytes()))["clips"]:
            if clip["name"] in clips:
                raise ValueError(f"Repeated sampled motion: {clip['name']}")
            clips[clip["name"]] = clip["frames"]

    result = {"parts": len(receipt["parts"]), "motions": len(clips), "comparedLocalTransforms": 0,
              "rotationMaxDegrees": 0, "translationMax": 0, "scaleMax": 0, "timeMaxSeconds": 0,
              "assemblySHA256": digest(receipt_path)}
    for part, item in receipt["parts"].items():
        model = room / "assets/heroine" / (part + ".glb")
        assert digest(model) == item["sha256"], model
        document, binary = parse_glb(model)
        mapping = bind_source_nodes(document, rig)
        for motion in receipt["motions"].values():
            animation = document["animations"][motion["clipIndex"]]
            assert animation["name"] == motion["name"]
            frames = clips[motion["name"]]
            tracks = {}
            for channel in animation["channels"]:
                target = channel["target"]
                sampler = animation["samplers"][channel["sampler"]]
                length = 4 if target["path"] == "rotation" else 3
                times = accessor(document, binary, sampler["input"], 1)
                result["timeMaxSeconds"] = max(result["timeMaxSeconds"],
                    *(abs(time[0] - frame["time"]) for time, frame in zip(times, frames, strict=True)))
                tracks[target["node"], target["path"]] = accessor(document, binary, sampler["output"], length)
            for node, original in mapping.items():
                base = document["nodes"][node]
                for path, field, default, axes in CHANNELS:
                    values = tracks.get((node, path))
                    measurement = {"rotation": "rotationMaxDegrees", "translation": "translationMax", "scale": "scaleMax"}
                    compare = angle if path == "rotation" else math.dist
                    for index, frame in enumerate(frames):
                        source = frame[field][original]
                        expected = [source[key] * sign for key, sign in zip("xyzw", axes)]
                        actual = values[index] if values is not None else base.get(path, default)
                        key = measurement[path]
                        result[key] = max(result[key], compare(actual, expected))
                result["comparedLocalTransforms"] += len(frames)
    assert result["parts"] == 7 and result["comparedLocalTransforms"] > 0
    assert result["rotationMaxDegrees"] < .1 and result["translationMax"] < .0005
    assert result["scaleMax"] < 1e-6 and result["timeMaxSeconds"] < 1e-6
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("room", "sample-root", "rig", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError(args.output)
    proof = verify(args.room, args.sample_root, args.rig)
    write(args.output, proof)
    print(json.dumps(proof))
