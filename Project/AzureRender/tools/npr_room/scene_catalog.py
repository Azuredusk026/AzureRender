"""Turn active Unity static renderers into a package-relative scene exchange."""

import argparse
from collections import Counter
import json
import math
from pathlib import Path

from tools.npr_room.builtin_mesh_export import entry_key
from tools.npr_room.skinned_mesh_export import UnityScene
from tools.npr_room.static_batch_catalog import batch_slot


def local_matrix(transform):
    t, q, s = (transform[name] for name in ("m_LocalPosition", "m_LocalRotation", "m_LocalScale"))
    x, y, z, w = -q["x"], -q["y"], q["z"], q["w"]
    length = math.hypot(x, y, z, w)
    x, y, z, w = (value / length for value in (x, y, z, w))
    sx, sy, sz = s["x"], s["y"], s["z"]
    return [
        (1 - 2 * (y*y + z*z)) * sx, 2 * (x*y + z*w) * sx, 2 * (x*z - y*w) * sx, 0,
        2 * (x*y - z*w) * sy, (1 - 2 * (x*x + z*z)) * sy, 2 * (y*z + x*w) * sy, 0,
        2 * (x*z + y*w) * sz, 2 * (y*z - x*w) * sz, (1 - 2 * (x*x + y*y)) * sz, 0,
        t["x"], t["y"], -t["z"], 1,
    ]


def build_static_exchange(scene, scene_id, slots, overrides, batches=None, camera_id=None):
    batches = {} if batches is None else batches
    world_nodes = []
    resources, nodes, active = {}, {}, {}
    stats = Counter()
    gaps = []
    for game_id in overrides:
        if scene.documents.get(game_id, (None,))[0] != 1:
            raise ValueError(f"Override must identify a GameObject: {game_id}")

    def node_id(file_id):
        return f"unity-{file_id}"

    def transform_active(file_id):
        if not file_id:
            return True
        if file_id not in active:
            transform = scene.read(file_id)
            game_id = transform["m_GameObject"]["fileID"]
            game = scene.read(game_id)
            value = overrides.get(game_id, bool(game["m_IsActive"]))
            active[file_id] = value and transform_active(transform["m_Father"]["fileID"])
        return active[file_id]

    def add_node(file_id):
        if file_id in nodes:
            return nodes[file_id]
        transform = scene.read(file_id)
        parent = transform["m_Father"]["fileID"]
        node = {
            "id": node_id(file_id),
            "name": scene.read(transform["m_GameObject"]["fileID"])["m_Name"],
            "localTransform": {"matrix": local_matrix(transform)},
        }
        if parent:
            add_node(parent)
            node["parent"] = node_id(parent)
        nodes[file_id] = node
        return node

    for document_id, (kind, _, _) in scene.documents.items():
        if kind != 33:
            continue
        mesh_filter = scene.read(document_id)
        game_id = mesh_filter["m_GameObject"]["fileID"]
        game = scene.read(game_id)
        components = {
            scene.documents[component_id][0]: component_id
            for item in game.get("m_Component", ())
            if (component_id := item["component"]["fileID"]) in scene.documents
        }
        renderer_id, transform_id = components.get(23), components.get(4)
        if renderer_id is None or transform_id is None:
            stats["missingRendererOrTransform"] += 1
            continue
        renderer = scene.read(renderer_id)
        if not renderer["m_Enabled"] or not transform_active(transform_id):
            stats["inactive"] += 1
            continue
        reference = mesh_filter["m_Mesh"]
        guid = reference.get("guid")
        batch = renderer.get("m_StaticBatchInfo", {})
        if batch.get("subMeshCount"):
            key = batch_slot(guid, batch["firstSubMesh"], batch["subMeshCount"])
            packed = batches.get(key, {})
            if packed.get("coordinates") != "world" or renderer["m_StaticBatchRoot"]["fileID"] != 0:
                stats["staticBatch"] += 1
                gaps.append({"code": "StaticBatch", "source": node_id(transform_id),
                             "message": "Requires a separate triangle range"})
                continue
            resources[key] = {"id": key, "path": packed["media"]}
            world_nodes.append({
                "id": f"unity-batch-{transform_id}", "name": game["m_Name"], "meshRef": key,
            })
            stats["renderables"] += 1
            stats["batchedRenderables"] += 1
            continue
        resource_key = entry_key(guid, reference["fileID"])
        slot = slots.get(resource_key, {})
        if slot.get("kind") != "static":
            stats["missingStaticMesh"] += 1
            gaps.append({"code": "MissingStaticMesh", "source": node_id(transform_id),
                         "message": f"Mesh {resource_key or reference['fileID']} is outside the static catalog"})
            continue

        node = add_node(transform_id)
        node["meshRef"] = resource_key
        resources[resource_key] = {"id": resource_key, "path": slot["media"]}
        stats["renderables"] += 1

    if camera_id is not None:
        if scene.documents.get(camera_id, (None,))[0] != 20:
            raise ValueError(f"Camera override must identify a Unity Camera: {camera_id}")
        camera = scene.read(camera_id)
        game = scene.read(camera["m_GameObject"]["fileID"])
        transform_id, = [item["component"]["fileID"] for item in game["m_Component"]
                         if scene.documents.get(item["component"]["fileID"], (None,))[0] == 4]
        if not camera["m_Enabled"] or not transform_active(transform_id):
            raise ValueError(f"Selected camera is inactive: {camera_id}")
        viewport = camera["m_NormalizedViewPortRect"]
        if (camera["m_projectionMatrixMode"] != 1 or camera["m_FOVAxisMode"] != 0
                or camera["m_LensShift"] != {"x": 0, "y": 0}
                or [viewport[field] for field in ("x", "y", "width", "height")] != [0, 0, 1, 1]):
            raise ValueError(f"Unsupported camera projection or viewport: {camera_id}")
        add_node(transform_id)["camera"] = {
            "enabled": True, "priority": 0, "orthographic": bool(camera["orthographic"]),
            "fieldOfView": camera["field of view"], "orthographicHeight": 2 * camera["orthographic size"],
            "nearPlane": camera["near clip plane"], "farPlane": camera["far clip plane"],
        }
        stats["cameras"] += 1

    stats["nodes"] = len(nodes) + len(world_nodes)
    stats["resources"] = len(resources)
    exchange = {
        "schemaVersion": 1, "id": scene_id, "upAxis": "Y", "unitMeters": 1,
        "nodes": [*nodes.values(), *world_nodes], "resources": list(resources.values()),
        "losses": gaps + [{"code": "MaterialsPending", "source": scene_id,
                          "message": "Uses the staged geometry material slots"}],
    }
    return exchange, dict(stats)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scene", type=Path, required=True)
    parser.add_argument("--catalog", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--batches", type=Path)
    parser.add_argument("--builtins", type=Path)
    parser.add_argument("--activate", type=int, action="append", default=[])
    parser.add_argument("--deactivate", type=int, action="append", default=[])
    parser.add_argument("--camera", type=int, help="File ID of the active Unity Camera")
    args = parser.parse_args()
    folder = args.catalog.resolve().parent
    if args.output.resolve().parent != folder:
        parser.error("--output must share the mesh catalog directory")
    if set(args.activate) & set(args.deactivate):
        parser.error("GameObject activation overrides overlap")
    overrides = dict.fromkeys(args.activate, True) | dict.fromkeys(args.deactivate, False)
    slots = json.loads(args.catalog.read_text(encoding="utf-8"))["slots"]
    if args.builtins:
        builtin_slots = json.loads(args.builtins.read_text(encoding="utf-8"))["slots"]
        if slots.keys() & builtin_slots.keys():
            raise ValueError("Built-in and exported mesh slots overlap")
        slots.update(builtin_slots)
    source = UnityScene(args.scene)
    batches = json.loads(args.batches.read_text(encoding="utf-8"))["slots"] if args.batches else None
    output, stats = build_static_exchange(source, args.scene.stem, slots, overrides, batches, args.camera)
    for resource in output["resources"]:
        asset = (folder / resource["path"]).resolve(strict=True)
        if not asset.is_relative_to(folder) or not asset.is_file():
            raise ValueError(f"Invalid packaged resource: {resource['path']}")
    with args.output.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(output, stream, ensure_ascii=False, indent=2)
        stream.write("\n")
    print(json.dumps(stats))


if __name__ == "__main__":
    main()
