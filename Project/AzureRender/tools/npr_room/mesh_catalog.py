"""Stage the static mesh portion of the locked Recovery scene inventory."""

import argparse
from collections import Counter
from hashlib import file_digest
import json
from pathlib import Path
import tempfile

from tools.build_face_sdf import write_glb
from tools.npr_room.catalog_storage import publish_directory
from tools.npr_room.mesh_export import export_mesh_geometry, read_unity_mesh
from tools.npr_room.reference_inventory import META_GUID, text


def conversion_kind(mesh):
    if mesh["m_BindPose"]:
        return "skin"
    if mesh.get("m_Shapes", {}).get("shapes"):
        return "morph"
    return "static"


def digest(path):
    with path.open("rb") as stream:
        return file_digest(stream, "sha256").hexdigest()


def stage_mesh_catalog(project, report, output):
    if output.exists():
        raise FileExistsError(output)
    project = project.resolve(strict=True)
    asset_root = project / "Assets/Mesh"
    slots, counts = {}, Counter()
    with tempfile.TemporaryDirectory(prefix="recovery-mesh-", dir=output.parent) as directory:
        candidate = Path(directory) / "publish"
        media = candidate / "meshes"
        media.mkdir(parents=True)
        for name in sorted(report["additionalClosure"]):
            if not name.startswith("Assets/Mesh/"):
                continue
            asset = (project / name).resolve(strict=True)
            if not asset.is_relative_to(asset_root):
                raise ValueError(f"Mesh resolves outside the source asset root: {name}")
            guid = report["assets"][name]
            identity = META_GUID.search(text(asset.with_name(asset.name + ".meta")))
            if not identity or identity[1].lower() != guid or guid in slots:
                raise ValueError(f"Mesh identity changed or is ambiguous: {name}")

            source_hash = digest(asset)
            mesh = read_unity_mesh(asset)
            kind = conversion_kind(mesh)
            counts[kind] += 1
            slot = {"source": name, "sourceSHA256": source_hash, "kind": kind}
            slots[guid] = slot
            if kind == "static":
                gltf, binary = export_mesh_geometry(mesh)
                target = media / f"{guid}.glb"
                write_glb(target, gltf, binary)
                slot.update(media=f"meshes/{target.name}", sha256=digest(target),
                            bytes=target.stat().st_size)
            if digest(asset) != source_hash:
                raise ValueError(f"Mesh source changed during conversion: {name}")

        summary = {
            "sourceSlots": len(slots),
            "staticMeshes": counts["static"],
            "skinRequired": counts["skin"],
            "morphRequired": counts["morph"],
            "mediaBytes": sum(slot.get("bytes", 0) for slot in slots.values()),
        }
        (candidate / "mesh-catalog.json").write_text(json.dumps({
            "formatVersion": 1, "summary": summary, "slots": slots,
        }, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n",
            encoding="utf-8", newline="\n")
        publish_directory(candidate, output)
    return summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--inventory", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    report = json.loads(args.inventory.read_text(encoding="utf-8"))
    print(json.dumps(stage_mesh_catalog(args.source, report, args.output)))


if __name__ == "__main__":
    main()
