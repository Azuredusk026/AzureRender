import copy
from struct import unpack_from
import unittest

from tools.npr_room import original_pose_sequence as sequence
from tools.npr_room.test_original_room_registration import pose


class OriginalPoseSequenceTests(unittest.TestCase):
    def setUp(self):
        self.document = {"nodes": [{"name": "Character", "children": [1]},
                                  {"name": "Face", "mesh": 0, "skin": 0}],
                         "scenes": [{"nodes": [0]}], "skins": [{"joints": [0]}],
                         "meshes": [{"name": "Face", "weights": [0, 0]}],
                         "buffers": [{"uri": "face.glb", "byteLength": 128}],
                         "bufferViews": [{"buffer": 0, "byteOffset": 64, "byteLength": 8}], "accessors": []}
        first = {"weather": "Night", "frame": 100, "simulationTime": 2.0,
                 "transforms": [pose("Character", 10, 0), pose("Character/Face", 20, 10)],
                 "skins": [{"path": "Character/Face", "mesh": "Face", "bones": ["Character"], "blendWeights": [25, 75]}]}
        second = copy.deepcopy(first)
        second.update(frame=104, simulationTime=2.0 + 4 / 60)
        second["transforms"][0]["position"]["z"] = 5
        second["skins"][0]["blendWeights"] = [50, 0]
        self.records = [first, second]

    def test_encodes_mirrored_pose_and_scalar_morph_samples_in_second_buffer(self):
        binary, proof = sequence.append_sequence(self.document, self.records, "face-motion.bin")
        self.assertEqual(self.document["buffers"][0], {"uri": "face.glb", "byteLength": 128})
        self.assertEqual(self.document["bufferViews"][0], {"buffer": 0, "byteOffset": 64, "byteLength": 8})
        self.assertEqual(self.document["buffers"][1], {"uri": "face-motion.bin", "byteLength": len(binary)})
        self.assertEqual(proof["samples"], 2)
        animation, = self.document["animations"]
        self.assertEqual([channel["target"]["path"] for channel in animation["channels"]], ["translation", "weights"])
        for channel in animation["channels"]:
            sampler = animation["samplers"][channel["sampler"]]
            accessor = self.document["accessors"][sampler["output"]]
            view = self.document["bufferViews"][accessor["bufferView"]]
            self.assertEqual(view["buffer"], 1)
            if channel["target"]["path"] == "weights":
                self.assertEqual((accessor["type"], accessor["count"]), ("SCALAR", 4))
                self.assertEqual(unpack_from("<4f", binary, view["byteOffset"]), (.25, .75, .5, 0))
            else:
                self.assertEqual(unpack_from("<6f", binary, view["byteOffset"]), (1, 2, -3, 1, 2, -5))

    def test_rejects_time_weather_rig_and_palette_changes_transactionally(self):
        variants = []
        for key, value in (("simulationTime", 2), ("weather", "Cloudy"), ("frame", 99)):
            records = copy.deepcopy(self.records)
            records[1][key] = value
            variants.append(records)
        for field, value in (("bones", ["Character/Face"]), ("blendWeights", [0]),
                             ("blendWeights", [0, float("nan")])):
            records = copy.deepcopy(self.records)
            records[1]["skins"][0][field] = value
            variants.append(records)
        records = copy.deepcopy(self.records)
        records[1]["transforms"][1]["parentId"] = 0
        variants.append(records)
        for records in variants:
            with self.subTest(records=records):
                original = copy.deepcopy(self.document)
                with self.assertRaises(ValueError):
                    sequence.append_sequence(self.document, records, "face-motion.bin")
                self.assertEqual(self.document, original)

    def test_supports_morph_only_and_constant_captured_poses(self):
        for morph_only in (True, False):
            with self.subTest(morph_only=morph_only):
                document = copy.deepcopy(self.document)
                records = copy.deepcopy(self.records)
                records[1]["transforms"] = copy.deepcopy(records[0]["transforms"])
                if not morph_only:
                    records[1]["skins"] = copy.deepcopy(records[0]["skins"])
                binary, proof = sequence.append_sequence(document, records, "motion.bin")
                animation, = document["animations"]
                self.assertEqual(proof["transformChannels"], 0)
                self.assertEqual(len(animation["channels"]), 1)
                self.assertEqual(animation["channels"][0]["target"]["path"], "weights" if morph_only else "translation")
                self.assertTrue(binary)


if __name__ == "__main__":
    unittest.main()
