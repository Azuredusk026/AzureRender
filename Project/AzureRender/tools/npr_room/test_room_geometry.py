import unittest

from room_geometry import merge_room_geometry


class RoomGeometryTests(unittest.TestCase):
    def test_reuses_imported_geometry_slots_for_original_furniture_and_motion(self):
        furniture_nodes = {
            "transform-7570": "study-chair", "transform-7993": "study-desk", "transform-7420": "study-parent",
        }
        quick_room = {
            "id": "room",
            "sceneImports": {"recovery-furniture": {"nodeMap": furniture_nodes}},
            "resources": [{"id": "chair-glb", "asset": "chair-original"}, {"id": "desk-glb", "asset": "desk-original"}],
            "nodes": [{"id": "study-parent"}, {"id": "study-chair", "resourceId": "chair-glb",
                                              "components": {"azure.animator": {"data": {"state": "window"}}}},
                      {"id": "study-desk", "resourceId": "desk-glb", "components": {}}, {"id": "room-timer"}],
        }
        geometry = {
            "id": "room-camera",
            "sceneImports": {"RoomScene": {"nodeMap": {"unity-7570": "chair", "unity-7993": "desk"}}},
            "resources": [{"id": "full-chair", "asset": "full-chair-asset"}, {"id": "full-desk", "asset": "full-desk-asset"}],
            "nodes": [{"id": "chair", "resourceId": "full-chair", "components": {"azure.transform": [1, 2, 3]}},
                      {"id": "desk", "resourceId": "full-desk", "components": {}}, {"id": "camera"}],
        }

        room, replacements = merge_room_geometry(quick_room, geometry)

        self.assertEqual(room["id"], "room")
        self.assertEqual(room["resources"], geometry["resources"])
        self.assertEqual(replacements, {"full-chair-asset": "chair-original", "full-desk-asset": "desk-original"})
        self.assertEqual(room["nodes"][0]["components"]["azure.transform"], [1, 2, 3])
        self.assertEqual(room["nodes"][0]["components"]["azure.animator"]["data"]["state"], "window")
        self.assertEqual([node["id"] for node in room["nodes"]], ["chair", "desk", "camera", "room-timer"])


if __name__ == "__main__":
    unittest.main()
