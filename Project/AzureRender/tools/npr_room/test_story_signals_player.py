"""Compare the projected first-chapter body graph with the offline Unity trace."""

import argparse
import json
import os
from pathlib import Path
import subprocess

from tools.npr_room.build_story_signal_room import EXAMPLE, GROUP, inputs
from tools.npr_room.pbr_materials import write
from tools.npr_room.reference_inventory import digest
from tools.npr_room.story_chapters import lua_value
from tools.npr_room.tutorial_camera_body import TUTORIAL_RECEIPT, project as tutorial_project


def camera_model_links(root, scene, parts, models, tag):
    links = {}
    receipt = json.loads((models / TUTORIAL_RECEIPT).read_bytes())
    part_ids = {node["resourceId"] for node in parts}
    for resource in scene["resources"]:
        if resource["id"] not in part_ids:
            continue
        part = resource["id"].removeprefix("heroine-")
        original = models / "assets/heroine" / (part + ".glb")
        assert receipt["parts"][part]["sha256"] == digest(original)
        alias = tag + "-" + original.name
        links[root / "heroine" / alias] = original
        resource["asset"] = "assets:/heroine/" + alias
    assert len(links) == 7
    return links


def stage(room, compiled, route, schedule, models=None):
    tag = "capture-first-chapter-body"
    root = room / "assets"
    project = room / (tag + ".azureproject")
    script, graph, level = (root / (tag + ext) for ext in (".lua", ".json", ".azurelevel"))
    starter = json.loads((room / "project.azureproject").read_bytes())
    starter["startupScene"] = "assets:/" + level.name
    scene = json.loads((root / "room.azurelevel").read_bytes())
    parts = [node for node in scene["nodes"] if node["id"].startswith("heroine-")
             and "azure.animator" in node["components"]]
    if len(parts) != 7:
        raise ValueError("Expected the seven native body components")
    for node in parts:
        node["components"]["azure.animator"]["data"].update(asset="assets:/" + graph.name, state=route["entry"])
    links = {}
    group = GROUP
    if models is not None:
        group = "tutorial_01"
        links = camera_model_links(root, scene, parts, models, tag)
    scene["nodes"].append({"id": tag, "components": {"azure.script": {
        "type": "azure.script", "version": 1, "data": {"asset": "assets:/" + script.name}}}})
    text = (EXAMPLE / "story-body.lua").read_text(encoding="utf-8") + f"""
local chapter = {lua_value({"group": group, "commands": [{"command": "Text", "bodyMotion": 0}]})}
local parts = {lua_value([node["id"] for node in parts])}
local commands = {lua_value({str(frame): motion for frame, motion in schedule})}
local motion = new_story_body(function(state)
    for _, part in ipairs(parts) do
        assert(self:find(part)):set("azure.animator", "state", state)
    end
end, {{}}, {lua_value({group: route})})
local frame = -1
function update()
    frame = frame + 1
    if frame == 0 then motion:begin(chapter) end
    local value = commands[tostring(frame)]
    if value == 400 then motion:effect({{command = "DirectionGameStartCamera"}})
    elseif value then motion:row({{command = "Text", bodyMotion = value}}) end
end
"""
    return project, starter["startupScene"], {project: json.dumps(starter), level: json.dumps(scene),
                                               script: text, graph: json.dumps(compiled)}, links


def compare(observed, unity, states, graph, route):
    assert not observed["scriptErrors"] and not observed["presentationErrors"], observed.get("presentationErrors")
    samples = []
    for row in observed["routeFrames"]:
        parts = [(pose["clip"], pose["time"], pose["previousClip"], pose["blend"])
                 for pose in row["animations"] if pose["node"].startswith("heroine-")]
        if not parts:
            continue
        assert len(parts) == 7 and len(set(parts)) == 1
        samples.append((row["frame"], *parts[0]))
    assert len(samples) >= unity["totalFrames"], len(samples)

    # Source current/next reports a blend on its first evaluation. Native reports
    # the target state and elapsed blend instead. Compare the same two clip slots.
    first = samples[0][0]
    previous = -1
    proof = []
    previous_pair = None
    repeats = 0
    for row in unity["frames"]:
        pair = (row["current"], row["next"]) if row["next"] else None
        if not pair or previous_pair == pair:
            previous_pair = pair
            continue
        previous_pair = pair
        target, source = states[row["next"]], states[row["current"]]
        signal = route["signals"].get(str(row["motion"]))
        edges = [edge for edge in graph["branches"] + graph["timeline"]
                 if edge["from"].removesuffix("-repeat") == source["name"]
                 and edge["to"].removesuffix("-repeat") == target["name"]
                 and ("signals" not in edge or signal in edge["signals"])]
        offsets = {edge["startSeconds"] for edge in edges}
        offset, = offsets
        repeats += pair[0] == pair[1]
        for position in range(previous + 1, len(samples)):
            frame, clip, time, before, blend = samples[position]
            if (before, clip) != (source["clip"], target["clip"]) or blend >= 1:
                continue
            tolerance = 1 / unity["sampleRate"] + 1e-5
            if not (offset - 1e-5 <= time <= offset + tolerance):
                continue
            delta = frame - first - row["frame"]
            assert abs(delta) <= 4, (row["frame"], frame, row["next"], delta)
            proof.append({"unityFrame": row["frame"], "nativeFrame": frame - first, "deltaFrames": delta,
                          "from": row["current"], "to": row["next"], "clip": clip, "targetSeconds": time})
            previous = position
            break
        else:
            raise AssertionError(f"Missing native transition: {row['current']} -> {row['next']}")
    special = "HeroineBaseAnimation.GameStart_Start" if "camera" in route else (
        "HeroineBaseAnimation.Story_SubBase_001.Story_SubBase001_Fun")
    clip = states[special]["clip"]
    special_entries = [sample for index, sample in enumerate(samples) if index
                       and sample[1] == clip and samples[index - 1][1] != clip]
    sample, = special_entries
    if "camera" in route:
        direct = next(row for row in unity["frames"] if row["motion"] == 400)
        assert abs(sample[0] - first - direct["frame"]) <= 2 and sample[4] == 1
    state_changes = sum(a[1] != b[1] or b[2] < a[2] - 1e-5 for a, b in zip(samples, samples[1:]))
    assert state_changes == len(proof) + ("camera" in route), (state_changes, len(proof))
    event = {"cameraEntries": 1, "holdRestarts": repeats} if "camera" in route else {"gestureEntries": 1}
    return {"frames": len(samples), "parts": 7, **event,
            "maximumFrameDelta": max(abs(row["deltaFrames"]) for row in proof), "transitions": proof}


def replay(player, room, resources, catalog, graph, chapters, source_trace, output, source_receipt=None, models=None):
    unity = json.loads(source_trace.read_bytes())
    if unity["editor"] != "2022.3.62f2c1" or unity["sampleRate"] != 60:
        raise ValueError("Expected the original editor and sample rate")
    if bool(source_receipt) != bool(models):
        raise ValueError("A camera replay needs both source receipt and sampled models")
    if models:
        compiled, _, route, states = tutorial_project(room, catalog, graph, chapters, source_receipt, models)
    else:
        compiled, route, states = inputs(room, catalog, graph, chapters)
    schedule = []
    for row in unity["frames"]:
        if not schedule or row["motion"] != schedule[-1][1]:
            schedule.append((row["frame"], row["motion"]))
    assert [motion for _, motion in schedule] in (
        ([0, 400, 401, 12, 15, 0, 3, 0], [0, 400, 401, 12, 0, 3, 0]) if models else
        ([0, 5, 0, 1, 0, 3, 0, 1000, 1003, 0],))
    project, level, fixture, links = stage(room, compiled, route, schedule, models)
    output.mkdir()
    actions = output / "input.json"
    runtime = output / "runtime.json"
    write(actions, {"schemaVersion": 1, "actions": [
        {"frame": 0, "action": "wait-level", "reference": level, "timeoutFrames": 500}]})
    staged = []
    try:
        for alias, original in links.items():
            os.link(original, alias)
            staged.append(alias)
        for path, payload in fixture.items():
            with path.open("x", encoding="utf-8") as stream:
                staged.append(path)
                stream.write(payload)
        result = subprocess.run([str(player), "--project", str(project), "--resource-root", str(resources),
                                 "--save-root", str(output / "profile"), "--game-actions", str(actions),
                                 "--runtime-report", str(runtime), "--fixed-frame-step", "--smoke-frames",
                                 str(unity["totalFrames"] + 10), "--width", "640", "--height", "360"],
                                cwd=output, capture_output=True, encoding="utf-8", errors="replace", timeout=200)
    finally:
        for path in reversed(staged):
            path.unlink()
    (output / "stdout.log").write_text(result.stdout, encoding="utf-8")
    (output / "stderr.log").write_text(result.stderr, encoding="utf-8")
    assert result.returncode == 0, result.stderr[-3500:]
    assert "Allocator after unload: buffers=0 images=0" in result.stdout
    report = compare(json.loads(runtime.read_bytes()), unity, states, compiled, route)
    report.update(playerSHA256=digest(player), sourceTraceSHA256=digest(source_trace),
                  runtimeSHA256=digest(runtime), cleanAllocatorExits=1)
    write(output / "first-chapter-body-replay.json", report)
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parameters = ("player", "room", "resource-root", "catalog", "graph", "chapters", "source-trace", "output")
    for name in parameters:
        parser.add_argument("--" + name, type=Path, required=True)
    for name in ("source-receipt", "models"):
        parser.add_argument("--" + name, type=Path)
    args = vars(parser.parse_args())
    print(json.dumps(replay(*(args[name.replace("-", "_")].resolve() for name in parameters),
                            source_receipt=args["source_receipt"], models=args["models"])))
