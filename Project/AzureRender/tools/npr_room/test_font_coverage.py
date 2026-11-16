import unittest

from font_coverage import fallback_coverage, glyph_rect_inside, required_codepoints


class FontCoverageTests(unittest.TestCase):
    def test_follows_ordered_fallbacks_by_document_identity_and_terminates_cycles(self):
        root = {"fileID": 1, "guid": "root"}
        fallback = {"fileID": 2, "guid": "fallback"}
        local = {"fileID": 3}
        fonts = {
            ("root", 1): ({65}, [fallback, local]),
            ("fallback", 2): ({65, 1040}, [root]),
            ("root", 3): ({54620}, []),
        }
        result, visited = fallback_coverage(root, fonts.__getitem__)
        self.assertEqual(result, {65: ("root", 1), 1040: ("fallback", 2), 54620: ("root", 3)})
        self.assertEqual(visited, [("root", 1), ("fallback", 2), ("root", 3)])

    def test_preserves_displayed_spaces_combining_marks_and_scalars(self):
        text = "A\r\n\t\u200b\u0301\u00a0\U0001f60a"
        self.assertEqual(required_codepoints([text, "A"]), {0x41, 0x301, 0xa0, 0x1f60a})

    def test_glyph_atlas_bounds_allow_blank_advance_glyphs(self):
        glyph = {"m_AtlasIndex": 0, "m_GlyphRect": {"m_X": 0, "m_Y": 0, "m_Width": 0, "m_Height": 0}}
        self.assertTrue(glyph_rect_inside(glyph, 32, 32, 1))
        glyph["m_GlyphRect"].update(m_X=30, m_Y=30, m_Width=3, m_Height=2)
        self.assertFalse(glyph_rect_inside(glyph, 32, 32, 1))
        glyph["m_GlyphRect"]["m_Width"] = 2
        self.assertTrue(glyph_rect_inside(glyph, 32, 32, 1))
        glyph["m_AtlasIndex"] = 1
        self.assertFalse(glyph_rect_inside(glyph, 32, 32, 1))


if __name__ == "__main__":
    unittest.main()
