import json
from pathlib import Path
import tempfile
import unittest

from tools.npr_room import original_program_globals
from tools.npr_room.original_room_frames import verify_frames


class OriginalProgramGlobalsTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.path = Path(self.temporary.name) / "programs.json"
        self.document = {"programs": {"sky": {"stages": {"fragment": {"constantBuffers": [{
            "name": "$Globals", "vectors": [
                {"name": "_TimeParameters", "dimension": 4, "array": 0, "type": 0},
                {"name": "CZY_SunSize", "dimension": 1, "array": 0, "type": 0},
                {"name": "_WorldSpaceCameraPos", "dimension": 3, "array": 0, "type": 0}]}]}}}}}

    def bindings(self):
        self.path.write_text(json.dumps(self.document), encoding="utf-8")
        return original_program_globals.make_bindings(self.path)

    def test_derives_typed_fields_and_rejects_conflicting_declarations(self):
        definition = self.bindings()
        self.assertEqual(definition["globals"], [{"name": "CZY_SunSize", "dimension": 1},
                                                 {"name": "_TimeParameters", "dimension": 4}])
        duplicate = {"name": "CZY_SunSize", "dimension": 2, "array": 0, "type": 0}
        self.document["programs"]["sky"]["stages"]["fragment"]["constantBuffers"][0]["vectors"].append(duplicate)
        with self.assertRaises(ValueError):
            self.bindings()

    def test_metadata_requires_exact_bindings_finite_values_and_source_provenance(self):
        definition = self.bindings()
        record = {"metadataOnly": True, "simulationTime": 2,
                  "programManifestSHA256": definition["programSHA256"],
                  "programGlobals": [{"name": "CZY_SunSize", "dimension": 1,
                                      "value": {"x": .2, "y": 0, "z": 0, "w": 0}},
                                     {"name": "_TimeParameters", "dimension": 4,
                                      "value": {"x": 2, "y": 0, "z": 0, "w": 0}}]}
        proof = original_program_globals.verify_metadata(record, definition)
        self.assertEqual(proof["globals"], 2)
        record["metadataOnly"] = False
        self.assertEqual(original_program_globals.verify_metadata(record, definition), proof)
        record["programGlobals"][0]["value"]["x"] = float("nan")
        with self.assertRaises(ValueError):
            original_program_globals.verify_metadata(record, definition)
        record["programGlobals"][0]["value"]["x"] = .2
        record["programManifestSHA256"] = "0" * 64
        with self.assertRaises(ValueError):
            original_program_globals.verify_metadata(record, definition)

    def test_metadata_diagnostic_requires_paired_images_for_full_reference_verification(self):
        folder = Path(self.temporary.name) / "frames"
        folder.mkdir()
        for mode in ["Night", "Cloudy"]:
            for index in range(12):
                (folder / f"{mode}-{index:02d}.json").write_text('{"metadataOnly":true}', encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "same-frame color and depth"):
            verify_frames(folder)

    def test_projects_program_fields_with_the_captured_camera_position(self):
        definition = self.bindings()
        record = {"programManifestSHA256": definition["programSHA256"], "simulationTime": 2,
                  "position": {"x": 1, "y": 2, "z": 3},
                  "programGlobals": [{"name": "CZY_SunSize", "dimension": 1,
                                      "value": {"x": .2, "y": 0, "z": 0, "w": 0}},
                                     {"name": "_TimeParameters", "dimension": 4,
                                      "value": {"x": 2, "y": .3, "z": .4, "w": 0}}]}
        fields = original_program_globals.captured_program_fields(record, self.path, "sky")
        self.assertEqual(fields, {"CZY_SunSize": (.2,), "_TimeParameters": (2, .3, .4, 0),
                                  "_WorldSpaceCameraPos": (1, 2, 3)})
        with self.assertRaises(ValueError):
            original_program_globals.captured_program_fields(record, self.path, "unknown")
