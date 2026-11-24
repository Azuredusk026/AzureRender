"""Mount the five source book arrangements in the normal opening room."""

import argparse
import json
import math
import os
from pathlib import Path
import shutil
import tempfile
from uuid import UUID

from tools.build_face_sdf import parse_glb
from tools.npr_room.build_city_window import json_file, verified_path
from tools.npr_room.build_story_room import EXAMPLE
from tools.npr_room.catalog_storage import publish_directory
from tools.npr_room.pbr_materials import staging_metadata, write
from tools.npr_room.reference_inventory import digest
from tools.npr_room.room_geometry import copy_room_asset
from tools.npr_room.room_surfaces import base_surface, mounted_assets, stage_textures
from tools.npr_room.skinned_mesh_export import UnityScene
from tools.npr_room.story_chapters import lua_value
from tools.npr_room.window_scene_inventory import ROOM_SCENE


MASTER = "Assets/App/BulbulAssets/MasterData/ScriptableObjects/DecorationMaster.asset"
CHANGER = "999a07457df10f91a280292a3814131d"
FACTORY = "local function new_room_opening(roomLanguage, level)"
LANGUAGE = "function() return roomLanguage:text(opening_tags) end, level)"
START = "function new_story_room("


def source_layouts(scene, master):
    model, = (row for row in master["DecorationModels"] if row["CategoryType"] == 4)
    skins = [row for row in master["DecorationSkins"] if row["ModelType"] == model["ModelType"]]
    if [row["SkinType"] for row in skins] != list(range(7, 12)):
        raise ValueError("Expected the five source book skins")
    default, = (f"Book_{row['SkinType'] - 6}" for row in skins if row["IsCategoryDefault"])
    changer, = (identity for identity in scene.documents
                if scene.documents[identity][0] == 114
                and scene.read(identity).get("m_Script", {}).get("guid") == CHANGER
                and scene.read(identity).get("_categoryType") == 4)
    data = scene.read(changer)
    if data["_modelObjs"]["m_list"]:
        raise ValueError("Book layout includes unhandled common model roots")
    groups = {row["Key"]: row["Value"] for row in data["_skinObjs"]["m_list"]}
    if set(groups) != {row["SkinType"] for row in skins}:
        raise ValueError("Book master and room layouts differ")
    return changer, default, {f"Book_{key - 6}": [item["fileID"] for item in groups[key]]
                              for key in sorted(groups)}


def stage_geometry(source, catalog, assets, level, scene, groups, default):
    mesh_root = catalog / "static-mesh-catalog"
    image_root = catalog / "image-catalog"
    meshes = json_file(mesh_root / "mesh-catalog.json")["slots"]
    images = json_file(image_root / "image-catalog.json")["slots"]
    material_catalog = json_file(catalog / "source-lock/material-catalog.json")
    folder = assets / "story-books"
    folder.mkdir()
    mounted = mounted_assets(assets)
    nodes = {node["id"]: node for node in level["nodes"]}
    scene_map = level["sceneImports"]["RoomScene"]["nodeMap"]
    mounted_resources = {asset["id"]: asset["asset"] for asset in level["resources"]}
    paths, applied, textures, materials, resources = {}, {}, set(), {}, {}

    def mount_transform(identity):
        key = f"unity-{identity}"
        if key in scene_map:
            return nodes[scene_map[key]]
        if identity not in paths:
            transform = scene.read(identity)
            rotation = transform["m_LocalRotation"]
            if rotation["x"] or rotation["z"]:
                raise ValueError(f"Expected yaw-only book transform {identity}")
            parent = mount_transform(transform["m_Father"]["fileID"])
            location, scale = transform["m_LocalPosition"], transform["m_LocalScale"]
            node = {"id": f"story-books-{identity}", "parentId": parent["id"],
                    "name": scene.read(transform["m_GameObject"]["fileID"])["m_Name"],
                    "components": {"azure.transform": {"type": "azure.transform", "version": 1, "data": {
                        "translation": [location["x"], location["y"], -location["z"]],
                        "rotation": [0, -math.degrees(2 * math.atan2(rotation["y"], rotation["w"])), 0],
                        "scale": [scale[axis] for axis in "xyz"],
                    }}}}
            level["nodes"].append(node)
            paths[identity] = node
        return paths[identity]

    for identity in sorted({item for members in groups.values() for item in members}):
        game_object = scene.read(identity)
        components = [(scene.documents[c["component"]["fileID"]][0], c["component"]["fileID"])
                      for c in game_object["m_Component"]]
        transform, = (key for kind, key in components if kind == 4)
        mesh_filter, = (key for kind, key in components if kind == 33)
        renderer, = (key for kind, key in components if kind == 23)
        binding = material_catalog["rendererBindings"][ROOM_SCENE][str(renderer)]
        if scene.read(transform)["m_Children"] or not binding["enabled"] or binding["staticBatch"]["subMeshCount"]:
            raise ValueError(f"Expected a single enabled book renderer: {identity}")
        mesh_guid = scene.read(mesh_filter)["m_Mesh"]["guid"]
        mesh = meshes[mesh_guid]
        asset = verified_path(mesh_root, mesh)
        if digest(source / mesh["source"]) != mesh["sourceSHA256"]:
            raise ValueError(f"Book mesh identity mismatch: {mesh['source']}")
        document, _ = parse_glb(asset)
        slots = {entry["name"] for entry in document["materials"]}
        material_guids = {}
        for index, entry in enumerate(binding["materials"]):
            guid = entry["guid"]
            name = f"unity-slot-{index}"
            material_guids[name] = guid
            if guid in materials:
                continue
            material = material_catalog["slots"][guid]
            if digest(source / material["source"]) != material["sourceSHA256"]:
                raise ValueError(f"Book material identity mismatch: {material['source']}")
            document, image = base_surface(material, images)
            if image:
                textures.add(image)
            uid = str(UUID(hex=guid))
            path = mounted.get(uid)
            if path is None:
                path = folder / (guid + ".azurematerial")
                write(path, document)
                staging_metadata(path, guid, [str(UUID(hex=image))] if image else [])
            elif json_file(path) != document:
                raise ValueError(f"Mounted book material differs: {guid}")
            materials[guid] = uid
        if slots != material_guids.keys():
            raise ValueError(f"Book geometry and renderer slots differ: {identity}")
        node = mount_transform(transform)
        component = node["components"].setdefault("azure.renderable", {
            "type": "azure.renderable", "version": 1, "data": {"primitiveIndex": 0}})
        component["data"]["visible"] = identity in groups[default]
        bindings = node.setdefault("materialSlots", {})
        for name, guid in material_guids.items():
            if bindings.setdefault(f"name:{name}", materials[guid]) != materials[guid]:
                raise ValueError(f"Mounted book slot differs: {identity}, {name}")
        if "resourceId" in node:
            ref = mounted_resources[node["resourceId"]]
            mounted_path = assets / ref.removeprefix("assets:/") if ref.startswith("assets:/") else mounted[ref]
            if digest(mounted_path) != mesh["sha256"]:
                raise ValueError(f"Mounted book geometry differs: {identity}")
        else:
            if mesh_guid not in resources:
                path = folder / asset.name
                os.link(asset, path)
                resource = "story-books-" + mesh_guid
                level["resources"].append({"id": resource, "asset": f"assets:/story-books/{path.name}"})
                resources[mesh_guid] = resource
            node["resourceId"] = resources[mesh_guid]
        applied[identity] = {"node": node["id"], "transform": transform, "renderer": renderer,
                             "geometrySHA256": mesh["sha256"], "materials": material_guids}
    image_receipt = stage_textures(assets, folder, image_root, images, textures, mounted)
    layouts = {name: [applied[item]["node"] for item in members] for name, members in groups.items()}
    return layouts, applied, image_receipt


def integrate_timer(timer, layouts, default, unlock_group):
    if timer.count(START) != 1 or timer.count(FACTORY) != 1 or timer.count(LANGUAGE) != 1:
        raise ValueError("Expected the normal opening presentation and factory")
    begin, end = timer.index(START), timer.index(FACTORY)
    program = (EXAMPLE / "story-book-layout.lua").read_text(encoding="utf-8")
    presentation = (EXAMPLE / "story-room.lua").read_text(encoding="utf-8")
    config = "local story_book_layouts = " + lua_value(layouts)
    books = f"new_story_book_layout(self, story_book_layouts, {lua_value(default)}, {lua_value(unlock_group)}, 1)"
    factory = timer[end:].replace(LANGUAGE, LANGUAGE[:-1] + ", " + books + ")")
    return timer[:begin] + "\n".join((program, presentation, config, factory))


def skip_capture_orphans(_, names):
    names = set(names)
    return {name for name in names if name == ".azure" or (
        name.startswith("capture-") and name.endswith(".azmeta") and name.removesuffix(".azmeta") not in names)}


def stage(source, catalog, room, output):
    lock = catalog / "source-lock"
    windows = json_file(lock / "window-scenes.json")
    scene_path, master_path = source / ROOM_SCENE, source / MASTER
    if digest(scene_path) != windows["roomSceneSHA256"]:
        raise ValueError("Book layouts require the locked room scene")
    unlocks = json_file(lock / "unlock-catalog.json")
    book_five = unlocks["decoration"]["Book_5"]
    purchase = json_file(room / "purchase-room-receipt.json")
    if book_five["policy"] != "scenario" or purchase["sourceUnlocksSHA256"] != digest(lock / "unlock-catalog.json"):
        raise ValueError("Book layout requires the same scenario unlock source")
    unlock_group = book_five["scenario"]
    scene = UnityScene(scene_path)
    changer, default, groups = source_layouts(scene, UnityScene(master_path).read(11400000))
    with tempfile.TemporaryDirectory(prefix="story-books-", dir=output.parent) as temporary:
        project = Path(temporary) / "project"
        assets = project / "assets"
        shutil.copytree(room / "assets", assets, copy_function=copy_room_asset, ignore=skip_capture_orphans)
        for path in [room / "project.azureproject", *room.glob("*-receipt.json")]:
            shutil.copyfile(path, project / path.name)
        level_path, script_path = assets / "room.azurelevel", assets / "pomodoro.lua"
        level = json_file(level_path)
        layouts, geometry, textures = stage_geometry(source, catalog, assets, level, scene, groups, default)
        script = integrate_timer(script_path.read_text(encoding="utf-8"), layouts, default, unlock_group)
        script_path.write_text(script, encoding="utf-8", newline="\n")
        write(level_path, level)
        story = json_file(project / "opening-story-room-receipt.json")
        story.update(levelSHA256=digest(level_path), scriptSHA256=digest(script_path),
                     documentSHA256=digest(assets / "timer.rml"))
        write(project / "opening-story-room-receipt.json", story)
        report = {"schemaVersion": 1, "changer": changer, "default": default, "unlockGroup": unlock_group,
                  "sceneSHA256": windows["roomSceneSHA256"], "masterSHA256": digest(master_path),
                  "sourceGroups": groups, "layouts": layouts, "geometry": geometry, "textures": textures,
                  "levelSHA256": story["levelSHA256"], "scriptSHA256": story["scriptSHA256"]}
        write(project / "story-book-layout-receipt.json", report)
        publish_directory(project, output)
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for arg in ("source", "catalog", "room", "output"):
        parser.add_argument("--" + arg, type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(stage(args.source, args.catalog, args.room, args.output)))
