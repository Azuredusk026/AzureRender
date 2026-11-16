import unittest

from startup_splash import sample_curve


class StartupSplashTests(unittest.TestCase):
    def test_hermite_and_stepped_keys(self):
        keys = [
            {"time": 0, "value": 0, "outSlope": 4, "inSlope": 0, "weightedMode": 0},
            {"time": 1, "value": 1, "outSlope": 0, "inSlope": 0, "weightedMode": 0},
        ]
        self.assertEqual(sample_curve(keys, 0.25), 0.71875)
        self.assertEqual(sample_curve(keys, 0.75, stepped=True), 0)
        self.assertEqual(sample_curve(keys, 1, stepped=True), 1)
        self.assertEqual(sample_curve(keys, 1.1), 1)
