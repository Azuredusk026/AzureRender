"""Audit parameterized private or public character sources; never copy media."""
import argparse
import hashlib
import json
import subprocess
import struct
import base64
import re
from pathlib import Path
from audit_face_sdf_compatibility import load_gltf


def validate_manifest(manifest):
    if manifest.get("schemaVersion") != 1:
        raise ValueError("Unsupported manifest version")
    for field in ("sources", "tools", "settings", "outputs", "counts", "losses", "evidence", "sourceKind"):
        if field not in manifest:
            raise ValueError(f"Missing manifest field: {field}")
    if manifest["sourceKind"] not in {"game-native", "retargeted", "procedural", "hand-authored"}:
        raise ValueError("Invalid sourceKind")
    entries = manifest["sources"] + manifest["tools"] + manifest["outputs"]
    ids = [entry.get("id") for entry in entries]
    if any(not isinstance(value, str) or not value for value in ids) or len(ids) != len(set(ids)):
        raise ValueError("Invalid or duplicate entry id")
    for entry in entries:
        if not re.fullmatch(r"[0-9a-f]{64}", entry.get("sha256", "")):
            raise ValueError(f"Missing or invalid sha256: {entry['id']}")
        for dependency in entry.get("dependencies", []):
            if dependency not in ids:
                raise ValueError(f"Missing dependency: {dependency}")
    return manifest


def gltf_buffers(path, document):
    data = path.read_bytes()
    binary = None
    if path.suffix.lower() == ".glb":
        cursor = 12
        while cursor + 8 <= len(data):
            length, kind = struct.unpack_from("<II", data, cursor)
            if kind == 0x004E4942:
                binary = data[cursor + 8:cursor + 8 + length]
            cursor += 8 + length
    buffers = []
    for source in document.get("buffers", []):
        uri = source.get("uri")
        if uri is None:
            if binary is None:
                raise ValueError("Missing binary buffer")
            buffers.append(binary)
        elif uri.startswith("data:"):
            buffers.append(base64.b64decode(uri.split(",", 1)[1], validate=True))
        else:
            buffers.append((path.parent / uri).read_bytes())
    return buffers


def audit_gltf(document, buffers=None):
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
    parents = {}
    for parent, node in enumerate(nodes):
        for child in node.get("children", []):
            if type(child) is not int or not 0 <= child < len(nodes) or child in parents:
                raise ValueError("Invalid node hierarchy")
            parents[child] = parent
    def node_path(index):
        chain, seen = [], set()
        while True:
            if index in seen:
                raise ValueError("Cyclic node hierarchy")
            seen.add(index)
            chain.append([index, nodes[index].get("name", "")])
            if index not in parents:
                return list(reversed(chain))
            index = parents[index]
    hierarchy = [{"path": node_path(i), "rest": {key: n[key] for key in
        ("matrix", "translation", "rotation", "scale") if key in n}} for i, n in enumerate(nodes)]
    bindings = []
    for skin in document.get("skins", []):
        values = [[1.0 if i % 5 == 0 else 0.0 for i in range(16)]] * len(skin["joints"])
        if "inverseBindMatrices" in skin:
            if buffers is None:
                raise ValueError("Binary buffers required for inverse bind fingerprint")
            accessor = accessors[skin["inverseBindMatrices"]]
            if accessor.get("type") != "MAT4" or accessor.get("componentType") != 5126 or "sparse" in accessor:
                raise ValueError("Unsupported inverse bind accessor")
            if accessor["count"] != len(skin["joints"]):
                raise ValueError("Inverse bind count differs from joints")
            view = document["bufferViews"][accessor["bufferView"]]
            start = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
            values = [struct.unpack_from("<16f", buffers[view["buffer"]], start + i * view.get("byteStride", 64))
                for i in range(accessor["count"])]
        bindings.append({"joints": skin["joints"], "inverseBindMatrices": values})
    fingerprint = hashlib.sha256(json.dumps([hierarchy, bindings], ensure_ascii=False, sort_keys=True).encode()).hexdigest()
    primitives = [p for m in document.get("meshes", []) for p in m.get("primitives", [])]
    return {"format": "glTF2", "skeleton": skeletons, "skeletonFingerprint": fingerprint,
        "jointCount": sum(map(len, skeletons)), "vertices": vertices, "triangles": triangles,
        "materials": [m.get("name", "") for m in document.get("materials", [])],
        "textures": len(document.get("textures", [])), "animations": animations,
        "skinCount": len(skeletons), "morphTargetCounts": [len(p.get("targets", [])) for p in primitives],
        "sparseAccessorCount": sum("sparse" in a for a in accessors),
        "secondaryJointPrimitiveCount": sum("JOINTS_1" in p["attributes"] for p in primitives),
        "secondaryWeightPrimitiveCount": sum("WEIGHTS_1" in p["attributes"] for p in primitives),
        "faceBoneCandidates": [n.get("name", "") for n in nodes if any(token in n.get("name", "").lower()
            for token in ("eyelid", "lip", "jaw", "mouth", "brow"))]}


def audit_fbx(path, sampler):
    result = subprocess.run([str(sampler.resolve()), str(path.resolve())], capture_output=True, check=True)
    scene = json.loads(result.stdout)
    names = [node["name"] for node in scene["nodes"] if node["bone"]]
    return {"format": "FBX", "parser": scene["parser"], "unitMeters": scene["sourceUnitMeters"],
        "jointCount": len(names), "skeleton": names,
        "skeletonFingerprint": hashlib.sha256(json.dumps(names).encode()).hexdigest(),
        "animations": [{"name": a["name"], "duration": a["duration"], "sampleRate": a["sampleRate"],
            "samples": len(a["frames"])} for a in scene["clips"]]}


def retarget_required(sources):
    return any(source.get("skeletonFingerprint") != sources["asset"]["skeletonFingerprint"]
        for name, source in sources.items() if name != "asset" and source.get("status") != "missing")


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
        if path.suffix.lower() == ".fbx":
            item = audit_fbx(path, args.sampler)
        else:
            document = load_gltf(path)
            item = audit_gltf(document, gltf_buffers(path, document))
        item.update(status="audited", filename=path.name, bytes=path.stat().st_size,
            sha256=hashlib.sha256(path.read_bytes()).hexdigest())
        result["sources"][semantic] = item
    result["retargetRequired"] = retarget_required(result["sources"])
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")
    print(json.dumps({"output": str(args.output), "retargetRequired": result["retargetRequired"]}))


if __name__ == "__main__":
    main()
