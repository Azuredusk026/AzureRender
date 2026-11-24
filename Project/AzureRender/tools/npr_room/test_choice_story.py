"""Check source option identity and the independent end of every conversation branch."""

import unittest

from tools.npr_room.choice_story import choice_episodes


class ChoiceStoryTests(unittest.TestCase):
    def test_options_target_the_original_first_match_in_the_same_episode(self):
        commands = [{"Command": "0", "ID": "intro"}]
        commands.extend({"Command": "3", "ID": f"option-{i}", "Arg1": f"reaction-{i}",
                         "LocalizationID": f"choice-{i}"} for i in range(3))
        for i in range(3):
            commands.extend(({"Command": "0", "ID": f"reaction-{i}"},
                             {"Command": "12", "ID": f"finish-{i}"}))
        story = {"groups": [{"ID": "first", "Scenario": "7", "EpisodeNumber": "1"}],
                 "commands": commands, "commandsByGroup": {"first": list(range(10))}}
        episodes = choice_episodes(story)
        self.assertEqual(episodes[0]["intro"], 0)
        self.assertEqual(episodes[0]["options"],
                         [{"labelRow": 1, "spokenRow": 4, "endRow": 5},
                          {"labelRow": 2, "spokenRow": 6, "endRow": 7},
                          {"labelRow": 3, "spokenRow": 8, "endRow": 9}])
        commands[7]["Command"] = "0"
        with self.assertRaisesRegex(ValueError, "terminal"):
            choice_episodes(story)


if __name__ == "__main__":
    unittest.main()
