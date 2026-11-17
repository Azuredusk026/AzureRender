"""Replay the source face requests on a fixed clock through the native expression field."""

import argparse
import gzip
import json
from pathlib import Path
import subprocess

from test_story_body_morph_player import fixture
from test_story_player import slot
from tools.npr_room.build_story_signal_room import load
from tools.npr_room.opening_face_layers import source_program
from tools.npr_room.pbr_materials import write
from tools.npr_room.reference_inventory import digest
from tools.npr_room.story_body_morphs import FACE
from tools.npr_room.story_chapters import lua_value


def script(catalog, frames):
    schedule, previous = {}, None
    for row in frames:
        value = [row["facialRequested"], row["talking"]]
        if value != previous:
            schedule[str(row["number"])] = value
            previous = value
    return source_program() + f"""
local config = {lua_value(catalog["program"])}
local keys = {lua_value(catalog["targets"])}
local schedule = {lua_value(schedule)}
local stop = {len(frames)}
local frame = -1
local actor, face
local chunk, index = {{}}, 0
function init()
    actor = assert(self:find(config.node))
    face = new_story_face(self, config)
end
function update(dt)
    frame = frame + 1
    if frame > stop then return end
    if frame > 0 then
        local mask, packed = {{}}, {{}}
        local preset = actor:get("azure.animator", "expression")
        for name, weight in preset:gmatch('"targetId":"([^"]+)","weight":([%+%-%d%.eE]+)') do
            mask[name] = assert(tonumber(weight))
        end
        for i, name in ipairs(keys) do packed[i] = mask[name] or false end
        chunk[#chunk + 1] = packed
        if #chunk == 64 or frame == stop then
            self:write_save("opening-face-" .. tostring(index), {{1, chunk}})
            index, chunk = index + 1, {{}}
        end
    else
        face:begin({{category = 18, commands = {{}}}}, false)
    end
    local command = schedule[tostring(frame)]
    if command then
        face:expression({{faceMotion = command[1]}})
        face:voice(command[2])
    end
    face:update(frame == 0 and 0 or dt)
end
"""


def replay(player, room, resources, models, catalog_path, trace_path, output):
    catalog = load(catalog_path)
    assert digest(models / FACE) == catalog["meshSHA256"]
    with gzip.open(trace_path, "rt", encoding="utf-8-sig") as stream:
        unity = json.load(stream)
    assert unity["targets"] == catalog["targets"] and unity["sampleRate"] == 60
    frames = unity["frames"]
    program = script(catalog, frames)
    output.mkdir()
    runtime = output / "runtime.json"
    profile = output / "profile"
    with fixture(room, models, 0, program) as project:
        level = load(project)["startupScene"]
        actions = output / "input.json"
        write(actions, {"schemaVersion": 1, "actions": [
            {"frame": 0, "action": "wait-level", "reference": level, "timeoutFrames": 300}]})
        result = subprocess.run([str(player), "--project", str(project), "--resource-root", str(resources),
                                 "--save-root", str(profile), "--game-actions", str(actions), "--runtime-report",
                                 str(runtime), "--fixed-frame-step", "--smoke-frames", str(len(frames) + 10),
                                 "--width", "640", "--height", "360"],
                                cwd=output, capture_output=True, encoding="utf-8", errors="replace", timeout=180)
    (output / "stdout.log").write_text(result.stdout, encoding="utf-8")
    (output / "stderr.log").write_text(result.stderr, encoding="utf-8")
    assert result.returncode == 0, result.stderr[-3500:]
    assert "Allocator after unload: buffers=0 images=0" in result.stdout
    assert "Expression skeleton fingerprint mismatch" not in result.stderr
    state = load(runtime)
    assert not state["scriptErrors"] and not state["presentationErrors"], (state["scriptErrors"], state["presentationErrors"])
    masks = []
    for index in range((len(frames) + 63) // 64):
        schema, chunk = slot(profile, "opening-face-" + str(index))
        assert schema == 1
        masks.extend(chunk)
    assert len(masks) == len(frames) and any(any(value is not False for value in row) for row in masks)
    errors = []
    for frame, mask in zip(frames, masks, strict=True):
        for i, (base, final, override) in enumerate(zip(frame["baseWeights"], frame["weights"], mask, strict=True)):
            actual = base / 100 if override is False else override
            errors.append((abs(actual - final / 100), frame["number"], catalog["targets"][i], actual, final / 100))
    worst = max(errors)
    assert worst[0] < 5e-5, worst
    proof = {"frames": len(frames), "values": len(errors), "maxWeightError": worst[0],
             "sourceSHA256": digest(trace_path), "catalogSHA256": digest(catalog_path),
             "playerSHA256": digest(player), "cleanAllocatorExits": 1}
    write(output / "opening-face-layer-replay.json", proof)
    return proof


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    names = ("player", "room", "resource-root", "models", "catalog", "source-trace", "output")
    for name in names:
        parser.add_argument("--" + name, type=Path, required=True)
    arguments = vars(parser.parse_args())
    print(json.dumps(replay(*(arguments[name.replace("-", "_")].resolve() for name in names))))
