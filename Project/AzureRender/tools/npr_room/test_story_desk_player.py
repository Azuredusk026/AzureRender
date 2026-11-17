"""Verify the original desk endpoint through the normal tutorial start."""

import argparse
from contextlib import contextmanager
import json
import math
from pathlib import Path
import shutil
import tempfile

import numpy as np
from PIL import Image

from test_notes_player import playing
from test_story_player import slot
from tools.npr_room.build_story_book_room import skip_capture_orphans
from tools.npr_room.build_story_desk_room import apply_pose
from tools.npr_room.pbr_materials import write
from tools.npr_room.reference_inventory import digest
from tools.npr_room.room_geometry import copy_room_asset
from tools.npr_room.story_chapters import lua_value


SAVE = "desk-pose-proof"
ACTION = "desk-pose-proof-snapshot"
KEY = 298


@contextmanager
def book_start(room, pose):
    with tempfile.TemporaryDirectory(prefix="desk-pose-proof-", dir=room.parent) as temp:
        project = Path(temp) / "project.azureproject"
        descriptor = json.loads((room / project.name).read_bytes())
        if any(KEY in keys for keys in descriptor["runtime"]["input"].values()):
            raise ValueError("Desk probe key is already assigned")
        descriptor["runtime"]["input"][ACTION] = [KEY]
        write(project, descriptor)
        assets = Path(temp) / "assets"
        shutil.copytree(room / "assets", assets, copy_function=copy_room_asset, ignore=skip_capture_orphans)
        level_file = assets / "room.azurelevel"
        level = json.loads(level_file.read_bytes())
        apply_pose(level, pose)
        level["nodes"].append({"id": "desk-pose-proof", "components": {"azure.script": {
            "type": "azure.script", "version": 1, "data": {"asset": "assets:/desk-pose-proof.lua"}}}})
        write(level_file, level)
        source = f"""local nodes = {lua_value([entry["node"] for entry in pose])}
local snapshots = 0
function update()
    if not self:pressed("{ACTION}") then return end
    local values = {{}}
    for _, node in ipairs(nodes) do
        local actor = assert(self:find(node))
        values[#values + 1] = {{node, actor:get("azure.transform", "translation"),
                              actor:get("azure.transform", "rotation")}}
    end
    snapshots = snapshots + 1
    self:write_save("{SAVE}", {{1, snapshots, values}})
end
"""
        (assets / "desk-pose-proof.lua").write_text(source, encoding="utf-8", newline="\n")
        yield project


def pose_errors(values, wanted, serial):
    expected = {entry["node"]: entry for entry in wanted}
    assert values[:2] == [1, serial] and {row[0] for row in values[2]} == expected.keys(), values
    distances, rotations = [], []
    for node, p, r in values[2]:
        distances.append(math.dist(p, expected[node]["translation"]))
        rotations.extend(abs((actual - goal + 180) % 360 - 180)
                         for actual, goal in zip(r, expected[node]["rotation"], strict=True))
    assert max(distances) < .0005 and max(rotations) < .1, (distances, rotations)
    return {"maxPositionMeters": max(distances), "maxEulerDegrees": max(rotations)}


def replay(player, room, resources, output):
    output.mkdir()
    file = room / "opening-desk-receipt.json"
    receipt = json.loads(file.read_bytes())
    poses = receipt["poses"]
    assert receipt["textbook"]["boneCount"] == 11 and receipt["textbook"]["pages"] == 9
    assert digest(room / "assets/desk/textbook.glb") == receipt["textbook"]["textbookGLBSHA256"]
    assert digest(room / "assets/room.azurelevel") == receipt["levelSHA256"]
    assert digest(room / "assets/pomodoro.lua") == receipt["scriptSHA256"]
    with playing(player, room / "project.azureproject", resources, output / "default-profile",
                 output / "default") as (_, _, _, screenshot):
        unwritten = screenshot("default-pc.png")
    profile = output / "probe-profile"
    with book_start(room, poses["Book"]) as project:
        with playing(player, project, resources, profile, output / "tutorial") as (click, press, _, screenshot):
            press(KEY, domain="game")
            book_error = pose_errors(slot(profile, SAVE), poses["Book"], 1)
            before = screenshot("before-pc.png")
            click("opening-story-start")
            press(KEY, domain="game")
            pc_error = pose_errors(slot(profile, SAVE), poses["Pc"], 2)
            click("opening-story-option-1", wait=70)
            after = screenshot("during-pc.png")
            assert slot(profile, "room-book-layout") is None

    def picture(path):
        with Image.open(path) as image:
            assert image.size == (1280, 720)
            return np.asarray(image.convert("RGB"))

    original, other, active = map(picture, (unwritten, before, after))
    keyboard = np.s_[604:658, 130:300]
    pane = np.s_[170:350, 273:550]

    def changed(left, right, region):
        return int(np.count_nonzero(np.any(left[region] != right[region], axis=2)))

    from_book = changed(other, active, keyboard)
    matches_pc = changed(original, active, keyboard)
    pane_deltas = [changed(other, active, pane), changed(original, active, pane)]
    assert from_book > 1000 and matches_pc == 0 and pane_deltas == [0, 0], (
        from_book, matches_pc, pane_deltas)
    replay_report = {"playerSHA256": digest(player), "deskReceiptSHA256": digest(file), "players": 2,
                     "changedKeyboardPixels": from_book, "keyboardPixelsVersusDefault": matches_pc,
                     "windowControlPixels": 180 * 277, "bookEndpoint": book_error, "pcEndpoint": pc_error,
                     "frames": {path.name: digest(path) for path in (unwritten, before, after)},
                     "bookDecorationSaveUnchanged": True, "cleanAllocatorExits": 2}
    write(output / "desk-pose-replay.json", replay_report)
    return replay_report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for arg in ("player", "room", "resource-root", "output"):
        parser.add_argument("--" + arg, type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(replay(args.player.resolve(), args.room.resolve(), args.resource_root.resolve(),
                            args.output.resolve())))
