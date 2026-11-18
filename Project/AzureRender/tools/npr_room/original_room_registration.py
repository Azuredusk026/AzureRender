"""Freeze imported room geometry at an observed original runtime pose."""

import argparse
import json
import math
import os
from pathlib import Path
import shutil
import tempfile

from tools.build_face_sdf import parse_glb, write_glb
from tools.npr_room.catalog_storage import publish_directory
from tools.npr_room.humanoid_clip_export import bind_source_nodes, local_samples, CHANNELS
from tools.npr_room.opening_camera import engine_euler
from tools.npr_room.build_story_desk_room import converted_quaternion
from tools.npr_room.reference_inventory import digest
from tools.npr_room.room_geometry import copy_room_asset, HEROINE_PARTS


def pose_rotation(value):
    x, y, z, w = converted_quaternion(value)
    sine = 2 * (x * z + w * y)
    if abs(sine) >= 1 - 1e-12:
        # At a singular Y axis, X and Z share one degree of freedom.
        # Choose Z=0 while preserving the complete rotation matrix.
        sign = math.copysign(1, sine)
        angle = sign * math.atan2(2 * (x * y + w * z), 1 - 2 * (x*x + z*z))
        return [-math.degrees(angle), sign * 90, 0]
    return engine_euler(value)


def source_rig(record):
    transforms = record["transforms"]
    identities = {node["id"]: index for index, node in enumerate(transforms)}
    if len(identities) != len(transforms):
        raise ValueError("Repeated captured transform identity")
    return [{"name": node["path"].rsplit("/", 1)[-1],
             "parent": identities[node["parentId"]] if node["parentId"] else -1}
            for node in transforms]


def freeze_model(document, record):
    mapping = bind_source_nodes(document, source_rig(record))
    frame = {field: [node[source] for node in record["transforms"]]
             for field, source in (("positions", "position"), ("rotations", "rotation"), ("scales", "scale"))}
    for index, source in mapping.items():
        node = document["nodes"][index]
        for path, field, default, axes in CHANNELS:
            node[path] = list(local_samples([frame], source, field, axes, default)[0])
        node.pop("matrix", None)
        if "mesh" not in node:
            continue
        original_path = record["transforms"][source]["path"]
        skins = [skin for skin in record["skins"] if skin["path"] == original_path]
        if len(skins) != 1:
            raise ValueError(f"Missing unique captured renderer: {original_path}")
        original, = skins
        palette = [record["transforms"][mapping[joint]]["path"]
                   for joint in document["skins"][node["skin"]]["joints"]]
        if palette != original["bones"]:
            raise ValueError(f"Captured skin palette differs: {original_path}")
        mesh = document["meshes"][node["mesh"]]
        weights = original["blendWeights"]
        if mesh.get("name") != original["mesh"] or len(weights) != len(mesh.get("weights", [])):
            raise ValueError(f"Captured mesh or blend shape count differs: {original_path}")
        if not all(math.isfinite(value) for value in weights):
            raise ValueError(f"Nonfinite captured blend weights: {original_path}")
        node["weights"] = [value / 100 for value in weights]
    document["animations"] = []
    return {"transforms": len(mapping), "renderers": sum("mesh" in node for node in document["nodes"])}


def apply_level_pose(level, record):
    nodes = {node["id"]: node for node in level["nodes"]}
    source = {}
    for transform in record["transforms"]:
        source.setdefault(transform["path"], []).append(transform)
    matched = []
    missing = []
    for node in nodes.values():
        components = node["components"]
        components.pop("azure.animator", None)
        if "azure.script" in components:
            components["azure.script"]["data"]["enabled"] = False
        transform = components.get("azure.transform")
        if transform is None or not node.get("name"):
            continue
        ancestry = [node]
        while ancestry[-1].get("parentId"):
            parent = nodes[ancestry[-1]["parentId"]]
            if parent in ancestry:
                raise ValueError("Cyclic imported room hierarchy")
            ancestry.append(parent)
        path = "/".join(parent.get("name", "") for parent in reversed(ancestry))
        candidates = source.get(path, [])
        if len(candidates) != 1:
            missing.append(path)
            continue
        pose, = candidates
        translation = [pose["position"][key] * sign for key, sign in zip("xyz", (1, 1, -1))]
        scale = [pose["scale"][key] for key in "xyz"]
        if not all(math.isfinite(value) for value in (*translation, *scale)):
            raise ValueError(f"Nonfinite captured room pose: {path}")
        transform["data"].update(translation=translation, rotation=pose_rotation(pose["rotation"]), scale=scale)
        if "azure.camera" in components and node["name"] == record["camera"]:
            projection = record["projection"]
            components["azure.camera"]["data"].update(
                nearPlane=record["near"], farPlane=record["far"],
                fieldOfView=math.degrees(2 * math.atan(1 / projection["m11"])),
                projectionOffsetX=projection["m02"], projectionOffsetY=projection["m12"])
        matched.append(path)
    return {"matched": sorted(matched), "unmatched": sorted(missing)}


def stage(room, frame, output):
    if output.exists():
        raise FileExistsError(output)
    record = json.loads(frame.read_bytes())
    with tempfile.TemporaryDirectory(prefix="original-room-pose-", dir=output.parent) as temporary:
        project = Path(temporary) / "project"
        assets = project / "assets"
        shutil.copytree(room / "assets", assets, copy_function=copy_room_asset,
                        ignore=shutil.ignore_patterns(".azure"))
        descriptor = json.loads((room / "project.azureproject").read_bytes())
        descriptor["startupScene"] = "assets:/room.azurelevel"
        (project / "project.azureproject").write_text(json.dumps(descriptor) + "\n", encoding="utf-8")
        parts = {}
        for part in HEROINE_PARTS:
            source = room / "assets/heroine" / (part + ".glb")
            document, binary = parse_glb(source)
            proof = freeze_model(document, record)
            target = Path(temporary) / source.name
            write_glb(target, document, binary)
            os.replace(target, assets / "heroine" / source.name)
            parts[part] = {**proof, "sourceSHA256": digest(source),
                           "outputSHA256": digest(assets / "heroine" / source.name)}
        level_path = assets / "room.azurelevel"
        level = json.loads(level_path.read_bytes())
        placement = apply_level_pose(level, record)
        for node in level["nodes"]:
            ui = node["components"].get("azure.game-ui")
            if ui:
                ui["data"]["enabled"] = False
        level_path.write_text(json.dumps(level) + "\n", encoding="utf-8")
        receipt = {"schemaVersion": 1, "frameSHA256": digest(frame), "frame": record["frame"],
                   "weather": record["weather"], "parts": parts, "placement": placement,
                   "levelSHA256": digest(level_path), "scope": "captured geometry pose with gameplay and UI disabled"}
        (project / "original-pose-receipt.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
        publish_directory(project, output)
    return receipt


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("room", "frame", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    arguments = parser.parse_args()
    result = stage(arguments.room.resolve(), arguments.frame.resolve(), arguments.output.resolve())
    print(json.dumps({"weather": result["weather"], "parts": len(result["parts"]),
                      "matchedRoomTransforms": len(result["placement"]["matched"]),
                      "unmatchedRoomTransforms": len(result["placement"]["unmatched"])}))
