import unittest
import gzip
from hashlib import sha256
from io import BytesIO
import json
from pathlib import Path
import tempfile

import numpy as np

from tools.npr_room.windows_weather_environment import compose, convert
from tools.npr_room.windows_sky_environment import rgbe, write_hdr


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


class WeatherCompositionIdentityTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.sky = np.broadcast_to([.2, .4, .6, 1], (6, 4, 4, 4)).astype("<f4").copy()
        self.cloud = np.broadcast_to([.9, .8, .7, .5], (6, 2, 2, 4)).astype("<f4").copy()
        for name, data in (("sky", self.sky), ("cloud", self.cloud)):
            (self.root / (name + ".gz")).write_bytes(gzip.compress(data.tobytes(), mtime=0))
        self.sky_report = {"mode": "Night", "rawSHA256": sha256(self.sky.tobytes()).hexdigest(),
                           "resolution": [4, 4], "preparation": {"sourceSeconds": 2, "referenceSHA256": "recipe"}}
        self.cloud_report = {"mode": "Night", "program": "cozy-cloud", "frames": [
            {"rawSHA256": sha256(self.cloud.tobytes()).hexdigest(),
             "inputs": {"seconds": 2, "referenceSHA256": "recipe", "resolution": [2, 2]}}]}
        self.output = self.root / "environment.hdr"

    def convert(self):
        for name, report in (("sky", self.sky_report), ("cloud", self.cloud_report)):
            (self.root / (name + ".json")).write_text(json.dumps(report), encoding="utf-8")
        return convert(self.root / "sky.gz", self.root / "sky.json", self.root / "cloud.gz",
                       self.root / "cloud.json", 0, self.output)

    def test_rejects_different_missing_and_invalid_capture_identities(self):
        for sky_frame, cloud_frame in (("a" * 64, "b" * 64), ("a" * 64, None),
                                       (None, "a" * 64), ("bad", "bad")):
            with self.subTest(sky=sky_frame, cloud=cloud_frame):
                self.output.unlink(missing_ok=True)
                self.sky_report["preparation"]["frameSHA256"] = sky_frame
                self.cloud_report["frames"][0]["inputs"]["capturedFrameSHA256"] = cloud_frame
                with self.assertRaisesRegex(ValueError, "same captured frame"):
                    self.convert()
                self.assertFalse(self.output.exists())

    def test_captured_input_requires_both_identities(self):
        self.sky_report["preparation"]["input"] = "captured-room-program-globals"
        with self.assertRaisesRegex(ValueError, "same captured frame"):
            self.convert()
        self.assertFalse(self.output.exists())

    def test_matching_capture_preserves_composition_bytes_and_returns_identity(self):
        identity = "a" * 64
        self.sky_report["preparation"]["frameSHA256"] = identity
        self.cloud_report["frames"][0]["inputs"]["capturedFrameSHA256"] = identity
        result = self.convert()
        expected = BytesIO()
        write_hdr(expected, rgbe(compose(self.sky, self.cloud)))
        self.assertEqual(self.output.read_bytes(), expected.getvalue())
        self.assertEqual(result["capturedFrameSHA256"], identity)

    def test_controlled_recipe_composition_keeps_existing_report_and_bytes(self):
        result = self.convert()
        expected = BytesIO()
        write_hdr(expected, rgbe(compose(self.sky, self.cloud)))
        self.assertEqual(self.output.read_bytes(), expected.getvalue())
        self.assertNotIn("capturedFrameSHA256", result)
