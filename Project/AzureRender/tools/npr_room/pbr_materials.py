"""Stage native material documents for the simple opaque URP Lit profile."""

import argparse
import json
from pathlib import Path
import shutil
import tempfile
from uuid import UUID

from tools.npr_room.catalog_storage import publish_directory
from tools.npr_room.reference_inventory import digest


LIT_SHADER = "Assets/Shader/Universal Render Pipeline_Lit.shader"


def pbr_document(rgba, roughness, metallic, double_sided, texture):
    textures = {}
    if texture:
        textures["albedo"] = {"asset": str(UUID(hex=texture)), "usage": "albedo"}
    return {
        "schemaVersion": 1, "typeId": "azure.pbr", "parameters": {
            "baseColor": rgba, "roughness": roughness, "metallic": metallic,
            "emissiveStrength": 0, "transmission": 0, "ior": 1.5, "thickness": 0,
            "attenuationDistance": 0, "attenuationColor": [1, 1, 1, 1], "doubleSided": double_sided,
        }, "textures": textures,
    }


def room_slot_assignments(level, import_id, renderers, transforms, materials):
    node_map = level["sceneImports"][import_id]["nodeMap"]
    mesh_nodes = {node["id"] for node in level["nodes"] if node.get("resourceId")}
    assignments = []
    for renderer in renderers.values():
        if renderer["type"] != "mesh" or not renderer["enabled"] or renderer["staticBatch"]["subMeshCount"]:
            continue
        source_transform = transforms.get(renderer["object"])
        node_id = node_map.get(f"unity-{source_transform}")
        if node_id not in mesh_nodes:
            continue
        for index, source in enumerate(renderer["materials"]):
            material = materials.get(source.get("guid"))
            if material:
                assignments.append({
                    "node": node_id, "slotId": f"name:unity-slot-{index}", "material": material["asset"],
                })
    return assignments


def opaque_lit_document(source, images):
    properties = source["properties"]
    floats = properties["m_Floats"]
    settings = source["settings"]
    if (source["shader"].get("source") != LIT_SHADER or source["parent"]["fileID"]
            or floats.get("_Surface") != 0 or floats.get("_AlphaClip") != 0
            or floats.get("_WorkflowMode") != 1 or floats.get("_Cull") not in (0, 2)
            or floats.get("_SpecularHighlights") != 1 or floats.get("_EnvironmentReflections") != 1
            or settings["m_ValidKeywords"] or settings["m_CustomRenderQueue"] not in (-1, 2000)):
        raise ValueError("Opaque metallic workflow with standard rendering flags is required")
    color = properties["m_Colors"]["_BaseColor"]
    rgba = [color[name] for name in ("r", "g", "b", "a")]
    smoothness, metallic = floats["_Smoothness"], floats["_Metallic"]
    if rgba[3] != 1 or not all(0 <= value <= 1 for value in [*rgba, smoothness, metallic]):
        raise ValueError("Source color or scalar is outside the opaque native profile")
    occupied = {name: record for name, record in properties["m_TexEnvs"].items() if record["m_Texture"]["fileID"]}
    base = occupied.get("_BaseMap")
    if occupied.keys() - {"_BaseMap", "_MainTex"} or ("_MainTex" in occupied and occupied["_MainTex"] != base):
        raise ValueError("Source texture layout needs a dedicated material profile")
    texture = None
    if base:
        if base["m_Scale"] != {"x": 1, "y": 1} or base["m_Offset"] != {"x": 0, "y": 0}:
            raise ValueError("Albedo transform needs a dedicated material profile")
        texture = base["m_Texture"].get("guid")
        if images.get(texture, {}).get("srgb") is not True:
            raise ValueError("Albedo requires a mapped sRGB source texture")
    return pbr_document(rgba, 1 - smoothness, metallic, floats["_Cull"] == 0, texture), texture


def write(path, data):
    path.write_text(json.dumps(data, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n",
                    encoding="utf-8", newline="\n")


def staging_metadata(path, guid, dependencies, *, retain_source_snapshot=True):
    write(path.with_name(path.name + ".azmeta"), {
        "schemaVersion": 1, "id": str(UUID(hex=guid)), "dependencies": dependencies,
        "settings": {} if retain_source_snapshot else {"retainSourceSnapshot": False},
    })


def stage_materials(materials, image_manifest, output):
    if output.exists():
        raise FileExistsError(output)
    image_root = image_manifest.resolve(strict=True).parent
    images = json.loads(image_manifest.read_text(encoding="utf-8"))["slots"]
    staged_materials, staged_images, pending = {}, {}, {}
    with tempfile.TemporaryDirectory(prefix="recovery-pbr-", dir=output.parent) as directory:
        staged = Path(directory) / "publish"
        staged.mkdir()
        for guid, data in sorted(materials.items()):
            if data["shader"].get("source") != LIT_SHADER:
                continue
            try:
                document, texture = opaque_lit_document(data, images)
            except ValueError as error:
                pending[guid] = str(error)
                continue
            if texture and texture not in staged_images:
                entry = images[texture]
                source = (image_root / entry["media"]).resolve(strict=True)
                if source.parent != image_root / "media" or source.suffix != ".png":
                    raise ValueError(f"Invalid albedo source: {texture}")
                target = staged / (texture + ".png")
                shutil.copyfile(source, target)
                if digest(target) != entry["sha256"]:
                    raise ValueError(f"Albedo bytes changed: {texture}")
                staging_metadata(target, texture, [])
                staged_images[texture] = target.name
            target = staged / (guid + ".azurematerial")
            write(target, document)
            staging_metadata(target, guid, [str(UUID(hex=texture))] if texture else [])
            staged_materials[guid] = {
                "asset": str(UUID(hex=guid)), "source": data["source"], "document": target.name, "sha256": digest(target),
            }
        report = {"formatVersion": 1, "materials": staged_materials, "images": staged_images, "pending": pending}
        write(staged / "pbr-materials.json", report)
        publish_directory(staged, output)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--materials", type=Path, required=True)
    parser.add_argument("--images", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    materials = json.loads(args.materials.read_text(encoding="utf-8"))["slots"]
    report = stage_materials(materials, args.images, args.output)
    print(json.dumps({key: len(report[key]) for key in ("materials", "images", "pending")}))


if __name__ == "__main__":
    main()
