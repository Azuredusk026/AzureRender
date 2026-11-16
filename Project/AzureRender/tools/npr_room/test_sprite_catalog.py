import unittest

from sprite_catalog import rect_within


class SpriteCatalogTests(unittest.TestCase):
    def test_tracks_missing_and_out_of_bounds_atlas_regions(self):
        self.assertTrue(rect_within({"x": 1, "y": 2, "width": 3, "height": 4},
                                    {"width": 4, "height": 6}))
        self.assertFalse(rect_within({"x": 1, "y": 2, "width": 4, "height": 4},
                                     {"width": 4, "height": 6}))
        self.assertFalse(rect_within({"x": 1, "y": 2, "width": 3, "height": 4}, {}))


    def test_accepts_unity_float32_serialization_at_a_texture_edge(self):
        self.assertTrue(rect_within(
            {"x": 2.0761204, "y": 1.0761205, "width": 357.9239, "height": 358.9239},
            {"width": 360, "height": 360},
        ))
        self.assertFalse(rect_within(
            {"x": 2.0761204, "y": 1.0761205, "width": 357.9240, "height": 358.9239},
            {"width": 360, "height": 360},
        ))


if __name__ == "__main__":
    unittest.main()
