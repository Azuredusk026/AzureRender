import unittest

from alpha_atlas import unpack_alpha8


class AlphaAtlasTests(unittest.TestCase):
    def test_upload_alpha_channel_uses_top_down_image_rows(self):
        data = {
            "m_TextureFormat": 1, "m_TextureDimension": 2, "m_MipCount": 1,
            "m_Width": 2, "m_Height": 2, "m_CompleteImageSize": 4,
            "image data": 4, "_typelessdata": "004080ff", "m_StreamData": {"size": 0, "path": ""},
        }
        output, source_pixels = unpack_alpha8(data)
        self.assertEqual(source_pixels, bytes((0, 64, 128, 255)))
        self.addCleanup(output.close)
        self.assertEqual(output.getchannel("A").tobytes(), bytes((128, 255, 0, 64)))
        self.assertEqual(output.getpixel((0, 0)), (255, 255, 255, 128))
        with self.assertRaisesRegex(ValueError, "Alpha8 payload size"):
            unpack_alpha8({**data, "image data": 0})
        with self.assertRaisesRegex(ValueError, "single-mip Alpha8"):
            unpack_alpha8({**data, "m_MipCount": 2})


if __name__ == "__main__":
    unittest.main()
