from pathlib import Path
import tempfile
import unittest

from tools.npr_room.original_room_visibility import filter_visibility
from tools.npr_room.skinned_mesh_export import UnityScene


def source_object(identity, name, x=0):
    return f"""--- !u!1 &{identity}
GameObject:
  m_Name: {name}
--- !u!4 &{identity + 1}
Transform:
  m_GameObject: {{fileID: {identity}}}
  m_Father: {{fileID: 0}}
  m_LocalPosition: {{x: {x}, y: 0, z: 0}}
  m_LocalRotation: {{x: 0, y: 0, z: 0, w: 1}}
  m_LocalScale: {{x: 1, y: 1, z: 1}}
"""


class OriginalRoomVisibilityTests(unittest.TestCase):
    def test_tracks_static_batch_origin_and_preserves_explicit_coverage(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "scene.unity"
            path.write_text(source_object(10, "Cup") + source_object(20, "Wall"), encoding="utf-8")
            level = {"sceneImports": {"RoomScene": {"nodeMap": {
                "unity-11": "cup", "unity-batch-21": "wall"}}}, "nodes": [
                {"id": "cup", "resourceId": "cup-mesh", "components": {}},
                {"id": "wall", "resourceId": "wall-mesh", "components": {"azure.renderable": {
                    "data": {"visible": False}}}}]}
            report = filter_visibility(level, {"activeObjects": ["Wall"]}, UnityScene(path))
            self.assertFalse(level["nodes"][0]["components"]["azure.renderable"]["data"]["visible"])
            self.assertFalse(level["nodes"][1]["components"]["azure.renderable"]["data"]["visible"])
            self.assertEqual(report["inactive"], ["Cup"])
            self.assertEqual(report["coverageHidden"], ["Wall"])

    def test_rejects_partially_observed_duplicate_paths(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "scene.unity"
            path.write_text(source_object(10, "Book") + source_object(20, "Book"), encoding="utf-8")
            level = {"sceneImports": {"RoomScene": {"nodeMap": {"unity-11": "book"}}},
                     "nodes": [{"id": "book", "resourceId": "mesh", "components": {}}]}
            with self.assertRaisesRegex(ValueError, "Ambiguous"):
                filter_visibility(level, {"activeObjects": ["Book"]}, UnityScene(path))

    def test_resolves_duplicate_path_using_world_pose(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "scene.unity"
            path.write_text(source_object(10, "Book", 0) + source_object(20, "Book", 1), encoding="utf-8")
            level = {"sceneImports": {"RoomScene": {"nodeMap": {"unity-batch-11": "a", "unity-batch-21": "b"}}},
                     "nodes": [{"id": name, "resourceId": "mesh", "components": {}} for name in ("a", "b")]}
            record = {"activeObjects": ["Book"], "transforms": [
                {"id": 8, "parentId": 0, "path": "Book", "position": {"x": 1, "y": 0, "z": 0},
                 "rotation": {"x": 0, "y": 0, "z": 0, "w": 1}, "scale": {"x": 1, "y": 1, "z": 1}}]}
            report = filter_visibility(level, record, UnityScene(path))
            self.assertFalse(level["nodes"][0]["components"]["azure.renderable"]["data"]["visible"])
            self.assertTrue(level["nodes"][1]["components"]["azure.renderable"]["data"]["visible"])
            self.assertEqual(report["poseResolved"], ["Book"])


if __name__ == "__main__":
    unittest.main()
