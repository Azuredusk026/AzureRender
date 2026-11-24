"""Verify the Recovery reader's group-local row and option resolution."""

import unittest

from tools.npr_room.story_routes import branch_inventory, command_names


class StoryRouteTests(unittest.TestCase):
    def test_first_group_local_match_preserves_duplicate_row_positions(self):
        names = command_names("""
        public enum CommandType {
            Text = 0,
            Selection = 3,
            Jump = 4,
            IfBetweenDateTime = 70,
            StartTextLoop = 81
        }""")
        commands = [
            {"ID": "target", "Command": "0"},
            {"ID": "loop", "Command": "81", "Arg3": "option"},
            {"ID": "option", "Command": "3", "Arg1": "target"},
            {"ID": "another-option", "Command": "3", "Arg1": "branch"},
            {"ID": "target", "Command": "0"},
            {"ID": "date", "Command": "70", "Arg1": "20260214000000~20260214235959",
             "Arg3": "target", "Arg4": "external-only"},
            {"ID": "branch", "Command": "4", "Arg1": "target"},
            {"ID": "external-only", "Command": "0"},
        ]
        result = branch_inventory({"commands": commands, "commandsByGroup": {"A": list(range(7)), "B": [7]}}, names)
        branches = {row["row"]: row for row in result["branches"]}
        self.assertEqual(branches[1]["options"], [2, 3])
        self.assertEqual(branches[2]["targets"], [{"id": "target", "row": 0, "otherRows": [4]}])
        self.assertEqual(branches[5]["targets"][1], {"id": "external-only", "row": None, "otherRows": []})
        self.assertEqual(branches[6]["targets"][0]["row"], 0)
        self.assertEqual(result["summary"]["unresolvedTargets"], 1)

    def test_rejects_an_unmapped_serialized_command(self):
        with self.assertRaisesRegex(ValueError, "command 999"):
            branch_inventory({"commands": [{"ID": "x", "Command": "999"}], "commandsByGroup": {"A": [0]}}, {0: "Text"})


if __name__ == "__main__":
    unittest.main()
