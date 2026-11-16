"""Verify timer preferences across two independent Player processes."""

import argparse
from pathlib import Path
import json
import shutil
import subprocess


SAVED = [1, 26, 6, 3]


def run_player(player, project, save_root, output, keys, resource_root):
    output.mkdir()
    input_path = output / "input.json"
    events = [{
        "frame": 1, "action": "wait-level", "reference": "assets:/room.azurelevel", "timeoutFrames": 300,
    }]
    for position, key in enumerate(keys):
        frame = 5 + 8 * position
        events.extend([
            {"frame": frame, "action": "key", "key": key, "down": True},
            {"frame": frame + 2, "action": "key", "key": key, "down": False},
        ])
    input_path.write_text(json.dumps({"schemaVersion": 1, "actions": events}), encoding="utf-8")
    runtime = output / "runtime.json"
    command = [
        str(player), "--project", str(project), "--save-root", str(save_root), "--game-actions", str(input_path),
        "--fixed-frame-step", "--smoke-frames", "360", "--width", "640", "--height", "360",
        "--runtime-report", str(runtime),
    ]
    if resource_root:
        command.extend(["--resource-root", str(resource_root)])
    result = subprocess.run(command, cwd=output, capture_output=True, encoding="utf-8", errors="replace", timeout=150)
    (output / "stdout.log").write_text(result.stdout, encoding="utf-8")
    (output / "stderr.log").write_text(result.stderr, encoding="utf-8")
    if result.returncode:
        raise RuntimeError(result.stderr[-3000:] + result.stdout[-1800:])
    report = json.loads(runtime.read_text(encoding="utf-8"))
    if (report["scene"] != "room" or report["replayedActions"] != len(events) or
            report["scriptErrors"] or report["presentationErrors"] or
            "Allocator after unload: buffers=0 images=0" not in result.stdout):
        raise RuntimeError(f"Room save replay did not complete: {report}")
    return report


def verify_save(player, project, output, resource_root=None):
    if output.is_relative_to(project.parent):
        raise ValueError("Save replay output must be outside the input project")
    output.mkdir()
    save_root = output / "profile"
    written = run_player(player, project, save_root, output / "write", [87, 66, 67, 87, 88, 84], resource_root)
    file, = save_root.glob("project-*/save-pomodoro-settings.json")
    saved = file.read_bytes()
    if json.loads(saved) != {"schemaVersion": 1, "payload": SAVED} or written["audioStarts"] != 6:
        raise RuntimeError("Persistent timer settings or source cues differ")

    reopened = output / "reopened"
    shutil.copytree(project.parent, reopened, ignore=shutil.ignore_patterns(".azure"))
    assets = reopened / "assets"
    level_file = assets / "room.azurelevel"
    level = json.loads(level_file.read_text(encoding="utf-8"))
    level["nodes"].append({
        "id": "verify-save",
        "components": {
            "azure.script": {"type": "azure.script", "version": 1, "data": {"asset": "assets:/verify-save.lua"}},
        },
    })
    level_file.write_text(json.dumps(level), encoding="utf-8")
    (assets / "verify-save.lua").write_text(
        'function init()\n'
        '    local settings = self:read_save("pomodoro-settings")\n'
        '    assert(settings and #settings == 4 and settings[1] == 1 and\n'
        '           settings[2] == 26 and settings[3] == 6 and settings[4] == 3, "Saved preferences differ")\n'
        'end\n', encoding="utf-8",
    )
    read = run_player(player, reopened / project.name, save_root, output / "read", [], resource_root)
    if read["activeScripts"] != 2 or read["audioStarts"] != 0 or saved != file.read_bytes():
        raise RuntimeError("Reopened room did not restore an idle timer with the saved preferences")
    return {"preferences": SAVED[1:], "replayedActions": written["replayedActions"],
            "audioStarts": written["audioStarts"], "readerActiveScripts": read["activeScripts"],
            "reopenedAudioStarts": read["audioStarts"], "openedSeparatePlayer": True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--player", type=Path, required=True)
    parser.add_argument("--project", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--resource-root", type=Path)
    args = parser.parse_args()
    print(json.dumps(verify_save(args.player.resolve(strict=True), args.project.resolve(strict=True),
                                 args.output.resolve(), args.resource_root)))


if __name__ == "__main__":
    main()
