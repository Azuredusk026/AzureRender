import unittest

from window_scene_inventory import inventory


class FixtureScene:
    def __init__(self):
        self.serialized = "guid: service\n"
        self.objects = {
            10: (114, {"m_Script": {"guid": "service"}, "_day": {"fileID": 20}, "_city": {"fileID": 50}}),
            20: (1, {"m_Name": "day", "m_IsActive": 0, "m_Component": [{"component": {"fileID": 21}}]}),
            21: (4, {"m_GameObject": {"fileID": 20}, "m_Father": {"fileID": 0}, "m_Children": [{"fileID": 31}]}),
            30: (1, {"m_Name": "cloud", "m_IsActive": 1,
                     "m_Component": [{"component": {"fileID": i}} for i in (31, 32, 33)]}),
            31: (4, {"m_GameObject": {"fileID": 30}, "m_Father": {"fileID": 21}, "m_Children": []}),
            32: (33, {"m_Mesh": {"guid": "mesh", "fileID": 4300000}}),
            33: (23, {"m_Enabled": 1, "m_Materials": [{"guid": "cloud", "fileID": 2100000}]}),
            50: (1, {"m_Name": "city", "m_IsActive": 1, "m_Component": [{"component": {"fileID": 51}}]}),
            51: (4, {"m_GameObject": {"fileID": 50}, "m_Father": {"fileID": 0}, "m_Children": []}),
        }
        self.documents = {i: (kind, 0, len(self.serialized)) for i, (kind, _) in self.objects.items()}

    def read(self, identity):
        return self.objects[identity][1]


class WindowSceneTests(unittest.TestCase):
    def test_resolves_inactive_root_and_active_renderer_without_enabling_a_child(self):
        report = inventory(FixtureScene(), "service", {11: "Day", 20: "City"})
        self.assertEqual(report["serviceComponent"], 10)
        day = report["cases"][11]
        self.assertFalse(day["initialActive"])
        self.assertEqual(day["transform"], 21)
        self.assertEqual(day["renderers"], [
            {"component": 33, "transform": 31, "name": "cloud", "kind": "mesh",
             "activeWhenSelected": True, "enabled": True, "materials": [{"guid": "cloud", "fileID": 2100000}],
             "mesh": {"guid": "mesh", "fileID": 4300000}},
        ])
        self.assertEqual(report["cases"][20]["renderers"], [])

    def test_requires_a_scene_root_for_each_enum_case(self):
        with self.assertRaisesRegex(ValueError, "Missing window scene field.*Sunset"):
            inventory(FixtureScene(), "service", {12: "Sunset"})
