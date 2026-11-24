"""Bind observed original light inputs to the isolated room view."""

import math
import numpy as np

from tools.npr_room.room_lights import attach_lights
from tools.npr_room.scene_catalog import local_matrix


def apply_captured_lights(level, record, linear_intensity):
    if type(linear_intensity) is not bool:
        raise ValueError("Captured light conversion requires the frozen graphics color policy")
    previous = level.get("lights", [])
    if any(not light["id"].startswith("unity-light-") for light in previous):
        raise ValueError("The room contains lights outside the source-owned diagnostic set")
    nodes = {node["id"]: node for node in record["transforms"]}
    worlds = {}

    def world(identity):
        if identity not in worlds:
            node = nodes[identity]
            data = {"m_Local" + name: node[key] for name, key in (
                ("Position", "position"), ("Rotation", "rotation"), ("Scale", "scale"))}
            matrix = np.asarray(local_matrix(data)).reshape(4, 4, order="F")
            orientation = np.asarray(local_matrix(data | {"m_LocalScale": dict.fromkeys("xyz", 1)})).reshape(4, 4, order="F")
            if node["parentId"]:
                parent, parent_orientation = world(node["parentId"])
                matrix, orientation = parent @ matrix, parent_orientation @ orientation
            worlds[identity] = matrix, orientation
        return worlds[identity]

    selected = []
    for source in record["lights"]:
        if not source["enabled"]:
            continue
        factors = (source["intensity"], source["range"], source["innerSpotAngle"], source["spotAngle"])
        if not all(math.isfinite(value) and value >= 0 for value in factors):
            raise ValueError("Captured light factors must be finite and nonnegative")
        color = np.array([source["color"][channel] for channel in "rgb"])
        if (not np.isfinite(color).all() or np.any(color < 0) or np.any(color > 1)
                or source["cullingMask"] not in (-1, 0xFFFFFFFF) or source["useColorTemperature"]):
            raise ValueError("Captured light requires normalized colors, common layers and authored temperature")
        if linear_intensity:
            color = np.where(color < .04045, color / 12.92, ((color + .055) / 1.055) ** 2.4)
        matrix, orientation = world(source["transformId"])
        direction = -orientation[:3, 2]
        direction /= np.linalg.norm(direction)
        kind = source["kind"].lower()
        shape = {"type": kind, "direction": direction.tolist()}
        if kind == "spot":
            shape.update(innerCone=math.radians(source["innerSpotAngle"] / 2),
                         outerCone=math.radians(source["spotAngle"] / 2))
        selected.append({"fileID": source["id"], "path": source["path"].split("/"), "position": matrix[:3, 3].tolist(),
                         "shape": shape, "kind": kind, "color": color.tolist(), "intensity": source["intensity"],
                         "range": source["range"], "usesTemperature": source["useColorTemperature"]})
    # Source conversion completes before replacing the owned light nodes.
    remove = {light["nodeId"] for light in previous}
    candidate = {"nodes": [node for node in level["nodes"] if node["id"] not in remove], "lights": []}
    attach_lights(candidate, selected)
    level.update(candidate)
    return {"lights": len(selected), "linearIntensity": linear_intensity,
            "sourceShadowModes": [light["shadows"] for light in record["lights"] if light["enabled"]]}
