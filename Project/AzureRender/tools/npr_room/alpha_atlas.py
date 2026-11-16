"""Stage PNG alpha atlases from serialized Unity Alpha8 textures."""

import argparse
from hashlib import sha256
import json
from pathlib import Path
import tempfile

from PIL import Image

from tools.npr_room.catalog_storage import publish_directory
from tools.npr_room.reference_inventory import META_GUID, digest, text
from tools.npr_room.skinned_mesh_export import UnityScene


def unpack_alpha8(data):
    stream = data["m_StreamData"]
    if ((data["m_TextureFormat"], data["m_TextureDimension"], data["m_MipCount"]) != (1, 2, 1)
            or stream["size"] or stream["path"]):
        raise ValueError("Expected embedded single-mip Alpha8 Texture2D")
    width, height = data["m_Width"], data["m_Height"]
    payload = bytes.fromhex(str(data["_typelessdata"]))
    size = width * height
    if width <= 0 or height <= 0 or size != len(payload) or size != data["m_CompleteImageSize"] or size != data["image data"]:
        raise ValueError("Alpha8 payload size differs from the image dimensions")
    with Image.frombytes("L", (width, height), payload) as alpha:
        output = Image.new("RGBA", alpha.size, (255, 255, 255, 255))
        output.putalpha(alpha.transpose(Image.Transpose.FLIP_TOP_BOTTOM))
    return output, payload


def stage_atlases(source, inventory, names, output):
    source = source.resolve(strict=True)
    if output.exists():
        raise FileExistsError(output)
    slots = {}
    closure = set(inventory["additionalClosure"])
    with tempfile.TemporaryDirectory(prefix="recovery-alpha-", dir=output.parent) as directory:
        staged = Path(directory) / "publish"
        staged.mkdir()
        for name in sorted(set(names)):
            asset = (source / name).resolve(strict=True)
            if name not in closure or not asset.is_relative_to(source / "Assets"):
                raise ValueError(f"Texture is outside the locked asset closure: {name}")
            guid = inventory["assets"][name]
            meta = META_GUID.search(text(asset.with_name(asset.name + ".meta")))
            if not meta or meta[1].lower() != guid or guid in slots:
                raise ValueError(f"Texture identity changed or is ambiguous: {name}")
            asset_sha = digest(asset)
            document = UnityScene(asset)
            file_id, = [key for key, entry in document.documents.items() if entry[0] == 28]
            data = document.read(file_id)
            image, payload = unpack_alpha8(data)
            pending = staged / f"{guid}.png"
            with image:
                image.save(pending)
                width, height = image.size
            with Image.open(pending) as png:
                alpha = png.getchannel("A").transpose(Image.Transpose.FLIP_TOP_BOTTOM)
                if alpha.tobytes() != payload:
                    raise ValueError(f"Stored alpha differs from Unity texture: {name}")
            payload_sha = digest(pending)
            generated = staged / f"{payload_sha}.png"
            if generated.exists():
                pending.unlink()
            else:
                pending.rename(generated)
            if digest(asset) != asset_sha:
                raise ValueError(f"Texture changed while reading: {name}")
            slots[guid] = {
                "source": name, "fileID": file_id, "sourceSHA256": asset_sha,
                "sourceColorSpace": data["m_ColorSpace"], "alphaSHA256": sha256(payload).hexdigest(),
                "width": width, "height": height, "sha256": payload_sha, "media": generated.name,
            }
        report = {"formatVersion": 1, "slots": slots}
        (staged / "alpha-atlas.json").write_text(
            json.dumps(report, sort_keys=True, separators=(",", ":")) + "\n",
            encoding="utf-8", newline="\n",
        )
        publish_directory(staged, output)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--inventory", type=Path, required=True)
    parser.add_argument("--texture", action="append", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    inventory = json.loads(args.inventory.read_text(encoding="utf-8"))
    report = stage_atlases(args.source, inventory, args.texture, args.output)
    print(json.dumps({"alphaAtlasSlots": len(report["slots"])}))


if __name__ == "__main__":
    main()
