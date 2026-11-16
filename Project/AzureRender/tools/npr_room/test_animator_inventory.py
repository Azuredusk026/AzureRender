import unittest

from animator_inventory import index_controller


class AnimatorInventoryTests(unittest.TestCase):
    def sample(self):
        return {
            1: (91, {
                "m_Name": "source",
                "m_AnimatorParameters": [{"m_Name": "Next", "m_Type": 9}],
                "m_AnimatorLayers": [{"m_Name": "Base", "m_StateMachine": {"fileID": 2}, "m_BlendingMode": 0}],
            }),
            2: (1107, {"m_Name": "Root", "m_ChildStates": [{"m_State": {"fileID": 3}}, {"m_State": {"fileID": 4}}],
                       "m_DefaultState": {"fileID": 3}, "m_AnyStateTransitions": [{"fileID": 6}, {"fileID": 5}]}),
            3: (1102, {"m_Name": "Idle", "m_Motion": {"fileID": 7400000, "guid": "clip"}, "m_Transitions": []}),
            4: (1102, {"m_Name": "Idle", "m_Motion": {"fileID": 0}, "m_Transitions": []}),
            5: (1101, {"m_Name": "", "m_DstState": {"fileID": 4}, "m_Conditions": []}),
            6: (1101, {"m_Name": "", "m_DstState": {"fileID": 3},
                       "m_Conditions": [{"m_ConditionEvent": "Next", "m_ConditionMode": 1}]}),
        }

    def test_preserves_state_identity_transition_order_and_source_motion(self):
        result = index_controller(self.sample(), {"clip": ["Assets/AnimationClip/idle.anim"]})
        self.assertEqual(result["states"]["3"]["m_Name"], result["states"]["4"]["m_Name"])
        self.assertEqual(result["machines"]["2"]["m_AnyStateTransitions"], [{"fileID": 6}, {"fileID": 5}])
        self.assertEqual(result["externalAssets"]["clip"], ["Assets/AnimationClip/idle.anim"])
        self.assertEqual(result["summary"]["states"], 2)

    def test_reports_unresolved_local_references(self):
        data = self.sample()
        data[6][1]["m_DstState"]["fileID"] = 7
        result = index_controller(data, {})
        self.assertEqual(result["unresolvedLocal"], [{"owner": 6, "field": "m_DstState", "fileID": 7}])
        self.assertEqual(result["externalAssets"]["clip"], [])


if __name__ == "__main__":
    unittest.main()
