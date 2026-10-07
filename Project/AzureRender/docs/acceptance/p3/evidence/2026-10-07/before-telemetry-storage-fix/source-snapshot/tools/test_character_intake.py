"""Check real glTF intake failures before renderer loading."""
from pathlib import Path
import json
import tempfile
import unittest
import character_intake


class IntakeTests(unittest.TestCase):
    def test_invalid_skin_joint_is_rejected(self):
        document = {"asset": {"version": "2.0"}, "nodes": [{"name": "root"}],
            "skins": [{"joints": [9]}]}
        with self.assertRaisesRegex(ValueError, "joint"):
            character_intake.audit_gltf(document)

    def test_records_skeleton_and_complexity(self):
        result = character_intake.audit_gltf({"asset": {"version": "2.0"},
            "nodes": [{"name": "root"}, {"name": "hip"}], "skins": [{"joints": [0, 1]}],
            "accessors": [{"count": 24}, {"count": 36}],
            "meshes": [{"primitives": [{"attributes": {"POSITION": 0}, "indices": 1}]}]})
        self.assertEqual(result["jointCount"], 2)
        self.assertEqual(result["vertices"], 24)
        self.assertEqual(result["triangles"], 12)
        self.assertEqual(result["skeleton"][0], ["root", "hip"])


if __name__ == "__main__":
    unittest.main()
