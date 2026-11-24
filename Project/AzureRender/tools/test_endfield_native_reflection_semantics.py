import math
import struct
import unittest

from endfield_native.reflection_semantics import pack_runtime_probe, sort_runtime_probes


def payload(rank=3, key=20):
    data = bytearray(0x130)
    data[0] = 6
    struct.pack_into('<i', data, 4, rank)
    struct.pack_into('<4f', data, 8, 0.5, 0.75, 1, 0.7)
    struct.pack_into('<3f', data, 0x18, 2, 4, 8)
    struct.pack_into('<16f', data, 0x24, *range(16))
    struct.pack_into('<16f', data, 0x64, *range(16, 32))
    struct.pack_into('<27f', data, 0xa4, *range(27))
    struct.pack_into('<Q', data, 0x110, key)
    return bytes(data)


class ReflectionSemanticsTests(unittest.TestCase):
    def test_channel_major_sh_and_axis_order(self):
        rows = pack_runtime_probe(payload(), 7, 0.25)
        for output, coefficient in zip(rows[0], (3, 1, 2, 0)):
            expected = (coefficient * 0.2126729041337967
                        + (coefficient + 9) * 0.7151522040367126
                        + (coefficient + 18) * 0.07217500358819962)
            self.assertAlmostEqual(output, expected)

    def test_matrix_columns_flags_fade_and_slot_order(self):
        rows = pack_runtime_probe(payload(), 7, 0.25)
        self.assertEqual(len(rows), 8)
        self.assertEqual(rows[1], (2, 4, 8, 7))
        self.assertEqual(rows[2:5], [(16, 20, 24, 28), (17, 21, 25, 29), (18, 22, 26, 30)])
        self.assertEqual(rows[6], (1, 0, 3, 0.25))
        self.assertEqual(rows[7][:3], (12, 13, 14))
        self.assertAlmostEqual(rows[7][3], math.sqrt(84))
        self.assertAlmostEqual(rows[5][0], 1)
        self.assertAlmostEqual(rows[5][1], 1)
        self.assertAlmostEqual(rows[5][2], 1 / (8 * (1 - struct.unpack('<f', bytes.fromhex('a4707d3f'))[0])))

    def test_signed_rank_descending_unsigned_key_ascending(self):
        values = [payload(-1, 0), payload(2, 2**64-1), payload(2, 2), payload(5, 9)]
        self.assertEqual(sort_runtime_probes(values), [values[3], values[2], values[1], values[0]])

    def test_invalid_payload_and_unusable_runtime_state(self):
        for source, layer, fade in ((b'', 1, 1), (payload(), 0, 1), (payload(), 1, math.nan)):
            with self.assertRaises(ValueError):
                pack_runtime_probe(source, layer, fade)
        for offset, value in ((0x18, 0), (0xa4, math.inf)):
            data = bytearray(payload())
            struct.pack_into('<f', data, offset, value)
            with self.assertRaises(ValueError):
                pack_runtime_probe(data, 1, 1)


if __name__ == '__main__':
    unittest.main()
