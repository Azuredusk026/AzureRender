import struct
import unittest
import zlib

from endfield_native.compat import normalize_legacy_catalog


def catalog():
    name = b"AuditIV"
    body = struct.pack("<I", 5157154) + struct.pack("<H", len(name)) + name
    body += bytes.fromhex("06223fe2ffffffff")
    body += struct.pack("<iqBi", 0, 0, 18, 0)
    body += struct.pack("<IqH", 3, 1769209085, 0)
    return struct.pack("<Iq", 3, 17) + body + struct.pack("<I", zlib.crc32(body))


class LegacyCatalogTests(unittest.TestCase):
    def test_preserves_group_and_reports_revision_and_footer(self):
        raw = catalog()
        normalized, metadata = normalize_legacy_catalog(raw)
        self.assertEqual(normalized, raw[:12] + raw[16:-18])
        self.assertEqual(metadata["revision"], 5157154)
        self.assertEqual(metadata["footer_version"], 3)
        self.assertTrue(metadata["crc_verified"])

    def test_corruption_is_rejected(self):
        raw = bytearray(catalog())
        raw[22] ^= 1
        with self.assertRaisesRegex(ValueError, "CRC"):
            normalize_legacy_catalog(bytes(raw))

    def test_truncated_catalog_is_rejected(self):
        with self.assertRaises(ValueError):
            normalize_legacy_catalog(catalog()[:20])

    def test_unknown_protocol_is_rejected(self):
        raw = bytearray(catalog())
        struct.pack_into("<I", raw, 0, 99)
        with self.assertRaisesRegex(ValueError, "protocol"):
            normalize_legacy_catalog(bytes(raw))


if __name__ == "__main__":
    unittest.main()
