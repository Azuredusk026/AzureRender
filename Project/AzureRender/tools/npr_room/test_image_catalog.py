from pathlib import Path
import tempfile
import unittest

from image_catalog import inspect_image
from tools.build_face_sdf import encode_png


class ImageCatalogTests(unittest.TestCase):
    def test_checks_real_png_and_keeps_dimensions(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "icon.png"
            png = encode_png(2, 3, bytes([255, 127, 0, 255]) * 6)
            path.write_bytes(png)
            self.assertEqual(inspect_image(path), {"width": 2, "height": 3, "mode": "RGBA"})
            path.write_bytes(png[:48])
            with self.assertRaises((OSError, SyntaxError, ValueError)):
                inspect_image(path)


if __name__ == "__main__":
    unittest.main()
