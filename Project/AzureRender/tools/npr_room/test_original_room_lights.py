import math
import unittest

from tools.npr_room.original_room_lights import apply_captured_lights


def frame():
    return {"transforms": [
        {"id": 1, "parentId": 0, "position": {"x": 0, "y": 0, "z": 2},
         "rotation": {"x": 0, "y": 0, "z": 0, "w": 1}, "scale": {"x": 2, "y": 3, "z": 4}},
        {"id": 2, "parentId": 1, "position": {"x": 1, "y": 0, "z": 0},
         "rotation": {"x": 0, "y": .5**.5, "z": 0, "w": .5**.5}, "scale": {"x": 1, "y": 1, "z": 1}}],
        "lights": [{"id": 10, "transformId": 2, "path": "Cloudy/Spot", "enabled": True,
                    "kind": "Spot", "color": {"r": .5, "g": .5, "b": .5}, "intensity": .8,
                    "range": 4, "spotAngle": 60, "innerSpotAngle": 30, "useColorTemperature": False,
                    "cullingMask": -1, "shadows": "Soft"}]}


class OriginalRoomLightsTests(unittest.TestCase):
    def test_applies_linear_color_world_position_and_source_spot_cones(self):
        level = {"nodes": [{"id": "unity-light-99"}], "lights": [{"id": "unity-light-99", "nodeId": "unity-light-99"}]}
        proof = apply_captured_lights(level, frame(), True)
        light, = level["lights"]
        self.assertAlmostEqual(light["color"][0], .21404114048223255)
        self.assertEqual(level["nodes"][0]["components"]["azure.transform"]["data"]["translation"], [2, 0, -2])
        self.assertAlmostEqual(light["shape"]["direction"][0], 1)
        self.assertAlmostEqual(light["shape"]["innerCone"], math.pi/12)
        self.assertEqual(proof["sourceShadowModes"], ["Soft"])

    def test_rejects_unowned_lights_and_unsupported_temperature(self):
        with self.assertRaises(ValueError):
            apply_captured_lights({"nodes": [], "lights": [{"id": "user-light"}]}, frame(), True)
        packet = frame()
        packet["lights"][0]["useColorTemperature"] = True
        with self.assertRaises(ValueError):
            apply_captured_lights({"nodes": [], "lights": []}, packet, True)

    def test_rejects_nonfinite_light_factors_before_changing_the_level(self):
        packet = frame()
        packet["lights"][0]["range"] = float("nan")
        level = {"nodes": [], "lights": []}
        with self.assertRaises(ValueError):
            apply_captured_lights(level, packet, True)
        self.assertEqual(level, {"nodes": [], "lights": []})


if __name__ == "__main__":
    unittest.main()
