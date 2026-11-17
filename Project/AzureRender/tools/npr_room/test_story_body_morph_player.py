"""Compare source face deformation in two renders at the same body clip time."""

import argparse
from contextlib import contextmanager
import json
import os
from pathlib import Path
import subprocess

from test_cloudy_room_player import image_values
from test_story_signals_player import camera_model_links
from tools.npr_room.build_story_signal_room import load
from tools.npr_room.pbr_materials import write
from tools.npr_room.reference_inventory import digest
from tools.npr_room.story_body_morphs import MORPH_RECEIPT

import numpy as np


@contextmanager
def fixture(room, models, clip, program=None):
    assets = room / "assets"
    tag = "capture-opening-body-face"
    level = load(assets / "room.azurelevel")
    parts = [node for node in level["nodes"] if node["id"].startswith("heroine-")
             and "azure.animator" in node["components"]]
    for node in level["nodes"]:
        node["components"].pop("azure.script", None)
    for node in parts:
        data = node["components"]["azure.animator"]["data"]
        data.update(asset="assets:/" + tag + ".json", state="camera-start")
        if node["id"] != "heroine-face":
            node["components"]["azure.renderable"] = {
                "type": "azure.renderable", "version": 1, "data": {"visible": False}}
    links = camera_model_links(assets, level, parts, models, tag)
    project = load(room / "project.azureproject")
    project["startupScene"] = "assets:/" + tag + ".azurelevel"
    descriptor = room / (tag + ".azureproject")
    script = assets / (tag + ".lua")
    if program:
        level["nodes"].append({"id": tag, "components": {"azure.script": {
            "type": "azure.script", "version": 1, "data": {"asset": "assets:/" + script.name}}}})
    graph = {"schemaVersion": 1, "initial": "camera-start", "transitions": [], "states": [
        {"name": "camera-start", "clip": clip, "loop": False}]}
    created = []
    try:
        for alias, source in links.items():
            os.link(source, alias)
            created.append(alias)
        for path, data in ((descriptor, project), (assets / (tag + ".azurelevel"), level),
                           (assets / (tag + ".json"), graph)):
            with path.open("x", encoding="utf-8") as stream:
                created.append(path)
                json.dump(data, stream)
        if program:
            with script.open("x", encoding="utf-8") as stream:
                created.append(script)
                stream.write(program)
        yield descriptor
    finally:
        for path in reversed(created):
            path.unlink()


def capture(player, room, resources, models, clip, output):
    output.mkdir()
    with fixture(room, models, clip) as project:
        runtime = output / "runtime.json"
        result = subprocess.run([
            str(player), "--project", str(project), "--resource-root", str(resources), "--save-root",
            str(output / "profile"), "--width", "1280", "--height", "720", "--fixed-frame-step",
            "--capture-dir", str(output / "frames"), "--capture-frames", "2", "--runtime-report", str(runtime),
        ], cwd=output, capture_output=True, encoding="utf-8", errors="replace", timeout=150)
    (output / "stdout.log").write_text(result.stdout, encoding="utf-8")
    (output / "stderr.log").write_text(result.stderr, encoding="utf-8")
    assert result.returncode == 0, result.stderr[-4500:]
    assert "Allocator after unload: buffers=0 images=0" in result.stdout
    report = load(runtime)
    assert not report["scriptErrors"] and not report["presentationErrors"], (
        report["scriptErrors"], report["presentationErrors"])
    animations = [row for row in report["animations"] if row["node"].startswith("heroine-")]
    assert len(animations) == 7 and {row["clip"] for row in animations} == {clip}, animations
    seconds, = {row["time"] for row in animations}
    assert 0 <= seconds <= .1, seconds
    images = sorted((output / "frames").glob("*.png"))
    assert len(images) == 2
    return images[-1], seconds


def replay(player, room, resources, original, models, output):
    receipt = load(models / MORPH_RECEIPT)
    clip = receipt["clips"]["anim_start_v3_full"]["clipIndex"]
    output.mkdir()
    blank, base_seconds = capture(player, room, resources, original, clip, output / "base")
    authored, authored_seconds = capture(player, room, resources, models, clip, output / "source")
    assert base_seconds == authored_seconds
    compare = np.any(image_values(blank)[0] != image_values(authored)[0], axis=2)
    row, column = np.nonzero(compare)
    assert len(row) > 100
    bounds = [int(column.min()), int(row.min()), int(column.max()), int(row.max())]
    assert bounds[2] - bounds[0] < 260 and bounds[3] - bounds[1] < 260, bounds
    proof = {"clip": clip, "clipSeconds": authored_seconds, "changedPixels": len(row), "boundsXYXY": bounds,
             "frames": [digest(blank), digest(authored)], "playerSHA256": digest(player),
             "modelSHA256": receipt["faceGlbSHA256"], "cleanAllocatorExits": 2}
    write(output / "opening-body-face-render.json", proof)
    return proof


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    names = ("player", "room", "resource-root", "original", "models", "output")
    for name in names:
        parser.add_argument("--" + name, type=Path, required=True)
    arguments = vars(parser.parse_args())
    print(json.dumps(replay(*(arguments[name.replace("-", "_")].resolve() for name in names))))
