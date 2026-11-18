import unittest

from tools.npr_room.original_room_registration import freeze_model, apply_level_pose


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


if __name__ == "__main__":
    unittest.main()
