"""Bind captured opaque main textures and colors through per-instance materials."""

from collections import defaultdict
import copy
from hashlib import sha256
import json
import math
from pathlib import Path
import shutil
import tempfile
from uuid import NAMESPACE_URL, UUID, uuid5

from tools.npr_room.catalog_storage import publish_directory
from tools.npr_room.original_room_registration import node_path
from tools.npr_room.pbr_materials import staging_metadata, write
from tools.npr_room.reference_inventory import digest
from tools.npr_room.room_surfaces import mounted_assets


def captured_base(material, by_name):
    colors = {row["name"]: row["value"] for row in material["colors"]}
    rgba = [colors.get("_Color", dict(r=1, g=1, b=1, a=1))[key] for key in "rgba"]
    if rgba[3] != 1 or any(not math.isfinite(value) or not 0 <= value <= 1 for value in rgba):
        return None, None, "Captured base color requires an opaque finite unit range"
    references = [row for row in material["textures"] if row["name"] == "_MainTex"]
    if len(references) != 1:
        return None, None, "Captured main texture binding is missing or repeated"
    texture, = references
    if texture["scale"] != {"x": 1, "y": 1} or texture["offset"] != {"x": 0, "y": 0}:
        return None, None, "Captured main texture requires its UV transform"
    if not texture["texture"]:
        return rgba, None, None
    matches = by_name.get(texture["texture"], [])
    if len(matches) != 1 or matches[0][1]["srgb"] is not True:
        return None, None, "Captured main texture requires one catalog sRGB image"
    return rgba, matches[0], None


def install(frame, image_catalog, assets, level):
    output = assets / "captured-surfaces"
    if output.exists():
        raise FileExistsError(output)
    raw = frame.read_bytes()
    record = json.loads(raw)
    identity = sha256(raw).hexdigest()
    images = json.loads(image_catalog.read_bytes())["slots"]
    by_name = defaultdict(list)
    for guid, image in images.items():
        by_name[Path(image["source"]).stem].append((guid, image))
    observed = defaultdict(list)
    for renderer in record["renderers"]:
        if renderer["kind"] == "MeshRenderer" and renderer["enabled"] and renderer["visible"]:
            observed[renderer["path"]].append(renderer)
    mounted = mounted_assets(assets)
    nodes = {node["id"]: node for node in level["nodes"]}
    variants, textures, assignments, pending = {}, {}, [], []
    checked_slots = 0
    for node in nodes.values():
        if not node.get("name") or not node.get("materialSlots"):
            continue
        path = node_path(node, nodes)
        renderers = observed.get(path, [])
        if not renderers:
            continue
        signatures = {json.dumps(row["materials"], sort_keys=True) for row in renderers}
        if len(signatures) != 1:
            pending.append({"path": path, "reason": "Captured same-path material instances differ"})
            continue
        for source in renderers[0]["materials"]:
            slot = "name:unity-slot-" + str(source["slot"])
            old_asset = node["materialSlots"].get(slot)
            if source["shader"] != "lilToon" or source["renderQueue"] != 2000 or old_asset is None:
                continue
            checked_slots += 1
            rgba, texture, reason = captured_base(source, by_name)
            if reason:
                pending.append({"path": path, "slot": slot, "reason": reason})
                continue
            original_file = mounted.get(old_asset)
            if original_file is None:
                raise ValueError("Captured material source binding is unavailable")
            original = json.loads(original_file.read_bytes())
            if original.get("typeId") != "azure.pbr" or original.get("schemaVersion") != 1:
                pending.append({"path": path, "slot": slot, "reason": "Captured base requires the existing PBR material profile"})
                continue
            variant = copy.deepcopy(original)
            variant["parameters"]["baseColor"] = rgba
            if texture is None:
                variant["textures"].pop("albedo", None)
            else:
                guid, image = texture
                asset_id = str(UUID(hex=guid))
                image_file = (image_catalog.parent / image["media"]).resolve(strict=True)
                if (not image_file.is_relative_to(image_catalog.parent.resolve())
                        or digest(image_file) != image["sha256"]):
                    raise ValueError("Captured main image identity differs")
                if asset_id in mounted:
                    if digest(mounted[asset_id]) != image["sha256"]:
                        raise ValueError("Mounted captured main image identity differs")
                else:
                    textures[guid] = (image_file, image["sha256"])
                variant["textures"]["albedo"] = {"asset": asset_id, "usage": "albedo"}
            if variant == original:
                continue
            variant_id = uuid5(NAMESPACE_URL, "captured-room-base-v1:" + identity + ":" + node["id"] + ":" + slot)
            variants[variant_id] = variant
            assignments.append({"node": node["id"], "path": path, "slot": slot, "asset": str(variant_id),
                                "sourceMaterialAsset": old_asset, "sourceMaterialSHA256": digest(original_file)})
    if variants or textures:
        with tempfile.TemporaryDirectory(prefix="captured-surfaces-", dir=assets) as temporary:
            staged = Path(temporary) / "captured-surfaces"
            staged.mkdir()
            for guid, (source, checksum) in textures.items():
                destination = staged / (guid + ".png")
                shutil.copyfile(source, destination)
                if digest(destination) != checksum:
                    raise ValueError("Captured main image changed during installation")
                staging_metadata(destination, guid, [], retain_source_snapshot=False)
            for variant_id, variant in variants.items():
                destination = staged / (variant_id.hex + ".azurematerial")
                write(destination, variant)
                dependencies = sorted({value["asset"] for value in variant["textures"].values()})
                staging_metadata(destination, variant_id.hex, dependencies, retain_source_snapshot=False)
            publish_directory(staged, output)
    for assignment in assignments:
        nodes[assignment["node"]]["materialSlots"][assignment["slot"]] = assignment["asset"]
    return {"schemaVersion": 1, "frameSHA256": identity, "checkedSlots": checked_slots,
            "changedSlots": len(assignments), "assignments": assignments, "newImages": len(textures), "pending": pending,
            "scope": "captured opaque rigid-renderer main texture and base color"}
