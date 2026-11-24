"""Index sprite geometry and atlas regions against the staged source images."""

import argparse
from collections import Counter
import json
import math
from pathlib import Path

from tools.npr_room.mesh_catalog import digest
from tools.npr_room.reference_inventory import META_GUID, text
from tools.npr_room.skinned_mesh_export import UnityScene


def rect_within(rect, texture):
    x, y, width, height = (rect[key] for key in ("x", "y", "width", "height"))
    if not {"width", "height"} <= texture.keys() or not all(map(math.isfinite, (x, y, width, height))):
        return False
    # Serialized X and extent round independently at Unity's float32 precision.
    edge_x = math.ldexp(1.0, math.frexp(texture["width"])[1] - 24)
    edge_y = math.ldexp(1.0, math.frexp(texture["height"])[1] - 24)
    return (x >= 0 and y >= 0 and width > 0 and height > 0
            and x + width <= texture["width"] + edge_x
            and y + height <= texture["height"] + edge_y)


def build_sprite_catalog(project, inventory, images):
    project = project.resolve(strict=True)
    slots, counts = {}, Counter()
    for name in sorted(set(inventory["additionalClosure"])):
        if not name.startswith("Assets/Sprite/"):
            continue
        source = (project / name).resolve(strict=True)
        if not source.is_relative_to(project / "Assets/Sprite"):
            raise ValueError(f"Sprite source resolves outside its asset folder: {name}")
        guid = inventory["assets"][name]
        match = META_GUID.search(text(source.with_name(source.name + ".meta")))
        if not match or match[1].lower() != guid or guid in slots:
            raise ValueError(f"Sprite identity changed or is ambiguous: {name}")
        scene = UnityScene(source)
        sprite_id, = [file_id for file_id, entry in scene.documents.items() if entry[0] == 213]
        sprite = scene.read(sprite_id)
        atlas = sprite["m_AtlasRD"]
        uses_atlas = bool(atlas["texture"]["fileID"])
        renderer = atlas if uses_atlas else sprite["m_RD"]
        texture = renderer["texture"].get("guid")
        alpha = renderer["alphaTexture"].get("guid")
        texture_rect = renderer["textureRect"]
        if not texture or texture not in images:
            state = "texture-missing"
        elif alpha and alpha not in images:
            state = "alpha-missing"
        elif not rect_within(texture_rect, images[texture]):
            state = "rectangle-outside"
        else:
            state = "region-mapped"
        counts[state] += 1
        slots[guid] = {
            "source": name, "sourceSHA256": digest(source), "fileID": sprite_id,
            "texture": texture, "alphaTexture": alpha,
            "usesAtlasRenderData": uses_atlas,
            "logicalRectangle": sprite["m_Rect"], "textureRectangle": texture_rect,
            "pivot": sprite["m_Pivot"], "border": sprite["m_Border"],
            "textureOffset": renderer["textureRectOffset"],
            "atlasOffset": renderer["atlasRectOffset"],
            "pixelsPerUnit": sprite["m_PixelsToUnits"],
            "settingsRaw": renderer["settingsRaw"], "uvTransform": renderer["uvTransform"],
            "state": state,
        }
    return {"formatVersion": 1, "summary": {"sources": len(slots), **counts}, "slots": slots}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--inventory", type=Path, required=True)
    parser.add_argument("--images", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    inventory = json.loads(args.inventory.read_text(encoding="utf-8"))
    images = json.loads(args.images.read_text(encoding="utf-8"))["slots"]
    data = build_sprite_catalog(args.source, inventory, images)
    with args.output.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(data, stream, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
        stream.write("\n")
    print(json.dumps(data["summary"]))


if __name__ == "__main__":
    main()
