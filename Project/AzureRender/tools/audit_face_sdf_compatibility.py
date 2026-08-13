#!/usr/bin/env python3
"""Audit whether an AzureRender glTF/GLB has explicit Face SDF inputs."""

from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path


def load_gltf(path: Path) -> dict:
    if path.suffix.lower() == ".gltf":
        return json.loads(path.read_text(encoding="utf-8-sig"))
    data = path.read_bytes()
    if len(data) < 20 or data[:4] != b"glTF":
        raise ValueError(f"Not a glTF 2.0 GLB: {path}")
    version = struct.unpack_from("<I", data, 4)[0]
    chunk_length, chunk_type = struct.unpack_from("<II", data, 12)
    if version != 2 or chunk_type != 0x4E4F534A:
        raise ValueError(f"GLB lacks a leading JSON chunk: {path}")
    return json.loads(data[20 : 20 + chunk_length].decode("utf-8").rstrip("\x00 "))


def texture_source(document: dict, texture_index: object) -> tuple[bool, str]:
    if type(texture_index) is not int:
        return False, "texture index is not an integer"
    textures = document.get("textures", [])
    if texture_index < 0 or texture_index >= len(textures):
        return False, f"texture index {texture_index} is out of range"
    source_index = textures[texture_index].get("source")
    images = document.get("images", [])
    if not isinstance(source_index, int) or not 0 <= source_index < len(images):
        return False, f"texture {texture_index} has no valid image source"
    image = images[source_index]
    source = image.get("uri") or image.get("name")
    if source is None and "bufferView" in image:
        source = f"bufferView:{image['bufferView']}"
    return True, str(source or f"image:{source_index}")


def audit(document: dict) -> dict:
    materials = document.get("materials", [])
    meshes = document.get("meshes", [])
    nodes = document.get("nodes", [])
    node_name_counts: dict[str, int] = {}
    for node in nodes:
        name = node.get("name")
        if isinstance(name, str) and name:
            node_name_counts[name] = node_name_counts.get(name, 0) + 1
    node_names = set(node_name_counts)
    head_node_candidates = sorted(
        name
        for name in node_names
        if any(token in name.lower() for token in ("head", "face", "neck"))
    )
    face_materials: list[dict] = []

    for index, material in enumerate(materials):
        profile = material.get("extras", {}).get("azureRenderMaterial", {})
        features = profile.get("features", [])
        if profile.get("class") != "face" and "face-sdf-eligible" not in features:
            continue
        face_materials.append(
            {
                "index": index,
                "name": material.get("name") or f"material-{index}",
                "profile": profile,
                "primitives": [],
            }
        )

    by_index = {entry["index"]: entry for entry in face_materials}
    for mesh_index, mesh in enumerate(meshes):
        for primitive_index, primitive in enumerate(mesh.get("primitives", [])):
            material_index = primitive.get("material")
            if material_index not in by_index:
                continue
            by_index[material_index]["primitives"].append(
                {
                    "mesh": mesh_index,
                    "primitive": primitive_index,
                    "hasTexcoord0": "TEXCOORD_0" in primitive.get("attributes", {}),
                }
            )

    issues: list[str] = []
    if not face_materials:
        issues.append("no face or face-sdf-eligible material profile")

    material_reports: list[dict] = []
    for entry in face_materials:
        name = entry["name"]
        profile = entry["profile"]
        face_sdf = profile.get("faceSdf")
        material_issues: list[str] = []
        if not entry["primitives"]:
            material_issues.append("material is not referenced by a mesh primitive")
        if any(not primitive["hasTexcoord0"] for primitive in entry["primitives"]):
            material_issues.append("one or more face primitives lack TEXCOORD_0")
        texture_name = None
        if not isinstance(face_sdf, dict):
            material_issues.append("missing azureRenderMaterial.faceSdf metadata")
        else:
            if face_sdf.get("schemaVersion") != 1:
                material_issues.append("unsupported faceSdf schemaVersion")
            texture_valid, texture_name = texture_source(
                document, face_sdf.get("texture")
            )
            if not texture_valid:
                material_issues.append(texture_name)
            head_node = face_sdf.get("headNode")
            if not isinstance(head_node, str) or head_node not in node_names:
                material_issues.append(f"head node {head_node!r} does not resolve")
            elif node_name_counts[head_node] != 1:
                material_issues.append(f"head node {head_node!r} is not unique")
            if face_sdf.get("texCoord") != 0:
                material_issues.append("only TEXCOORD_0 is supported in Face SDF v1")
            if face_sdf.get("channel") not in {"r", "g", "b", "a"}:
                material_issues.append("invalid Face SDF channel")
            if face_sdf.get("horizontalAxis") not in {
                "left-to-right",
                "right-to-left",
            }:
                material_issues.append("invalid Face SDF horizontalAxis")
            if not isinstance(face_sdf.get("shadowOnLowValues"), bool):
                material_issues.append("shadowOnLowValues must be boolean")
        issues.extend(f"{name}: {issue}" for issue in material_issues)
        material_reports.append(
            {
                "index": entry["index"],
                "name": name,
                "primitiveCount": len(entry["primitives"]),
                "texture": texture_name,
                "compatible": not material_issues,
                "issues": material_issues,
            }
        )

    return {
        "format": "AzureRender Face SDF Compatibility Audit v1",
        "compatible": bool(face_materials) and not issues,
        "faceMaterialCount": len(face_materials),
        "headNodeCandidates": head_node_candidates,
        "materials": material_reports,
        "issues": issues,
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("asset", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument(
        "--require-compatible",
        action="store_true",
        help="return a failing exit code when the asset is not compatible",
    )
    args = parser.parse_args()

    report = audit(load_gltf(args.asset))
    report["asset"] = str(args.asset)
    encoded = json.dumps(report, indent=2, ensure_ascii=True) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(encoded, encoding="utf-8")
    print(encoded, end="")
    if args.require_compatible and not report["compatible"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
