import unittest

import numpy as np

from tools.npr_room.windows_sky_probe import FACES
from tools.npr_room.windows_weather_probe import depth_stripes, fog_camera, FOG_DEPTHS


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
