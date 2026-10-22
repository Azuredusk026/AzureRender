"""Decode the reserved frame using integer bit patterns and explicit bind poses."""
import struct
import unittest
import numpy as np

try:
    from endfield_native.capture_skin_frame import compare_skin_frame
except ImportError:
    compare_skin_frame = None


class NativeSkinFrameTests(unittest.TestCase):
    def fixture(self, instance=0):
        records = bytearray(256 * (instance + 1))
        model = np.eye(4); model[:3, 3] = [10, 20, 30]
        offset = 256 * instance
        struct.pack_into('<16f', records, offset, *model.T.ravel())
        struct.pack_into('<I', records, offset + 76, 16)
        struct.pack_into('<I', records, offset + 80, 7)
        palette = np.zeros((13, 4), dtype='<f4')
        expected = np.eye(4); expected[:3, 3] = [10, 22, 30]
        palette[7:10] = expected[:3]
        skin = np.eye(4); skin[1, 3] = 2
        palette[10:13] = skin[:3]
        return records, palette.tobytes(), [np.eye(4).tolist()]

    def setUp(self):
        self.assertTrue(callable(compare_skin_frame), 'Reserved skin frame decoder is required')

    def test_uses_reserved_rows_and_skips_them_for_bones(self):
        result = compare_skin_frame(*self.fixture())
        self.assertEqual(result['palette_row'], 7)
        self.assertEqual(result['origin'], [10, 22, 30])
        self.assertEqual(result['model_origin'], [10, 20, 30])
        self.assertEqual(result['bones'][0]['matrix_error'], 0)
        self.assertEqual(result['bones'][0]['position_error'], 0)

    def test_uses_nonzero_draw_instance_offset(self):
        result = compare_skin_frame(*self.fixture(2), instance_index=2)
        self.assertEqual(result['origin'], [10, 22, 30])

    def test_rejects_disabled_reserved_frame_and_truncated_ranges(self):
        instance, palette, binds = self.fixture()
        struct.pack_into('<I', instance, 76, 0)
        with self.assertRaisesRegex(ValueError, 'reserved'):
            compare_skin_frame(instance, palette, binds)
        instance, palette, binds = self.fixture()
        for a, b in [(instance[:80], palette), (instance, palette[:-16])]:
            with self.assertRaises(ValueError): compare_skin_frame(a, b, binds)

    def test_rejects_nonfinite_and_noninvertible_bind_pose(self):
        instance, palette, binds = self.fixture()
        binds[0][0][0] = float('nan')
        with self.assertRaises(ValueError): compare_skin_frame(instance, palette, binds)
        binds[0] = np.zeros((4,4)).tolist()
        with self.assertRaises(ValueError): compare_skin_frame(instance, palette, binds)

    def test_reports_rotation_difference_even_when_origins_match(self):
        instance, palette, binds = self.fixture()
        binds[0][0][0] = -1
        binds[0][2][2] = -1
        result = compare_skin_frame(instance, palette, binds)
        self.assertEqual(result['bones'][0]['position_error'], 0)
        self.assertEqual(result['bones'][0]['matrix_error'], 2)


if __name__ == '__main__':
    unittest.main()
