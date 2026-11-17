"""Bind the sampled Windows opening camera to the normal room presentation."""

import math
from pathlib import Path

from tools.npr_room.build_purchase_room import system_sound
from tools.npr_room.build_story_desk_room import euler_xyz
from tools.npr_room.build_story_signal_room import EXAMPLE, load
from tools.npr_room.reference_inventory import digest
from tools.npr_room.story_chapters import lua_value

ASSET = "story-camera.lua"
CUE = "opening-camera-sound"


def engine_euler(quaternion):
    x, y, z = euler_xyz(quaternion)
    # LinearMath uses the negative right-handed angle for its X and Z matrices.
    return [-x, y, -z]


def compile_track(level, trace):
    nodes = {node["id"]: node for node in level["nodes"]}
    camera, = [node for node in nodes.values() if "azure.camera" in node["components"]]
    initial, anchor = trace["initial"], trace["anchor"]
    rate, seconds = trace["sampleRate"], trace["seconds"]
    last = round(rate * seconds)
    poses = trace["samples"]
    if (trace["state"] != "anim_startcamera_pc" or rate != 60 or seconds != 2
            or len(poses) <= last or trace["rebind"] != trace["disabled"]
            or trace["disabled"]["rotation"] != initial["rotation"]):
        raise ValueError("Expected the sampled Windows camera and its rebind")
    displacement = anchor["position"]
    if any(pose["position"] != displacement for pose in (*poses, trace["disabled"])):
        raise ValueError("The source camera does not stay on its anchor")
    if any(pose["rotation"] != poses[last]["rotation"] for pose in poses[last:]):
        raise ValueError("The source clip does not hold its last key")

    def position(pose):
        value = pose["position"]
        return [value["x"], value["y"], -value["z"]]

    def transform(node):
        return node["components"]["azure.transform"]["data"]

    origin = [0, 0, 0]
    parent = nodes.get(camera.get("parentId"))
    while parent:
        data = transform(parent)
        if data["rotation"] != [0, 0, 0] or data["scale"] != [1, 1, 1]:
            raise ValueError("Expected translation-only parents for the source camera")
        origin = [a + b for a, b in zip(origin, data["translation"], strict=True)]
        parent = nodes.get(parent.get("parentId"))

    def local(pose):
        return [a - b for a, b in zip(position(pose), origin, strict=True)]

    data = transform(camera)
    if (math.dist(data["translation"], local(initial)) > 1e-5
            or max(abs((a - b + 180) % 360 - 180) for a, b in zip(
                data["rotation"], engine_euler(initial["rotation"]), strict=True)) > 1e-3):
        raise ValueError("The native room does not match the source camera placement")
    return {"node": camera["id"], "sampleRate": rate, "translation": local(anchor),
            "rotations": [engine_euler(pose["rotation"]) for pose in poses[:last + 1]],
            "finish": engine_euler(trace["disabled"]["rotation"])}


def source_program():
    return (EXAMPLE / ASSET).read_text(encoding="utf-8").rstrip()


def bind(script, track):
    program = {
        "function new_story_room(": source_program() + "\n\nlocal story_camera_track = "
        + lua_value(track) + "\n\nfunction new_story_room(",
        "level, books, body, desk)": "level, books, body, desk, camera)",
        "        noise:update(dt)": "        noise:update(dt)\n        camera:update(dt)",
        "        noise:effect(row, delay)": "        camera:effect(row)\n        noise:effect(row, delay)",
        "        noise:finish()": "        noise:finish()\n        camera:finish()",
        "new_story_desk(self, story_desk_poses))":
            "new_story_desk(self, story_desk_poses), new_story_camera(self, story_camera_track))",
    }
    for marker, replacement in program.items():
        if script.count(marker) != 1:
            raise ValueError(f"Expected one normal camera integration marker: {marker}")
        script = script.replace(marker, replacement)
    return script


def attach_audio(level, sound):
    if any(node["id"] == CUE for node in level["nodes"]):
        raise ValueError("The room already has an opening camera cue")
    level["nodes"].append({"id": CUE, "components": {"azure.audio-source": {
        "type": "azure.audio-source", "version": 1,
        "data": {"asset": "assets:/" + sound["asset"], "volume": .5}}}})


def project(room, program, trace_path, source=None):
    level_path = room / "assets/room.azurelevel"
    trace = load(trace_path)
    track = compile_track(load(level_path), trace)
    receipt = {"schemaVersion": 1, "levelSourceSHA256": digest(level_path),
               "traceSHA256": digest(trace_path), "track": track}
    if source:
        receipt["cue"] = system_sound(source, "GameStartCameraNoise")
        track["cueNode"] = CUE
    return bind(program, track), receipt
