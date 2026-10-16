import struct
import unittest

from endfield_native.scene import validate_raw_header, merge_instances


def empty_chunk():
    data = bytearray(64)
    struct.pack_into('<I', data, 0, 24)
    struct.pack_into('<HH8H', data, 4, 20, 40, 4, 8, 16, 20, 24, 28, 32, 36)
    struct.pack_into('<i', data, 24, 20)
    return bytes(data)


class NativeSceneTests(unittest.TestCase):
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
