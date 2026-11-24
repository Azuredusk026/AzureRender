import unittest

from tools.npr_room.feature_matrix import attach_window_references, settings, story_rows, unlock_rows, window_rows


class FeatureMatrixTests(unittest.TestCase):
    def test_source_rows_preserve_repeated_command_labels_and_unassigned_rows(self):
        groups = [{"ID": "tutorial_01", "Scenario": "18", "EpisodeNumber": "1", "UnlockLevel": "0",
                   "UnlockStoryEpisode": "None", "ContentType": "None", "PlayTiming": "None",
                   "Arg1": "0", "Arg2": "0", "Arg3": "0"}]
        commands = [{"ScenarioGroupID": "tutorial_01", "ID": "intro"},
                    {"ScenarioGroupID": "tutorial_01", "ID": "intro"},
                    {"ScenarioGroupID": "orphan", "ID": "missing"}]
        inventory = {"groups": groups, "commands": commands, "commandsByGroup": {"tutorial_01": [0, 1]},
                     "commandsWithoutGroup": {"orphan": [2]}}
        rows, unassigned = story_rows(inventory, {"branches": []}, {18: "Tutorial"})
        self.assertEqual(rows["tutorial_01"]["source"]["commandRows"], [0, 1])
        self.assertEqual(unassigned, [2])
        inventory["commandsByGroup"]["tutorial_01"] = [0, 0]
        with self.assertRaisesRegex(ValueError, "command row"):
            story_rows(inventory, {"branches": []}, {18: "Tutorial"})

    def test_window_rows_require_one_scene_branch_per_enum_value(self):
        rows = window_rows({"cases": {"20": {"name": "City", "gameObject": 19}}}, {20: "City"})
        self.assertEqual(rows["City"]["source"]["windowViewType"], 20)
        with self.assertRaisesRegex(ValueError, "window enum"):
            window_rows({"cases": {}}, {20: "City"})
        capture = {"case": "City", "sha256": "a" * 64}
        images = {"isolated": {"cases": {"20": capture}}}
        attach_window_references(rows, images)
        self.assertEqual(rows["City"]["evidence"][0]["sha256"], capture["sha256"])
        capture["case"] = "Whale"
        with self.assertRaisesRegex(ValueError, "name disagrees"):
            attach_window_references(rows, images)

    def test_unlocks_keep_price_expiry_and_matching_visual_branch(self):
        catalog = {"environment": {"Aurora": {"policy": "purchase", "cost": 14000, "expires": "20261231235959"}},
                   "decoration": {"Hat": {"policy": "scenario", "scenario": "main_20"}}}
        rows = unlock_rows(catalog, {"main_20"}, {"Aurora"})
        self.assertEqual(rows["environment"]["Aurora"]["preconditions"]["cost"], 14000)
        self.assertEqual(rows["environment"]["Aurora"]["source"]["windowCase"], "Aurora")
        self.assertIsNone(rows["decoration"]["Hat"]["source"]["windowCase"])
        with self.assertRaisesRegex(ValueError, "unknown story group"):
            unlock_rows(catalog, set(), {"Aurora"})

    def test_settings_keep_public_choices_and_exclude_transition_bookkeeping(self):
        source = """
        public class SettingData {
            public ReactiveProperty<GameLanguageType> GameLanguage = new ReactiveProperty<GameLanguageType>();
            public float AmbientBGMVolumeBeforeStory;
            public bool EnablePictureInPicture = true;
            public VolumeInfo GetVolumeInfo(AudioMixerType audioMixerType)
        """
        self.assertEqual(set(settings(source)), {"GameLanguage", "EnablePictureInPicture"})
