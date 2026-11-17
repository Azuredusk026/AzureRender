"""Exercise queued and consumed animator signals through native room script input."""

import argparse
from itertools import groupby
import json
from pathlib import Path
import subprocess

from tools.npr_room.pbr_materials import write
from tools.npr_room.reference_inventory import digest


STEP = 1 / 60
GRAPH = {
    "schemaVersion": 4, "initial": "enter",
    "states": [{"name": name, "clip": index} for index, name in enumerate(
        ("enter", "loop", "accent", "leave", "settled"))],
    "signals": ["gesture", "stop"],
    "transitions": [],
    "timeline": [
        {"from": "enter", "to": "loop", "afterSeconds": .4, "blendSeconds": .25, "startSeconds": .1},
        {"from": "accent", "to": "loop", "afterSeconds": .15, "blendSeconds": .1},
        {"from": "leave", "to": "settled", "afterSeconds": .3, "blendSeconds": .05},
    ],
    "branches": [
        {"from": "loop", "to": "accent", "signals": ["gesture"], "once": True, "blendSeconds": .1},
        {"from": "loop", "to": "leave", "signals": ["stop"], "blendSeconds": .2},
    ],
}


def stage(room):
    name = "capture-animation-signals"
    root = room / "assets"
    descriptor = room / (name + ".azureproject")
    script = root / (name + ".lua")
    graph = root / (name + ".json")
    level = root / (name + ".azurelevel")
    starter = json.loads((room / "project.azureproject").read_bytes())
    starter["startupScene"] = "assets:/" + level.name
    starter["runtime"]["input"].update({"signal-gesture": [298], "signal-stop": [299]})
    scene = json.loads((root / "room.azurelevel").read_bytes())
    parts = [node for node in scene["nodes"] if node["id"].startswith("heroine-")
             and "azure.animator" in node["components"]]
    assert len(parts) == 7
    for node in parts:
        animator = node["components"]["azure.animator"]["data"]
        animator.update(asset="assets:/" + graph.name, state="enter")
    scene["nodes"].append({"id": name, "components": {"azure.script": {
        "type": "azure.script", "version": 1, "data": {"asset": "assets:/" + script.name}}}})
    names = ", ".join(json.dumps(node["id"]) for node in parts)
    text = f"""local parts = {{{names}}}
function update()
    local signal = self:pressed("signal-gesture") and "gesture" or self:pressed("signal-stop") and "stop"
    if not signal then return end
    for _, part in ipairs(parts) do
        assert(self:find(part)):set("azure.animator", "state", signal)
    end
end
"""
    return descriptor, starter["startupScene"], {descriptor: json.dumps(starter),
        level: json.dumps(scene), script: text, graph: json.dumps(GRAPH)}


def replay(player, room, resources, output):
    output.mkdir()
    descriptor, level, fixture = stage(room)
    actions = [{"frame": 0, "action": "wait-level", "reference": level, "timeoutFrames": 500}]
    for frame, key in ((12, 298), (115, 299)):
        actions.extend({"frame": frame + offset, "action": "key", "key": key, "down": active}
                       for offset, active in ((0, True), (2, False)))
    inputs = output / "input.json"
    runtime = output / "runtime.json"
    write(inputs, {"schemaVersion": 1, "actions": actions})
    created = []
    try:
        for path, payload in fixture.items():
            with path.open("x", encoding="utf-8") as file:
                created.append(path)
                file.write(payload)
        result = subprocess.run([str(player), "--project", str(descriptor), "--resource-root", str(resources),
                                 "--save-root", str(output / "profile"), "--game-actions", str(inputs),
                                 "--runtime-report", str(runtime), "--fixed-frame-step", "--smoke-frames", "170",
                                 "--width", "640", "--height", "360"], cwd=output, capture_output=True,
                                encoding="utf-8", errors="replace", timeout=150)
    finally:
        for path in reversed(created):
            path.unlink()
    (output / "stdout.log").write_text(result.stdout, encoding="utf-8")
    (output / "stderr.log").write_text(result.stderr, encoding="utf-8")
    assert result.returncode == 0, result.stderr[-3500:]
    assert "Allocator after unload: buffers=0 images=0" in result.stdout
    observed = json.loads(runtime.read_bytes())
    assert not observed["scriptErrors"] and not observed["presentationErrors"], observed.get("presentationErrors")
    assert observed["replayedActions"] == len(actions)
    frames = []
    for row in observed["routeFrames"]:
        poses = [(pose["clip"], pose["time"], pose["previousClip"], pose["blend"])
                 for pose in row["animations"] if pose["node"].startswith("heroine-")]
        assert len(poses) == 7 and len(set(poses)) == 1
        frames.append(poses[0])
    spans = [(clip, list(rows)) for clip, rows in groupby(frames, key=lambda pose: pose[0])]
    assert [clip for clip, _ in spans] == [0, 1, 2, 1, 3, 4], [(clip, len(rows)) for clip, rows in spans]
    first_loop = spans[1][1][0]
    first_accent = spans[2][1][0]
    first_leave = spans[-2][1][0]
    first_settled = spans[-1][1][0]
    assert .1 - 1e-6 <= first_loop[1] <= .1 + STEP + 1e-6 and first_loop[2] == 0
    assert spans[1][1][-1][3] >= 1 - STEP / .25 - 1e-6 and first_accent[2] == 1
    assert len(spans[3][1]) > 30 and first_leave[2] == 1
    assert .3 - STEP - 1e-6 <= spans[-2][1][-1][1] <= .3 + 1e-6 and first_settled[2] == 3
    assert 0 <= first_accent[3] <= STEP / .1 + 1e-6 and 0 <= first_leave[3] <= STEP / .2 + 1e-6
    proof = {"playerSHA256": digest(player), "runtimeSHA256": digest(runtime),
             "frames": len(frames), "parts": 7, "clips": [clip for clip, _ in spans],
             "gestureCount": 1, "firstLoopTime": first_loop[1],
             "gestureBlend": first_accent[3], "returnBlend": first_leave[3],
             "lastReturnTime": spans[-2][1][-1][1], "cleanAllocatorExits": 1}
    write(output / "animation-signal-replay.json", proof)
    return proof


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("player", "room", "resource-root", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(replay(args.player.resolve(), args.room.resolve(), args.resource_root.resolve(),
                            args.output.resolve())))
