from pathlib import Path
import tempfile
import unittest

from material_catalog import build_material_catalog, property_map


MATERIAL = "1" * 32
SHADER = "2" * 32
TEXTURE = "3" * 32


class MaterialCatalogTests(unittest.TestCase):
    def test_maps_source_properties_and_preserves_renderer_slot_order(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary)
            (source / "Assets").mkdir()
            name = "Assets/Material.mat"
            (source / name).write_text(f"""--- !u!21 &2100000
Material:
  m_Name: Fixture
  m_Shader: {{fileID: 4800000, guid: {SHADER}, type: 3}}
  m_Parent: {{fileID: 0}}
  m_ValidKeywords: [_NORMALMAP]
  m_CustomRenderQueue: 2475
  m_SavedProperties:
    serializedVersion: 3
    m_TexEnvs:
    - _BaseMap:
        m_Texture: {{fileID: 2800000, guid: {TEXTURE}, type: 3}}
        m_Scale: {{x: 2, y: 3}}
        m_Offset: {{x: 0, y: 0.25}}
    m_Ints: {{}}
    m_Floats: {{_Cutoff: 0.4}}
    m_Colors:
    - _BaseColor: {{r: 0.5, g: 1, b: 1, a: 1}}
""", encoding="utf-8")
            (source / f"{name}.meta").write_text(f"guid: {MATERIAL}\n", encoding="utf-8")
            scene = "Assets/Room.unity"
            (source / scene).write_text(f"""--- !u!23 &53
MeshRenderer:
  m_GameObject: {{fileID: 10}}
  m_Enabled: 1
  m_Materials:
  - {{fileID: 2100000, guid: {MATERIAL}, type: 2}}
  - {{fileID: 0}}
--- !u!137 &54
SkinnedMeshRenderer:
  m_GameObject: {{fileID: 11}}
  m_Enabled: 0
  m_Materials:
  - {{fileID: 2100000, guid: {MATERIAL}, type: 2}}
""", encoding="utf-8")
            inventory = {
                "additionalClosure": [name, scene],
                "assets": {name: MATERIAL, scene: "4" * 32, "Assets/Test.shader": SHADER,
                           "Assets/Test.png": TEXTURE},
            }
            report = build_material_catalog(source, inventory, {TEXTURE: {"media": "map.png"}})
        material = report["slots"][MATERIAL]
        self.assertEqual(material["shader"]["source"], "Assets/Test.shader")
        self.assertEqual(material["settings"]["m_CustomRenderQueue"], 2475)
        self.assertEqual(material["properties"]["m_Floats"], {"_Cutoff": 0.4})
        texture = material["properties"]["m_TexEnvs"]["_BaseMap"]
        self.assertEqual(texture["m_Scale"], {"x": 2, "y": 3})
        self.assertEqual(texture["m_Texture"]["state"], "image-catalog")
        references = report["rendererBindings"][scene]
        self.assertEqual([slot["state"] for slot in references["53"]["materials"]], ["material-catalog", "null"])
        self.assertEqual(references["54"]["enabled"], False)

    def test_rejects_ambiguous_property_lists(self):
        with self.assertRaisesRegex(ValueError, "Repeated material property"):
            property_map([{"_Color": 1}, {"_Color": 2}])


if __name__ == "__main__":
    unittest.main()
