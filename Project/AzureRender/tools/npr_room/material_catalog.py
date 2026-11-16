"""Index Unity material properties and ordered mesh renderer assignments."""

import argparse
from collections import Counter
import json
from pathlib import Path

from tools.npr_room.reference_inventory import BUILTIN_GUIDS, META_GUID, digest, group_paths_by_guid, text
from tools.npr_room.skinned_mesh_export import UnityScene


PROPERTY_TABLES = ("m_TexEnvs", "m_Ints", "m_Floats", "m_Colors")
RENDERER_TYPES = {23: "mesh", 137: "skinned"}


def property_map(value):
    if isinstance(value, dict):
        return value
    entries = {}
    for item in value:
        if not isinstance(item, dict) or len(item) != 1:
            raise ValueError("Expected one serialized material property per list item")
        (name, value), = item.items()
        if name in entries:
            raise ValueError(f"Repeated material property {name}")
        entries[name] = value
    return entries


def build_material_catalog(project, inventory, images):
    project = project.resolve(strict=True)
    indexed = group_paths_by_guid(inventory["assets"])
    candidates = sorted(set(inventory["additionalClosure"]))
    materials, bindings = {}, {}
    counts, shader_refs, texture_refs, material_refs = (Counter() for _ in range(4))

    def reference(raw, staged, mapped_state, file_ids=False):
        result = dict(raw)
        file_id, guid = raw["fileID"], raw.get("guid")
        if not file_id:
            state = "null"
        elif not guid:
            state = "local"
        elif guid in BUILTIN_GUIDS:
            state = "builtin"
        else:
            paths = indexed.get(guid, ())
            if len(paths) == 1:
                result["source"], = paths
                state = "indexed"
                if guid in staged:
                    state = mapped_state
                    if file_ids and file_id != staged[guid]["fileID"]:
                        state = "file-id-mismatch"
            else:
                state = "ambiguous" if paths else "unresolved"
        result["state"] = state
        return result

    def open_source(name):
        asset = (project / name).resolve(strict=True)
        if not asset.is_relative_to(project / "Assets"):
            raise ValueError(f"Source resolves outside Assets: {name}")
        return asset

    for name in candidates:
        if Path(name).suffix != ".mat":
            continue
        source = open_source(name)
        guid = inventory["assets"][name]
        match = META_GUID.search(text(source.with_name(source.name + ".meta")))
        if not match or match[1].lower() != guid or len(indexed[guid]) != 1:
            raise ValueError(f"Material identity changed or is ambiguous: {name}")
        fingerprint = digest(source)
        document = UnityScene(source)
        file_id, = [key for key, entry in document.documents.items() if entry[0] == 21]
        data = document.read(file_id)
        shader = reference(data["m_Shader"], {}, "")
        shader_refs[shader["state"]] += 1
        saved = data["m_SavedProperties"]
        properties = {table: property_map(saved[table]) for table in PROPERTY_TABLES}
        textures = {}
        for prop, entry in properties["m_TexEnvs"].items():
            texture = reference(entry["m_Texture"], images, "image-catalog")
            texture_refs[texture["state"]] += 1
            textures[prop] = {**entry, "m_Texture": texture}
        properties["m_TexEnvs"] = textures
        materials[guid] = {
            "source": name, "sourceSHA256": fingerprint, "fileID": file_id,
            "name": data["m_Name"], "shader": shader,
            "parent": reference(data.get("m_Parent", {"fileID": 0}), {}, ""),
            "settings": {key: value for key, value in data.items()
                         if key not in ("m_Name", "m_Shader", "m_Parent", "m_SavedProperties")},
            "propertyVersion": saved["serializedVersion"], "properties": properties,
        }
        if digest(source) != fingerprint:
            raise ValueError(f"Material changed while reading: {name}")

    for name in candidates:
        if Path(name).suffix not in (".unity", ".prefab"):
            continue
        document = UnityScene(open_source(name))
        renderers = {}
        for file_id, (kind, _, _) in document.documents.items():
            if kind not in RENDERER_TYPES:
                continue
            renderer = document.read(file_id)
            if "m_Materials" not in renderer:
                counts["rendererWithoutMaterialTable"] += 1
                continue
            slots = [reference(slot, materials, "material-catalog", file_ids=True)
                     for slot in renderer["m_Materials"]]
            material_refs.update(slot["state"] for slot in slots)
            counts[RENDERER_TYPES[kind]] += 1
            renderers[str(file_id)] = {
                "type": RENDERER_TYPES[kind], "object": renderer["m_GameObject"]["fileID"],
                "enabled": bool(renderer["m_Enabled"]), "materials": slots,
                "staticBatch": renderer.get("m_StaticBatchInfo"),
            }
        if renderers:
            bindings[name] = renderers

    return {
        "formatVersion": 1, "slots": materials, "rendererBindings": bindings,
        "summary": {
            "materials": len(materials), "filesWithRenderers": len(bindings),
            "renderers": dict(counts), "rendererMaterialReferences": dict(material_refs),
            "shaderReferences": dict(shader_refs), "textureReferences": dict(texture_refs),
        },
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--inventory", type=Path, required=True)
    parser.add_argument("--images", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    inventory = json.loads(args.inventory.read_text(encoding="utf-8"))
    images = json.loads(args.images.read_text(encoding="utf-8"))["slots"]
    report = build_material_catalog(args.source, inventory, images)
    payload = json.dumps(report, ensure_ascii=False, allow_nan=False, sort_keys=True, separators=(",", ":"))
    with args.output.open("x", encoding="utf-8", newline="\n") as stream:
        stream.write(payload + "\n")
    print(json.dumps(report["summary"]))


if __name__ == "__main__":
    main()
