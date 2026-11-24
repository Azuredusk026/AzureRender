"""Exercise every assembled opening body clip through native room input."""

import argparse
from itertools import groupby
import json
import math
from pathlib import Path
import subprocess

from reference_inventory import digest


def fixtures(graph, ordered):
    if graph["schemaVersion"] != 3:
        return [(name, 10) for name in ordered], [(name, None) for name in ordered]
    edges = {edge["from"]: edge for edge in graph["timeline"]}
    presets, expected = [], []
    for root in sorted(edges.keys() - {edge["to"] for edge in edges.values()}):
        name, seconds = root, 1
        expected.append((name, None))
        while name in edges:
            edge = edges[name]
            name = edge["to"]
            expected.append((name, edge))
            seconds += edge["afterSeconds"] + edge["blendSeconds"]
        presets.append((root, math.ceil(seconds * 60)))
    assert len(expected) == len(presets) + len(edges)
    return presets, expected


def replay(player, room, resources, output, timeline_path=None):
    output.mkdir()
    evidence = json.loads((room / "opening-body-receipt.json").read_bytes())
    graph = json.loads((room / "assets/heroine/humanoid-motion.json").read_bytes())
    if timeline_path:
        timed = json.loads(timeline_path.read_bytes())
        assert timed["schemaVersion"] == 3 and graph["states"] == timed["states"]
        graph = timed
    clip_states = {}
    for state in graph["states"]:
        if state["name"].startswith("story-"):
            clip_states.setdefault(state["clip"], state["name"])
    expected = sorted(motion["clipIndex"] for motion in evidence["motions"].values())
    assert list(sorted(clip_states)) == expected and len(expected) == 18
    ordered = [clip_states[index] for index in expected]
    presets, expected_states = fixtures(graph, ordered)
    descriptor = room / "capture-opening-body.azureproject"
    level = room / "assets/capture-opening-body.azurelevel"
    script = room / "assets/capture-opening-body.lua"
    project = json.loads((room / "project.azureproject").read_bytes())
    project["startupScene"] = "assets:/" + level.name
    project["runtime"]["input"]["opening-motion-next"] = [74]
    scene = json.loads((room / "assets/room.azurelevel").read_bytes())
    timeline_asset = room / "assets/heroine/capture-opening-timeline.json"
    if timeline_path:
        names = {"heroine-" + part for part in evidence["parts"]}
        matching = {node["id"]: node for node in scene["nodes"] if node["id"] in names}
        assert matching.keys() == names
        for node in matching.values():
            node["components"]["azure.animator"]["data"]["asset"] = "assets:/heroine/" + timeline_asset.name
    scene["nodes"].append({"id": "opening-body-proof", "components": {
        "azure.script": {"type": "azure.script", "version": 1, "data": {"asset": "assets:/" + script.name}},
    }})
    parts = ",".join(json.dumps(part) for part in evidence["parts"])
    state_list = ",".join(json.dumps(name) for name, _ in presets)
    program = f"""local parts = {{{parts}}}
local states = {{{state_list}}}
local current = 0
function update()
    if not self:pressed("opening-motion-next") then return end
    current = current + 1
    for _, part in ipairs(parts) do
        assert(self:find("heroine-" .. part)):set("azure.animator", "state", assert(states[current]))
    end
end
"""
    frame = 20
    actions = [{"frame": 0, "action": "wait-level", "reference": project["startupScene"], "timeoutFrames": 400}]
    for _, duration in presets:
        for offset, active in ((0, True), (2, False)):
            actions.append({"frame": frame + offset, "action": "key", "key": 74, "down": active})
        frame += duration
    frames = frame + 15
    inputs = output / "input.json"
    inputs.write_text(json.dumps({"schemaVersion": 1, "actions": actions}), encoding="utf-8")
    runtime = output / "runtime.json"
    temporary = []
    try:
        assets = [(descriptor, json.dumps(project)), (level, json.dumps(scene)), (script, program)]
        if timeline_path:
            assets.append((timeline_asset, json.dumps(graph)))
        for path, payload in assets:
            with path.open("x", encoding="utf-8") as target:
                temporary.append(path)
                target.write(payload)
        result = subprocess.run([str(player), "--project", str(descriptor), "--resource-root", str(resources),
                                 "--save-root", str(output / "profile"), "--game-actions", str(inputs),
                                 "--runtime-report", str(runtime), "--fixed-frame-step", "--smoke-frames", str(frames),
                                 "--width", "640", "--height", "360"], cwd=output, capture_output=True,
                                encoding="utf-8", errors="replace", timeout=240)
    finally:
        for path in reversed(temporary):
            path.unlink()
    (output / "stdout.log").write_text(result.stdout, encoding="utf-8")
    (output / "stderr.log").write_text(result.stderr, encoding="utf-8")
    assert result.returncode == 0, result.stderr[-3500:]
    assert "Allocator after unload: buffers=0 images=0" in result.stdout
    report = json.loads(runtime.read_bytes())
    assert not report["scriptErrors"] and not report["presentationErrors"]
    assert report["audioStarts"] == 1 and report["replayedActions"] == len(actions)
    timeline = []
    def components(pose):
        return pose["clip"], pose["time"], pose["previousClip"], pose["blend"]

    for frame in report["routeFrames"]:
        parts = [pose for pose in frame["animations"] if pose["node"].startswith("heroine-")]
        assert len(parts) == 7 and len({components(part) for part in parts}) == 1
        timeline.append(components(parts[0]))
    spans = [(clip, list(run)) for clip, run in groupby(timeline, key=lambda row: row[0])]
    captured = []
    if timeline_path:
        states = {state["name"]: state["clip"] for state in graph["states"]}
        assert spans[0][0] == states[graph["initial"]]
        spans = spans[1:]
        expected = [states[name] for name, _ in expected_states]
        for index, ((name, edge), (_, run)) in enumerate(zip(expected_states, spans, strict=True)):
            if not edge:
                assert run[0][1] <= 1 / 60 + 1e-6
                continue
            last_source = spans[index - 1][1][-1][1]
            assert -1e-6 <= edge["afterSeconds"] - last_source <= 1 / 60 + 1e-6, (edge, last_source)
            _, first_target, previous_clip, blend = run[0]
            assert previous_clip == states[edge["from"]]
            assert -1e-6 <= first_target - edge["startSeconds"] <= 1 / 60 + 1e-6, (edge, run[0])
            assert 0 <= blend <= (1 / 60) / edge["blendSeconds"] + 1e-6 and run[-1][3] == 1
            captured.append({"from": edge["from"], "to": name, "lastSource": last_source,
                             "firstTarget": first_target, "firstBlend": blend, "frames": len(run)})
    else:
        assert all(len(run) >= 9 and run[0][1] <= 1 / 60 + 1e-6 for _, run in spans[1:])
    assert [clip for clip, _ in spans] == expected, [clip for clip, _ in spans]
    proof = {"clipIndices": expected, "frames": len(timeline), "parts": 7, "playerSHA256": digest(player),
             "runtimeSHA256": digest(runtime), "roomReceiptSHA256": digest(room / "opening-body-receipt.json")}
    if timeline_path:
        proof.update(timelineSHA256=digest(timeline_path), timedTransitions=captured)
    (output / "opening-motion-replay.json").write_text(json.dumps(proof) + "\n", encoding="utf-8")
    return proof


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for argument in ("player", "room", "resource-root", "output"):
        parser.add_argument("--" + argument, type=Path, required=True)
    parser.add_argument("--timeline", type=Path)
    args = parser.parse_args()
    print(json.dumps(replay(args.player.resolve(), args.room.resolve(), args.resource_root.resolve(), args.output.resolve(),
                            args.timeline.resolve() if args.timeline else None)))
