"""Generate original MIT-licensed Face/Hair/Brow/Generic GPU fixtures."""
import base64
import json
from pathlib import Path
import struct
import os
import zlib
import math


def create(output):
    document = {"asset": {"version": "2.0", "generator": "AzureRender original material fixture"},
                "scene": 0, "scenes": [{"nodes": [0, 3]}],
                "nodes": [{"name": "FixtureRoot", "children": [1]},
                          {"name": "FixtureHead", "children": [2]},
                          {"name": "FixtureBrow"}, {"name": "FixtureMesh", "mesh": 0, "skin": 0}],
                "skins": [{"joints": [0, 1, 2], "skeleton": 0}],
                "materials": [], "meshes": [{"primitives": []}], "buffers": [],
                "bufferViews": [], "accessors": []}
    binary = bytearray()
    def accessor(values, fmt, kind, components, bounds=False):
        offset = len(binary)
        for value in values:
            binary.extend(struct.pack("<" + fmt * components, *value))
        size = len(binary) - offset
        binary.extend(b"\0" * (-len(binary) % 4))
        document["bufferViews"].append({"buffer": 0, "byteOffset": offset, "byteLength": size})
        entry = {"bufferView": len(document["bufferViews"]) - 1, "componentType": 5126 if fmt == "f" else 5123,
                 "count": len(values), "type": kind}
        if bounds:
            entry["min"] = [min(v[i] for v in values) for i in range(components)]
            entry["max"] = [max(v[i] for v in values) for i in range(components)]
        document["accessors"].append(entry)
        return len(document["accessors"]) - 1
    def geometry(positions, normals, uv, indices, material, joint):
        primitive = {"attributes": {
            "POSITION": accessor(positions, "f", "VEC3", 3, True),
            "NORMAL": accessor(normals, "f", "VEC3", 3),
            "TEXCOORD_0": accessor(uv, "f", "VEC2", 2),
            "JOINTS_0": accessor([(joint, 0, 0, 0)] * len(positions), "H", "VEC4", 4),
            "WEIGHTS_0": accessor([(1., 0., 0., 0.)] * len(positions), "f", "VEC4", 4)},
            "indices": accessor(indices, "H", "SCALAR", 1), "material": material}
        document["meshes"][0]["primitives"].append(primitive)
    def box(center, extent, material, joint):
        positions, normals, uv, indices = [], [], [], []
        for axis, sign in ((0, 1), (0, -1), (1, 1), (1, -1), (2, 1), (2, -1)):
            other = [i for i in range(3) if i != axis]
            if (axis == 1) == (sign > 0):
                other.reverse()
            start = len(positions)
            for a, b in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
                point = list(center)
                point[axis] += sign * extent[axis]
                point[other[0]] += a * extent[other[0]]
                point[other[1]] += b * extent[other[1]]
                normal = [0., 0., 0.]; normal[axis] = float(sign)
                positions.append(point); normals.append(normal); uv.append(((a + 1) / 2, (b + 1) / 2))
            indices.extend([(start,), (start + 1,), (start + 2,), (start,), (start + 2,), (start + 3,)])
        geometry(positions, normals, uv, indices, material, joint)
    def hair_cap():
        positions, normals, uv, indices = [], [], [], []
        for row in range(9):
            theta = (.01 + row / 8 * .49) * math.pi
            for column in range(33):
                phi = column / 32 * 2 * math.pi
                x, y, z = math.sin(theta) * math.cos(phi), math.cos(theta), math.sin(theta) * math.sin(phi)
                positions.append((.265 * x, 1.51 + .21 * y, .235 * z))
                normal = (x / .265, y / .21, z / .235)
                length = math.sqrt(sum(value * value for value in normal))
                normals.append(tuple(value / length for value in normal))
                uv.append((column / 32, row / 8))
        for row in range(8):
            for column in range(32):
                a = row * 33 + column; b = a + 33
                indices.extend([(a,), (a + 1,), (b,), (a + 1,), (b + 1,), (b,)])
        geometry(positions, normals, uv, indices, 2, 1)
    specifications = [("generic", ["neutral-fallback"], [0.12, .25, .3, 1]),
                      ("face", ["stylized-shadow", "face-sdf-eligible"], [1, .76, .65, 1]),
                      ("hair", ["stylized-shadow", "hair-anisotropy"], [.45, .06, .12, 1]),
                      ("overlay", ["overlay", "brow-overlay"], [.14, .015, .025, 1])]
    for kind, features, color in specifications:
        profile = {"schemaVersion": 1, "class": kind, "features": features,
                   "styleParameters": [.9, .8, .4, .4], "featureParameters": [.8, 1, 1, 1]}
        material = {"name": "Fixture_" + kind, "pbrMetallicRoughness": {"baseColorFactor": color},
                    "extras": {"azureRenderMaterial": profile}}
        if kind == "overlay":
            material.update(alphaMode="BLEND", doubleSided=True)
            profile.update(styleParameters=[.0012, 0, 0, 0], featureParameters=[.001, 1, .02, 1])
        if kind == "face":
            profile["faceSdf"] = {"schemaVersion": 1, "texture": 0, "texCoord": 0,
                "channel": "r", "maskChannel": "a", "shadowOnLowValues": True,
                "horizontalAxis": "left-to-right", "headNode": "FixtureHead"}
        if kind == "hair":
            material["extras"]["afterglowHairDataTexture"] = 1
            material["extras"]["afterglowHairParameters"] = [128, .4, 4, 1]
        document["materials"].append(material)
    face_texture = Path(__file__).resolve().parents[1] / "assets_public/face_sdf_v1.png"
    document["images"] = [{"uri": Path(os.path.relpath(face_texture, output.parent)).as_posix()}, {"uri": "fixture_hair_data.png"}]
    document["textures"] = [{"source": 0}, {"source": 1}]
    box((0, .5, 0), (.24, .5, .15), 0, 0)
    box((0, 1.3, 0), (.22, .25, .19), 1, 1)
    hair_cap()
    for x in (-.115, .115):
        box((x, 1.4, .198), (.067, .009, .002), 3, 2)
    document["buffers"] = [{"byteLength": len(binary), "uri": "data:application/octet-stream;base64," + base64.b64encode(binary).decode()}]
    output.parent.mkdir(parents=True, exist_ok=True)
    # RG carry authored hair response; BA encode a flat octahedral normal.
    def chunk(kind, payload):
        return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload))
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 2, 2, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes([0, 192, 255, 128, 128, 255, 192, 128, 128] * 2))) + chunk(b"IEND", b"")
    (output.parent / "fixture_hair_data.png").write_bytes(png)
    output.write_text(json.dumps(document, indent=2), encoding="utf-8")


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--output", type=Path, default=Path("assets_public/third_person/material_fixture.gltf"))
    create(parser.parse_args().output)
