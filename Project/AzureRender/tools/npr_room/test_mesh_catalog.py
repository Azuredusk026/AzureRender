import unittest

from mesh_catalog import conversion_kind


class MeshCatalogTests(unittest.TestCase):
    def test_never_routes_bound_or_morph_assets_through_static_export(self):
        self.assertEqual(conversion_kind({"m_BindPose": [object()], "m_Shapes": {"shapes": []}}), "skin")
        self.assertEqual(conversion_kind({"m_BindPose": [], "m_Shapes": {"shapes": [object()]}}), "morph")
        self.assertEqual(conversion_kind({"m_BindPose": [], "m_Shapes": {"shapes": []}}), "static")


if __name__ == "__main__":
    unittest.main()
