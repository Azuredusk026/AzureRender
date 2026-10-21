import copy
import struct
import unittest

try:
    from endfield_native.spline_bake import compile_emitter, compile_emitters
except ImportError:
    compile_emitter = compile_emitters = None


def emitter(source='CAB-a', path_id=1, speed=1):
    values = {'simulateSpeed': speed, 'emissionRateOverTime': .5,
              'hasNoise': 0, 'hasRotation': 0, 'has3dRotation': 0,
              'prewarm': 1, 'emissionType': 0, 'has3dStartRotation': 1,
              'hasAlphaControl': 0}
    for name, value in [('velocityOverLifeTime', 2), ('startLifeTime', 1),
                        ('startSize', .8), ('sizeOverLifeTime', 1), ('startRotationX', 0),
                        ('startRotationY', 180), ('startRotationZ', 0)]:
        values[name + '/m_Mode'] = 0
        values[name + '/m_ConstantMax'] = value
    return {'identity': [source, path_id], 'name': 'same-name',
            'fields': {'desc/' + key: {'value': value} for key, value in values.items()},
            'segments': [{'stored_length': 10,
                          'controls': [[0, 0, 0], [0, 10/3, 0],
                                       [0, 20/3, 0], [0, 10, 0]]}]}


class SplineBakeTests(unittest.TestCase):
    def setUp(self):
        self.assertIsNotNone(compile_emitter, 'multi-emitter bake contract missing')

    def test_native_expiry_gap_and_rebirth_are_explicit_keys(self):
        result = compile_emitter(emitter(), sample_rate=2)
        self.assertEqual(result['period'], 6)
        self.assertEqual(len(result['slots']), 3)
        slot = result['slots'][2]
        times = slot['times']
        self.assertEqual(slot['scales'][times.index(1)], [0, 0, 0])
        self.assertEqual(slot['scales'][times.index(2)], [.8, .8, .8])
        self.assertAlmostEqual(slot['positions'][times.index(2)][1], 0)
        previous = times[times.index(1) - 1]
        self.assertLess(1 - previous, .0001)
        self.assertGreater(slot['positions'][times.index(previous)][1], 9.99)
        self.assertEqual(slot['positions'][0], slot['positions'][-1])
        self.assertEqual(slot['scales'][0], slot['scales'][-1])

    def test_emitters_keep_independent_clocks_and_source_identity(self):
        a, b = emitter(), emitter('CAB-b', 1, 2)
        result = compile_emitters([a, b], sample_rate=2)
        self.assertEqual([e['identity'] for e in result], [['CAB-a', 1], ['CAB-b', 1]])
        self.assertEqual([e['period'] for e in result], [6, 3])
        self.assertAlmostEqual(result[1]['slots'][0]['positions'][1][1], 2)
        with self.assertRaisesRegex(ValueError, 'duplicate'):
            compile_emitters([a, copy.deepcopy(a)])

    def test_random_range_requires_explicit_seed_and_stays_in_source_range(self):
        source = emitter()
        source['fields']['desc/startSize/m_Mode']['value'] = 3
        source['fields']['desc/startSize/m_ConstantMin'] = {'value': .7}
        with self.assertRaisesRegex(ValueError, 'seed'):
            compile_emitter(source)
        result = compile_emitter(source, seed=42, sample_rate=2)
        self.assertEqual(result, compile_emitter(source, seed=42, sample_rate=2))
        sizes = [s['scales'][0][0] for s in result['slots']]
        self.assertTrue(all(.7 <= size <= .8 for size in sizes))
        self.assertGreater(len(set(sizes)), 1)
        self.assertFalse(result['nativeRandomVerified'])

    def test_unsupported_curves_and_unbounded_work_are_rejected(self):
        source = emitter()
        source['fields']['desc/velocityOverLifeTime/m_Mode']['value'] = 1
        with self.assertRaisesRegex(ValueError, 'velocityOverLifeTime'):
            compile_emitter(source)
        for rate in (0, -1, float('nan'), 100001):
            with self.subTest(rate=rate), self.assertRaises(ValueError):
                compile_emitter(emitter(), sample_rate=rate)
        with self.assertRaisesRegex(ValueError, 'budget'):
            compile_emitter(emitter(), sample_rate=30, max_keys=10)

    def test_source_random_rotation_accepts_descending_endpoints(self):
        source = emitter()
        source['fields']['desc/startRotationY/m_Mode']['value'] = 3
        source['fields']['desc/startRotationY/m_ConstantMin'] = {'value': 270}
        result = compile_emitter(source, seed=42, sample_rate=2)
        self.assertTrue(all(180 <= slot['sourceEuler'][1] <= 270
                            for slot in result['slots']))

    def test_event_keys_remain_distinct_in_gltf_float32_time(self):
        result = compile_emitter(emitter(speed=.01), sample_rate=2)
        for slot in result['slots']:
            packed = [struct.unpack('<f', struct.pack('<f', t))[0] for t in slot['times']]
            self.assertTrue(all(a < b for a, b in zip(packed, packed[1:])),
                            'float32 event keys must be strictly increasing')

    def test_unimplemented_source_branches_fail_instead_of_becoming_prewarmed(self):
        for field, unsupported in [('prewarm', 0), ('emissionType', 1),
                                   ('has3dStartRotation', 0)]:
            source = emitter()
            source['fields']['desc/' + field]['value'] = unsupported
            with self.subTest(field=field), self.assertRaisesRegex(ValueError, field):
                compile_emitter(source)
        source = emitter()
        source['fields']['desc/sizeOverLifeTime/m_ConstantMax']['value'] = .5
        self.assertEqual(compile_emitter(source)['slots'][0]['scales'][0], [.4] * 3)

    def test_source_alpha_curve_drives_independent_slot_birth_and_expiry(self):
        source = emitter()
        fields = source['fields']
        fields['desc/hasAlphaControl']['value'] = 1
        prefix = 'desc/alphaOverLifeTime/'
        fields[prefix + 'm_Mode'] = {'value': 1}
        fields[prefix + 'm_CurveMultiplier'] = {'value': 1}
        fields[prefix + 'm_CurveMax/m_Curve/size'] = {'value': 4}
        for i, (t, v, ins, outs) in enumerate([(0, 0, 0, 10), (.1, 1, 10, 0),
                                               (.9, 1, 0, -10), (1, 0, -10, 0)]):
            for key, value in dict(time=t, value=v, inSlope=ins, outSlope=outs, weightedMode=0).items():
                fields[prefix + f'm_CurveMax/m_Curve/size/data[{i}]/' + key] = {'value': value}
        result = compile_emitter(source, sample_rate=4)
        slot = result['slots'][0]
        self.assertEqual(slot['opacities'][0], 0)
        self.assertAlmostEqual(slot['opacities'][slot['times'].index(.25)], .5)
        self.assertEqual(slot['opacities'][slot['times'].index(.5)], 1)
        self.assertEqual(result['slots'][2]['opacities'][result['slots'][2]['times'].index(1)], 0)
        fields[prefix + 'm_CurveMax/m_Curve/size/data[0]/weightedMode']['value'] = 1
        with self.assertRaisesRegex(ValueError, 'weighted'):
            compile_emitter(source)


if __name__ == '__main__':
    unittest.main()
