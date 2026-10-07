"""Verify source preservation and explicit material corrections."""
import json
from pathlib import Path
import struct
import tempfile
import unittest
from prepare_character import prepare


class PreparationTests(unittest.TestCase):
    def test_changes_brow_material_and_preserves_geometry(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source, output = root / "source.glb", root / "output.glb"
            document = {"asset": {"version": "2.0"}, "materials": [
                {"name": "brow", "extras": {"azureRenderMaterial": {"features": ["brow-overlay"]}}},
                {"name": "eye", "doubleSided": False}], "meshes": [{"name": "unchanged"}]}
            payload = json.dumps(document).encode(); payload += b" " * (-len(payload) % 4)
            tail = struct.pack("<II", 4, 0x004e4942) + b"mesh"
            data = struct.pack("<III", 0x46546c67, 2, 20 + len(payload) + len(tail)) + struct.pack("<II", len(payload), 0x4e4f534a) + payload + tail
            source.write_bytes(data)
            prepare(source, output, brow_double_sided=True)
            result = output.read_bytes(); length = struct.unpack_from("<I", result, 12)[0]
            changed = json.loads(result[20:20 + length])
            self.assertTrue(changed["materials"][0]["doubleSided"])
            self.assertFalse(changed["materials"][1]["doubleSided"])
            self.assertEqual(changed["meshes"], [{"name": "unchanged"}])
            self.assertEqual(result[20 + length:], tail)
            self.assertEqual(source.read_bytes(), data)
            with self.assertRaises(FileExistsError):
                prepare(source, output, brow_double_sided=True)
            with self.assertRaises(ValueError):
                prepare(source, source, brow_double_sided=True)


if __name__ == "__main__":
    unittest.main()
