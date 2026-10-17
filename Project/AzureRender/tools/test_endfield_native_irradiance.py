import struct
import unittest

from endfield_native.irradiance import decode_amplitude, parse_sparse_tile, parse_index


def sample(value=1.0):
    half = struct.unpack('<H', struct.pack('<e', value))[0]
    word = (half >> 4) | ((half >> 4) << 11) | ((half >> 5) << 22)
    return struct.pack('<I', word) + bytes(range(12))


def tile():
    # One valid brick at (0, 0, 0), samples 0 and 63.
    return struct.pack('<3I2IQ', 1, 1, 0, 0, 28, (1 << 63) | 1) + sample() + sample(2)


def index():
    def string(text):
        data = text.encode('utf-16le')
        return struct.pack('<I', len(data)) + data
    return (struct.pack('<2I', 0x03000002, 1) + string('variant')
            + struct.pack('<5I', 2, 0, 0, 123, 1) + struct.pack('<2I', 0, 2)
            + string('iv_0_0.bytes') + string('iv_123_0.bytes')
            + bytes(116) + struct.pack('<2I', 0, 2)
            + struct.pack('<8I', 0x00404040, 0, 0, 60, 28, 32, 0, 0xffffffff)
            + struct.pack('<8I', 0x00404040, 0, 0, 60, 28, 32, 1, 0xffffffff)
            + struct.pack('<I', 0))


class IrradianceTests(unittest.TestCase):
    def test_packed_hdr(self):
        self.assertEqual(decode_amplitude(sample(2)), [2., 2., 2.])
        self.assertEqual(decode_amplitude(sample(0)), [0., 0., 0.])

    def test_sparse_rank_uses_occupancy(self):
        parsed = parse_sparse_tile(tile(), 28)
        self.assertEqual(parsed.sample((0, 0, 0), 63)['amplitude'], [2., 2., 2.])
        self.assertEqual(parsed.sample((0, 0, 0), 63)['byte_offset'], 44)
        self.assertIsNone(parsed.sample((0, 0, 0), 1))
        self.assertIsNone(parsed.sample((1, 0, 0), 0))

    def test_corrupt_hash_and_offsets(self):
        for offset, value in ((12, 0x01000000), (16, 24)):
            broken = bytearray(tile())
            struct.pack_into('<I', broken, offset, value)
            with self.assertRaises(ValueError):
                parse_sparse_tile(bytes(broken), 28)

    def test_hash_displacement_matches_stored_slot(self):
        records = [struct.pack('<IIQ', 0xffffffff, 0, 0)] * 8
        records[1] = struct.pack('<IIQ', 1, 140, 1)
        data = struct.pack('<3I', 2, 1, 0) + b''.join(records) + sample()
        self.assertEqual(parse_sparse_tile(data, 140).sample((1, 0, 0), 0)['amplitude'], [1., 1., 1.])
        broken = bytearray(data)
        struct.pack_into('<I', broken, 8, 1)
        with self.assertRaisesRegex(ValueError, 'spatial hash mismatch'):
            parse_sparse_tile(bytes(broken), 140)

    def test_truncation_and_trailing_payload(self):
        for data in (tile()[:-1], tile() + b'\0', b'', struct.pack('<2I', 0xffffffff, 2)):
            with self.assertRaises(ValueError):
                parse_sparse_tile(data, 28)

    def test_index_keeps_scenarios_distinct(self):
        result = parse_index(index())
        self.assertEqual(result['scenario_names'], ['variant'])
        self.assertEqual([row['file_index'] for row in result['tiles']], [0, 1])
        self.assertEqual(result['branches'], [[0, 0], [123, 1]])

    def test_index_version_and_file_reference(self):
        bad = bytearray(index()); bad[3] = 4
        with self.assertRaises(ValueError): parse_index(bytes(bad))
        bad = bytearray(index()); struct.pack_into('<I', bad, len(bad)-12, 2)
        with self.assertRaises(ValueError): parse_index(bytes(bad))
        with self.assertRaises(ValueError): parse_index(index()[:-1])


if __name__ == '__main__':
    unittest.main()
