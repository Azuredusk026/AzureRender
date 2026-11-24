import unittest

from story_inventory import index_story


class StoryInventoryTests(unittest.TestCase):
    def test_keeps_order_and_reports_source_anomalies(self):
        groups = [{"ID": "g1"}, {"ID": "g2"}]
        rows = [
            {"ID": "r", "ScenarioGroupID": "g1", "Command": "2", "LocalizationID": "loc"},
            {"ID": "r", "ScenarioGroupID": "g1", "Command": "5", "LocalizationID": "0"},
            {"ID": "s", "ScenarioGroupID": "g3", "Command": "2", "LocalizationID": "unknown"},
        ]
        report = index_story(groups, rows, [{"ID": "loc", "Ja": "text"}])
        self.assertEqual(report["commandsByGroup"], {"g1": [0, 1], "g2": []})
        self.assertEqual(report["duplicateCommandIDs"], {"r": [0, 1]})
        self.assertEqual(report["commandsWithoutGroup"], {"g3": [2]})
        self.assertEqual(report["unresolvedLocalization"], {"unknown": [2]})
        self.assertEqual(report["commandKinds"], {"2": 2, "5": 1})


if __name__ == "__main__":
    unittest.main()
