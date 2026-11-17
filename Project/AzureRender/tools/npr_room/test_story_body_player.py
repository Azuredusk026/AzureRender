"""Exercise every assembled opening body clip through native room input."""

import argparse
from itertools import groupby
import json
from pathlib import Path
import subprocess

from reference_inventory import digest


def replay(player, room, resources, output):
    output.mkdir()
    evidence = json.loads((room / "opening-body-receipt.json").read_bytes())
    graph = json.loads((room / "assets/heroine/humanoid-motion.json").read_bytes())
    clip_states = {}
    for state in graph["states"]:
        if state["name"].startswith("story-"):
            clip_states.setdefault(state["clip"], state["name"])
    expected = sorted(motion["clipIndex"] for motion in evidence["motions"].values())
    assert list(sorted(clip_states)) == expected and len(expected) == 18
    ordered = [clip_states[index] for index in expected]
    descriptor = room / "capture-opening-body.azureproject"
    level = room / "assets/capture-opening-body.azurelevel"
    script = room / "assets/capture-opening-body.lua"
    project = json.loads((room / "project.azureproject").read_bytes())
    project["startupScene"] = "assets:/" + level.name
    project["runtime"]["input"]["opening-motion-next"] = [74]
    scene = json.loads((room / "assets/room.azurelevel").read_bytes())
    scene["nodes"].append({"id": "opening-body-proof", "components": {
        "azure.script": {"type": "azure.script", "version": 1, "data": {"asset": "assets:/" + script.name}},
    }})
    parts = ",".join(json.dumps(part) for part in evidence["parts"])
    state_list = ",".join(json.dumps(name) for name in ordered)
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
    frames = len(ordered) * 10 + 35
    actions = [{"frame": 0, "action": "wait-level", "reference": project["startupScene"], "timeoutFrames": 400}]
    for frame in range(20, 20 + len(ordered) * 10, 10):
        for offset, active in ((0, True), (2, False)):
            actions.append({"frame": frame + offset, "action": "key", "key": 74, "down": active})
    assert len(actions) == 37
    inputs = output / "input.json"
    inputs.write_text(json.dumps({"schemaVersion": 1, "actions": actions}), encoding="utf-8")
    runtime = output / "runtime.json"
    temporary = []
    try:
        for path, payload in ((descriptor, json.dumps(project)), (level, json.dumps(scene)), (script, program)):
            with path.open("x", encoding="utf-8") as target:
                temporary.append(path)
                target.write(payload)
        result = subprocess.run([str(player), "--project", str(descriptor), "--resource-root", str(resources),
                                 "--save-root", str(output / "profile"), "--game-actions", str(inputs),
                                 "--runtime-report", str(runtime), "--fixed-frame-step", "--smoke-frames", str(frames),
                                 "--width", "640", "--height", "360"], cwd=output, capture_output=True,
                                encoding="utf-8", errors="replace", timeout=135)
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
    for frame in report["routeFrames"]:
        parts = [pose for pose in frame["animations"] if pose["node"].startswith("heroine-")]
        assert len(parts) == 7 and len({(part["clip"], part["time"]) for part in parts}) == 1
        timeline.append((parts[0]["clip"], parts[0]["time"]))
    spans = [(clip, list(run)) for clip, run in groupby(timeline, key=lambda row: row[0])]
    assert [clip for clip, _ in spans] == expected, [clip for clip, _ in spans]
    assert all(len(run) >= 9 and run[0][1] <= 1 / 60 + 1e-6 for _, run in spans[1:])
    proof = {"clipIndices": expected, "frames": len(timeline), "parts": 7, "playerSHA256": digest(player),
             "runtimeSHA256": digest(runtime), "roomReceiptSHA256": digest(room / "opening-body-receipt.json")}
    (output / "opening-motion-replay.json").write_text(json.dumps(proof) + "\n", encoding="utf-8")
    return proof


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for argument in ("player", "room", "resource-root", "output"):
        parser.add_argument("--" + argument, type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(replay(args.player.resolve(), args.room.resolve(), args.resource_root.resolve(), args.output.resolve())))
