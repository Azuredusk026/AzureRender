"""Decode observed original weather globals and renderer world transforms."""

import math
import numpy as np

from tools.npr_room.original_room_frames import verify_weather_globals
from tools.npr_room.scene_catalog import local_matrix
from tools.npr_room.windows_weather_probe import (
    COMMON_COLORS, CLOUD_COLORS, FOG_COLORS, COMMON_SCALARS, CLOUD_SCALARS, FOG_SCALARS,
)


def captured_fields(record, role):
    if role not in ("cozy-cloud", "fog"):
        raise ValueError("Captured weather requires a cloud or fog program")
    verify_weather_globals(record)
    scalars = {row["name"]: row["value"] for row in record["weatherScalars"]}
    vectors = {row["name"]: row["value"] for row in record["weatherVectors"]}
    rows = record["shaderValues"]
    colors = {row["name"].removesuffix("ID"): row["value"] for row in rows}
    if len(colors) != len(rows):
        raise ValueError("Captured shader color identities are duplicated")
    if scalars["_UnderwaterRenderingEnabled"] != 0 or scalars["_FullySubmerged"] != 0:
        raise ValueError("Captured underwater rendering requires its source mask texture")
    cloud = role == "cozy-cloud"
    fields = {name: (scalars[name],) for name in COMMON_SCALARS | (CLOUD_SCALARS if cloud else FOG_SCALARS)}
    for name in COMMON_COLORS | (CLOUD_COLORS if cloud else FOG_COLORS):
        if name not in colors:
            raise ValueError("Missing captured shader color: " + name)
        fields[name] = tuple(colors[name][axis] for axis in "xyzw")
    if "CZY_SunDirection" not in colors:
        raise ValueError("Missing captured sun direction")
    fields["CZY_SunDirection"] = tuple(colors["CZY_SunDirection"][axis] for axis in "xyz")
    fields["_WorldSpaceCameraPos"] = tuple(record["position"][axis] for axis in "xyz")
    dimensions = {"_TimeParameters": 4, "CZY_MoonDirection": 3}
    if cloud:
        dimensions.update(CZY_StormDirection=3, CZY_AltocumulusWindSpeed=2)
    else:
        dimensions.update(CZY_VariationWindDirection=3, _GlobalMipBias=2)
        fields["CZY_FogDepthMultiplier"] = (scalars["CZY_FogDepthMultiplier"],)
    for name, dimension in dimensions.items():
        fields[name] = tuple(vectors[name][axis] for axis in "xyzw"[:dimension])
    for name in ("_UnderwaterRenderingEnabled", "_FullySubmerged"):
        fields[name] = (scalars[name],)
    if not all(math.isfinite(value) for values in fields.values() for value in values):
        raise ValueError("Captured weather fields must be finite")
    return fields


def captured_model(record, shader):
    renderers = [renderer for renderer in record["renderers"] if renderer["enabled"]
                 and any(material["shader"] == shader for material in renderer["materials"])]
    if len(renderers) != 1:
        raise ValueError("Captured weather shader requires one active renderer")
    nodes = {node["id"]: node for node in record["transforms"]}
    if len(nodes) != len(record["transforms"]):
        raise ValueError("Captured weather transform identities are duplicated")
    matrix = np.eye(4)
    identity = renderers[0]["transformId"]
    visited = set()
    while identity:
        if identity in visited or identity not in nodes:
            raise ValueError("Captured weather transform ancestry is incomplete or cyclic")
        visited.add(identity)
        node = nodes[identity]
        data = {"m_Local" + name: node[key] for name, key in (
            ("Position", "position"), ("Rotation", "rotation"), ("Scale", "scale"))}
        rotation = node["rotation"]
        if not math.hypot(*(rotation[axis] for axis in "xyzw")) > 0:
            raise ValueError("Captured weather rotation requires a nonzero quaternion")
        matrix = np.asarray(local_matrix(data)).reshape(4, 4, order="F") @ matrix
        identity = node["parentId"]
    if not np.isfinite(matrix).all():
        raise ValueError("Captured weather transform must be finite")
    mirror = np.diag([1, 1, -1, 1])
    return mirror @ matrix @ mirror
