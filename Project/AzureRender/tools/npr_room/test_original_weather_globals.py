import tempfile
from pathlib import Path
import unittest

from tools.npr_room import original_room_frames
from tools.npr_room.windows_weather_probe import COMMON_SCALARS, CLOUD_SCALARS, FOG_SCALARS


class OriginalWeatherGlobalsTests(unittest.TestCase):
    def record(self):
        names = set(COMMON_SCALARS) | set(CLOUD_SCALARS) | set(FOG_SCALARS) | {
            "CZY_FogDepthMultiplier", "_UnderwaterRenderingEnabled", "_FullySubmerged"}
        return {"simulationTime": 2, "weatherScalars": [{"name": name, "value": 1} for name in sorted(names)],
                "weatherVectors": [{"name": name, "value": {"x": 2, "y": 0, "z": 0, "w": 0}}
                                   for name in ["_TimeParameters", "CZY_AltocumulusWindSpeed",
                                                "CZY_StormDirection", "CZY_VariationWindDirection",
                                                "CZY_MoonDirection", "_GlobalMipBias"]]}

    def test_validates_complete_unique_and_finite_same_frame_globals(self):
        proof = original_room_frames.verify_weather_globals(self.record())
        self.assertEqual(proof, {"weatherScalars": 40, "weatherVectors": 6})
        for missing in ["weatherScalars", "weatherVectors"]:
            record = self.record()
            record[missing].pop()
            with self.assertRaises(ValueError):
                original_room_frames.verify_weather_globals(record)
        record = self.record()
        record["weatherScalars"].append(record["weatherScalars"][0])
        with self.assertRaises(ValueError):
            original_room_frames.verify_weather_globals(record)

    def test_rejects_nonfinite_and_different_frame_time(self):
        record = self.record()
        record["weatherScalars"][0]["value"] = float("nan")
        with self.assertRaises(ValueError):
            original_room_frames.verify_weather_globals(record)
        record = self.record()
        record["weatherVectors"][0]["value"]["x"] = 1
        with self.assertRaises(ValueError):
            original_room_frames.verify_weather_globals(record)

    def test_diagnostic_sample_count_is_explicit_and_bounded(self):
        with tempfile.TemporaryDirectory() as directory:
            for count in [1, 13, 2.5]:
                with self.assertRaisesRegex(ValueError, "sample count"):
                    original_room_frames.verify_frames(Path(directory), samples_per_weather=count)
            with self.assertRaisesRegex(ValueError, "twelve original frames"):
                original_room_frames.verify_frames(Path(directory))
