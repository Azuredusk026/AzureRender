"""Compare fixed-step native camera transforms with the original Unity controller."""

import argparse
import json
import math
from pathlib import Path
import subprocess

from test_story_player import slot
from tools.npr_room.build_story_desk_room import converted_quaternion
from tools.npr_room.build_story_signal_room import load
from tools.npr_room.opening_camera import compile_track, source_program
from tools.npr_room.pbr_materials import write
from tools.npr_room.reference_inventory import digest
from tools.npr_room.story_chapters import lua_value
from tools.npr_room.verify_timer_motion import angle


def native_quaternion(euler):
    x, y, z = (math.radians(coordinate * sign) / 2 for coordinate, sign in zip(euler, (-1, 1, -1), strict=True))
    sx, sy, sz = map(math.sin, (x, y, z))
    cx, cy, cz = map(math.cos, (x, y, z))
    return (sx * cy * cz + cx * sy * sz, cx * sy * cz - sx * cy * sz,
            cx * cy * sz + sx * sy * cz, cx * cy * cz - sx * sy * sz)


def pose_error(translation, rotation, wanted, origin):
    position = wanted["position"]
    expected = (position["x"], position["y"], -position["z"])
    world = [a + b for a, b in zip(translation, origin, strict=True)]
    return math.dist(expected, world), angle(native_quaternion(rotation), converted_quaternion(wanted["rotation"]))


def native_replay(player, room, resources, trace_path, output):
    original = load(room / "assets/room.azurelevel")
    trace = load(trace_path)
    track = compile_track(original, trace)
    source_nodes = {node["id"]: node for node in original["nodes"]}
    node, origin = source_nodes[track["node"]], [0, 0, 0]
    camera_chain = []
    while node:
        camera_chain.append({**node, "resourceId": "",
                             "components": {kind: value for kind, value in node["components"].items()
                                            if kind in ("azure.transform", "azure.camera")}})
        node = source_nodes.get(node.get("parentId"))
        if node:
            parent = node["components"]["azure.transform"]["data"]["translation"]
            origin = [a + b for a, b in zip(origin, parent, strict=True)]
    output.mkdir()
    assets = output / "assets"
    assets.mkdir()
    tag = "opening-camera-proof"
    level = "assets:/" + tag + ".azurelevel"
    project = output / "project.azureproject"
    descriptor = load(room / "project.azureproject")
    descriptor["startupScene"] = level
    write(project, descriptor)
    helper = {"id": tag, "components": {"azure.script": {"type": "azure.script", "version": 1,
                                                          "data": {"asset": "assets:/" + tag + ".lua"}}}}
    write(assets / (tag + ".azurelevel"), {"schemaVersion": original["schemaVersion"], "id": tag, "name": tag,
                                         "nodes": [*reversed(camera_chain), helper], "resources": []})
    script = source_program() + f"""
local track = {lua_value(track)}
local camera, actor
local frame = -1
local samples = {{}}
local component = "azure.transform"
function init()
    actor = assert(self:find(track.node))
    camera = new_story_camera(self, track)
end
function update(dt)
    frame = frame + 1
    samples[#samples + 1] = {{frame, actor:get(component, "translation"), actor:get(component, "rotation")}}
    if frame == 1 then
        camera:effect({{command = "DirectionGameStartCamera"}})
    elseif frame == 152 then
        camera:finish()
    else
        camera:update(dt)
    end
    if frame == 155 then self:write_save("{tag}", {{1, samples}}) end
end
"""
    (assets / (tag + ".lua")).write_text(script, encoding="utf-8", newline="\n")
    actions, runtime = output / "input.json", output / "runtime.json"
    write(actions, {"schemaVersion": 1, "actions": [
        {"frame": 0, "action": "wait-level", "reference": level, "timeoutFrames": 200}]})
    profile = output / "profile"
    command = [str(player), "--project", str(project), "--resource-root", str(resources), "--save-root", str(profile),
               "--game-actions", str(actions), "--runtime-report", str(runtime), "--fixed-frame-step",
               "--smoke-frames", "175", "--width", "640", "--height", "360"]
    with (output / "stdout.log").open("w", encoding="utf-8") as out, (output / "stderr.log").open("w", encoding="utf-8") as err:
        result = subprocess.run(command, cwd=output, stdout=out, stderr=err, timeout=90)
    assert result.returncode == 0, (output / "stderr.log").read_text(encoding="utf-8")[-4500:]
    assert "Allocator after unload: buffers=0 images=0" in (output / "stdout.log").read_text(encoding="utf-8")
    observed = load(runtime)
    assert not observed["scriptErrors"] and not observed["presentationErrors"], (observed["scriptErrors"], observed["presentationErrors"])
    schema, samples = slot(profile, tag)
    assert schema == 1 and [sample[0] for sample in samples] == list(range(156))
    errors = []
    for frame, translation, rotation in samples:
        wanted = (trace["initial"] if frame < 2 else trace["samples"][frame - 2]
                  if frame <= 152 else trace["disabled"])
        error = pose_error(translation, rotation, wanted, origin)
        assert error[0] <= 1e-6 and error[1] <= .002, (frame, error, translation, rotation)
        errors.append(error)
    report = {"frames": len(samples), "sourceClipFrames": len(trace["samples"]), "sourceTraceSHA256": digest(trace_path),
              "playerSHA256": digest(player), "maxPositionMeters": max(error[0] for error in errors),
              "maxRotationDegrees": max(error[1] for error in errors), "cleanAllocatorExits": 1}
    write(output / "camera-frames.json", samples)
    write(output / "opening-camera-replay.json", report)
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    names = ("player", "room", "resource-root", "source-trace", "output")
    for name in names:
        parser.add_argument("--" + name, type=Path, required=True)
    arguments = vars(parser.parse_args())
    print(json.dumps(native_replay(*(arguments[name.replace("-", "_")].resolve() for name in names))))
