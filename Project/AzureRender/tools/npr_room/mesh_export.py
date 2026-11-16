"""Convert an inline, uncompressed Unity static mesh to an indexed glTF mesh."""

import argparse
import json
import math
from pathlib import Path
from struct import Struct, iter_unpack, pack

import yaml

from tools.build_face_sdf import write_glb


FORMATS = {
    0: ("f", 1.0),
    1: ("e", 1.0),
    2: ("B", 1 / 255),
    3: ("b", 1 / 127),
    4: ("H", 1 / 65535),
    5: ("h", 1 / 32767),
    10: ("I", 1.0),
}
ATTRIBUTES = (
    (0, "POSITION", 3),
    (1, "NORMAL", 3),
    (2, "TANGENT", 4),
    (3, "COLOR_0", 4),
    (4, "TEXCOORD_0", 2),
    (5, "TEXCOORD_1", 2),
)


def read_unity_mesh(path):
    with path.open(encoding="utf-8-sig") as source:
        for line in source:
            if line.startswith("--- !u!43 "):
                break
        else:
            raise ValueError(f"{path.name}: expected a Unity Mesh object")
        return yaml.load(source, Loader=getattr(yaml, "CBaseLoader", yaml.BaseLoader))["Mesh"]


def vertex_channels(vertex_data):
    data = bytes.fromhex(vertex_data["_typelessdata"])
    count = int(vertex_data["m_VertexCount"])
    if not count or len(data) != int(vertex_data["m_DataSize"]):
        raise ValueError("Missing inline vertex data")
    channels = {}
    strides = {}
    for index, channel in enumerate(vertex_data["m_Channels"]):
        dimension = int(channel["dimension"]) & 0xF
        if not dimension:
            continue
        stream, offset = int(channel["stream"]), int(channel["offset"])
        component, factor = FORMATS[int(channel["format"])]
        layout = Struct(f"<{dimension}{component}")
        strides[stream] = max(strides.get(stream, 0), offset + layout.size)
        channels[index] = (stream, offset, layout, dimension, factor)

    offsets = {}
    start = 0
    for stream, stride in sorted(strides.items()):
        offsets[stream] = start
        end = start + stride * count
        if end > len(data):
            raise ValueError(f"Vertex stream {stream} is truncated")
        start = (end + 15) & ~15
    return data, count, channels, strides, offsets


def attributes(mesh):
    data, count, channels, strides, offsets = vertex_channels(mesh["m_VertexData"])
    if 0 not in channels or not channels[0][3] == 3:
        raise ValueError("Mesh requires three-component positions")
    for index, semantic, size in ATTRIBUTES:
        if index not in channels:
            continue
        stream, offset, layout, dimension, factor = channels[index]
        if dimension < size:
            raise ValueError(f"Unsupported dimension for {semantic}: {dimension}")
        start = offsets[stream] + offset
        stride = strides[stream]
        rows = []
        for vertex in range(count):
            values = [value * factor for value in layout.unpack_from(data, start + vertex * stride)[:size]]
            if not all(map(math.isfinite, values)):
                raise ValueError(f"Non-finite {semantic} at vertex {vertex}")
            if semantic in ("POSITION", "NORMAL", "TANGENT"):
                values[2] = -values[2]
            if semantic in ("NORMAL", "TANGENT"):
                length = math.hypot(*values[:3])
                if length == 0:
                    raise ValueError(f"Zero-length {semantic} at vertex {vertex}")
                values[:3] = [value / length for value in values[:3]]
            if semantic.startswith("TEXCOORD_"):
                values[1] = 1 - values[1]
            rows.append(values)
        yield semantic, size, rows


def indices(mesh, vertex_count):
    index_format = int(mesh["m_IndexFormat"])
    if index_format not in (0, 1):
        raise ValueError(f"Unsupported index format: {index_format}")
    source_type = "H" if index_format == 0 else "I"
    item_size = 2 if index_format == 0 else 4
    source = bytes.fromhex(mesh["m_IndexBuffer"])
    for mesh_range in mesh["m_SubMeshes"]:
        start = int(mesh_range["firstByte"])
        count = int(mesh_range["indexCount"])
        base = int(mesh_range["baseVertex"])
        if (int(mesh_range["topology"]) != 0 or count <= 0 or count % 3
                or start < 0 or start % item_size or start + count * item_size > len(source)):
            raise ValueError("Invalid or non-triangular submesh")
        raw = source[start:start + count * item_size]
        result = [index + base for a, b, c in iter_unpack(f"<3{source_type}", raw) for index in (a, c, b)]
        if min(result) < 0 or max(result) >= vertex_count:
            raise ValueError("Submesh index exceeds vertex range")
        yield result


def gltf_motion(motion, add_accessor):
    samples = motion["samples"]
    if len(samples) < 2:
        raise ValueError("Motion requires at least two samples")
    times, positions, rotations = [], [], []
    for sample in samples:
        time = Struct("<f").unpack(pack("<f", sample["time"]))[0]
        px, py, pz = sample["position"]
        qx, qy, qz, qw = sample["rotation"]
        if (not all(map(math.isfinite, (time, px, py, pz, qx, qy, qz, qw)))
                or time < 0 or (times and time <= times[-1])):
            raise ValueError("Motion samples require finite values and strictly increasing times")
        length = math.hypot(qx, qy, qz, qw)
        if length == 0:
            raise ValueError("Motion contains a zero quaternion")
        rotation = [-qx / length, -qy / length, qz / length, qw / length]
        if rotations and sum(a * b for a, b in zip(rotation, rotations[-1])) < 0:
            rotation = [-value for value in rotation]
        times.append(time)
        positions.append([px, py, -pz])
        rotations.append(rotation)

    time_index = add_accessor(pack(f"<{len(times)}f", *times), len(times), "SCALAR", 5126)
    tracks = [("translation", "VEC3", positions), ("rotation", "VEC4", rotations)]
    animation = {"name": motion["name"], "channels": [], "samplers": []}
    for sampler, (path, value_type, rows) in enumerate(tracks):
        payload = b"".join(pack(f"<{len(row)}f", *row) for row in rows)
        output_index = add_accessor(payload, len(rows), value_type, 5126)
        animation["samplers"].append({"input": time_index, "output": output_index, "interpolation": "LINEAR"})
        animation["channels"].append({"sampler": sampler, "target": {"node": 0, "path": path}})
    return animation, time_index, [times[0]], [times[-1]]


def export_static_mesh(path, base_color_textures=(), motion=None):
    mesh = read_unity_mesh(path)
    if mesh["m_BindPose"] or mesh.get("m_Shapes", {}).get("shapes"):
        raise ValueError("Expected a mesh without skin or morph targets")
    return export_mesh_geometry(mesh, base_color_textures, motion)


def export_mesh_geometry(mesh, base_color_textures=(), motion=None):
    if int(mesh["m_MeshCompression"]):
        raise ValueError("Expected uncompressed mesh geometry")

    name = mesh["m_Name"]
    binary = bytearray()
    gltf = {
        "asset": {"version": "2.0"},
        "scenes": [{"nodes": [0]}],
        "scene": 0,
        "nodes": [{"name": name, "mesh": 0}],
        "meshes": [{"name": name, "primitives": []}],
        "materials": [],
        "accessors": [],
        "bufferViews": [],
    }

    def add_view(payload, target=None):
        binary.extend(b"\0" * (-len(binary) % 4))
        view = {"buffer": 0, "byteOffset": len(binary), "byteLength": len(payload)}
        if target is not None:
            view["target"] = target
        gltf["bufferViews"].append(view)
        binary.extend(payload)
        return len(gltf["bufferViews"]) - 1

    def add_accessor(payload, count, value_type, component_type, target=None):
        index = add_view(payload, target)
        gltf["accessors"].append({
            "bufferView": index, "componentType": component_type, "count": count, "type": value_type
        })
        return len(gltf["accessors"]) - 1

    attribute_ids = {}
    vertex_count = int(mesh["m_VertexData"]["m_VertexCount"])
    for semantic, size, rows in attributes(mesh):
        layout = Struct(f"<{size}f")
        payload = b"".join(layout.pack(*row) for row in rows)
        index = add_accessor(payload, vertex_count, f"VEC{size}", 5126, 34962)
        attribute_ids[semantic] = index
        if semantic == "POSITION":
            axes = list(zip(*iter_unpack("<3f", payload)))
            gltf["accessors"][index].update(
                min=[min(axis) for axis in axes], max=[max(axis) for axis in axes]
            )
    for slot, values in enumerate(indices(mesh, vertex_count)):
        wide = max(values) >= 65535
        payload = pack(f"<{len(values)}{'I' if wide else 'H'}", *values)
        index = add_accessor(payload, len(values), "SCALAR", 5125 if wide else 5123, 34963)
        gltf["materials"].append({"name": f"unity-slot-{slot}"})
        gltf["meshes"][0]["primitives"].append(
            {"attributes": attribute_ids, "indices": index, "material": slot}
        )
    if not gltf["materials"]:
        raise ValueError("Mesh has no triangle submeshes")
    if base_color_textures:
        if len(base_color_textures) != len(gltf["materials"]) or "TEXCOORD_0" not in attribute_ids:
            raise ValueError("Provide one base color PNG per material slot and UV0")
        gltf["images"] = []
        gltf["textures"] = []
        for slot, texture in enumerate(base_color_textures):
            png = texture.read_bytes()
            if not png.startswith(b"\x89PNG\r\n\x1a\n"):
                raise ValueError(f"Expected a PNG: {texture.name}")
            view = add_view(png)
            gltf["images"].append({"bufferView": view, "mimeType": "image/png"})
            gltf["textures"].append({"source": slot})
            gltf["materials"][slot]["pbrMetallicRoughness"] = {
                "baseColorTexture": {"index": slot}, "metallicFactor": 0.0, "roughnessFactor": 1.0,
            }
    if motion is not None:
        animation, time_index, first, last = gltf_motion(motion, add_accessor)
        gltf["accessors"][time_index].update(min=first, max=last)
        gltf["animations"] = [animation]
    gltf["buffers"] = [{"byteLength": len(binary)}]
    return gltf, bytes(binary)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--base-color-png", type=Path, action="append", default=[],
                        help="Repeat once per submesh, in material slot order")
    parser.add_argument("--motion-reference", type=Path)
    parser.add_argument("--clip", help="Clip basename in the Recovery motion reference")
    args = parser.parse_args()
    if args.output.suffix != ".glb" or args.output.exists():
        parser.error("Output must be a new .glb path")
    if bool(args.motion_reference) != bool(args.clip):
        parser.error("--motion-reference and --clip must be supplied together")
    motion = None
    if args.motion_reference:
        reference = json.loads(args.motion_reference.read_text(encoding="utf-8"))
        candidates = [item for item in reference["clips"] if Path(item["path"]).stem == args.clip]
        if len(candidates) != 1:
            parser.error("--clip must identify exactly one reference clip")
        motion = {"name": args.clip, "samples": candidates[0]["samples"]}
    gltf, binary = export_static_mesh(args.source, args.base_color_png, motion)
    write_glb(args.output, gltf, binary)
    position = gltf["accessors"][gltf["meshes"][0]["primitives"][0]["attributes"]["POSITION"]]
    print(json.dumps({
        "vertices": position["count"],
        "bounds": [position["min"], position["max"]],
        "submeshes": len(gltf["materials"]),
    }))


if __name__ == "__main__":
    main()
