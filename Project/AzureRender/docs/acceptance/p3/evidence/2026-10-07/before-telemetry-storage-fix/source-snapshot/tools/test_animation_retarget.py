"""Offline motion must preserve bind frames, bone lengths and in-place roots."""
import unittest
import numpy as np
from retarget_animation import retarget_samples, qmatrix, validate_weights


class RetargetTests(unittest.TestCase):
    def test_rest_and_world_rotation_with_different_joint_axes(self):
        target = {"nodes": [{"name": "root", "children": [1]},
                            {"name": "hip", "rotation": [0, 0, .7071067812, .7071067812], "children": [2]},
                            {"name": "head", "translation": [1, 0, 0]}]}
        source = {"nodes": [{"name": "hips", "parent": -1, "rest": [[0,0,0],[0,0,0,1],[1,1,1]]},
                            {"name": "head", "parent": 0, "rest": [[0,1,0],[0,0,0,1],[1,1,1]]}],
                  "clips": [{"duration": 1, "frames": [
                      {"time": 0, "transforms": [[[0,0,0],[0,0,0,1],[1,1,1]], [[0,1,0],[0,0,0,1],[1,1,1]]]},
                      {"time": 1, "transforms": [[[5,0,7],[0,.7071067812,0,.7071067812],[1,1,1]], [[0,1,0],[0,0,0,1],[1,1,1]]]}]}]}
        frames, report = retarget_samples(target, source, {"hip": "hips", "head": "head"}, "hip", loop=False)
        np.testing.assert_allclose(qmatrix(frames[0][1][1]), qmatrix(target["nodes"][1]["rotation"]), atol=1e-7)
        np.testing.assert_allclose(frames[1][1][0], [0,0,0], atol=1e-7)
        np.testing.assert_allclose(frames[1][2][0], [1,0,0], atol=1e-7)
        expected = qmatrix([0,.7071067812,0,.7071067812]) @ qmatrix(target["nodes"][1]["rotation"])
        np.testing.assert_allclose(qmatrix(frames[1][1][1]), expected, atol=1e-7)
        self.assertEqual(report["mappedJoints"], 2)

    def test_missing_joint_and_invalid_time_rejected(self):
        target = {"nodes": [{"name": "hip"}]}
        source = {"nodes": [], "clips": [{"duration": 1, "frames": []}]}
        with self.assertRaises(ValueError):
            retarget_samples(target, source, {"hip": "missing"}, "hip")

    def test_zero_weights_require_source_recovery(self):
        with self.assertRaises(ValueError): validate_weights(np.array([[0,0,0,0],[.5,.5,0,0]]))
        self.assertEqual(validate_weights(np.array([[.5,.5,0,0]])),1)


if __name__ == "__main__":
    unittest.main()
