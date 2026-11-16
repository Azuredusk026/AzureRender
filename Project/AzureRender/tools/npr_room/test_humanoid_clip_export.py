import unittest
from struct import unpack_from

from humanoid_clip_export import append_humanoid_clip, bind_source_nodes


def vector(*values):
    return dict(zip("xyzw", values))


class HumanoidClipExportTests(unittest.TestCase):
    def setUp(self):
        self.rig = [{"fileId": 1, "parent": -1, "name": "Character"},
                    {"fileId": 6, "parent": 0, "name": "Hips"},
                    {"fileId": 9, "parent": 0, "name": "Spare"}]
        self.gltf = {"nodes": [{"name": "Character", "children": [1]}, {"name": "Hips"}],
                     "scene": 0, "scenes": [{"nodes": [0]}], "accessors": [], "bufferViews": []}

    def test_matches_pruned_rig_by_parent_hierarchy(self):
        self.assertEqual(bind_source_nodes(self.gltf, self.rig), {0: 0, 1: 1})
        self.rig.append({"fileId": 10, "parent": 0, "name": "Hips"})
        with self.assertRaisesRegex(ValueError, "Ambiguous"):
            bind_source_nodes(self.gltf, self.rig)

    def test_mirrors_samples_preserves_quaternion_continuity_and_skips_bind_tracks(self):
        mapping = bind_source_nodes(self.gltf, self.rig)
        unchanged = vector(0, 0, 0)
        root_rotation = vector(0, 0, 0, 1)
        scale = [vector(1, 1, 1)] * 3
        clip = {"name": "report", "frames": [
            {"time": 0, "positions": [unchanged, vector(0, 1, 1), unchanged],
             "rotations": [root_rotation, vector(.6, 0, 0, .8), root_rotation], "scales": scale},
            {"time": 1, "positions": [unchanged, vector(0, 2, 4), unchanged],
             "rotations": [root_rotation, vector(-.8, 0, 0, -.6), root_rotation], "scales": scale},
        ]}
        binary = bytearray()
        append_humanoid_clip(self.gltf, binary, mapping, clip)
        channels = self.gltf["animations"][0]["channels"]
        self.assertEqual([c["target"] for c in channels],
                         [{"node": 1, "path": "translation"}, {"node": 1, "path": "rotation"}])
        position = self.gltf["bufferViews"][1]["byteOffset"]
        rotation = self.gltf["bufferViews"][2]["byteOffset"]
        self.assertEqual(unpack_from("<6f", binary, position), (0, 1, -1, 0, 2, -4))
        self.assertAlmostEqual(unpack_from("<8f", binary, rotation)[4], -.8)


if __name__ == "__main__":
    unittest.main()
