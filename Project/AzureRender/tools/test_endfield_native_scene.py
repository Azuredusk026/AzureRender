import struct
import unittest

from endfield_native.scene import validate_raw_header, merge_instances, decode_chunk_data, select_instance


def empty_chunk():
    data = bytearray(64)
    struct.pack_into('<I', data, 0, 24)
    struct.pack_into('<HH8H', data, 4, 20, 40, 4, 8, 16, 20, 24, 28, 32, 36)
    struct.pack_into('<i', data, 24, 20)
    return bytes(data)


class NativeSceneTests(unittest.TestCase):
    def test_wall_origin_outside_region_keeps_intersecting_geometry(self):
        wall = dict(x=0, y=-1.05, z=-45.01,
                    world_bounds=[-40.4, 40.4, -2.5, 22.92, -156.6, -35.84])
        region = [-40, 40, -3, 40, -40, 40]
        self.assertTrue(select_instance(wall, region))
        self.assertFalse(select_instance(wall, region, policy='origin'))
        self.assertFalse(select_instance(wall, [-20, 20, -3, 40, -20, 20]))

    def test_bridge_span_and_touching_boundary_are_conservatively_selected(self):
        bridge = dict(x=100, y=2, z=0, world_bounds=[-10, 110, 2, 2, -1, 1])
        self.assertTrue(select_instance(bridge, [-5, 5, 0, 5, -5, 5]))
        self.assertTrue(select_instance(bridge, [-20, -10, 0, 5, -5, 5]))
        self.assertFalse(select_instance(bridge, [-20, -10.01, 0, 5, -5, 5]))

    def test_missing_bounds_uses_finite_half_open_origin(self):
        point = dict(x=0, y=0, z=0, world_bounds=None)
        self.assertTrue(select_instance(point, [-1, 1, -1, 1, -1, 1]))
        self.assertFalse(select_instance(point, [-1, 0, -1, 1, -1, 1]))
        with self.assertRaises(ValueError):
            select_instance(dict(point, x=float('nan')), [-1, 1, -1, 1, -1, 1])

    def test_invalid_selection_bounds_and_source_bounds_are_rejected(self):
        point = dict(x=0, y=0, z=0, world_bounds=[-1, 1, -1, 1, -1, 1])
        valid = [-2, 2, -2, 2, -2, 2]
        for bounds in ([0, 0, -1, 1, -1, 1], [1, -1, -1, 1, -1, 1],
                       [0]*5, [float('inf')]*6, [False, 1, -1, 1, -1, 1]):
            with self.subTest(bounds=bounds), self.assertRaises(ValueError):
                select_instance(point, bounds)
        for bounds in ([1, -1, -1, 1, -1, 1], [0]*5, [float('nan')]*6):
            with self.subTest(source=bounds), self.assertRaises(ValueError):
                select_instance(dict(point, world_bounds=bounds), valid)
        with self.assertRaisesRegex(ValueError, 'policy'):
            select_instance(point, valid, policy='guess')

    def test_raw_and_compressed_share_the_validated_chunk_contract(self):
        raw = empty_chunk()
        self.assertIs(decode_chunk_data(raw), raw)
        packed = struct.pack('<I', len(raw)) + b'packed'
        def decompress(payload, size):
            self.assertEqual((payload, size), (b'packed', 64))
            return raw
        self.assertEqual(decode_chunk_data(packed, decompress), raw)

    def test_compressed_payload_requires_exact_size_and_valid_root(self):
        packed = struct.pack('<I', 64) + b'packed'
        for decoded in (bytes(64), empty_chunk()[:-1]):
            with self.assertRaises(ValueError):
                decode_chunk_data(packed, lambda payload, size: decoded)
        for size in (0, 63, 256*1024*1024+1):
            with self.assertRaises(ValueError):
                decode_chunk_data(struct.pack('<I', size)+b'x',
                                  lambda payload, expected: self.fail('Budget must precede decode'))

    def test_raw_root_is_not_a_compressed_size(self):
        validate_raw_header(empty_chunk())

    def test_truncated_or_wrong_layout_rejected(self):
        for size in (0, 4, 24, 63):
            with self.assertRaises(ValueError):
                validate_raw_header(empty_chunk()[:size])
        data = bytearray(empty_chunk())
        struct.pack_into('<I', data, 0, 0xffffffff)
        with self.assertRaises(ValueError):
            validate_raw_header(data)

    def test_duplicate_identity_requires_equal_payload(self):
        row = dict(entity_id=1, name='floor', matrix=[1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 2, 3, 4, 1])
        rows, duplicates = merge_instances([row, dict(row)])
        self.assertEqual((len(rows), duplicates), (1, 1))
        with self.assertRaisesRegex(ValueError, 'conflicting'):
            merge_instances([row, dict(row, name='other')])

    def test_nonfinite_or_nonaffine_matrix_rejected(self):
        for matrix in ([float('nan')]*16, [0]*16, [1]*15):
            with self.assertRaises(ValueError):
                merge_instances([dict(entity_id=1, name='bad', matrix=matrix)])


if __name__ == '__main__':
    unittest.main()
