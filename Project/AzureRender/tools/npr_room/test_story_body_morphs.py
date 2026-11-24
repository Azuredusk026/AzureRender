from struct import unpack_from
import unittest
from zlib import crc32

from tools.npr_room.story_body_morphs import append_weights, direct_bindings


def binding(name, start, stop):
    return {"path": "Character/Face", "classID": 137, "attribute": "blendShape." + name,
            "curve": {"m_Curve": [
                {"time": 0, "value": start, "inSlope": 0, "outSlope": 0, "weightedMode": 0},
                {"time": 1, "value": stop, "inSlope": 0, "outSlope": 0, "weightedMode": 0}]}}


class StoryBodyMorphs(unittest.TestCase):
    def test_adds_weights_to_body_clip_without_changing_its_clock(self):
        document = {"accessors": [
            {"bufferView": 0, "componentType": 5126, "type": "SCALAR", "count": 4, "min": [0], "max": [1.5]}],
            "bufferViews": [{"buffer": 0, "byteLength": 16}],
            "nodes": [{"mesh": 0}],
            "meshes": [{"extras": {"targetNames": ["A", "B"]}, "weights": [0, .2]}]}
        binary = bytearray.fromhex("000000000000003f0000803f0000c03f")
        body = {"name": "opening", "channels": [{"sampler": 0, "target": {"node": 0, "path": "translation"}}],
                "samplers": [{"input": 0, "output": 4, "interpolation": "LINEAR"}]}
        clip = {"m_Name": "opening", "m_SampleRate": 2, "m_AnimationClipSettings": {"m_StartTime": 0, "m_StopTime": 1},
                "m_FloatCurves": [binding("A", 100, 0)]}
        result = append_weights(document, binary, body, clip, 0)
        self.assertEqual(result, {"bindings": 1, "samples": 4})
        self.assertEqual(body["channels"][1], {"sampler": 1, "target": {"node": 0, "path": "weights"}})
        self.assertEqual(body["channels"][0]["target"]["path"], "translation")
        sampler = body["samplers"][1]
        self.assertEqual(document["accessors"][sampler["input"]]["max"], [1.5])
        weights = document["accessors"][sampler["output"]]
        samples = unpack_from("<8f", binary, document["bufferViews"][weights["bufferView"]]["byteOffset"])
        self.assertEqual(samples[::2], (1, .5, 0, 0))
        self.assertTrue(all(abs(value - .2) < 1e-7 for value in samples[1::2]))

    def test_only_zero_curves_with_a_verified_unbound_target_are_recorded(self):
        name = "path_0x185E3ED8_uwjUpkN"
        curve = binding(name, 0, 0)
        clip = {"m_FloatCurves": [curve], "m_ClipBindingConstant": {"genericBindings": [
            {"typeID": 137, "path": crc32(b"Character/Face"), "attribute": 0x185E3ED8}]}}
        self.assertEqual(direct_bindings(clip, ["A"]), ([], ["185e3ed8"]))
        curve["curve"]["m_Curve"][0]["outSlope"] = .1
        with self.assertRaisesRegex(ValueError, "Unresolved active face binding"):
            direct_bindings(clip, ["A"])


if __name__ == "__main__":
    unittest.main()
