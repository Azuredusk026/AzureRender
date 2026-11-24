from pathlib import Path
import tempfile
import unittest

from PIL import Image

from window_references import capture, isolated_captures, source_cases


class WindowReferenceTests(unittest.TestCase):
    def test_requires_numeric_identity_and_matching_filename(self):
        self.assertEqual(source_cases("enum WindowViewType : byte { Day = 11, Sunset = 12 }"),
                         {11: "Day", 12: "Sunset"})
        with self.assertRaisesRegex(ValueError, "Duplicate window"):
            source_cases("enum WindowViewType : byte { Day = 11, Sunset = 11 }")
        with tempfile.TemporaryDirectory() as path:
            folder = Path(path)
            with Image.new("RGB", (8, 4), (100, 20, 10)) as sample:
                sample.save(folder / "11-Day.png")
            with self.assertRaisesRegex(ValueError, "Missing reference captures"):
                isolated_captures(folder, {11: "Day", 12: "Sunset"})
            with Image.new("RGB", (8, 4), (35, 20, 10)) as sample:
                sample.save(folder / "12-Sunset.png")
            report = isolated_captures(folder, {11: "Day", 12: "Sunset"})
            self.assertEqual(report[11]["size"], [8, 4])
            self.assertNotEqual(report[11]["sha256"], report[12]["sha256"])

    def test_reads_original_capture_encoding_from_the_file_bytes(self):
        with tempfile.TemporaryDirectory() as path:
            sample = Path(path) / "original.png"
            with Image.new("RGB", (8, 4), (100, 20, 10)) as picture:
                picture.save(sample, "JPEG")
            self.assertEqual(capture(sample, ("PNG", "JPEG"))["format"], "JPEG")
            with self.assertRaisesRegex(ValueError, "Unexpected capture format JPEG"):
                capture(sample)


if __name__ == "__main__":
    unittest.main()
