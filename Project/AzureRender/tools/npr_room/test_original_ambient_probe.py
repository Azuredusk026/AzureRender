from itertools import product
import json
import math
from pathlib import Path
import tempfile
import unittest

from tools.npr_room.original_ambient_probe import encode, fit_probe
from tools.npr_room import original_ambient_probe


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

    def test_installs_source_grid_metadata_and_scene_binding(self):
        record = {"frame": 1, "weather": "Night", "transforms": [{"id": 1}],
                  "lights": [{"id": 2, "transformId": 1, "intensity": 1, "color": {"r": 1}}],
                  "renderers": [{"id": 3, "transformId": 1,
                                 "materials": [{"shader": "fixture", "floats": [], "colors": []}]}],
                  "weatherScalars": [{"value": 1}],
                  "environment": {"ambientIntensity": 1, "reflectionIntensity": 1,
                                  "lightsUseLinearIntensity": True, "lightsUseColorTemperature": True,
                                  "ambientProbe": [0] * 27, "ambientProbeSamples": samples()}}
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            frame = root / "frame.json"
            frame.write_text(json.dumps(record), encoding="utf-8")
            assets = root / "assets"
            assets.mkdir()
            level = {}
            proof = original_ambient_probe.install(frame, assets, level)
            binding = level["irradianceVolume"]
            self.assertEqual(binding, {"version": 2, "enabled": True, "intensity": 1,
                                       "asset": "assets:/original-ambient/captured.aziv"})
            grid = assets / "original-ambient/captured.aziv"
            self.assertEqual(grid.read_bytes()[:8], b"AZIV0002")
            metadata = json.loads(grid.with_name(grid.name + ".azmeta").read_bytes())
            self.assertEqual(metadata["id"], proof["assetId"])
            self.assertEqual(metadata["dependencies"], [])
            self.assertEqual(json.loads(frame.read_bytes()), record)
            with self.assertRaises(FileExistsError):
                original_ambient_probe.install(frame, assets, {})

    def test_failed_source_validation_leaves_binding_and_assets_untouched(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            frame = root / "frame.json"
            frame.write_text("{}", encoding="utf-8")
            level = {"irradianceVolume": {"enabled": False}}
            with self.assertRaisesRegex(ValueError, "incomplete"):
                original_ambient_probe.install(frame, root, level)
            self.assertEqual(level, {"irradianceVolume": {"enabled": False}})
            self.assertEqual(list(root.iterdir()), [frame])


if __name__ == "__main__":
    unittest.main()
