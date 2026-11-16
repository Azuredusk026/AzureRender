"""Stage compact submesh ranges from Unity's world-space static mesh batches."""

import argparse
import json
from pathlib import Path
import tempfile

from tools.build_face_sdf import write_glb
from tools.npr_room.catalog_storage import publish_directory
from tools.npr_room.mesh_catalog import digest
from tools.npr_room.mesh_export import export_mesh_geometry, read_unity_mesh
from tools.npr_room.skinned_mesh_export import UnityScene


def batch_slot(guid, first, count):
    return f"{guid}-{first}-{count}"


def find_batches(scene):
    ranges = {}
    for renderer_id, (kind, _, _) in scene.documents.items():
        if kind != 23:
            continue
        renderer = scene.read(renderer_id)
        batch = renderer.get("m_StaticBatchInfo", {})
        count = batch.get("subMeshCount", 0)
        if not count:
            continue
        if renderer["m_StaticBatchRoot"]["fileID"] != 0:
            raise ValueError(f"Static batch root requires coordinate conversion: {renderer_id}")
        game = scene.read(renderer["m_GameObject"]["fileID"])
        filters = [
            item["component"]["fileID"] for item in game["m_Component"]
            if scene.documents.get(item["component"]["fileID"], (None,))[0] == 33
        ]
        filter_id, = filters
        guid = scene.read(filter_id)["m_Mesh"]["guid"]
        first = batch["firstSubMesh"]
        entry = ranges.setdefault(batch_slot(guid, first, count), {
            "sourceGuid": guid, "firstSubMesh": first, "subMeshCount": count,
            "renderers": [],
        })
        entry["renderers"].append(renderer_id)
    return ranges


def stage_batches(project, scene, mesh_slots, output):
    if output.exists():
        raise FileExistsError(output)
    project = project.resolve(strict=True)
    ranges = find_batches(scene)
    meshes = {}
    with tempfile.TemporaryDirectory(prefix="recovery-batches-", dir=output.parent) as directory:
        candidate = Path(directory) / "publish"
        candidate.mkdir()
        for key, row in ranges.items():
            guid = row["sourceGuid"]
            slot = mesh_slots[guid]
            if slot["kind"] != "static":
                raise ValueError(f"Batch source needs static geometry: {guid}")
            if guid not in meshes:
                source = (project / slot["source"]).resolve(strict=True)
                if not source.is_relative_to(project / "Assets/Mesh"):
                    raise ValueError(f"Static batch source is outside Assets/Mesh: {guid}")
                if digest(source) != slot["sourceSHA256"]:
                    raise ValueError(f"Static batch source changed: {guid}")
                mesh = read_unity_mesh(source)
                if digest(source) != slot["sourceSHA256"]:
                    raise ValueError(f"Static batch source changed while reading: {guid}")
                meshes[guid] = mesh
            document, binary = export_mesh_geometry(
                meshes[guid], submesh_range=(row["firstSubMesh"], row["subMeshCount"]))
            glb = candidate / f"{key}.glb"
            write_glb(glb, document, binary)
            row.update(media=f"batches/{key}.glb", sha256=digest(glb),
                       bytes=glb.stat().st_size, coordinates="world")
        summary = {
            "rendererRanges": sum(len(row["renderers"]) for row in ranges.values()),
            "uniqueRanges": len(ranges), "sourceMeshes": len(meshes),
            "mediaBytes": sum(row["bytes"] for row in ranges.values()),
        }
        (candidate / "static-batches.json").write_text(json.dumps({
            "formatVersion": 1, "summary": summary, "slots": ranges,
        }, indent=2, sort_keys=True) + "\n", encoding="utf-8", newline="\n")
        publish_directory(candidate, output)
    return summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--scene", type=Path, required=True)
    parser.add_argument("--catalog", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    catalog = json.loads(args.catalog.read_text(encoding="utf-8"))
    print(json.dumps(stage_batches(args.source, UnityScene(args.scene), catalog["slots"], args.output)))


if __name__ == "__main__":
    main()
