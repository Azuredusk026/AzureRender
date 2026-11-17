"""Compile opening facial and mouth clips into sparse native expression inputs."""

import argparse
import json
import math
from pathlib import Path
import subprocess
import tempfile

from tools.build_face_sdf import parse_glb
from tools.npr_room.build_story_signal_room import EXAMPLE, load
from tools.npr_room.opening_camera import use_presentation
from tools.npr_room.opening_look_basis import source_overlay
from tools.npr_room.pbr_materials import write
from tools.npr_room.reference_inventory import digest
from tools.npr_room.skinned_mesh_export import UnityScene, read_morph_curves
from tools.npr_room.story_body_morphs import BODY_RECEIPT, FACE, MORPH_RECEIPT
from tools.npr_room.story_chapters import lua_value

ACTOR = "heroine-face"


def curve_values(curve):
    times, values, tangents = curve
    if (all(value == values[0] for value in values)
            and all(slope == 0 for pair in tangents for slope in pair)):
        return values[0]
    steps = [math.isinf(left[1]) or math.isinf(right[0]) for left, right in zip(tangents, tangents[1:])]
    incoming, outgoing = ([slope[axis] if math.isfinite(slope[axis]) else 0 for slope in tangents]
                         for axis in range(2))
    return [times, values, incoming, outgoing, steps]


def tracks(source, receipt, names):
    by_name = {Path(relative).stem: (source / relative, sha) for relative, sha in receipt["assets"].items()
               if Path(relative).suffix == ".anim"}
    slots = {name: index for index, name in enumerate(names, 1)}
    def read(name):
        path, sha = by_name[name]
        path = path.resolve(strict=True)
        if not path.is_relative_to(source / "Assets/AnimationClip") or digest(path) != sha:
            raise ValueError(f"Changed source face clip: {name}")
        clip = UnityScene(path).read(7400000)
        settings = clip["m_AnimationClipSettings"]
        start, stop = settings["m_StartTime"], settings["m_StopTime"]
        if clip["m_Name"] != name or not math.isfinite(start + stop) or stop < start:
            raise ValueError(f"Invalid source face clip: {name}")
        keys = read_morph_curves(clip["m_FloatCurves"], "Character/Face", names, start)
        return {"seconds": stop - start, "loop": bool(settings["m_LoopTime"]),
                "curves": [[slots[target], curve_values(keys[target])] for target in names if target in keys]}
    facial = {number: read(row["name"]) for number, row in receipt["facialRoutes"].items()}
    return facial, read("anim_face_talk")


def compile_catalog(source, receipt_path, graph_file, models, skeleton_file):
    receipt = load(receipt_path)
    graph = load(graph_file)
    if digest(graph_file) != receipt["graphSHA256"]:
        raise ValueError("Facial routes require the same source controller")
    mesh_file = models / FACE
    model_receipt = load(models / MORPH_RECEIPT)
    if model_receipt["faceGlbSHA256"] != digest(mesh_file) or model_receipt["inputGlbSHA256"] != receipt["meshSHA256"]:
        raise ValueError("Facial layers require the sampled body binding")
    gltf, _ = parse_glb(mesh_file)
    mesh, = gltf["meshes"]
    names, defaults = mesh["extras"]["targetNames"], mesh["weights"]
    skeleton = load(skeleton_file)
    face, = [node for node in skeleton["nodes"] if node["name"] == "Face"]
    if gltf["nodes"][face["index"]]["name"] != "Face" or len(names) != len(defaults) or len(names) != 57:
        raise ValueError("Expected the native face target layout")
    if not skeleton["skeletonFingerprint"].startswith("expression-v1-fnv64:"):
        raise ValueError("Expected a native expression skeleton inspection")

    face_blends = {route["blend"] for route in receipt["facialRoutes"].values()}
    blend, = face_blends
    layers = {layer["m_Name"]: layer for layer in graph["layers"]}
    talk_layer = layers["Talk"]
    machine = graph["machines"][str(talk_layer["m_StateMachine"]["fileID"])]
    states = {row["m_State"]["fileID"]: graph["states"][str(row["m_State"]["fileID"])] for row in machine["m_ChildStates"]}
    times = {}
    for state in states.values():
        pointer, = state["m_Transitions"]
        transition = graph["stateTransitions"][str(pointer["fileID"])]
        condition, = transition["m_Conditions"]
        destination = states[transition["m_DstState"]["fileID"]]["m_Name"]
        if (condition["m_ConditionEvent"] != "Enable_Talk" or condition["m_ConditionMode"] not in (1, 2)
                or not transition["m_HasFixedDuration"] or transition["m_HasExitTime"]
                or transition["m_InterruptionSource"] or transition["m_TransitionOffset"]):
            raise ValueError("Unexpected source talk transition")
        times[destination] = transition["m_TransitionDuration"]
    facial, voiced = tracks(source, receipt, names)
    preset = {"schemaVersion": 1, "id": "opening-story-expression",
              "skeletonFingerprint": skeleton["skeletonFingerprint"]}
    return {"schemaVersion": 1, "meshSHA256": digest(mesh_file), "sourceSHA256": digest(receipt_path),
            "skeletonSHA256": digest(skeleton_file), "targets": names, "chaptersSHA256": receipt["chaptersSHA256"],
            "program": {"node": ACTOR, "defaults": defaults, "keys": [json.dumps(name) for name in names],
                        "prefix": json.dumps(preset, separators=(",", ":"))[:-1] + ',"morphs":[',
                        "nodePath": json.dumps(face["path"]), "facial": facial, "faceBlend": blend,
                        "silent": facial["0"], "voiced": voiced, "speakIn": times["anim_face_talk"], "speakOut": times["_None"]}}


def compile_gaze(catalog, source, models, basis_path, receipt, inspector, output_parent):
    calibration = load(basis_path)
    clip_sha, seconds, curves, eyes = source_overlay(source, receipt)
    if clip_sha != calibration["clipSHA256"]:
        raise ValueError("The gaze basis and its source clip differ")
    joints = calibration["jointBasis"]
    model_parts = load(models / BODY_RECEIPT)["parts"]
    parts = []
    model_hashes = {}
    with tempfile.TemporaryDirectory(prefix="gaze-skeleton-", dir=output_parent) as temp:
        inspection_file = Path(temp) / "skeleton.json"
        for part, record in sorted(model_parts.items()):
            model = models / "assets/heroine" / (part + ".glb")
            if digest(model) != record["sha256"]:
                raise ValueError(f"Changed gaze target: {part}")
            document, _ = parse_glb(model)
            nodes = {row["name"] for row in document["nodes"]}
            present = [(idx, row) for idx, row in enumerate(joints, 1) if row["joint"] in nodes]
            if not present:
                continue
            subprocess.run([str(inspector), "--inspect", str(model), str(inspection_file)], check=True, cwd=temp)
            inspected = load(inspection_file)
            node_paths = {row["name"]: row["path"] for row in inspected["nodes"] if row["name"] in nodes}
            prefix = {"schemaVersion": 2, "id": "opening-story-gaze", "skeletonFingerprint": inspected["skeletonFingerprint"]}
            bound = []
            for index, row in present:
                data = {"path": node_paths[row["joint"]], **{key: row[key] for key in ("zero", "axes", "rates")}}
                bound.append({"index": index, "prefix": json.dumps(data, separators=(",", ":"))[:-1] + ',"offset":['})
            parts.append({"node": "heroine-" + part, "joints": bound,
                          "prefix": json.dumps(prefix, separators=(",", ":"))[:-1] + ',"humanoidRotations":['})
            model_hashes[part] = record["sha256"]
    if not any(part["node"] == ACTOR for part in parts):
        raise ValueError("The source gaze must bind the native face")
    slots = {name: idx for idx, name in enumerate(catalog["targets"], 1)}
    catalog["look"] = {"basisSHA256": digest(basis_path), "receiptSHA256": digest(receipt),
                       "inspectorSHA256": digest(inspector), "models": model_hashes, "clipSHA256": clip_sha}
    catalog["program"]["look"] = {
        "seconds": seconds, "targets": parts,
        "jointCurves": [[curve_values(curves[name]) for name in row["muscles"]] for row in joints],
        "eyes": [[slots[name], curve_values(curve)] for name, curve in eyes.items()]
    }
    return catalog


def source_program():
    return (EXAMPLE / "story-face.lua").read_text(encoding="utf-8").rstrip()


def bind(script, catalog, models, chapters):
    if catalog["meshSHA256"] != digest(models / FACE) or catalog["chaptersSHA256"] != digest(chapters):
        raise ValueError("Opening expression inputs differ from the staged room")
    for part, expected in catalog.get("look", {}).get("models", {}).items():
        if digest(models / "assets/heroine" / (part + ".glb")) != expected:
            raise ValueError(f"Opening gaze target differs from the staged room: {part}")
    camera = "new_story_camera(self, story_camera_track))"
    desk = "new_story_desk(self, story_desk_poses))"
    factory = camera if camera in script else desk
    if script.count(factory) != 1:
        raise ValueError("Expected one normal facial integration marker")
    tail = "" if factory == camera else ", nil"
    script = script.replace(factory, factory[:-1] + tail + ", new_story_face(self, story_face_tracks))")
    if "    function dialogue:speaking()" not in script:
        anchor = "    function dialogue:update(dt)"
        if script.count(anchor) != 1:
            raise ValueError("Expected a normal voice playback helper")
        script = script.replace(anchor, """    function dialogue:speaking()
        return line and line.moveMouth and audio:audio_playing()
    end

""" + anchor)
    return use_presentation(script, source_program() + "\n\nlocal story_face_tracks = " + lua_value(catalog["program"]))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    names = ("source", "source-receipt", "graph", "models", "skeleton", "output")
    for name in names:
        parser.add_argument("--" + name, type=Path, required=True)
    options = ("look-basis", "look-source-receipt", "expression-inspector")
    for name in options:
        parser.add_argument("--" + name, type=Path)
    args = vars(parser.parse_args())
    paths = [args[name.replace("-", "_")].resolve() for name in names]
    catalog = compile_catalog(*paths[:-1])
    basis, receipt, inspector = [args[name.replace("-", "_")] for name in options]
    if any((basis, receipt, inspector)):
        if not all((basis, receipt, inspector)):
            parser.error("Gaze compilation requires the basis, locked source and expression inspector")
        compile_gaze(catalog, paths[0], paths[3], basis.resolve(), receipt.resolve(), inspector.resolve(), paths[-1].parent)
    write(paths[-1], catalog)
    print(json.dumps({"faceEntries": len(catalog["program"]["facial"]), "targets": len(catalog["targets"])}))
