"""Apply observed original object activation to imported room drawables."""

from collections import Counter
import numpy as np

from tools.npr_room.scene_catalog import local_matrix


def filter_visibility(level, record, scene, extra_origins=None):
    paths = {}

    def source_path(identity):
        if identity not in paths:
            transform = scene.read(identity)
            name = scene.read(transform["m_GameObject"]["fileID"])["m_Name"]
            parent = transform["m_Father"]["fileID"]
            paths[identity] = source_path(parent) + "/" + name if parent else name
        return paths[identity]

    transforms = [identity for identity, document in scene.documents.items() if document[0] == 4]
    original_counts = Counter(source_path(identity) for identity in transforms)
    active_counts = Counter(record["activeObjects"])
    captured = {node["id"]: node for node in record.get("transforms", [])}
    original_world, captured_world, resolved = {}, {}, {}

    def world(identity, cache, observed):
        if identity not in cache:
            if observed:
                node = captured[identity]
                transform = {"m_Local" + field: node[key] for field, key in (
                    ("Position", "position"), ("Rotation", "rotation"), ("Scale", "scale"))}
                parent = node["parentId"]
            else:
                transform = scene.read(identity)
                parent = transform["m_Father"]["fileID"]
            matrix = np.array(local_matrix(transform)).reshape(4, 4, order="F")
            cache[identity] = world(parent, cache, observed) @ matrix if parent else matrix
        return cache[identity]

    def resolve(path):
        if path not in resolved:
            candidates = [identity for identity in transforms if source_path(identity) == path]
            selected = set()
            observed = [node for node in captured.values() if node["path"] == path]
            if len(observed) != active_counts[path]:
                raise ValueError(f"Ambiguous captured activation count: {path}")
            for node in observed:
                matrix = world(node["id"], captured_world, True)
                matches = [identity for identity in candidates
                           if np.max(np.abs(world(identity, original_world, False) - matrix)) < .0005]
                if len(matches) != 1 or matches[0] in selected:
                    raise ValueError(f"Ambiguous captured activation pose: {path}")
                selected.add(matches[0])
            resolved[path] = selected
        return resolved[path]
    origins = {node: int(key.rsplit("-", 1)[-1])
               for key, node in level["sceneImports"]["RoomScene"]["nodeMap"].items()}
    origins.update(extra_origins or {})
    report = {"inactive": [], "active": [], "coverageHidden": [], "unmapped": []}
    pending = []
    for node in level["nodes"]:
        if not node.get("resourceId"):
            continue
        identity = origins.get(node["id"])
        if identity is None:
            report["unmapped"].append(node["id"])
            continue
        path = source_path(identity)
        observed = active_counts[path]
        active = identity in resolve(path) if 0 < observed < original_counts[path] else observed > 0
        report["active" if active else "inactive"].append(path)
        current = node["components"].get("azure.renderable", {"data": {}})["data"].get("visible", True)
        if active and not current:
            report["coverageHidden"].append(path)
        pending.append((node, active and current))
    # Validate all source identities before changing the diagnostic level.
    for node, visible in pending:
        component = node["components"].setdefault("azure.renderable", {
            "type": "azure.renderable", "version": 1, "data": {}})
        component["data"]["visible"] = visible
    report["poseResolved"] = list(resolved)
    return {key: sorted(value) for key, value in report.items()}
