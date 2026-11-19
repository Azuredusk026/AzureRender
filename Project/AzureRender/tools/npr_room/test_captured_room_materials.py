import copy
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.build_face_sdf import encode_png
from tools.npr_room import captured_room_materials as captured
from tools.npr_room.pbr_materials import pbr_document, staging_metadata
from tools.npr_room.reference_inventory import digest


class CapturedRoomMaterialTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.assets = self.root / "assets"
        self.assets.mkdir()
        self.images = self.root / "images"
        self.images.mkdir()
        self.old_id = "12345678-1234-1234-1234-123456789abc"
        self.texture_id = "abcdef01-1234-1234-1234-123456789abc"
        self.material = pbr_document([1, 1, 1, 1], 1, 0, False, None)
        self.material_file = self.assets / "original.azurematerial"
        self.material_file.write_text(json.dumps(self.material), encoding="utf-8")
        staging_metadata(self.material_file, self.old_id, [])
        self.image = self.images / "bear.png"
        self.image.write_bytes(encode_png(1, 1, bytes((255, 0, 0, 255))))
        self.catalog = self.images / "image-catalog.json"
        self.catalog.write_text(json.dumps({"slots": {self.texture_id.replace("-", ""): {
            "source": "Assets/Texture2D/bear.png", "media": "bear.png", "srgb": True, "sha256": digest(self.image)}}}), encoding="utf-8")
        self.level = {"nodes": [{"id": "a", "name": "CupA", "components": {}, "materialSlots": {"name:unity-slot-0": self.old_id}},
                                {"id": "b", "name": "CupB", "components": {}, "materialSlots": {"name:unity-slot-0": self.old_id}}]}
        self.record = {"weather": "Night", "frame": 100, "renderers": [{"path": "CupA", "kind": "MeshRenderer", "enabled": True,
                       "visible": True, "materials": [{"slot": 0, "shader": "lilToon", "renderQueue": 2000,
                       "colors": [{"name": "_Color", "value": dict(r=.5, g=1, b=1, a=1)}],
                       "textures": [{"name": "_MainTex", "texture": "bear", "scale": {"x": 1, "y": 1}, "offset": {"x": 0, "y": 0}}]}]}]}
        self.frame = self.root / "frame.json"

    def install(self):
        self.frame.write_text(json.dumps(self.record), encoding="utf-8")
        return captured.install(self.frame, self.catalog, self.assets, self.level)

    def test_clones_shared_material_for_captured_instance_and_keeps_original(self):
        original = self.material_file.read_bytes()
        proof = self.install()
        self.assertEqual(proof["changedSlots"], 1)
        self.assertEqual(proof["pending"], [])
        new_id = self.level["nodes"][0]["materialSlots"]["name:unity-slot-0"]
        self.assertNotEqual(new_id, self.old_id)
        self.assertEqual(self.level["nodes"][1]["materialSlots"]["name:unity-slot-0"], self.old_id)
        self.assertEqual(self.material_file.read_bytes(), original)
        path = self.assets / "captured-surfaces" / (new_id.replace("-", "") + ".azurematerial")
        variant = json.loads(path.read_bytes())
        self.assertEqual(variant["parameters"]["baseColor"], [.5, 1, 1, 1])
        self.assertEqual(variant["parameters"]["roughness"], self.material["parameters"]["roughness"])
        self.assertEqual(variant["textures"]["albedo"]["asset"], self.texture_id)
        metadata = json.loads(path.with_suffix(".azurematerial.azmeta").read_bytes())
        self.assertFalse(metadata["settings"].get("retainSourceSnapshot", True))

    def test_distinct_instances_get_distinct_variants(self):
        other = copy.deepcopy(self.record["renderers"][0])
        other["path"] = "CupB"
        other["materials"][0]["colors"][0]["value"]["r"] = .25
        self.record["renderers"].append(other)
        self.assertEqual(self.install()["changedSlots"], 2)
        self.assertNotEqual(self.level["nodes"][0]["materialSlots"], self.level["nodes"][1]["materialSlots"])

    def test_corrupt_image_rejects_publication_and_keeps_bindings(self):
        self.image.write_bytes(b"corrupt")
        level = copy.deepcopy(self.level)
        with self.assertRaises(ValueError):
            self.install()
        self.assertEqual(self.level, level)
        self.assertFalse((self.assets / "captured-surfaces").exists())

    def test_unknown_image_is_reported_with_the_source_slot(self):
        self.record["renderers"][0]["materials"][0]["textures"][0]["texture"] = "runtime-only-image"
        level = copy.deepcopy(self.level)
        proof = self.install()
        self.assertEqual(proof["changedSlots"], 0)
        self.assertEqual(proof["pending"][0]["path"], "CupA")
        self.assertEqual(self.level, level)
        self.assertFalse((self.assets / "captured-surfaces").exists())

    def test_identical_captured_state_preserves_binding_and_creates_no_variants(self):
        target = self.assets / "existing.png"
        target.write_bytes(self.image.read_bytes())
        staging_metadata(target, self.texture_id, [])
        self.material["parameters"]["baseColor"] = [.5, 1, 1, 1]
        self.material["textures"]["albedo"] = {"asset": self.texture_id, "usage": "albedo"}
        self.material_file.write_text(json.dumps(self.material), encoding="utf-8")
        level = copy.deepcopy(self.level)
        proof = self.install()
        self.assertEqual(proof["changedSlots"], 0)
        self.assertEqual(proof["newImages"], 0)
        self.assertEqual(self.level, level)
        self.assertFalse((self.assets / "captured-surfaces").exists())
        metadata = json.loads(target.with_suffix(".png.azmeta").read_bytes())
        self.assertEqual(metadata["settings"], {})

    def test_failed_publication_keeps_bindings_and_source_material(self):
        level = copy.deepcopy(self.level)
        original = self.material_file.read_bytes()
        with patch.object(captured, "publish_directory", side_effect=OSError("publication failed")):
            with self.assertRaisesRegex(OSError, "publication failed"):
                self.install()
        self.assertEqual(self.level, level)
        self.assertEqual(self.material_file.read_bytes(), original)
        self.assertFalse((self.assets / "captured-surfaces").exists())
