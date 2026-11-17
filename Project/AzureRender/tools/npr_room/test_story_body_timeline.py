import unittest

from tools.npr_room.story_body_timeline import compile_timeline


class StoryBodyTimelineTests(unittest.TestCase):
    def test_converts_only_direct_unconditional_authored_transitions(self):
        graph = {"schemaVersion": 1, "states": [{"name": "story-Stand", "clip": 2},
                                                {"name": "story-Read", "clip": 3}]}
        source = {"m_DstState": {"fileID": 17}, "m_DstStateMachine": {"fileID": 0},
                  "m_Conditions": [], "m_HasExitTime": 1, "m_ExitTime": .75, "m_IsExit": 0,
                  "m_TransitionDuration": .25, "m_TransitionOffset": 1.125,
                  "m_HasFixedDuration": 1, "m_InterruptionSource": 0, "m_Mute": 0, "m_Solo": 0}
        stand = {"state": "Stand", "clip": "Clips/stand.anim", "variants": [
            {"id": "16", "outgoing": [source, {**source, "m_Conditions": [{}]}]}]}
        read = {"state": "Read", "clip": "Clips/read.anim", "variants": [
            {"id": "17", "outgoing": [{**source, "m_DstState": {"fileID": 0}, "m_IsExit": 1}]}]}
        catalog = {"motions": {"0": {"states": [stand, read]}}}
        receipt = {"motions": {"stand": {"seconds": 2, "clipIndex": 2},
                               "read": {"seconds": 4, "clipIndex": 3}}}
        built = compile_timeline(graph, catalog, receipt)
        self.assertEqual(graph["schemaVersion"], 1)
        self.assertEqual(built["schemaVersion"], 3)
        self.assertEqual(built["timeline"], [{"from": "story-Stand", "to": "story-Read", "afterSeconds": 1.5,
                                             "blendSeconds": .25, "startSeconds": 4.5}])
        source["m_HasFixedDuration"] = 0
        with self.assertRaisesRegex(ValueError, "fixed"):
            compile_timeline(graph, catalog, receipt)
