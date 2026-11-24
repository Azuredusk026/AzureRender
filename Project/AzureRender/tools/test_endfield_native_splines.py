import unittest
import json
from pathlib import Path
import tempfile

try:
    from endfield_native.splines import decode_descriptor, inventory_splines
except ImportError:
    decode_descriptor = None


def descriptor():
    lines = ['MonoBehaviour Base', '\tPPtr<GameObject> m_GameObject',
             '\t\tint m_FileID = 1', '\t\tSInt64 m_PathID = 77',
             '\tSplineObjectsDesc desc', '\t\tstring name = "test"',
             '\t\tfloat simulateSpeed = 0.25',
             '\t\tfloat emissionRateOverTime = 0.5',
             '\t\tMinMaxCurve alphaLifeTime',
             '\t\t\tfloat m_ConstantMax = 0.8',
             '\tSplineObjectsKnotData knotData', '\t\tArray Array', '\t\tint size = 2']
    for index, heights in enumerate(((0, 0, 4, 4), (4, 4, 4, 4))):
        lines += [f'\t\t\t[{index}]', '\t\t\tSplineObjectsKnotData data']
        for point, height in enumerate(heights):
            lines += [f'\t\t\t\tVector3f p{point}', '\t\t\t\t\tfloat x = 0',
                      f'\t\t\t\t\tfloat y = {height}', '\t\t\t\t\tfloat z = 0']
        lines += [f'\t\t\t\tfloat length = {4 if index == 0 else 0.001}']
    return '\n'.join(lines)


class SplineTests(unittest.TestCase):
    def setUp(self):
        self.assertIsNotNone(decode_descriptor, 'spline descriptor reader is missing')
        self.source = {'Name': 'CAB-a', 'ExternalFiles': ['CAB-b']}

    def decode(self, text=None, available=None):
        return decode_descriptor(descriptor() if text is None else text, self.source,
                                 {'CAB-a', 'CAB-b'} if available is None else available)

    def test_terminal_slot_preserved_without_inventing_active_topology(self):
        result = self.decode()
        self.assertEqual(len(result['segments']), 2)
        self.assertEqual(result['segments'][0]['controls'][3], [0, 4, 0])
        self.assertFalse(result['segments'][0]['geometrically_degenerate'])
        self.assertTrue(result['segments'][1]['geometrically_degenerate'])
        self.assertEqual(result['segments'][1]['stored_length'], .001)
        self.assertIsNone(result['active_segment_count'])

    def test_curves_and_raw_speed_preserved_without_unit_inference(self):
        result = self.decode()
        self.assertEqual(result['fields']['desc/alphaLifeTime/m_ConstantMax']['value'], .8)
        self.assertEqual(result['fields']['desc/simulateSpeed']['value'], .25)
        self.assertEqual(result['runtime_semantics'], 'unverified')

    def test_external_references_resolve_and_missing_dependency_stays_visible(self):
        self.assertEqual(self.decode()['references']['m_GameObject'], ('CAB-b', 77))
        missing = self.decode(available={'CAB-a'})
        self.assertEqual(len(missing['unresolved_dependencies']), 1)
        self.assertNotIn('m_GameObject', missing['references'])

    def test_truncated_or_invalid_segment_rejected(self):
        for text in (descriptor().replace('int size = 2', 'int size = 3'),
                     descriptor().replace('float length = 4', 'float length = -4'),
                     descriptor().replace('float y = 4', 'float y = NaN')):
            with self.subTest(text=text[-40:]), self.assertRaises(ValueError):
                self.decode(text)

    def test_non_descriptor_rejected(self):
        with self.assertRaises(ValueError):
            self.decode('MonoBehaviour Base\n\tstring m_Name = "unrelated"')

    def test_same_named_emitters_keep_source_identity(self):
        with tempfile.TemporaryDirectory() as directory:
            probe = Path(directory)
            (probe / 'metadata').mkdir()
            sources = [dict(self.source, OriginalPath='main/aa.ab'),
                       {'Name': 'CAB-b', 'ExternalFiles': ['CAB-a'], 'OriginalPath': 'main/bb.ab'}]
            (probe / 'scene_manifest.json').write_text(json.dumps({
                'Format': 'EndfieldSceneProbe/1', 'SerializedFiles': sources}), encoding='utf-8')
            for name in ('aa_1_unnamed.txt', 'bb_1_unnamed.txt'):
                (probe / 'metadata' / name).write_text(descriptor(), encoding='utf-8')
            (probe / 'metadata' / 'aa_2_shader.txt').write_text('Shader Base\n', encoding='utf-8')
            result = inventory_splines(probe)
            self.assertEqual([e['identity'] for e in result['emitters']], [['CAB-a', 1], ['CAB-b', 1]])
            self.assertEqual(result['emitters'][1]['references']['m_GameObject'], ('CAB-a', 77))


if __name__ == '__main__':
    unittest.main()
