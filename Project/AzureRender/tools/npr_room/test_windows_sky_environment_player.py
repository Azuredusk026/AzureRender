"""Verify that the original HDR sky reaches the native room environment pass."""

import argparse
import json
from pathlib import Path
import subprocess

import numpy as np
from PIL import Image

from reference_inventory import digest


def capture(player, room, resources, environment, atmosphere, output, name, mode, *, level_setup=None):
    output.mkdir()
    descriptor = room / f"capture-sky-{name}.azureproject"
    level = room / "assets" / f"capture-sky-{name}.azurelevel"
    metadata = level.with_name(level.name + ".azmeta")
    had_metadata = metadata.exists()
    project = json.loads((room / "project.azureproject").read_bytes())
    project["startupScene"] = "assets:/" + level.name
    scene = json.loads((room / "assets/room.azurelevel").read_bytes())
    scene["renderSettings"]["skyMode"] = mode
    from tools.npr_room.original_room_atmosphere import hide_atmosphere_geometry
    hide_atmosphere_geometry(scene, atmosphere)
    if level_setup is not None:
        level_setup(scene)
    with level.open("x", encoding="utf-8") as source:
        json.dump(scene, source)
    stats = output / "runtime.json"
    try:
        with descriptor.open("x", encoding="utf-8") as source:
            json.dump(project, source)
        try:
            process = subprocess.run([str(player), "--project", str(descriptor), "--resource-root", str(resources),
                "--save-root", str(output / "profile"), "--environment", str(environment), "--width", "1280",
                "--height", "720", "--fixed-frame-step", "--capture-dir", str(output / "frames"),
                "--capture-frames", "2", "--runtime-report", str(stats)], cwd=output, capture_output=True,
                encoding="utf-8", errors="replace", timeout=120)
        finally:
            descriptor.unlink()
    finally:
        level.unlink()
        if not had_metadata:
            metadata.unlink(missing_ok=True)
    (output / "stdout.log").write_text(process.stdout, encoding="utf-8")
    (output / "stderr.log").write_text(process.stderr, encoding="utf-8")
    assert process.returncode == 0, process.stderr[-3000:]
    assert "Allocator after unload: buffers=0 images=0" in process.stdout
    diagnostics = process.stdout + process.stderr
    assert not any(marker in diagnostics for marker in ("VUID-", "SYNC-HAZARD", "Validation Error")), diagnostics
    assert f"Environment: {environment}" in process.stdout
    report = json.loads(stats.read_bytes())
    assert not report["scriptErrors"] and not report["presentationErrors"]
    shot = max((output / "frames").glob("*.png"))
    with Image.open(shot) as image:
        assert image.size == (1280, 720)
        return np.asarray(image.convert("RGB")), digest(shot)


def replay(player, room, resources, environment, references, output):
    output.mkdir()
    atmosphere = json.loads(references.read_bytes())
    control, control_hash = capture(player, room, resources, environment, atmosphere, output / "control", "solid", 2)
    source, source_hash = capture(player, room, resources, environment, atmosphere, output / "source", "sunset", 1)
    # The exposed strip of the left pane excludes the wall, desk props and UI.
    roi = np.s_[44:216, 220:286]
    count = int(np.count_nonzero(np.any(control[roi] != source[roi], axis=2)))
    assert count > 10000, (count, control[roi].mean(axis=(0, 1)), source[roi].mean(axis=(0, 1)))
    stable = np.s_[525:595, 900:1000]
    assert np.array_equal(control[stable], source[stable]), "Sky transport changed the interior control region"
    report = {"environmentSHA256": digest(environment), "playerSHA256": digest(player), "skyChangedPixels": count,
              "skyMeanRgb": source[roi].mean(axis=(0, 1)).tolist(), "controlFrameSHA256": control_hash,
              "sourceFrameSHA256": source_hash}
    (output / "sky-transfer.json").write_text(json.dumps(report) + "\n", encoding="utf-8")
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for arg in ("player", "room", "resource-root", "environment", "references", "output"):
        parser.add_argument("--" + arg, type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(replay(args.player.resolve(), args.room.resolve(), args.resource_root.resolve(),
                            args.environment.resolve(), args.references.resolve(), args.output.resolve())))
