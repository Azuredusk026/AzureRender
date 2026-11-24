import struct
import unittest

from endfield_native.reflection_bridge import decode_serialized_probe, pack_serialized_probe


def fields(mode=2):
    def field(kind, fmt, *values):
        return {'type': kind, 'hex': struct.pack('<' + fmt, *values).hex()}
    return {
        '0x10000000': field(9, '3f', -8, 10, 12),
        '0x10030000': field(9, '3f', .8, .9, .95),
        '0x10040000': field(17, '16f', *range(16)),
        '0x10050000': field(17, '16f', *range(16, 32)),
        '0x10060000': field(4, 'f', .5),
        '0x10070000': field(1, 'B', 1),
        '0x10080000': field(2, 'i', mode),
        '0x10090000': field(2, 'i', 4),
        '0x100a0000': field(17, '27f', *range(27)),
        '0x100b0000': field(17, '27f', *range(27, 54)),
        '0x100d0000': field(2, 'i', 0),
    }


class ReflectionBridgeTests(unittest.TestCase):
    def test_custom_mode_selects_custom_sh_and_native_half_extent(self):
        p = decode_serialized_probe(fields())
        self.assertEqual(p['sh_coefficients'], list(range(27, 54)))
        self.assertEqual(p['half_extent'], [4, 5, 6])
        self.assertEqual(p['importance'], 4)
        self.assertIsNone(p['activation'])
        self.assertIsNone(p['registration_sort_key'])

    def test_baked_mode_uses_its_own_coefficients(self):
        p = decode_serialized_probe(fields(0))
        self.assertEqual(p['sh_coefficients'], list(range(27)))

    def test_projection_origin_preserves_white_yellow_world_center(self):
        for mode in (0, 2):
            with self.subTest(mode=mode):
                source = fields(mode)
                source['0x10040000']['hex'] = struct.pack(
                    '<16f', 4, 0, 0, 0, 0, 5, 0, 0, 0, 0, 6, 0,
                    0, 11.601508140563965, -7.430000305175781, 1).hex()
                decoded = decode_serialized_probe(source)
                self.assertEqual(decoded.get('projection_origin'),
                                 [0, 11.601508140563965, -7.430000305175781])

    def test_projection_origin_rejects_nonfinite_matrix_values(self):
        for key in ('0x10040000', '0x10050000'):
            for axis, value in enumerate((float('nan'), float('inf'), -float('inf'))):
                with self.subTest(matrix=key, axis=axis, value=value):
                    source = fields()
                    matrix = list(range(16))
                    matrix[12 + axis] = value
                    source[key]['hex'] = struct.pack('<16f', *matrix).hex()
                    with self.assertRaises(ValueError):
                        decode_serialized_probe(source)

    def test_shader_rows_preserve_shape_intensity_and_explicit_runtime_state(self):
        rows = pack_serialized_probe(fields(), array_layer=3, fade=.6)
        self.assertEqual(rows[1], (4, 5, 6, 3))
        self.assertEqual(rows[2], (16, 20, 24, 28))
        self.assertEqual(rows[5][3], .5)
        self.assertEqual(rows[6], (1, 1, 4, .6))
        f = fields()
        f['0x100d0000']['hex'] = '01000000'
        self.assertEqual(pack_serialized_probe(f, array_layer=3, fade=1)[6][1], 0)

    def test_importance_matches_native_signed_short_clamp(self):
        f = fields()
        f['0x10090000']['hex'] = struct.pack('<i', 65535).hex()
        self.assertEqual(decode_serialized_probe(f)['importance'], 0)

    def test_missing_coefficients_and_unknown_modes_are_explicit_failures(self):
        f = fields()
        del f['0x100b0000']
        for source in (f, fields(1), fields(5)):
            with self.assertRaises(ValueError):
                decode_serialized_probe(source)

    def test_type_and_size_rejection(self):
        for kind, raw in ((9, bytes(64)), (17, bytes(60))):
            f = fields()
            f['0x10040000'] = {'type': kind, 'hex': raw.hex()}
            with self.assertRaises(ValueError):
                decode_serialized_probe(f)


if __name__ == '__main__':
    unittest.main()
