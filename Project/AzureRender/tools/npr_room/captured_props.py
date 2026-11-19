"""Install source-posed skinned props from the existing deformer catalog."""

import copy
from hashlib import sha256
import json
from pathlib import Path
import shutil
import tempfile
from uuid import NAMESPACE_URL, uuid5

from tools.npr_room.captured_accessories import captured_material
from tools.npr_room.catalog_storage import publish_directory
from tools.npr_room.glb_reference import reference_document
from tools.npr_room.humanoid_clip_export import bind_source_nodes
from tools.npr_room.original_pose_sequence import append_sequence, captured_graph
from tools.npr_room.original_room_registration import freeze_model, source_rig
from tools.npr_room.pbr_materials import staging_metadata, write
from tools.npr_room.reference_inventory import digest


def install(frame, catalog, assets, level, *, represented_meshes=(), sequence=None):
    output = assets / "captured-props"
    if output.exists():
        raise FileExistsError(output)
    raw = frame.read_bytes()
    record = json.loads(raw)
    identity = sha256(raw).hexdigest()
    records = None
    if sequence is not None:
        if not 2 <= len(sequence) <= 12 or digest(sequence[0]) != identity:
            raise ValueError("Captured prop sequence requires the same first frame and two to twelve samples")
        records = [json.loads(path.read_bytes()) for path in sequence]
    selected = []
    for skin in record["skins"]:
        if skin["mesh"] in represented_meshes:
            continue
        matches = [row for row in record["renderers"] if row["path"] == skin["path"] and row["kind"] == "SkinnedMeshRenderer"
                   and row["enabled"] and row["visible"]]
        if len(matches) > 1:
            raise ValueError("Captured prop requires a unique renderer")
        if matches:
            if matches[0]["mesh"] != skin["mesh"]:
                raise ValueError("Captured prop renderer mesh identity differs")
            selected.append((skin, matches[0]))
    if not selected:
        return {"count": 0, "frameSHA256": identity, "props": []}
    deformer_root = catalog / "deformer-catalog"
    variants = json.loads((deformer_root / "deformer-catalog.json").read_bytes())["variants"]
    image_root = catalog / "image-catalog"
    images = json.loads((image_root / "image-catalog.json").read_bytes())["slots"]
    models = []
    for key, row in variants.items():
        source = (deformer_root / row["media"]).resolve(strict=True)
        if source.parent != deformer_root.resolve() or digest(source) != row["sha256"]:
            raise ValueError("Captured prop deformer identity differs")
        models.append((key, row, source, reference_document(source)))
    resources, nodes, proofs = [], [], []
    existing = {row["id"] for row in level["resources"]} | {row["id"] for row in level["nodes"]}
    with tempfile.TemporaryDirectory(prefix="captured-props-", dir=assets) as temporary:
        staged = Path(temporary) / "captured-props"
        staged.mkdir()
        graph_id = uuid5(NAMESPACE_URL, "captured-prop-graph-v1:" + identity)
        if records is not None:
            path = staged / "motion.json"
            write(path, captured_graph())
            staging_metadata(path, graph_id.hex, [], retain_source_snapshot=False)
        for skin, renderer in selected:
            candidates = []
            for key, row, source, model in models:
                if len(model.get("meshes", [])) != 1 or model["meshes"][0].get("name") != skin["mesh"]:
                    continue
                document = copy.deepcopy(model)
                try:
                    pose = freeze_model(document, record)
                    mapping = bind_source_nodes(document, source_rig(record))
                except ValueError:
                    continue
                mesh_node, = [i for i, node in enumerate(document["nodes"]) if "mesh" in node]
                if record["transforms"][mapping[mesh_node]]["path"] == skin["path"]:
                    candidates.append((key, row, source, document, pose))
            if len(candidates) != 1:
                raise ValueError(f"Captured prop requires a unique matching catalog hierarchy: {skin['path']}")
            key, row, source, document, pose = candidates[0]
            if len(document["materials"]) != len(renderer["materials"]):
                raise ValueError("Captured prop material slot count differs")
            for slot, material in enumerate(renderer["materials"]):
                if material["slot"] != slot:
                    raise ValueError("Captured prop material slot order differs")
                name = "captured-prop-" + str(uuid5(NAMESPACE_URL, skin["path"] + ":" + str(slot)))
                document["materials"][slot] = captured_material(document, material, images, image_root, name)
            token = uuid5(NAMESPACE_URL, "captured-prop-v1:" + identity + ":" + skin["path"])
            resource_id = "captured-prop-" + uuid5(NAMESPACE_URL, skin["path"]).hex
            if resource_id in existing:
                raise ValueError("Captured prop identity collides with the room")
            existing.add(resource_id)
            geometry_id = uuid5(NAMESPACE_URL, "captured-prop-source-v1:" + identity + ":" + source.name)
            geometry = staged / source.name
            if not geometry.exists():
                shutil.copyfile(source, geometry)
                if digest(geometry) != row["sha256"]:
                    raise ValueError("Captured prop changed during installation")
                staging_metadata(geometry, geometry_id.hex, [], retain_source_snapshot=False)
            motion_proof = None
            dependencies = [str(geometry_id)]
            if records is not None:
                motion_name = token.hex + ".bin"
                payload, motion_proof = append_sequence(document, records, motion_name)
                path = staged / motion_name
                path.write_bytes(payload)
                motion_id = uuid5(NAMESPACE_URL, "captured-prop-motion-v1:" + token.hex)
                staging_metadata(path, motion_id.hex, [], retain_source_snapshot=False)
                dependencies.extend((str(motion_id), str(graph_id)))
            name = token.hex + ".gltf"
            path = staged / name
            write(path, document)
            staging_metadata(path, token.hex, dependencies, retain_source_snapshot=False)
            components = {}
            if records is not None:
                components["azure.animator"] = {"type": "azure.animator", "version": 1, "data": {
                    "asset": "assets:/captured-props/motion.json", "state": "captured"}}
            resources.append({"id": resource_id, "asset": "assets:/captured-props/" + name})
            nodes.append({"id": resource_id, "resourceId": resource_id, "name": skin["mesh"], "components": components})
            proofs.append({"path": skin["path"], "mesh": skin["mesh"], "variant": key, "geometrySHA256": row["sha256"],
                           "modelSHA256": digest(path), "pose": pose, "motion": motion_proof, "resourceId": resource_id,
                           "joints": sum(len(value["joints"]) for value in document.get("skins", []))})
        publish_directory(staged, output)
    level["resources"].extend(resources)
    level["nodes"].extend(nodes)
    return {"count": len(proofs), "frameSHA256": identity, "props": proofs,
            "scope": "captured visible skinned props with source pose and base material"}
