"""Verify the room chromatic composite in a fixed-step native Player."""

import argparse
import json
import os
from pathlib import Path

import numpy as np

from tools.npr_room.opening_chromatic import KIND, room_inputs
from tools.npr_room.reference_inventory import digest
from tools.test_screen_noise_player import capture, component, write


def replay(player, resources, lock, images, output):
    node, media, receipt = room_inputs(lock, images)

    def effect(ratio, scripted=False):
        def configure(assets, nodes):
            for name, source in media.items():
                os.link(source, assets / name)
            settings = node["components"][KIND]["data"] | {"ratio": ratio}
            components = {KIND: component(KIND, settings)}
            if scripted:
                (assets / "enable.lua").write_text(
                    'function init() self:set("azure.screen-chromatic", "ratio", .2) end\n', encoding="utf-8")
                components["azure.script"] = component("azure.script", {"asset": "assets:/enable.lua"})
            nodes.append({"id": node["id"], "components": components})
        return configure

    options = {"control": None, "inactive": effect(0), "active": effect(.2), "scripted": effect(0, scripted=True)}
    frames = {name: capture(player, resources, output / name, None, configure=configure)
              for name, configure in options.items()}
    background = np.s_[170:, :, :]
    ui = np.s_[15:45, 15:110, :]
    changes = []
    for index, (control, inactive, active, scripted) in enumerate(zip(*frames.values(), strict=True)):
        assert np.array_equal(control, inactive), "An inactive effect changed the frame"
        assert np.array_equal(scripted, active if index else control), index
        assert np.all(control[ui] == [44, 121, 167]) and np.array_equal(control[ui], active[ui]), (
            "Chromatic effect changed opaque UI")
        before, after = control[background], active[background]
        assert np.max(after[..., 1:] - before[..., 1:]) <= 1, "Chromatic noise brightened green or blue"
        changed = int(np.count_nonzero(np.max(np.abs(after - before), axis=2) >= 4))
        assert changed > 20000, changed
        changes.append(changed)
    moved = int(np.count_nonzero(np.any(frames["active"][1][background] != frames["active"][2][background], axis=2)))
    assert moved > 1000, moved
    proof = {"changedBackgroundPixels": changes, "temporalChangedPixels": moved, "opaqueUiChangedPixels": 0,
             "defaultPreservesFrame": True, "scriptMatchesAuthoredFromFrame": 1,
             "material": receipt, "playerSHA256": digest(player)}
    write(output / "screen-chromatic-replay.json", proof)
    return proof


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("player", "resource-root", "source-lock", "image-catalog", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(replay(args.player.resolve(), args.resource_root.resolve(), args.source_lock.resolve(),
                            args.image_catalog.resolve(), args.output.resolve())))
