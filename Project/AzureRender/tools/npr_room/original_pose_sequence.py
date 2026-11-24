"""Encode captured character transforms and morph weights as a glTF clip."""

import copy
import json
import math
from struct import pack
from urllib.parse import quote
from uuid import NAMESPACE_URL, uuid5

from tools.npr_room.humanoid_clip_export import append_humanoid_clip, bind_source_nodes
from tools.npr_room.original_room_registration import freeze_model, source_rig
from tools.npr_room.skinned_mesh_export import add_accessor


def captured_graph():
    return {"schemaVersion": 1, "initial": "captured",
            "states": [{"name": "captured", "clip": 0, "loop": False}], "transitions": []}


def install_graph(assets, level, parts):
    path = assets / "heroine/original-captured-motion.json"
    if path.exists() or path.with_suffix(".json.azmeta").exists():
        raise FileExistsError(path)
    nodes = []
    for part in parts:
        node, = [node for node in level["nodes"] if node.get("resourceId") == "heroine-" + part]
        nodes.append(node)
    path.write_text(json.dumps(captured_graph()) + "\n", encoding="utf-8")
    path.with_suffix(".json.azmeta").write_text(json.dumps({
        "schemaVersion": 1, "id": str(uuid5(NAMESPACE_URL, "original-captured-motion-graph-v1")),
        "settings": {"retainSourceSnapshot": False}}) + "\n", encoding="utf-8")
    for node in nodes:
        node["components"]["azure.animator"] = {"type": "azure.animator", "version": 1,
            "data": {"asset": "assets:/heroine/original-captured-motion.json", "state": "captured"}}


def append_sequence(document, records, buffer_uri):
    if not 2 <= len(records) <= 12:
        raise ValueError("Captured motion requires two to twelve frames")
    if (not buffer_uri or quote(buffer_uri, safe="") != buffer_uri
            or "/" in buffer_uri or "\\" in buffer_uri):
        raise ValueError("Motion buffer requires an adjacent URI-safe filename")
    if len(document.get("buffers", [])) != 1 or "uri" not in document["buffers"][0]:
        raise ValueError("Captured motion requires a referenced source buffer")
    rig = source_rig(records[0])
    identities = [node["id"] for node in records[0]["transforms"]]
    origin = records[0]["simulationTime"]
    frames = []
    previous_frame = -1
    for record in records:
        if (record["weather"] != records[0]["weather"] or source_rig(record) != rig
                or [node["id"] for node in record["transforms"]] != identities):
            raise ValueError("Captured motion weather or transform hierarchy differs")
        if type(record["frame"]) is not int or record["frame"] <= previous_frame:
            raise ValueError("Captured motion frame numbers must increase")
        previous_frame = record["frame"]
        if not math.isfinite(record["simulationTime"]):
            raise ValueError("Captured motion requires finite simulation time")
        freeze_model(copy.deepcopy(document), record)
        frames.append({"time": record["simulationTime"] - origin,
                       **{field: [node[source] for node in record["transforms"]]
                          for field, source in (("positions", "position"), ("rotations", "rotation"), ("scales", "scale"))}})
    candidate = copy.deepcopy(document)
    freeze_model(candidate, records[0])
    mapping = bind_source_nodes(candidate, rig)
    old_views = len(candidate.setdefault("bufferViews", []))
    time_accessor = len(candidate.setdefault("accessors", []))
    binary = bytearray()
    transform_channels = append_humanoid_clip(candidate, binary, mapping,
                                             {"name": "captured", "frames": frames}, require_motion=False)
    animation, = candidate["animations"]
    morph_channels = 0
    for node, source in mapping.items():
        if "mesh" not in candidate["nodes"][node]:
            continue
        path = records[0]["transforms"][source]["path"]
        values = []
        for record in records:
            skin, = [skin for skin in record["skins"] if skin["path"] == path]
            values.append([value / 100 for value in skin["blendWeights"]])
        reference = candidate["nodes"][node]["weights"]
        if not reference or all(max(abs(a - b) for a, b in zip(row, reference)) <= 1e-7 for row in values):
            continue
        flat = [value for row in values for value in row]
        output = add_accessor(candidate, binary, pack(f"<{len(flat)}f", *flat), len(flat), "SCALAR", 5126)
        animation["channels"].append({"sampler": len(animation["samplers"]), "target": {"node": node, "path": "weights"}})
        animation["samplers"].append({"input": time_accessor, "output": output, "interpolation": "LINEAR"})
        morph_channels += 1
    if not animation["channels"]:
        node = next(iter(mapping))
        value = candidate["nodes"][node]["translation"]
        output = add_accessor(candidate, binary, pack(f"<{len(records) * 3}f", *(value * len(records))),
                              len(records), "VEC3", 5126)
        animation["channels"].append({"sampler": 0, "target": {"node": node, "path": "translation"}})
        animation["samplers"].append({"input": time_accessor, "output": output, "interpolation": "LINEAR"})
    for view in candidate["bufferViews"][old_views:]:
        view["buffer"] = 1
    candidate["buffers"].append({"uri": buffer_uri, "byteLength": len(binary)})
    document.clear()
    document.update(candidate)
    return bytes(binary), {"samples": len(records), "duration": frames[-1]["time"],
                           "transformChannels": transform_channels, "morphChannels": morph_channels,
                           "sourceFrames": [record["frame"] for record in records]}
