import unittest
from pathlib import Path
import tempfile
import yaml

from tools.npr_room.room_transparent_surfaces import transparent_document, install_lens, TRANSPARENT_SHADER, LENS_GUID


def source_material():
    floats = {"_SrcBlend": 1, "_DstBlend": 10, "_ZTest": 4, "_ZWrite": 1,
              "_Cull": 2, "_Smoothness": 1, "_Metallic": 0}
    return {"m_Shader": {"guid": TRANSPARENT_SHADER}, "m_Parent": {"fileID": 0},
            "m_SavedProperties": {"m_Floats": [{key: value} for key, value in floats.items()],
                                  "m_Colors": [{"_Color": {"r": 1, "g": 1, "b": 1, "a": 0}}],
                                  "m_TexEnvs": []}}


class RoomTransparentSurfacesTests(unittest.TestCase):
    def test_keeps_transparent_color_and_original_depth_write(self):
        document = transparent_document(source_material())
        material, = document["surface"]["materials"]
        self.assertEqual(material["alphaMode"], "BLEND")
        self.assertEqual(material["pbrMetallicRoughness"]["baseColorFactor"], [1, 1, 1, 0])
        self.assertEqual(material["pbrMetallicRoughness"]["roughnessFactor"], 0)
        self.assertEqual(material["extras"]["azureRenderMaterial"]["blendDepth"],
                         {"schemaVersion": 1, "write": True, "compare": "less-or-equal"})

    def test_rejects_other_blend_and_texture_inputs(self):
        source = source_material()
        source["m_SavedProperties"]["m_Floats"][0]["_SrcBlend"] = 5
        with self.assertRaises(ValueError):
            transparent_document(source)
        source = source_material()
        source["m_SavedProperties"]["m_TexEnvs"] = [{"_MainTex": {"m_Texture": {"fileID": 2800000}}}]
        with self.assertRaises(ValueError):
            transparent_document(source)

    def test_reads_material_identity_from_its_unity_document(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "source"
            folder = source / "Assets/Material"
            folder.mkdir(parents=True)
            material = folder / "mt_Lens.mat"
            material.write_text("--- !u!21 &21\n" + yaml.safe_dump({"Material": source_material()}), encoding="utf-8")
            material.with_suffix(".mat.meta").write_text("guid: " + LENS_GUID + "\n", encoding="utf-8")
            assets = root / "assets"
            assets.mkdir()
            level = {"sceneImports": {"RoomScene": {"nodeMap": {"unity-8186": "lens"}}},
                     "nodes": [{"id": "lens"}]}
            receipt = install_lens(assets, level, source)
            self.assertEqual(receipt["alpha"], 0)
            self.assertEqual(receipt, install_lens(assets, level, source))


if __name__ == "__main__":
    unittest.main()
