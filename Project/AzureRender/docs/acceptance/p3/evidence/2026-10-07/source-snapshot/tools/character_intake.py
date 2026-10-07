"""Audit parameterized private or public character sources; never copy media."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path
from audit_face_sdf_compatibility import load_gltf


def audit_gltf(document):
    nodes = document.get("nodes", [])
    skeletons = []
    for skin in document.get("skins", []):
        joints = skin["joints"]
        if len(set(joints)) != len(joints) or any(type(i) is not int or i < 0 or i >= len(nodes) for i in joints):
            raise ValueError("Invalid skin joint reference")
        skeletons.append([nodes[i].get("name", str(i)) for i in joints])
    accessors = document.get("accessors", [])
    vertices = triangles = 0
    for mesh in document.get("meshes", []):
        for primitive in mesh["primitives"]:
            count = accessors[primitive["attributes"]["POSITION"]]["count"]
            vertices += count
            if primitive.get("mode", 4) == 4:
                triangles += (accessors[primitive["indices"]]["count"] if "indices" in primitive else count) // 3
    animations = [{"name": a.get("name", str(i)), "channels": len(a["channels"]),
        "interpolation": sorted({s.get("interpolation", "LINEAR") for s in a["samplers"]})}
        for i, a in enumerate(document.get("animations", []))]
    fingerprint = hashlib.sha256(json.dumps(skeletons, ensure_ascii=False).encode()).hexdigest()
    return {"format": "glTF2", "skeleton": skeletons, "skeletonFingerprint": fingerprint,
        "jointCount": sum(map(len, skeletons)), "vertices": vertices, "triangles": triangles,
        "materials": [m.get("name", "") for m in document.get("materials", [])],
        "textures": len(document.get("textures", [])), "animations": animations}


def audit_fbx(path, sampler):
    result = subprocess.run([str(sampler.resolve()), str(path.resolve())], capture_output=True, check=True)
    scene = json.loads(result.stdout)
    names = [node["name"] for node in scene["nodes"] if node["bone"]]
    return {"format": "FBX", "parser": scene["parser"], "unitMeters": scene["sourceUnitMeters"],
        "jointCount": len(names), "skeleton": names,
        "skeletonFingerprint": hashlib.sha256(json.dumps(names).encode()).hexdigest(),
        "animations": [{"name": a["name"], "duration": a["duration"], "sampleRate": a["sampleRate"],
            "samples": len(a["frames"])} for a in scene["clips"]]}


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--asset", type=Path, required=True)
    parser.add_argument("--idle", type=Path)
    parser.add_argument("--walk", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--sampler", type=Path, default=Path("build/f1/azure_fbx_sample.exe"))
    args = parser.parse_args()
    result = {"schemaVersion": 1, "distribution": "local-user-provided", "sources": {}}
    for semantic in ["asset", "idle", "walk"]:
        path = getattr(args, semantic)
        if path is None:
            result["sources"][semantic] = {"status": "missing"}
            continue
        item = audit_fbx(path, args.sampler) if path.suffix.lower() == ".fbx" else audit_gltf(load_gltf(path))
        item.update(status="audited", filename=path.name, bytes=path.stat().st_size,
            sha256=hashlib.sha256(path.read_bytes()).hexdigest())
        result["sources"][semantic] = item
    result["retargetRequired"] = any(result["sources"].get(name, {}).get("skeletonFingerprint") !=
        result["sources"]["asset"]["skeletonFingerprint"] for name in ["idle", "walk"])
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")
    print(json.dumps({"output": str(args.output), "retargetRequired": result["retargetRequired"]}))


if __name__ == "__main__":
    main()
