import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.npr_room.original_room_registration import freeze_model, apply_level_pose
from tools.npr_room import original_room_registration as registration
from tools.build_face_sdf import write_glb


def pose(path, identity, parent, position=None):
    return {"path": path, "id": identity, "parentId": parent,
            "position": position or {"x": 1, "y": 2, "z": 3},
            "rotation": {"x": 0, "y": 0, "z": 0, "w": 1},
            "scale": {"x": 1, "y": 1, "z": 1}}


class OriginalRoomRegistrationTests(unittest.TestCase):
    def setUp(self):
        self.record = {"transforms": [pose("Character", 10, 0), pose("Character/Face", 20, 10)],
                       "skins": [{"path": "Character/Face", "mesh": "Face", "bones": ["Character"],
                                  "blendWeights": [25, 75]}]}
        self.model = {"nodes": [{"name": "Character", "children": [1]},
                                {"name": "Face", "mesh": 0, "skin": 0}],
                      "scenes": [{"nodes": [0]}], "skins": [{"joints": [0]}],
                      "meshes": [{"name": "Face", "weights": [0, 0]}], "animations": [{"name": "idle"}]}

    def test_freezes_source_hierarchy_and_normalizes_blend_weights(self):
        proof = freeze_model(self.model, self.record)
        self.assertEqual(proof["transforms"], 2)
        self.assertEqual(self.model["nodes"][0]["translation"], [1, 2, -3])
        self.assertEqual(self.model["nodes"][1]["weights"], [.25, .75])
        self.assertEqual(self.model["animations"], [])

    def test_rejects_reordered_or_missing_palette(self):
        self.record["skins"][0]["bones"] = ["Character/Face"]
        with self.assertRaisesRegex(ValueError, "palette"):
            freeze_model(self.model, self.record)

    def test_rejects_missing_and_nonfinite_pose(self):
        self.record["transforms"][1]["position"]["x"] = float("nan")
        with self.assertRaisesRegex(ValueError, "Nonfinite"):
            freeze_model(self.model, self.record)

    def test_applies_only_identified_level_nodes_and_disables_motion(self):
        level = {"nodes": [{"id": "root", "name": "Character", "components": {
            "azure.transform": {"data": {}}, "azure.animator": {"data": {"state": "idle"}}}},
            {"id": "ui", "components": {"azure.script": {"data": {"enabled": True}}}}]}
        proof = apply_level_pose(level, self.record)
        self.assertEqual(proof["matched"], ["Character"])
        self.assertEqual(level["nodes"][0]["components"]["azure.transform"]["data"]["translation"], [1, 2, -3])
        self.assertNotIn("azure.animator", level["nodes"][0]["components"])
        self.assertFalse(level["nodes"][1]["components"]["azure.script"]["data"]["enabled"])

    def test_preserves_captured_right_angle_room_rotation(self):
        self.record["transforms"][0]["rotation"] = {"x": 0, "y": .5**.5, "z": 0, "w": .5**.5}
        level = {"nodes": [{"id": "root", "name": "Character", "components": {
            "azure.transform": {"data": {}}}}]}
        apply_level_pose(level, self.record)
        angles = level["nodes"][0]["components"]["azure.transform"]["data"]["rotation"]
        self.assertAlmostEqual(angles[1], -90)
        self.assertAlmostEqual(angles[0], 0)
        self.assertAlmostEqual(angles[2], 0)

    def test_copies_source_camera_projection(self):
        self.record.update(camera="Character", near=.05, far=1000,
                           projection={"m11": 2, "m02": 0, "m12": 0})
        level = {"nodes": [{"id": "root", "name": "Character", "components": {
            "azure.transform": {"data": {}}, "azure.camera": {"data": {"farPlane": 1500}}}}]}
        apply_level_pose(level, self.record)
        lens = level["nodes"][0]["components"]["azure.camera"]["data"]
        self.assertEqual(lens["farPlane"], 1000)
        self.assertEqual(lens["nearPlane"], .05)
        self.assertAlmostEqual(lens["fieldOfView"], 53.13010235415598)


class RegistrationEntryTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.room = self.root / "room"
        self.assets = self.room / "assets"
        (self.assets / "heroine").mkdir(parents=True)
        self.model = self.assets / "heroine/Face.glb"
        write_glb(self.model, {"asset": {"version": "2.0"},
                               "nodes": [{"name": "Character"}],
                               "scenes": [{"nodes": [0]}]}, b"")
        (self.assets / "room.azurelevel").write_text(
            json.dumps({"nodes": []}), encoding="utf-8")
        (self.room / "project.azureproject").write_text(
            json.dumps({"startupScene": "assets:/boot.azurelevel"}), encoding="utf-8")
        self.frame = self.root / "frame.json"
        self.frame.write_text(json.dumps({"frame": 1, "weather": "Night",
                                         "transforms": [pose("Character", 10, 0)],
                                         "skins": []}), encoding="utf-8")
        self.output = self.root / "registered"
        self.original = {p.relative_to(self.room): p.read_bytes()
                         for p in self.room.rglob("*") if p.is_file()}
        self.addCleanup(patch.stopall)
        patch.object(registration, "HEROINE_PARTS", ["Face"]).start()
        self.freeze = patch.object(registration, "freeze_model", wraps=registration.freeze_model).start()

    def assert_inputs_preserved(self):
        self.assertEqual(self.original, {p.relative_to(self.room): p.read_bytes()
                                         for p in self.room.rglob("*") if p.is_file()})

    def test_default_entry_publishes_without_original_project(self):
        receipt = registration.stage(self.room, self.frame, self.output)
        self.assertIsNone(receipt["activation"])
        self.assertIsNone(receipt["transparency"])
        descriptor = json.loads((self.output / "project.azureproject").read_bytes())
        self.assertEqual(descriptor["startupScene"], "assets:/room.azurelevel")
        self.assertEqual(json.loads((self.output / "original-pose-receipt.json").read_bytes()), receipt)
        self.assert_inputs_preserved()
        with self.assertRaises(FileExistsError):
            registration.stage(self.room, self.frame, self.output)

    def test_referenced_models_bind_relative_wrapper_and_preserve_base(self):
        level_path = self.assets / "room.azurelevel"
        level_path.write_text(json.dumps({"nodes": [], "resources": [
            {"id": "heroine-Face", "asset": "assets:/heroine/Face.glb"}]}), encoding="utf-8")
        self.original[level_path.relative_to(self.room)] = level_path.read_bytes()
        receipt = registration.stage(self.room, self.frame, self.output, reference_models=True)
        wrapper = self.output / "assets/heroine/Face.gltf"
        model = json.loads(wrapper.read_bytes())
        self.assertEqual(model["buffers"][0]["uri"], "Face.glb")
        self.assertEqual(model["nodes"][0]["translation"], [1, 2, -3])
        self.assertEqual((self.output / "assets/heroine/Face.glb").read_bytes(), self.model.read_bytes())
        level = json.loads((self.output / "assets/room.azurelevel").read_bytes())
        self.assertEqual(level["resources"][0]["asset"], "assets:/heroine/Face.gltf")
        self.assertFalse(json.loads(wrapper.with_suffix(".gltf.azmeta").read_bytes())["settings"]["retainSourceSnapshot"])
        self.assertEqual(receipt["parts"]["Face"]["storage"], "referenced-glb")
        self.assert_inputs_preserved()

    def test_explicit_original_project_reaches_source_adapters(self):
        source = self.root / "unity"
        scene = source / registration.ROOM_SCENE
        scene.parent.mkdir(parents=True)
        scene.write_text("source scene", encoding="utf-8")
        with patch.object(registration, "UnityScene") as scene_reader, \
                patch.object(registration, "filter_visibility", return_value={"matched": 1}) as visibility, \
                patch.object(registration, "install_lens", return_value={"installed": 1}) as lens:
            receipt = registration.stage(self.room, self.frame, self.output, source)
        scene_reader.assert_called_once_with(scene)
        self.assertEqual(visibility.call_args.args[2], scene_reader.return_value)
        self.assertEqual(lens.call_args.args[2], source)
        self.assertEqual(receipt["activation"]["matched"], 1)
        self.assertEqual(receipt["transparency"]["installed"], 1)
        self.assert_inputs_preserved()

    def test_referenced_model_collision_keeps_inputs_and_output_unpublished(self):
        (self.assets / "heroine/Face.gltf").write_text("existing asset", encoding="utf-8")
        self.original[Path("assets/heroine/Face.gltf")] = b"existing asset"
        with self.assertRaises(FileExistsError):
            registration.stage(self.room, self.frame, self.output, reference_models=True)
        self.assertFalse(self.output.exists())
        self.assert_inputs_preserved()

    def test_failed_staging_keeps_inputs_and_output_unpublished(self):
        self.freeze.side_effect = ValueError("Invalid captured palette")
        with self.assertRaisesRegex(ValueError, "palette"):
            registration.stage(self.room, self.frame, self.output)
        self.assertFalse(self.output.exists())
        self.assert_inputs_preserved()

    def test_optional_ambient_probe_uses_the_captured_frame(self):
        with patch.object(registration, "install_ambient_probe", return_value={"assetId": "fixture"}) as install:
            receipt = registration.stage(self.room, self.frame, self.output, ambient_probe=True)
        self.assertEqual(install.call_args.args[0], self.frame)
        self.assertEqual(receipt["ambientProbe"], {"assetId": "fixture"})
        self.assert_inputs_preserved()


if __name__ == "__main__":
    unittest.main()
