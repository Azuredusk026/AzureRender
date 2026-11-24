"""Check proof of the authored glitch camera's drawable coverage."""

import unittest

from tools.npr_room.opening_glitch_camera import layer_rows


class OpeningGlitchLayerTests(unittest.TestCase):
    def test_records_membership_and_preserves_exclusions(self):
        bindings = {"a": (7106, "transform"), "b": (9476, "renderer")}
        objects = {7106: {"m_GameObject": {"fileID": 1}}, 9476: {"m_GameObject": {"fileID": 2}},
                   1: {"m_Name": "Desk", "m_Layer": 0}, 2: {"m_Name": "Heroine", "m_Layer": 4}}
        rows = layer_rows(bindings, objects.__getitem__, 1 << 4)
        self.assertEqual([(row["node"], row["sourceId"], row["layer"], row["included"]) for row in rows],
                         [("a", 7106, 0, False), ("b", 9476, 4, True)])
