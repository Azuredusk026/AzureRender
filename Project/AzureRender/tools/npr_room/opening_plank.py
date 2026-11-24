"""Bind the source looping body overlay and compare it to independent Animator traces."""

import argparse
import gzip
import json
import math
from pathlib import Path
import subprocess
import tempfile

from tools.npr_room.build_story_signal_room import load
from tools.npr_room.pbr_materials import write
from tools.npr_room.reference_inventory import digest
from tools.npr_room.skinned_mesh_export import UnityScene
from tools.npr_room.story_body_morphs import BODY_RECEIPT
from tools.npr_room.verify_timer_motion import angle

IDENTITY = (0, 0, 0, 1, 0, 0, 0)
SOURCE_MOTIONS = {0, 1, 3, 5, 12, 400, 401}


def samples(report, cycle, gaze):
    frames = report["frames"]
    if (report["sampleRate"] != 60 or report["weight"] != .5 or abs(report["gaze"] - gaze) > 1e-6
            or len(frames) != 720 or {frame["motion"] for frame in frames} != SOURCE_MOTIONS):
        raise ValueError("Body overlay trace does not cover the expected opening poses")
    result = []
    for index, frame in enumerate(frames):
        if frame["frame"] != index or not math.isclose(frame["phase"] * cycle, index, abs_tol=.01):
            raise ValueError("Body overlay trace does not use the source state clock")
        joints = {}
        for row in frame["nodes"]:
            name = report["joints"][row["index"]]
            x, y, z, w = (row["rotation"][axis] for axis in "xyzw")
            norm = math.copysign(math.hypot(x, y, z, w), w)
            translation = row["translation"]
            value = (-x / norm, -y / norm, z / norm, w / norm, translation["x"], translation["y"], -translation["z"])
            if name in joints or not all(math.isfinite(channel) for channel in value):
                raise ValueError(f"Invalid body overlay sample: {name}")
            joints[name] = value
        result.append(joints)
    return result


def validate(cycle, reference, measured):
    rotation, translation = 0, 0
    for index, target in enumerate(measured):
        expected = reference[index % cycle]
        for joint in expected.keys() | target.keys():
            a, b = expected.get(joint, IDENTITY), target.get(joint, IDENTITY)
            rotation = max(rotation, angle(a[:4], b[:4]))
            translation = max(translation, math.dist(a[4:], b[4:]))
    if rotation > .1 or translation > .0005:
        raise ValueError(f"Body overlay exceeds pose tolerance: {rotation} degrees, {translation} metres")
    return {"frames": len(measured), "maxRotationDegrees": rotation, "maxTranslationMetres": translation}


def compile_plank(catalog, source, receipt_path, graph_path, baseline_path, gaze_path, models, inspector, output):
    receipt, graph = load(receipt_path), load(graph_path)
    if receipt["graphSHA256"] != digest(graph_path) or "plankSamplerSHA256" not in receipt:
        raise ValueError("Body overlay requires the locked Animator controller and sampler")
    layer = graph["layers"][4]
    machine = graph["machines"][str(layer["m_StateMachine"]["fileID"])]
    state = graph["states"][str(machine["m_DefaultState"]["fileID"])]
    if (layer["m_Name"] != "plankshake" or layer["m_BlendingMode"] != 1 or layer["m_DefaultWeight"] != .5
            or state["m_Speed"] != 1 or state["m_Transitions"] or layer["m_Mask"]["fileID"]):
        raise ValueError("Unexpected source body overlay controller")
    relative, = graph["externalAssets"][state["m_Motion"]["guid"]]
    clip = (source / relative).resolve(strict=True)
    if not clip.is_relative_to(source / "Assets/AnimationClip") or digest(clip) != receipt["assets"][relative]:
        raise ValueError("Changed source body overlay clip")
    settings = UnityScene(clip).read(7400000)["m_AnimationClipSettings"]
    if (settings["m_StartTime"] != 0 or settings["m_StopTime"] != 4.5 or settings["m_LoopTime"] != 1
            or settings["m_HasAdditiveReferencePose"]):
        raise ValueError("Unexpected source body overlay cycle")
    cycle = 270
    baseline, gaze = (json.loads(gzip.decompress(path.read_bytes())) for path in (baseline_path, gaze_path))
    if baseline["editor"] != gaze["editor"] or not math.isclose(baseline["seconds"], gaze["seconds"]):
        raise ValueError("Body overlay traces require the same source editor and clip")
    plain, with_gaze = samples(baseline, cycle, 0), samples(gaze, cycle, .7)
    reference = plain[:cycle]
    checks = {"baseline": validate(cycle, reference, plain), "withGaze": validate(cycle, reference, with_gaze)}
    names = sorted(set().union(*(frame.keys() for frame in reference)))
    targets, hashes, bound = [], {}, set()
    with tempfile.TemporaryDirectory(prefix="plank-skeleton-", dir=output.parent) as folder:
        inspection = Path(folder) / "skeleton.json"
        for part, record in sorted(load(models / BODY_RECEIPT)["parts"].items()):
            model = models / "assets/heroine" / (part + ".glb")
            if digest(model) != record["sha256"]:
                raise ValueError(f"Changed body overlay target: {part}")
            subprocess.run([str(inspector), "--inspect", str(model), str(inspection)], check=True, cwd=folder)
            inspected = load(inspection)
            paths = {row["name"]: row["path"] for row in inspected["nodes"]}
            joints = [{"index": index, "prefix": '{"path":' + json.dumps(paths[name])}
                      for index, name in enumerate(names, 1) if name in paths]
            if joints:
                prefix = {"schemaVersion": 2, "id": "room-body-overlay",
                          "skeletonFingerprint": inspected["skeletonFingerprint"]}
                targets.append({"node": "heroine-" + part, "joints": joints,
                                "prefix": json.dumps(prefix, separators=(",", ":"))[:-1]})
                hashes[part] = record["sha256"]
                bound.update(name for name in names if name in paths)
    if bound != set(names) or "face" not in hashes:
        raise ValueError("Source body overlay joints do not resolve in the native room")
    catalog["plank"] = {"clipSHA256": digest(clip), "sourceReceiptSHA256": digest(receipt_path),
                        "baselineSHA256": digest(baseline_path), "withGazeSHA256": digest(gaze_path),
                        "inspectorSHA256": digest(inspector), "checks": checks, "joints": names, "models": hashes}
    catalog["program"]["plank"] = {"sampleRate": 60, "targets": targets,
                                   "frames": [[frame.get(name, IDENTITY) for name in names] for frame in reference]}
    write(output, catalog)
    return checks


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    names = ("catalog", "source", "receipt", "graph", "baseline", "gaze", "models", "inspector", "output")
    for name in names:
        parser.add_argument("--" + name, type=Path, required=True)
    args = vars(parser.parse_args())
    paths = [args[name].resolve() for name in names]
    print(json.dumps(compile_plank(load(paths[0]), *paths[1:])))
