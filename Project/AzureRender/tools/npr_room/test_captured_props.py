import copy
import json
from pathlib import Path
from struct import pack
import tempfile
import unittest
from unittest.mock import patch

from tools.build_face_sdf import encode_png, write_glb
from tools.npr_room import captured_props
from tools.npr_room.reference_inventory import digest
from tools.npr_room.skinned_mesh_export import add_accessor
from tools.npr_room.test_original_room_registration import pose


class CapturedPropTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.catalog = self.root / "catalog"
        self.deformers = self.catalog / "deformer-catalog"
        self.images = self.catalog / "image-catalog"
        self.deformers.mkdir(parents=True)
        self.images.mkdir()
        self.assets = self.root / "assets"
        self.assets.mkdir()
        model = {"asset": {"version": "2.0"}, "nodes": [
            {"name": "BookRoot", "children": [1, 2]}, {"name": "PageJoint"}, {"name": "Mesh", "mesh": 0, "skin": 0}],
            "scenes": [{"nodes": [0]}], "bufferViews": [], "accessors": [], "materials": [{"name": "unity-slot-0"}]}
        binary = bytearray()
        position = add_accessor(model, binary, pack("<9f", 0, 0, 0, 1, 0, 0, 0, 1, 0), 3, "VEC3", 5126)
        joints = add_accessor(model, binary, bytes(24), 3, "VEC4", 5123)
        weights = add_accessor(model, binary, pack("<12f", *([1, 0, 0, 0] * 3)), 3, "VEC4", 5126)
        inverse = add_accessor(model, binary, pack("<16f", *(1 if r == c else 0 for c in range(4) for r in range(4))), 1, "MAT4", 5126)
        model["skins"] = [{"joints": [1], "inverseBindMatrices": inverse}]
        model["meshes"] = [{"name": "Book", "primitives": [{"attributes": {
            "POSITION": position, "JOINTS_0": joints, "WEIGHTS_0": weights}, "material": 0}]}]
        model["buffers"] = [{"byteLength": len(binary)}]
        source = self.deformers / "book.glb"
        write_glb(source, model, binary)
        self.row = {"sourceGuid": "a" * 32, "kind": "skin", "media": "book.glb", "sha256": digest(source)}
        self.deformer_manifest = self.deformers / "deformer-catalog.json"
        self.deformer_manifest.write_text(json.dumps({"variants": {"scene:100": self.row}}), encoding="utf-8")
        image = self.images / "book.png"
        image.write_bytes(encode_png(1, 1, bytes((255, 255, 255, 255))))
        (self.images / "image-catalog.json").write_text(json.dumps({"slots": {"b" * 32: {
            "source": "Assets/Texture2D/book.png", "media": "book.png", "sha256": digest(image), "srgb": True}}}), encoding="utf-8")
        self.record = {"weather": "Night", "frame": 100, "simulationTime": 2,
            "transforms": [pose("BookRoot", 1, 0), pose("BookRoot/PageJoint", 2, 1), pose("BookRoot/Mesh", 3, 1)],
            "skins": [{"path": "BookRoot/Mesh", "mesh": "Book", "bones": ["BookRoot/PageJoint"], "blendWeights": []}],
            "renderers": [{"path": "BookRoot/Mesh", "mesh": "Book", "kind": "SkinnedMeshRenderer", "enabled": True, "visible": True,
                "materials": [{"slot": 0, "shader": "lilToon", "renderQueue": 2000, "floats": [],
                    "colors": [{"name": "_Color", "value": dict(r=1, g=1, b=1, a=1)}],
                    "textures": [{"name": "_MainTex", "texture": "book", "scale": {"x": 1, "y": 1}, "offset": {"x": 0, "y": 0}}]}]}]}
        self.frame = self.root / "frame.json"
        self.frame.write_text(json.dumps(self.record), encoding="utf-8")
        self.level = {"nodes": [], "resources": []}

    def test_installs_frozen_skin_and_source_texture_at_world_root(self):
        proof = captured_props.install(self.frame, self.catalog, self.assets, self.level)
        self.assertEqual(proof["count"], 1)
        resource, = self.level["resources"]
        model = json.loads((self.assets / resource["asset"].removeprefix("assets:/")).read_bytes())
        self.assertEqual(model["nodes"][0]["translation"], [1, 2, -3])
        self.assertEqual(model["skins"][0]["joints"], [1])
        self.assertEqual(model["animations"], [])
        self.assertTrue(model["images"][0]["uri"].startswith("data:image/png;base64,"))
        self.assertEqual(self.level["nodes"][0]["resourceId"], resource["id"])
        self.assertNotIn("parentId", self.level["nodes"][0])

    def test_represented_character_meshes_are_excluded(self):
        original = copy.deepcopy(self.level)
        self.assertEqual(captured_props.install(self.frame, self.catalog, self.assets, self.level,
                                               represented_meshes={"Book"})["count"], 0)
        self.assertEqual(self.level, original)
        self.assertEqual(list(self.assets.iterdir()), [])

    def test_ambiguous_variant_keeps_level_and_assets_untouched(self):
        self.deformer_manifest.write_text(json.dumps({"variants": {"scene:100": self.row, "scene:101": self.row}}), encoding="utf-8")
        original = copy.deepcopy(self.level)
        with self.assertRaisesRegex(ValueError, "unique"):
            captured_props.install(self.frame, self.catalog, self.assets, self.level)
        self.assertEqual(self.level, original)
        self.assertEqual(list(self.assets.iterdir()), [])

    def test_sequence_uses_existing_clip_contract_for_page_joint_motion(self):
        other = copy.deepcopy(self.record)
        other.update(frame=104, simulationTime=2 + 4 / 60)
        other["transforms"][1]["position"]["x"] = 2
        second = self.root / "second.json"
        second.write_text(json.dumps(other), encoding="utf-8")
        proof = captured_props.install(self.frame, self.catalog, self.assets, self.level, sequence=[self.frame, second])
        self.assertEqual(proof["props"][0]["motion"]["samples"], 2)
        self.assertEqual(self.level["nodes"][0]["components"]["azure.animator"]["data"]["state"], "captured")

    def test_renderer_mesh_mismatch_rejects_source_before_publication(self):
        self.record["renderers"][0]["mesh"] = "DifferentBook"
        self.frame.write_text(json.dumps(self.record), encoding="utf-8")
        original = copy.deepcopy(self.level)
        with self.assertRaisesRegex(ValueError, "mesh identity"):
            captured_props.install(self.frame, self.catalog, self.assets, self.level)
        self.assertEqual(self.level, original)
        self.assertEqual(list(self.assets.iterdir()), [])

    def test_failed_publication_preserves_resources_and_nodes(self):
        original = copy.deepcopy(self.level)
        with patch.object(captured_props, "publish_directory", side_effect=OSError("publication failed")):
            with self.assertRaisesRegex(OSError, "publication failed"):
                captured_props.install(self.frame, self.catalog, self.assets, self.level)
        self.assertEqual(self.level, original)
        self.assertEqual(list(self.assets.iterdir()), [])
