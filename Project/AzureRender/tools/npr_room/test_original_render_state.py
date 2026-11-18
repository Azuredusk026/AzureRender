import unittest

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


if __name__ == "__main__":
    unittest.main()
