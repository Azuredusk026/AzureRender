"""Read Unity's own embedded static mesh and emit a local native resource."""

import argparse
import json
from pathlib import Path
import tempfile

from tools.build_face_sdf import write_glb
from tools.npr_room.catalog_storage import publish_directory
from tools.npr_room.mesh_catalog import digest
from tools.npr_room.mesh_export import export_mesh_geometry
from tools.npr_room.reference_inventory import BUILTIN_GUIDS


def entry_key(guid, file_id):
    return f"{guid}:{file_id}" if guid in BUILTIN_GUIDS else guid


def export_builtin(source, guid, file_id, output):
    import UnityPy

    if UnityPy.__version__ != "1.25.0":
        raise ValueError("Built-in mesh extraction requires UnityPy 1.25.0")
    identity = digest(source)
    objects = UnityPy.load(str(source)).objects
    matching = [obj for obj in objects if obj.path_id == file_id and obj.type.name == "Mesh"]
    if len(matching) != 1:
        raise ValueError(f"Expected exactly one built-in mesh for file ID {file_id}")
    obj = matching[0]
    mesh = obj.parse_as_dict()
    if mesh["m_BindPose"] or mesh["m_Shapes"]["shapes"]:
        raise ValueError("Built-in asset must be a static mesh")
    mesh["m_IndexBuffer"] = bytes(mesh["m_IndexBuffer"]).hex()
    vertex = mesh["m_VertexData"]
    payload = vertex["m_DataSize"]
    vertex["m_DataSize"], vertex["_typelessdata"] = len(payload), payload.hex()
    gltf, binary = export_mesh_geometry(mesh)
    if digest(source) != identity:
        raise ValueError("Built-in source changed while reading")

    with tempfile.TemporaryDirectory(prefix="recovery-builtin-", dir=output.parent) as directory:
        staged = Path(directory) / "publish"
        staged.mkdir()
        file = staged / f"{file_id}.glb"
        write_glb(file, gltf, binary)
        metadata = {
            "formatVersion": 1,
            "sourceSHA256": identity,
            "containerVersion": obj.assets_file.unity_version,
            "slots": {
                entry_key(guid, file_id): {
                    "kind": "static", "name": mesh["m_Name"],
                    "media": f"builtins/{file_id}.glb", "sha256": digest(file),
                    "vertices": vertex["m_VertexCount"],
                },
            },
        }
        (staged / "builtins.json").write_text(json.dumps(metadata, indent=2) + "\n",
                                              encoding="utf-8", newline="\n")
        publish_directory(staged, output)
    return metadata


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--guid", choices=sorted(BUILTIN_GUIDS), required=True)
    parser.add_argument("--file-id", type=int, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(export_builtin(args.source, args.guid, args.file_id, args.output)))


if __name__ == "__main__":
    main()
