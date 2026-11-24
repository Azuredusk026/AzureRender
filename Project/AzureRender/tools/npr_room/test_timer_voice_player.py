"""Measure native timer voice starts against the sampled source motion cues."""

import argparse
from itertools import groupby
import json
from pathlib import Path
import subprocess

from reference_inventory import digest
from test_room_voice_player import opened_voice


EVENTS = ("work", "break", "finish")


def replay(player, project, resources, output):
    evidence = json.loads(project.with_name("timer-voice-receipt.json").read_bytes())
    assert evidence["cueReplay"]
    report, runtime, timeline = run_room(player, project, resources, output, ((20, 84), (410, 75), (760, 82)), 1450)
    assert report["audioStarts"] == 7, (report["audioStarts"], opened_voice(output))
    expected_files = [Path(evidence["selectedVoices"][event]["asset"]).name for event in EVENTS]
    assert opened_voice(output) == expected_files, (opened_voice(output), expected_files)
    clips = [clip for clip, _ in groupby(timeline, key=lambda row: row[0])]
    assert clips == [0, 2, 1, 3, 0, 4, 0], clips

    observed = []
    for event in EVENTS:
        voice = evidence["selectedVoices"][event]
        motion = evidence["motions"][str(voice["bodyMotion"])]
        samples = [row for row in timeline if row[0] == motion["clipIndex"]]
        marker = motion["cues"][str(voice["animationCue"])]
        assert samples and samples[0][2] == 1, (event, samples[:4])
        at, = [row for row in samples[1:] if row[2] == 1]
        delta = at[1] - marker
        assert -1e-6 <= delta <= 1 / 60 + 1e-6, (event, marker, at)
        observed.append({"event": event, "group": voice["group"], "motion": motion["name"],
                         "clipIndex": motion["clipIndex"], "sourceMarker": marker, "nativeClipTime": at[1],
                         "startFrame": at[3], "errorSeconds": delta})
    proof = {"audioStarts": report["audioStarts"], "clipSequence": clips, "cues": observed,
             "frames": len(timeline), "playerSHA256": digest(player), "runtimeSHA256": digest(runtime)}
    (output / "timer-voice-replay.json").write_text(json.dumps(proof), encoding="utf-8")
    return proof


def run_room(player, project, resources, output, keys, frames):
    output.mkdir()
    actions = [{"frame": 0, "action": "wait-level", "reference": "assets:/room.azurelevel",
                "afterRevision": 0, "timeoutFrames": 400}]
    for frame, key in keys:
        actions.extend(({"frame": frame, "action": "key", "key": key, "down": True},
                        {"frame": frame + 2, "action": "key", "key": key, "down": False}))
    input_path = output / "input.json"
    input_path.write_text(json.dumps({"schemaVersion": 1, "actions": actions}), encoding="utf-8")
    native_project = json.loads(project.read_bytes())
    native_project["startupScene"] = "assets:/room.azurelevel"
    descriptor = project.with_name("capture-timer-voice.azureproject")
    with descriptor.open("x", encoding="utf-8") as scenario:
        json.dump(native_project, scenario)
    runtime = output / "runtime.json"
    try:
        result = subprocess.run([
            str(player), "--project", str(descriptor), "--resource-root", str(resources),
            "--save-root", str(output / "profile"), "--game-actions", str(input_path),
            "--runtime-report", str(runtime), "--fixed-frame-step", "--smoke-frames", str(frames),
            "--width", "640", "--height", "360",
        ], cwd=output, capture_output=True, encoding="utf-8", errors="replace", timeout=165)
    finally:
        descriptor.unlink()
    (output / "stdout.log").write_text(result.stdout, encoding="utf-8")
    (output / "stderr.log").write_text(result.stderr, encoding="utf-8")
    assert result.returncode == 0, result.stderr[-4500:]
    assert "Allocator after unload: buffers=0 images=0" in result.stdout
    report = json.loads(runtime.read_bytes())
    assert not report["scriptErrors"] and not report["presentationErrors"]
    assert report["scene"] == "room" and report["replayedActions"] == len(actions)

    timeline = []
    previous_starts = 0
    for frame in report["routeFrames"]:
        parts = [node for node in frame["animations"] if node["node"].startswith("heroine-")]
        assert len(parts) == 7 and len({(part["clip"], part["time"]) for part in parts}) == 1
        starts = frame["audioStarts"] - previous_starts
        timeline.append((parts[0]["clip"], parts[0]["time"], starts, frame["frame"]))
        previous_starts = frame["audioStarts"]
    return report, runtime, timeline


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("player", "project", "resource-root", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(replay(args.player.resolve(), args.project.resolve(), args.resource_root.resolve(),
                            args.output.resolve())))
