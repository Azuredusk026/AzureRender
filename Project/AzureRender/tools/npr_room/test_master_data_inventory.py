"""Check that the startup label is paired with the typed loader branches."""

import unittest

from tools.npr_room.master_data_inventory import master_locations


class MasterDataTests(unittest.TestCase):
    def test_inventory_requires_a_branch_and_one_location_for_each_type(self):
        catalog = {
            "keys": [{"value": "MasterData", "locations": [0, 1]}],
            "locations": [
                {"internalId": "Assets/One.asset", "resourceType": {"m_ClassName": "Bulbul.One"}},
                {"internalId": "Assets/Two.asset", "resourceType": {"m_ClassName": "Bulbul.MasterData.Two"}},
            ],
        }
        source = 'item is Two two; item is One one;'
        self.assertEqual(master_locations(catalog, source), [
            {"location": 0, "asset": "Assets/One.asset", "type": "Bulbul.One"},
            {"location": 1, "asset": "Assets/Two.asset", "type": "Bulbul.MasterData.Two"},
        ])
        with self.assertRaisesRegex(ValueError, "branches"):
            master_locations(catalog, 'item is Two two;')
        catalog["locations"][1]["resourceType"]["m_ClassName"] = "Other.One"
        with self.assertRaisesRegex(ValueError, "Duplicate"):
            master_locations(catalog, source)
