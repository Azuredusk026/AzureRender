"""Append sampled Recovery Humanoid motion to an original skinned GLB."""

import argparse
from collections import defaultdict
import json
import math
from pathlib import Path
from struct import pack

from tools.build_face_sdf import parse_glb, write_glb
from tools.npr_room.skinned_mesh_export import add_accessor


CHANNELS = (
    ("translation", "positions", (0, 0, 0), (1, 1, -1)),
    ("rotation", "rotations", (0, 0, 0, 1), (-1, -1, 1, 1)),
    ("scale", "scales", (1, 1, 1), (1, 1, 1)),
)


def bind_source_nodes(document, rig):
    by_parent = defaultdict(list)
    for index, node in enumerate(rig):
        by_parent[node["parent"], node["name"]].append(index)
    mapping = {}

    def bind(index, source_parent):
        node = document["nodes"][index]
        matches = by_parent[source_parent, node["name"]]
        if len(matches) != 1:
            raise ValueError(f"Ambiguous or missing source transform: {node['name']} under {source_parent}")
        source, = matches
        if index in mapping:
            raise ValueError(f"Repeated GLB transform: {index}")
        mapping[index] = source
        for child in node.get("children", ()):
            bind(child, source)

    for root in document["scenes"][document.get("scene", 0)]["nodes"]:
        bind(root, -1)
    if len(mapping) != len(document["nodes"]):
        raise ValueError("GLB has nodes outside the bound character hierarchy")
    return mapping


def local_samples(frames, origin, field, axes, default):
    previous = tuple(default)
    output = []
    for frame in frames:
        raw = frame[field][origin]
        value = tuple(raw[key] * sign for key, sign in zip("xyzw", axes))
        if not all(math.isfinite(component) for component in value):
            raise ValueError("Nonfinite source transform")
        if field == "rotations":
            length = math.hypot(*value)
            if length <= 0:
                raise ValueError("Invalid source quaternion")
            sign = -1 if sum(a * b for a, b in zip(value, previous)) < 0 else 1
            value = tuple(sign * component / length for component in value)
        output.append(value)
        previous = value
    return output


def append_humanoid_clip(document, binary, mapping, clip, *, require_motion=True):
    if any(animation["name"] == clip["name"] for animation in document.get("animations", ())):
        raise ValueError(f"Motion name already exists: {clip['name']}")
    frames = clip["frames"]
    times = [frame["time"] for frame in frames]
    if len(times) < 2 or times[0] != 0 or not all(math.isfinite(time) for time in times):
        raise ValueError("Clip requires finite samples starting at zero")
    if any(last >= current for last, current in zip(times, times[1:])):
        raise ValueError("Clip sample times must increase")
    time_access = add_accessor(document, binary, pack(f"<{len(times)}f", *times), len(times), "SCALAR", 5126)
    document["accessors"][time_access].update(min=[times[0]], max=[times[-1]])

    animation = {"name": clip["name"], "samplers": [], "channels": []}
    for node, origin in mapping.items():
        for path, field, default, axes in CHANNELS:
            reference = document["nodes"][node].get(path, default)
            values = local_samples(frames, origin, field, axes, reference)
            if all(max(abs(a - b) for a, b in zip(value, reference)) <= 1e-7 for value in values):
                continue
            width = len(axes)
            data = b"".join(pack(f"<{width}f", *value) for value in values)
            output = add_accessor(document, binary, data, len(times), f"VEC{width}", 5126)
            animation["channels"].append({"sampler": len(animation["samplers"]), "target": {"node": node, "path": path}})
            animation["samplers"].append({"input": time_access, "output": output, "interpolation": "LINEAR"})
    if not animation["channels"] and require_motion:
        raise ValueError(f"Motion does not animate the GLB hierarchy: {clip['name']}")
    document.setdefault("animations", []).append(animation)
    return len(animation["channels"])


def export_model(source, output, rig, clips):
    document, data = parse_glb(source)
    mapping = bind_source_nodes(document, rig)
    binary = bytearray(data)
    counts = {clip["name"]: append_humanoid_clip(document, binary, mapping, clip) for clip in clips}
    document["buffers"][0]["byteLength"] = len(binary)
    write_glb(output, document, bytes(binary))
    return counts


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("rig", "samples", "input", "output"):
        parser.add_argument(f"--{name}", type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError(args.output)
    rig = json.loads(args.rig.read_bytes())["nodes"]
    clips = json.loads(args.samples.read_bytes())["clips"]
    print(json.dumps(export_model(args.input, args.output, rig, clips)))


if __name__ == "__main__":
    main()
