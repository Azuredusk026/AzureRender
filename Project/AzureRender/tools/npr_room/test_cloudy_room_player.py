"""Capture the integrated scene with and without the authored Cloudy lights."""

import argparse
import json
from pathlib import Path
import subprocess

import numpy as np
from PIL import Image

from reference_inventory import digest


def capture(player, room, resources, output):
    output.mkdir()
    project = json.loads((room / "project.azureproject").read_bytes())
    project["startupScene"] = "assets:/room.azurelevel"
    descriptor = room / "capture-cloudy.azureproject"
    with descriptor.open("x", encoding="utf-8") as stream:
        json.dump(project, stream)
    stats_path = output / "runtime.json"
    try:
        result = subprocess.run([
            str(player), "--project", str(descriptor), "--resource-root", str(resources), "--width", "1280",
            "--height", "720", "--fixed-frame-step", "--capture-dir", str(output / "frames"),
            "--capture-frames", "2", "--runtime-report", str(stats_path),
        ], cwd=output, capture_output=True, encoding="utf-8", errors="replace", timeout=150)
    finally:
        descriptor.unlink()
    (output / "stdout.log").write_text(result.stdout, encoding="utf-8")
    (output / "stderr.log").write_text(result.stderr, encoding="utf-8")
    assert result.returncode == 0, result.stderr[-3500:]
    assert "Allocator after unload: buffers=0 images=0" in result.stdout
    stats = json.loads(stats_path.read_bytes())
    assert stats["scene"] == "room" and not stats["scriptErrors"] and not stats["presentationErrors"], stats
    png = max((output / "frames").glob("*.png"))
    return png, stats


def image_values(path):
    with Image.open(path) as image:
        assert image.size == (1280, 720)
        values = np.asarray(image.convert("RGB"), dtype=np.float64) / 255.0
    linear = np.where(values < .04045, values / 12.92, ((values + .055) / 1.055) ** 2.4)
    return values, float((linear @ [0.2126, 0.7152, 0.0722]).mean())


def replay(player, base, cloudy, resources, output):
    output.mkdir()
    before = json.loads((base / "assets/room.azurelevel").read_bytes())
    after = json.loads((cloudy / "assets/room.azurelevel").read_bytes())
    lights = after.pop("lights")
    previous_lights = before.pop("lights", [])
    assert lights[:len(previous_lights)] == previous_lights
    added = lights[len(previous_lights):]
    ids = {row["nodeId"] for row in added}
    assert ids == {"unity-light-9420", "unity-light-9423", "unity-light-9467"}
    after["nodes"] = [node for node in after["nodes"] if node["id"] not in ids]
    assert before == after, "The level variants must match outside the new authored lights"

    base_image, base_stats = capture(player, base, resources, output / "base")
    cloudy_image, cloudy_stats = capture(player, cloudy, resources, output / "cloudy")
    assert ids.isdisjoint(node["id"] for node in base_stats["nodes"])
    assert ids <= {node["id"] for node in cloudy_stats["nodes"]}
    base_values, base_luma = image_values(base_image)
    cloudy_values, cloudy_luma = image_values(cloudy_image)
    different = int(np.count_nonzero(np.any(base_values != cloudy_values, axis=2)))
    assert different > 1000 and cloudy_luma != base_luma
    report = {"lights": added, "changedPixels": different, "meanLinearLuma": [base_luma, cloudy_luma],
              "imageSHA256": [digest(base_image), digest(cloudy_image)]}
    (output / "cloudy-pair.json").write_text(json.dumps(report), encoding="utf-8")
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for argument in ("player", "base", "cloudy", "resource-root", "output"):
        parser.add_argument("--" + argument, type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(replay(args.player.resolve(), args.base.resolve(), args.cloudy.resolve(),
                            args.resource_root.resolve(), args.output.resolve())))
