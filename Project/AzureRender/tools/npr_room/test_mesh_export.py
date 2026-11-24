from pathlib import Path
from struct import pack, unpack_from
import tempfile
import unittest

from mesh_export import export_mesh_geometry, export_static_mesh, read_unity_mesh
from tools.build_face_sdf import encode_png


class StaticMeshExportTests(unittest.TestCase):
    def test_preserves_submesh_and_reflects_z_with_winding(self):
        rows = []
        for x, y, z in [(0, 0, 1), (1, 0, 1), (0, 1, 1), (9, 9, 9)]:
            rows.append(pack("<3f3e2e", x, y, z, 0, 0, 1, x, y))
        raw_vertices = b"".join(rows)
        raw_indices = pack("<3H", 0, 1, 2)
        fields = f"""
%YAML 1.1
%TAG !u! tag:unity3d.com,2011:
--- !u!43 &4300000
Mesh:
  m_Name: Triangle
  m_MeshCompression: 0
  m_BindPose: []
  m_IndexFormat: 0
  m_IndexBuffer: {raw_indices.hex()}
  m_SubMeshes:
  - firstByte: 0
    indexCount: 3
    topology: 0
    baseVertex: 0
  m_VertexData:
    m_VertexCount: 4
    m_Channels:
    - {{stream: 0, offset: 0, format: 0, dimension: 3}}
    - {{stream: 0, offset: 12, format: 1, dimension: 35}}
    - {{stream: 0, offset: 0, format: 0, dimension: 0}}
    - {{stream: 0, offset: 0, format: 0, dimension: 0}}
    - {{stream: 0, offset: 18, format: 1, dimension: 2}}
    m_DataSize: {len(raw_vertices)}
    _typelessdata: {raw_vertices.hex()}
""".lstrip()
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "triangle.asset"
            source.write_text(fields, encoding="utf-8")
            texture = Path(directory) / "red.png"
            png = encode_png(1, 1, bytes((255, 0, 0, 255)))
            texture.write_bytes(png)
            motion = {"name": "Open", "samples": [
                {"time": 0, "position": [0, 0, 1], "rotation": [0, 0, 0, 1]},
                {"time": 1, "position": [2, 3, 4], "rotation": [0, 0.6, 0, 0.8]},
            ]}
            gltf, binary = export_static_mesh(source, [texture], motion)
            mesh = read_unity_mesh(source)
            subset, _ = export_mesh_geometry(mesh, submesh_range=(0, 1))
            subset_position = subset["accessors"][0]
            self.assertEqual(subset_position["count"], 3)
            self.assertEqual(subset_position["max"], [1, 1, -1])

        primitive, = gltf["meshes"][0]["primitives"]
        self.assertEqual(primitive["material"], 0)
        self.assertEqual(gltf["materials"][0]["name"], "unity-slot-0")
        color = gltf["materials"][0]["pbrMetallicRoughness"]["baseColorTexture"]
        image = gltf["images"][gltf["textures"][color["index"]]["source"]]
        image_view = gltf["bufferViews"][image["bufferView"]]
        start = image_view["byteOffset"]
        self.assertEqual(binary[start:start + image_view["byteLength"]], png)
        animation, = gltf["animations"]
        self.assertEqual(animation["name"], "Open")
        targets = {item["target"]["path"]: item["sampler"] for item in animation["channels"]}
        self.assertEqual(set(targets), {"translation", "rotation"})
        sampler = animation["samplers"][targets["translation"]]
        access = gltf["accessors"][sampler["output"]]
        view = gltf["bufferViews"][access["bufferView"]]
        self.assertEqual(unpack_from("<6f", binary, view["byteOffset"]), (0, 0, -1, 2, 3, -4))
        position = gltf["accessors"][primitive["attributes"]["POSITION"]]
        self.assertEqual(position["min"], [0, 0, -9])
        self.assertEqual(position["max"], [9, 9, -1])
        self.assertEqual(unpack_from("<3f", binary), (0, 0, -1))
        normal = gltf["accessors"][primitive["attributes"]["NORMAL"]]
        normal_view = gltf["bufferViews"][normal["bufferView"]]
        self.assertEqual(unpack_from("<3f", binary, normal_view["byteOffset"]), (0, 0, -1))
        index = gltf["accessors"][primitive["indices"]]
        index_view = gltf["bufferViews"][index["bufferView"]]
        self.assertEqual(unpack_from("<3H", binary, index_view["byteOffset"]), (0, 2, 1))


if __name__ == "__main__":
    unittest.main()
