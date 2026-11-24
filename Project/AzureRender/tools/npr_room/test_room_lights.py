"""Verify the original room light transform and the active desktop layer."""

import math
import unittest

from tools.npr_room.room_lights import light_inventory, cloud_lights


class RoomLightTests(unittest.TestCase):
    def test_transform_lighting_axis_and_filter_by_active_hierarchy(self):
        def transform(game, parent, position, rotation, scale):
            return {"m_GameObject": {"fileID": game}, "m_Father": {"fileID": parent},
                    "m_LocalPosition": dict(zip("xyz", position)), "m_LocalRotation": dict(zip("xyzw", rotation)),
                    "m_LocalScale": dict(zip("xyz", scale))}

        def game(name, transform_id, active=1):
            return {"m_Name": name, "m_IsActive": active,
                    "m_Component": [{"component": {"fileID": transform_id}}]}

        def light(game_id, mask):
            return {"m_GameObject": {"fileID": game_id}, "m_Enabled": 1, "m_Type": 0, "m_Intensity": .8,
                    "m_Color": {"r": .8, "g": .9, "b": 1, "a": 1}, "m_Range": 4, "m_CullingMask": {"m_Bits": mask},
                    "m_SpotAngle": 60, "m_InnerSpotAngle": 30}

        class Scene:
            s = math.sqrt(.5)
            data = {
                1: game("Cloudy", 3), 2: game("Spot", 4),
                3: transform(1, 0, (0, 0, 2), (0, 0, 0, 1), (2, 3, 4)),
                4: transform(2, 3, (1, 0, 0), (0, s, 0, s), (1, 1, 1)),
                5: light(2, 4294967295), 6: light(2, 0),
            }
            documents = {key: (kind,) for key, kind in zip(data, (1, 1, 4, 4, 108, 108))}
            def read(self, key):
                return self.data[key]

        lights = light_inventory(Scene())
        self.assertEqual(len(lights), 2)
        rows = cloud_lights(lights)
        self.assertEqual([row["fileID"] for row in rows], [5])
        self.assertEqual(rows[0]["position"], [2, 0, -2])
        for a, b in zip(rows[0]["direction"], (1, 0, 0)):
            self.assertAlmostEqual(a, b)
        self.assertAlmostEqual(rows[0]["shape"]["innerCone"], math.pi / 12)
        self.assertAlmostEqual(rows[0]["shape"]["outerCone"], math.pi / 6)


if __name__ == "__main__":
    unittest.main()
