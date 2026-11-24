"""Check that the startup label is paired with the typed loader branches."""

import unittest

from tools.npr_room.master_data_inventory import loader_exports, member_reads, master_locations


class MasterDataTests(unittest.TestCase):
    def test_groups_loader_fields_and_only_counts_typed_receivers(self):
        loader = ('if (item is A a) { AData = a; Primary = a.Value; }\n'
                  'if (item is B b) { BItems = b.Items.ToDictionary(x => x.Id); }')
        self.assertEqual(loader_exports(loader), {"A": ["AData", "Primary"], "B": ["BItems"]})
        consumer = ('private MasterDataLoader loader;\n'
                    'var primary = loader.Primary;\n'
                    'var list = unrelated.BItems;\n'
                    'return loader.BItems;')
        self.assertEqual(member_reads(consumer, {"AData", "Primary", "BItems"}),
                         {"Primary": [2], "BItems": [4]})
        with self.assertRaisesRegex(ValueError, "loader fields"):
            loader_exports('if (item is A a) {}')

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
