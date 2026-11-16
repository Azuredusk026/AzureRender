"""Verify the first original tone profile and the per-renderer material slots."""

import unittest
from pathlib import Path
from tempfile import TemporaryDirectory

from tools.build_face_sdf import write_glb
from tools.npr_room.pbr_materials import staging_metadata
from tools.npr_room.room_surfaces import TOON_SHADER, base_surface, material_bindings, mesh_slots, mounted_assets


class RoomSurfaceTests(unittest.TestCase):
    def test_reads_mesh_slots_from_a_uuid_mounted_by_scene_import(self):
        with TemporaryDirectory() as holder:
            root = Path(holder)
            mesh = root / "mesh.glb"
            write_glb(mesh, {"materials": [{"name": "unity-slot-0"}]}, b"")
            staging_metadata(mesh, "068548c359f6476b8acddc7329582fe8", [])
            level = {"resources": [{"id": "m", "asset": "068548c3-59f6-476b-8acd-dc7329582fe8"}]}
            self.assertEqual(mesh_slots(root, level, mounted_assets(root)), {"m": {"unity-slot-0"}})

    def test_source_color_and_valid_srgb_main_texture(self):
        guid = "068548c359f6476b8acddc7329582fe8"
        texture = {"m_Texture": {"fileID": 2800000, "guid": guid},
                   "m_Scale": {"x": 1, "y": 1}, "m_Offset": {"x": 0, "y": 0}}
        mat = {"shader": {"source": TOON_SHADER}, "parent": {"fileID": 0},
               "properties": {"m_Colors": {"_Color": {"r": .25, "g": .5, "b": .9, "a": 1}},
                              "m_TexEnvs": {"_MainTex": texture}, "m_Floats": {"_Cull": 0}}}
        document, image = base_surface(mat, {guid: {"srgb": True}})
        self.assertEqual(image, guid)
        self.assertEqual(document["parameters"]["baseColor"], [.25, .5, .9, 1])
        self.assertTrue(document["parameters"]["doubleSided"])
        self.assertEqual(document["textures"]["albedo"]["asset"], "068548c3-59f6-476b-8acd-dc7329582fe8")
        mat["properties"]["m_Floats"].update(_AlphaMaskMode=2, _AlphaMaskScale=1, _AlphaMaskValue=0)
        self.assertEqual(base_surface(mat, {guid: {"srgb": True}}), (document, image))
        invisible = mat["properties"]["m_Floats"]
        invisible["_Invisible"] = 1
        with self.assertRaisesRegex(ValueError, "coverage"):
            base_surface(mat, {guid: {"srgb": True}})
        invisible["_Invisible"] = 0
        texture["m_Scale"]["x"] = 2
        with self.assertRaisesRegex(ValueError, "transform"):
            base_surface(mat, {guid: {"srgb": True}})

    def test_static_batch_renumbers_its_source_slots_locally(self):
        nodes = {"regular": {"resourceId": "regular"}, "batch": {"resourceId": "batch"}}
        mappings = {"unity-25": "regular", "unity-batch-26": "batch"}
        rows = {"1": {"object": 11, "enabled": True, "type": "mesh", "materials": [{"guid": "a"}],
                      "staticBatch": {"subMeshCount": 0}},
                "2": {"object": 12, "enabled": True, "type": "mesh", "materials": [{"guid": "b"}, {"guid": "a"}],
                      "staticBatch": {"firstSubMesh": 8, "subMeshCount": 2}}}
        bindings = material_bindings(mappings, nodes, rows, {11: 25, 12: 26},
                                     {"regular": {"unity-slot-0"}, "batch": {"unity-slot-0", "unity-slot-1"}})
        self.assertEqual(bindings, [("regular", "unity-slot-0", "a"), ("batch", "unity-slot-0", "b"),
                                    ("batch", "unity-slot-1", "a")])


if __name__ == "__main__":
    unittest.main()
