"""Read Unity room lights and attach the authored Cloudy emitters to the room."""

import argparse
from collections import Counter
from functools import lru_cache
import json
import math
from pathlib import Path
import shutil
import tempfile

import numpy as np

from tools.npr_room.catalog_storage import publish_directory
from tools.npr_room.pbr_materials import write
from tools.npr_room.reference_inventory import digest
from tools.npr_room.room_geometry import copy_room_asset
from tools.npr_room.scene_catalog import local_matrix
from tools.npr_room.skinned_mesh_export import UnityScene
from tools.npr_room.window_scene_inventory import ROOM_SCENE


KINDS = {0: "spot", 1: "directional", 2: "point"}


def light_inventory(scene):
    @lru_cache(maxsize=None)
    def hierarchy(identity):
        if not identity:
            return np.eye(4), np.eye(4), True, ()
        data = scene.read(identity)
        entity = scene.read(data["m_GameObject"]["fileID"])
        parent, orientation, active, path = hierarchy(data["m_Father"]["fileID"])
        matrix = np.asarray(local_matrix(data)).reshape((4, 4), order="F")
        rotation = np.asarray(local_matrix(data | {"m_LocalScale": dict.fromkeys("xyz", 1)})).reshape((4, 4), order="F")
        return (parent @ matrix, orientation @ rotation, active and bool(entity["m_IsActive"]),
                (*path, entity["m_Name"]))

    result = []
    for identity, data in scene.documents.items():
        if data[0] != 108:
            continue
        source = scene.read(identity)
        entity_id = source["m_GameObject"]["fileID"]
        entity = scene.read(entity_id)
        transform, = [item["component"]["fileID"] for item in entity["m_Component"]
                      if scene.documents[item["component"]["fileID"]][0] == 4]
        world, orientation, active, path = hierarchy(transform)
        direction = -orientation[:3, 2]
        direction /= np.linalg.norm(direction)
        kind = KINDS.get(source["m_Type"], "unsupported")
        shape = {"type": kind, "direction": direction.tolist()}
        if kind == "spot":
            shape.update(innerCone=math.radians(source["m_InnerSpotAngle"] / 2),
                         outerCone=math.radians(source["m_SpotAngle"] / 2))
        color = source["m_Color"]
        result.append({"fileID": identity, "transform": transform, "gameObject": entity_id, "path": list(path),
                       "color": [color[channel] for channel in "rgb"], "position": world[:3, 3].tolist(),
                       "sourceType": source["m_Type"], "kind": kind, "direction": direction.tolist(), "shape": shape,
                       "intensity": source["m_Intensity"], "range": source["m_Range"], "activeHierarchy": active,
                       "enabled": bool(source["m_Enabled"]), "cullingMask": source["m_CullingMask"]["m_Bits"],
                       "renderingLayers": source.get("m_RenderingLayerMask", 1),
                       "lightmapping": source.get("m_Lightmapping"), "shadow": source.get("m_Shadows"),
                       "usesTemperature": bool(source.get("m_UseColorTemperature", 0))})
    return result


def cloud_lights(lights):
    return [row for row in lights if "Cloudy" in row["path"] and row["enabled"] and row["activeHierarchy"]
            and row["cullingMask"] == 0xFFFFFFFF and row["renderingLayers"] == 1 and row["intensity"] > 0]


def attach_lights(level, lights):
    if not lights or any(row["kind"] not in KINDS.values() or row["usesTemperature"] for row in lights):
        raise ValueError("Expected supported authored Cloudy lights")
    target = level.setdefault("lights", [])
    existing = {node["id"] for node in level["nodes"]}
    for entry in lights:
        identity = f"unity-light-{entry['fileID']}"
        if identity in existing:
            raise ValueError(f"Duplicate room light {identity}")
        level["nodes"].append({"id": identity, "name": entry["path"][-1], "components": {
            "azure.transform": {"type": "azure.transform", "version": 1,
                                "data": {"translation": entry["position"]}},
        }})
        shape = dict(entry["shape"])
        if entry["kind"] != "directional":
            shape["falloff"] = "smooth-inverse-square"
        target.append({"id": identity, "nodeId": identity, "shape": shape, "color": entry["color"],
                       "intensity": entry["intensity"], "radius": entry["range"], "enabled": True})


def stage_cloudy(source, room, output):
    scene_file = source / ROOM_SCENE
    inventory = light_inventory(UnityScene(scene_file))
    selected = cloud_lights(inventory)
    with tempfile.TemporaryDirectory(prefix="room-cloudy-", dir=output.parent) as parent:
        project = Path(parent) / "project"
        assets = project / "assets"
        shutil.copytree(room / "assets", assets, copy_function=copy_room_asset, ignore=shutil.ignore_patterns(".azure"))
        for item in room.glob("*"):
            if item.is_file() and (item.name == "project.azureproject" or item.name.endswith("-receipt.json")):
                shutil.copyfile(item, project / item.name)
        room_level = assets / "room.azurelevel"
        level = json.loads(room_level.read_bytes())
        attach_lights(level, selected)
        write(room_level, level)
        report = {"schemaVersion": 1, "sceneSHA256": digest(scene_file), "sourceLights": inventory,
                  "projectLights": level["lights"], "selected": [row["fileID"] for row in selected]}
        write(project / "cloudy-light-receipt.json", report)
        publish_directory(project, output)
    return {"sourceLights": len(inventory), "selected": report["selected"],
            "types": dict(Counter(row["kind"] for row in selected))}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for argument in ("source", "room", "output"):
        parser.add_argument("--" + argument, type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(stage_cloudy(args.source, args.room, args.output)))
