import unittest

try:
    from endfield_native.environment import parse_metadata, resolve_reference
    from endfield_native.render_inventory import build_inventory
except ImportError:
    parse_metadata = resolve_reference = build_inventory = None


class EnvironmentTests(unittest.TestCase):
    def setUp(self):
        self.assertIsNotNone(parse_metadata, 'environment decoder is missing')

    def test_typed_unknown_fields_preserved(self):
        text = 'MonoBehaviour Base\n\tHGSkyConfig sky\n\t\tSphericalHarmonicsL2 sh\n\t\t\tfloat sh[ 0] = 0.25\n\t\t\tfloat sh[ 1] = -0.5\n\tstring label = "a = b"\n'
        fields = parse_metadata(text)
        self.assertEqual(fields['sky/sh/sh[ 0]'], {'type': 'float', 'value': .25, 'line': 4})
        self.assertEqual(fields['label']['value'], 'a = b')

    def test_invalid_float_and_duplicate_fields_rejected(self):
        for text in ('MonoBehaviour Base\n\tfloat x = NaN',
                     'MonoBehaviour Base\n\tfloat x = 1\n\tfloat x = 2'):
            with self.assertRaises(ValueError):
                parse_metadata(text)

    def test_array_indices_keep_repeated_data_distinct(self):
        fields = parse_metadata('MonoBehaviour Base\n\tvector items\n\t\tArray Array\n\t\tint size = 2\n\t\t\t[0]\n\t\t\tEntry data\n\t\t\t\tfloat value = 3\n\t\t\t[1]\n\t\t\tEntry data\n\t\t\t\tfloat value = 7\n')
        self.assertEqual(fields['items/size/data[0]/value']['value'], 3)
        self.assertEqual(fields['items/size/data[1]/value']['value'], 7)
        unsigned = parse_metadata('Base root\n\tvector items\n\t\tint size = 2\n\t\t\t[0]\n\t\t\tunsigned int data = 4\n\t\t\t[1]\n\t\t\tunsigned int data = 9')
        self.assertEqual(unsigned['items/size/data[1]']['value'], 9)

    def test_full_external_identity_and_null(self):
        source = {'Name': 'CAB-a', 'ExternalFiles': ['CAB-b']}
        self.assertEqual(resolve_reference(source, 1, 44, {'CAB-a', 'CAB-b'}), ('CAB-b', 44))
        self.assertEqual(resolve_reference(source, 0, 44, {'CAB-a', 'CAB-b'}), ('CAB-a', 44))
        self.assertIsNone(resolve_reference(source, 0, 0, {'CAB-a'}))
        with self.assertRaisesRegex(ValueError, 'dependency'):
            resolve_reference(source, 1, 44, {'CAB-a'})
        with self.assertRaises(ValueError):
            resolve_reference(source, 2, 44, {'CAB-a', 'CAB-b'})

    def test_object_collision_and_state_conflict(self):
        row = {'identity': ['CAB-a', 4], 'activation': 'unverified', 'fields': {}}
        with self.assertRaisesRegex(ValueError, 'identity'):
            build_inventory('1.0.14', [row, row], [])
        with self.assertRaisesRegex(ValueError, 'activation'):
            build_inventory('1.0.14', [dict(row, activation=True)], [])

    def test_unknown_activation_blocks_complete_inventory(self):
        row = {'identity': ['CAB-a', 4], 'activation': 'unverified', 'fields': {}}
        result = build_inventory('1.0.14', [row], [])
        self.assertEqual(result['status'], 'incomplete')
        self.assertEqual(result['unverified_objects'], [['CAB-a', 4]])


if __name__ == '__main__':
    unittest.main()
