from collections import OrderedDict
from struct import pack, unpack_from
import unittest

from skinned_mesh_export import append_skin, inverse_bind, rig_channels


class SkinnedExportTests(unittest.TestCase):
    def test_converts_inverse_bind_axis_and_matrix_order(self):
        matrix = {f"e{row}{col}": float(row == col) for row in range(4) for col in range(4)}
        matrix["e03"], matrix["e13"], matrix["e23"] = 1.0, 2.0, 3.0
        self.assertEqual(inverse_bind(matrix)[12:], (1.0, 2.0, -3.0, 1.0))

    def test_skin_joints_keep_palette_order_and_weight_width(self):
        vertices = pack("<2f2I", 0.25, 0.75, 1, 0)
        mesh = {
            "m_VertexData": {
                "m_VertexCount": 1, "m_DataSize": len(vertices),
                "_typelessdata": vertices.hex(),
                "m_Channels": (
                    [{"stream": 0, "offset": 0, "format": 0, "dimension": 0}] * 12
                    + [{"stream": 0, "offset": 0, "format": 0, "dimension": 2},
                       {"stream": 0, "offset": 8, "format": 10, "dimension": 2}]
                ),
            },
            "m_BindPose": [
                {f"e{row}{col}": float(row == col) for row in range(4) for col in range(4)}
                for _ in range(2)
            ],
        }
        document = {
            "accessors": [], "bufferViews": [], "scenes": [{"nodes": [0]}],
            "meshes": [{"primitives": [{"attributes": {"POSITION": 4}}]}]
        }
        slot = OrderedDict(((88, {"name": "first"}), (2, {"name": "second"}), (9, {"name": "mesh"})))
        binary = bytearray()
        append_skin(document, binary, mesh, slot, [2, 88], 9)
        self.assertEqual(document["skins"][0]["joints"], [1, 0])
        self.assertEqual(document["nodes"][2]["skin"], 0)
        att = document["meshes"][0]["primitives"][0]["attributes"]
        view = document["bufferViews"][document["accessors"][att["JOINTS_0"]]["bufferView"]]
        self.assertEqual(unpack_from("<4H", binary, view["byteOffset"]), (1, 0, 0, 0))
        view = document["bufferViews"][document["accessors"][att["WEIGHTS_0"]]["bufferView"]]
        self.assertEqual(unpack_from("<4f", binary, view["byteOffset"]), (0.25, 0.75, 0.0, 0.0))


    def test_single_joint_channel_has_an_implicit_unit_weight(self):
        data = pack("<I", 1)
        mesh = {"m_VertexData": {
            "m_VertexCount": 1, "m_DataSize": len(data), "_typelessdata": data.hex(),
            "m_Channels": ([{"stream": 0, "offset": 0, "format": 0, "dimension": 0}] * 13
                           + [{"stream": 0, "offset": 0, "format": 10, "dimension": 1}]),
        }}
        joints, weights = rig_channels(mesh, 2)
        self.assertEqual(joints, [(1, 0, 0, 0)])
        self.assertEqual(weights, [(1.0, 0, 0, 0)])


if __name__ == "__main__":
    unittest.main()
