"""Assemble the original City mesh, albedo, and emissive map in the room."""

import argparse
import json
import math
import os
from pathlib import Path
import shutil
import tempfile

from tools.build_face_sdf import parse_glb, write_glb
from tools.npr_room.catalog_storage import publish_directory
from tools.npr_room.pbr_materials import write
from tools.npr_room.reference_inventory import digest
from tools.npr_room.skinned_mesh_export import UnityScene
from tools.npr_room.window_scene_inventory import ROOM_SCENE


CITY_CASE = "20"
CITY_RESOURCE = "window-city"
CITY_ASSET = "assets:/window-city.glb"
GLASS_NODES = ("unity-7858", "unity-7230")
TRANSITION_NODE = "unity-8445"
GLASS_MATERIAL = "0b33036dd8fc0e84ea4ae8a1ca30cc10"
TRANSITION_MATERIAL = "0c944552201eddb4987bd7f5967bc5c5"


def json_file(path):
    return json.loads(path.read_bytes())


def verified_path(root, slot):
    path = (root / slot["media"]).resolve(strict=True)
    if not path.is_relative_to(root.resolve()) or digest(path) != slot["sha256"]:
        raise ValueError(f"Asset identity mismatch: {slot['media']}")
    return path


def bake_city_material(mesh, material, images, output):
    document, geometry = parse_glb(mesh)
    slot, = document["materials"]
    properties = material["properties"]
    floats, colors, maps = (properties[key] for key in ("m_Floats", "m_Colors", "m_TexEnvs"))
    if (material["shader"]["source"] != "Assets/Shader/lilToon.shader" or material["parent"]["fileID"]
            or floats["_Cull"] != 2 or floats["_UseEmission"] != 1 or floats["_UseEmission2nd"] != 0
            or floats["_EmissionBlend"] != 1 or floats["_EmissionBlendMode"] != 1):
        raise ValueError("City source material does not match the supported base and additive emission inputs")
    rgba = [colors["_Color"][key] for key in ("r", "g", "b", "a")]
    emission = [colors["_EmissionColor"][key] for key in ("r", "g", "b")]
    strength = max(emission)
    if rgba[3] != 1 or not all(math.isfinite(v) and v >= 0 for v in rgba + emission) or strength <= 0:
        raise ValueError("Invalid City material color")
    binary = bytearray(geometry[:document["buffers"][0]["byteLength"]])
    document["images"], document["textures"] = [], []
    source_maps = {}
    for name in ("_MainTex", "_EmissionMap"):
        reference = maps[name]
        image = images[reference["m_Texture"]["guid"]]
        if reference["m_Scale"] != {"x": 1, "y": 1} or reference["m_Offset"] != {"x": 0, "y": 0}:
            raise ValueError(f"Unsupported source UV transform: {name}")
        if not image["srgb"] or not image["media"].endswith(".png"):
            raise ValueError(f"Expected original sRGB PNG: {name}")
        payload = image["payload"]
        binary.extend(b"\0" * (-len(binary) % 4))
        view = len(document["bufferViews"])
        document["bufferViews"].append({"buffer": 0, "byteOffset": len(binary), "byteLength": len(payload)})
        binary.extend(payload)
        document["textures"].append({"source": len(document["images"])})
        document["images"].append({"bufferView": view, "mimeType": "image/png"})
        source_maps[name] = image["sha256"]
    slot.update({
        "pbrMetallicRoughness": {"baseColorFactor": rgba, "baseColorTexture": {"index": 0},
                                 "metallicFactor": 0, "roughnessFactor": 1},
        "emissiveTexture": {"index": 1}, "emissiveFactor": [value / strength for value in emission],
        "extensions": {"KHR_materials_emissive_strength": {"emissiveStrength": strength}},
    })
    document["extensionsUsed"] = ["KHR_materials_emissive_strength"]
    document["buffers"][0]["byteLength"] = len(binary)
    write_glb(output, document, bytes(binary))
    return source_maps


def append_city(scene, level, transform_id):
    def add(identity):
        transform = scene.read(identity)
        parent = transform["m_Father"]["fileID"]
        node_id = f"{CITY_RESOURCE}-{identity}"
        location = transform["m_LocalPosition"]
        rotation = transform["m_LocalRotation"]
        scale = transform["m_LocalScale"]
        if rotation["x"] or rotation["z"]:
            raise ValueError(f"City transform requires a yaw-only rotation: {identity}")
        yaw = -math.degrees(2 * math.atan2(rotation["y"], rotation["w"]))
        node = {"id": node_id, "name": scene.read(transform["m_GameObject"]["fileID"])["m_Name"],
                "components": {"azure.transform": {"type": "azure.transform", "version": 1, "data": {
                    "translation": [location["x"], location["y"], -location["z"]],
                    "rotation": [0, yaw, 0],
                    "scale": [scale[axis] for axis in "xyz"],
                }}}}
        if parent:
            node["parentId"] = add(parent)
        if identity == transform_id:
            node["resourceId"] = CITY_RESOURCE
        level["nodes"].append(node)
        return node_id

    if (any(node["id"].startswith(CITY_RESOURCE + "-") for node in level["nodes"])
            or any(entry["id"] == CITY_RESOURCE for entry in level["resources"])):
        raise ValueError("Room already has a City window")
    level["resources"].append({"id": CITY_RESOURCE, "asset": CITY_ASSET})
    return add(transform_id)


def apply_steady_window(level, materials):
    glass = materials[GLASS_MATERIAL]["properties"]["m_Colors"]["_Color"]
    transition = materials[TRANSITION_MATERIAL]["properties"]["m_Floats"]
    if glass["a"] != 0 or transition["_Threshold"] != 0:
        raise ValueError("Expected clear glass and steady window transition in the captured source state")
    root = level["sceneImports"]["RoomScene"]["nodeMap"]
    nodes = {entry["id"]: entry for entry in level["nodes"]}
    # The static room fallback is opaque. Suppress it while the transparent glass
    # reflection and transition effects are implemented through their own profiles.
    for source in (*GLASS_NODES, TRANSITION_NODE):
        nodes[root[source]]["components"]["azure.renderable"]["data"]["visible"] = False


def stage_city(source, catalog, room, output):
    scene_path = source / ROOM_SCENE
    windows = json_file(catalog / "source-lock/window-scenes.json")
    if digest(scene_path) != windows["roomSceneSHA256"]:
        raise ValueError("City requires the locked RoomScene")
    window = windows["cases"][CITY_CASE]
    renderer, = [row for row in window["renderers"] if row["enabled"] and row["activeWhenSelected"]]
    if window["name"] != "City" or renderer["kind"] != "mesh":
        raise ValueError("Expected the original City mesh renderer")
    mesh_root = catalog / "static-mesh-catalog"
    mesh = json_file(mesh_root / "mesh-catalog.json")["slots"][renderer["mesh"]["guid"]]
    material_ref, = renderer["materials"]
    materials = json_file(catalog / "source-lock/material-catalog.json")["slots"]
    material = materials[material_ref["guid"]]
    if digest(source / mesh["source"]) != mesh["sourceSHA256"]:
        raise ValueError("City mesh differs from the locked source")
    for entry in (material, materials[GLASS_MATERIAL], materials[TRANSITION_MATERIAL]):
        if digest(source / entry["source"]) != entry["sourceSHA256"]:
            raise ValueError(f"City material differs from the locked source: {entry['source']}")
    image_root = catalog / "image-catalog"
    image_slots = json_file(image_root / "image-catalog.json")["slots"]
    images = {}
    for name in ("_MainTex", "_EmissionMap"):
        guid = material["properties"]["m_TexEnvs"][name]["m_Texture"]["guid"]
        slot = image_slots[guid]
        images[guid] = {**slot, "payload": verified_path(image_root, slot).read_bytes()}

    def copy_asset(path, target):
        return os.link(path, target) if Path(path).suffix.lower() == ".glb" else shutil.copyfile(path, target)

    with tempfile.TemporaryDirectory(prefix="city-window-", dir=output.parent) as holder:
        project = Path(holder) / "project"
        project.mkdir()
        shutil.copyfile(room / "project.azureproject", project / "project.azureproject")
        assets = project / "assets"
        shutil.copytree(room / "assets", assets, copy_function=copy_asset)
        asset_path = assets / Path(CITY_ASSET).name
        maps = bake_city_material(verified_path(mesh_root, mesh), material, images, asset_path)
        level_path = assets / "room.azurelevel"
        level = json_file(level_path)
        node = append_city(UnityScene(scene_path), level, renderer["transform"])
        apply_steady_window(level, materials)
        write(level_path, level)
        report = {
            "formatVersion": 1, "case": int(CITY_CASE), "renderer": renderer["component"],
            "node": node, "sourceMesh": mesh["source"], "meshSHA256": mesh["sha256"], "textures": maps,
            "sourceMaterial": material["source"], "sourceMaterialSHA256": material["sourceSHA256"],
            "steadyWindow": [*GLASS_NODES, TRANSITION_NODE],
            "assetSHA256": digest(asset_path), "roomSHA256": digest(level_path),
        }
        write(project / "city-receipt.json", report)
        publish_directory(project, output)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for arg in ("source", "catalog", "room", "output"):
        parser.add_argument("--" + arg, type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(stage_city(args.source, args.catalog, args.room, args.output), sort_keys=True))


if __name__ == "__main__":
    main()
