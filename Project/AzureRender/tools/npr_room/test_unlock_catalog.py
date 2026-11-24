"""Check the exact scenario, price and date values used by room unlock rules."""

import unittest

from tools.npr_room.unlock_catalog import item_rules


class UnlockCatalogTests(unittest.TestCase):
    def test_keeps_unconditional_and_paid_rules_distinct(self):
        def item(name, kind, value, expire=0):
            return {"ItemType": name, "ConditionsType": kind, "Arg1": value, "Arg2": 0, "Arg3": 0,
                    "UnlockExpire": expire, "IsMobileDemoUserLockedTarget": 0}

        rules = item_rules([
            item("HeavyRain", "Scenario", "None"),
            item("LightRain", "Scenario", "main_19", 20260430235959),
            item("City", "PointPurchase", 14000),
        ], {"main_19"})
        self.assertEqual(rules["HeavyRain"]["policy"], "always")
        self.assertEqual(rules["LightRain"]["scenario"], "main_19")
        self.assertEqual(rules["LightRain"]["expires"], "20260430235959")
        self.assertEqual(rules["City"]["cost"], 14000)
        with self.assertRaisesRegex(ValueError, "scenario"):
            item_rules([item("Night", "Scenario", "missing")], {"main_19"})
        with self.assertRaisesRegex(ValueError, "price"):
            item_rules([item("City", "PointPurchase", -5)], set())


if __name__ == "__main__":
    unittest.main()
