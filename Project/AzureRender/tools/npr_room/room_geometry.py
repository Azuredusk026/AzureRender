"""Assemble the imported room with its textured furniture and gameplay nodes."""

import copy
from hashlib import file_digest
import json
import os
from pathlib import Path
import shutil


FURNITURE_TRANSFORMS = (7570, 7993)
EXCHANGE_DOCUMENT = "RoomScene-WithCamera.azuresceneexchange"
HEROINE_PARTS = ("face", "body", "hair", "top", "bottom", "socks", "jacket")
NOTE_ACTIONS = ("toggle", "prev", "next", "new", "save", "up", "down", "delete")
TODO_ACTIONS = ("toggle", "save", *(f"list-{name}" for name in ("prev", "next", "new", "delete")),
                *(f"task-{name}" for name in ("prev", "next", "new", "up", "down", "complete", "delete")))


def activate_room_control(project, level, name, actions):
    project["runtime"]["input"].update({f"{name}-{action}": [] for action in actions})
    node_id = f"room-{name}"
    if any(node["id"] == node_id for node in level["nodes"]):
        raise ValueError(f"Room already has a controller: {name}")
    level["nodes"].append({"id": node_id, "components": {
        "azure.script": {"type": "azure.script", "version": 1, "data": {"asset": f"assets:/{name}.lua"}},
    }})


def ui_font_document(rml, filename):
    stylesheet = '@font-face { font-family: "Room UI Fallback"; src: "' + filename + '"; -rmlui-fallback-face: true; }'
    return rml.replace("<style>", "<style>\n" + stylesheet, 1)


def install_todo_audio(source, playback, assets, level):
    original = source / "Assets/App/BulbulAssets/Audio/SE/bulbul_systemse_completetask.wav"
    with original.open("rb") as stream:
        sha = file_digest(stream, "sha256").hexdigest()
    playback = playback.resolve(strict=True)
    repairs = json.loads((playback / "audio-repairs.json").read_bytes())["repairs"]
    entry = repairs["media/" + sha + ".wav"]
    media = (playback / entry["media"]).resolve(strict=True)
    if media.parent != playback or sha != entry["sourceSHA256"]:
        raise ValueError("Task completion audio source identity differs")
    with media.open("rb") as stream:
        if file_digest(stream, "sha256").hexdigest() != entry["sha256"]:
            raise ValueError("Task completion audio playback identity differs")
    filename = "todo-complete.wav"
    os.link(media, assets / filename)
    level["nodes"].append({"id": "todo-complete", "components": {
        "azure.audio-source": {"type": "azure.audio-source", "version": 1,
                               "data": {"asset": "assets:/" + filename, "volume": 1}},
    }})


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


def install_heroine_motion(source, assets, level):
    root = source / "assets"
    target = assets / "heroine"
    target.mkdir()
    graph = "humanoid-motion.json"
    shutil.copyfile(root / graph, target / graph)
    existing = {node["id"] for node in level["nodes"]} | {resource["id"] for resource in level["resources"]}
    for part in HEROINE_PARTS:
        node_id = f"heroine-{part}"
        if node_id in existing:
            raise ValueError(f"Heroine slot collides with a room identity: {node_id}")
        filename = f"{part}.glb"
        os.link(root / filename, target / filename)
        level["resources"].append({"id": node_id, "asset": f"assets:/heroine/{filename}"})
        level["nodes"].append({
            "id": node_id, "resourceId": node_id,
            "components": {"azure.animator": {
                "type": "azure.animator", "version": 1, "data": {"asset": f"assets:/heroine/{graph}", "state": "idle"},
            }},
        })
