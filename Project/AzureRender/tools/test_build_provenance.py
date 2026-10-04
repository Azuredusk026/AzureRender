"""Reject stale source and altered build products."""
import json
from pathlib import Path
import tempfile
import unittest
import build_provenance


class ProvenanceTests(unittest.TestCase):
    def test_rejects_changed_source_and_binary(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "src").mkdir()
            source = root / "src/main.cpp"
            source.write_text("int main() { return 0; }")
            binary = root / "player.exe"
            binary.write_bytes(b"original product")
            manifest = build_provenance.describe(root, [binary])
            build_provenance.verify(root, manifest)
            source.write_text("int main() { return 1; }")
            with self.assertRaisesRegex(ValueError, "source"):
                build_provenance.verify(root, manifest)
            source.write_text("int main() { return 0; }")
            binary.write_bytes(b"altered product")
            with self.assertRaisesRegex(ValueError, "product"):
                build_provenance.verify(root, manifest)

    def test_missing_binary_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            with self.assertRaises(FileNotFoundError):
                build_provenance.describe(Path(temp), [Path(temp) / "missing.exe"])


if __name__ == "__main__":
    unittest.main()
