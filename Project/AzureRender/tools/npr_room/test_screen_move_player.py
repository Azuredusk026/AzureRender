"""Verify native screen displacement and the order of the opening composites."""

import argparse
import json
import os
from pathlib import Path

import numpy as np

from tools.npr_room.opening_chromatic import room_inputs as chromatic_inputs
from tools.npr_room.opening_screen_move import KIND, room_inputs as movement_inputs
from tools.npr_room.reference_inventory import digest
from tools.test_screen_noise_player import capture, component, write


def replay(player, resources, lock, images, output):
    movement = movement_inputs(lock, images)
    chromatic = chromatic_inputs(lock, images)

    def configure(ratio=None, color=False, scripted=False):
        def install(assets, nodes):
            modes = []
            if ratio is not None:
                modes.append((movement, ratio))
            if color:
                modes.append((chromatic, .2))
            for (node, media, _), amount in modes:
                for name, source in media.items():
                    os.link(source, assets / name)
                kind, schema = next(iter(node["components"].items()))
                data = schema["data"] | {"ratio": amount}
                components = {kind: component(kind, data)}
                if kind == KIND and scripted:
                    (assets / "enable.lua").write_text(
                        'function init() self:set("azure.screen-move", "ratio", .5) end\n', encoding="utf-8")
                    components["azure.script"] = component("azure.script", {"asset": "assets:/enable.lua"})
                nodes.append({"id": node["id"], "components": components})
        return install

    fixtures = {"control": None, "inactive": configure(0), "movement": configure(.5),
                "scripted": configure(0, scripted=True), "chromatic": configure(color=True),
                "combined": configure(.5, color=True)}
    reports = {name: capture(player, resources, output / name, None, configure=setup)
               for name, setup in fixtures.items()}
    marker = np.s_[15:45, 15:110]
    scene = np.s_[170:335, 20:620]
    source = np.s_[100:]
    weights = np.array([65536, 256, 1], dtype=np.int32)
    moved, reordered, invented = [], [], []
    for index, (control, inactive, displacement, scripted, color, combined) in enumerate(
            zip(*reports.values(), strict=True)):
        assert np.array_equal(control, inactive)
        assert np.array_equal(scripted, displacement if index else control), index
        for frame in (displacement, color, combined):
            assert np.all(frame[marker] == [44, 121, 167]), "Opening effects changed the opaque game UI"
        moved.append(int(np.count_nonzero(np.any(control[scene] != displacement[scene], axis=2))))
        reordered.append(int(np.count_nonzero(np.any(color[scene] != combined[scene], axis=2))))
        unexpected = ~np.isin(combined[scene] @ weights, np.unique(color[source] @ weights))
        invented.append(int(unexpected.sum()))
    assert max(moved) > 500 and max(reordered) > 500, (moved, reordered)
    assert invented == [0, 0, 0], invented
    proof = {"movementChangedPixels": moved, "combinedChangedPixels": reordered, "combinedNewColors": invented,
             "defaultPreservesFrame": True, "opaqueUiChangedPixels": 0, "scriptMatchesAuthoredFromFrame": 1,
             "material": movement[2], "playerSHA256": digest(player)}
    write(output / "screen-move-replay.json", proof)
    return proof


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("player", "resource-root", "source-lock", "image-catalog", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(replay(args.player.resolve(), args.resource_root.resolve(), args.source_lock.resolve(),
                            args.image_catalog.resolve(), args.output.resolve())))
