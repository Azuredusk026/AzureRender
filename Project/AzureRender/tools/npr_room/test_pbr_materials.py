import unittest

from pbr_materials import opaque_lit_document, room_slot_assignments


class PbrMaterialTests(unittest.TestCase):
    def setUp(self):
        self.texture = "7" * 32
        self.image = {self.texture: {"srgb": True}}
        self.material = {
            "shader": {"source": "Assets/Shader/Universal Render Pipeline_Lit.shader"},
            "parent": {"fileID": 0}, "settings": {"m_ValidKeywords": [], "m_CustomRenderQueue": -1},
            "properties": {
                "m_Floats": {"_Surface": 0, "_AlphaClip": 0, "_Cull": 0, "_WorkflowMode": 1,
                             "_SpecularHighlights": 1, "_EnvironmentReflections": 1, "_Metallic": 0.25,
                             "_Smoothness": 0.875},
                "m_Colors": {"_BaseColor": dict(r=0.25, g=0.5, b=0.8, a=1)},
                "m_TexEnvs": {
                    "_BaseMap": {"m_Texture": {"fileID": 2800000, "guid": self.texture},
                                 "m_Scale": {"x": 1, "y": 1}, "m_Offset": {"x": 0, "y": 0}},
                },
            },
        }

    def test_maps_the_supported_parameter_and_texture_slots(self):
        native, texture = opaque_lit_document(self.material, self.image)
        self.assertEqual(texture, self.texture)
        self.assertEqual(native["parameters"]["baseColor"], [0.25, 0.5, 0.8, 1])
        self.assertEqual(native["parameters"]["roughness"], 0.125)
        self.assertTrue(native["parameters"]["doubleSided"])
        self.assertEqual(native["textures"]["albedo"]["asset"], "77777777-7777-7777-7777-777777777777")

    def test_rejects_unsupported_pipeline_behaviors(self):
        self.material["properties"]["m_Floats"]["_Surface"] = 1
        with self.assertRaisesRegex(ValueError, "Opaque metallic workflow"):
            opaque_lit_document(self.material, self.image)
        self.material["properties"]["m_Floats"]["_Surface"] = 0
        self.material["properties"]["m_TexEnvs"]["_BaseMap"]["m_Offset"]["y"] = 0.2
        with self.assertRaisesRegex(ValueError, "Albedo transform"):
            opaque_lit_document(self.material, self.image)

    def test_uses_source_transform_and_ordered_material_slots(self):
        materials = {"a": {"asset": "a-id"}, "b": {"asset": "b-id"}}
        level = {
            "sceneImports": {"Room": {"nodeMap": {
                "unity-5": "model", "unity-batch-6": "combined", "unity-7": "parent",
            }}},
            "nodes": [{"id": node, "resourceId": resource}
                      for node, resource in (("model", "m"), ("combined", "b"), ("parent", ""))],
        }
        renderers = {
            str(index): {"type": "mesh", "enabled": True, "object": index,
                         "staticBatch": {"subMeshCount": int(index == 2)},
                         "materials": [{"guid": "missing"}, {"guid": "a"}, {"guid": "b"}]}
            for index in range(1, 5)
        }
        self.assertEqual(room_slot_assignments(level, "Room", renderers, {1: 5, 2: 6, 3: 7, 4: 8}, materials), [
            {"node": "model", "slotId": "name:unity-slot-1", "material": "a-id"},
            {"node": "model", "slotId": "name:unity-slot-2", "material": "b-id"},
        ])


if __name__ == "__main__":
    unittest.main()
