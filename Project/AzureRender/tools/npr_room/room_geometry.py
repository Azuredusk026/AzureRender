"""Assemble the imported room with its textured furniture and gameplay nodes."""

import copy
import json
import os
from pathlib import Path
import shutil


FURNITURE_TRANSFORMS = (7570, 7993)
EXCHANGE_DOCUMENT = "RoomScene-WithCamera.azuresceneexchange"


def merge_room_geometry(furniture, imported_room):
    room = copy.deepcopy(imported_room)
    room["id"] = furniture["id"]
    furniture_map = furniture["sceneImports"]["recovery-furniture"]["nodeMap"]
    room_map = room["sceneImports"]["RoomScene"]["nodeMap"]
    furniture_nodes = {node["id"]: node for node in furniture["nodes"]}
    room_nodes = {node["id"]: node for node in room["nodes"]}
    furniture_assets = {resource["id"]: resource["asset"] for resource in furniture["resources"]}
    room_assets = {resource["id"]: resource["asset"] for resource in room["resources"]}
    replacements = {}
    for transform in FURNITURE_TRANSFORMS:
        source = furniture_nodes[furniture_map[f"transform-{transform}"]]
        target = room_nodes[room_map[f"unity-{transform}"]]
        destination_asset = room_assets[target["resourceId"]]
        if destination_asset in replacements:
            raise ValueError("Imported furniture slots must identify distinct meshes")
        replacements[destination_asset] = furniture_assets[source["resourceId"]]
        if "azure.animator" in source["components"]:
            target["components"]["azure.animator"] = copy.deepcopy(source["components"]["azure.animator"])

    imported_furniture = set(furniture_map.values())
    for node_id, node in furniture_nodes.items():
        if node_id not in imported_furniture:
            if node_id in room_nodes:
                raise ValueError(f"Room gameplay node collides with a geometry node: {node_id}")
            room["nodes"].append(copy.deepcopy(node))
    return room, replacements


def mesh_paths_by_id(folder):
    paths = {}
    for metadata in folder.rglob("*.glb.azmeta"):
        asset_id = json.loads(metadata.read_bytes())["id"]
        if asset_id in paths:
            raise ValueError(f"Duplicate model identity: {asset_id}")
        paths[asset_id] = metadata.with_suffix("").resolve(strict=True)
    return paths


def install_room_geometry(geometry, furniture_assets, destination, replacements):
    original_paths = mesh_paths_by_id(furniture_assets)
    geometry_paths = mesh_paths_by_id(geometry)
    swap_media = {geometry_paths[target]: original_paths[source] for target, source in replacements.items()}

    def install_file(source, target):
        source = Path(source)
        if source.suffix == ".glb":
            os.link(swap_media.get(source.resolve(), source), target)
        else:
            shutil.copyfile(source, target)

    for name in ("batches", "builtins", "meshes", "pbr-materials"):
        shutil.copytree(geometry / name, destination / name, copy_function=install_file)
    for name in (EXCHANGE_DOCUMENT, EXCHANGE_DOCUMENT + ".azmeta"):
        shutil.copyfile(geometry / name, destination / name)
