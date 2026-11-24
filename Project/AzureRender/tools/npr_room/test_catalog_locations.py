from base64 import b64encode
from struct import pack
import unittest

from reference_inventory import catalog_locations, locate_catalog_assets, resource_names


def encoded(data):
    return b64encode(data).decode("ascii")


class CatalogLocationTests(unittest.TestCase):
    def test_reconstructs_label_membership_and_dependencies(self):
        key_data = pack("<i", 3) + bytes.fromhex(
            "00 0a000000 4d617374657244617461 "
            "01 02000000 2d4e "
            "03 63000000"
        )
        buckets = pack("<10i", 3, 4, 1, 0, 19, 1, 0, 26, 1, 1)
        entries = pack("<15i", 2, 0, 0, 2, 0, -1, 1, 0, 1, 0, -1, 0, -1, 2, 0)
        catalog = {
            "m_KeyDataString": encoded(key_data),
            "m_BucketDataString": encoded(buckets),
            "m_EntryDataString": encoded(entries),
            "m_ProviderIds": ["Provider"],
            "m_InternalIds": ["Assets/Story.asset", "master.bundle"],
            "m_resourceTypes": [{}],
        }
        keys, locations = catalog_locations(catalog)
        self.assertEqual(keys, [
            {"kind": 0, "value": "MasterData", "locations": [0]},
            {"kind": 1, "value": "中", "locations": [0]},
            {"kind": 3, "value": 99, "locations": [1]},
        ])
        self.assertEqual(locations[0]["dependencyKey"], 2)
        self.assertEqual(locations[0]["internalId"], "Assets/Story.asset")
        self.assertIsNone(locations[1]["dependencyKey"])

        catalog["m_BucketDataString"] = encoded(buckets[:-4] + pack("<i", 2))
        with self.assertRaisesRegex(ValueError, "location index"):
            catalog_locations(catalog)


    def test_catalog_candidates_keep_ambiguous_resources_unresolved(self):
        assets = {
            "Assets/App/Track.ogg": "1" * 32,
            "Assets/A/Resources/Shared.prefab": "2" * 32,
            "Assets/B/Resources/Shared.prefab": "3" * 32,
            "Assets/Resources/Menu.asset": "4" * 32,
        }
        catalog_ids = ["Assets/App/Track.wav", "shared", "Menu", "Missing"]
        matches = locate_catalog_assets(catalog_ids, assets, resource_names(assets))
        self.assertEqual(matches["Assets/App/Track.wav"], {
            "basis": "exported-stem", "paths": ["Assets/App/Track.ogg"]
        })
        self.assertEqual(matches["shared"], {
            "basis": "resources-key-casefold",
            "paths": ["Assets/A/Resources/Shared.prefab", "Assets/B/Resources/Shared.prefab"]
        })
        self.assertEqual(matches["Menu"]["paths"], ["Assets/Resources/Menu.asset"])
        self.assertNotIn("Missing", matches)

if __name__ == "__main__":
    unittest.main()
