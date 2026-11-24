import copy
import json
from pathlib import Path
from struct import pack
import tempfile
import unittest

from tools.build_face_sdf import encode_png, write_glb
from tools.npr_room import captured_accessories as accessories
from tools.npr_room.reference_inventory import digest
from tools.npr_room.test_original_room_registration import pose


class CapturedAccessoryTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.heroine = self.root / "heroine"
        self.heroine.mkdir()
        self.meshes = self.root / "meshes"
        self.images = self.root / "images"
        self.meshes.mkdir()
        self.images.mkdir()
        self.guid = "a" * 32
        mesh = self.meshes / "gear.glb"
        write_glb(mesh, {"asset": {"version": "2.0"}, "buffers": [{"byteLength": 36}],
                        "bufferViews": [{"buffer": 0, "byteLength": 36}],
                        "accessors": [{"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3"}],
                        "meshes": [{"name": "Gear", "primitives": [{"attributes": {"POSITION": 0}, "material": 0}]}],
                        "materials": [{"name": "unity-slot-0"}], "nodes": [{"name": "Gear", "mesh": 0}],
                        "scenes": [{"nodes": [0]}]}, pack("<9f", 0, 0, 0, 1, 0, 0, 0, 1, 0))
        self.mesh_catalog = self.meshes / "mesh-catalog.json"
        self.mesh_catalog.write_text(json.dumps({"slots": {self.guid: {
            "source": "Assets/Mesh/Gear.asset", "kind": "static", "media": "gear.glb",
            "sha256": digest(mesh), "sourceSHA256": "c" * 64}}}), encoding="utf-8")
        image = self.images / "base.png"
        image.write_bytes(encode_png(1, 1, bytes((255, 0, 0, 255))))
        self.image_catalog = self.images / "image-catalog.json"
        self.image_catalog.write_text(json.dumps({"slots": {"b" * 32: {
            "source": "Assets/Texture2D/gear_base.png", "media": "base.png", "sha256": digest(image), "srgb": True}}}), encoding="utf-8")
        self.document = {"nodes": [{"name": "Root", "children": [1]}, {"name": "Head"}],
                         "scenes": [{"nodes": [0]}], "buffers": [{"uri": "hair.glb", "byteLength": 128}],
                         "bufferViews": [], "accessors": [], "materials": [{"name": "unity-slot-0"}], "meshes": []}
        self.record = {"transforms": [pose("Root", 1, 0), pose("Root/Head", 2, 1), pose("Root/Head/Gear", 3, 2)],
                       "renderers": [{"path": "Root/Head/Gear", "transformId": 3, "kind": "MeshRenderer",
                                      "mesh": "Gear", "enabled": True, "visible": True, "materials": [{
                                          "slot": 0, "shader": "lilToon", "renderQueue": 2000,
                                          "floats": [{"name": "_Cull", "value": 0}, {"name": "_UseEmission", "value": 0}],
                                          "colors": [{"name": "_Color", "value": dict(r=1, g=.5, b=.25, a=1)}],
                                          "textures": [{"name": "_MainTex", "texture": "gear_base",
                                                        "scale": {"x": 1, "y": 1}, "offset": {"x": 0, "y": 0}}]}]}]}
        self.frame = self.root / "frame.json"

    def install(self):
        self.frame.write_text(json.dumps(self.record), encoding="utf-8")
        return accessories.install(self.frame, self.document, self.mesh_catalog, self.image_catalog, self.heroine)

    def test_attaches_unique_material_and_external_geometry_to_captured_parent(self):
        before = copy.deepcopy(self.record)
        proof = self.install()
        self.assertEqual(proof["count"], 1)
        self.assertEqual(self.document["nodes"][1]["children"], [2])
        node = self.document["nodes"][2]
        self.assertEqual(node["translation"], [1, 2, -3])
        self.assertEqual(node["mesh"], 0)
        self.assertEqual(self.document["bufferViews"][0]["buffer"], 1)
        self.assertEqual(self.document["buffers"][1]["uri"], "captured-accessories/" + self.guid + ".glb")
        self.assertTrue((self.heroine / self.document["buffers"][1]["uri"]).is_file())
        primitive = self.document["meshes"][0]["primitives"][0]
        self.assertEqual(primitive["material"], 1)
        self.assertNotEqual(self.document["materials"][1]["name"], "unity-slot-0")
        self.assertEqual(self.document["materials"][1]["pbrMetallicRoughness"]["baseColorFactor"], [1, .5, .25, 1])
        self.assertTrue(self.document["images"][0]["uri"].startswith("data:image/png;base64,"))
        self.assertEqual(self.record, before)

    def test_invalid_texture_keeps_document_and_destination_untouched(self):
        original = copy.deepcopy(self.document)
        (self.images / "base.png").write_bytes(b"changed source")
        with self.assertRaises(ValueError):
            self.install()
        self.assertEqual(self.document, original)
        self.assertEqual(list(self.heroine.iterdir()), [])

    def test_empty_visible_attachment_set_preserves_document(self):
        self.record["renderers"][0]["visible"] = False
        original = copy.deepcopy(self.document)
        self.assertEqual(self.install()["count"], 0)
        self.assertEqual(self.document, original)
        self.assertEqual(list(self.heroine.iterdir()), [])

    def test_hdr_emission_uses_standard_strength_extension(self):
        material = self.record["renderers"][0]["materials"][0]
        material["floats"][1]["value"] = 1
        material["colors"].append({"name": "_EmissionColor", "value": dict(r=3, g=.75, b=1.5, a=1)})
        self.install()
        result = self.document["materials"][1]
        self.assertEqual(result["emissiveFactor"], [1, .25, .5])
        self.assertEqual(result["extensions"]["KHR_materials_emissive_strength"]["emissiveStrength"], 3)
        self.assertIn("KHR_materials_emissive_strength", self.document["extensionsUsed"])
