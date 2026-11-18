import unittest

import numpy as np

from tools.npr_room.windows_weather_environment import compose


class WeatherEnvironmentTests(unittest.TestCase):
    def test_uncovered_cloud_texels_do_not_bleed_color(self):
        sky = np.zeros((6, 4, 4, 4))
        sky[..., 3] = 1
        cloud = np.broadcast_to([100, 0, 0, 0], (6, 2, 2, 4)).copy()
        cloud[:, :, 0] = [0, 1, 0, 1]
        image = compose(sky, cloud)
        self.assertEqual(image.shape, (8, 16, 3))
        self.assertEqual(float(image[..., 0].max()), 0)
        self.assertGreater(float(image[..., 1].max()), .9)
        self.assertTrue(np.any((image[..., 1] > 0) & (image[..., 1] < 1)))
