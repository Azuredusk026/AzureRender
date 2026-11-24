"""Attach captured rigid renderers to an imported character's bone nodes."""

from base64 import b64encode
import copy
from hashlib import sha256
import json
import math
from pathlib import Path
import shutil
import tempfile
from uuid import NAMESPACE_URL, UUID, uuid5

from tools.npr_room.catalog_storage import publish_directory
from tools.npr_room.glb_reference import reference_document
from tools.npr_room.humanoid_clip_export import bind_source_nodes, local_samples, CHANNELS
from tools.npr_room.original_room_registration import source_rig
from tools.npr_room.reference_inventory import digest


def texture_index(document, binding, catalog, root):
    if not binding or not binding["texture"]:
        return None
    if binding["scale"] != {"x": 1, "y": 1} or binding["offset"] != {"x": 0, "y": 0}:
        raise ValueError("Captured accessory texture requires its source UV transform")
    matches = [row for row in catalog.values() if Path(row["source"]).stem == binding["texture"]]
    if len(matches) != 1:
        raise ValueError("Captured accessory texture name must identify one source image")
    image, = matches
    path = (root / image["media"]).resolve(strict=True)
    if not path.is_relative_to(root.resolve()) or image["srgb"] is not True or digest(path) != image["sha256"]:
        raise ValueError("Captured accessory image identity or color space differs")
    payload = path.read_bytes()
    if not payload.startswith(b"\x89PNG\r\n\x1a\n") or sha256(payload).hexdigest() != image["sha256"]:
        raise ValueError("Captured accessory requires an unchanged PNG")
    images = document.setdefault("images", [])
    textures = document.setdefault("textures", [])
    index = len(textures)
    textures.append({"source": len(images)})
    images.append({"uri": "data:image/png;base64," + b64encode(payload).decode("ascii")})
    return index


def captured_material(document, material, images, image_root, name):
    floats = {row["name"]: row["value"] for row in material["floats"]}
    colors = {row["name"]: row["value"] for row in material["colors"]}
    maps = {row["name"]: row for row in material["textures"]}
    if (material["shader"] != "lilToon" or material["renderQueue"] != 2000
            or floats.get("_SrcBlend", 1) != 1 or floats.get("_DstBlend", 0) != 0
            or floats.get("_Cull", 2) not in (0, 2)
            or any(floats.get(key, 0) for key in ("_UseMain2ndTex", "_UseMain3rdTex", "_UseEmission2nd", "_Invisible"))):
        raise ValueError("Captured accessory requires the opaque source base and first emission layer")
    tint = [colors.get("_Color", dict(r=1, g=1, b=1, a=1))[key] for key in "rgba"]
    roughness = 1 - floats.get("_Smoothness", 0)
    metallic = floats.get("_Metallic", 0)
    if tint[3] != 1 or any(not math.isfinite(value) or not 0 <= value <= 1 for value in (*tint, roughness, metallic)):
        raise ValueError("Captured accessory base material values are invalid")
    pbr = {"baseColorFactor": tint, "metallicFactor": metallic, "roughnessFactor": roughness}
    texture = texture_index(document, maps.get("_MainTex"), images, image_root)
    if texture is not None:
        pbr["baseColorTexture"] = {"index": texture}
    result = {"name": name, "pbrMetallicRoughness": pbr, "doubleSided": floats.get("_Cull", 2) == 0}
    if floats.get("_UseEmission", 0):
        emission = [colors["_EmissionColor"][key] for key in "rgb"]
        if any(not math.isfinite(value) or value < 0 for value in emission):
            raise ValueError("Captured accessory emission values are invalid")
        result["emissiveFactor"] = emission
        strength = max(emission)
        if strength > 1:
            result["emissiveFactor"] = [value / strength for value in emission]
            result["extensions"] = {"KHR_materials_emissive_strength": {"emissiveStrength": strength}}
            used = document.setdefault("extensionsUsed", [])
            if "KHR_materials_emissive_strength" not in used:
                used.append("KHR_materials_emissive_strength")
        texture = texture_index(document, maps.get("_EmissionMap"), images, image_root)
        if texture is not None:
            result["emissiveTexture"] = {"index": texture}
    return result


def install(frame, document, mesh_catalog, image_catalog, heroine):
    output = heroine / "captured-accessories"
    if output.exists():
        raise FileExistsError(output)
    raw = frame.read_bytes()
    record = json.loads(raw)
    identity = sha256(raw).hexdigest()
    mapping = bind_source_nodes(document, source_rig(record))
    parents = {record["transforms"][source]["id"]: node for node, source in mapping.items()}
    transforms = {row["id"]: row for row in record["transforms"]}
    selected = [row for row in record["renderers"] if row["kind"] == "MeshRenderer"
                and row["enabled"] and row["visible"] and transforms[row["transformId"]]["parentId"] in parents]
    if not selected:
        return {"count": 0, "frameSHA256": identity, "attachments": []}
    meshes = json.loads(mesh_catalog.read_bytes())["slots"]
    images = json.loads(image_catalog.read_bytes())["slots"]
    candidate = copy.deepcopy(document)
    files = {}
    attachments = []
    for renderer in selected:
        matches = [(guid, row) for guid, row in meshes.items() if Path(row["source"]).stem == renderer["mesh"]]
        if len(matches) != 1:
            raise ValueError("Captured accessory mesh name must identify one source mesh")
        guid, row = matches[0]
        UUID(hex=guid)
        source = (mesh_catalog.parent / row["media"]).resolve(strict=True)
        if (not source.is_relative_to(mesh_catalog.parent.resolve()) or row["kind"] != "static"
                or digest(source) != row["sha256"]):
            raise ValueError("Captured accessory mesh identity differs")
        model = reference_document(source)
        if (len(model["meshes"]) != 1 or len(model["nodes"]) != 1 or model["nodes"][0].get("mesh") != 0
                or model.get("skins") or model.get("animations") or model.get("images")):
            raise ValueError("Captured accessory requires one catalog static mesh")
        mesh = model["meshes"][0]
        if mesh.get("weights") or any(primitive.get("targets") for primitive in mesh["primitives"]):
            raise ValueError("Captured accessory morphs require captured deformation")
        if len(renderer["materials"]) != len(model["materials"]):
            raise ValueError("Captured accessory material slots differ")
        view_offset = len(candidate.setdefault("bufferViews", []))
        accessor_offset = len(candidate.setdefault("accessors", []))
        material_offset = len(candidate.setdefault("materials", []))
        mesh_offset = len(candidate.setdefault("meshes", []))
        buffer_index = len(candidate["buffers"])
        model["buffers"][0]["uri"] = "captured-accessories/" + guid + ".glb"
        candidate["buffers"].extend(model["buffers"])
        for view in model["bufferViews"]:
            view["buffer"] = buffer_index
        for accessor in model["accessors"]:
            if "sparse" in accessor:
                raise ValueError("Captured accessory requires catalog dense accessors")
            accessor["bufferView"] += view_offset
        candidate["bufferViews"].extend(model["bufferViews"])
        candidate["accessors"].extend(model["accessors"])
        for slot, material in enumerate(renderer["materials"]):
            if material["slot"] != slot:
                raise ValueError("Captured accessory material order differs")
            name = "captured-accessory-" + str(uuid5(NAMESPACE_URL, renderer["path"] + ":" + str(slot)))
            candidate["materials"].append(captured_material(candidate, material, images, image_catalog.parent, name))
        for primitive in mesh["primitives"]:
            primitive["attributes"] = {key: value + accessor_offset for key, value in primitive["attributes"].items()}
            if "indices" in primitive:
                primitive["indices"] += accessor_offset
            primitive["material"] += material_offset
        candidate["meshes"].append(mesh)
        transform = transforms[renderer["transformId"]]
        parent = parents[transform["parentId"]]
        node = {"name": transform["path"].rsplit("/", 1)[-1], "mesh": mesh_offset}
        values = {field: [transform[source]] for field, source in (("positions", "position"), ("rotations", "rotation"), ("scales", "scale"))}
        for path, field, default, axes in CHANNELS:
            node[path] = list(local_samples([values], 0, field, axes, default)[0])
        node_index = len(candidate["nodes"])
        candidate["nodes"][parent].setdefault("children", []).append(node_index)
        candidate["nodes"].append(node)
        files[guid] = (source, row["sha256"])
        attachments.append({"path": renderer["path"], "node": node_index, "parent": parent,
                            "meshGUID": guid, "meshSHA256": row["sha256"], "sourceMeshSHA256": row["sourceSHA256"]})
    with tempfile.TemporaryDirectory(prefix="captured-accessories-", dir=heroine) as temporary:
        staged = Path(temporary) / "captured-accessories"
        staged.mkdir()
        for guid, (source, checksum) in files.items():
            path = staged / (guid + ".glb")
            shutil.copyfile(source, path)
            if digest(path) != checksum:
                raise ValueError("Captured accessory changed during installation")
            metadata = {"schemaVersion": 1, "id": str(uuid5(NAMESPACE_URL, "captured-accessory-v1:" + identity + ":" + guid)),
                        "settings": {"retainSourceSnapshot": False}, "dependencies": []}
            path.with_name(path.name + ".azmeta").write_text(json.dumps(metadata) + "\n", encoding="utf-8")
        publish_directory(staged, output)
    document.clear()
    document.update(candidate)
    return {"count": len(attachments), "frameSHA256": identity, "attachments": attachments,
            "scope": "captured static attachment pose, base texture and first emission layer"}
