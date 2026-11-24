"""Check the original room base material bindings in a native Player pair."""

import argparse
import json
from pathlib import Path

import numpy as np

from reference_inventory import digest
from test_cloudy_room_player import capture, image_values


def replay(player, base, textured, resources, output):
    data = json.loads((textured / "room-surface-receipt.json").read_bytes())
    materials = data["materials"]
    assets = textured / "assets"
    before = json.loads((base / "assets/room.azurelevel").read_bytes())
    after = json.loads((assets / "room.azurelevel").read_bytes())
    nodes = {node["id"]: node for node in after["nodes"]}
    for row in data["assignments"]:
        node = nodes[row["node"]]
        group = node["materialSlots"]
        assert group.pop(row["slot"]) == materials[row["guid"]]["asset"], row
        if not group:
            del node["materialSlots"]
    assert before == after, "The variants must match outside their new material bindings"
    for guid, row in materials.items():
        path = assets / "room-surfaces" / (guid + ".azurematerial")
        assert digest(path) == row["sha256"], path
    for row in data["textures"].values():
        assert digest(assets / row["path"]) == row["sha256"], row
    sources = {Path(row["source"]).stem for row in materials.values()}
    assert {"mt_desk", "mt_wall_b", "mt_Chair", "mt_floor_2", "mt_combined_room_far"} <= sources

    output.mkdir()
    first, original = capture(player, base, resources, output / "base", frames=1)
    second, modified = capture(player, textured, resources, output / "textured", frames=1)
    assert [node["id"] for node in original["nodes"]] == [node["id"] for node in modified["nodes"]]
    before_rgb, before_luma = image_values(first)
    after_rgb, after_luma = image_values(second)
    changed = int(np.count_nonzero(np.any(before_rgb != after_rgb, axis=2)))
    assert changed > 1000 and before_luma != after_luma
    report = {"changedPixels": changed, "meanLinearLuma": [before_luma, after_luma],
              "imageSHA256": [digest(first), digest(second)], "assignments": len(data["assignments"]),
              "materials": len(materials), "textures": len(data["textures"])}
    (output / "room-surface-pair.json").write_text(json.dumps(report), encoding="utf-8")
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for argument in ("player", "base", "textured", "resource-root", "output"):
        parser.add_argument("--" + argument, type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(replay(args.player.resolve(), args.base.resolve(), args.textured.resolve(),
                            args.resource_root.resolve(), args.output.resolve())))
