import unittest
from itertools import product
import math

from tools.npr_room.original_room_frames import verify_render_state


def state():
    return {"transforms": [{"id": 1}],
            "lights": [{"id": 2, "transformId": 1, "intensity": 1, "color": {"r": 1, "g": 1, "b": 1, "a": 1}}],
            "renderers": [{"id": 3, "transformId": 1, "materials": [
                {"shader": "Transparent", "floats": [{"name": "_ZWrite", "value": 1}],
                 "colors": [{"name": "_Color", "value": {"r": 1, "g": 1, "b": 1, "a": 0}}]}]}],
            "weatherScalars": [{"name": "CZY_FogIntensity", "value": .1}],
            "environment": {"ambientIntensity": 1, "reflectionIntensity": 1}}


class OriginalRenderStateTests(unittest.TestCase):
    def test_verifies_source_light_and_material_ownership(self):
        proof = verify_render_state(state())
        self.assertEqual(proof, {"lights": 1, "renderers": 1, "materials": 1, "weatherScalars": 1})

    def test_rejects_missing_state_nonfinite_values_and_unbound_renderer(self):
        for mutate in (lambda row: row.pop("lights"),
                       lambda row: row["lights"][0].update(intensity=float("nan")),
                       lambda row: row["renderers"][0].update(transformId=99)):
            row = state()
            mutate(row)
            with self.assertRaises(ValueError):
                verify_render_state(row)

    def test_requires_finite_ambient_coefficients_and_normalized_probe_directions(self):
        row = state()
        with self.assertRaises(ValueError):
            verify_render_state(row, require_ambient_probe=True)
        directions = [direction for direction in product((-1, 0, 1), repeat=3) if any(direction)]
        row["environment"]["ambientProbeSamples"] = [
            {"direction": dict(zip("xyz", (value / math.hypot(*direction) for value in direction))),
             "value": {"r": 0, "g": 0, "b": 0, "a": 1}} for direction in directions]
        row["environment"].update(ambientProbe=[0] * 27,
                                  lightsUseLinearIntensity=True, lightsUseColorTemperature=True)
        self.assertEqual(verify_render_state(row, require_ambient_probe=True)["ambientProbeDirections"], 26)
        row["environment"]["ambientProbe"][0] = float("nan")
        with self.assertRaises(ValueError):
            verify_render_state(row, require_ambient_probe=True)
        row["environment"].update(ambientProbe=[0] * 27,
            lightsUseLinearIntensity=True, lightsUseColorTemperature=True,
            ambientProbeSamples=[{"direction": {"x": 1, "y": 0, "z": 0},
                                  "value": {"r": 0, "g": 0, "b": 0, "a": 1}}])
        with self.assertRaises(ValueError):
            verify_render_state(row, require_ambient_probe=True)


if __name__ == "__main__":
    unittest.main()
