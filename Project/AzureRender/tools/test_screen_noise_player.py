"""Check screen noise compositing with an opaque game UI in a native Player."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import uuid

import numpy as np
from PIL import Image


def write(path, value):
    path.write_text(json.dumps(value), encoding="utf-8")


def component(type_name, data):
    return {"type": type_name, "version": 1, "data": data}


def capture(player, resources, root, opacity, scripted=False):
    assets = root / "assets"
    assets.mkdir(parents=True)
    project = root / "project.azureproject"
    write(project, {"schemaVersion": 1, "id": str(uuid.UUID(int=42)), "name": "Screen Noise Probe",
                    "startupScene": "assets:/screen.azurelevel", "mounts": [{"name": "assets", "path": "assets"}]})
    (assets / "marker.rml").write_text(
        '<rml><head><style>#marker { position: absolute; top: 0; left: 0; '
        'width: 250dp; height: 100dp; background-color: #2c79a7; }</style></head>'
        '<body><div id="marker"></div></body></rml>', encoding="utf-8")
    ui = {"id": "ui", "components": {"azure.game-ui": component("azure.game-ui", {"asset": "assets:/marker.rml"})}}
    nodes = [ui]
    if opacity is not None:
        data = {"opacity": opacity} if opacity else {}
        noise = {"azure.screen-noise": component("azure.screen-noise", data)}
        if scripted:
            (assets / "enable.lua").write_text(
                'function init() self:set("azure.screen-noise", "opacity", .25) end\n', encoding="utf-8")
            noise["azure.script"] = component("azure.script", {"asset": "assets:/enable.lua"})
        nodes.append({"id": "noise", "components": noise})
    write(assets / "screen.azurelevel", {"schemaVersion": 1, "id": "screen-noise", "sceneType": "sample",
                                       "nodes": nodes, "resources": []})
    capture_dir = root / "frames"
    report_path = root / "runtime.json"
    command = [str(player), "--project", str(project), "--resource-root", str(resources),
               "--save-root", str(root / "save"), "--capture-dir", str(capture_dir), "--capture-frames", "3",
               "--capture-fps", "4", "--width", "640", "--height", "360", "--runtime-report", str(report_path)]
    result = subprocess.run(command, cwd=root, capture_output=True, encoding="utf-8", errors="replace", timeout=100)
    (root / "stdout.log").write_text(result.stdout, encoding="utf-8")
    (root / "stderr.log").write_text(result.stderr, encoding="utf-8")
    assert result.returncode == 0, result.stderr
    assert "VUID-" not in result.stderr and "Validation Error" not in result.stderr, result.stderr
    assert "Allocator after unload: buffers=0 images=0" in result.stdout, result.stdout
    report = json.loads(report_path.read_bytes())
    assert report["uiDrawCalls"] > 0 and not report["scriptErrors"] and not report["presentationErrors"], report
    frames = [np.array(Image.open(path).convert("RGB"), dtype=np.int16) for path in sorted(capture_dir.glob("*.png"))]
    assert len(frames) == 3 and frames[0].shape == (360, 640, 3)
    return frames


def replay(player, resources, output):
    base = capture(player, resources, output / "base", None)
    disabled = capture(player, resources, output / "default", 0)
    active = capture(player, resources, output / "active", .25)
    scripted = capture(player, resources, output / "scripted", 0, scripted=True)
    masks, patterns = [], []
    for frame, (original, passive, effect, changed) in enumerate(zip(base, disabled, active, scripted)):
        assert np.array_equal(original, passive)
        # Script mutations become visible after the first fixed simulation step.
        assert np.array_equal(effect if frame else original, changed), frame
        signed = effect - original
        difference = abs(signed)
        # Compare the interior of the opaque UI marker away from its edges.
        assert not np.any(difference[15:45, 15:110])
        assert np.all(original[15:45, 15:110] == [44, 121, 167])
        mask = np.max(difference[170:], axis=-1) >= 8
        assert np.count_nonzero(mask) > 20000
        masks.append(mask)
        patterns.append(np.max(signed[170:], axis=-1) >= 8)
    moved = int(np.count_nonzero(patterns[1] != patterns[2]))
    assert moved > 1000, moved
    proof = {"backgroundChangedPixels": [int(np.count_nonzero(mask)) for mask in masks],
             "temporalMaskChangedPixels": moved, "opaqueUiChangedPixels": 0,
             "defaultPreservesFrame": True, "scriptMatchesAuthoredFromFrame": 1,
             "playerSHA256": hashlib.sha256(player.read_bytes()).hexdigest()}
    write(output / "screen-noise-replay.json", proof)
    return proof


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for value in ("player", "resource-root"):
        parser.add_argument("--" + value, type=Path, required=True)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if args.output:
        print(json.dumps(replay(args.player.resolve(), args.resource_root.resolve(), args.output.resolve())))
    else:
        with tempfile.TemporaryDirectory(prefix="screen-noise-", dir=Path.cwd()) as folder:
            print(json.dumps(replay(args.player.resolve(), args.resource_root.resolve(), Path(folder))))
