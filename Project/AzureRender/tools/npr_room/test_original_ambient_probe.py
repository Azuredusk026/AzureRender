from itertools import product
import math
import unittest

from tools.npr_room.original_ambient_probe import encode, fit_probe


def samples():
    result = []
    for raw in product((-1, 0, 1), repeat=3):
        if not any(raw):
            continue
        x, y, z = (value / math.hypot(*raw) for value in raw)
        result.append({"direction": dict(zip("xyz", (x, y, z))),
                       "value": dict(zip("rgba", (1 + 2*z + 3*x*z, 2 + 4*x*y, .5 + y*z, 1)))})
    return result


class OriginalAmbientProbeTests(unittest.TestCase):
    def test_preserves_quadratic_direction_response_and_mirrors_world_z(self):
        probe = fit_probe(samples())
        self.assertAlmostEqual(probe["linear"][0][2], -2)
        self.assertAlmostEqual(probe["linear"][0][3], 1)
        self.assertAlmostEqual(probe["quadratic"][0][3], -3)
        self.assertAlmostEqual(probe["quadratic"][1][0], 4)
        self.assertAlmostEqual(probe["quadratic"][2][1], -1)
        self.assertLess(probe["maximumError"], 1e-12)

    def test_rejects_incomplete_and_nonlinear_probe_responses(self):
        with self.assertRaises(ValueError):
            fit_probe(samples()[:8])
        rows = samples()
        rows[0]["value"]["r"] += 1
        with self.assertRaises(ValueError):
            fit_probe(rows)

    def test_encodes_version_two_and_rejects_unrepresentable_grid_extents(self):
        probe = fit_probe(samples())
        payload = encode(probe, (-2048, -2048, -2048), 4096)
        self.assertEqual(payload[:8], b"AZIV0002")
        self.assertEqual(len(payload), 36 + 128 * 8)
        with self.assertRaises(ValueError):
            encode(probe, (1e38, 1e38, 1e38), 3e38)


if __name__ == "__main__":
    unittest.main()
