"""Trace the window view enum through the original RoomScene renderer hierarchy."""

import argparse
from collections import Counter
import json
from pathlib import Path

from tools.npr_room.reference_inventory import META_GUID, digest, text
from tools.npr_room.skinned_mesh_export import UnityScene
from tools.npr_room.window_references import source_cases


RENDERERS = {23: "mesh", 137: "skinned", 199: "particle", 212: "sprite"}
SOURCE_CODE = "Assets/Scripts/Assembly-CSharp/Bulbul"
ROOM_SCENE = "Assets/App/Scenes/RoomScene.unity"


def inventory(scene, service_guid, cases):
    services = []
    for identity, (kind, start, stop) in scene.documents.items():
        if kind == 114 and scene.serialized.find(service_guid, start, stop) != -1:
            candidate = scene.read(identity)
            if candidate["m_Script"].get("guid") == service_guid:
                services.append((identity, candidate))
    if len(services) != 1:
        raise ValueError("RoomScene must contain exactly one WindowViewService")
    service_id, service = services[0]
    fields = {field.casefold(): (field, ref) for field, ref in service.items() if field.startswith("_")}

    def components(entity):
        rows = [item["component"]["fileID"] for item in entity["m_Component"]]
        return [(scene.documents[identity][0], identity) for identity in rows]

    def visit(node, active, root):
        transform = scene.read(node)
        entity = scene.read(transform["m_GameObject"]["fileID"])
        active = active and (node == root or bool(entity["m_IsActive"]))
        co = components(entity)
        mesh_filter = next((identity for kind, identity in co if kind == 33), None)
        for kind, identity in co:
            if kind not in RENDERERS:
                continue
            info = scene.read(identity)
            row = {
                "component": identity, "transform": node, "name": entity["m_Name"], "kind": RENDERERS[kind],
                "activeWhenSelected": active, "enabled": bool(info["m_Enabled"]),
                "materials": info["m_Materials"],
            }
            if kind == 23 and mesh_filter is not None:
                row["mesh"] = scene.read(mesh_filter)["m_Mesh"]
            elif kind == 137:
                row["mesh"] = info["m_Mesh"]
            elif kind == 199:
                row["meshes"] = [ref for name in ("m_Mesh", "m_Mesh1", "m_Mesh2", "m_Mesh3")
                                 if (ref := info[name])["fileID"]]
            elif kind == 212:
                row["sprite"] = info["m_Sprite"]
            yield row
        for child in transform["m_Children"]:
            yield from visit(child["fileID"], active, root)

    result = {}
    roots = set()
    for case, name in sorted(cases.items()):
        binding = fields.get("_" + name.casefold())
        if not binding:
            raise ValueError(f"Missing window scene field for {name}")
        field, raw = binding
        game_id = raw["fileID"]
        if game_id in roots or scene.documents.get(game_id, (None,))[0] != 1:
            raise ValueError(f"Invalid or duplicate window GameObject for {name}: {game_id}")
        roots.add(game_id)
        entity = scene.read(game_id)
        root, = [identity for kind, identity in components(entity) if kind == 4]
        by_renderer = list(visit(root, True, root))
        result[case] = {
            "name": name, "field": field, "gameObject": game_id, "rootName": entity["m_Name"],
            "transform": root, "initialActive": bool(entity["m_IsActive"]), "renderers": by_renderer,
        }
    return {"serviceComponent": service_id, "cases": result}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    scene_path = args.source / ROOM_SCENE
    enum_path = args.source / SOURCE_CODE / "WindowViewType.cs"
    service_path = args.source / SOURCE_CODE / "WindowViewService.cs"
    guid, = META_GUID.findall(text(service_path.with_suffix(".cs.meta")))
    report = inventory(UnityScene(scene_path), guid, source_cases(text(enum_path)))
    counts = Counter(row["kind"] for case in report["cases"].values() for row in case["renderers"])
    active = Counter(row["kind"] for case in report["cases"].values() for row in case["renderers"]
                     if row["activeWhenSelected"] and row["enabled"])
    report.update({
        "formatVersion": 1, "roomSceneSHA256": digest(scene_path),
        "sourceEnumSHA256": digest(enum_path), "serviceSHA256": digest(service_path),
        "summary": {"cases": len(report["cases"]), "renderers": dict(counts), "enabledWhenSelected": dict(active)},
    })
    with args.output.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(report, stream, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
        stream.write("\n")
    print(json.dumps(report["summary"], sort_keys=True))


if __name__ == "__main__":
    main()
