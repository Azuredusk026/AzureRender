import math
import unittest

from tools.npr_room.build_story_desk_room import euler_xyz


class DeskPoseTests(unittest.TestCase):
    def test_unity_yaw_reverses_in_room_coordinates(self):
        value = dict(x=0, y=.5, z=0, w=math.sqrt(.75))
        expected = (0, -60, 0)
        for actual, target in zip(euler_xyz(value), expected, strict=True):
            self.assertAlmostEqual(actual, target)

    def test_source_textbook_preserves_tilt_and_roll(self):
        value = dict(x=.0017429327, y=.21761562, z=.0019701303, w=.976031)
        angles = [math.radians(component) for component in euler_xyz(value)]
        cx, cy, cz = [math.cos(angle / 2) for angle in angles]
        sx, sy, sz = [math.sin(angle / 2) for angle in angles]
        recomposed = (
            sx * cy * cz + cx * sy * sz,
            cx * sy * cz - sx * cy * sz,
            cx * cy * sz + sx * sy * cz,
            cx * cy * cz - sx * sy * sz,
        )
        length = math.hypot(*value.values())
        target = (-value["x"], -value["y"], value["z"], value["w"])
        for actual, wanted in zip(recomposed, target, strict=True):
            self.assertAlmostEqual(actual, wanted / length, places=9)


if __name__ == "__main__":
    unittest.main()
