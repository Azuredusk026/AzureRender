import math
import unittest
import numpy as np

from tools.npr_room import original_weather_fields
from tools.npr_room.windows_weather_probe import COMMON_SCALARS, CLOUD_SCALARS, FOG_SCALARS
from tools.npr_room.windows_weather_probe import COMMON_COLORS, CLOUD_COLORS, FOG_COLORS


def frame():
    scalar_names = set(COMMON_SCALARS) | set(CLOUD_SCALARS) | set(FOG_SCALARS) | {
        "CZY_FogDepthMultiplier", "_UnderwaterRenderingEnabled", "_FullySubmerged"}
    color_names = set(COMMON_COLORS) | set(CLOUD_COLORS) | set(FOG_COLORS) | {"CZY_SunDirection"}
    values = {name: {"x": 0, "y": 0, "z": 0, "w": 0} for name in [
        "_TimeParameters", "CZY_AltocumulusWindSpeed", "CZY_StormDirection",
        "CZY_VariationWindDirection", "CZY_MoonDirection", "_GlobalMipBias"]}
    values["_TimeParameters"] = dict(zip("xyzw", (2, math.sin(2), math.cos(2), 0)))
    record = {"weather": "Night", "simulationTime": 2, "position": {"x": 1, "y": 2, "z": 3},
              "weatherScalars": [{"name": name, "value": 0} for name in scalar_names],
              "weatherVectors": [{"name": name, "value": value} for name, value in values.items()],
              "shaderValues": [{"name": name + "ID", "value": dict(zip("xyzw", (.1, .2, .3, .4)))}
                               for name in color_names]}
    next(row for row in record["weatherScalars"] if row["name"] == "CZY_FogDepthMultiplier")["value"] = 1.31
    return record


class OriginalWeatherFieldsTests(unittest.TestCase):
    def test_preserves_captured_global_values_and_zero_storm_direction(self):
        record = frame()
        cloud = original_weather_fields.captured_fields(record, "cozy-cloud")
        fog = original_weather_fields.captured_fields(record, "fog")
        self.assertEqual(cloud["CZY_StormDirection"], (0, 0, 0))
        self.assertEqual(cloud["_TimeParameters"], (2, math.sin(2), math.cos(2), 0))
        self.assertEqual(fog["CZY_FogDepthMultiplier"], (1.31,))
        self.assertEqual(fog["_WorldSpaceCameraPos"], (1, 2, 3))
        self.assertEqual(fog["CZY_FilterColor"], (.1, .2, .3, .4))
        self.assertEqual(fog["CZY_SunDirection"], (.1, .2, .3))

    def test_rejects_unknown_program_missing_colors_and_invalid_source_mask(self):
        with self.assertRaises(ValueError):
            original_weather_fields.captured_fields(frame(), "sky")
        record = frame()
        record["shaderValues"] = []
        with self.assertRaises(ValueError):
            original_weather_fields.captured_fields(record, "fog")
        record = frame()
        next(row for row in record["weatherScalars"] if row["name"] == "_FullySubmerged")["value"] = 1
        with self.assertRaises(ValueError):
            original_weather_fields.captured_fields(record, "fog")

    def test_preserves_original_renderer_world_transform_and_rejects_ambiguity(self):
        def node(identity, parent, position, scale):
            return {"id": identity, "parentId": parent, "position": dict(zip("xyz", position)),
                    "scale": dict(zip("xyz", scale)), "rotation": dict(zip("xyzw", (0, 0, 0, 1)))}
        record = {"transforms": [node(1, 0, (15, 0, 0), (2, 2, 2)), node(2, 1, (1, 2, 3), (3, 4, 5))],
                  "renderers": [{"enabled": True, "transformId": 2, "materials": [{"shader": "fixture"}]}]}
        matrix = original_weather_fields.captured_model(record, "fixture")
        np.testing.assert_allclose(matrix[:3, 3], [17, 4, 6])
        np.testing.assert_allclose(np.diag(matrix)[:3], [6, 8, 10])
        record["renderers"].append(record["renderers"][0])
        with self.assertRaises(ValueError):
            original_weather_fields.captured_model(record, "fixture")
