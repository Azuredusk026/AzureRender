"""Export one Unity SkinnedMeshRenderer and its source bind hierarchy."""

import argparse
from bisect import bisect_right
import math
from pathlib import Path
import re
from struct import pack, unpack

import yaml

from tools.build_face_sdf import write_glb
from tools.npr_room.mesh_export import export_mesh_geometry, read_unity_mesh, vertex_channels
from tools.npr_room.reference_inventory import asset_index, group_paths_by_guid


DOCUMENT = re.compile(r"(?m)^--- !u!(\d+) &(-?\d+)(?: stripped)?\n")
SCENE_LOADER = getattr(yaml, "CSafeLoader", yaml.SafeLoader)
GUID_SCALAR = re.compile(r"(?<=guid: )([0-9a-fA-F]{32})(?=[,}])")
AXIS = (1, 1, -1, 1)


def inverse_bind(matrix):
    return tuple(
        float(matrix[f"e{row}{column}"]) * AXIS[row] * AXIS[column]
        for column in range(4) for row in range(4)
    )


def add_accessor(gltf, binary, payload, count, type_id, component, target=None):
    binary.extend(b"\x00" * (-len(binary) % 4))
    view = {"buffer": 0, "byteOffset": len(binary), "byteLength": len(payload)}
    if target is not None:
        view["target"] = target
    view_id = len(gltf["bufferViews"])
    gltf["bufferViews"].append(view)
    binary.extend(payload)
    index = len(gltf["accessors"])
    gltf["accessors"].append({
        "bufferView": view_id, "componentType": component, "count": count, "type": type_id,
    })
    return index


def rig_channels(mesh, palette_size):
    data, count, channels, strides, offsets = vertex_channels(mesh["m_VertexData"])

    def rows(index):
        stream, offset, layout, dimension, _ = channels[index]
        if not 1 <= dimension <= 4:
            raise ValueError("A skin vertex must carry one to four weights and joints")
        return [
            layout.unpack_from(data, offsets[stream] + offset + vertex * strides[stream])
            for vertex in range(count)
        ]

    joint_rows = rows(13)
    if 12 in channels:
        weight_rows = rows(12)
    elif channels[13][3] == 1:
        weight_rows = [(1.0,)] * count
    else:
        raise ValueError("Missing explicit weights for a multi-joint skin")
    joints, weights = [], []
    for raw_weights, raw_joints in zip(weight_rows, joint_rows, strict=True):
        if len(raw_weights) != len(raw_joints):
            raise ValueError("Joint/weight counts differ")
        if any(not isinstance(j, int) or not 0 <= j < palette_size for j in raw_joints):
            raise ValueError("Joint index is outside the skin palette")
        total = sum(raw_weights)
        if any(not math.isfinite(w) or w < 0 for w in raw_weights) or abs(total - 1) > 0.02:
            raise ValueError("Skin weights must be finite and normalized")
        padding = (0,) * (4 - len(raw_weights))
        joints.append(tuple(raw_joints) + padding)
        weights.append(tuple(w / total for w in raw_weights) + padding)
    return joints, weights


def append_skin(gltf, binary, mesh, nodes, bones, mesh_node):
    binds = mesh["m_BindPose"]
    if not bones or len(bones) != len(binds) or len(set(bones)) != len(bones):
        raise ValueError("Renderer palette and mesh bind matrices must correspond")
    node_ids = {source_id: index for index, source_id in enumerate(nodes)}
    skin_indices = [node_ids[bone] for bone in bones]
    joints, weights = rig_channels(mesh, len(binds))
    joint_acc = add_accessor(gltf, binary, b"".join(pack("<4H", *j) for j in joints),
                             len(joints), "VEC4", 5123, 34962)
    weight_acc = add_accessor(gltf, binary, b"".join(pack("<4f", *w) for w in weights),
                              len(weights), "VEC4", 5126, 34962)
    bind_acc = add_accessor(gltf, binary, b"".join(pack("<16f", *inverse_bind(m)) for m in binds),
                            len(binds), "MAT4", 5126)
    gltf["nodes"] = [dict(node) for node in nodes.values()]
    roots = set(range(len(nodes)))
    for node in gltf["nodes"]:
        if "children" in node:
            node["children"] = [node_ids[child] for child in node["children"]]
            roots.difference_update(node["children"])
    gltf["nodes"][node_ids[mesh_node]].update(mesh=0, skin=0)
    gltf["scenes"][0]["nodes"] = sorted(roots)
    gltf["skins"] = [{"joints": skin_indices, "inverseBindMatrices": bind_acc}]
    for primitive in gltf["meshes"][0]["primitives"]:
        primitive["attributes"].update(JOINTS_0=joint_acc, WEIGHTS_0=weight_acc)


def append_morphs(gltf, binary, mesh, source_weights):
    morphs = mesh["m_Shapes"]
    channels = morphs["channels"]
    if not channels:
        return
    if len(source_weights) != len(channels):
        raise ValueError("Renderer blend shape weights do not match the mesh channels")
    count = int(mesh["m_VertexData"]["m_VertexCount"])
    targets = []
    names = []
    for channel in channels:
        frame_index = int(channel["frameIndex"])
        if int(channel["frameCount"]) != 1 or float(morphs["fullWeights"][frame_index]) != 100:
            raise ValueError("A morph channel requires one frame at full weight")
        shape = morphs["shapes"][frame_index]
        offset = int(shape["firstVertex"])
        entries = morphs["vertices"][offset:offset + int(shape["vertexCount"])]
        attributes = [("POSITION", "vertex")]
        if int(shape["hasNormals"]):
            attributes.append(("NORMAL", "normal"))
        if int(shape["hasTangents"]):
            attributes.append(("TANGENT", "tangent"))
        target = {}
        for semantic, field in attributes:
            deltas = [(0, 0, 0)] * count
            for entry in entries:
                index = int(entry["index"])
                if not 0 <= index < count:
                    raise ValueError("Morph delta vertex index is outside the mesh")
                value = entry[field]
                deltas[index] = (float(value["x"]), float(value["y"]), -float(value["z"]))
            accessor = add_accessor(gltf, binary, b"".join(pack("<3f", *entry) for entry in deltas),
                                    count, "VEC3", 5126, 34962)
            if semantic == "POSITION":
                axes = list(zip(*deltas))
                gltf["accessors"][accessor].update(min=[min(a) for a in axes], max=[max(a) for a in axes])
            target[semantic] = accessor
        names.append(channel["name"])
        targets.append(target)
    for primitive in gltf["meshes"][0]["primitives"]:
        primitive["targets"] = targets
    gltf["meshes"][0]["extras"] = {"targetNames": names}
    gltf["meshes"][0]["weights"] = [float(w) / 100 for w in source_weights]


def sample_shape(times, values, tangents, time):
    right = bisect_right(times, time)
    if right == 0:
        return values[0]
    if right == len(times):
        return values[-1]
    left = right - 1
    start, stop = values[left], values[right]
    incoming, outgoing = tangents[right][0], tangents[left][1]
    if math.isinf(incoming) or math.isinf(outgoing):
        return start
    duration = times[right] - times[left]
    phase = (time - times[left]) / duration
    phase2, phase3 = phase * phase, phase * phase * phase
    return ((2 * phase3 - 3 * phase2 + 1) * start + (phase3 - 2 * phase2 + phase) * outgoing * duration
            + (3 * phase2 - 2 * phase3) * stop + (phase3 - phase2) * incoming * duration)


def float32(value):
    return unpack("<f", pack("<f", value))[0]


def append_morph_clip(gltf, binary, clip, target_path, target_node):
    if any(existing["name"] == clip["m_Name"] for existing in gltf.get("animations", ())):
        raise ValueError(f"Duplicate clip name: {clip['m_Name']}")
    if any(clip.get(field) for field in ("m_RotationCurves", "m_CompressedRotationCurves",
                                        "m_EulerCurves", "m_PositionCurves", "m_ScaleCurves",
                                        "m_PPtrCurves", "m_Events", "m_Compressed")):
        raise ValueError("Clip contains additional animation data")
    mesh = gltf["meshes"][gltf["nodes"][target_node]["mesh"]]
    names, defaults = mesh["extras"]["targetNames"], mesh["weights"]
    if len(set(names)) != len(names):
        raise ValueError("Morph target names must be unique")
    settings = clip["m_AnimationClipSettings"]
    start, stop, rate = (settings["m_StartTime"], settings["m_StopTime"], clip["m_SampleRate"])
    if not all(map(math.isfinite, (start, stop, rate))) or stop <= start or rate <= 0:
        raise ValueError("Invalid clip time range or sample rate")
    duration = float32(stop - start)
    curves = {}
    sampled_times = {float32(i / rate) for i in range(math.ceil(duration * rate))}
    sampled_times.add(duration)
    for binding in clip["m_FloatCurves"]:
        prefix = "blendShape."
        attribute = binding["attribute"]
        if binding["classID"] != 137 or binding["path"] != target_path or not attribute.startswith(prefix):
            raise ValueError("Only direct blend-shape bindings on the selected renderer are supported")
        name = attribute.removeprefix(prefix)
        if name not in names or name in curves:
            raise ValueError(f"Missing or ambiguous morph target {name}")
        keys = binding["curve"]["m_Curve"]
        times = [float32(key["time"] - start) for key in keys]
        values = [key["value"] / 100 for key in keys]
        tangents = [(float(key["inSlope"]) / 100, float(key["outSlope"]) / 100) for key in keys]
        if (not keys or any(key["weightedMode"] for key in keys)
                or not all(map(math.isfinite, times + values))
                or any(math.isnan(value) for pair in tangents for value in pair)
                or any(right <= left for left, right in zip(times, times[1:]))):
            raise ValueError(f"Invalid or unsupported blend-shape keys for {name}")
        curves[name] = (times, values, tangents)
        sampled_times.update(time for time in times if 0 <= time <= duration)
    if not curves:
        raise ValueError("Clip has no blend-shape bindings")

    times = sorted(sampled_times)
    values = [
        sample_shape(*curves[name], time) if name in curves else default
        for time in times for name, default in zip(names, defaults)
    ]
    inputs = add_accessor(gltf, binary, pack(f"<{len(times)}f", *times), len(times), "SCALAR", 5126)
    output = add_accessor(gltf, binary, pack(f"<{len(values)}f", *values), len(values), "SCALAR", 5126)
    gltf["accessors"][inputs].update(min=[times[0]], max=[times[-1]])
    gltf.setdefault("animations", []).append({
        "name": clip["m_Name"], "extras": {"loop": bool(settings["m_LoopTime"])},
        "channels": [{"sampler": 0, "target": {"node": target_node, "path": "weights"}}],
        "samplers": [{"input": inputs, "output": output, "interpolation": "LINEAR"}],
    })


class UnityScene:
    def __init__(self, path):
        self.serialized = path.read_text(encoding="utf-8-sig")
        headers = list(DOCUMENT.finditer(self.serialized))
        self.documents = {
            int(head[2]): (int(head[1]), head.end(),
                           headers[i + 1].start() if i + 1 < len(headers) else len(self.serialized))
            for i, head in enumerate(headers)
        }

    def read(self, file_id):
        _, start, end = self.documents[file_id]
        source = GUID_SCALAR.sub(lambda match: '"' + match[1] + '"', self.serialized[start:end])
        entry = yaml.load(source, Loader=SCENE_LOADER)
        return next(iter(entry.values()))

    def skin_hierarchy(self, renderer):
        entity = self.read(renderer["m_GameObject"]["fileID"])
        mesh_id, = [c["component"]["fileID"] for c in entity["m_Component"]
                    if self.documents[c["component"]["fileID"]][0] == 4]
        bones = [pointer["fileID"] for pointer in renderer["m_Bones"]]
        nodes = {}

        def add_node(file_id):
            if file_id in nodes:
                return
            source = self.read(file_id)
            parent = source["m_Father"]["fileID"]
            if parent:
                add_node(parent)
            game_object = self.read(source["m_GameObject"]["fileID"])
            t = source["m_LocalPosition"]
            q = source["m_LocalRotation"]
            s = source["m_LocalScale"]
            rotation = (-q["x"], -q["y"], q["z"], q["w"])
            length = math.hypot(*rotation)
            nodes[file_id] = {
                "name": game_object["m_Name"],
                "translation": [t["x"], t["y"], -t["z"]],
                "rotation": [value / length for value in rotation],
                "scale": [s["x"], s["y"], s["z"]],
            }
            if parent:
                nodes[parent].setdefault("children", []).append(file_id)

        for file_id in [*bones, mesh_id]:
            add_node(file_id)
        return nodes, bones, mesh_id


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True, help="ExportedProject directory")
    parser.add_argument("--scene", type=Path, required=True)
    parser.add_argument("--renderer", type=int, required=True)
    parser.add_argument("--base-color-png", type=Path, action="append", default=[])
    parser.add_argument("--morph-clip", type=Path, action="append", default=[])
    parser.add_argument("--morph-path", help="Source binding path relative to the animation root")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.suffix != ".glb" or args.output.exists():
        parser.error("Output must be a new .glb path")
    if bool(args.morph_clip) != bool(args.morph_path):
        parser.error("--morph-clip and --morph-path must be used together")
    scene = UnityScene(args.scene)
    if scene.documents[args.renderer][0] != 137:
        parser.error("--renderer must identify a SkinnedMeshRenderer")
    renderer = scene.read(args.renderer)
    slots = group_paths_by_guid(asset_index(args.source))
    source_mesh, = slots[renderer["m_Mesh"]["guid"]]
    mesh = read_unity_mesh(args.source / source_mesh)
    gltf, geometry = export_mesh_geometry(mesh, args.base_color_png)
    binary = bytearray(geometry)
    nodes, bones, mesh_id = scene.skin_hierarchy(renderer)
    append_skin(gltf, binary, mesh, nodes, bones, mesh_id)
    append_morphs(gltf, binary, mesh, renderer["m_BlendShapeWeights"])
    for path in args.morph_clip:
        animation = UnityScene(path)
        clip_id, = [file_id for file_id, entry in animation.documents.items() if entry[0] == 74]
        append_morph_clip(gltf, binary, animation.read(clip_id), args.morph_path, list(nodes).index(mesh_id))
    gltf["buffers"][0]["byteLength"] = len(binary)
    write_glb(args.output, gltf, binary)
    print(f"Exported {len(bones)} joints and {len(gltf['meshes'][0].get('weights', []))} morph channels.")


if __name__ == "__main__":
    main()
