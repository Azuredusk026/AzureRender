import unittest
import math

import numpy as np

from tools.npr_room.windows_sky_probe import FACES
from tools.npr_room.windows_room_fog import DEPTH_ENCODING, SLICES, depth_samples
from tools.npr_room.windows_weather_probe import depth_stripes, fog_camera, FOG_DEPTHS
from tools.npr_room import windows_room_fog


class WindowsWeatherProbeTests(unittest.TestCase):
    def test_reversed_depth_round_trips_through_original_fog_projection(self):
        depth = depth_stripes(128)
        _, matrices = fog_camera(*FACES[4])
        inverse = np.array(matrices["unity_CameraInvProjection"]).reshape(4, 4).T
        self.assertEqual(depth.shape, (128, 128))
        self.assertTrue(np.all(depth[:, 1:] <= depth[:, :-1]))
        for index, distance in enumerate(FOG_DEPTHS):
            sample = float(depth[64, int((index + .5) * 128 / len(FOG_DEPTHS))])
            recovered = inverse @ [0, 0, 1 - 2 * sample, 1]
            self.assertAlmostEqual(-recovered[2] / recovered[3], distance, delta=2e-4)

    def test_room_volume_slices_sample_the_engine_depth_texel_centres(self):
        offset, multiplier, scale = DEPTH_ENCODING
        centres = np.log2(depth_samples() * multiplier + offset) * scale
        np.testing.assert_allclose(centres, (np.arange(SLICES) + .5) / SLICES, atol=1e-12)

    def test_original_camera_clip_planes_control_projection_and_depth_encoding(self):
        near, far = .05, 1000
        distances = depth_samples(near=near, far=far)
        _, matrices = fog_camera(*FACES[4], near=near, far=far)
        inverse = np.array(matrices["unity_CameraInvProjection"]).reshape(4, 4).T
        for distance in distances:
            sample = near * (far / distance - 1) / (far - near)
            recovered = inverse @ [0, 0, 1 - 2 * sample, 1]
            self.assertAlmostEqual(-recovered[2] / recovered[3], distance, delta=1e-6)
        binding = windows_room_fog.binding("assets:/fog.azfog", {"farPlane": far}, True)
        scale = binding["projection"]["depthEncoding"][2]
        np.testing.assert_allclose(np.log2(1 + distances) * scale,
                                   (np.arange(SLICES) + .5) / SLICES, atol=1e-12)

    def test_original_camera_matrix_preserves_native_view_axes(self):
        matrix = np.diag([1., 1., -1., 1.])
        matrix[:3, 3] = [-3, -4, 5]
        focal = 1 / math.tan(math.radians(35) / 2)
        projection = np.array([[focal * 9/16, 0, 0, 0], [0, focal, 0, 0],
                               [0, 0, -1.0001, -.100005], [0, 0, -1, 0]])
        record = {"width": 1280, "height": 720, "near": .05, "far": 1000, "reversedZ": True,
                  "position": {"x": 3, "y": 4, "z": 5},
                  "worldToCamera": {f"m{row}{column}": matrix[row, column]
                                    for row in range(4) for column in range(4)},
                  "projection": {f"m{row}{column}": projection[row, column]
                                 for row in range(4) for column in range(4)}}
        view = windows_room_fog.capture_original_view(record)
        np.testing.assert_allclose(view["position"], [3, 4, -5])
        np.testing.assert_allclose(view["target"], [3, 4, -6])
        np.testing.assert_allclose(view["up"], [0, 1, 0])
        self.assertAlmostEqual(view["fieldOfViewRadians"], math.radians(35))
        self.assertEqual(view["farPlane"], 1000)
        record["far"] = 500
        with self.assertRaisesRegex(ValueError, "projection"):
            windows_room_fog.capture_original_view(record)

    def test_rejects_invalid_fog_clip_planes(self):
        for near, far in [(0, 1000), (.1, .1), (.1, float("nan"))]:
            with self.assertRaises(ValueError):
                fog_camera(*FACES[4], near=near, far=far)
            with self.assertRaises(ValueError):
                depth_samples(near=near, far=far)
