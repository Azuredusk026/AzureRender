import copy
from pathlib import Path
from struct import unpack_from
import tempfile
import unittest

from tools.build_face_sdf import write_glb
from tools.npr_room import glb_reference


class GlbReferenceTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.source = self.root / "source-model.glb"
        self.document = {"asset": {"version": "2.0"}, "buffers": [{"byteLength": 8}],
                         "bufferViews": [{"buffer": 0, "byteOffset": 4, "byteLength": 4}],
                         "nodes": [{"name": "Root"}], "scenes": [{"nodes": [0]}]}
        write_glb(self.source, self.document, b"abcdefgh")

    def test_external_view_reads_identical_payload_and_preserves_source(self):
        original = self.source.read_bytes()
        document = glb_reference.reference_document(self.source)
        self.assertEqual(document["buffers"], [{"uri": "source-model.glb", "byteLength": len(original)}])
        view = document["bufferViews"][0]
        self.assertEqual(original[view["byteOffset"]:view["byteOffset"] + view["byteLength"]], b"efgh")
        self.assertEqual(view["byteOffset"], 28 + unpack_from("<I", original, 12)[0] + 4)
        document["nodes"][0]["translation"] = [1, 2, 3]
        self.assertEqual(self.source.read_bytes(), original)

    def test_rejects_external_storage_and_out_of_bounds_views(self):
        for changes in ({"buffers": [{"uri": "other.bin", "byteLength": 8}]},
                        {"bufferViews": [{"buffer": 0, "byteOffset": 8, "byteLength": 4}]},
                        {"images": [{"uri": "image.png"}]}):
            with self.subTest(changes=changes):
                document = copy.deepcopy(self.document)
                document.update(changes)
                write_glb(self.source, document, b"abcdefgh")
                with self.assertRaises(ValueError):
                    glb_reference.reference_document(self.source)

    def test_rejects_truncation_and_preserves_input(self):
        raw = self.source.read_bytes()[:-1]
        self.source.write_bytes(raw)
        with self.assertRaises(ValueError):
            glb_reference.reference_document(self.source)
        self.assertEqual(self.source.read_bytes(), raw)

    def test_rejects_names_requiring_uri_decoding_by_asset_database(self):
        renamed = self.source.with_name("source model.glb")
        self.source.rename(renamed)
        with self.assertRaisesRegex(ValueError, "URI-safe"):
            glb_reference.reference_document(renamed)


if __name__ == "__main__":
    unittest.main()
