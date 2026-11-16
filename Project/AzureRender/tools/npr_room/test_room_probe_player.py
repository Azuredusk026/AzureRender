"""Replay the room probe through the standalone production input route."""

import argparse
import json
from pathlib import Path
import subprocess


def replay(player, project, output):
    output.mkdir()
    route = output / "input.json"
    runtime = output / "runtime.json"
    events = [{"frame": 1, "action": "wait-level", "reference": "assets:/room.azurelevel",
               "timeoutFrames": 300}]
    for frame, key in [(5, 84), (14, 84), (22, 84), (30, 75), (38, 82)]:
        events.append({"frame": frame, "action": "key", "key": key, "down": True})
        events.append({"frame": frame + 2, "action": "key", "key": key, "down": False})
    route.write_text(json.dumps({"schemaVersion": 1, "actions": events}, indent=2), encoding="utf-8")
    result = subprocess.run(
        [str(player), "--project", str(project), "--game-actions", str(route),
         "--fixed-frame-step", "--smoke-frames", "360", "--width", "640", "--height", "360",
         "--runtime-report", str(runtime)],
        cwd=output, capture_output=True, encoding="utf-8", errors="replace", timeout=140,
    )
    (output / "stdout.log").write_text(result.stdout, encoding="utf-8")
    (output / "stderr.log").write_text(result.stderr, encoding="utf-8")
    if result.returncode:
        raise RuntimeError(result.stderr[-3000:] + result.stdout[-2000:])
    if "Allocator after unload: buffers=0 images=0" not in result.stdout:
        raise RuntimeError("Player did not confirm resource release")
    report = json.loads(runtime.read_text(encoding="utf-8"))
    if report["presentationErrors"] or report["scriptErrors"]:
        raise RuntimeError(f"Room script or presentation failed: {report}")
    if report["replayedActions"] != len(events) or report["audioStarts"] != 5:
        raise RuntimeError(f"Expected production input replay and five cue starts: {report}")
    if not report["uiDrawCalls"] or not report["animationFrames"]:
        raise RuntimeError(f"Room UI or chair animation did not run: {report}")
    return {key: report[key] for key in (
        "scene", "levelRevision", "uiDrawCalls", "animationFrames", "audioStarts",
        "replayedActions", "replayDelayedFrames")}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--player", type=Path, required=True)
    parser.add_argument("--project", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(replay(args.player.resolve(), args.project.resolve(), args.output.resolve())))


if __name__ == "__main__":
    main()
