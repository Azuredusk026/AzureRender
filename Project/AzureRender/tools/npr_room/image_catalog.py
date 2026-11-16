"""Stage PNG textures and source fonts from the locked Recovery inventory."""

import argparse
from collections import Counter
import json
from pathlib import Path
import re
import shutil
from struct import unpack_from
import tempfile

from PIL import Image

from tools.npr_room.catalog_storage import publish_directory
from tools.npr_room.mesh_catalog import digest
from tools.npr_room.reference_inventory import META_GUID, text


EXTENSIONS = {".png", ".ttf", ".otf"}
SRGB = re.compile(r"(?m)^\s*sRGBTexture: ([01])\s*$")


def inspect_image(path):
    with Image.open(path) as source:
        if source.format != "PNG":
            raise ValueError(f"Expected PNG data: {path.name}")
        dimensions = {"width": source.width, "height": source.height, "mode": source.mode}
        source.verify()
    return dimensions


def inspect_font(path):
    data = path.read_bytes()
    if len(data) < 12 or data[:4] not in (b"OTTO", b"\0\1\0\0", b"true"):
        raise ValueError(f"Expected OpenType or TrueType data: {path.name}")
    count = unpack_from(">H", data, 4)[0]
    if len(data) < 12 + count * 16:
        raise ValueError(f"Font table directory is truncated: {path.name}")
    tables = {}
    for entry in range(count):
        tag, _, offset, size = unpack_from(">4sIII", data, 12 + entry * 16)
        if offset + size > len(data) or tag in tables:
            raise ValueError(f"Invalid font table: {path.name}")
        tables[tag] = (offset, size)
    outlines = {b"glyf", b"CFF ", b"CFF2"} & tables.keys()
    if not {b"cmap", b"head", b"maxp"} <= tables.keys() or len(outlines) != 1:
        raise ValueError(f"Font is missing a character map or outline table: {path.name}")
    offset, size = tables[b"maxp"]
    if size < 6:
        raise ValueError(f"Truncated font profile: {path.name}")
    return {
        "outlineFormat": next(iter(outlines)).decode("ascii").strip(),
        "glyphCount": unpack_from(">H", data, offset + 4)[0],
    }


def stage_image_catalog(project, report, output):
    if output.exists():
        raise FileExistsError(output)
    project = project.resolve(strict=True)
    slots, payloads, counts = {}, {}, Counter()
    with tempfile.TemporaryDirectory(prefix="recovery-images-", dir=output.parent) as directory:
        candidate = Path(directory) / "publish"
        media = candidate / "media"
        media.mkdir(parents=True)
        for name in sorted(set(report["additionalClosure"])):
            extension = Path(name).suffix.casefold()
            if extension not in EXTENSIONS:
                continue
            source = (project / name).resolve(strict=True)
            if not source.is_relative_to(project / "Assets"):
                raise ValueError(f"Source resolves outside Assets: {name}")
            meta = text(source.with_name(source.name + ".meta"))
            guid = report["assets"][name]
            match = META_GUID.search(meta)
            if not match or match[1].lower() != guid or guid in slots:
                raise ValueError(f"Source identity changed or is ambiguous: {name}")
            sha256 = digest(source)
            is_image = extension == ".png"
            metadata = inspect_image(source) if is_image else inspect_font(source)
            if is_image:
                srgb = SRGB.search(meta)
                metadata["srgb"] = None if srgb is None else srgb[1] == "1"
            payload = media / f"{sha256}{extension}"
            if payload.name not in payloads:
                shutil.copyfile(source, payload)
                if digest(payload) != sha256:
                    raise ValueError(f"Source changed during staging: {name}")
                payloads[payload.name] = payload.stat().st_size
            if digest(source) != sha256:
                raise ValueError(f"Source changed while reading: {name}")
            counts["textures" if is_image else "fonts"] += 1
            slots[guid] = {
                "source": name, "sha256": sha256, "media": f"media/{payload.name}", **metadata,
            }

        summary = {
            "textures": counts["textures"], "fonts": counts["fonts"],
            "sourceSlots": len(slots), "uniquePayloads": len(payloads),
            "payloadBytes": sum(payloads.values()),
        }
        (candidate / "image-catalog.json").write_text(json.dumps({
            "formatVersion": 1, "summary": summary, "slots": slots,
        }, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n",
            encoding="utf-8", newline="\n")
        publish_directory(candidate, output)
    return summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--inventory", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    inventory = json.loads(args.inventory.read_text(encoding="utf-8"))
    print(json.dumps(stage_image_catalog(args.source, inventory, args.output)))


if __name__ == "__main__":
    main()
