"""Stage renderer-specific GLBs for the catalog's skin and morph sources."""

import argparse
from collections import Counter
import json
from pathlib import Path
import re
import tempfile

from tools.build_face_sdf import write_glb
from tools.npr_room.catalog_storage import publish_directory
from tools.npr_room.mesh_catalog import digest
from tools.npr_room.mesh_export import export_mesh_geometry, read_unity_mesh
from tools.npr_room.skinned_mesh_export import UnityScene, append_morphs, append_skin


MESH_GUID = re.compile(r"(?m)^  m_Mesh: .*guid: ([0-9a-f]{32}),")


def stage_deformers(project, slots, output):
    if output.exists():
        raise FileExistsError(output)
    project = project.resolve(strict=True)
    pending = {guid: row for guid, row in slots.items() if row["kind"] != "static"}
    variants, counts, observed = {}, Counter(), set()
    source_scenes = sorted((project / "Assets/App/Scenes").glob("*.unity"))
    source_prefabs = sorted((project / "Assets/GameObject").glob("*.prefab"))
    meshes = {}

    with tempfile.TemporaryDirectory(prefix="recovery-deformers-", dir=output.parent) as directory:
        candidate = Path(directory) / "publish"
        candidate.mkdir()
        for scene_path in [*source_scenes, *source_prefabs]:
            scene = UnityScene(scene_path)
            scene_name = scene_path.relative_to(project).as_posix()
            for renderer_id, (kind, start, end) in scene.documents.items():
                if kind != 137:
                    continue
                match = MESH_GUID.search(scene.serialized, start, end)
                if not match or match[1] not in pending:
                    continue
                guid = match[1]
                slot = pending[guid]
                if guid not in meshes:
                    source = (project / slot["source"]).resolve(strict=True)
                    if not source.is_relative_to(project / "Assets/Mesh"):
                        raise ValueError(f"Renderer mesh is outside the source mesh folder: {guid}")
                    if digest(source) != slot["sourceSHA256"]:
                        raise ValueError(f"Renderer mesh source changed: {guid}")
                    meshes[guid] = read_unity_mesh(source)
                    if digest(source) != slot["sourceSHA256"]:
                        raise ValueError(f"Renderer mesh changed while reading: {guid}")
                mesh = meshes[guid]
                renderer = scene.read(renderer_id)
                gltf, body = export_mesh_geometry(mesh)
                binary = bytearray(body)
                if slot["kind"] == "skin":
                    nodes, bones, mesh_node = scene.skin_hierarchy(renderer)
                    append_skin(gltf, binary, mesh, nodes, bones, mesh_node)
                append_morphs(gltf, binary, mesh, renderer["m_BlendShapeWeights"])
                gltf["buffers"][0]["byteLength"] = len(binary)
                target = candidate / f"{guid}-{renderer_id}.glb"
                if target.exists():
                    raise ValueError(f"Renderer output is ambiguous: {target.name}")
                write_glb(target, gltf, binary)
                variants[f"{scene_name}:{renderer_id}"] = {
                    "sourceGuid": guid, "kind": slot["kind"],
                    "media": target.name, "sha256": digest(target), "bytes": target.stat().st_size,
                    "joints": len(mesh["m_BindPose"]),
                    "morphs": len(gltf["meshes"][0].get("weights", [])),
                }
                counts[slot["kind"]] += 1
                observed.add(guid)
        missing = pending.keys() - observed
        if missing:
            raise ValueError(f"Missing skin or morph renderer for {', '.join(sorted(missing))}")
        summary = {
            "sourceMeshes": len(observed), "rendererVariants": len(variants),
            "skinVariants": counts["skin"], "morphVariants": counts["morph"],
            "mediaBytes": sum(row["bytes"] for row in variants.values()),
        }
        (candidate / "deformer-catalog.json").write_text(json.dumps({
            "formatVersion": 1, "summary": summary, "variants": variants,
        }, indent=2, sort_keys=True) + "\n", encoding="utf-8", newline="\n")
        publish_directory(candidate, output)
    return summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--catalog", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    slots = json.loads(args.catalog.read_text(encoding="utf-8"))["slots"]
    print(json.dumps(stage_deformers(args.source, slots, args.output)))


if __name__ == "__main__":
    main()
