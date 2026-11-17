"""Restore the source face bindings carried by the opening body clips."""

import argparse
import json
import math
import os
from pathlib import Path
import re
import shutil
import tempfile
from zlib import crc32

from tools.build_face_sdf import parse_glb, write_glb
from tools.npr_room.build_story_signal_room import load
from tools.npr_room.catalog_storage import publish_directory
from tools.npr_room.pbr_materials import write
from tools.npr_room.reference_inventory import digest
from tools.npr_room.skinned_mesh_export import UnityScene, add_morph_sampler, float32, read_morph_curves

FACE = "assets/heroine/face.glb"
BODY_RECEIPT = "tutorial-camera-body-receipt.json"
MORPH_RECEIPT = "opening-body-morph-receipt.json"
TARGET_PATH = "Character/Face"
PREFIX = "blendShape."
DORMANT = re.compile(r"path_0x([0-9A-Fa-f]{8})_[A-Za-z0-9]+")



def direct_bindings(clip, names):
    result, dormant = [], []
    target_hash = crc32(TARGET_PATH.encode())
    names_by_hash = {crc32(name.encode()): name for name in names}
    for record in clip["m_FloatCurves"]:
        attribute = record["attribute"]
        if not attribute.startswith(PREFIX):
            continue
        name = attribute.removeprefix(PREFIX)
        if name in names:
            result.append(record)
            continue
        marker = DORMANT.fullmatch(name)
        if not marker or record["path"] != TARGET_PATH or record["classID"] != 137:
            raise ValueError(f"Unrecognized face curve: {attribute}")
        target = int(marker[1], 16)
        keys = record["curve"]["m_Curve"]
        present = any(row["path"] == target_hash and row["attribute"] == target and row["typeID"] == 137
                      for row in clip["m_ClipBindingConstant"]["genericBindings"])
        if (target != crc32(name.encode()) or target in names_by_hash or not present or not keys
                or any(key[field] != 0 for key in keys for field in ("value", "inSlope", "outSlope"))):
            raise ValueError(f"Unresolved active face binding: {attribute}")
        dormant.append(marker[1].lower())
    return result, dormant


def append_weights(document, binary, body, clip, target):
    if body["name"] != clip["m_Name"] or any(channel["target"]["path"] == "weights" for channel in body["channels"]):
        raise ValueError(f"Unsupported body track: {body['name']}")
    mesh = document["meshes"][document["nodes"][target]["mesh"]]
    names = mesh["extras"]["targetNames"]
    defaults = document["nodes"][target].get("weights", mesh["weights"])
    bindings, dormant = direct_bindings(clip, names)
    if not bindings:
        return {"bindings": 0, "samples": 0, **({"unboundDefaults": dormant} if dormant else {})}
    inputs = [document["accessors"][sampler["input"]] for sampler in body["samplers"]]
    if not inputs or any(row["min"] != [0] for row in inputs):
        raise ValueError("Expected body time starting at zero")
    end = float32(max(row["max"][0] for row in inputs))
    settings = clip["m_AnimationClipSettings"]
    start, stop, rate = settings["m_StartTime"], settings["m_StopTime"], clip["m_SampleRate"]
    if not all(map(math.isfinite, (start, stop, rate, end))) or start >= stop or rate <= 0 or stop - start > end + 1e-6:
        raise ValueError(f"Shape curve exceeds the body clip clock: {body['name']}")
    curves = read_morph_curves(bindings, TARGET_PATH, names, start)
    times = {0, end}
    for frame in range(1, math.ceil(end * rate)):
        time = float32(frame / rate)
        if time < end:
            times.add(time)
    for key_times, _, _ in curves.values():
        times.update(time for time in key_times if 0 < time < end)
    body["channels"].append({"sampler": len(body["samplers"]), "target": {"node": target, "path": "weights"}})
    body["samplers"].append(add_morph_sampler(document, binary, names, defaults, curves, sorted(times)))
    return {"bindings": len(curves), "samples": len(times), **({"unboundDefaults": dormant} if dormant else {})}


def bake(source, room, models, catalog_path, source_receipt, output):
    parts = load(models / BODY_RECEIPT)
    if parts["parts"]["face"]["sha256"] != digest(models / FACE):
        raise ValueError("Expected the seven-piece sampled body")
    lookup = {**load(room / "opening-body-receipt.json")["motions"], **parts["motions"]}
    catalog = load(catalog_path)["clips"] | load(source_receipt)["cameraClips"]
    if {Path(relative).stem for relative in catalog} != lookup.keys():
        raise ValueError("Source morphs must cover all opening body clips")
    document, payload = parse_glb(models / FACE)
    target, = [index for index, node in enumerate(document["nodes"]) if node["name"] == "Face" and "mesh" in node]
    binary, receipts = bytearray(payload), {}
    for relative, original in sorted(catalog.items()):
        name = Path(relative).stem
        clip = lookup[name]
        file = (source / relative).resolve(strict=True)
        if (not file.is_relative_to(source / "Assets/AnimationClip")
                or digest(file) != original["sha256"] or clip["sourceSHA256"] != original["sha256"]):
            raise ValueError(f"Changed original body curve: {name}")
        animation = document["animations"][clip["clipIndex"]]
        if animation["name"] != name:
            raise ValueError(f"Changed opening body clip index: {name}")
        data = UnityScene(file).read(7400000)
        receipts[name] = {"clipIndex": clip["clipIndex"], "relative": relative, "sha256": original["sha256"],
                          **append_weights(document, binary, animation, data, target)}
    document["buffers"][0]["byteLength"] = len(binary)
    with tempfile.TemporaryDirectory(prefix="opening-body-morphs-", dir=output.parent) as parent:
        stage = Path(parent) / "models"
        (stage / FACE).parent.mkdir(parents=True)
        for part, expected in parts["parts"].items():
            if part == "face":
                continue
            name = "assets/heroine/" + part + ".glb"
            if digest(models / name) != expected["sha256"]:
                raise ValueError(f"Changed sampled body part: {part}")
            os.link(models / name, stage / name)
        write_glb(stage / FACE, document, bytes(binary))
        result = {"schemaVersion": 1, "sourceModelsSHA256": digest(models / BODY_RECEIPT),
                  "inputGlbSHA256": digest(models / FACE), "faceGlbSHA256": digest(stage / FACE),
                  "sourceCatalogSHA256": digest(catalog_path), "targetCount": 57, "clips": receipts}
        parts["parts"]["face"]["sha256"] = result["faceGlbSHA256"]
        write(stage / BODY_RECEIPT, parts)
        write(stage / MORPH_RECEIPT, result)
        publish_directory(stage, output)
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    names = ("source", "room", "models", "catalog", "source-receipt", "output")
    for name in names:
        parser.add_argument("--" + name, type=Path, required=True)
    args = vars(parser.parse_args())
    print(json.dumps(bake(*(args[name.replace("-", "_")].resolve() for name in names))))
