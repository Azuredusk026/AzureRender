"""Verify that a City purchase changes the native window view and restores the selected state."""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image

from test_ambient_room_player import opened_ambient, preferences
from test_notes_player import playing
from test_purchase_room_player import CONFIRM, expect_hidden
from test_story_player import slot
from test_unlock_room_player import save


def replay(player, project, resources, output):
    output.mkdir()
    profile = output / "profile"
    proof = json.loads(project.with_name("purchase-room-receipt.json").read_bytes())
    assert proof["cityWindow"]["sceneCase"] == 20 and proof["cityWindow"]["renderer"] == 8872
    ambient = json.loads(project.with_name("ambient-receipt.json").read_bytes())
    city, = (row["uuid"] for row in ambient["sounds"] if row["id"] == 25)
    save(profile, project, "work-calendar", [2, [], [12, 0, 15000], 0])
    images, frames = {}, {}

    def capture(screenshot, name):
        path = screenshot(name + ".png")
        frames[name] = hashlib.sha256(path.read_bytes()).hexdigest()
        with Image.open(path) as image:
            assert image.size == (1280, 720)
            images[name] = np.asarray(image.convert("RGB"))

    def select(click):
        click("ambient-toggle")
        for _ in range(3):
            click("ambient-page-next")
        click("ambient-select-25")

    solo, combined = "ambient-city-window", "ambient-city-combined"
    with playing(player, project, resources, profile, output / "purchase", 3) as (click, _, _, screenshot):
        select(click)
        expect_hidden(click, solo)
        capture(screenshot, "locked")
        click(CONFIRM)
        assert slot(profile, "room-purchases") == [1, [["City", 14000]]]
        assert preferences(profile) == [1, [[25, 50, False]]]
        capture(screenshot, "purchased")
        click(solo)
        assert slot(profile, "room-windows") == [1, [["City", False]]]
        assert preferences(profile) == [1, [[25, 50, False]]]
        capture(screenshot, "window-off")

    with playing(player, project, resources, profile, output / "restart-off", 2) as (click, _, _, screenshot):
        select(click)
        capture(screenshot, "restored-off")
        click(combined)
        assert slot(profile, "room-windows") == [1, [["City", False]]]
        assert preferences(profile) == [1, [[25, 50, True]]]
        click(combined)
        assert slot(profile, "room-windows") == [1, [["City", True]]]
        assert preferences(profile) == [1, [[25, 50, False]]]

    with playing(player, project, resources, profile, output / "restart-on", 2) as (click, _, _, screenshot):
        select(click)
        capture(screenshot, "restored-on")
        assert slot(profile, "room-windows") == [1, [["City", True]]]
        assert slot(profile, "room-purchases") == [1, [["City", 14000]]]
    for phase in ("purchase", "restart-off", "restart-on"):
        assert opened_ambient(output / phase) == [city]

    # Left pane and desk patches exclude the heroine and the animated purchase menu.
    view = np.s_[170:355, 273:580]
    desk = np.s_[400:470, 296:396]

    def changed(left, right, region):
        return int(np.count_nonzero(np.any(images[left][region] != images[right][region], axis=2)))

    pairs = (("locked", "purchased"), ("window-off", "purchased"), ("restored-off", "restored-on"))
    view_deltas = [changed(left, right, view) for left, right in pairs]
    assert all(value >= 15000 for value in view_deltas), view_deltas
    assert all(changed(left, right, desk) == 0 for left, right in pairs)
    for left, right in (("locked", "window-off"), ("window-off", "restored-off"), ("purchased", "restored-on")):
        assert changed(left, right, view) == 0, (left, right, changed(left, right, view))
    result = {"independentPlayers": 3, "sourceCase": 20, "changedWindowPixels": view_deltas,
              "unchangedDeskPixels": 7000, "restoredOffAndOn": True, "audioSource": city, "frames": frames,
              "audioStarts": [3, 2, 2], "cleanAllocatorExits": 3}
    (output / "city-switch-replay.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("player", "project", "resource-root", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(replay(args.player.resolve(), args.project.resolve(), args.resource_root.resolve(),
                            args.output.resolve())))
