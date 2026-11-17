"""Check handedness and HDR encoding for the native sky transfer."""

import unittest

import numpy as np

from tools.npr_room.windows_sky_environment import rgbe, sample_sky


class WindowsSkyEnvironmentTests(unittest.TestCase):
    def test_axes_keep_unity_and_azure_two_way_orientation(self):
        cube = np.broadcast_to(np.arange(6)[:, None, None, None], (6, 4, 4, 3))
        directions = np.asarray(((1, 0, 0), (-1, 0, 0), (0, 1, 0), (0, -1, 0), (0, 0, 1), (0, 0, -1)))
        colors = sample_sky(cube, directions)
        np.testing.assert_array_equal(colors[:, 0], [0, 1, 2, 3, 5, 4])

    def test_radiance_encoding_preserves_black_and_high_values(self):
        linear = np.asarray([[[0, 0, 0], [.019, .62, 1.0597], [2, 16, 8]]], dtype=np.float64)
        encoded = rgbe(linear)
        np.testing.assert_array_equal(encoded[0, 0], [0, 0, 0, 0])
        scale = np.exp2(encoded[0, 1:, 3].astype(np.float64) - 136)
        reconstructed = encoded[0, 1:, :3] * scale[:, None]
        self.assertTrue(np.all(np.abs(reconstructed - linear[0, 1:]) < scale[:, None]))


if __name__ == "__main__":
    unittest.main()
