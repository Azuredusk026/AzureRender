"""Exercise room preload, cancellation and re-entry in the standalone Player."""

import argparse
import json
from itertools import groupby
from pathlib import Path
import subprocess


def scene_reference(level, levels):
    return levels[level] if levels is not None else f"assets:/{level}.azurelevel"


def replay(player, project, output, resource_root, expect_heroine=False, expect_music=False, exercise_timer=True, *,
           levels=None, profile=None, frame_limit=850, level_timeout_frames=400):
    output.mkdir()
    events = []

    def wait(frame, level, revision):
        events.append({"frame": frame, "action": "wait-level", "reference": scene_reference(level, levels),
                       "afterRevision": revision - 1, "timeoutFrames": level_timeout_frames})

    def key(frame, code):
        events.extend([{"frame": frame, "action": "key", "key": code, "down": True},
                       {"frame": frame + 2, "action": "key", "key": code, "down": False}])

    wait(0, "boot", 1)
    wait(1, "asset-load", 2)
    wait(2, "entry", 3)
    wait(3, "transition", 4)
    key(8, 259)
    wait(240, "transition", 4)
    key(245, 257)
    wait(252, "room", 5)
    if exercise_timer:
        for frame, code in ((260, 84), (270, 84), (280, 84), (290, 75), (300, 82)):
            key(frame, code)
    events.append({"frame": 320, "action": "load-level", "reference": scene_reference("transition", levels)})
    wait(321, "transition", 6)
    wait(322, "room", 7)

    route = output / "input.json"
    route.write_text(json.dumps({"schemaVersion": 1, "actions": events}, indent=2), encoding="utf-8")
    runtime = output / "runtime.json"
    command = [str(player), "--project", str(project), "--save-root", str(profile or output / "profile"),
               "--runtime-report", str(runtime), "--game-actions",
               str(route), "--fixed-frame-step", "--smoke-frames", str(frame_limit), "--width", "640", "--height", "360"]
    if resource_root:
        command.extend(("--resource-root", str(resource_root)))
    result = subprocess.run(command, cwd=output, capture_output=True, encoding="utf-8", errors="replace", timeout=220)
    (output / "stdout.log").write_text(result.stdout, encoding="utf-8")
    (output / "stderr.log").write_text(result.stderr, encoding="utf-8")
    if result.returncode:
        raise RuntimeError(result.stderr[-4000:] + result.stdout[-1000:])
    return verify(project, output, expect_heroine, expect_music, exercise_timer, levels=levels)


def verify(project, output, expect_heroine=False, expect_music=False, exercise_timer=True, *, levels=None):
    stdout = (output / "stdout.log").read_text(encoding="utf-8")
    stderr = (output / "stderr.log").read_text(encoding="utf-8")
    assert "Allocator after unload: buffers=0 images=0" in stdout
    report = json.loads((output / "runtime.json").read_bytes())
    events = json.loads((output / "input.json").read_bytes())["actions"]
    assert report["scene"] == "room" and report["levelRevision"] == 7, (
        report["scene"], report["levelRevision"], report["replayedActions"])
    expected_starts = (2 if expect_music else 0) + (5 if exercise_timer else 0)
    assert (report["replayedActions"], report["audioStarts"]) == (len(events), expected_starts), (
        report["replayedActions"], report["audioStarts"])
    if expect_music:
        tracks = json.loads(project.with_name("music-receipt.json").read_bytes())["tracks"]
        first_track = next(track for track in tracks if not track["locked"])
        music = [line for line in stderr.splitlines() if "Streaming source:" in line]
        assert len(music) == 2 and all(line.endswith(Path(first_track["asset"]).name) for line in music), music
        cache = project.parent / ".azure/cache"
        assert all(not list((cache / track["uuid"]).glob("*.azcache")) for track in tracks)
    assert report["uiDrawCalls"] and report["animationFrames"], report
    assert not report["presentationErrors"] and not report["scriptErrors"], report
    frames = report["routeFrames"]
    visits = [(reference, len(list(group))) for reference, group
              in groupby(frames, key=lambda frame: frame["levelReference"])]
    assert [reference for reference, _ in visits] == [
        scene_reference(level, levels) for level in
        ("boot", "asset-load", "entry", "transition", "room", "transition", "room")
    ], visits
    assert visits[3][1] >= 240 and visits[5][1] >= 150, visits
    assert all(not item["loadError"] for item in frames), visits
    result = {key: report[key] for key in ("scene", "levelRevision", "replayedActions", "replayDelayedFrames",
                                          "audioStarts", "animationFrames", "uiDrawCalls")} | {"visits": visits}
    if expect_heroine:
        sampled = []
        for frame in frames:
            parts = [item for item in frame["animations"] if item["node"].startswith("heroine-")]
            if parts:
                assert len(parts) == 7 and len({(part["clip"], part["time"]) for part in parts}) == 1
                sampled.append(parts[0]["clip"])
        sequence = [clip for clip, _ in groupby(sampled)]
        assert sequence == ([0, 1, 0] if exercise_timer else [0]), sequence
        result["heroineClips"] = sequence
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--player", type=Path, required=True)
    parser.add_argument("--project", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--resource-root", type=Path)
    parser.add_argument("--expect-heroine", action="store_true")
    parser.add_argument("--expect-music", action="store_true")
    parser.add_argument("--startup-only", action="store_true", help="Check level re-entry with the timer idle.")
    args = parser.parse_args()
    print(json.dumps(replay(args.player.resolve(), args.project.resolve(), args.output.resolve(),
                            args.resource_root.resolve() if args.resource_root else None, args.expect_heroine,
                            args.expect_music, not args.startup_only)))
