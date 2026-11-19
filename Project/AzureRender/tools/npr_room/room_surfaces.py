"""Attach original opaque lilToon base colors and main textures to imported meshes."""

import argparse
import json
import math
import os
from pathlib import Path
import shutil
import tempfile
from uuid import UUID

from tools.build_face_sdf import parse_glb
from tools.npr_room.catalog_storage import publish_directory
from tools.npr_room.room_transparent_surfaces import install_lens
from tools.npr_room.pbr_materials import pbr_document, staging_metadata, write
from tools.npr_room.reference_inventory import digest
from tools.npr_room.room_geometry import copy_room_asset
from tools.npr_room.skinned_mesh_export import UnityScene
from tools.npr_room.window_scene_inventory import ROOM_SCENE


TOON_SHADER = "Assets/Shader/lilToon.shader"


def base_surface(source, images):
    properties = source["properties"]
    floats, colors, maps = (properties[key] for key in ("m_Floats", "m_Colors", "m_TexEnvs"))
    culling = floats.get("_Cull", 2)
    if source["shader"].get("source") != TOON_SHADER or source["parent"]["fileID"] or culling not in (0, 2):
        raise ValueError("Expected an independent opaque lilToon material")
    # lil_pass_forward_normal.hlsl uses LIL_RENDER == 0 for this shader's
    # opaque pass. The pass forces coverage to one and skips the alpha mask.
    if floats.get("_Invisible", 0):
        raise ValueError("Source coverage requires a separate profile")
    rgba = [colors.get("_Color", {}).get(key, 1) for key in "rgba"]
    if rgba[3] != 1 or any(not math.isfinite(value) or not 0 <= value <= 1 for value in rgba):
        raise ValueError("Source tint is outside the opaque base profile")
    uv_motion = colors.get("_MainTex_ScrollRotate", {})
    if any(uv_motion.get(key, 0) != 0 for key in "rgba"):
        raise ValueError("Animated main texture requires a separate transform")
    reference = maps.get("_MainTex", {})
    texture = None
    if reference.get("m_Texture", {}).get("fileID"):
        if reference["m_Scale"] != {"x": 1, "y": 1} or reference["m_Offset"] != {"x": 0, "y": 0}:
            raise ValueError("Main texture requires a separate UV transform")
        texture = reference["m_Texture"].get("guid")
        if images.get(texture, {}).get("srgb") is not True:
            raise ValueError("Main texture requires an sRGB PNG slot")
    return pbr_document(rgba, 1, 0, culling == 0, texture), texture


def material_bindings(node_map, nodes, renderers, transforms, mesh_slots):
    result = []
    for renderer in renderers.values():
        if renderer["type"] != "mesh" or not renderer["enabled"]:
            continue
        transform = transforms.get(renderer["object"])
        prefix = "unity-batch-" if renderer["staticBatch"]["subMeshCount"] else "unity-"
        node = node_map.get(f"{prefix}{transform}")
        if node not in nodes:
            continue
        names = mesh_slots.get(nodes[node].get("resourceId"), ())
        for index, source in enumerate(renderer["materials"]):
            slot = f"unity-slot-{index}"
            guid = source.get("guid")
            if guid and slot in names:
                result.append((node, slot, guid))
    return result


def mounted_assets(assets):
    assets = assets.resolve(strict=True)
    mounted = {}
    for sidecar in assets.rglob("*.azmeta"):
        item = json.loads(sidecar.read_bytes())
        source = sidecar.with_suffix("")
        if not source.exists():
            continue
        path = source.resolve(strict=True)
        if not path.is_relative_to(assets) or item["id"] in mounted:
            raise ValueError(f"Duplicate or external mounted asset: {sidecar}")
        mounted[item["id"]] = path
    return mounted


def mesh_slots(assets, level, mounted):
    result = {}
    for resource in level["resources"]:
        asset = resource["asset"]
        if asset.startswith("assets:/"):
            path = (assets / asset.removeprefix("assets:/")).resolve(strict=True)
        else:
            path = mounted[asset]
        if path.suffix.lower() != ".glb":
            continue
        if not path.is_relative_to(assets):
            raise ValueError(f"Mesh slot source escapes the room: {asset}")
        document, _ = parse_glb(path)
        result[resource["id"]] = {material.get("name") for material in document.get("materials", ())}
    return result


def stage_textures(assets, folder, image_root, image_slots, textures, mounted):
    images = {}
    for guid in sorted(textures):
        image = image_slots[guid]
        source = (image_root / image["media"]).resolve(strict=True)
        if source.parent != image_root / "media" or source.suffix != ".png" or digest(source) != image["sha256"]:
            raise ValueError(f"Main texture identity differs: {guid}")
        uuid = str(UUID(hex=guid))
        destination = mounted.get(uuid)
        if destination is None:
            destination = folder / (guid + ".png")
            os.link(source, destination)
            write(destination.with_suffix(".png.azmeta"), {"schemaVersion": 1, "id": uuid,
                  "settings": {"retainSourceSnapshot": False}, "dependencies": []})
        elif digest(destination) != image["sha256"]:
            raise ValueError(f"Existing main texture has changed: {guid}")
        images[guid] = {"asset": uuid, "sha256": image["sha256"], "path": destination.relative_to(assets).as_posix()}
    return images


def stage_surfaces(source, room, output, catalog, images):
    scene_path = source / ROOM_SCENE
    scene = UnityScene(scene_path)
    transforms = {scene.read(identity)["m_GameObject"]["fileID"]: identity
                  for identity, row in scene.documents.items() if row[0] == 4}
    materials = json.loads(catalog.read_bytes())
    image_slots = json.loads(images.read_bytes())["slots"]
    with tempfile.TemporaryDirectory(prefix="room-surfaces-", dir=output.parent) as parent:
        project = Path(parent) / "project"
        assets = project / "assets"
        shutil.copytree(room / "assets", assets, copy_function=copy_room_asset, ignore=shutil.ignore_patterns(".azure"))
        for item in room.glob("*"):
            if item.is_file() and (item.name == "project.azureproject" or item.name.endswith("-receipt.json")):
                shutil.copyfile(item, project / item.name)
        level_path = assets / "room.azurelevel"
        level = json.loads(level_path.read_bytes())
        assets = assets.resolve()
        mounted = mounted_assets(assets)
        nodes = {node["id"]: node for node in level["nodes"]}
        slots = material_bindings(level["sceneImports"]["RoomScene"]["nodeMap"], nodes,
                                  materials["rendererBindings"][ROOM_SCENE], transforms,
                                  mesh_slots(assets, level, mounted))
        selected, pending, textures = {}, {}, set()
        folder = assets / "room-surfaces"
        folder.mkdir()
        for guid in sorted({guid for _, _, guid in slots}):
            material = materials["slots"].get(guid)
            if material is None or material["shader"].get("source") != TOON_SHADER:
                continue
            try:
                native, texture = base_surface(material, image_slots)
            except ValueError as error:
                pending[guid] = str(error)
                continue
            path = folder / (guid + ".azurematerial")
            write(path, native)
            dependencies = [str(UUID(hex=texture))] if texture else []
            staging_metadata(path, guid, dependencies)
            selected[guid] = {"asset": str(UUID(hex=guid)), "source": material["source"],
                              "texture": texture, "sha256": digest(path)}
            if texture:
                textures.add(texture)
        applied = []
        for node, slot, guid in slots:
            if guid not in selected:
                continue
            variant = nodes[node].setdefault("materialSlots", {})
            identity = f"name:{slot}"
            if identity in variant and variant[identity] != selected[guid]["asset"]:
                raise ValueError(f"Room already has a distinct assignment for {node}: {identity}")
            variant[identity] = selected[guid]["asset"]
            applied.append({"node": node, "slot": identity, "guid": guid})
        if not applied:
            raise ValueError("The imported room requires mapped source material assignments")
        image_receipt = stage_textures(assets, folder, images.resolve().parent, image_slots, textures, mounted)
        transparency = None
        if "unity-8186" in level["sceneImports"]["RoomScene"]["nodeMap"]:
            transparency = install_lens(assets, level, source)
        write(level_path, level)
        report = {"schemaVersion": 1, "sourceSHA256": digest(scene_path), "catalogSHA256": digest(catalog),
                  "assignments": applied, "materials": selected, "textures": image_receipt, "pending": pending,
                  "transparency": transparency}
        write(project / "room-surface-receipt.json", report)
        publish_directory(project, output)
    return {name: len(report[name]) for name in ("assignments", "materials", "textures", "pending")}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for argument in ("source", "room", "output", "catalog", "images"):
        parser.add_argument("--" + argument, type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(stage_surfaces(args.source, args.room, args.output, args.catalog, args.images)))
